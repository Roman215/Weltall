#include "Field/FieldGearDrawing.h"

#include "Field/FieldLight.h"
#include "Field/FieldRenderState.h"
#include "Gears/GearCloth.h"
#include "Gears/GearModels.h"
#include "Graphics/GteVectors.h"
#include "Graphics/Vram.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Weltall
{
	namespace
	{
		FGteMatrix Diagonal(int16 Scale)
		{
			FGteMatrix Result;
			Result.M[0][0] = Result.M[1][1] = Result.M[2][2] = Scale;
			return Result;
		}

		/** The gears' light matrix (row i light i's direction) and colour matrix (column i its colour), as their words hold them. */
		void LoadLights(const FFieldGearLights& Lights, FGteMatrix& Light, FGte& Gte)
		{
			for (int32 Row = 0; Row < 3; ++Row)
			{
				for (int32 Column = 0; Column < 3; ++Column)
				{
					const std::size_t At = static_cast<std::size_t>(3 * Row + Column);
					Light.M[static_cast<std::size_t>(Row)][static_cast<std::size_t>(Column)] = Lights.Words[At];
				}
			}
			LoadFieldGearColors(Lights, Gte);
		}

		/** A billboarded part keeps the model's scale across and ahead, and turns about the up axis (mode 1) or faces the screen. */
		void Billboard(FGteMatrix& Matrix, int16 Mode, int16 Scale, const FGteMatrix& RootView)
		{
			Matrix.M[0][2] = Matrix.M[1][0] = Matrix.M[1][2] = Matrix.M[2][0] = 0;
			Matrix.M[0][0] = Matrix.M[2][2] = Scale;
			if (Mode == 1)
			{
				for (std::size_t Row = 0; Row < 3; ++Row)
				{
					Matrix.M[Row][1] = RootView.M[Row][1];
				}
			}
			else
			{
				Matrix.M[0][1] = Matrix.M[2][1] = 0;
				Matrix.M[1][1] = Scale;
			}
		}

		/** A placed model's matrices under the view, as the GTE's whole numbers hold them and as real numbers. */
		struct FPlaced
		{
			/** What takes the model's own space to the view's, for its cloths. */
			FGteMatrix Space;
			FAffine FineSpace;
			/** The same times the root's local matrix, for its parts. */
			FAffine FineRoot;
			/** The model's scale as it is drawn, 1 for a model unit to a view unit. */
			float Scale = 0.0f;
		};

		int16 Rounded16(float Value)
		{
			return static_cast<int16>(std::lround(std::clamp(Value, -32768.0f, 32767.0f)));
		}

		/** A placement and a matrix of the model's own space as one GTE matrix, each entry rounded. */
		FGteMatrix PlaceMatrix(const FAffine& Placement, const FGteMatrix& Matrix)
		{
			const FAffine Placed = Placement.After(FAffine::From(Matrix));
			FGteMatrix Result;
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				for (std::size_t Column = 0; Column < 3; ++Column)
				{
					Result.M[Row][Column] = Rounded16(Placed.M[Row][Column] * static_cast<float>(FGteMatrix::One));
				}
				Result.T[Row] = static_cast<int32>(std::lround(Placed.T[Row]));
			}
			return Result;
		}

		float ColumnLength(const FAffine& Matrix, std::size_t Column)
		{
			return std::sqrt(Matrix.M[0][Column] * Matrix.M[0][Column] + Matrix.M[1][Column] * Matrix.M[1][Column] + Matrix.M[2][Column] * Matrix.M[2][Column]);
		}

		/** Billboard for a matrix of real numbers, at Scale. */
		void BillboardFine(FAffine& Matrix, int16 Mode, float Scale, const FAffine& RootView)
		{
			Matrix.M[0][2] = Matrix.M[1][0] = Matrix.M[1][2] = Matrix.M[2][0] = 0.0f;
			Matrix.M[0][0] = Matrix.M[2][2] = Scale;
			if (Mode == 1)
			{
				for (std::size_t Row = 0; Row < 3; ++Row)
				{
					Matrix.M[Row][1] = RootView.M[Row][1];
				}
			}
			else
			{
				Matrix.M[0][1] = Matrix.M[2][1] = 0.0f;
				Matrix.M[1][1] = Scale;
			}
		}

		struct FDrawing
		{
			const FFieldView& View;
			FGte Gte;
			FGteMatrix Light;
			int32 FarBucket = 0;
			EFieldCulling Culling = EFieldCulling::InRange;
			int32 OrderingShift = FieldGearOrderingShift;
			/** The light matrix the GTE holds: the last part's, before any the one the drawing before the gears left. */
			std::optional<FGteMatrix> LastLight;
			/** The tint the model's lit colours are drawn under, where it has one. */
			const std::array<uint8, 3>* Tint = nullptr;
			/** The model's placement under the view, where its host places it. */
			const FPlaced* Placed = nullptr;
		};

		constexpr uint8 NeutralTint = 0x80;

		/** A colour under a sprite's tint, as the GPU modulates a texel by it: each channel times the tint over 128. */
		FMdlColor Tinted(FMdlColor Color, const std::array<uint8, 3>& Tint)
		{
			const auto Channel = [](uint8 Value, uint8 By) { return static_cast<uint8>(std::min(0xFF, Value * By / NeutralTint)); };
			Color.R = Channel(Color.R, Tint[0]);
			Color.G = Channel(Color.G, Tint[1]);
			Color.B = Channel(Color.B, Tint[2]);
			return Color;
		}

		void DrawShadow(FDrawing& Drawing, const FGearFrameModel& Drawn, const FFieldGearLayoutItem& Item, FFieldGearObject& Object, FFieldGearsFrame& Frame)
		{
			FFieldGearPolygon& Polygon = Frame.Polygons[Item.FirstPolygon];
			const FGearFrameShadow& Shadow = Drawn.Shadow;
			FGteMatrix Placed = RotationXyz({0, Shadow.Yaw, 0});
			Placed.T = Shadow.Position;
			Object.Matrix = MulMatrix(CompMatrix(Drawing.View.View, Placed), Diagonal(Shadow.Scale));
			Object.bDrawn = true;
			FGte& Gte = Drawing.Gte;
			Gte.Transform = Object.Matrix;
			// Sorted by the nearest corner.
			int32 Nearest = 0xFFFF;
			for (const int32 Z : {1, -1})
			{
				for (const int32 X : {1, -1})
				{
					const FGteProjection Corner = Gte.Rtps({static_cast<int16>(X * Item.HalfWidth), 0, static_cast<int16>(Z * Item.HalfDepth)});
					Nearest = std::min(Nearest, Corner.ScreenZ >> 2);
				}
			}
			Polygon.Colors.fill({FieldGearShadow::Shade, FieldGearShadow::Shade, FieldGearShadow::Shade});
			Polygon.Bucket = static_cast<uint16>(Nearest >> Drawing.OrderingShift);
			Polygon.bDrawn = Polygon.Bucket <= Drawing.FarBucket;
			Polygon.Dropped = Polygon.bDrawn ? 0 : FieldPolygonDrop::BeyondFarBucket;
			if (Polygon.bDrawn)
			{
				Frame.Links.push_back({EFieldGearDraw::Polygon, Item.FirstPolygon, Polygon.Bucket});
			}
		}

		void DrawPart(FDrawing& Drawing, const FGearFrameModel& Drawn, const FGearFramePart& Part, const FGteMatrix& RootView, const FGteMatrix& RootLight,
			const FFieldGearLayoutItem& Item, FFieldGearObject& Object, FFieldGearsFrame& Frame)
		{
			FGte& Gte = Drawing.Gte;
			Gte.LightMatrix = MulMatrix(RootLight, Part.World);
			Drawing.LastLight = Gte.LightMatrix;
			Object.Matrix = CompMatrix(RootView, Part.World);
			if (Part.Billboard > 0)
			{
				const int16 Scale = Drawing.Placed != nullptr ? Rounded16(Drawing.Placed->Scale * static_cast<float>(FGteMatrix::One)) : Drawn.Scale;
				Billboard(Object.Matrix, Part.Billboard, Scale, RootView);
			}
			Object.bDrawn = true;
			Object.bFine = Drawing.Placed != nullptr;
			if (Object.bFine)
			{
				Object.Fine = Drawing.Placed->FineRoot.After(FAffine::From(Part.World));
				if (Part.Billboard > 0)
				{
					BillboardFine(Object.Fine, Part.Billboard, Drawing.Placed->Scale, Drawing.Placed->FineRoot);
				}
			}
			Gte.Transform = Object.Matrix;
			const FMdlPart& Model = *Item.Part;
			Frame.Projected.resize(Model.Vertices.size());
			for (std::size_t Vertex = 0; Vertex < Model.Vertices.size(); ++Vertex)
			{
				Frame.Projected[Vertex] = Gte.Rtps(Model.Vertices[Vertex]);
			}
			for (uint32 Number = 0; Number < Item.PolygonCount; ++Number)
			{
				const FMdlPrimitive& Primitive = Model.Primitives[Number];
				const FMdlPacket& Packet = Item.Packets->Packets[Number];
				FFieldGearPolygon& Polygon = Frame.Polygons[Item.FirstPolygon + Number];
				const FFieldPolygonTest Test = TestFieldPolygon(Primitive, Frame.Projected.data(), EFieldDrawMode::Relit, Gte, Drawing.OrderingShift);
				const bool bBeyond = Test.Bucket > Drawing.FarBucket;
				Polygon.Bucket = Test.Bucket;
				Polygon.Dropped = static_cast<uint8>(Test.Dropped | (bBeyond ? FieldPolygonDrop::BeyondFarBucket : 0));
				Polygon.bDrawn = Drawing.Culling == EFieldCulling::Original ? Polygon.Dropped == 0 : !bBeyond && Test.bInRange;
				Polygon.Colors = Packet.Colors;
				if (Packet.Relight != EMdlRelight::None)
				{
					FMdlPacket Lit = Packet;
					RelightMdlPacket(Lit, Primitive, Model, Gte);
					Polygon.Colors = Lit.Colors;
				}
				if (Drawing.Tint != nullptr)
				{
					for (FMdlColor& Color : Polygon.Colors)
					{
						Color = Tinted(Color, *Drawing.Tint);
					}
				}
				if (Polygon.bDrawn)
				{
					Frame.Links.push_back({EFieldGearDraw::Polygon, Item.FirstPolygon + Number, Polygon.Bucket});
				}
			}
		}

		std::optional<FError> DrawCloth(FDrawing& Drawing, int32 Slot, int32 ClothNumber, const FGearCloth& Cloth, FFieldGearsFrame& Frame)
		{
			if (!Drawing.LastLight)
			{
				return MakeError("the gears' drawing: a cloth is lit by the light matrix the drawing before the gears left, which is not known");
			}
			FGte& Gte = Drawing.Gte;
			// A cloth's points lie in the world, or in the space its model is placed from.
			Gte.Transform = Drawing.Placed != nullptr ? Drawing.Placed->Space : Drawing.View.View;
			Gte.LightMatrix = *Drawing.LastLight;
			const std::size_t First = Frame.Cloths.size();
			DrawGearCloth(Cloth, Gte, GearClothCulling(Drawing.Culling), Drawing.FarBucket, Drawing.OrderingShift, static_cast<uint8>(Slot),
				static_cast<uint8>(ClothNumber), Frame.Cloths);
			for (std::size_t Drawn = First; Drawn < Frame.Cloths.size(); ++Drawn)
			{
				if (Drawing.Tint != nullptr)
				{
					for (FMdlColor& Color : Frame.Cloths[Drawn].Colors)
					{
						Color = Tinted(Color, *Drawing.Tint);
					}
				}
				Frame.Links.push_back({EFieldGearDraw::Cloth, static_cast<uint32>(Drawn), Frame.Cloths[Drawn].Bucket});
			}
			if (Drawing.Placed != nullptr && Frame.Cloths.size() != First
				&& std::none_of(Frame.ClothSpaces.begin(), Frame.ClothSpaces.end(), [Slot](const FFieldGearClothSpace& Space) { return Space.Slot == Slot; }))
			{
				Frame.ClothSpaces.push_back({static_cast<uint8>(Slot), Drawing.Placed->FineSpace});
			}
			return std::nullopt;
		}
	}

	void LayOutFieldGears(const FGearModels& Models, std::vector<FFieldGearLayoutItem>& Items)
	{
		Items.clear();
		AddFieldGearLayout(Models, 0, Items);
	}

	void AddFieldGearLayout(const FGearModels& Models, int32 FirstSlot, std::vector<FFieldGearLayoutItem>& Items)
	{
		uint32 Polygon = Items.empty() ? 0 : Items.back().FirstPolygon + Items.back().PolygonCount;
		for (int32 Slot = 0; Slot < FGearModels::Slots; ++Slot)
		{
			const FGearModel* Model = Models.Model(Slot);
			if (Model == nullptr)
			{
				continue;
			}
			const bool bPortRows = Model->TextureY >= FVram::Height;
			Items.push_back({FirstSlot + Slot, nullptr, nullptr, Model->ShadowHalfWidth, Model->ShadowHalfDepth, Polygon, 1, bPortRows});
			++Polygon;
			// A worn model's parts in the model's stead (FGearModels::Wear).
			const std::vector<FGearBonePackets>& Packets = Model->Dress != nullptr ? Model->Dress->Packets() : Model->BonePackets;
			const FMdlModel& Parts = Model->Dress != nullptr ? Model->Dress->Model().Model : *Model->Parts;
			for (const FGearBonePackets& Built : Packets)
			{
				const FMdlPart& Part = Parts.Parts[Built.Part];
				const uint32 Count = static_cast<uint32>(Part.Primitives.size());
				Items.push_back({FirstSlot + Slot, &Built, &Part, 0, 0, Polygon, Count, bPortRows});
				Polygon += Count;
			}
		}
	}

	std::optional<FError> EvaluateFieldGears(const FGearModels& Models, const FFieldView& View, const FFieldGearLights& Lights, int32 FarBucket,
		EFieldCulling Culling, bool bDrawn, FFieldGearsFrame& Frame, int32 OrderingShift, const FGteMatrix* LightBefore)
	{
		Frame.Items.clear();
		Frame.Layout = Models.Revision;
		Frame.Objects.clear();
		Frame.Polygons.clear();
		Frame.Cloths.clear();
		Frame.ClothSpaces.clear();
		Frame.Links.clear();
		Frame.TextureSeeds.clear();
		return AddFieldGears(Models, 0, {}, View, Lights, FarBucket, Culling, bDrawn, Frame, OrderingShift, LightBefore);
	}

	std::optional<FError> AddFieldGears(const FGearModels& Models, int32 FirstSlot, std::span<const std::array<uint8, 3>> Tints, const FFieldView& View,
		const FFieldGearLights& Lights, int32 FarBucket, EFieldCulling Culling, bool bDrawn, FFieldGearsFrame& Frame, int32 OrderingShift,
		const FGteMatrix* LightBefore)
	{
		const std::size_t FirstItem = Frame.Items.size();
		AddFieldGearLayout(Models, FirstSlot, Frame.Items);
		for (int32 Slot = 0; Slot < FGearModels::Slots; ++Slot)
		{
			const FGearModel* Model = Models.Model(Slot);
			if (Model != nullptr && Model->TextureSeeds)
			{
				Frame.TextureSeeds.insert(Frame.TextureSeeds.end(), Model->TextureSeeds->begin(), Model->TextureSeeds->end());
			}
		}
		Frame.Objects.resize(Frame.Items.size());
		Frame.Polygons.resize(Frame.Items.empty() ? 0 : Frame.Items.back().FirstPolygon + Frame.Items.back().PolygonCount);
		FDrawing Drawing{View, FieldDrawingGte(View), {}, FarBucket, Culling, OrderingShift,
			LightBefore != nullptr ? std::optional<FGteMatrix>(*LightBefore) : std::nullopt};
		LoadLights(Lights, Drawing.Light, Drawing.Gte);
		constexpr std::array<uint8, 3> Untinted{NeutralTint, NeutralTint, NeutralTint};
		for (std::size_t At = FirstItem; At < Frame.Items.size();)
		{
			const int32 Slot = Frame.Items[At].Slot;
			std::size_t End = At + 1;
			while (End < Frame.Items.size() && Frame.Items[End].Slot == Slot)
			{
				++End;
			}
			const std::size_t ModuleSlot = static_cast<std::size_t>(Slot - FirstSlot);
			const FGearFrameModel& Drawn = Models.Frame().Models[ModuleSlot];
			Drawing.Tint = ModuleSlot < Tints.size() && Tints[ModuleSlot] != Untinted ? &Tints[ModuleSlot] : nullptr;
			// A model loaded this tick has not drawn yet.
			const bool bModelDrawn = bDrawn && Drawn.bPresent && Drawn.bShown && Drawn.Parts.size() == End - At - 1;
			// The root as the world has it: where its host places it, or its own matrix.
			const FGteMatrix RootLocal = Drawn.Placement ? PlaceMatrix(*Drawn.Placement, Drawn.RootLocal) : Drawn.RootLocal;
			FPlaced Placed;
			Drawing.Placed = nullptr;
			if (Drawn.Placement)
			{
				Placed.Space = CompMatrix(View.View, PlaceMatrix(*Drawn.Placement, FGteMatrix::Identity()));
				Placed.FineSpace = FAffine::From(View.View).After(*Drawn.Placement);
				Placed.FineRoot = Placed.FineSpace.After(FAffine::From(Drawn.RootLocal));
				Placed.Scale = static_cast<float>(Drawn.Scale) / static_cast<float>(FGteMatrix::One) * ColumnLength(*Drawn.Placement, 0);
				Drawing.Placed = &Placed;
			}
			for (std::size_t Object = At; Object < End; ++Object)
			{
				Frame.Objects[Object].World = RootLocal.T;
			}
			const std::size_t Next = std::exchange(At, End);
			if (!bModelDrawn)
			{
				continue;
			}
			if (Drawn.Shadow.bShown && !Drawn.Placement)
			{
				DrawShadow(Drawing, Drawn, Frame.Items[Next], Frame.Objects[Next], Frame);
			}
			const FGteMatrix RootView = CompMatrix(View.View, RootLocal);
			const FGteMatrix RootLight = MulMatrix(Drawing.Light, Drawn.RootWorld);
			for (std::size_t Part = 0; Part < Drawn.Parts.size(); ++Part)
			{
				const std::size_t Object = Next + 1 + Part;
				if (Drawn.Parts[Part].bShown)
				{
					DrawPart(Drawing, Drawn, Drawn.Parts[Part], RootView, RootLight, Frame.Items[Object], Frame.Objects[Object], Frame);
				}
			}
			if (Drawn.Cloths == nullptr)
			{
				continue;
			}
			for (std::size_t Number = 0; Number < Drawn.Cloths->size(); ++Number)
			{
				const FGearCloth& Cloth = (*Drawn.Cloths)[Number];
				if (Cloth.Bone < 0)
				{
					continue;
				}
				if (auto Error = DrawCloth(Drawing, Slot, static_cast<int32>(Number), Cloth, Frame))
				{
					return Error;
				}
			}
		}
		return std::nullopt;
	}
}
