#include "Battle/Show/BattleShowObjects.h"

#include "Battle/BattleContext.h"
#include "Battle/Show/BattleStageFiles.h"
#include "BattleObjectMachine.h"
#include "Gears/GearPackets.h"
#include "Gears/GearSkeleton.h"
#include "Gears/GearTextureRegions.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace BattleObjectCreateFlags;

		/** A gear's sub-objects take the slots after the special model's, two a party slot. */
		constexpr int32 SubObjectsPerSlot = 2;
		constexpr uint8 LockedTag = 0xFF;
		/** The objects' parts are built lit, to be relit as they are drawn; a still object's (the stage's) keep their stored colours. */
		constexpr int32 ObjectLightMode = 2;
		constexpr int32 StillLightMode = 0;

		/** An object made afresh, its lists' room kept. */
		void Reset(FBattleObject& Object)
		{
			std::vector<FGearBone> Bones = std::move(Object.Bones);
			std::vector<FBattleTrailEmitter> Trails = std::move(Object.Trails);
			std::vector<FGearCloth> Cloths = std::move(Object.Cloths);
			std::vector<FBattleClutAnimation> Animations = std::move(Object.ClutAnimations);
			Object = FBattleObject{};
			Object.Bones = std::move(Bones);
			Object.Trails = std::move(Trails);
			Object.Cloths = std::move(Cloths);
			Object.ClutAnimations = std::move(Animations);
			Object.Bones.clear();
			Object.Trails.clear();
			Object.Cloths.clear();
			Object.ClutAnimations.clear();
		}

		/** An object's pictures named again as what its making uploads after its own (a variant's images, its parts' textures) leaves them. */
		void KeyAfter(FBattleObject& Object, const FGearTexturePlace& Place, std::span<const FGearTextureBlock> Over, const FGearTexturePlace* OverPlace)
		{
			std::vector<FTextureRegion> Regions;
			for (const FTexturePictureSeed& Seed : *Object.TextureSeeds)
			{
				Regions.push_back(Seed.Region);
			}
			const bool bPlaced = (Object.Flags & BattleObjectFlags::AbsoluteTextures) == 0;
			Object.TextureSeeds = std::make_shared<const std::vector<FTexturePictureSeed>>(
				GearTextureSeeds(*Object.Model, bPlaced ? &Place : nullptr, Regions, Over, OverPlace));
		}
	}

	void FBattleObjects::Create(int32 Slot, uint32 Flags, FGearMotionPack* Motion, std::shared_ptr<const FGearModelPack> Model, const FGearTexturePlace& Place)
	{
		if (Faulted() || Slot > StageSlot)
		{
			return;
		}
		if (Slot < 0)
		{
			Fail("an object is made in slot " + std::to_string(Slot));
			return;
		}
		FSlot& Entry = Slots[static_cast<std::size_t>(Slot)];
		if (Entry.bPresent)
		{
			return;
		}
		FBattleObject& Object = Entry.Object;
		Reset(Object);
		if ((Flags & SharesSequences) == 0 && Motion != nullptr && Motion->bHasSoundEffects)
		{
			const std::optional<uint16> FileId = BattleObjectMachine::SoundFileId(*Motion);
			if (!FileId)
			{
				Fail("object " + std::to_string(Slot) + "'s sound effects lie past its archive");
				return;
			}
			if (!Services->SoundEffectsRegistered(*FileId))
			{
				Services->RegisterSoundEffects(BattleObjectMachine::SoundFile(*Motion));
				Object.bSoundEffectsRegistered = true;
			}
		}
		const FGearModelHeader& Header = Model->Header;
		Entry.bPresent = true;
		Object.Height = Header.Height;
		Object.RadiusX = Header.ShadowHalfWidth;
		Object.RadiusZ = Header.ShadowHalfDepth;
		Object.Stance = Header.DefaultRecord;
		Object.Flags = Header.Flags;
		const bool bPlaced = (Flags & Still) == 0 && (Object.Flags & BattleObjectFlags::AbsoluteTextures) == 0;
		if ((Flags & SharesModel) == 0)
		{
			for (const FGearTextureBlock& Block : Model->Textures)
			{
				const std::array<int16, 2> Corner = GearTextureBlockCorner(Block, bPlaced ? &Place : nullptr);
				Services->UploadImage(Block, Corner[0], Corner[1]);
			}
		}
		MakeGearSkeleton(Model->Skeleton, Object.Bones);
		// The Gouraud quads keep their build's light, which the drawing gives them afresh: no lighting of the build's matters.
		TResult<std::vector<FGearBonePackets>> Packets = BuildGearPackets(Model->Model, Object.Bones, (Flags & Still) != 0 ? StillLightMode : ObjectLightMode,
			bPlaced ? &Place : nullptr, FGte{});
		if (!Packets)
		{
			Fail("object " + std::to_string(Slot) + "'s parts: " + Packets.Error().Message);
			return;
		}
		Object.Packets = std::make_shared<const std::vector<FGearBonePackets>>(std::move(*Packets));
		if ((Flags & SharesModel) == 0)
		{
			Object.TextureX = Place.X;
			Object.TextureY = Place.Y;
			Object.ClutX = Place.ClutX;
			Object.ClutY = Place.ClutY;
		}
		Object.bOwnsMotion = (Flags & (SharesSequences | BorrowsSequences)) == 0 && Motion != nullptr;
		Object.Scale = (Flags & Still) == 0 ? static_cast<int16>((Header.Scale * Stage->ObjectScale()) >> 12) : Header.Scale;
		Object.Trails.assign(Header.Trails, FBattleTrailEmitter{});
		Object.ClutAnimations.assign(Header.TextureAnimations, FBattleClutAnimation{});
		std::optional<std::vector<FGearCloth>> Cloths = MakeGearCloths(*Model, Place);
		if (!Cloths)
		{
			Fail("object " + std::to_string(Slot) + " has a cloth whose making divides by zero or leaves triangles unmade");
			return;
		}
		Object.Cloths = std::move(*Cloths);
		const std::vector<FTextureRegion> Regions = GearTextureRegions(Model->Model, Object.Cloths, bPlaced ? &Place : nullptr);
		Object.TextureSeeds = std::make_shared<const std::vector<FTexturePictureSeed>>(GearTextureSeeds(*Model, bPlaced ? &Place : nullptr, Regions));
		Object.bFromFiles = false;
		Object.Slot = static_cast<uint8>(Slot);
		Object.bVisible = Slot >= CombatantSlots || Context->Slots[static_cast<std::size_t>(Slot)].Presence == 0 ? 1 : 0;
		Object.Model = std::move(Model);
		if ((Flags & Still) == 0)
		{
			Object.bHasSoundTable = true;
			BattleObjectMachine::InitMotion(Object, Motion);
			StartSequence(Slot, Slot, 0);
			if (FBattleObject* Made = this->Object(Slot))
			{
				BattleObjectMachine::SnapToFloor(*this, *Made);
			}
		}
	}

	std::shared_ptr<const FBattleGearDress> FBattleObjects::MakeDress(FBattleModelFiles& Files, FGearMotionPack& Motion, const FGearTexturePlace& Place)
	{
		if (!Files.Dress)
		{
			return nullptr;
		}
		// Without a pairing for the two, or with models other than the ones it was drawn up for, the performer is drawn as it is.
		const FBattleGearDress::FPairing* Pairing = Files.Dress->bIdsGear ? FBattleGearDress::IdsPairing(Files.Dress->PerformerGear)
																		  : FBattleGearDress::FindPairing(Files.Dress->PerformerGear, Files.Dress->Gear);
		if (Pairing == nullptr)
		{
			return nullptr;
		}
		TResult<FBattleGearDress> Made = FBattleGearDress::Make(*Pairing, Files.Model, Motion, std::move(Files.Dress->Model), Files.Dress->Motion, &Place);
		return Made ? std::make_shared<const FBattleGearDress>(std::move(*Made)) : nullptr;
	}

	void FBattleObjects::CreateFromFiles(int32 Slot, FBattleModelFiles&& Files, const FGearTexturePlace& Place)
	{
		FGearMotionPack* Motion = HoldPack(std::move(Files.Motion));
		std::shared_ptr<const FBattleGearDress> Dress = MakeDress(Files, *Motion, Place);
		if (Dress != nullptr)
		{
			// The performer's own pieces are never drawn at its place, so its pictures never go up there; the wearer's go up
			// in their place before the object is made, as its own would, for a palette animation its first sequence starts
			// to read.
			Files.Model.Textures.clear();
			const bool bPlaced = (Dress->Model().Header.Flags & GearModelFlags::AbsoluteTextures) == 0;
			for (const FGearTextureBlock& Block : Dress->Model().Textures)
			{
				const std::array<int16, 2> Corner = GearTextureBlockCorner(Block, bPlaced ? &Place : nullptr);
				Services->UploadImage(Block, Corner[0], Corner[1]);
			}
			// The performer's own, for the pieces of it the dress keeps, where the wearer's leave them whole.
			for (const FGearTextureBlock& Block : Dress->PerformerTextures())
			{
				const std::array<int16, 2> Corner = GearTextureBlockCorner(Block, &Dress->PerformerPlace());
				Services->UploadToPortRows(Block, Corner[0], Corner[1]);
			}
		}
		Create(Slot, 0, Motion, std::make_shared<const FGearModelPack>(std::move(Files.Model)), Place);
		FBattleObject* Made = Object(Slot);
		if (Made == nullptr)
		{
			Fail("a model's files are made into the missing slot " + std::to_string(Slot));
			return;
		}
		Made->Playing = 1;
		Made->bFromFiles = true;
		if (Dress != nullptr)
		{
			Made->TextureSeeds = Dress->TextureSeeds();
			Made->Dress = std::move(Dress);
		}
		if (!Files.Variant)
		{
			return;
		}
		FBattleGearVariant& Variant = *Files.Variant;
		if (Variant.Parts.empty())
		{
			for (const FGearTextureBlock& Block : Variant.Images)
			{
				const int16 BaseX = Block.bClut ? Place.ClutX : Place.X;
				const int16 BaseY = Block.bClut ? Place.ClutY : Place.Y;
				Services->UploadImage(Block, static_cast<int16>(BaseX + Block.OffsetX), static_cast<int16>(BaseY + Block.OffsetY));
			}
			KeyAfter(*Made, Place, Variant.Images, &Place);
			return;
		}
		FGearMotionPack* PartsMotion = HoldPack(std::move(*Variant.Motion));
		const std::shared_ptr<const FGearModelPack> PartsModel = std::make_shared<const FGearModelPack>(std::move(*Variant.Model));
		const int32 Count = static_cast<int32>(Variant.Parts.size());
		for (int32 Part = 0; Part < Count; ++Part)
		{
			// The first part uploads the parts' textures and keeps their model; the last lets it go.
			uint32 Flags = Part == 0 ? KeepsModel : (SharesModel | KeepsModel | SharesSequences);
			if (Part == Count - 1)
			{
				Flags &= ~KeepsModel;
			}
			const int32 PartSlot = Slot * SubObjectsPerSlot + Part + FirstSubObject;
			if (PartSlot >= SlotCount)
			{
				Fail("a sub-object of slot " + std::to_string(Slot) + " lies past the objects' slots");
				return;
			}
			Create(PartSlot, Flags, PartsMotion, PartsModel, Place);
			FBattleObject* Sub = Object(PartSlot);
			if (Sub == nullptr)
			{
				Fail("sub-object slot " + std::to_string(PartSlot) + " was not made, where the original writes into a missing object");
				return;
			}
			Sub->HostBone = Variant.Parts[static_cast<std::size_t>(Part)].Bone;
			Sub->Host = static_cast<uint8>(Slot);
			Sub->HostTurns = 2;
			Sub->NoFloorSnap = 1;
			const FGteVector& Offset = Variant.Parts[static_cast<std::size_t>(Part)].Offset;
			Sub->AttachOffset = {Offset[0], Offset[1], Offset[2]};
		}
		if (FBattleObject* Host = Object(Slot))
		{
			const bool bPartsPlaced = (PartsModel->Header.Flags & BattleObjectFlags::AbsoluteTextures) == 0;
			KeyAfter(*Host, Place, PartsModel->Textures, bPartsPlaced ? &Place : nullptr);
		}
	}

	void FBattleObjects::Free(int32 Slot)
	{
		if (Slot < 0 || Slot >= SlotCount)
		{
			Fail("the objects free slot " + std::to_string(Slot) + ", past their slots");
			return;
		}
		if (FBattleObject* Freed = Object(Slot))
		{
			if (Freed->bSoundEffectsRegistered && Freed->Motion != nullptr && BattleObjectMachine::SoundFileId(*Freed->Motion))
			{
				Services->UnregisterSoundEffects(BattleObjectMachine::SoundFile(*Freed->Motion));
			}
			if (Freed->bOwnsMotion)
			{
				ReleasePack(Freed->Motion);
			}
			GearMachine::FreeUnlockedChannels(*this, *Freed);
			GearMachine::FreeTaggedChannels(*this, *Freed, LockedTag);
			if (Slot < FirstClone || Slot > StageSlot - 1)
			{
				BattleObjectMachine::FreeExtension(*this, *Freed);
			}
			for (FBattleClutAnimation& Animation : Freed->ClutAnimations)
			{
				BattleObjectMachine::StopClutAnimation(*this, Animation);
			}
			Slots[static_cast<std::size_t>(Slot)].bPresent = false;
		}
		if (Slot < FBattleContext::PartySlotCount)
		{
			Free(Slot * SubObjectsPerSlot + FirstSubObject);
			Free(Slot * SubObjectsPerSlot + FirstSubObject + 1);
		}
	}

	void FBattleObjects::FreeAll()
	{
		for (int32 Slot = 0; Slot < StageSlot; ++Slot)
		{
			Free(Slot);
		}
		Channels.Release();
		TrailSegments.clear();
		TrailLowest = 0;
	}

	namespace BattleObjectMachine
	{
		void InitMotion(FBattleObject& Object, FGearMotionPack* Motion)
		{
			Object.WatchTag = 0xFFFF;
			Object.Host = FBattleObject::NoHost;
			Object.Loudness = 0x6B;
			Object.Motion = Motion;
			Object.Extension = nullptr;
			Object.bExtensionInstalled = false;
			Object.Pc = {};
			Object.Queue = 0;
			Object.KeyFrame = -1;
			Object.Who = 0;
			Object.Signal = 0;
			Object.ScaledBones = 0;
			Object.Playing = 0;
			Object.Mark = -1;
			Object.Spin = {};
			Object.SpinAcceleration = {};
			Object.Velocity = {};
			Object.Acceleration = {};
			Object.Target = {};
			Object.Speed = 1;
			Object.NoFloorSnap = 0;
			Object.FloorTriangle = -1;
		}

		std::optional<uint16> SoundFileId(const FGearMotionPack& Pack)
		{
			constexpr std::size_t FileIdOffset = 0x14;
			if (!Pack.Has(Pack.SoundEffects + static_cast<uint32>(FileIdOffset), 2))
			{
				return std::nullopt;
			}
			return static_cast<uint16>(Pack.Bytes[Pack.SoundEffects + FileIdOffset] | Pack.Bytes[Pack.SoundEffects + FileIdOffset + 1] << 8);
		}

		std::span<const uint8> SoundFile(const FGearMotionPack& Pack)
		{
			const std::size_t Start = std::min<std::size_t>(Pack.SoundEffects, Pack.Bytes.size());
			const std::size_t End = std::clamp<std::size_t>(Pack.SoundEffectsEnd, Start, Pack.Bytes.size());
			return std::span<const uint8>(Pack.Bytes).subspan(Start, End - Start);
		}

		void Clone(FBattleObjects& Objects, int32 Slot, int32 HiddenBone, uint32 Sequence)
		{
			constexpr uint8 NoSequence = 0xFF;
			int32 CloneSlot = FBattleObjects::FirstClone;
			while (CloneSlot < FBattleObjects::StageSlot && Objects.Object(CloneSlot) != nullptr)
			{
				++CloneSlot;
			}
			if (CloneSlot >= FBattleObjects::StageSlot)
			{
				Objects.Fail("a sequence clones an object with every clone slot taken, where the original writes through a stale pointer");
				return;
			}
			const FBattleObject& Original = *Objects.Object(Slot);
			for (const FGearBone& Bone : Original.Bones)
			{
				if (Bone.Scaling != FGearBone::NoChannel)
				{
					Objects.Fail("a sequence clones an object whose bones have scale tracks, which the clone would share with it");
					return;
				}
			}
			FBattleObjects::FSlot& Entry = Objects.Slots[static_cast<std::size_t>(CloneSlot)];
			FBattleObject& Made = Entry.Object;
			std::vector<FGearBone> Bones = std::move(Made.Bones);
			std::vector<FBattleTrailEmitter> Trails = std::move(Made.Trails);
			Made = Original;
			Made.Bones = std::move(Bones);
			Made.Trails = std::move(Trails);
			Entry.bPresent = true;
			Made.WatchTag = 0xFFFF;
			Made.KeyFrame = -1;
			Made.WaitTicks = 0;
			Made.Who = 0;
			Made.Host = FBattleObject::NoHost;
			Made.TextureX = -1;
			Made.bSoundEffectsRegistered = false;
			Made.bOwnsMotion = false;
			Made.Loudness = 0x6B;
			Made.Pc = {};
			Made.Owner = Original.Slot;
			Made.Cloths.clear();
			Made.ClutAnimations.clear();
			Made.Slot = static_cast<uint8>(CloneSlot);
			Made.Trails.assign(Original.Trails.size(), FBattleTrailEmitter{});
			Made.Bones.assign(Original.Bones.begin(), Original.Bones.end());
			for (FGearBone& Bone : Made.Bones)
			{
				Bone.bShown = 0;
				Bone.Rotation = FGearBone::NoChannel;
				Bone.Translation = FGearBone::NoChannel;
			}
			// The hidden bone and its subtree go over to the clone, their tracks stopped on the original.
			FBattleObject& Owner = *Objects.Object(Slot);
			if (HiddenBone < 0 || static_cast<std::size_t>(HiddenBone) >= Owner.Bones.size())
			{
				Objects.Fail("a sequence clones bone " + std::to_string(HiddenBone) + ", which its object does not have");
				return;
			}
			std::vector<int32> Pending = {HiddenBone};
			while (!Pending.empty())
			{
				const int32 Bone = Pending.back();
				Pending.pop_back();
				FGearBone& Given = Owner.Bones[static_cast<std::size_t>(Bone)];
				Given.bShown = 0;
				Made.Bones[static_cast<std::size_t>(Bone)].bShown = 1;
				GearMachine::FreeChannel(Objects, Given.Rotation);
				GearMachine::FreeChannel(Objects, Given.Translation);
				GearMachine::FreeChannel(Objects, Given.Scaling);
				for (std::size_t Child = Owner.Bones.size(); Child-- > 1;)
				{
					if (Owner.Bones[Child].Parent == Bone)
					{
						Pending.push_back(static_cast<int32>(Child));
					}
				}
			}
			if (Sequence != NoSequence)
			{
				Objects.StartSequence(CloneSlot, CloneSlot, static_cast<int32>(Sequence));
			}
		}
	}
}
