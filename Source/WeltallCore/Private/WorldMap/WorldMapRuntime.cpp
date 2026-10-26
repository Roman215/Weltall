#include "WorldMap/WorldMapRuntime.h"

#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/PsxRandom.h"
#include "Input/KernelPad.h"
#include "Media/GameFileIndex.h"
#include "Story/StoryPacks.h"
#include "Timing/PlayClock.h"
#include "WorldMap/WorldMapContext.h"
#include "WorldMap/WorldMapCutscenes.h"
#include "WorldMap/WorldMapOps.h"
#include "WorldMap/WorldMapScene.h"
#include "WorldMap/WorldMapStateLayout.h"

#include <string>
#include <vector>

namespace Weltall
{
	namespace
	{
		using namespace WorldMapOps;

		constexpr uint16 SoftResetButtons = 0x090C;
		constexpr uint16 StartButton = 0x0800;
		constexpr uint16 SelectButton = 0x0100;
		constexpr int32 FirstCutsceneScene = 8;
		/** A running loop's exit code, which the scene table starts each scene with. */
		constexpr int32 LoopExitCode = 2;
		constexpr int32 FieldExit = 0;
		constexpr int32 BattleExit = 1;
		constexpr uint8 FieldProgram = 1;
		constexpr uint8 BattleProgram = 2;
		/** A place record of this kind leads where the story's list says. */
		constexpr int16 StoryPlaceKind = 3;
		constexpr int32 StoryList = 3;
		/** The geometry's screen distance (256), which the camera's preset choice also reads. */
		constexpr int32 ScreenDistance = 0x100;
		constexpr int32 FadeBlendSubtract = 2;
		constexpr int32 FadeStepStart = 4;
		/** The particle effect era 0's scene starts. */
		constexpr int32 EraZeroEffect = 0xE;
		constexpr uint16 CameraYawMenuFlag = 0x2000;
		constexpr uint16 CameraYawMask = 0x3FFF;

		struct FRoutinePair
		{
			EWorldMapRoutine Init;
			EWorldMapRoutine Update;
		};

		using R = EWorldMapRoutine;

		/** The tasks every playable scene starts with, slots 0-14. */
		constexpr FRoutinePair BaseTasks[] = {
			{R::InitFade, R::UpdateFade},
			{R::InitLeader, R::UpdateLeader},
			{R::InitMember2, R::UpdateMember},
			{R::InitMember3, R::UpdateMember},
			{R::InitGear1, R::UpdateGear1},
			{R::InitGear2, R::UpdateGear},
			{R::InitGear3, R::UpdateGear},
			{R::InitVehicle, R::UpdateVehicle},
			{R::InitVehicleParts, R::UpdateVehicleParts},
			{R::InitCameraTarget, R::UpdateCameraTarget},
			{R::InitCameraDistance, R::UpdateCameraDistance},
			{R::InitScreenCentre, R::UpdateScreenCentre},
			{R::InitPlaceName, R::UpdatePlaceName},
			{R::InitNotice, R::UpdateNotice},
			{R::InitFrame, R::UpdateFrame},
		};

		constexpr FRoutinePair Thames{R::InitThames, R::UpdateThames};
		constexpr FRoutinePair Shevat{R::InitShevat, R::UpdateShevat};
		constexpr FRoutinePair FastSpinnerPair{R::InitFastSpinnerPair, R::UpdateFastSpinnerPair};
		constexpr FRoutinePair SpinnerPair{R::InitSpinnerPair, R::UpdateSpinnerPair};
		constexpr FRoutinePair StoryObject{R::InitStoryObject, R::UpdateStoryObject};
		constexpr FRoutinePair ColliderA{R::InitLandmarkColliderA, R::UpdateLandmarkColliderA};
		constexpr FRoutinePair ColliderB{R::InitLandmarkColliderB, R::UpdateLandmarkColliderB};
		constexpr FRoutinePair ColliderC{R::InitLandmarkColliderC, R::UpdateLandmarkColliderC};
		constexpr FRoutinePair EmptySlot{R::InitEmptyEraSlot, R::UpdateEmptyEraSlot};

		/** Each era's own tasks, from slot 15 on. */
		const std::vector<FRoutinePair>& EraTasks(int32 Era)
		{
			static const std::vector<FRoutinePair> Lists[FWorldMapTables::EraCount] = {
				{{R::InitSpinner, R::UpdateSpinner}},
				{},
				{},
				{Thames, Shevat},
				{Thames, Shevat, FastSpinnerPair, SpinnerPair, StoryObject, ColliderC},
				{Thames, Shevat, FastSpinnerPair, SpinnerPair, EmptySlot, ColliderC},
				{Thames, Shevat, FastSpinnerPair, SpinnerPair, EmptySlot, ColliderC, ColliderA},
				{Thames, Shevat, FastSpinnerPair, SpinnerPair, EmptySlot, ColliderC, ColliderA},
				{EmptySlot, EmptySlot, FastSpinnerPair, SpinnerPair, EmptySlot, ColliderC, ColliderA, ColliderB},
			};
			return Lists[Era];
		}

		/** The inits the return from a battle runs again in the kept slots: the fade, the party, the gears, the vehicle, the windows, the frame. */
		struct FRestore
		{
			EWorldMapRoutine Init;
			int32 Slot;
		};

		constexpr FRestore Restores[] = {
			{R::InitFade, 0},
			{R::RestoreLeader, 1},
			{R::RestoreMember2, 2},
			{R::RestoreMember3, 3},
			{R::RestoreGear1, 4},
			{R::RestoreGear2, 5},
			{R::RestoreGear3, 6},
			{R::RestoreVehicle, 7},
			{R::RestoreVehicleParts, 8},
			{R::InitPlaceName, 12},
			{R::InitNotice, 13},
			{R::InitFrame, 14},
		};

		/** The era objects' restores, by the slots the eras' lists give them. */
		const std::vector<FRestore>& EraRestores(int32 Era)
		{
			static const std::vector<FRestore> None;
			static const std::vector<FRestore> Era3{{R::RestoreThames, 15}, {R::RestoreShevat, 16}};
			static const std::vector<FRestore> Era4{{R::RestoreThames, 15}, {R::RestoreShevat, 16}, {R::RestoreFastSpinnerPair, 17},
				{R::RestoreStoryObject, 19}};
			static const std::vector<FRestore> Eras5To7{{R::RestoreThames, 15}, {R::RestoreShevat, 16}, {R::RestoreFastSpinnerPair, 17}};
			static const std::vector<FRestore> Era8{{R::RestoreFastSpinnerPair, 17}};
			switch (Era)
			{
			case 3:
				return Era3;
			case 4:
				return Era4;
			case 5:
			case 6:
			case 7:
				return Eras5To7;
			case 8:
				return Era8;
			default:
				return None;
			}
		}

		uint16 Word(const FGameState& Game, std::size_t Offset)
		{
			return LittleEndian::ReadU16(Game.Block(), Offset);
		}

		void SetWord(FGameState& Game, std::size_t Offset, uint16 Value)
		{
			LittleEndian::WriteU16(Game.Block(), Offset, Value);
		}

		/** The travel mode: the vehicle boarded, or the gears if anyone rides, else on foot; never the vehicle while it is away. */
		std::optional<int32> TravelModeFor(const FGameState& Game, bool bVehicleAway)
		{
			const uint16 Flags = bVehicleAway ? 0 : Word(Game, WorldMapStateLayout::VehicleFlags);
			if ((Flags & WorldMapStateLayout::Aboard) == 0)
			{
				const std::span<const uint8> Block = Game.Block();
				const bool bRiding = Block[WorldMapStateLayout::Riding] != 0 || Block[WorldMapStateLayout::Riding + 1] != 0
					|| Block[WorldMapStateLayout::Riding + 2] != 0;
				return bRiding ? WorldMapTravel::Gears : WorldMapTravel::OnFoot;
			}
			switch (Flags & WorldMapStateLayout::VehicleTypeMask)
			{
			case 1:
				return WorldMapTravel::SandShip;
			case 2:
				return WorldMapTravel::SeaShip;
			case 3:
			case 4:
				return WorldMapTravel::Flying;
			default:
				return std::nullopt;
			}
		}

		FWorldMapVector VehiclePlace(const FGameState& Game)
		{
			FWorldMapVector Place;
			Place.X = static_cast<int32>(static_cast<uint32>(static_cast<int16>(Word(Game, WorldMapStateLayout::VehicleX))) << 12);
			Place.Y = static_cast<int32>(static_cast<uint32>(static_cast<int16>(Word(Game, WorldMapStateLayout::VehicleY))) << 12);
			Place.Z = static_cast<int32>(static_cast<uint32>(static_cast<int16>(Word(Game, WorldMapStateLayout::VehicleZ))) << 12);
			return Place;
		}

		/** Where the menu's save left the party (or its ship). */
		void PlaceFromSave(FWorldMapContext& Context, FGameState& Game)
		{
			SetWord(Game, WorldMapStateLayout::MenuOpen, 0);
			switch (Context.TravelMode)
			{
			case WorldMapTravel::OnFoot:
			case WorldMapTravel::Gears:
				Context.StartYaw = Word(Game, WorldMapStateLayout::LeaderHeading);
				Context.Start.X = static_cast<int32>(static_cast<uint32>(Word(Game, WorldMapStateLayout::PartyX)) << 12);
				Context.Start.Z = static_cast<int32>(static_cast<uint32>(Word(Game, WorldMapStateLayout::PartyZ)) << 12);
				break;
			case WorldMapTravel::SandShip:
			case WorldMapTravel::SeaShip:
			case WorldMapTravel::Flying:
			{
				const FWorldMapVector Place = VehiclePlace(Game);
				Context.Start.X = Place.X;
				Context.Start.Y = Place.Y;
				Context.Start.Z = Place.Z;
				Context.StartYaw = Word(Game, WorldMapStateLayout::VehicleHeading);
				break;
			}
			default:
				break;
			}
		}

		/** The scene's entrance of the id the field handed over (the origin if none), or the ship. */
		void PlaceAtEntrance(FWorldMapContext& Context, FGameState& Game)
		{
			const uint16 Flags = Word(Game, WorldMapStateLayout::VehicleFlags);
			if ((Flags & WorldMapStateLayout::StartAtVehicle) == 0 || Context.bVehicleAway)
			{
				Context.Start = {};
				for (const FWorldMapEntrance& Entrance : Context.Scene->Entrances)
				{
					if (Entrance.Id == Context.Entrance)
					{
						Context.Start.X = static_cast<int32>(static_cast<uint32>(static_cast<int32>(Entrance.X)) << 12);
						Context.Start.Z = static_cast<int32>(static_cast<uint32>(static_cast<int32>(Entrance.Z)) << 12);
						break;
					}
				}
				return;
			}
			SetWord(Game, WorldMapStateLayout::VehicleFlags, static_cast<uint16>(Flags & ~WorldMapStateLayout::StartAtVehicle));
			const FWorldMapVector Place = VehiclePlace(Game);
			Context.Start.X = Place.X;
			Context.Start.Y = Place.Y;
			Context.Start.Z = Place.Z;
			Context.StartYaw = Word(Game, WorldMapStateLayout::VehicleHeading);
		}

		/** The story list's record under the party for the story's value, which then replaces the place's. */
		void FindStoryPlace(FWorldMapContext& Context, uint16 StoryVariable)
		{
			const std::optional<std::vector<FWorldMapTrigger>>& List = Context.Scene->TriggerLists[StoryList];
			if (!List)
			{
				return;
			}
			const int32 X = (Context.Controlled.X >> 12) & 0xFFFF;
			const int32 Z = (Context.Controlled.Z >> 12) & 0xFFFF;
			for (std::size_t Index = 0; Index < List->size(); ++Index)
			{
				const FWorldMapTrigger& Record = (*List)[Index];
				if (Record.X <= X && X <= Record.X + Record.Width && Record.Z <= Z && Z <= Record.Z + Record.Height && Record.Kind == StoryVariable)
				{
					Context.PlaceRecord = Record;
					Context.PlaceList = StoryList;
					Context.PlaceIndex = static_cast<int32>(Index);
					return;
				}
			}
		}
	}

	FWorldMapRuntime::FWorldMapRuntime(const FWorldMapServices& InServices)
		: Services(InServices)
	{
	}

	FWorldMapRuntime::~FWorldMapRuntime() = default;

	FWorldMapContext& FWorldMapRuntime::Context()
	{
		return *MapContext;
	}

	const FWorldMapContext& FWorldMapRuntime::Context() const
	{
		return *MapContext;
	}

	const FWorldMapScene& FWorldMapRuntime::Scene() const
	{
		return *SceneData;
	}

	std::optional<FError> FWorldMapRuntime::Enter()
	{
		if (Phase != EPhase::NotEntered)
		{
			return MakeError("the world map was entered already");
		}
		FGameState& Game = *Services.Game;
		const uint16 SceneWord = Word(Game, WorldMapStateLayout::Scene);
		if (SceneWord == 0)
		{
			return MakeError("the world map was entered with scene 0, the program's own test start with a party of its own");
		}
		const bool bFromBattle = (SceneWord & WorldMapStateLayout::FromBattle) != 0;
		if (bFromBattle && Services.BattleKeep == nullptr)
		{
			return MakeError("the world map returns from a battle without what it kept for it");
		}
		const int32 SceneNumber = SceneWord & 0x7FFF;
		if (SceneNumber >= FirstCutsceneScene)
		{
			Cutscene = FindCutscene(SceneNumber);
			if (Cutscene == nullptr)
			{
				return MakeError("the world map's cutscene scene " + std::to_string(SceneNumber) + " is not built yet");
			}
		}
		const uint16 StoryVariable = Word(Game, WorldMapStateLayout::StoryVariable);
		TResult<FWorldMapSceneChoice> Choice = ChooseWorldMapScene(*Services.Tables, SceneNumber, StoryVariable);
		if (!Choice)
		{
			return Choice.Error();
		}
		if (Choice->Scene.StartExitCode < LoopExitCode)
		{
			return MakeError("the world map's scene " + std::to_string(SceneNumber) + " starts with exit code " + std::to_string(Choice->Scene.StartExitCode)
				+ ", which runs no loop");
		}
		TResult<FWorldMapScene> Read = ReadWorldMapScene(*Services.Image, *Services.Index, Choice->Scene);
		if (!Read)
		{
			return Read.Error();
		}
		SceneData = std::make_unique<FWorldMapScene>(std::move(*Read));
		bool bVehicleAway = false;
		if (Services.Story != nullptr && Cutscene == nullptr && Choice->Era)
		{
			const bool bExpandedStory = Game.StoryMode() == EStoryMode::Expanded;
			if (std::optional<FError> Error = Services.Story->ApplyToWorldMap(*Choice->Era, *SceneData, bExpandedStory))
			{
				return Error;
			}
			bVehicleAway = Services.Story->KeepsVehicleAway(*Choice->Era, StoryVariable, bExpandedStory);
		}
		const std::optional<int32> Mode = TravelModeFor(Game, bVehicleAway);
		if (!Mode)
		{
			return MakeError("the party is aboard a vehicle of type " + std::to_string(Word(Game, WorldMapStateLayout::VehicleFlags) & WorldMapStateLayout::VehicleTypeMask)
				+ ", which the world map has no travel mode for");
		}
		MapContext = std::make_unique<FWorldMapContext>(*Services.Tables, *SceneData);
		FWorldMapContext& Context = *MapContext;
		Context.Game = Services.Game;
		Context.Random = Services.Random;
		Context.Arctangents = Services.Arctangents;
		Context.Text = &Services.Text;
		Context.Sound = Services.Sound;
		Context.Options = Services.Options;
		Context.Encounters.Rate = Services.EncounterRate;
		Context.Sprites.Random = Services.Random;
		Context.Sprites.Arctangents = Services.Arctangents;
		Context.Sprites.bMirroredPartsPlacedByArt = Services.bMirroredSpritePartsPlacedByArt;
		if (Cutscene == nullptr)
		{
			// A cutscene reads no place names or notices.
			TResult<FMessageTable> Messages = SceneData->Messages();
			if (!Messages)
			{
				return Messages.Error();
			}
			Context.Messages = *Messages;
		}

		Context.TravelMode = *Mode;
		Context.bVehicleAway = bVehicleAway;
		Context.ReturnWord = bFromBattle ? 1 : 0;
		Context.Entrance = Word(Game, WorldMapStateLayout::Entrance);
		Context.StartYaw = Word(Game, WorldMapStateLayout::CameraYaw);
		Context.SkipPlaceExit = 0;
		Context.SceneNumber = SceneNumber;
		Context.Location = (Word(Game, WorldMapStateLayout::Place) & 0x3FFF) - 0x400;
		SetWord(Game, WorldMapStateLayout::Scene, static_cast<uint16>(SceneNumber));
		// A cutscene's era is not set: the program's start leaves it 0.
		Context.Era = Choice->Era.value_or(0);
		Context.ExitCode = Choice->Scene.StartExitCode;

		if (auto Error = LoadScene())
		{
			return Error;
		}
		if (Cutscene != nullptr)
		{
			if (Services.Sound != nullptr)
			{
				Services.Sound->SetSceneGroup(Choice->Scene.GroupHeader);
			}
			Context.GearLock = Services.GearLock;
			// The screen's fade before the start draws with the blend the start gives the scene's own fade.
			EntryBlend = Cutscene->FadeBlend;
			StartCutscene(Context, *Cutscene);
		}
		else
		{
			if (auto Error = LoadParty())
			{
				return Error;
			}
			StartSceneSound(Choice->Scene.GroupHeader, bFromBattle);
			StartScene();
		}
		if (Context.Fault)
		{
			return Context.Fault;
		}
		RunTasks(Context);
		if (Context.Fault)
		{
			return Context.Fault;
		}
		Services.Pad->Reset();
		Context.ReturnWord = Context.ExitCode;
		Context.DrawBuffer = 1;
		Context.Running = 1;
		// What the program before left in the GTE is not kept: the first pass projects about the scene's own centre.
		Context.GteCenterY = Context.ScreenCentreY;
		Phase = EPhase::Loop;
		return std::nullopt;
	}

	std::optional<FError> FWorldMapRuntime::LoadScene()
	{
		FWorldMapContext& Context = *MapContext;
		Context.SpriteSlots.clear();
		Context.SpriteSlots.resize(FWorldMapContext::TaskCount);
		Context.MemberCount = 0;
		TResult<FWorldMapParticles> Particles = FWorldMapParticles::FromSection(SceneData->ParticleEffects);
		if (!Particles)
		{
			return Particles.Error();
		}
		Context.Particles = std::move(*Particles);
		return std::nullopt;
	}

	std::optional<FError> FWorldMapRuntime::LoadParty()
	{
		FWorldMapContext& Context = *MapContext;
		const auto ReadSprite = [&](int32 File) -> TResult<FSpriteFile> {
			const FIndexRecord* Record = Services.Index->Find(WorldMapFiles::Directory, File);
			if (Record == nullptr)
			{
				return MakeError("the disc lacks the world map's sprite file " + std::to_string(File));
			}
			TResult<std::vector<uint8>> Bytes = ReadFileBytes(*Services.Image, *Record);
			if (!Bytes)
			{
				return Bytes.Error();
			}
			return FSpriteFile::Parse(*Bytes);
		};
		// The world map's logic reads none of its sprites: whose files they show is the drawing's alone.
		MembersShown = ShownMembers();
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			const uint8 Character = PartyCharacter(Context, Slot);
			Context.MemberFiles[static_cast<std::size_t>(Slot)].reset();
			Context.GearFiles[static_cast<std::size_t>(Slot)].reset();
			Context.GearsShown[static_cast<std::size_t>(Slot)] = FGameState::EmptySlot;
			if (Character == FGameState::EmptySlot)
			{
				continue;
			}
			const uint8 Shown = MembersShown[static_cast<std::size_t>(Slot)];
			TResult<FSpriteFile> Member = ReadSprite(Shown + WorldMapFiles::CharacterSpriteBase);
			if (!Member)
			{
				return Member.Error();
			}
			Context.MemberFiles[static_cast<std::size_t>(Slot)] = std::move(*Member);
			++Context.MemberCount;
			// The gear of the member shown, or where it has none the slot's own member's, which the tasks draw.
			const uint8 ShownGear = CharacterGear(Context, Shown);
			const uint8 Gear = ShownGear != FGameState::EmptySlot ? ShownGear : CharacterGear(Context, Character);
			if (Gear != FGameState::EmptySlot)
			{
				TResult<FSpriteFile> GearFile = ReadSprite(Gear + WorldMapFiles::GearSpriteBase);
				if (!GearFile)
				{
					return GearFile.Error();
				}
				Context.GearFiles[static_cast<std::size_t>(Slot)] = std::move(*GearFile);
				Context.GearsShown[static_cast<std::size_t>(Slot)] = Gear;
			}
		}
		return std::nullopt;
	}

	std::array<uint8, 3> FWorldMapRuntime::ShownMembers() const
	{
		const FWorldMapContext& Context = *MapContext;
		const std::array<uint8, 3> Party{PartyCharacter(Context, 0), PartyCharacter(Context, 1), PartyCharacter(Context, 2)};
		// A member rides or walks as its own slot does, wherever its player placed it: the two are mixed after a
		// battle one member boarded in, and where a member has no gear to mount.
		const std::array<bool, 3> bRides{Riding(Context, 0) == 1, Riding(Context, 1) == 1, Riding(Context, 2) == 1};
		return Services.Game->PartyOrder().ShownAsTheyRide(Party, bRides);
	}

	std::optional<FError> FWorldMapRuntime::ShowArrangedParty()
	{
		if (ShownMembers() == MembersShown)
		{
			return std::nullopt;
		}
		FWorldMapContext& Context = *MapContext;
		struct FKept
		{
			int32 Slot = 0;
			FSpriteResource Places;
			int32 Animation = 0;
			int16 Facing = 0;
			int16 Scale = 0;
		};
		// The leader's task, the two members' and the three gears', in the files' order.
		constexpr int32 PartyTasks = 6;
		constexpr int32 MemberTasks = 3;
		std::vector<FKept> Kept;
		for (int32 Slot = FWorldMapContext::LeaderSlot; Slot < FWorldMapContext::LeaderSlot + PartyTasks; ++Slot)
		{
			const FWorldMapTask& Task = Context.Tasks[static_cast<std::size_t>(Slot)];
			FSprite* Sprite = SlotSprite(Context, Task);
			if (Sprite == nullptr || Sprite->Resource == nullptr)
			{
				continue;
			}
			Kept.push_back({Slot, *Sprite->Resource, SpriteAnimation(Context, Task), Sprite->Facing, Sprite->Scale});
			Context.Sprites.Free(*Sprite);
		}
		if (auto Error = LoadParty())
		{
			return Error;
		}
		for (const FKept& Each : Kept)
		{
			const int32 Party = Each.Slot - FWorldMapContext::LeaderSlot;
			const std::optional<FSpriteFile>& File = Party < MemberTasks ? Context.MemberFiles[static_cast<std::size_t>(Party)]
																		  : Context.GearFiles[static_cast<std::size_t>(Party - MemberTasks)];
			if (!File)
			{
				return MakeError("the world map's party lost a sprite file as its slots were shown anew");
			}
			CreateSprite(Context, Each.Slot, *File, Each.Places.ClutX, Each.Places.ClutY, Each.Places.TextureX, Each.Places.TextureY, Each.Animation, Each.Scale);
			Context.Sprites.SetFacing(*Context.SpriteSlots[static_cast<std::size_t>(Each.Slot)], Each.Facing);
		}
		return std::nullopt;
	}

	void FWorldMapRuntime::StartSceneSound(int32 GroupHeader, bool bFromBattle)
	{
		IWorldMapSound* Sound = Services.Sound;
		if (Sound == nullptr)
		{
			return;
		}
		Sound->SetSceneGroup(GroupHeader);
		if (bFromBattle)
		{
			// The battle's song goes and the one kept from before it goes on where it was, fading in.
			Sound->StopAllSongs();
			Sound->FreeSong();
			Sound->TakeKeptSong();
			Sound->RegisterEffects();
			Sound->ResumeSong(WorldMapFullVolume, WorldMapSongFadeTicks);
			return;
		}
		Sound->StopFieldSong();
		Sound->LoadBank();
		Sound->RegisterEffects();
		Sound->LoadSong(MapContext->TravelMode == WorldMapTravel::Flying ? EWorldMapSong::Vehicle : EWorldMapSong::Map);
		Sound->PlaySong(WorldMapFullVolume, 0);
	}

	void FWorldMapRuntime::StartScene()
	{
		FWorldMapContext& Context = *MapContext;
		FGameState& Game = *Services.Game;
		Context.ProjectionDistance = ScreenDistance;
		Context.Tasks = {};
		Context.FadeBlend = FadeBlendSubtract;
		Context.FadeStep = FadeStepStart;
		Context.MenuRequested = 0;
		Context.GearButtons = 0;
		Context.GearButtonsBefore = 0;
		Context.GearToggle = 0;
		Context.LookAt = 0;
		Context.Sprites.TickExtraFrames = 1;
		Context.Fading = 1;
		if (Word(Game, WorldMapStateLayout::MenuOpen) != 0)
		{
			PlaceFromSave(Context, Game);
		}
		else if (Context.ReturnWord == 0)
		{
			PlaceAtEntrance(Context, Game);
		}
		else
		{
			RestoreAfterBattle(*Services.BattleKeep);
			ApplyMountFlags(Context);
		}
		InitObjects(Context);
		if (Context.Fault)
		{
			return;
		}
		Context.ShadowCount = 0;
		InitClouds(Context);
		if (Context.ReturnWord == 0)
		{
			for (const FRoutinePair& Pair : BaseTasks)
			{
				AddTask(Context, Pair.Init, Pair.Update);
			}
			for (const FRoutinePair& Pair : EraTasks(Context.Era))
			{
				AddTask(Context, Pair.Init, Pair.Update);
			}
		}
		else
		{
			for (const FRestore& Restore : Restores)
			{
				SetTask(Context, Restore.Init, Restore.Slot);
			}
			for (const FRestore& Restore : EraRestores(Context.Era))
			{
				SetTask(Context, Restore.Init, Restore.Slot);
			}
		}
		Context.GearLock = Context.Era == 0 ? 1 : 0;
		if (Context.Era == 0)
		{
			Context.Particles.Start(EraZeroEffect, std::nullopt, std::nullopt);
		}
		if (Context.ReturnWord == 0)
		{
			ResetEncounters(Context);
		}
	}

	void FWorldMapRuntime::RestoreAfterBattle(const FWorldMapBattleKeep& Keep)
	{
		FWorldMapContext& Context = *MapContext;
		Context.Tasks = Keep.Tasks;
		Context.Start = Keep.Controlled;
		Context.Controlled = Keep.Controlled;
		Context.ControlledHeading = Keep.ControlledHeading;
		Context.Encounters.Timers = Keep.EncounterTimers;
		Context.Encounters.Period = Keep.EncounterPeriod;
		Context.Encounters.Slots = Keep.EncounterSlots;
		Context.Encounters.Left = Keep.EncounterLeft;
		Context.Trail = Keep.Trail;
		Context.CameraPitch = Keep.CameraPitch;
		Context.CameraYaw = Keep.CameraYaw;
		Context.CameraRoll = Keep.CameraRoll;
		Context.CameraTarget = Keep.CameraTarget;
		Context.TrailIndex = Keep.TrailIndex;
		Context.CameraDistance = Keep.CameraDistance;
		Context.ScreenCentreY = Keep.ScreenCentreY;
	}

	std::shared_ptr<const FWorldMapBattleKeep> FWorldMapRuntime::SaveForBattle() const
	{
		const FWorldMapContext& Context = *MapContext;
		auto Keep = std::make_shared<FWorldMapBattleKeep>();
		Keep->Tasks = Context.Tasks;
		// The program frees the sprites before it keeps the slots.
		for (FWorldMapTask& Task : Keep->Tasks)
		{
			Task.Sprite = FWorldMapTask::NoSprite;
		}
		Keep->Controlled = Context.Controlled;
		Keep->ControlledHeading = Context.ControlledHeading;
		Keep->EncounterPeriod = Context.Encounters.Period;
		Keep->EncounterSlots = Context.Encounters.Slots;
		Keep->EncounterLeft = Context.Encounters.Left;
		Keep->EncounterTimers = Context.Encounters.Timers;
		Keep->Trail = Context.Trail;
		Keep->TrailIndex = Context.TrailIndex;
		Keep->CameraPitch = Context.CameraPitch;
		Keep->CameraYaw = Context.CameraYaw;
		Keep->CameraRoll = Context.CameraRoll;
		Keep->CameraDistance = Context.CameraDistance;
		Keep->ScreenCentreY = Context.ScreenCentreY;
		Keep->CameraTarget = Context.CameraTarget;
		return Keep;
	}

	int32 FWorldMapRuntime::VblanksPerTick() const
	{
		return Phase == EPhase::Loop ? 2 : 1;
	}

	void FWorldMapRuntime::RunVblanks(std::span<const FPadState> HeldPerVblank)
	{
		FGameState& Game = *Services.Game;
		for (const FPadState Held : HeldPerVblank)
		{
			// The console counts the vertical blank before it reads the pad: the repeat keeps to the count.
			Game.SetPlayTime(Game.PlayTime() + 1);
			Services.Pad->Vblank(Held, Game.PlayTime());
			Services.Clock->Vblank();
		}
	}

	void FWorldMapRuntime::TakeInput()
	{
		const FKernelPadWords Words = Services.Pad->TakeMerged();
		MapContext->Pad.Held = Words.Held;
		MapContext->Pad.Pressed = Words.Pressed;
		MapContext->Pad.StickHeld = Words.StickHeld;
		MapContext->Pad.StickX = Words.StickX;
		MapContext->Pad.StickY = Words.StickY;
		MapContext->Stick = MapContext->bAnalogMovement ? ReadAnalogStick(Words.Held, Words.StickHeld, Words.StickX, Words.StickY) : std::nullopt;
	}

	std::optional<FError> FWorldMapRuntime::Tick(std::span<const FPadState> HeldPerVblank, IWorldMapDrawing* Drawing)
	{
		if (PendingRequest.Kind != EWorldMapRequest::None)
		{
			return MakeError("the world map waits on its request");
		}
		if (Phase == EPhase::NotEntered || Phase == EPhase::Stopped)
		{
			return MakeError("the world map is not running");
		}
		if (static_cast<int32>(HeldPerVblank.size()) != VblanksPerTick())
		{
			return MakeError("the world map's frame lasts " + std::to_string(VblanksPerTick()) + " vertical blanks, not "
				+ std::to_string(HeldPerVblank.size()));
		}
		bPassEnded = false;
		switch (Phase)
		{
		case EPhase::Loop:
		{
			LastFrameKind = EWorldMapFrame::Tick;
			// Who rides changed since the party's sprites were made (a mount, a battle one member boarded in): the
			// slots show their members anew before this tick draws them.
			if (!MapContext->bCutscene)
			{
				if (auto Error = ShowArrangedParty())
				{
					return Error;
				}
			}
			MapContext->Drawing = Drawing;
			std::optional<FError> Result = RunPass(HeldPerVblank);
			MapContext->Drawing = nullptr;
			if (Phase == EPhase::Loop)
			{
				EndPass();
			}
			bPassEnded = Phase != EPhase::PauseEntry;
			return Result;
		}
		case EPhase::PauseEntry:
			LastFrameKind = EWorldMapFrame::Paused;
			RunVblanks(HeldPerVblank);
			Phase = EPhase::Paused;
			return std::nullopt;
		case EPhase::Paused:
			LastFrameKind = EWorldMapFrame::Paused;
			RunVblanks(HeldPerVblank);
			TakeInput();
			if (bPauseForPad ? Services.Pad->IsConnected() : (MapContext->Pad.Pressed & StartButton) != 0)
			{
				Phase = EPhase::PauseExit;
			}
			return std::nullopt;
		case EPhase::PauseExit:
			LastFrameKind = EWorldMapFrame::Paused;
			RunVblanks(HeldPerVblank);
			if (Services.Sound != nullptr)
			{
				Services.Sound->Resume();
			}
			Services.Game->SetPlayTime(PauseStartPlayTime);
			Phase = EPhase::Loop;
			{
				std::optional<FError> Result = bPauseForPad ? AfterPadWait() : AfterPause();
				if (Phase == EPhase::Loop)
				{
					EndPass();
				}
				bPassEnded = true;
				return Result;
			}
		default:
			return std::nullopt;
		}
	}

	std::optional<FError> FWorldMapRuntime::RunPass(std::span<const FPadState> HeldPerVblank)
	{
		FWorldMapContext& Context = *MapContext;
		TakeInput();
		Context.DrawBuffer = Context.DrawBuffer == 0 ? 1 : 0;
		Context.FadeShown = -1;
		Context.bNoticeBackdrop = false;
		Context.Sprites.BuildPendingFrames();
		Context.Ground.ClearReadOutside();
		Context.bPartyPaced = false;
		RunTasks(Context);
		if (!Context.bPartyPaced)
		{
			RestorePartyPace(Context);
		}
		if (Context.Fault)
		{
			Phase = EPhase::Stopped;
			return Context.Fault;
		}
		if (Context.Ground.ReadOutside())
		{
			Phase = EPhase::Stopped;
			return MakeError("the world map read terrain outside its blocks");
		}
		if (auto SpriteFault = Context.Sprites.FaultError())
		{
			Phase = EPhase::Stopped;
			return SpriteFault;
		}
		RunVblanks(HeldPerVblank);
		if (Services.Pad->Held() == SoftResetButtons)
		{
			FWorldMapRequest Reset;
			Reset.Kind = EWorldMapRequest::SoftReset;
			Stop(Reset);
			return std::nullopt;
		}
		CheckGearButtons();
		if (Context.Fading == 0 && Context.MenuRequested == 0 && Context.Running != 0 && Context.Encounters.Fired == 0
			&& (Context.Pad.Pressed & StartButton) != 0)
		{
			BeginPause(false);
			return std::nullopt;
		}
		return AfterPause();
	}

	void FWorldMapRuntime::BeginPause(bool bForPad)
	{
		// Neither the pause nor the wait for a controller counts as play time; the sound stands still meanwhile.
		PauseStartPlayTime = Services.Game->PlayTime();
		bPauseForPad = bForPad;
		if (Services.Sound != nullptr)
		{
			Services.Sound->Pause();
		}
		Phase = EPhase::PauseEntry;
	}

	void FWorldMapRuntime::EndPass()
	{
		// The loop's last word before it draws the frame: the screen's centre the next pass projects about.
		MapContext->GteCenterY = MapContext->ScreenCentreY;
	}

	void FWorldMapRuntime::CheckGearButtons()
	{
		FWorldMapContext& Context = *MapContext;
		const bool bAllowed = Context.GearLock == 0 && Context.GearToggle != 0 && Context.Fading == 0 && Context.MenuRequested == 0
			&& Context.Place == -1 && Context.Notice == -1 && Context.Running != 0 && Context.Encounters.Fired == 0;
		Context.GearToggle = 0;
		if (!bAllowed || Context.Ground.TerrainType(Context.Controlled.X, Context.Controlled.Z) == WorldMapTerrainType::Forest)
		{
			return;
		}
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			SetStateWord(Context, WorldMapStateLayout::BattleRiding + 2 * static_cast<std::size_t>(Slot), Riding(Context, Slot));
		}
		if (Riding(Context, 0) == 0)
		{
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				if (CharacterGear(Context, PartyCharacter(Context, Slot)) != FGameState::EmptySlot)
				{
					SetRiding(Context, Slot, 1);
				}
			}
		}
		else
		{
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				SetRiding(Context, Slot, 0);
			}
		}
		ApplyMountFlags(Context);
	}

	std::optional<FError> FWorldMapRuntime::AfterPause()
	{
		const FWorldMapContext& Context = *MapContext;
		if (Context.Fading == 0 && Context.MenuRequested == 0 && Context.Running != 0 && Context.Encounters.Fired == 0 && !Services.Pad->IsConnected())
		{
			BeginPause(true);
			return std::nullopt;
		}
		return AfterPadWait();
	}

	std::optional<FError> FWorldMapRuntime::AfterPadWait()
	{
		FWorldMapContext& Context = *MapContext;
		FGameState& Game = *Services.Game;
		if (Context.Fading == 0 && Context.MenuRequested == 0 && Context.Place == -1 && Context.Notice == -1 && Context.Running != 0
			&& Context.Encounters.Fired != 0)
		{
			const std::optional<uint8> Formation = PickWorldMapFormation(Context.Ground, SceneData->Regions, Context.Controlled,
				Word(Game, WorldMapStateLayout::StoryVariable), *Services.Random);
			if (Formation)
			{
				Context.Running = 0;
				for (int32 Slot = 0; Slot < 3; ++Slot)
				{
					SetStateWord(Context, WorldMapStateLayout::BattleRiding + 2 * static_cast<std::size_t>(Slot), Riding(Context, Slot));
				}
				Context.ExitCode = BattleExit;
				PendingRequest.Formation = *Formation;
				PendingRequest.Region = WorldMapEncounterRegion(Context.Ground, Context.Controlled);
			}
		}
		Context.Encounters.Fired = 0;
		if ((Context.Pad.Pressed & SelectButton) != 0)
		{
			SetWord(Game, WorldMapStateLayout::MinimapHidden, static_cast<uint16>(Word(Game, WorldMapStateLayout::MinimapHidden) ^ 1));
		}
		if (Context.Fading == 0 && Context.MenuRequested != 0 && Context.Running != 0)
		{
			if (Context.TravelMode > 0 && Context.TravelMode < WorldMapTravel::SandShip)
			{
				// The menu's load: the menu saves where the party stands and turns, and who rides.
				SetWord(Game, WorldMapStateLayout::MenuOpen, 1);
				SetWord(Game, WorldMapStateLayout::CameraYaw, static_cast<uint16>((Context.CameraYaw + CameraYawMenuFlag) & CameraYawMask));
				for (int32 Slot = 0; Slot < 3; ++Slot)
				{
					SetStateWord(Context, WorldMapStateLayout::BattleRiding + 2 * static_cast<std::size_t>(Slot), Riding(Context, Slot));
				}
				GearLockBeforeMenu = Context.GearLock;
				if (Context.Ground.TerrainType(Context.Controlled.X, Context.Controlled.Z) == WorldMapTerrainType::Forest)
				{
					Context.GearLock = 1;
				}
				FWorldMapRequest Menu;
				Menu.Kind = EWorldMapRequest::Menu;
				PendingRequest = Menu;
				return std::nullopt;
			}
			if (Context.TravelMode >= WorldMapTravel::SandShip && Context.TravelMode <= WorldMapTravel::Flying)
			{
				Context.Running = 0;
				Context.ExitCode = FieldExit;
				Context.PlaceRecord = Services.Tables->InteriorPlace;
				Context.PlaceList = 4;
				Context.PlaceIndex = 2;
				SetWord(Game, WorldMapStateLayout::VehicleFlags,
					static_cast<uint16>(Word(Game, WorldMapStateLayout::VehicleFlags) | WorldMapStateLayout::StartAtVehicle));
			}
		}
		else
		{
			Context.MenuRequested = 0;
		}
		if (Context.Running == 0)
		{
			return Leave();
		}
		return std::nullopt;
	}

	std::optional<FError> FWorldMapRuntime::ResumeAfterMenu()
	{
		if (PendingRequest.Kind != EWorldMapRequest::Menu)
		{
			return MakeError("the world map is not waiting on the menu");
		}
		FWorldMapContext& Context = *MapContext;
		PendingRequest = {};
		Services.Pad->Reset();
		Context.MenuRequested = 0;
		SetWord(*Services.Game, WorldMapStateLayout::MenuOpen, 0);
		Context.GearLock = GearLockBeforeMenu;
		ApplyMountFlags(Context);
		return ShowArrangedParty();
	}

	std::optional<FError> FWorldMapRuntime::Leave()
	{
		FWorldMapContext& Context = *MapContext;
		FGameState& Game = *Services.Game;
		FWorldMapRequest Request = PendingRequest;
		IWorldMapSound* Sound = Services.Sound;
		if (Cutscene != nullptr)
		{
			Cutscene->End(Context);
		}
		else if (Sound != nullptr)
		{
			// The scene's end: for a place the song fades out (and plays on silent: nothing frees it).
			if (Context.ExitCode == FieldExit)
			{
				Sound->SetSongVolume(0, WorldMapSongFadeTicks);
			}
			Sound->StopAllEffects();
			Sound->UnregisterEffects();
		}
		if (Context.ExitCode == FieldExit)
		{
			// A voyage's end and a cutscene's have written the field's words themselves; any other leave reads the place's record.
			if (Context.SkipPlaceExit == 0 && Cutscene == nullptr)
			{
				if (!Context.PlaceRecord)
				{
					Phase = EPhase::Stopped;
					return MakeError("the world map left for the field with no place's record");
				}
				if (Context.PlaceRecord->Kind == StoryPlaceKind)
				{
					FindStoryPlace(Context, Word(Game, WorldMapStateLayout::StoryVariable));
				}
				SetWord(Game, WorldMapStateLayout::Place, static_cast<uint16>(Context.PlaceRecord->Map));
				SetWord(Game, WorldMapStateLayout::CameraYaw, static_cast<uint16>(Context.CameraYaw));
				SetWord(Game, WorldMapStateLayout::Scene, static_cast<uint16>(Context.PlaceRecord->Scene));
			}
			SetWord(Game, WorldMapStateLayout::LocationVariable, static_cast<uint16>(Context.Location + 0x400));
			Request.Kind = EWorldMapRequest::Field;
			Request.Record = Context.PlaceRecord.value_or(FWorldMapTrigger{});
			Request.NextProgram = FieldProgram;
		}
		else if (Context.ExitCode == BattleExit)
		{
			SetWord(Game, WorldMapStateLayout::Scene, static_cast<uint16>(Word(Game, WorldMapStateLayout::Scene) | WorldMapStateLayout::FromBattle));
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				SetStateWord(Context, WorldMapStateLayout::BattleRiding + 2 * static_cast<std::size_t>(Slot), Riding(Context, Slot));
			}
			Request.Kind = EWorldMapRequest::Battle;
			Request.NextProgram = BattleProgram;
			Request.Keep = SaveForBattle();
			if (Sound != nullptr)
			{
				// The world map starts the battle's song itself, keeping its own for the return.
				Sound->StopAllSongs();
				Sound->KeepSong();
				Sound->LoadSong(EWorldMapSong::Battle);
				Sound->PlaySong(WorldMapFullVolume, 0);
			}
		}
		else
		{
			Phase = EPhase::Stopped;
			return MakeError("the world map's loop ended with exit code " + std::to_string(Context.ExitCode));
		}
		Stop(Request);
		return std::nullopt;
	}

	void FWorldMapRuntime::Stop(const FWorldMapRequest& Request)
	{
		PendingRequest = Request;
		Phase = EPhase::Stopped;
	}
}
