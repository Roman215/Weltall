#include "Battle/BattleAction.h"
#include "Battle/BattleAi.h"
#include "Battle/BattleContext.h"
#include "Battle/BattleProgram.h"
#include "Battle/BattleScene.h"
#include "Battle/BattleSetup.h"
#include "Battle/BattleTables.h"
#include "Battle/Show/BattleHudSheet.h"
#include "Battle/Show/BattleObjectFiles.h"
#include "Battle/Show/BattleSceneGraphics.h"
#include "Battle/Show/BattleShow.h"
#include "Battle/Show/BattleSpriteFiles.h"
#include "Battle/Show/BattleSpriteTextures.h"
#include "Battle/Show/BattleStageFiles.h"
#include "Field/FieldArchive.h"
#include "Game/GameState.h"
#include "Game/PsxRandom.h"
#include "Foundation/LittleEndian.h"
#include "Graphics/GteMath.h"
#include "Graphics/TextureRegion.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/BattleObjectDigest.h"
#include "Support/BattleObjectServices.h"
#include "Support/BattleFrameDigest.h"
#include "Support/BattleShowDigest.h"
#include "Support/BattleSoundRecorder.h"
#include "Support/BattleSpriteDigest.h"
#include "Support/GameDiscs.h"
#include "Support/OracleDigest.h"
#include "Support/XorShift32.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace Weltall;
using Weltall::Testing::FOracleDigest;
using Weltall::Testing::FXorShift32;

// The battle's show frame by frame against the original's:
// the same cases drawn from XorShift32, the same frames, the same digest.
namespace
{
	constexpr int32 SceneCount = 76;
	constexpr int32 StageCount = 75;
	constexpr int32 FieldDirectory = 4;
	constexpr int32 FirstMapFile = 0xB8;
	constexpr std::array<int32, 10> OpeningMaps{3, 15, 22, 49, 53, 56, 57, 58, 60, 61};
	/** The models part's maps: their formations fight between them 49 of the 52 scenes with 3D enemies. */
	constexpr std::array<int32, 12> ModelMaps{198, 496, 660, 223, 403, 180, 2, 54, 260, 406, 101, 703};
	constexpr uint32 GearCount = 19;
	using FFormationRecord = std::array<uint8, FBattleFormation::RecordSize>;

	/** The disc's files the cases draw from and the battles read. */
	struct FShowData
	{
		std::optional<FGameFileIndex> Index;
		FGteArctangents Arctangents;
		std::optional<FBattleMarkerModel> MarkerModel;
		std::optional<FBattleTables> Tables;
		std::optional<FBattleProgramTables> Program;
		std::vector<FBattleScene> Scenes;
		std::vector<FBattleStage> Layouts;
		std::vector<FBattleStageFiles> Stages;
		std::vector<FBattleSceneGraphics> Graphics;
		std::optional<FBattleHudSheet> Sheet;
		std::optional<FBattleSpriteFiles> Files;
		std::optional<FBattleObjectDisc> Objects;
		std::vector<FFormationRecord> Formations;
		std::vector<FFormationRecord> ModelFormations;
		/** The formations whose enemies all stand on foot (the effects part's), the effects a record may name and their animations. */
		std::vector<FFormationRecord> FootFormations;
		std::vector<int32> Effects;
		std::vector<int32> EffectAnimations;
		/** The effects that load an animation extension module (the extensions part's). */
		std::vector<int32> ExtensionEffects;
	};

	/**
	 * The formations of the maps' encounter sections (16 each) fought against a scene Scenes marks, without the special
	 * party, with an enemy: in map order, then their own.
	 */
	bool AddFormations(const FCdImage& Image, const FGameFileIndex& Index, std::span<const int32> Maps, const std::vector<bool>& Scenes,
		std::vector<FFormationRecord>& Formations)
	{
		for (const int32 Map : Maps)
		{
			const FIndexRecord* Record = Index.Find(FieldDirectory, FirstMapFile + 2 * Map);
			if (Record == nullptr)
			{
				return false;
			}
			const auto Sectors = ReadFileSectors(Image, *Record);
			const auto Archive = Sectors ? ParseFieldArchive(*Sectors) : TResult<FFieldArchive>(MakeError("no map"));
			if (!Archive)
			{
				return false;
			}
			const std::vector<uint8>& Encounters = Archive->Section(EFieldSection::Encounters);
			for (std::size_t Number = 0; Number < 16 && (Number + 1) * FBattleFormation::RecordSize <= Encounters.size(); ++Number)
			{
				FFormationRecord Formation{};
				std::copy_n(Encounters.begin() + static_cast<std::ptrdiff_t>(Number * FBattleFormation::RecordSize), FBattleFormation::RecordSize, Formation.begin());
				const bool bEnemy = std::any_of(Formation.begin() + 8, Formation.begin() + 16, [](uint8 Type) { return (Type & 0x7F) != 0x7F; });
				if (Formation[0] < SceneCount && Scenes[Formation[0]] && (Formation[1] & FormationFlags::SpecialParty) == 0 && bEnemy)
				{
					Formations.push_back(Formation);
				}
			}
		}
		return true;
	}

	/** Reads the data into Data, which stays where it is: the sprite files keep pointers to its index. */
	bool ReadData(const FIso9660& Disc, FShowData& Data)
	{
		const auto Executable = ReadBootExecutable(Disc);
		if (!Executable)
		{
			return false;
		}
		auto Index = FGameFileIndex::Read(*Executable);
		const auto Arctangents = FGteArctangents::Read(*Executable);
		auto MarkerModel = FBattleMarkerModel::Read(*Executable);
		if (!Index || !Arctangents || !MarkerModel)
		{
			return false;
		}
		Data.Index = std::move(*Index);
		Data.Arctangents = *Arctangents;
		Data.MarkerModel = std::move(*MarkerModel);
		const FCdImage& Image = Disc.Image();
		auto Tables = FBattleTables::Load(Image, *Data.Index);
		auto Program = FBattleProgramTables::Load(Image, *Data.Index);
		auto Sheet = FBattleHudSheet::Load(Image, *Data.Index);
		auto Files = FBattleSpriteFiles::Load(Image, *Data.Index);
		auto Objects = FBattleObjectDisc::Load(Image, *Data.Index);
		if (!Tables || !Program || !Sheet || !Files || !Objects)
		{
			return false;
		}
		Data.Objects = std::move(*Objects);
		Data.Tables = std::move(*Tables);
		Data.Program = std::move(*Program);
		Data.Sheet = std::move(*Sheet);
		Data.Files = std::move(*Files);
		for (int32 Scene = 0; Scene < SceneCount; ++Scene)
		{
			auto Enemies = FBattleScene::Load(Image, *Data.Index, Scene);
			auto Graphics = FBattleSceneGraphics::Load(Image, *Data.Index, Scene);
			if (!Enemies || !Graphics)
			{
				return false;
			}
			Data.Scenes.push_back(std::move(*Enemies));
			Data.Graphics.push_back(std::move(*Graphics));
		}
		for (int32 Stage = 0; Stage < StageCount; ++Stage)
		{
			auto Layout = FBattleStage::Load(Image, *Data.Index, Stage);
			auto StageFiles = FBattleStageFiles::Load(Image, *Data.Index, Stage);
			if (!Layout || !StageFiles)
			{
				return false;
			}
			Data.Layouts.push_back(*Layout);
			Data.Stages.push_back(std::move(*StageFiles));
		}
		// The scenes whose enemies are all sprites and those with a 3D model among them, and the maps' formations fought against them.
		std::vector<bool> SpriteScene(SceneCount, false);
		std::vector<bool> ModelScene(SceneCount, false);
		for (int32 Scene = 0; Scene < SceneCount; ++Scene)
		{
			const FBattleSceneGraphics& Graphics = Data.Graphics[static_cast<std::size_t>(Scene)];
			SpriteScene[static_cast<std::size_t>(Scene)] = std::all_of(Graphics.Entries().begin(), Graphics.Entries().end(), [&Graphics](const FBattleSceneEntry& Entry) {
				return Entry.IsSprite() && Entry.Data > 7 && Entry.Data < Graphics.KeptEnd();
			});
			ModelScene[static_cast<std::size_t>(Scene)] = std::any_of(Graphics.Entries().begin(), Graphics.Entries().end(), [](const FBattleSceneEntry& Entry) {
				return !Entry.IsSprite();
			});
		}
		if (!AddFormations(Image, *Data.Index, OpeningMaps, SpriteScene, Data.Formations) || !AddFormations(Image, *Data.Index, ModelMaps, ModelScene, Data.ModelFormations))
		{
			return false;
		}
		for (const FFormationRecord& Formation : Data.Formations)
		{
			if (std::none_of(Formation.begin() + 8, Formation.begin() + 16, [](uint8 Type) { return (Type & 0x7F) != 0x7F && (Type & 0x80) != 0; }))
			{
				Data.FootFormations.push_back(Formation);
			}
		}
		// The build tool's stand-ins are the effects that do not read.
		Data.EffectAnimations.assign(static_cast<std::size_t>(Data.Files->EffectCount()), 0);
		for (int32 Effect = 0; Effect < Data.Files->EffectCount(); ++Effect)
		{
			if (const TResult<FBattleSpriteFile> File = Data.Files->ReadEffect(Effect))
			{
				Data.Effects.push_back(Effect);
				Data.EffectAnimations[static_cast<std::size_t>(Effect)] = File->Sprite().AnimationCount(File->Sprite().Tables());
				if (File->Extension() != 0)
				{
					Data.ExtensionEffects.push_back(Effect);
				}
			}
		}
		return true;
	}

	/** A case: the game state, the formation, the battle's kind, the frames and rand()'s seed, as OpeningCase draws them. */
	struct FOpeningCase
	{
		FGameState State;
		FFormationRecord Formation{};
		uint8 Kind = 0;
		int32 Frames = 0;
		uint32 Seed = 0;

		FOpeningCase(FXorShift32& G, const std::vector<FFormationRecord>& Formations)
		{
			std::span<uint8> Block = State.Block();
			for (uint8& Byte : Block)
			{
				Byte = static_cast<uint8>(G.Next());
			}
			const uint32 Members = 1 + G.Next() % 3;
			std::vector<uint8> Party;
			while (Party.size() < Members)
			{
				const uint8 Character = static_cast<uint8>(G.Next() % 11);
				if (std::find(Party.begin(), Party.end(), Character) == Party.end())
				{
					Party.push_back(Character);
				}
			}
			for (std::size_t Slot = 0; Slot < 3; ++Slot)
			{
				Block[0x1D34 + Slot] = Slot < Party.size() ? Party[Slot] : 0xFF;
				Block[0x22B1 + Slot] = 0;
			}
			Block[0x1D30] = 0xFF;
			Block[0x1D31] = 0x07;
			Block[0x1D32] = 0xFF;
			Block[0x1D33] = 0xFF;
			for (std::size_t Character = 0; Character < 11; ++Character)
			{
				const std::size_t Record = 0x26C + Character * 0xA4;
				Block[Record + 0x56] = static_cast<uint8>(Character);
				Block[Record + 0xA0] = 0xFF;
			}
			const uint32 Story = G.Next() % 400;
			Block[0x1930] = static_cast<uint8>(Story);
			Block[0x1931] = static_cast<uint8>(Story >> 8);
			Formation = Formations[G.Next() % Formations.size()];
			Kind = static_cast<uint8>(G.Next() % 6);
			Frames = static_cast<int32>(40 + G.Next() % 40);
			Seed = G.Next();
		}
	};

	/**
	 * A models part's case: an opening's against 3D enemies, the party in gears when the formation's enemies are (as the game has
	 * them fight), each character's gear drawn (ModelCase).
	 */
	struct FModelCase : FOpeningCase
	{
		FModelCase(FXorShift32& G, const std::vector<FFormationRecord>& Formations) : FOpeningCase(G, Formations)
		{
			std::span<uint8> Block = State.Block();
			const uint8 InGears = std::any_of(Formation.begin() + 8, Formation.begin() + 16, [](uint8 Type) { return (Type & 0x7F) != 0x7F && (Type & 0x80) != 0; }) ? 1 : 0;
			for (std::size_t Slot = 0; Slot < 3; ++Slot)
			{
				Block[0x22B1 + Slot] = InGears;
			}
			for (std::size_t Character = 0; Character < 11; ++Character)
			{
				Block[0x26C + Character * 0xA4 + 0xA0] = static_cast<uint8>(G.Next() % GearCount);
			}
		}
	};

	/** A case's battle and its show: the rules' setup from the case's game state and formation, the show begun on it. */
	struct FShowCase
	{
		FPsxRandom Random;
		std::unique_ptr<FBattleContext> Context = std::make_unique<FBattleContext>();
		std::unique_ptr<FBattleShow> Show = std::make_unique<FBattleShow>();
		Testing::FBattleSoundRecorder Sound;
		Testing::FBattleShowDigester Shown{*Show, *Context, Random, Sound};
		/** A call from the rules' code ran past CallFrames. */
		bool bEndless = false;
		/** Shown each frame displayed, after the oracle's digest of it. */
		std::function<void(const FBattleShow&)> Watch;

		FShowCase(const FShowData& Data, FOpeningCase& Case) : Random(Case.Seed)
		{
			Context->GameState = &Case.State;
			Context->Random = &Random;
			Context->Program = &*Data.Program;
			Context->Arctangents = &Data.Arctangents;
			const FBattleFormation Formation = FBattleFormation::FromRecord(Case.Formation);
			SetUpBattle(*Context, Formation, *Data.Tables, Data.Scenes[Formation.Scene], Data.Layouts[Formation.Stage]);
			FinishBattlePartySetup(*Context);
			REQUIRE(Context->Fault == EBattleFault::None);

			FBattleShowSetup Setup;
			Setup.Context = Context.get();
			Setup.Arctangents = &Data.Arctangents;
			Setup.SpriteFiles = &*Data.Files;
			Setup.Stage = &Data.Stages[Formation.Stage];
			Setup.SceneGraphics = &Data.Graphics[Formation.Scene];
			Setup.Sheet = &*Data.Sheet;
			Setup.MarkerModel = &*Data.MarkerModel;
			Setup.ObjectFiles = &*Data.Objects;
			Setup.Kind = Case.Kind;
			Setup.bEvent = Context->bEventBattle;
			Setup.Sound = &Sound;
			// The original's Regurus: a case drawing it waits for its opening for good.
			Show->Options.bRegurusFightsAsVierge = false;
			Show->Begin(Setup);
		}

		~FShowCase() { Show->End(); }
	};

	/**
	 * The frames one call from the rules' code may run, its own waits' frames included: past them the original waits for good (a
	 * 3D combatant's sequence that stays busy in the objects' idle wait) and the case ends (CALL_FRAMES).
	 */
	constexpr int32 CallFrames = 600;

	/**
	 * A frame the rules' code runs, with the frames the show runs inside its own waits (the objects' idle wait) until it returns
	 * there; false when it would run past CallFrames.
	 */
	bool RunFrame(FBattleShow& Show)
	{
		for (int32 Frame = 0; Frame < CallFrames; ++Frame)
		{
			Show.BeginFrame();
			if (Show.EndFrame() || Show.Fault() != EBattleShowFault::None)
			{
				return true;
			}
		}
		return false;
	}

	/** One case of the opening, digested frame by frame. */
	FSha1::FDigest RunOpening(const FShowData& Data, FXorShift32& G, FOpeningCase& Case)
	{
		FShowCase C(Data, Case);
		FBattleShow& Show = *C.Show;
		FOracleDigest One;
		// Target markers over drawn slots from a drawn frame on, the glow on slot 0.
		const int32 MarkedAt = static_cast<int32>(G.Next() % static_cast<uint32>(Case.Frames));
		const uint16 Marked = static_cast<uint16>(G.Next() & 0x7FF);
		for (int32 Frame = 0; Frame < Case.Frames; ++Frame)
		{
			INFO("frame " << Frame);
			if (Frame == MarkedAt)
			{
				C.Context->TurnActor = 0;
				Show.SetTargetMarkers(Marked);
			}
			REQUIRE(RunFrame(Show));
			INFO("fault " << static_cast<int32>(Show.Fault()) << (Show.Objects.Fault ? ": " + Show.Objects.Fault->Message : std::string()));
			REQUIRE(Show.Fault() == EBattleShowFault::None);
			for (const FKernelTaskEntry& Entry : Show.Sprites.Tasks())
			{
				if (Entry.Sprite)
				{
					const FSpriteTask& Task = *Entry.Sprite;
					One.U8(1, static_cast<uint8>(Show.Battle.UpdateRoutine(Task)), Task.bDrawn);
					One.I16(C.Shown.Digester.IndexOf(Task.Owner));
					One.U8(Task.Marks >> 29 & 7);
					C.Shown.Digester.DigestSprite(One, Task.Sprite);
				}
				else if (Entry.Task == &Show.Fade)
				{
					const FBattleFade& Fade = Show.Fade.Fade;
					One.U8(5);
					One.I32(Fade.Total, Fade.Remaining);
					One.U8(Fade.Mode, Fade.Target[0], Fade.Target[1], Fade.Target[2], Fade.From[0], Fade.From[1], Fade.From[2], Fade.Shown[0], Fade.Shown[1],
						Fade.Shown[2]);
				}
				else if (const FBattleMarker* Marker = Show.Markers.Find(Entry.Task))
				{
					One.U8(12);
					One.I16(C.Shown.Digester.IndexOf(Marker->Target));
					One.I32(Marker->Position[0], Marker->Position[1], Marker->Position[2]);
					One.I16(Marker->Rotation[0], Marker->Rotation[1], Marker->Rotation[2]);
					One.I32(Marker->Height, Marker->Count);
				}
				else if (Entry.Task == &Show.Markers.Glow)
				{
					One.U8(13);
					One.I16(C.Shown.Digester.IndexOf(Show.Markers.Glow.Sprite));
					One.I32(Show.Markers.Glow.Count, Show.Markers.Glow.Actor);
				}
				else if (const std::optional<int32> Step = Show.Objects.GearLoadStep(*Entry.Task))
				{
					One.U8(14, *Step);
				}
				else
				{
					REQUIRE(Entry.Task != &Show.Shake);
					REQUIRE(Entry.Task != &Show.TintFade);
					One.U8(11);
				}
			}
			One.U8(0xFF);
			C.Shown.DigestState(One);
			C.Shown.DigestObjects(One);
			One.U8(0xFD);
			const FBattleShowFrame& Shown = Show.Frame();
			for (const FBattleShowLink& Link : Shown.Order)
			{
				if (Link.Kind == EBattleShowLink::Quad)
				{
					Testing::DigestBattleShowQuad(One, Shown, Shown.Quads[Link.Index]);
				}
				else if (Link.Kind == EBattleShowLink::Fade)
				{
					Testing::DigestBattleShowFade(One, Link, Shown.Fade);
				}
				else if (Link.Kind == EBattleShowLink::Triangle)
				{
					Testing::DigestBattleShowTriangle(One, Shown, Link, Shown.Triangles[Link.Index]);
				}
			}
			One.U8(0xFC);
			C.Shown.DigestVram(One);
		}
		return One.Finish();
	}

	/** The characters standing: a drawn state leaves many knocked out or gone (stand_party). */
	void StandParty(FGameState& State)
	{
		std::span<uint8> Block = State.Block();
		for (std::size_t Character = 0; Character < 11; ++Character)
		{
			const std::size_t Record = 0x26C + Character * 0xA4;
			if (Block[Record + 0x4C] == 0 && Block[Record + 0x4D] == 0)
			{
				Block[Record + 0x4C] = 1;
			}
			Block[Record + 0x7D] &= 0x3F;
		}
	}

	/** The records part's cases: the opening's, with the party standing. */
	struct FRecordsCase : FOpeningCase
	{
		FRecordsCase(FXorShift32& G, const std::vector<FFormationRecord>& Formations) : FOpeningCase(G, Formations) { StandParty(State); }
	};

	/** The gears whole: a drawn state leaves many destroyed or out (stand_gears). */
	void StandGears(FGameState& State)
	{
		std::span<uint8> Block = State.Block();
		for (std::size_t Gear = 0; Gear < GearCount; ++Gear)
		{
			const std::size_t Record = 0x978 + Gear * 0xA4;
			if (LittleEndian::ReadU32(Block, Record + 0x60) == 0)
			{
				Block[Record + 0x60] = 1;
			}
			Block[Record + 0x7D] &= 0x3F;
		}
	}

	/** The model records part's cases: the models part's, with the party and the gears standing. */
	struct FModelRecordsCase : FModelCase
	{
		FModelRecordsCase(FXorShift32& G, const std::vector<FFormationRecord>& Formations) : FModelCase(G, Formations)
		{
			StandParty(State);
			StandGears(State);
		}
	};

	/** The show's own codes a case plays (a 3D actor's among them), the results its targets take and the totals' classes (the oracle's). */
	constexpr std::array<uint8, 5> ControlCodes{0xF3, 0xF6, 0xF7, 0xFC, 0xFD};
	constexpr std::array<uint8, 4> ModelControlCodes{0xF3, 0xF6, 0xF7, 0xFD};

	/** Whether the object's own sequence is left empty or its first instruction jumps to itself (unused_sequence). */
	bool UnusedSequence(const FBattleObject& Object, int32 Sequence)
	{
		const FGearMotionPack& Pack = *Object.Motion;
		const std::optional<uint32> Start = Pack.Script(Sequence);
		if (!Start || LittleEndian::ReadU32(Pack.Bytes, Pack.Scripts + 4 * (static_cast<std::size_t>(Sequence) + 2)) == 0)
		{
			return true;
		}
		return Pack.Has(*Start, 4) && LittleEndian::ReadU16(Pack.Bytes, *Start) == 0x32 && LittleEndian::ReadU16(Pack.Bytes, *Start + 2) == 0;
	}
	constexpr std::array<uint8, 7> TargetResults{0, 1, 2, 3, 4, 5, 7};
	constexpr std::array<uint8, 6> TotalClasses{0, 1, 2, 3, 5, 0xB};
	constexpr int32 StartFrames = 300;
	constexpr int32 PlayFrames = 600;
	constexpr int32 AfterFrames = 4;
	/** The frames digested after the battle's end pose. */
	constexpr int32 EndFrames = 30;

	void DigestText(FOracleDigest& One, std::string_view Text)
	{
		One.Raw(std::span<const uint8>(reinterpret_cast<const uint8*>(Text.data()), Text.size()));
	}

	/**
	 * A frame the rules' code runs, with the frames the show runs inside it, each digested as it is shown and counted in Frame;
	 * false once the show has stopped (a fault: the oracle's case ends there too) or the frames have run past CallFrames.
	 */
	bool PlayFrame(FShowCase& C, FOracleDigest& One, int32& Frame)
	{
		FBattleShow& Show = *C.Show;
		while (Frame < CallFrames)
		{
			Show.BeginFrame();
			if (Show.Fault() != EBattleShowFault::None)
			{
				return false;
			}
			// The screen wave's own vertical blanks, which the original runs inside the frame's task updates.
			while (Show.IsHolding())
			{
				Show.HoldBlank();
			}
			++Frame;
			Show.DisplayFrame();
			// A module read over its running effects stops the frame at its display, before the oracle digests it.
			if (Show.Fault() != EBattleShowFault::None)
			{
				return false;
			}
			C.Shown.DigestPlayed(One);
			if (C.Watch)
			{
				C.Watch(Show);
			}
			const bool bBack = Show.FinishFrame();
			if (Show.Fault() != EBattleShowFault::None)
			{
				return false;
			}
			if (bBack)
			{
				return true;
			}
		}
		C.bEndless = true;
		return false;
	}

	bool PlayFrame(FShowCase& C, FOracleDigest& One)
	{
		int32 Frames = 0;
		return PlayFrame(C, One, Frames);
	}

	/**
	 * The frames a call of the rules' takes in the show, each digested, the call going on at each frame's start; false once the
	 * show has stopped or the call has run past CallFrames.
	 */
	bool RulesWait(FShowCase& C, FOracleDigest& One)
	{
		FBattleShow& Show = *C.Show;
		int32 Frames = 0;
		while (Show.Fault() == EBattleShowFault::None && Show.HoldsRules())
		{
			if (!PlayFrame(C, One, Frames))
			{
				return false;
			}
			Show.ResumeRules();
		}
		return Show.Fault() == EBattleShowFault::None;
	}

	/**
	 * The show's calls at the turn's end (ShowRig.turn_end), each from the rules' code: the markers taken away, the next
	 * ready slot's attack readied, a party actor's drains half the time, then the combatants' states changed and their
	 * poses refreshed.
	 */
	bool RunTurnEnd(FShowCase& C, FXorShift32& G, FOracleDigest& One, int32 Actor)
	{
		FBattleShow& Show = *C.Show;
		FBattleContext& Context = *C.Context;
		Show.SetTargetMarkers(0);
		const uint8 NextReady = FindNextReadySlot(Context, static_cast<uint8>(Actor));
		if (NextReady < FBattleContext::SlotCount)
		{
			Show.PrepareNext(NextReady);
			if (!RulesWait(C, One))
			{
				return false;
			}
		}
		if (Actor < FBattleContext::PartySlotCount && G.Next() % 2 != 0)
		{
			std::array<uint32, 3> Drains{};
			for (uint32& Drain : Drains)
			{
				const bool bDrains = G.Next() % 2 != 0;
				Drain = bDrains ? G.Next() % 400 : 0;
			}
			Show.ShowTurnDamage(static_cast<uint8>(Actor), Drains);
			if (!RulesWait(C, One))
			{
				return false;
			}
		}
		for (int32 Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
		{
			if (Show.Battle.SlotSprites[static_cast<std::size_t>(Slot)] == nullptr)
			{
				continue;
			}
			FBattleCombatant& Combatant = Context.Combatants[static_cast<std::size_t>(Slot)];
			switch (G.Next() % 8)
			{
			case 0: Combatant.Status ^= 0x800; break;
			case 1: Combatant.Mental ^= 0x1000; break;
			case 2: Combatant.Mental ^= 0x2000; break;
			case 3: Combatant.Status ^= 0x8000; break;
			case 4: Combatant.Elements = static_cast<uint16>(Combatant.Elements | 0x100u << G.Next() % 8); break;
			case 5: Combatant.Hp = static_cast<uint16>(Combatant.MaxHp / 10); break;
			default: break;
			}
		}
		Show.RefreshPoses();
		return RulesWait(C, One);
	}

	/**
	 * The show's calls at the battle's end (ShowRig.battle_end), each from the rules' code: the loads finished, a drawn
	 * end pose, the dim half the time, then frames of the show.
	 */
	void RunBattleEnd(FShowCase& C, FXorShift32& G, FOracleDigest& One)
	{
		FBattleShow& Show = *C.Show;
		Show.FinishLoads();
		if (!RulesWait(C, One))
		{
			return;
		}
		Show.PlayEndPose(static_cast<EBattleEndPose>(G.Next() % 3));
		if (!RulesWait(C, One))
		{
			return;
		}
		if (G.Next() % 2 != 0)
		{
			Show.Dim();
		}
		for (int32 Frame = 0; Frame < EndFrames; ++Frame)
		{
			if (!PlayFrame(C, One))
			{
				return;
			}
		}
	}

	/**
	 * An effect's record among Numbers (draw_effect): an item's (F5) or an enemy's cast (F4) of any of them with an animation of
	 * its sprite drawn, or a party member's own code for one of them its codes reach (10h its first; any effect its codes reach
	 * when none of them is).
	 */
	void DrawEffect(const FShowData& Data, const std::vector<int32>& Numbers, FXorShift32& G, const FBattleContext& Context, int32 Actor, bool bItem,
		uint8& Code, uint16& Parameter)
	{
		if (bItem || Actor >= FBattleContext::PartySlotCount)
		{
			const int32 Effect = Numbers[G.Next() % Numbers.size()];
			const uint32 Animation = G.Next() % static_cast<uint32>(std::max(1, Data.EffectAnimations[static_cast<std::size_t>(Effect)]));
			Code = bItem ? 0xF5 : 0xF4;
			Parameter = static_cast<uint16>(static_cast<uint32>(Effect) | Animation << 9);
			return;
		}
		const int32 Base = Data.Program->EffectBases[Context.Slots[static_cast<std::size_t>(Actor)].Occupant];
		const auto InReach = [Base](int32 Effect) { return Effect >= Base && Effect < Base + 0xE0; };
		std::vector<int32> Reached;
		std::copy_if(Numbers.begin(), Numbers.end(), std::back_inserter(Reached), InReach);
		if (Reached.empty())
		{
			std::copy_if(Data.Effects.begin(), Data.Effects.end(), std::back_inserter(Reached), InReach);
		}
		Code = static_cast<uint8>(Reached[G.Next() % Reached.size()] - Base + 0x10);
	}

	/** One case of the records part (ShowRig.records), of the model records part (bModels) or of a part casting effects among Effects. */
	std::string RunRecords(const FShowData& Data, FXorShift32& G, FOracleDigest& One, FOpeningCase& Case, bool bModels, const std::vector<int32>* Effects = nullptr,
		const std::function<void(const FBattleShow&)>& Watch = {})
	{
		FShowCase C(Data, Case);
		C.Watch = Watch;
		FBattleShow& Show = *C.Show;
		FBattleContext& Context = *C.Context;
		for (int32 Frame = 0; Frame < StartFrames && !Show.HasStarted(); ++Frame)
		{
			if (!RunFrame(Show))
			{
				DigestText(One, "ends");
				return "a wait that never ends";
			}
			REQUIRE(Show.Fault() == EBattleShowFault::None);
		}
		if (!Show.HasStarted())
		{
			DigestText(One, "not started");
			return {};
		}
		// The action, drawn and written as the rules leave it.
		const auto InGear = [&](int32 Slot) { return Context.Slots[static_cast<std::size_t>(Slot)].InGear != 0; };
		const auto Usable = [&](int32 Slot) {
			return Show.Battle.SlotSprites[static_cast<std::size_t>(Slot)] != nullptr && (Context.Living >> Slot & 1) != 0 && (bModels || !InGear(Slot));
		};
		std::vector<int32> Actors;
		for (int32 Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
		{
			if (Usable(Slot))
			{
				Actors.push_back(Slot);
			}
		}
		std::vector<int32> Opponents;
		int32 Actor = 0;
		if (!Actors.empty())
		{
			Actor = Actors[G.Next() % Actors.size()];
			for (int32 Slot = Actor < 3 ? 3 : 0; Slot < (Actor < 3 ? FBattleContext::SlotCount : 3); ++Slot)
			{
				if (Usable(Slot))
				{
					Opponents.push_back(Slot);
				}
			}
		}
		if (Opponents.empty())
		{
			DigestText(One, "no action");
			return {};
		}
		const int32 First = Opponents[G.Next() % Opponents.size()];
		std::vector<int32> Targets{First};
		for (const int32 Slot : Opponents)
		{
			if (Slot != First && G.Next() % 2 != 0)
			{
				Targets.push_back(Slot);
			}
		}
		Targets.resize(std::min<std::size_t>(Targets.size(), 3));
		uint16 Mask = 0;
		for (const int32 Slot : Targets)
		{
			Mask = static_cast<uint16>(Mask | 1 << Slot);
		}
		// The animations it plays: a party member's attack file's, an enemy's own; a 3D actor's sequences.
		const bool bModel = bModels && InGear(Actor);
		const FBattleObject* Model = bModel ? Show.Objects.Object(Actor) : nullptr;
		REQUIRE((!bModel || Model != nullptr));
		const FSprite& ActorSprite = *Show.Battle.SlotSprites[static_cast<std::size_t>(Actor)];
		const int32 Own = bModel ? static_cast<int32>(Model->Motion->ScriptEntries) - 1 : ActorSprite.Resource->File->AnimationCount(ActorSprite.Resource->Tables);
		int32 Animations = Own;
		if (!bModel && Actor < 3)
		{
			TResult<FBattleSpriteFile> Attacks = Data.Files->ReadAttacks(Context.Slots[static_cast<std::size_t>(Actor)].Occupant);
			REQUIRE(Attacks);
			Animations = Attacks->Sprite().AnimationCount(Attacks->Sprite().Tables());
		}
		for (FBattlePerformance& Performance : Context.Performances)
		{
			Performance.Code = 0xFF;
		}
		const int32 Count = static_cast<int32>(1 + G.Next() % 4);
		for (int32 Number = 0; Number <= Count; ++Number)
		{
			FBattlePerformance& Performance = Context.Performances[static_cast<std::size_t>(Number)];
			uint16 Parameter = 0;
			uint8 Code = 0xFE;
			const bool bPlays = Number < Count;
			const uint32 Choice = bPlays ? G.Next() % 8 : 0;
			if (bPlays && Effects != nullptr && Choice >= 3 && Choice < 7)
			{
				DrawEffect(Data, *Effects, G, Context, Actor, Choice == 6, Code, Parameter);
			}
			else if (bPlays && Choice < 5)
			{
				Code = static_cast<uint8>(G.Next() % static_cast<uint32>(std::max(1, Animations)));
				if (bModel && UnusedSequence(*Model, Code))
				{
					Code = 0xF7;
				}
			}
			else if (bPlays)
			{
				Code = bModel ? ModelControlCodes[G.Next() % ModelControlCodes.size()] : ControlCodes[G.Next() % ControlCodes.size()];
				if (Code == 0xF7)
				{
					Parameter = static_cast<uint16>(G.Next() % 3);
				}
				else if (Code == 0xFC)
				{
					Parameter = static_cast<uint16>(G.Next() % static_cast<uint32>(std::max(1, Own)));
				}
				else if (Code == 0xFD)
				{
					Parameter = static_cast<uint16>(G.Next() % 2);
				}
			}
			for (int32 Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
			{
				Performance.Amounts[static_cast<std::size_t>(Slot)] = (Mask >> Slot & 1) != 0 ? static_cast<uint16>(G.Next() % 1000) : 0;
			}
			Performance.Targets = Mask;
			for (int32 Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
			{
				Performance.Results[static_cast<std::size_t>(Slot)] = (Mask >> Slot & 1) != 0 ? TargetResults[G.Next() % TargetResults.size()] : 0xFF;
			}
			Performance.Actor = static_cast<uint8>(Actor);
			for (int32 Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
			{
				Performance.Totals[static_cast<std::size_t>(Slot)] = (Mask >> Slot & 1) != 0 ? static_cast<uint16>(G.Next() % 10000) : 0;
			}
			Performance.Parameter = Parameter;
			for (int32 Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
			{
				Performance.TotalKinds[static_cast<std::size_t>(Slot)] = (Mask >> Slot & 1) != 0 ? TotalClasses[G.Next() % TotalClasses.size()] : 0;
			}
			Performance.Code = Code;
		}
		uint16 Knocked = 0;
		for (const int32 Slot : Targets)
		{
			if (G.Next() % 4 == 0)
			{
				Knocked = static_cast<uint16>(Knocked | 1 << Slot);
			}
		}
		Context.Action.KnockedOut = Knocked;
		Context.Party.bStoredCombo = G.Next() % 4 == 0;
		const bool bStays = G.Next() % 2 != 0;
		Context.bActionsPlayed = false;
		if (!bStays)
		{
			PlanBattleApproachPath(Context, static_cast<uint8>(Actor), static_cast<uint8>(First));
		}
		Show.Camera.Frame(static_cast<uint32>(Mask | 1 << Actor), Show.Battle.CameraSubjects(), Show.Gte);
		const uint8 NextReady = FindNextReadySlot(Context, static_cast<uint8>(Actor));
		// The markers over the targets (and the actor) as the party's turn leaves them, a third of the time none.
		const uint32 Marked = G.Next() % 3;
		if (Marked != 0)
		{
			Context.TurnActor = static_cast<uint8>(Actor);
			Show.SetTargetMarkers(static_cast<uint16>(Mask | (Marked == 2 ? 1 << Actor : 0)));
		}

		// The playback from the rules' code, then its frames until the controller has gone and four more.
		std::string Ends;
		Show.BeginPlayback(bStays, static_cast<uint8>(Actor), static_cast<uint8>(First), NextReady);
		bool bRunning = RulesWait(C, One);
		int32 After = AfterFrames;
		for (int32 Frame = 0; bRunning && Frame < PlayFrames; ++Frame)
		{
			bRunning = PlayFrame(C, One);
			if (bRunning && !Show.Playback.HasController() && --After == 0)
			{
				break;
			}
		}
		if (bRunning && !Show.Playback.HasController() && !Show.Battle.bCombatantsHidden && RunTurnEnd(C, G, One, Actor))
		{
			RunBattleEnd(C, G, One);
		}
		if (Show.Fault() != EBattleShowFault::None || C.bEndless)
		{
			const std::optional<FError> SpriteFault = Show.Sprites.FaultError();
			Ends = C.bEndless ? "a wait that never ends"
							  : "fault " + std::to_string(static_cast<int32>(Show.Fault())) + (Show.Objects.Fault ? ": " + Show.Objects.Fault->Message : std::string())
					+ (SpriteFault ? ": " + SpriteFault->Message : std::string());
			DigestText(One, "ends");
		}
		return Ends;
	}
}

TEST_CASE("The battle's show plays an action's records as the original's, frame by frame: the walk out, the animations, the collapse, the walk home and the end", "[Battle][Oracle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Data = std::make_unique<FShowData>();
	REQUIRE(ReadData(*Disc, *Data));
	FXorShift32 G(0x0BE7A002);
	FOracleDigest Whole;
	std::string Cases;
	for (int32 Case = 0; Case < 30; ++Case)
	{
		INFO("case " << Case);
		FOracleDigest One;
		FRecordsCase Drawn(G, Data->Formations);
		const std::string Ends = RunRecords(*Data, G, One, Drawn, false);
		const FSha1::FDigest Digest = One.Finish();
		Whole.Raw(Digest);
		Cases += "records " + std::to_string(Case) + " " + FSha1::ToHex(Digest) + (Ends.empty() ? std::string() : " (ends: " + Ends + ")") + "\n";
	}
	INFO(Cases);
	CHECK(Whole.Hex() == "76843a9eff175b19b258cc8a2125e2b658fd8dd5");
}

TEST_CASE("The battle's show plays 3D combatants' records as the original's, frame by frame: their sequences, the model records, the collapse and the end", "[Battle][Oracle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Data = std::make_unique<FShowData>();
	REQUIRE(ReadData(*Disc, *Data));
	FXorShift32 G(0x0BE7A004);
	FOracleDigest Whole;
	std::string Cases;
	for (int32 Case = 0; Case < 30; ++Case)
	{
		INFO("case " << Case);
		FOracleDigest One;
		FModelRecordsCase Drawn(G, Data->ModelFormations);
		const std::string Ends = RunRecords(*Data, G, One, Drawn, true);
		const FSha1::FDigest Digest = One.Finish();
		Whole.Raw(Digest);
		Cases += "model records " + std::to_string(Case) + " " + FSha1::ToHex(Digest) + (Ends.empty() ? std::string() : " (ends: " + Ends + ")") + "\n";
	}
	INFO(Cases);
	CHECK(Whole.Hex() == "35793835a6291d15d43311afc70bd8e1fee0500b");
}

TEST_CASE("The battle's show plays effects as the original's, frame by frame: the cast, the load, the effect's sprites, the waits and the end", "[Battle][Oracle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Data = std::make_unique<FShowData>();
	REQUIRE(ReadData(*Disc, *Data));
	FXorShift32 G(0x0BE7A005);
	FOracleDigest Whole;
	std::string Cases;
	for (int32 Case = 0; Case < 30; ++Case)
	{
		INFO("case " << Case);
		FOracleDigest One;
		FRecordsCase Drawn(G, Data->FootFormations);
		const std::string Ends = RunRecords(*Data, G, One, Drawn, false, &Data->Effects);
		const FSha1::FDigest Digest = One.Finish();
		Whole.Raw(Digest);
		Cases += "effects " + std::to_string(Case) + " " + FSha1::ToHex(Digest) + (Ends.empty() ? std::string() : " (ends: " + Ends + ")") + "\n";
	}
	INFO(Cases);
	CHECK(Whole.Hex() == "2dc5a2c451501f0c135954adccd52a40c4ff1de9");
}

TEST_CASE("Effects and the sprites they play draw cells whose pictures the texture export keys", "[Battle][Content][TextureRegion]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Data = std::make_unique<FShowData>();
	REQUIRE(ReadData(*Disc, *Data));
	// Every sprite's cell the effect cases draw: the combatants', the effects' (a streamed one through its actor's CLUT,
	// a resident one or a section's through the effect resource's) and the common effects'.
	std::map<FTextureRegion, std::set<uint64>> Drawn;
	const auto Watch = [&Drawn](const FBattleShow& Show)
	{
		const FBattleShowFrame& Shown = Show.Frame();
		for (const FBattleShowQuad& Entry : Shown.Quads)
		{
			if (Shown.Drawers[Entry.Drawer].Kind != EBattleShowDrawer::Sprite)
			{
				continue;
			}
			const FSpriteQuad& Quad = Entry.Quad;
			const auto Texel = [](int32 Value) { return static_cast<uint8>(std::clamp(Value, 0, 255)); };
			const FTextureRegion Cell = TextureRegionBetween(PackTextureView(FTexturePage::FromAttribute(Quad.TexturePage), FClutPosition::FromAttribute(Quad.Clut)),
				Texel(std::min(Quad.U0, Quad.U1)), Texel(std::min(Quad.V0, Quad.V1)), Texel(std::max(Quad.U0, Quad.U1) - 1), Texel(std::max(Quad.V0, Quad.V1) - 1));
			Drawn[Cell].insert(TextureRegionKey(Show.Vram, Cell));
		}
	};
	FXorShift32 G(0x0BE7A005);
	for (int32 Case = 0; Case < 10; ++Case)
	{
		INFO("case " << Case);
		FOracleDigest One;
		FRecordsCase Played(G, Data->FootFormations);
		RunRecords(*Data, G, One, Played, false, &Data->Effects, Watch);
	}
	REQUIRE(Drawn.size() > 1000);
	std::set<uint64> Effects;
	std::set<uint64> Others;
	const auto Into = [](std::set<uint64>& Keys)
	{
		return FBattleSpriteCellVisit([&Keys](const FVram& Vram, const FTextureRegion& Cell, const std::string&) { Keys.insert(TextureRegionKey(Vram, Cell)); });
	};
	REQUIRE_FALSE(ForEachBattleCommonEffectCell(*Data->Files, Into(Effects)));
	for (int32 Effect = 0; Effect < Data->Files->EffectCount(); ++Effect)
	{
		INFO("effect " << Effect);
		REQUIRE_FALSE(ForEachBattleEffectCell(*Data->Files, Effect, Into(Effects)));
	}
	for (int32 Character = 0; Character < FBattleSpriteFiles::CharacterCount; ++Character)
	{
		REQUIRE_FALSE(ForEachBattleCharacterCell(*Data->Files, Character, Into(Others)));
	}
	for (const FBattleSceneGraphics& Scene : Data->Graphics)
	{
		REQUIRE_FALSE(ForEachBattleSceneSpriteCell(Scene, Into(Others)));
	}
	int32 Missing = 0;
	int32 EffectsOnly = 0;
	for (const auto& [Cell, Keys] : Drawn)
	{
		const bool bEffect = std::any_of(Keys.begin(), Keys.end(), [&Effects](uint64 Key) { return Effects.contains(Key); });
		const bool bOther = std::any_of(Keys.begin(), Keys.end(), [&Others](uint64 Key) { return Others.contains(Key); });
		Missing += bEffect || bOther ? 0 : 1;
		EffectsOnly += bEffect && !bOther ? 1 : 0;
	}
	INFO("drawn " << Drawn.size() << ", the effects' alone " << EffectsOnly << ", not exported " << Missing);
	CHECK(EffectsOnly > 50);
	CHECK(Missing == 0);
}

TEST_CASE("The battle's show plays the animation extensions as the original's: waves, shards and scrolls", "[Battle][Oracle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Data = std::make_unique<FShowData>();
	REQUIRE(ReadData(*Disc, *Data));
	FXorShift32 G(0x0BE7A006);
	FOracleDigest Whole;
	std::string Cases;
	for (int32 Case = 0; Case < 12; ++Case)
	{
		INFO("case " << Case);
		FOracleDigest One;
		FRecordsCase Drawn(G, Data->FootFormations);
		const std::string Ends = RunRecords(*Data, G, One, Drawn, false, &Data->ExtensionEffects);
		const FSha1::FDigest Digest = One.Finish();
		Whole.Raw(Digest);
		Cases += "extensions " + std::to_string(Case) + " " + FSha1::ToHex(Digest) + (Ends.empty() ? std::string() : " (ends: " + Ends + ")") + "\n";
	}
	INFO(Cases);
	CHECK(Whole.Hex() == "eca4ee90ec63168107f6ad81b595f3b07f9328f8");
}

TEST_CASE("The battle's show opens battles as the original's, frame by frame: the loads, the combatants, the run-in and the start", "[Battle][Oracle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Data = std::make_unique<FShowData>();
	REQUIRE(ReadData(*Disc, *Data));
	REQUIRE(Data->Formations.size() == 143);
	FXorShift32 G(0x0BE7A001);
	FOracleDigest Whole;
	std::string Cases;
	for (int32 Case = 0; Case < 24; ++Case)
	{
		INFO("case " << Case);
		FOpeningCase Drawn(G, Data->Formations);
		const FSha1::FDigest Digest = RunOpening(*Data, G, Drawn);
		Whole.Raw(Digest);
		Cases += "opening " + std::to_string(Case) + " " + FSha1::ToHex(Digest) + "\n";
	}
	INFO(Cases);
	CHECK(Whole.Hex() == "bb5fca5e34494d8bb6c729d085ff15d760b075e1");
}

TEST_CASE("The battle's show opens battles against 3D enemies as the original's, frame by frame: the models, the gears' loads, the proxies and the start", "[Battle][Oracle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Data = std::make_unique<FShowData>();
	REQUIRE(ReadData(*Disc, *Data));
	FXorShift32 G(0x0BE7A003);
	FOracleDigest Whole;
	std::string Cases;
	for (int32 Case = 0; Case < 24; ++Case)
	{
		INFO("case " << Case);
		FModelCase Drawn(G, Data->ModelFormations);
		const FSha1::FDigest Digest = RunOpening(*Data, G, Drawn);
		Whole.Raw(Digest);
		Cases += "models " + std::to_string(Case) + " " + FSha1::ToHex(Digest) + "\n";
	}
	INFO(Cases);
	CHECK(Whole.Hex() == "582a3c16f03037d914ed3fcd30e2e80a7110d4dc");
}
