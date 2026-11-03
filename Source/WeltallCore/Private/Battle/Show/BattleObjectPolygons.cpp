#include "BattleObjectMachine.h"

#include "Field/FieldSceneFrame.h"
#include "Gears/GearPackets.h"
#include "Graphics/GteVectors.h"
#include "Graphics/MdlPackets.h"

#include <string>

namespace Weltall
{
	namespace
	{
		using namespace BattleObjectMachine;

		/** The kernel's screen clip, which Battle keeps as the field set it: a point counts across below 320, down below 239. */
		constexpr uint16 ClipWidth = 320;
		constexpr uint16 ClipHeight = 239;
		/** The stage draws its draw-kind-0 bones into the table's last entry, by a shift that sends every depth there. */
		constexpr int32 FarthestShift = 16;
		constexpr int32 FarthestEntry = 0xFFF;
		constexpr int16 Unscaled = 0x1000;
		/** Battle's environment mapping, set as an object of header flag 200h is made. */
		constexpr FMdlEnvironmentMapping Environment{2, 2, 0x40, 0x40};

		/** The stage's draw kinds past the billboards: the kernel's draw modes 2 to 5. */
		enum EStageDrawKind : int16
		{
			FarthestKind = 4,
			NearestKind = 5,
			FogKind = 6,
			FogFarthestKind = 7,
		};

		/** The kernel's on-screen test of one of a triangle's points: projected at a depth the GTE could keep, on the clip's screen. */
		bool IsOnScreen(const FGte& Gte, const FGteVector& Point)
		{
			const FGteProjection At = Gte.Rtps(Point);
			return At.ScreenZ != 0 && At.ScreenZ != 0xFFFF && static_cast<uint16>(At.ScreenY) < ClipHeight && static_cast<uint16>(At.ScreenX) < ClipWidth;
		}

		/** A part's box test (flag 1): none of the part's box's corners, nor its middle, is on the screen. */
		bool IsBoxHidden(const FMdlPart& Part, const FGte& Gte)
		{
			const FGteVector& Low = Part.BoundsMin;
			const FGteVector& High = Part.BoundsMax;
			FGteVector Middle{};
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Middle[Axis] = static_cast<int16>(Low[Axis] + (High[Axis] - Low[Axis]) / 2);
			}
			const std::array<FGteVector, 9> Points{{Low, High, Middle, {High[0], Low[1], Low[2]}, {Low[0], High[1], Low[2]}, {High[0], High[1], Low[2]},
				{Low[0], Low[1], High[2]}, {High[0], Low[1], High[2]}, {Low[0], High[1], High[2]}}};
			for (const FGteVector& Point : Points)
			{
				if (IsOnScreen(Gte, Point))
				{
					return false;
				}
			}
			return true;
		}

		/**
		 * A Gouraud kind the relighting mode passes by keeps the light of its build, which the GTE's state gives as the
		 * object is made (the drawing before it, or the program before the battle): the port lights it as it is drawn.
		 */
		EMdlRelight RelightOf(const FMdlPrimitive& Primitive, const FMdlPacket& Packet)
		{
			if (Packet.Relight != EMdlRelight::None || !Primitive.Traits().bGouraud)
			{
				return Packet.Relight;
			}
			return Primitive.Traits().bTextured ? EMdlRelight::Corners : EMdlRelight::CornersTimesColor;
		}

		/** A billboarded part keeps Scale across and ahead, and turns about the up axis (kind 1, Facing's column 1) or faces the screen. */
		void Billboard(FGteMatrix& Matrix, int16 Kind, int16 Scale, const FGteMatrix& Facing)
		{
			Matrix.M[0][2] = Matrix.M[1][0] = Matrix.M[1][2] = Matrix.M[2][0] = 0;
			Matrix.M[0][0] = Matrix.M[2][2] = Scale;
			if (Kind == 1)
			{
				for (std::size_t Row = 0; Row < 3; ++Row)
				{
					Matrix.M[Row][1] = Facing.M[Row][1];
				}
			}
			else
			{
				Matrix.M[0][1] = Matrix.M[2][1] = 0;
				Matrix.M[1][1] = Scale;
			}
		}

		/** Whether a part has the flat textured kinds the fog's draw modes depth-cue. */
		bool HasFoggedKinds(const FMdlPart& Part)
		{
			for (const FMdlPrimitive& Primitive : Part.Primitives)
			{
				switch (Primitive.Type)
				{
				case EMdlPrimitiveType::TexturedTriangle:
				case EMdlPrimitiveType::StaticTexturedTriangle:
				case EMdlPrimitiveType::TexturedQuad:
				case EMdlPrimitiveType::StaticTexturedQuad:
					return true;
				default:
					break;
				}
			}
			return false;
		}
	}

	namespace BattleObjectMachine
	{
		void DrawPart(FBattleObjects& Objects, FBattleObjectsFrame& Frame, std::vector<FGteProjection>& Projected, const FGte& Gte, const FBattleDrawnPart& Part,
			const FPartDrawing& How)
		{
			const FMdlPart& Model = *Part.Part;
			const bool bOriginalCulling = Objects.Culling == EFieldCulling::Original;
			if (How.bBoxCulled && bOriginalCulling && IsBoxHidden(Model, Gte))
			{
				return;
			}
			if (Part.Packets->Packets.size() != Model.Primitives.size())
			{
				Objects.Fail("object " + std::to_string(Part.Slot) + "'s bone " + std::to_string(Part.Bone) + " draws packets built for another part");
				return;
			}
			const uint32 PartIndex = static_cast<uint32>(Frame.Parts.size());
			Frame.Parts.push_back(Part);
			Projected.resize(Model.Vertices.size());
			for (std::size_t Vertex = 0; Vertex < Model.Vertices.size(); ++Vertex)
			{
				Projected[Vertex] = Gte.Rtps(Model.Vertices[Vertex]);
			}
			for (std::size_t Number = 0; Number < Model.Primitives.size(); ++Number)
			{
				const FMdlPrimitive& Primitive = Model.Primitives[Number];
				const FFieldPolygonTest Test = TestFieldPolygon(Primitive, Projected.data(), How.Mode, Gte, How.Shift);
				if (bOriginalCulling ? Test.Dropped != 0 : !Test.bInRange)
				{
					continue;
				}
				const FMdlPacket& Packet = Part.Packets->Packets[Number];
				FBattleDrawnPolygon& Drawn = Frame.Polygons.emplace_back();
				Drawn.Part = PartIndex;
				Drawn.Number = static_cast<uint16>(Number);
				Drawn.Colors = Packet.Colors;
				if (How.Mode == EFieldDrawMode::Relit && RelightOf(Primitive, Packet) != EMdlRelight::None)
				{
					FMdlPacket Lit = Packet;
					Lit.Relight = RelightOf(Primitive, Packet);
					RelightMdlPacket(Lit, Primitive, Model, Gte);
					Drawn.Colors = Lit.Colors;
				}
				if (Primitive.Type == EMdlPrimitiveType::EnvironmentMappedTriangle)
				{
					Drawn.Texels = MdlEnvironmentTexels(Model, Primitive, Gte.Transform, Environment);
				}
				Frame.Links.push_back({EBattleObjectDraw::Polygon, static_cast<uint32>(Frame.Polygons.size() - 1), How.FirstEntry + Test.Bucket});
			}
		}

		const FGearBonePackets* BonePackets(FBattleObjects& Objects, std::span<const FGearBonePackets> Packets, int32 Slot, std::size_t Bone, std::size_t& Cursor)
		{
			while (Cursor < Packets.size() && Packets[Cursor].Bone < Bone)
			{
				++Cursor;
			}
			if (Cursor == Packets.size() || Packets[Cursor].Bone != Bone)
			{
				Objects.Fail("object " + std::to_string(Slot) + "'s bone " + std::to_string(Bone) + " has a part its making built nothing for");
				return nullptr;
			}
			return &Packets[Cursor];
		}

		void DrawStageModel(FBattleObjects& Objects, FBattleObjectsFrame& Frame, std::vector<FGteProjection>& Projected, const FBattleObject& Stage,
			const FGteMatrix& View)
		{
			FGte Gte = *Objects.Gte;
			std::size_t Cursor = 0;
			for (std::size_t Bone = 1; Bone < Stage.Bones.size() && !Objects.Faulted(); ++Bone)
			{
				const FGearBone& Drawn = Stage.Bones[Bone];
				if (Drawn.Part == FGearBone::NoPart || Drawn.bShown == 0)
				{
					continue;
				}
				const FGearBonePackets* Packets = BonePackets(Objects, *Stage.Packets, FBattleObjects::StageSlot, Bone, Cursor);
				if (Packets == nullptr)
				{
					return;
				}
				FBattleDrawnPart Part{CompMatrix(View, Drawn.World), &Stage.Model->Model.Parts[Drawn.Part], Packets, FBattleObjects::StageSlot,
					static_cast<uint16>(Bone)};
				const int16 Kind = Drawn.Billboard;
				if (Kind == 1 || Kind == 2)
				{
					Billboard(Part.Matrix, Kind, Unscaled, View);
				}
				FPartDrawing How{EFieldDrawMode::Average, Objects.OrderingShift, 0, false};
				switch (Kind)
				{
				case 0:
					How.Shift = FarthestShift;
					How.FirstEntry = FarthestEntry;
					break;
				case FarthestKind:
					How.Mode = EFieldDrawMode::Farthest;
					break;
				case NearestKind:
					How.Mode = EFieldDrawMode::Nearest;
					break;
				case FogKind:
				case FogFarthestKind:
					if (HasFoggedKinds(*Part.Part))
					{
						Objects.Fail("the stage's bone " + std::to_string(Bone) + " is drawn in fog, whose colours the battle's fog state would give: no stage asks for it");
						return;
					}
					break;
				default:
					break;
				}
				Gte.Transform = Part.Matrix;
				DrawPart(Objects, Frame, Projected, Gte, Part, How);
			}
		}
	}
}
