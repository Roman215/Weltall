#include "GearMachine.h"

#include "Graphics/GteVectors.h"

#include <algorithm>
#include <string>

namespace Weltall::GearMachine
{
	namespace
	{
		/** The wind blows a quarter turn from the root's yaw. */
		constexpr int32 WindQuarterTurn = 0x400;

		/** A product over 4096 rounded toward zero, as the compiler divides a signed value. */
		int16 Over4096(int32 Value)
		{
			return static_cast<int16>((Value < 0 ? Value + 0xFFF : Value) >> 12);
		}

		FGteVector Short(const FGteLongVector& Vector)
		{
			return {static_cast<int16>(Vector[0]), static_cast<int16>(Vector[1]), static_cast<int16>(Vector[2])};
		}

		/**
		 * The floor shadow as the drawing lays it: under bone 1, turned with its forward axis, on the
		 * floor, shrinking as the root rises above it.
		 */
		void LayShadow(FGearModels& Models, const FGearModel& Model, FGearFrameShadow& Shadow)
		{
			Shadow.bShown = (Model.Flags & GearModelFlags::NoShadow) == 0 && Model.Bones.size() > 1;
			if (!Shadow.bShown)
			{
				return;
			}
			const FGteMatrix Placed = CompMatrix(Model.Bones[0].Local, Model.Bones[1].World);
			const FGteLongVector Ahead = RotTrans(Placed, {0, 0, 0x1000});
			const FGteLongVector Origin = RotTrans(Placed, {0, 0, 0});
			Shadow.Yaw = static_cast<int16>(-static_cast<int16>(Ratan2(Models, Ahead[2] - Origin[2], Ahead[0] - Origin[0])));
			Shadow.Position = {Origin[0], Model.FloorY, Origin[2]};
			const int32 Rise = Model.FloorY - Model.Bones[0].Position[1];
			Shadow.Scale = static_cast<int16>(std::max(0, Model.Scale - ((Rise < 0 ? Rise + 3 : Rise) >> 2)));
			Shadow.HalfWidth = Model.ShadowHalfWidth;
			Shadow.HalfDepth = Model.ShadowHalfDepth;
		}

		/** A cloth's anchors and spheres placed on their bones for this tick, then its step. */
		void StepCloth(FGearModels& Models, FGearModel& Model, FGearCloth& Cloth)
		{
			if (Cloth.Bone < 0)
			{
				return;
			}
			const FGearBone& Root = Model.Bones[0];
			const int32 Yaw = Root.Angles[1] + WindQuarterTurn;
			const std::array<int16, 3> Wind = {Over4096(WrapMul(-Models.Wind, GteCos(Yaw))), Model.WindY, Over4096(WrapMul(Models.Wind, GteSin(Yaw)))};
			const FGearBone* Bone = BoneAt(Models, Model, Cloth.Bone);
			if (Bone == nullptr)
			{
				return;
			}
			const FGteMatrix OnBone = CompMatrix(Root.Local, Bone->World);
			for (std::size_t Column = 0; Column < Cloth.Anchors.size(); ++Column)
			{
				Cloth.Points[Cloth.ColumnStarts[Column]].Position = Short(RotTrans(OnBone, Cloth.Anchors[Column]));
			}
			for (FGearClothSphereState& Sphere : Cloth.Spheres)
			{
				const FGearBone* Holder = BoneAt(Models, Model, Sphere.Bone);
				if (Holder == nullptr)
				{
					return;
				}
				Sphere.World = Short(RotTrans(CompMatrix(Root.Local, Holder->World), Sphere.Local));
			}
			if (!StepGearCloth(Cloth, Wind, static_cast<uint16>(Model.Scale), Model.FloorY))
			{
				Models.Fail("a gear's cloth has a point in no triangle, whose normal divides by zero");
			}
		}
	}

	void Update(FGearModels& Models, int32 Slot, int32 Ticks)
	{
		FGearModel* Model = Models.Model(Slot);
		uint32 Events = 0;
		if (Model->bShown != 0)
		{
			if (Model->FreeHeight == 0)
			{
				Model->Bones[0].Position[1] = Model->FloorY;
			}
			if (Model->ScaledBones == 0)
			{
				ComputeGearBones(Model->Bones, Model->Scale);
			}
			else if (!ComputeGearBonesScaled(Model->Bones, Model->Scale))
			{
				Models.Fail("a gear's scaled bones divide by a parent's scale of 0");
				return;
			}
			for (int32 Tick = 0; Tick < Ticks && !Models.Faulted(); ++Tick)
			{
				Events |= StepChannels(Models, *Model);
				StepTimeline(Models, *Model);
			}
		}
		if (!Models.Faulted())
		{
			RunScript(Models, Slot, static_cast<int32>(Events), Ticks);
		}
	}

	void Draw(FGearModels& Models, int32 Slot)
	{
		FGearModel& Model = *Models.Model(Slot);
		FGearFrameModel& Drawn = Models.DrawnFrame.Models[static_cast<std::size_t>(Slot)];
		Drawn.bPresent = true;
		Drawn.bShown = Model.bShown != 0;
		Drawn.Cloths = &Model.Cloths;
		Drawn.Model = Model.Dress != nullptr ? &Model.Dress->Model().Model : Model.Parts.get();
		Drawn.Packets = Model.Dress != nullptr ? &Model.Dress->Packets() : &Model.BonePackets;
		Drawn.Scale = Model.Scale;
		if (!Drawn.bShown)
		{
			Drawn.Shadow.bShown = false;
			return;
		}
		Drawn.RootLocal = Model.Bones[0].Local;
		Drawn.RootWorld = Model.Bones[0].World;
		Drawn.Placement = Model.Placement;
		// A worn model's bones placed over the model's as they stand, and drawn in their stead.
		if (Model.Dress != nullptr)
		{
			Model.Dress->Place(Model.Bones, Model.DressBones);
		}
		const std::vector<FGearBone>& Bones = Model.Dress != nullptr ? Model.DressBones : Model.Bones;
		for (FGearFramePart& Part : Drawn.Parts)
		{
			const FGearBone& Bone = Bones[Part.Bone];
			Part.bShown = Bone.bShown != 0;
			Part.World = Bone.World;
			Part.Billboard = Bone.Billboard;
		}
		LayShadow(Models, Model, Drawn.Shadow);
		for (FGearCloth& Cloth : Model.Cloths)
		{
			if (Models.Faulted())
			{
				return;
			}
			StepCloth(Models, Model, Cloth);
		}
	}

	void ClearFrame(FGearModels& Models, int32 Slot)
	{
		FGearFrameModel& Drawn = Models.DrawnFrame.Models[static_cast<std::size_t>(Slot)];
		Drawn.bPresent = false;
		Drawn.bShown = false;
		Drawn.Placement.reset();
		Drawn.Parts.clear();
		Drawn.Shadow.bShown = false;
		Drawn.Model = nullptr;
		Drawn.Packets = nullptr;
		Drawn.Cloths = nullptr;
	}
}
