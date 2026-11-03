#include "BattleObjectMachine.h"

#include "Battle/Show/BattleStageFiles.h"
#include "Field/FieldGearDrawing.h"
#include "Gears/GearCloth.h"
#include "Graphics/GteVectors.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace BattleObjectMachine;

		constexpr int32 LoudestEntry = 0x2D8;
		constexpr uint8 FullLoudness = 0x6B;
		/** The ordering table's last entry. */
		constexpr int32 LastEntry = 0xFFF;
		constexpr int32 WindQuarterTurn = 0x400;
		/** A trail's colours are kept in 64ths. */
		constexpr int32 TrailColourShift = 6;

		int16 Over4096(int32 Value)
		{
			return static_cast<int16>((Value < 0 ? Value + 0xFFF : Value) >> 12);
		}

		FGteVector Short(const FGteLongVector& Vector)
		{
			return {static_cast<int16>(Vector[0]), static_cast<int16>(Vector[1]), static_cast<int16>(Vector[2])};
		}

		/** A bone of the object the drawing names, or a fault past the skeleton. */
		const FGearBone* BoneOf(FBattleObjects& Objects, const FBattleObject& Object, int32 Bone)
		{
			if (Bone < 0 || static_cast<std::size_t>(Bone) >= Object.Bones.size())
			{
				Objects.Fail("object " + std::to_string(Object.Slot) + "'s drawing names bone " + std::to_string(Bone) + ", which it does not have");
				return nullptr;
			}
			return &Object.Bones[static_cast<std::size_t>(Bone)];
		}

		/** Each point light's direction and strength toward the object's middle, into rows 1 and 2 of the objects' light matrix. */
		void LightToward(FBattleObjects& Objects, const FBattleObject& Object)
		{
			const FGearBone& Root = Object.Bones[0];
			for (std::size_t Index = 0; Index < Objects.PointLights.size(); ++Index)
			{
				const FBattlePointLight& Lamp = Objects.PointLights[Index];
				std::array<int16, 3>& Row = Objects.LightMatrix.M[Index + 1];
				if (Lamp.Strength == 0)
				{
					Row = {0, 0, 0};
					continue;
				}
				// The object's middle: the original shifts the root's height together with the scaled half height, so a
				// root off the floor counts for an 8192th of itself and the half height rounds toward the floor.
				const int32 ScaledHeight = Object.Height * ((static_cast<uint16>(Object.Scale) * Root.Scale[1]) >> 12);
				const int32 Middle = (Root.Position[1] - ScaledHeight) >> 13;
				const std::array<int32, 3> Toward = {static_cast<int16>(Lamp.Position[0] - static_cast<uint16>(Root.Position[0])),
					static_cast<int16>(Lamp.Position[1] - Middle), static_cast<int16>(Lamp.Position[2] - static_cast<uint16>(Root.Position[2]))};
				const int32 Distance = GearMachine::Distance(Objects, Toward[0], Toward[1], Toward[2]) + 1;
				const int32 Reach = Lamp.Strength + Distance;
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					const int32 Unit = GearMachine::Divide(Objects, GearMachine::WrapMul(Toward[Axis], 0x1000), Distance);
					Row[Axis] = static_cast<int16>(GearMachine::Divide(Objects, GearMachine::WrapMul(Unit, Lamp.Strength), Reach));
				}
			}
		}

		/**
		 * The floor shadow as the drawing lays it (under bone 1, turned with its forward axis, on the floor, shrinking
		 * as the root rises), linked at its nearest corner's entry, whose depth also sets how loud the object's sounds
		 * play.
		 */
		void LayShadow(FBattleObjects& Objects, FBattleObject& Object, int32 Slot, const FGteMatrix& View, FBattleObjectsFrame& Frame)
		{
			if (Object.Bones.size() < 2)
			{
				Objects.Fail("object " + std::to_string(Object.Slot) + "'s shadow reads a bone 1 it does not have");
				return;
			}
			const FGearBone& Root = Object.Bones[0];
			const FGteMatrix Placed = CompMatrix(Root.Local, Object.Bones[1].World);
			const FGteLongVector Ahead = RotTrans(Placed, {0, 0, 0x1000});
			const FGteLongVector Origin = RotTrans(Placed, {0, 0, 0});
			const int16 Yaw = static_cast<int16>(-GearMachine::Ratan2(Objects, Ahead[2] - Origin[2], Ahead[0] - Origin[0]));
			const int32 Rise = Object.FloorY - Root.Position[1];
			const int16 Scale = static_cast<int16>(std::max(0, Object.Scale - ((Rise < 0 ? Rise + 3 : Rise) >> 2)));
			FGteMatrix OnFloor = RotationY(Yaw);
			OnFloor.T = {Origin[0], Object.FloorY, Origin[2]};
			OnFloor = CompMatrix(View, OnFloor);
			FGteMatrix Scaling;
			Scaling.M[0][0] = Scaling.M[1][1] = Scaling.M[2][2] = Scale;
			OnFloor.M = MulMatrix(OnFloor, Scaling).M;
			FBattleDrawnShadow& Shadow = Frame.Shadows.emplace_back();
			Shadow.Matrix = OnFloor;
			Shadow.Corners = {{{Object.RadiusX, 0, Object.RadiusZ}, {static_cast<int16>(-Object.RadiusX), 0, Object.RadiusZ},
				{Object.RadiusX, 0, static_cast<int16>(-Object.RadiusZ)}, {static_cast<int16>(-Object.RadiusX), 0, static_cast<int16>(-Object.RadiusZ)}}};
			const std::array<uint8, 3> Colour = Objects.Stage->ShadowColour();
			Shadow.Colour = {Colour[0], Colour[1], Colour[2]};
			Shadow.Slot = static_cast<uint8>(Slot);
			FGte Projection = *Objects.Gte;
			Projection.Transform = OnFloor;
			int32 Nearest = 0;
			for (std::size_t Corner = 0; Corner < Shadow.Corners.size(); ++Corner)
			{
				const int32 Depth = Projection.Rtps(Shadow.Corners[Corner]).ScreenZ >> 2;
				Nearest = Corner == 0 ? Depth : std::min(Nearest, Depth);
			}
			const int32 Entry = Nearest >> Objects.OrderingShift;
			Frame.Links.push_back({EBattleObjectDraw::Shadow, static_cast<uint32>(Frame.Shadows.size() - 1), Entry});
			Object.Loudness = static_cast<uint8>(FullLoudness - (std::min(Entry, LoudestEntry) >> 3));
		}

		/**
		 * Each cloth's anchors and spheres placed on their bones, its step in the wind, then its triangles drawn under
		 * Gte (its transform the view, its lights the last part's) into the frame.
		 */
		void StepCloths(FBattleObjects& Objects, FBattleObject& Object, const FGte& Gte, FBattleObjectsFrame& Frame)
		{
			const FGearBone& Root = Object.Bones[0];
			for (std::size_t Number = 0; Number < Object.Cloths.size(); ++Number)
			{
				FGearCloth& Cloth = Object.Cloths[Number];
				if (Cloth.Bone < 0 || Objects.Faulted())
				{
					continue;
				}
				const int32 Yaw = Root.Angles[1] + WindQuarterTurn;
				const std::array<int16, 3> Wind = {Over4096(GearMachine::WrapMul(-Objects.ClothWind, GteCos(Yaw))), Object.WindY,
					Over4096(GearMachine::WrapMul(Objects.ClothWind, GteSin(Yaw)))};
				const FGearBone* Bone = BoneOf(Objects, Object, Cloth.Bone);
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
					const FGearBone* Holder = BoneOf(Objects, Object, Sphere.Bone);
					if (Holder == nullptr)
					{
						return;
					}
					Sphere.World = Short(RotTrans(CompMatrix(Root.Local, Holder->World), Sphere.Local));
				}
				if (!StepGearCloth(Cloth, Wind, static_cast<uint16>(Object.Scale), Object.FloorY))
				{
					Objects.Fail("object " + std::to_string(Object.Slot) + "'s cloth has a point in no triangle, whose normal divides by zero");
					return;
				}
				const std::size_t First = Frame.Cloths.size();
				DrawGearCloth(Cloth, Gte, GearClothCulling(Objects.Culling), LastEntry, Objects.OrderingShift, Object.Slot, static_cast<uint8>(Number),
					Frame.Cloths);
				for (std::size_t Drawn = First; Drawn < Frame.Cloths.size(); ++Drawn)
				{
					Frame.Links.push_back({EBattleObjectDraw::Cloth, static_cast<uint32>(Drawn), Frame.Cloths[Drawn].Bucket});
				}
			}
		}

		/** A billboarded part keeps the object's scale across and ahead, and turns about the up axis (kind 1) or faces the screen. */
		void Billboard(FGteMatrix& Matrix, int16 Kind, int16 Scale, const FGteMatrix& ViewRoot)
		{
			Matrix.M[0][2] = Matrix.M[1][0] = Matrix.M[1][2] = Matrix.M[2][0] = 0;
			Matrix.M[0][0] = Matrix.M[2][2] = Scale;
			if (Kind == 1)
			{
				for (std::size_t Row = 0; Row < 3; ++Row)
				{
					Matrix.M[Row][1] = ViewRoot.M[Row][1];
				}
			}
			else
			{
				Matrix.M[0][1] = Matrix.M[2][1] = 0;
				Matrix.M[1][1] = Scale;
			}
		}

		/** A new quad of the trails' pool for an emitter, its colours and life from the emitter. */
		FBattleTrailSegment& StartSegment(FBattleObjects& Objects, FBattleTrailEmitter& Emitter)
		{
			Emitter.Counter = 1;
			Emitter.Segment = AllocateTrailSegment(Objects, Emitter.Blend);
			FBattleTrailSegment& Segment = Objects.TrailSegments[static_cast<std::size_t>(Emitter.Segment)];
			Segment.Kind = Emitter.Kind;
			Segment.Age = 0;
			Segment.Life = Emitter.Life;
			Segment.Colour = Emitter.Colour;
			Segment.Fade = Emitter.Fade;
			return Segment;
		}

		/** A screen point as the GTE stores it in a word: x in the low half, y in the high. */
		int32 ScreenWord(const FGteProjection& Point)
		{
			return static_cast<int32>(static_cast<uint32>(static_cast<uint16>(Point.ScreenX)) | static_cast<uint32>(static_cast<uint16>(Point.ScreenY)) << 16);
		}

		void SetScreenCorner(FGteVector& Corner, int32 Word)
		{
			Corner[0] = static_cast<int16>(Word);
			Corner[1] = static_cast<int16>(static_cast<uint32>(Word) >> 16);
		}

		FGteVector WorldCorner(const std::array<int32, FBattleTrailEmitter::SampleWords>& Samples, std::size_t First)
		{
			return {static_cast<int16>(Samples[First]), static_cast<int16>(Samples[First + 1]), static_cast<int16>(Samples[First + 2])};
		}

		/**
		 * Each emitter's sample of its bone's two points, a quad started every few samples and drawn to the
		 * latest. The first sample of a started emitter takes no quad and leaves the loop on the same
		 * emitter: it is sampled again at once, and the object's last emitter goes without a sample.
		 */
		void SampleTrails(FBattleObjects& Objects, FBattleObject& Object, const FGteMatrix& ViewRoot)
		{
			constexpr std::size_t SecondEnd = FBattleTrailEmitter::SampleWords / 2;
			const FGearBone& Root = Object.Bones[0];
			std::size_t Index = 0;
			for (std::size_t Pass = 0; Pass < Object.Trails.size() && !Objects.Faulted(); ++Pass)
			{
				FBattleTrailEmitter& Emitter = Object.Trails[Index];
				if (Emitter.Bone < 0)
				{
					++Index;
					continue;
				}
				const FGearBone* Bone = BoneOf(Objects, Object, Emitter.Bone);
				if (Bone == nullptr)
				{
					return;
				}
				Emitter.Ring = static_cast<int16>((Emitter.Ring - 1) & 7);
				const std::size_t Ring = static_cast<std::size_t>(Emitter.Ring);
				std::array<int32, FBattleTrailEmitter::SampleWords>& Samples = Emitter.Samples;
				// A world sample's first word, and the one before it.
				const std::size_t Latest = 4 * (Ring & 1);
				const std::size_t Earlier = 4 - Latest;
				if (Emitter.Kind == 0)
				{
					FGte Projection = *Objects.Gte;
					Projection.Transform = CompMatrix(ViewRoot, Bone->World);
					Samples[Ring] = ScreenWord(Projection.Rtps(Emitter.Points[0]));
					Samples[SecondEnd + Ring] = ScreenWord(Projection.Rtps(Emitter.Points[1]));
				}
				else
				{
					const FGteMatrix OnBone = CompMatrix(Root.Local, Bone->World);
					const FGteLongVector A = RotTrans(OnBone, Emitter.Points[0]);
					const FGteLongVector B = RotTrans(OnBone, Emitter.Points[1]);
					std::copy(A.begin(), A.end(), Samples.begin() + static_cast<std::ptrdiff_t>(Latest));
					std::copy(B.begin(), B.end(), Samples.begin() + static_cast<std::ptrdiff_t>(SecondEnd + Latest));
				}
				Emitter.Counter = static_cast<int16>(Emitter.Counter + 1);
				if (Emitter.Counter == 0)
				{
					continue;
				}
				if (Emitter.SegmentSamples < Emitter.Counter || Emitter.Segment < 0)
				{
					FBattleTrailSegment& Started = StartSegment(Objects, Emitter);
					if (Emitter.Kind == 0)
					{
						const std::size_t Before = static_cast<std::size_t>((Emitter.Ring + Emitter.Counter) & 7);
						SetScreenCorner(Started.Corners[0], Samples[Before]);
						SetScreenCorner(Started.Corners[2], Samples[SecondEnd + Before]);
					}
					else
					{
						Started.Corners[0] = WorldCorner(Samples, Earlier);
						Started.Corners[2] = WorldCorner(Samples, SecondEnd + Earlier);
					}
				}
				FBattleTrailSegment& Segment = Objects.TrailSegments[static_cast<std::size_t>(Emitter.Segment)];
				if (Emitter.Kind == 0)
				{
					SetScreenCorner(Segment.Corners[1], Samples[Ring]);
					SetScreenCorner(Segment.Corners[3], Samples[SecondEnd + Ring]);
				}
				else
				{
					Segment.Corners[1] = WorldCorner(Samples, Latest);
					Segment.Corners[3] = WorldCorner(Samples, SecondEnd + Latest);
				}
				++Index;
			}
		}
	}

	namespace BattleObjectMachine
	{
		int32 AllocateTrailSegment(FBattleObjects& Objects, uint8 Blend)
		{
			const int32 Count = static_cast<int32>(Objects.TrailSegments.size()) - 1;
			const int32 Lowest = Objects.TrailLowest;
			if (Lowest < Count && Objects.TrailSegments[static_cast<std::size_t>(Lowest)].Age == -1)
			{
				Objects.TrailLowest = static_cast<int16>(Lowest + 1);
				while (Objects.TrailLowest < Count && Objects.TrailSegments[static_cast<std::size_t>(Objects.TrailLowest)].Age != -1)
				{
					++Objects.TrailLowest;
				}
				Objects.TrailSegments[static_cast<std::size_t>(Lowest)].bSemiTransparent = Blend != 0;
				return Lowest;
			}
			// A full pool hands out its spare quad, which is never drawn nor aged.
			return Count;
		}
	}

	void FBattleObjects::UpdatePointLights()
	{
		for (FBattlePointLight& Lamp : PointLights)
		{
			if (Lamp.Strength == 0)
			{
				continue;
			}
			const FBattleObject* Carrier = Lamp.Slot < 0 ? nullptr : Object(Lamp.Slot);
			if (Carrier == nullptr)
			{
				Lamp.Position = Lamp.Offset;
				continue;
			}
			const FGearBone* Bone = BoneOf(*this, *Carrier, Lamp.Bone + 1);
			if (Bone == nullptr)
			{
				return;
			}
			Lamp.Position = Short(RotTrans(CompMatrix(Carrier->Bones[0].Local, Bone->World), Lamp.Offset));
		}
		Drawn.PointLights = PointLights;
	}

	void FBattleObjects::DrawObject(int32 Slot, const FGteMatrix& View, int32 CatchUp)
	{
		FBattleObject& Shown = Slots[static_cast<std::size_t>(Slot)].Object;
		// The GTE takes the object's ambient light whether the object is drawn or not, and keeps it for what is drawn after.
		const std::array<uint8, 3> Ambient = ((PulseMask >> Slot) & 1) != 0 ? PulsedAmbient : Stage->AmbientColour();
		for (std::size_t Channel = 0; Channel < 3; ++Channel)
		{
			Gte->BackColor[Channel] = Ambient[Channel] * 16;
		}
		if (Shown.bVisible == 0)
		{
			return;
		}
		LightToward(*this, Shown);
		FGte Drawing = *Gte;
		const FGearBone& Root = Shown.Bones[0];
		const FGteMatrix ViewRoot = CompMatrix(View, Root.Local);
		if ((Shown.Flags & BattleObjectFlags::NoShadow) == 0 && bShadows)
		{
			LayShadow(*this, Shown, Slot, View, Drawn);
		}
		const FGteMatrix RootLight = MulMatrix(LightMatrix, Root.World);
		const BattleObjectMachine::FPartDrawing How{EFieldDrawMode::Relit, OrderingShift, 0, (Shown.Flags & BattleObjectFlags::NoBoxCulling) == 0};
		// A dressed object draws the dress's pieces, placed over its bones as they stand; its own are never drawn.
		if (Shown.Dress != nullptr)
		{
			Shown.Dress->Place(Shown.Bones, Shown.DressBones);
		}
		const std::vector<FGearBone>& Bones = Shown.Dress != nullptr ? Shown.DressBones : Shown.Bones;
		const std::span<const FGearBonePackets> AllPackets = Shown.Dress != nullptr ? std::span<const FGearBonePackets>(Shown.Dress->Packets()) : *Shown.Packets;
		const FMdlModel& Model = Shown.Dress != nullptr ? Shown.Dress->Model().Model : Shown.Model->Model;
		const auto DrawBonePart = [&](const FGearBone& Placed, const FMdlPart& Piece, const FGearBonePackets& Packets, std::size_t Number, bool bPortRows)
		{
			Drawing.LightMatrix = MulMatrix(RootLight, Placed.World);
			Gte->LightMatrix = Drawing.LightMatrix;
			FBattleDrawnPart Part{CompMatrix(ViewRoot, Placed.World), &Piece, &Packets, static_cast<uint8>(Slot), static_cast<uint16>(Number), Ambient,
				bPortRows};
			if (Placed.Billboard > 0)
			{
				Billboard(Part.Matrix, Placed.Billboard, Shown.Scale, ViewRoot);
			}
			Drawing.Transform = Part.Matrix;
			BattleObjectMachine::DrawPart(*this, Drawn, Projected, Drawing, Part, How);
		};
		std::size_t Cursor = 0;
		for (std::size_t Bone = 1; Bone < Bones.size() && !Faulted(); ++Bone)
		{
			const FGearBone& Placed = Bones[Bone];
			if (Placed.Part == FGearBone::NoPart || Placed.bShown == 0)
			{
				continue;
			}
			const FGearBonePackets* Packets = BattleObjectMachine::BonePackets(*this, AllPackets, Slot, Bone, Cursor);
			if (Packets == nullptr)
			{
				return;
			}
			DrawBonePart(Placed, Model.Parts[Placed.Part], *Packets, Bone, false);
		}
		// The performer's own pieces the dress keeps, on its own bones as they stand, whenever its moves show them.
		if (Shown.Dress != nullptr)
		{
			for (const FGearBonePackets& Kept : Shown.Dress->PerformerPackets())
			{
				const FGearBone& Placed = Shown.Bones[Kept.Bone];
				if (!Faulted() && Placed.Part != FGearBone::NoPart && Placed.bShown != 0)
				{
					DrawBonePart(Placed, Shown.Model->Model.Parts[Kept.Part], Kept, Bones.size() + Kept.Bone, true);
				}
			}
		}
		if (!Shown.Cloths.empty() && !Faulted())
		{
			// Lit by the light matrix the GTE holds: the last part's, or without one whatever was lit before.
			Drawing.Transform = View;
			StepCloths(*this, Shown, Drawing, Drawn);
		}
		for (FBattleClutAnimation& Animation : Shown.ClutAnimations)
		{
			StepClutAnimation(*this, Shown, Animation, CatchUp);
		}
		SampleTrails(*this, Shown, ViewRoot);
	}

	void FBattleObjects::DrawTrails(const FGteMatrix& View, int32 Ticks)
	{
		// The pool's quads are drawn, then age with the steps; one past its life goes back to the pool.
		FGte Projection = *Gte;
		Projection.Transform = View;
		const int32 Count = static_cast<int32>(TrailSegments.size()) - 1;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FBattleTrailSegment& Segment = TrailSegments[static_cast<std::size_t>(Index)];
			if (Segment.Age == -1)
			{
				continue;
			}
			if (Segment.Age >= Segment.Life)
			{
				if (Index <= TrailLowest)
				{
					TrailLowest = static_cast<int16>(Index);
				}
				Segment.Age = -1;
				continue;
			}
			FBattleTrailQuad& Quad = Drawn.Trails.emplace_back();
			Quad.Segment = static_cast<uint16>(Index);
			Quad.Kind = Segment.Kind;
			Quad.Corners = Segment.Corners;
			Quad.bSemiTransparent = Segment.bSemiTransparent;
			for (std::size_t Channel = 0; Channel < 3; ++Channel)
			{
				Quad.Colour[Channel] = static_cast<uint8>(static_cast<uint16>(Segment.Colour[Channel]) >> TrailColourShift);
			}
			// A quad on the screen goes into the table's first entry, one in the world at its third corner's depth.
			const int32 Entry = Quad.Kind == 0 ? 0 : (Projection.Rtps(Quad.Corners[2]).ScreenZ >> 2) >> OrderingShift;
			Drawn.Links.push_back({EBattleObjectDraw::Trail, static_cast<uint32>(Drawn.Trails.size() - 1), Entry});
			Segment.Age = static_cast<int16>(Segment.Age + Ticks);
			for (std::size_t Channel = 0; Channel < 3; ++Channel)
			{
				Segment.Colour[Channel] = static_cast<int16>(Segment.Colour[Channel] - Segment.Fade[Channel] * Ticks);
			}
		}
	}

	void FBattleObjects::ExchangeFrame(FBattleObjectsFrame& Kept)
	{
		Drawn.TextureSeeds.clear();
		const std::vector<FTexturePictureSeed>* Last = nullptr;
		for (const FSlot& Entry : Slots)
		{
			const std::vector<FTexturePictureSeed>* Seeds = Entry.bPresent ? Entry.Object.TextureSeeds.get() : nullptr;
			if (Seeds != nullptr && Seeds != Last)
			{
				Drawn.TextureSeeds.insert(Drawn.TextureSeeds.end(), Seeds->begin(), Seeds->end());
				Last = Seeds;
			}
		}
		std::swap(Drawn, Kept);
	}

	void FBattleObjects::DrawStage(const FGteMatrix& View, bool bShown)
	{
		Drawn.Parts.clear();
		Drawn.Polygons.clear();
		Drawn.Shadows.clear();
		Drawn.Cloths.clear();
		Drawn.Trails.clear();
		Drawn.Links.clear();
		Drawn.View = View;
		const FBattleObject* StageObject = Object(StageSlot);
		if (bShown && StageObject != nullptr && !Faulted())
		{
			BattleObjectMachine::DrawStageModel(*this, Drawn, Projected, *StageObject, View);
		}
	}
}
