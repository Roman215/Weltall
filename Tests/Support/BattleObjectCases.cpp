#include "Support/BattleObjectCases.h"

#include "Battle/Show/BattleSceneModels.h"
#include "Graphics/GteVectors.h"
#include "Media/Iso9660.h"
#include "Media/PsxExecutable.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <type_traits>

namespace Weltall::Testing
{
	namespace
	{
		constexpr int32 StageCount = 75;
		constexpr int32 SceneCount = 76;
		constexpr int32 PartySlots = 3;
		constexpr int32 SlotCount = 11;
		constexpr int32 SceneEntrySlots = 5;
		constexpr FGearTexturePlace SpecialPlace{0x300, 0x100, 0, 0x1DB};
		constexpr std::array<uint8, 11> ResultCodes{0, 1, 2, 3, 4, 5, 6, 7, 10, 11, 0xFF};
		constexpr std::array<uint8, 8> RecordCodes{0, 1, 5, 0x10, 0xF4, 0xF7, 0xFE, 0xFF};
		/** The combatant bytes the cases draw: from the gear's model bytes past its variant, 1Fh of them. */
		constexpr std::size_t DrawnBytes = 0x101;
		constexpr std::size_t DrawnByteCount = 0x1F;
		/** Where the cases place the slots: the stages' layouts place them within 0 to 7360. */
		constexpr uint32 StagePlaces = 0x2000;
		/** The occupancy's groups of the enemies in gears, as four bytes each. */
		constexpr std::size_t EnemyGearGroups = 24;

		/** Byte Offset of a combatant's record, as the cases draw it: into the member that holds it (none for a byte the port does not keep). */
		void SetCombatantByte(FBattleCombatant& Combatant, std::size_t Offset, uint8 Value)
		{
			const std::size_t ModelBytes = FBattleCombatant::CharacterRecordSize + FBattleGearPart::ModelBytesOffset;
			if (Offset >= ModelBytes && Offset < ModelBytes + Combatant.GearPart.ModelBytes.size())
			{
				Combatant.GearPart.ModelBytes[Offset - ModelBytes] = Value;
				return;
			}
			VisitBattleCombatantLayout(Combatant, [&](std::size_t At, auto& Member) {
				using FMember = std::remove_cvref_t<decltype(Member)>;
				using FUnsigned = std::make_unsigned_t<FMember>;
				if (Offset >= At && Offset < At + sizeof(FMember))
				{
					const uint32 Shift = static_cast<uint32>(8 * (Offset - At));
					const uint32 Bits = static_cast<uint32>(static_cast<FUnsigned>(Member));
					Member = static_cast<FMember>(static_cast<FUnsigned>((Bits & ~(0xFFu << Shift)) | static_cast<uint32>(Value) << Shift));
				}
			});
		}

		/** ObjectRig.reset: what the rules and the kernel keep that the objects read. */
		void DrawWorld(FXorShift32& G, FBattleObjectCase& Case)
		{
			const uint32 SetCount = 1 + G.Next() % 3;
			for (uint32 Set = 0; Set < SetCount; ++Set)
			{
				Case.Services.Registered.insert(static_cast<uint16>(G.Next()));
			}
			Case.Random = FPsxRandom(G.Next());
			Case.Camera.Init();
			for (FGteVector* Point : {&Case.Camera.Eye, &Case.Camera.Target})
			{
				for (int16& Axis : *Point)
				{
					Axis = static_cast<int16>(DrawSigned(G, 11));
				}
			}
			Case.Camera.bLocked = G.Next() % 8 == 0;
			Case.Gte.OffsetX = 0xA0 << 16;
			Case.Gte.OffsetY = 0xA4 << 16;
			Case.Gte.ProjectionDistance = 0x200;
			FBattleContext& Context = *Case.Context;
			for (FBattleSlot& Slot : Context.Slots)
			{
				Slot.Occupant = 0;
				Slot.Group = static_cast<uint8>(G.Next() % 8);
				Slot.Place = static_cast<uint8>(G.Next() % 4);
				Slot.Position.X = static_cast<uint16>(G.Next() % StagePlaces);
				Slot.Position.Z = static_cast<uint16>(G.Next() % StagePlaces);
			}
			for (FBattlePerformance& Record : Context.Performances)
			{
				for (uint8& Result : Record.Results)
				{
					Result = ResultCodes[G.Next() % ResultCodes.size()];
				}
				Record.Code = RecordCodes[G.Next() % RecordCodes.size()];
			}
			Case.Services.Record = static_cast<int32>(1 + G.Next() % 32);
			Context.Action.KnockedOut = static_cast<uint16>(G.Next() & 0x7FF);
			Case.Objects.BattleKind = static_cast<uint8>(G.Next() % 2);
			Case.Services.bEffectLoaded = G.Next() % 2 != 0;
			for (FBattleCombatant& Combatant : Context.Combatants)
			{
				Combatant.Absorption = static_cast<uint16>(G.Next());
				for (std::size_t Byte = 0; Byte < DrawnByteCount; ++Byte)
				{
					SetCombatantByte(Combatant, DrawnBytes + Byte, static_cast<uint8>(G.Next()));
				}
				Combatant.GearPart.Hp = G.Next() % 5000;
				Combatant.GearPart.MaxHp = G.Next() % 5000;
				Combatant.GearPart.Status = static_cast<uint16>(G.Next());
				Combatant.AttackLevel = static_cast<uint8>(G.Next() % 6);
			}
			for (std::size_t Group = EnemyGearGroups; Group < Context.Occupancy.size(); ++Group)
			{
				Context.Occupancy[Group].Count = static_cast<uint8>(G.Next() % 3);
				Context.Occupancy[Group].Places = static_cast<uint8>(G.Next() % 3);
				G.Next();
				G.Next();
			}
			Case.Services.SystemSet = static_cast<uint16>(G.Next());
			Case.Services.EventSet = static_cast<uint16>(G.Next());
		}

		/** ObjectRig.stage: the stage's object in slot 31 with its sequence 0, then its floor, then the shadows on. */
		void MakeStage(FXorShift32& G, FBattleObjectCase& Case, const FBattleObjectData& Data, std::optional<int32> Choice)
		{
			const uint32 Drawn = G.Next() % StageCount;
			const FBattleStageFiles& Stage = Data.Stages[Choice ? static_cast<std::size_t>(*Choice) : Drawn];
			Case.Objects.Stage = &Stage.Record;
			Case.Objects.Init(Stage.Record);
			Case.Objects.CreateStage(Stage);
			Case.Floor.Reset(Stage.Record.FloorVertices(), Stage.Record.FloorTriangles());
			Case.Objects.EnableShadows();
		}

		/** ObjectRig.scene: a scene's enemies in slots 3 on, its model entries made into objects as the setup makes them. */
		void MakeScene(FXorShift32& G, FBattleObjectCase& Case, const FBattleObjectData& Data, const FBattleObjectCaseChoice& Choice)
		{
			std::size_t Drawn = G.Next() % Data.ModelScenes.size();
			if (Choice.Scene)
			{
				const auto Chosen = std::find(Data.ModelSceneNumbers.begin(), Data.ModelSceneNumbers.end(), *Choice.Scene);
				REQUIRE(Chosen != Data.ModelSceneNumbers.end());
				Drawn = static_cast<std::size_t>(Chosen - Data.ModelSceneNumbers.begin());
			}
			const FBattleSceneGraphics& Scene = Data.ModelScenes[Drawn];
			const uint32 Entries = static_cast<uint32>(Scene.Entries().size());
			const int32 Enemies = static_cast<int32>(1 + G.Next() % SceneEntrySlots);
			std::array<uint8, SlotCount> Types{};
			for (int32 Slot = PartySlots; Slot < SlotCount; ++Slot)
			{
				uint8 Occupant = Slot < PartySlots + Enemies ? static_cast<uint8>(G.Next() % Entries) : FBattleSlot::Empty;
				if (Slot == PartySlots && Choice.Entry)
				{
					Occupant = *Choice.Entry;
				}
				Case.Context->Slots[static_cast<std::size_t>(Slot)].Occupant = Occupant;
				Types[static_cast<std::size_t>(Slot)] = Occupant;
			}
			const std::optional<std::vector<int16>> Columns = BattleSceneColumns(Scene);
			REQUIRE(Columns);
			for (std::size_t Index = 0; Index < Scene.Entries().size(); ++Index)
			{
				const FBattleSceneEntry& Entry = Scene.Entries()[Index];
				if (!Entry.IsSprite() && Entry.HasOwnImages())
				{
					const FBattleSceneModels Made = CreateBattleSceneModels(Case.Objects, Scene, Index, (*Columns)[Index], Types);
					REQUIRE_FALSE(Made.bOutsideFile);
				}
			}
		}

		/** One to three party members in gears, each made as the battle makes a gear's model (none after a fault). */
		void MakeGears(FXorShift32& G, FBattleObjectCase& Case, const FBattleObjectData& Data)
		{
			const int32 Party = static_cast<int32>(1 + G.Next() % 3);
			for (int32 Slot = 0; Slot < PartySlots; ++Slot)
			{
				Case.Context->Slots[static_cast<std::size_t>(Slot)].Occupant = Slot < Party ? static_cast<uint8>(G.Next() % 11) : FBattleSlot::Empty;
			}
			for (int32 Slot = 0; Slot < Party; ++Slot)
			{
				const uint8 Gear = static_cast<uint8>(G.Next() % FBattleObjectTables::GearCount);
				FBattleCombatant& Combatant = Case.Context->Combatants[static_cast<std::size_t>(Slot)];
				Combatant.Gear = Gear;
				Combatant.GearPart.ModelBytes[0] = static_cast<uint8>(G.Next() % (Data.Tables->Gears[Gear].LastVariant + 2u));
				Case.Objects.ReadGearModelFiles(Slot);
				Case.Objects.CreateGearModel(Slot);
				if (Case.Objects.Faulted())
				{
					return;
				}
			}
		}

		/** ObjectRig.aimed_slot: the first enemy slot holding Entry, with its object. */
		std::optional<int32> AimedSlot(const FBattleObjectCase& Case, std::optional<uint8> Entry)
		{
			if (!Entry)
			{
				return std::nullopt;
			}
			for (int32 Slot = PartySlots; Slot < SlotCount; ++Slot)
			{
				if (Case.Context->Slots[static_cast<std::size_t>(Slot)].Occupant == *Entry && Case.Objects.Object(Slot) != nullptr)
				{
					return Slot;
				}
			}
			return std::nullopt;
		}

		/**
		 * ObjectRig.aim: the view's translation putting the slot's place on the floor (where its object stands from its
		 * first frame) a quarter of the drawn offsets off the screen's middle, at the drawn depth.
		 */
		void Aim(FGteMatrix& View, const FBattleSlot& Slot)
		{
			const int32 X = Slot.Position.X;
			const int32 Z = Slot.Position.Z;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				const int32 Placed = (View.M[Axis][0] * X + View.M[Axis][2] * Z) >> 12;
				View.T[Axis] = (Axis < 2 ? View.T[Axis] >> 2 : View.T[Axis]) - Placed;
			}
		}

		std::string FaultOf(const FBattleObjects& Objects)
		{
			return Objects.Fault ? Objects.Fault->Message : std::string();
		}

		/** Whether the object's sequence is one left unused: an empty entry of its table, or a jump to itself, which never ends. */
		bool Unused(const FBattleObject& Object, int32 Sequence)
		{
			const FGearMotionPack& Motion = *Object.Motion;
			const std::optional<uint32> Start = Motion.Script(Sequence);
			if (!Start || *Start == Motion.Scripts)
			{
				return true;
			}
			constexpr std::array<uint8, 4> JumpToItself{0x32, 0, 0, 0};
			return Motion.Has(*Start, static_cast<uint32>(JumpToItself.size())) && std::equal(JumpToItself.begin(), JumpToItself.end(), Motion.Bytes.begin() + *Start);
		}
	}

	bool ReadBattleObjectData(const FIso9660& Disc, FBattleObjectData& Data)
	{
		const auto Executable = ReadBootExecutable(Disc);
		if (!Executable)
		{
			return false;
		}
		auto Index = FGameFileIndex::Read(*Executable);
		const auto Arctangents = FGteArctangents::Read(*Executable);
		if (!Index || !Arctangents)
		{
			return false;
		}
		Data.Image = &Disc.Image();
		Data.Index = std::move(*Index);
		Data.Arctangents = *Arctangents;
		auto Tables = FBattleObjectTables::Load(*Data.Image, *Data.Index);
		if (!Tables)
		{
			return false;
		}
		Data.Tables = *Tables;
		for (int32 Stage = 0; Stage < StageCount; ++Stage)
		{
			auto Files = FBattleStageFiles::Load(*Data.Image, *Data.Index, Stage);
			if (!Files)
			{
				return false;
			}
			Data.Stages.push_back(std::move(*Files));
		}
		for (int32 Scene = 0; Scene < SceneCount; ++Scene)
		{
			auto Graphics = FBattleSceneGraphics::Load(*Data.Image, *Data.Index, Scene);
			if (!Graphics)
			{
				return false;
			}
			const std::vector<FBattleSceneEntry>& Entries = Graphics->Entries();
			if (std::any_of(Entries.begin(), Entries.end(), [](const FBattleSceneEntry& Entry) { return !Entry.IsSprite() && Entry.HasOwnImages(); }))
			{
				Data.ModelScenes.push_back(std::move(*Graphics));
				Data.ModelSceneNumbers.push_back(Scene);
			}
		}
		return true;
	}

	FBattleObjectCase::FBattleObjectCase(const FBattleObjectData& Data) : Services(*Data.Image, *Data.Index, *Data.Tables)
	{
		Objects.Context = Context.get();
		Objects.Camera = &Camera;
		Objects.Floor = &Floor;
		Objects.Tables = &*Data.Tables;
		Objects.Gte = &Gte;
		Objects.Services = &Services;
		Objects.Random = &Random;
		Objects.Arctangents = &Data.Arctangents;
		ShowOptions.bRegurusFightsAsVierge = false;
		Objects.ShowOptions = &ShowOptions;
	}

	std::string RunBattleObjectCase(FXorShift32& G, const FBattleObjectData& Data, FOracleDigest& Digest, const FBattleObjectCaseFrames& Frames,
		const FBattleObjectCaseChoice& Chosen)
	{
		FBattleObjectCase Case(Data);
		const auto Ended = [&]() {
			if (!Case.Objects.Faulted())
			{
				return false;
			}
			Digest.I32(1);
			return true;
		};
		DrawWorld(G, Case);
		MakeStage(G, Case, Data, Chosen.Stage);
		if (Ended())
		{
			return FaultOf(Case.Objects);
		}
		MakeScene(G, Case, Data, Chosen);
		if (Ended())
		{
			return FaultOf(Case.Objects);
		}
		MakeGears(G, Case, Data);
		if (Ended())
		{
			return FaultOf(Case.Objects);
		}
		if (G.Next() % 2 != 0)
		{
			TResult<FBattleModelFiles> Files = Case.Services.ReadSpecialModel(static_cast<int32>(G.Next() % BattleObjectFiles::SpecialModelCount));
			REQUIRE(Files);
			Case.Objects.CreateFromFiles(FBattleObjects::SpecialSlot, std::move(*Files), SpecialPlace);
			if (Ended())
			{
				return FaultOf(Case.Objects);
			}
		}
		const FGteVector Angles = {static_cast<int16>(G.Next() & 0xFFF), static_cast<int16>(G.Next() & 0xFFF), static_cast<int16>(G.Next() & 0xFFF)};
		FGteMatrix View = RotationXyz(Angles);
		View.T[0] = DrawSigned(G, 12);
		View.T[1] = DrawSigned(G, 12);
		View.T[2] = static_cast<int32>(0x1000 + G.Next() % 0x2000);
		const std::optional<int32> Aimed = AimedSlot(Case, Chosen.Entry);
		if (Aimed)
		{
			Aim(View, Case.Context->Slots[static_cast<std::size_t>(*Aimed)]);
			if (Chosen.Sequence)
			{
				Case.Objects.Perform(*Aimed, 0, *Chosen.Sequence);
				if (Ended())
				{
					return FaultOf(Case.Objects);
				}
			}
		}
		if (Frames.DigestStart)
		{
			Frames.DigestStart(Case, Digest);
		}
		const uint32 FrameCount = 1 + G.Next() % Frames.MostFrames;
		for (uint32 Frame = 0; Frame < FrameCount; ++Frame)
		{
			const uint32 Choice = G.Next() % 32;
			std::vector<int32> Present;
			for (int32 Slot = 0; Slot < FBattleObjects::SlotCount; ++Slot)
			{
				if (Case.Objects.Object(Slot) != nullptr)
				{
					Present.push_back(Slot);
				}
			}
			if (Choice < 4)
			{
				const int32 Drawn = Present[G.Next() % Present.size()];
				const int32 Slot = Aimed && Case.Objects.Object(*Aimed) != nullptr ? *Aimed : Drawn;
				const uint16 Targets = static_cast<uint16>(G.Next() & 0x7FF);
				const FBattleObject& Object = *Case.Objects.Object(Slot);
				const int32 Sequences = static_cast<int32>(Object.Motion->ScriptEntries) - 1;
				int32 Sequence = static_cast<int32>(G.Next() % static_cast<uint32>(std::max(1, Sequences)));
				if (Aimed && Slot == *Aimed && Chosen.Sequence)
				{
					Sequence = *Chosen.Sequence;
				}
				if (!Unused(Object, Sequence))
				{
					if (Choice == 2)
					{
						Case.Objects.React(Slot, Targets, Sequence);
					}
					else if (Choice == 3)
					{
						Case.Objects.PerformSignalled(Slot, Targets, Sequence);
					}
					else
					{
						Case.Objects.Perform(Slot, Targets, Sequence);
					}
					if (Ended())
					{
						return FaultOf(Case.Objects);
					}
				}
			}
			else if (Choice == 4)
			{
				Case.Services.bNumbersPending = G.Next() % 2 != 0;
			}
			Frames.Draw(Case, View, static_cast<int32>(Frame & 1));
			if (Ended())
			{
				return FaultOf(Case.Objects);
			}
			Frames.DigestFrame(Case, Digest);
		}
		return {};
	}
}
