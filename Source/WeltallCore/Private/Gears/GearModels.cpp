#include "Gears/GearModels.h"

#include "GearMachine.h"
#include "Gears/GearTextureRegions.h"
#include "Graphics/GteVectors.h"

#include <string>

namespace Weltall
{
	namespace
	{
		constexpr uint32 MaxBufferedVblanks = 6;
		constexpr uint16 WindPhaseStep = 56;
		/** The gears' loads build their parts lit, to be relit as they are drawn. */
		constexpr int32 GearLightMode = 2;

		bool HasEnvironmentMapping(const FMdlModel& Model)
		{
			for (const FMdlPart& Part : Model.Parts)
			{
				for (const FMdlPrimitive& Primitive : Part.Primitives)
				{
					if (Primitive.Type == EMdlPrimitiveType::EnvironmentMappedTriangle)
					{
						return true;
					}
				}
			}
			return false;
		}

		/** The wind's phase moved on by Ticks, and its strength from it. */
		void AdvanceWind(FGearModels& Models, int32 Ticks)
		{
			Models.WindPhase = static_cast<uint16>(Models.WindPhase + WindPhaseStep * Ticks);
			Models.Wind = static_cast<int16>((GteCos(static_cast<int16>(Models.WindPhase)) + 0x1000) / 800 + 4);
		}

		/**
		 * A model made in an empty slot as the load makes it, short of its first script: its textures placed, its
		 * skeleton, its cloths, its parts built. False where the module stops instead.
		 */
		bool MakeModel(FGearModels& Models, int32 Slot, FGearModelPack&& ModelPack, FGearMotionPack&& MotionPack, const FGearTexturePlace& Place,
			const FGteVector& Position)
		{
			const std::string Named = "gear model in slot " + std::to_string(Slot);
			if (Models.PartTablesUsed >= FGearModels::PartTables)
			{
				Models.Fail("a ninth model's parts, where the original has eight tables");
				return false;
			}
			++Models.PartTablesUsed;
			auto Model = std::make_unique<FGearModel>();
			const FGearModelHeader& Header = ModelPack.Header;
			Model->Motion = std::make_unique<FGearMotionPack>(std::move(MotionPack));
			Model->Motion->Slot = Slot;
			Model->ShadowHalfWidth = Header.ShadowHalfWidth;
			Model->ShadowHalfDepth = Header.ShadowHalfDepth;
			Model->DefaultRecord = Header.DefaultRecord;
			Model->Flags = Header.Flags;
			const FGearTexturePlace* Placed = (Header.Flags & GearModelFlags::AbsoluteTextures) == 0 ? &Place : nullptr;
			for (const FGearTextureBlock& Block : ModelPack.Textures)
			{
				const std::array<int16, 2> Corner = GearTextureBlockCorner(Block, Placed);
				Models.PendingUploads.push_back({Corner[0], Corner[1], Block.Width, Block.Height, Block.Pixels});
			}
			MakeGearSkeleton(ModelPack.Skeleton, Model->Bones);
			Model->Bones[0].Position = {Position[0], Position[1], Position[2]};
			Model->TextureX = Place.X;
			Model->TextureY = Place.Y;
			Model->ClutX = Place.ClutX;
			Model->ClutY = Place.ClutY;
			Model->Scale = Header.Scale;
			Model->Trails = Header.Trails;
			Model->TextureAnimations = Header.TextureAnimations;
			for (std::size_t Index = 0; Index < ModelPack.Cloths.size(); ++Index)
			{
				const FGearClothSpec& Spec = ModelPack.Cloths[Index];
				std::optional<FGearCloth> Cloth = MakeGearCloth(Spec, ModelPack.ClothMeshes[Index], static_cast<int16>(Place.X + Spec.TextureX),
					static_cast<int16>(Place.Y + Spec.TextureY), static_cast<int16>(Place.ClutX + Spec.ClutX), Place.ClutY);
				if (!Cloth)
				{
					--Models.PartTablesUsed;
					Models.Fail(Named + " has a cloth whose making divides by zero or leaves triangles unmade");
					return false;
				}
				Model->Cloths.push_back(std::move(*Cloth));
			}
			const std::vector<FTextureRegion> Regions = GearTextureRegions(ModelPack.Model, Model->Cloths, Placed);
			Model->TextureSeeds = std::make_shared<const std::vector<FTexturePictureSeed>>(GearTextureSeeds(ModelPack, Placed, Regions));
			if (HasEnvironmentMapping(ModelPack.Model))
			{
				--Models.PartTablesUsed;
				Models.Fail(Named + " has environment-mapped polygons, whose texels the field's gear drawing does not make: no gear the field loads has them");
				return false;
			}
			TResult<std::vector<FGearBonePackets>> Packets = BuildGearPackets(ModelPack.Model, Model->Bones, GearLightMode, Placed,
				Models.BuildLighting);
			if (!Packets)
			{
				--Models.PartTablesUsed;
				Models.Fail(Named + ": " + Packets.Error().Message);
				return false;
			}
			Model->BonePackets = std::move(*Packets);
			Model->Parts = std::make_shared<const FMdlModel>(std::move(ModelPack.Model));
			Model->Slot = static_cast<uint8>(Slot);
			FGearFrameModel& Drawn = Models.DrawnFrame.Models[static_cast<std::size_t>(Slot)];
			Drawn.Parts.clear();
			for (const FGearBonePackets& Built : Model->BonePackets)
			{
				Drawn.Parts.push_back({Built.Bone, Built.Part, false, {}, 0});
			}
			Models.Models[static_cast<std::size_t>(Slot)] = std::move(Model);
			++Models.Revision;
			return true;
		}
	}

	bool FGearModels::Wear(int32 Slot, std::shared_ptr<const IGearDress> Dress)
	{
		FGearModel* Worn = Model(Slot);
		if (Worn == nullptr || Dress == nullptr)
		{
			return false;
		}
		const FGearModelPack& Pack = Dress->Model();
		const FGearTexturePlace Place{Worn->TextureX, Worn->TextureY, Worn->ClutX, Worn->ClutY};
		const FGearTexturePlace* Placed = (Pack.Header.Flags & GearModelFlags::AbsoluteTextures) == 0 ? &Place : nullptr;
		for (const FGearTextureBlock& Block : Pack.Textures)
		{
			const std::array<int16, 2> Corner = GearTextureBlockCorner(Block, Placed);
			PendingUploads.push_back({Corner[0], Corner[1], Block.Width, Block.Height, Block.Pixels});
		}
		Worn->TextureSeeds = Dress->TextureSeeds();
		FGearFrameModel& Drawn = DrawnFrame.Models[static_cast<std::size_t>(Slot)];
		Drawn.Parts.clear();
		for (const FGearBonePackets& Built : Dress->Packets())
		{
			Drawn.Parts.push_back({Built.Bone, Built.Part, false, {}, 0});
		}
		Worn->Dress = std::move(Dress);
		++Revision;
		return true;
	}

	FGearModels::FGearModels() : FGearEngine("the gears' models")
	{
	}

	FGearModels::~FGearModels() = default;

	bool FGearModels::HoldsPack(const FGearMotionPack* Pack) const
	{
		for (const std::unique_ptr<FGearModel>& Model : Models)
		{
			if (Model && Model->Motion.get() == Pack)
			{
				return true;
			}
		}
		return false;
	}

	std::optional<FError> FGearModels::Init(int32 PoolSize, const FGearModelOptions& InOptions)
	{
		VblankCounter = 0;
		WindPhase = 0;
		Options = InOptions;
		for (std::unique_ptr<FGearModel>& Model : Models)
		{
			Model.reset();
		}
		PartTablesUsed = 0;
		if (PoolSize <= 0)
		{
			// The original keeps the pool it had, whose channels a module released has given back.
			Fail("a pool of " + std::to_string(PoolSize) + " channels leaves the module no pool");
			return Fault;
		}
		Channels.Init(PoolSize, Options.bChannelPoolGrows);
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			GearMachine::ClearFrame(*this, Slot);
		}
		return Fault;
	}

	std::optional<FError> FGearModels::Load(int32 Slot, FGearModelPack&& ModelPack, FGearMotionPack&& MotionPack, const FGearTexturePlace& Place, const FGteVector& Position)
	{
		if (Fault || Slot < 0 || Slot >= Slots || Models[static_cast<std::size_t>(Slot)])
		{
			return Fault;
		}
		if (MotionPack.bHasSoundEffects)
		{
			Fail("gear model in slot " + std::to_string(Slot) + " brings sound effects, which the port's gears do not play");
			return Fault;
		}
		if (!MakeModel(*this, Slot, std::move(ModelPack), std::move(MotionPack), Place, Position))
		{
			return Fault;
		}
		GearMachine::StartScript(*this, Slot, Slot, 0);
		return Fault;
	}

	std::optional<FError> FGearModels::LoadStill(int32 Slot, FGearModelPack&& ModelPack, FGearMotionPack&& MotionPack, const FGearTexturePlace& Place)
	{
		if (Fault || Slot < 0 || Slot >= Slots || Models[static_cast<std::size_t>(Slot)])
		{
			return Fault;
		}
		MakeModel(*this, Slot, std::move(ModelPack), std::move(MotionPack), Place, {});
		return Fault;
	}

	std::optional<FError> FGearModels::DrawPosed()
	{
		if (Fault)
		{
			return Fault;
		}
		AdvanceWind(*this, 1);
		for (int32 Slot = 0; Slot < Slots && !Fault; ++Slot)
		{
			FGearModel* Model = this->Model(Slot);
			if (Model == nullptr)
			{
				continue;
			}
			if (Model->bShown != 0)
			{
				ComputeGearBones(Model->Bones, Model->Scale);
			}
			GearMachine::Draw(*this, Slot);
		}
		return Fault;
	}

	void FGearModels::Free(int32 Slot)
	{
		FGearModel* Model = this->Model(Slot);
		if (Model == nullptr)
		{
			return;
		}
		GearMachine::FreeUnlockedChannels(*this, *Model);
		GearMachine::FreeTaggedChannels(*this, *Model, GearChannelTag::Locked);
		--PartTablesUsed;
		Models[static_cast<std::size_t>(Slot)].reset();
		GearMachine::ClearFrame(*this, Slot);
		++Revision;
	}

	void FGearModels::FreeAll()
	{
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			Free(Slot);
		}
		Channels.Release();
	}

	std::optional<FError> FGearModels::PlayAnimation(int32 Slot, uint16 Mask, int32 Animation)
	{
		if (Fault)
		{
			return Fault;
		}
		PlaySlot = static_cast<uint16>(Slot);
		PlayMask = Mask;
		const int32 Wanted = Slot & 0xFFFF;
		if (Wanted >= Slots)
		{
			Fail("an animation is played on slot " + std::to_string(Wanted) + ", past the models' slots");
			return Fault;
		}
		if (Models[static_cast<std::size_t>(Wanted)])
		{
			GearMachine::StartScript(*this, Wanted, Wanted, Animation);
		}
		return Fault;
	}

	std::optional<FError> FGearModels::PlayAnimationFrom(int32 Owner, int32 Slot, uint16 Mask, int32 Animation)
	{
		if (Fault)
		{
			return Fault;
		}
		PlaySlot = static_cast<uint16>(Slot);
		PlayMask = Mask;
		const int32 Wanted = Slot & 0xFFFF;
		if (Wanted >= Slots)
		{
			Fail("an animation is played on slot " + std::to_string(Wanted) + ", past the models' slots");
			return Fault;
		}
		if (Models[static_cast<std::size_t>(Wanted)] && static_cast<uint32>(Wanted) != GearMachine::FirstMaskSlot(*this))
		{
			GearMachine::StartScript(*this, Wanted, Owner, Animation);
		}
		return Fault;
	}

	std::optional<FError> FGearModels::Tick(uint32 ExtraVblanks)
	{
		if (Fault)
		{
			return Fault;
		}
		// The counter is the original's signed word: a huge count can make it negative, and then no tick runs.
		VblankCounter += 1 + ExtraVblanks;
		if (static_cast<int32>(VblankCounter) > static_cast<int32>(MaxBufferedVblanks))
		{
			VblankCounter = MaxBufferedVblanks;
		}
		int32 Ticks = 0;
		while (static_cast<int32>(VblankCounter) > 1)
		{
			VblankCounter -= 2;
			++Ticks;
		}
		AdvanceWind(*this, Ticks);
		for (int32 Slot = 0; Slot < Slots && !Fault; ++Slot)
		{
			if (FGearModel* Model = this->Model(Slot))
			{
				Model->RootBefore = Model->Bones[0].Position;
				GearMachine::Update(*this, Slot, Ticks);
			}
		}
		for (int32 Slot = 0; Slot < Slots && !Fault; ++Slot)
		{
			FGearModel* Model = this->Model(Slot);
			if (Model != nullptr && Model->HostSlot != FGearModel::NoHost)
			{
				GearMachine::FollowAttachment(*this, *Model);
			}
		}
		for (int32 Slot = 0; Slot < Slots && !Fault; ++Slot)
		{
			if (FGearModel* Model = this->Model(Slot))
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Model->RootMotion[Axis] = GearMachine::WrapSub(Model->RootBefore[Axis], Model->Bones[0].Position[Axis]);
				}
				GearMachine::Draw(*this, Slot);
			}
		}
		return Fault;
	}

	std::optional<FGteMatrix> FGearModels::BoneMatrix(int32 Slot, int32 Bone)
	{
		const FGearModel* Found = Model(Slot);
		if (Found == nullptr)
		{
			return std::nullopt;
		}
		// A bone the skeleton does not have: the original reads past the bones, whatever the heap holds
		// there; the port writes nothing, as for an empty slot.
		if (Bone < 0 || static_cast<std::size_t>(Bone) >= Found->Bones.size())
		{
			return std::nullopt;
		}

		const FGearBone& Root = Found->Bones[0];
		return Bone == 0 ? Root.Local : CompMatrix(Root.Local, Found->Bones[static_cast<std::size_t>(Bone)].World);
	}

	void FGearModels::TakeVramChanges()
	{
		PendingUploads.clear();
		PendingMoves.clear();
	}
}
