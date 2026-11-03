#include "Battle/Show/BattleShowObjects.h"

#include "Battle/BattleContext.h"
#include "Battle/Show/BattleRegurus.h"
#include "Battle/Show/BattleStageBackdrop.h"
#include "Battle/Show/BattleStageFiles.h"
#include "BattleObjectMachine.h"
#include "Foundation/LittleEndian.h"
#include "Game/CharacterLooks.h"
#include "Graphics/SpriteSystem.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace BattleObjectCreateFlags;

		/** The stage's object: made still, from the stage's model whose archive it does not own. */
		constexpr uint32 StageFlags = SharesSequences | Still | BorrowsSequences;
		/** A special model's textures and CLUT row, and the sequence that makes it before it runs. */
		constexpr FGearTexturePlace SpecialPlace{0x300, 0x100, 0, 0x1DB};
		constexpr int32 SpecialMakingSequence = 2;
		/** Where an installed extension's images and CLUTs go. */
		constexpr int16 ExtensionImagesX = 0x380;
		constexpr int16 ExtensionImagesY = 0x100;
		constexpr int16 ExtensionClutX = 0;
		constexpr int16 ExtensionClutY = 0x1D0;

		/**
		 * A gear's model load as a task: a step to read the gear's files once no other gear's are
		 * being read, a step to make its object once they are in (the port's reads end at once).
		 */
		class FBattleGearLoad final : public IKernelTask
		{
		public:
			FBattleGearLoad(FBattleObjects& InObjects, int32 InSlot) : Objects(InObjects), Slot(InSlot)
			{
			}

			void Update() override
			{
				if (!bRead)
				{
					if (Objects.ReadingGearModels == 0)
					{
						Objects.ReadGearModelFiles(Slot);
						bRead = true;
					}
					return;
				}
				Objects.Tasks->UnlinkTask(*this);
				Objects.CreateGearModel(Slot);
			}

			int32 Step() const { return bRead ? 1 : 0; }

		private:
			FBattleObjects& Objects;
			int32 Slot = 0;
			bool bRead = false;
		};
	}

	void FBattleObjects::CreateStage(const FBattleStageFiles& StageFiles)
	{
		StageTintPalette = FBattleClutAnimation{};
		Create(StageSlot, StageFlags, nullptr, std::make_shared<const FGearModelPack>(StageFiles.Model.Pack), {});
		FBattleObject* Made = Object(StageSlot);
		if (Made == nullptr || Faulted())
		{
			return;
		}
		// The backdrop's panoramas and dome draw from the stage's textures: their pictures are the stage's too.
		std::vector<FTexturePictureSeed> Seeds = *Made->TextureSeeds;
		const std::vector<FTexturePictureSeed> Backdrop = BattleBackdropTextureSeeds(MakeBattleStageBackdrop(StageFiles.Record), StageFiles.Model.Pack);
		Seeds.insert(Seeds.end(), Backdrop.begin(), Backdrop.end());
		Made->TextureSeeds = std::make_shared<const std::vector<FTexturePictureSeed>>(std::move(Seeds));
		for (std::size_t Bone = 1; Bone < Made->Bones.size() && Bone - 1 < StageFiles.Model.BonePlaces.size(); ++Bone)
		{
			const FBattleStageBonePlace& Placed = StageFiles.Model.BonePlaces[Bone - 1];
			Made->Bones[Bone].Position = {Placed.Translation[0], Placed.Translation[1], Placed.Translation[2]};
			Made->Bones[Bone].Billboard = Placed.DrawKind;
		}
		// The stage record's archive: its first section the obstacles and scrolls, its sequences after; it has no animations.
		FGearMotionPack Pack;
		const std::span<const uint8> Record = StageFiles.Record.Bytes();
		Pack.Bytes.assign(Record.begin(), Record.end());
		Pack.Scripts = StageFiles.Record.ArchiveOffset();
		if (!Pack.Has(Pack.Scripts, 4))
		{
			Fail("the stage record's sequence archive lies past the record");
			return;
		}
		Pack.ScriptEntries = LittleEndian::ReadU32(Pack.Bytes, Pack.Scripts);
		BattleObjectMachine::InitMotion(*Made, HoldPack(std::move(Pack)));
		StartSequence(StageSlot, StageSlot, 0);
		if (FBattleObject* Started = Object(StageSlot))
		{
			ComputeGearBones(Started->Bones, Started->Scale);
		}
	}

	void FBattleObjects::CreateModel(int32 Slot, uint32 Flags, std::span<const uint8> Motion, std::span<const uint8> Model, int32 TextureX, int32 TextureY,
		int32 ClutRow)
	{
		std::shared_ptr<const FGearModelPack> Made = KeptModel;
		if ((Flags & SharesModel) != 0 && Made == nullptr)
		{
			Fail("slot " + std::to_string(Slot) + " shares a model the last object made let go");
			return;
		}
		if ((Flags & SharesModel) == 0)
		{
			TResult<FGearModelPack> Parsed = ParseGearModelPack(Model);
			if (!Parsed)
			{
				Fail("slot " + std::to_string(Slot) + "'s model: " + Parsed.Error().Message);
				return;
			}
			Made = std::make_shared<const FGearModelPack>(std::move(*Parsed));
		}
		FGearMotionPack* Pack = LastMotion;
		if ((Flags & SharesSequences) == 0 || Pack == nullptr || Motion.data() != LastMotionBytes.data())
		{
			TResult<FGearMotionPack> Parsed = ParseGearMotionPack(std::vector<uint8>(Motion.begin(), Motion.end()));
			if (!Parsed)
			{
				Fail("slot " + std::to_string(Slot) + "'s sequences: " + Parsed.Error().Message);
				return;
			}
			Pack = HoldPack(std::move(*Parsed));
			LastMotion = Pack;
			LastMotionBytes = Motion;
		}
		KeptModel = (Flags & KeepsModel) != 0 ? Made : nullptr;
		Create(Slot, Flags, Pack, Made, {static_cast<int16>(TextureX), static_cast<int16>(TextureY), 0, static_cast<int16>(ClutRow)});
	}

	void FBattleObjects::QueueGearModel(int32 Slot)
	{
		if (Tasks == nullptr)
		{
			Fail("a gear's model is queued without the kernel's tasks to load it");
			return;
		}
		GearLoadTasks.push_back(std::make_unique<FBattleGearLoad>(*this, Slot));
		// The load's task is not one of the tasks the playback counts.
		const bool bCounting = Tasks->bCountingTasks;
		Tasks->bCountingTasks = false;
		Tasks->LinkTask(*GearLoadTasks.back());
		Tasks->bCountingTasks = bCounting;
		++PendingGearModels;
	}

	std::optional<int32> FBattleObjects::GearLoadStep(const IKernelTask& Task) const
	{
		for (const std::unique_ptr<IKernelTask>& Load : GearLoadTasks)
		{
			if (Load.get() == &Task)
			{
				return static_cast<const FBattleGearLoad&>(Task).Step();
			}
		}
		return std::nullopt;
	}

	void FBattleObjects::ReadGearModelFiles(int32 Slot)
	{
		if (Slot < 0 || Slot >= FBattleContext::PartySlotCount)
		{
			Fail("a gear model is read for slot " + std::to_string(Slot) + ", past the party's");
			return;
		}
		const FBattleCombatant& Combatant = Context->Combatants[static_cast<std::size_t>(Slot)];
		const FCharacterLooks Looks = Context->PartyLooks();
		// Regurus fights as Vierge wearing its own model (BattleRegurus); the looks' gears as the player has them drawn.
		const bool bRegurusAsVierge = (ShowOptions == nullptr || ShowOptions->bRegurusFightsAsVierge) && Combatant.Gear == BattleRegurus::Regurus;
		const uint8 PerformingGear = bRegurusAsVierge ? BattleRegurus::Vierge : Looks.PerformingGear(Combatant.Gear);
		TResult<FBattleModelFiles> Files = Services->ReadGear(PerformingGear, bRegurusAsVierge ? 0 : Combatant.GearPart.ModelBytes[0]);
		if (!Files)
		{
			Fail(Files.Error().Message);
			return;
		}
		if (bRegurusAsVierge || Looks.WearsOwnModel(Combatant.Gear))
		{
			// Its own model with no variant: no weapon in its hands.
			TResult<FBattleModelFiles> Own = Services->ReadGear(Combatant.Gear, 0);
			if (!Own)
			{
				Fail(Own.Error().Message);
				return;
			}
			Files->Dress = FBattleGearDressFiles{PerformingGear, Combatant.Gear, std::move(Own->Model), std::move(Own->Motion)};
		}
		else if (const FBattleGearDress::FPairing* Ids = Looks.WearsIdsGear(Combatant.Gear) ? FBattleGearDress::IdsPairing(Combatant.Gear) : nullptr)
		{
			TResult<FBattleModelFiles> Red = Services->ReadSceneModel(Ids->WearerScene, Ids->WearerEntry);
			if (!Red)
			{
				Fail(Red.Error().Message);
				return;
			}
			Files->Dress = FBattleGearDressFiles{PerformingGear, Combatant.Gear, std::move(Red->Model), std::move(Red->Motion), true};
		}
		GearFiles[static_cast<std::size_t>(Slot)] = std::move(*Files);
		++ReadingGearModels;
	}

	void FBattleObjects::CreateGearModel(int32 Slot)
	{
		if (Slot < 0 || Slot >= FBattleContext::PartySlotCount || !GearFiles[static_cast<std::size_t>(Slot)])
		{
			Fail("a gear model is made for slot " + std::to_string(Slot) + " without its files read");
			return;
		}
		int32 Place = 0;
		while (Place < GearPlaces && ((GearPlacesTaken >> Place) & 1) != 0)
		{
			++Place;
		}
		if (Place == GearPlaces)
		{
			Fail("a fourth gear model takes a VRAM place past the three");
			return;
		}
		GearPlacesTaken = static_cast<uint16>(GearPlacesTaken | 1u << Place);
		FGearTexturePlace Where = Tables->GearPlaces[static_cast<std::size_t>(Place)];
		Where.ClutX = 0;
		Where.ClutY = static_cast<int16>(PartyTextureRows + Slot);
		FBattleModelFiles Files = std::move(*GearFiles[static_cast<std::size_t>(Slot)]);
		GearFiles[static_cast<std::size_t>(Slot)].reset();
		CreateFromFiles(Slot, std::move(Files), Where);
		--ReadingGearModels;
		Services->CreateProxy(Slot);
		--PendingGearModels;
		if (PendingGearModels == 0)
		{
			bIdleWaitAsked = true;
		}
	}

	void FBattleObjects::LoadSpecialModel(int32 Model)
	{
		TResult<FBattleModelFiles> Files = Services->ReadSpecialModel(Model);
		if (!Files)
		{
			Fail(Files.Error().Message);
			return;
		}
		SpecialFiles = std::move(*Files);
	}

	void FBattleObjects::RunSpecialModel(int32 Sequence, uint16 Targets)
	{
		if (Sequence == SpecialMakingSequence)
		{
			if (!SpecialFiles)
			{
				Fail("a special model is made without its files read");
				return;
			}
			FBattleModelFiles Files = std::move(*SpecialFiles);
			SpecialFiles.reset();
			CreateFromFiles(SpecialSlot, std::move(Files), SpecialPlace);
		}
		Perform(SpecialSlot, Targets, Sequence);
	}

	void FBattleObjects::FreeSpecialModel()
	{
		Free(SpecialSlot);
	}

	namespace BattleObjectMachine
	{
		namespace
		{
			std::span<const uint8> ExtensionSoundFile(const FBattleSequenceExtension& Extension)
			{
				const std::size_t Start = std::min<std::size_t>(Extension.SoundEffects.Offset, Extension.Pack.Bytes.size());
				const std::size_t End = std::clamp<std::size_t>(Extension.SoundEffects.End, Start, Extension.Pack.Bytes.size());
				return std::span<const uint8>(Extension.Pack.Bytes).subspan(Start, End - Start);
			}
		}

		void LoadExtension(FBattleObjects& Objects, FBattleObject& Object, int32 File)
		{
			TResult<FBattleSequenceExtension> Read = Objects.Services->ReadExtension(File);
			if (!Read)
			{
				Objects.Fail(Read.Error().Message);
				return;
			}
			Object.Extension = Objects.HoldExtension(std::move(*Read));
		}

		void InstallExtension(FBattleObjects& Objects, FBattleObject& Object)
		{
			FBattleSequenceExtension& Extension = *Object.Extension;
			if (!Extension.Images.IsEmpty())
			{
				TResult<std::vector<FGearTextureBlock>> Blocks = ReadGearImageList(Extension.Pack.Bytes, Extension.Images.Offset);
				if (!Blocks)
				{
					Objects.Fail("an extension's images: " + Blocks.Error().Message);
					return;
				}
				for (const FGearTextureBlock& Block : *Blocks)
				{
					const int16 BaseX = Block.bClut ? ExtensionClutX : ExtensionImagesX;
					const int16 BaseY = Block.bClut ? ExtensionClutY : ExtensionImagesY;
					Objects.Services->UploadImage(Block, static_cast<int16>(BaseX + Block.OffsetX), static_cast<int16>(BaseY + Block.OffsetY));
				}
			}
			Object.bExtensionSoundEffectsRegistered = false;
			// An empty sound file's id is read all the same: the bytes where it would start.
			constexpr uint32 FileIdOffset = 0x14;
			Object.ExtensionSoundSet.reset();
			if (Extension.Pack.Has(Extension.SoundEffects.Offset + FileIdOffset, 2))
			{
				Object.ExtensionSoundSet = LittleEndian::ReadU16(Extension.Pack.Bytes, Extension.SoundEffects.Offset + FileIdOffset);
			}
			if (!Extension.SoundEffects.IsEmpty())
			{
				if (!Object.ExtensionSoundSet)
				{
					Objects.Fail("an extension's sound effects lie past it");
					return;
				}
				const uint16 FileId = *Object.ExtensionSoundSet;
				if (!Objects.Services->SoundEffectsRegistered(FileId))
				{
					Objects.Services->RegisterSoundEffects(ExtensionSoundFile(Extension));
					Object.bExtensionSoundEffectsRegistered = true;
				}
			}
			Object.bExtensionInstalled = true;
			if (!Extension.Sprite.IsEmpty())
			{
				Objects.Services->TakeEffectResource(Extension);
			}
		}

		void FreeExtension(FBattleObjects& Objects, FBattleObject& Object)
		{
			if (Object.Extension == nullptr)
			{
				return;
			}
			if (Object.bExtensionSoundEffectsRegistered && Object.ExtensionSoundSet)
			{
				Objects.Services->UnregisterSoundEffects(ExtensionSoundFile(*Object.Extension));
				Object.bExtensionSoundEffectsRegistered = false;
			}
			// The extension's memory is given back, not cleared, and no track is stopped: a bone still streaming one
			// of its animations reads on from the block, which holds its bytes until the heap hands it out again.
			// The objects keep those bytes for whoever still reads them (HoldsPack).
			Objects.ReleasePack(&Object.Extension->Pack);
			Object.Extension = nullptr;
			Object.bExtensionInstalled = false;
		}
	}
}
