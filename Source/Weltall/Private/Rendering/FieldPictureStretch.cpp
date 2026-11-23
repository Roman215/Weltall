#include "FieldDrawBuilder.h"

#include "Field/FieldScene.h"
#include "Graphics/Gte.h"

#include <algorithm>

namespace
{
	/** The original's 320 x 224 screen as the target shows it (FieldScreenPlacement), and the widest a picture's scene is drawn. */
	constexpr float PictureScreenAspect = 320.0f / 224.0f;
	constexpr float PictureWidestAspect = 21.0f / 9.0f;

	FVector3f PictureAnchorPoint(const std::array<float, 3>& Point)
	{
		return FVector3f(Point[0], Point[1], Point[2]);
	}

	/** Every corner of [First, First + Count) moves by Anchor. */
	void AnchorCorners(FFieldPictureStretch& Stretch, uint32 First, uint32 Count, const FVector3f& Anchor)
	{
		for (uint32 Corner = First; Corner < First + Count; ++Corner)
		{
			Stretch.CornerAnchors[static_cast<int32>(Corner)] = Anchor;
			Stretch.CornerMoves[static_cast<int32>(Corner)] = 1;
		}
	}
}

void AddFieldPictureStretch(const Weltall::FFieldScene& Scene, const Weltall::FFieldFrame& Latest, const Weltall::FFieldFrame* Previous, float Alpha,
	const FFieldSceneMesh& Mesh, FFieldSceneDraw& Draw)
{
	FFieldPictureStretch& Stretch = Draw.PictureStretch;
	Stretch.Pictures.Reset();
	Stretch.ObjectAnchors.Reset();
	Stretch.CornerAnchors.Reset();
	Stretch.CornerMoves.Reset();
	Stretch.bStaged = std::any_of(Scene.Objects.begin(), Scene.Objects.end(), [](const Weltall::FFieldSceneObject& Object) { return Object.bPicture; });
	if (!Stretch.bStaged)
	{
		return;
	}
	Stretch.ObjectAnchors.Init(FFieldPictureStretch::NoAnchor, Draw.Objects.Num() / 3);
	for (int32 Object = 0; Object < static_cast<int32>(Scene.Objects.size()); ++Object)
	{
		if (Scene.Objects[static_cast<std::size_t>(Object)].bPicture)
		{
			Stretch.Pictures.Add(Object);
		}
		else
		{
			Stretch.ObjectAnchors[Object] = Object;
		}
	}
	const std::vector<Weltall::FFieldGearLayoutItem>& Items = Latest.Gears.Items;
	const int32 FirstGear = static_cast<int32>(Mesh.FirstGearObject());
	// A gear's objects move together, by its first: its floor shadow, which lies under the model.
	const auto LeadOf = [&Items](int32 Slot)
	{ return static_cast<int32>(std::find_if(Items.begin(), Items.end(), [Slot](const Weltall::FFieldGearLayoutItem& Item) { return Item.Slot == Slot; }) - Items.begin()); };
	if (Draw.bGearsDrawn)
	{
		for (int32 Object = 0; Object < static_cast<int32>(Items.size()) && FirstGear + Object < Stretch.ObjectAnchors.Num(); ++Object)
		{
			Stretch.ObjectAnchors[FirstGear + Object] = FirstGear + LeadOf(Items[static_cast<std::size_t>(Object)].Slot);
		}
	}

	Stretch.CornerAnchors.SetNumZeroed(Draw.DynamicCorners.Num());
	Stretch.CornerMoves.SetNumZeroed(Draw.DynamicCorners.Num());
	// The characters' quads come first, each by its character's origin as drawn.
	for (int32 Quad = 0; Quad < static_cast<int32>(Latest.Quads.size()); ++Quad)
	{
		const Weltall::FFieldCharacterView& Character = Latest.Characters[Latest.Quads[static_cast<std::size_t>(Quad)].Character];
		const FVector3f Anchor = Character.bOnFloor ? PictureAnchorPoint(Character.Origin) + FieldCharacterOffset(Character, Previous, Alpha) : FVector3f::ZeroVector;
		AnchorCorners(Stretch, static_cast<uint32>(Quad) * QuadCorners, QuadCorners, Anchor);
	}
	// A particle by its own middle.
	for (uint32 Particle = 0; Particle < Latest.Particles.size(); ++Particle)
	{
		const uint32 First = Draw.ParticleCorners + Particle * QuadCorners;
		FVector3f Middle = FVector3f::ZeroVector;
		for (uint32 Corner = First; Corner < First + QuadCorners; ++Corner)
		{
			Middle += Draw.DynamicCorners[static_cast<int32>(Corner)].Position / static_cast<float>(QuadCorners);
		}
		AnchorCorners(Stretch, First, QuadCorners, Middle);
	}
	// A child's model by its origin.
	const Weltall::FFieldSpriteModelsFrame& Models = Latest.SpriteModels;
	for (int32 Polygon = 0; Polygon < Draw.SpriteModelCorners.Num(); ++Polygon)
	{
		const uint32 First = Draw.SpriteModelCorners[Polygon];
		if (First == FFieldSceneDraw::NoRedirect)
		{
			continue;
		}
		const Weltall::FFieldSpriteModelPolygon& Drawn = Models.Polygons[static_cast<std::size_t>(Polygon)];
		AnchorCorners(Stretch, First, Drawn.CornerCount == 4 ? QuadCorners : 3, PictureAnchorPoint(Weltall::ViewPoint(Models.Models[Drawn.Model].Matrix, {0, 0, 0})));
	}
	// A cloth by its gear's place in the world, which its corners lie in.
	if (Draw.bGearsDrawn)
	{
		const uint32 FirstCloth = Draw.FirstClothCorner - Mesh.Corners();
		for (uint32 Triangle = 0; Triangle < Latest.Gears.Cloths.size(); ++Triangle)
		{
			const int32 Lead = LeadOf(Latest.Gears.Cloths[Triangle].Slot);
			if (Lead < static_cast<int32>(Latest.Gears.Objects.size()))
			{
				const std::array<int32, 3>& World = Latest.Gears.Objects[static_cast<std::size_t>(Lead)].World;
				AnchorCorners(Stretch, FirstCloth + 3 * Triangle, 3, FVector3f(static_cast<float>(World[0]), static_cast<float>(World[1]), static_cast<float>(World[2])));
			}
		}
	}
}

float FieldPictureStretchFactor(FIntPoint Size)
{
	if (Size.Y <= 0)
	{
		return 1.0f;
	}
	const float Aspect = static_cast<float>(Size.X) / static_cast<float>(Size.Y);
	return FMath::Max(FMath::Min(Aspect, PictureWidestAspect) / PictureScreenAspect, 1.0f);
}

FIntRect FieldPictureArea(FIntPoint Size)
{
	const int32 Widest = FMath::RoundToInt(static_cast<float>(Size.Y) * PictureWidestAspect);
	if (Size.X <= Widest)
	{
		return FIntRect(FIntPoint::ZeroValue, Size);
	}
	const int32 Left = (Size.X - Widest) / 2;
	return FIntRect(Left, 0, Left + Widest, Size.Y);
}

bool StretchFieldPictureScene(const FFieldSceneDraw& Draw, float Factor, TArray<FVector4f>& Objects, TArray<FFieldSceneCorner>& Corners)
{
	const FFieldPictureStretch& Stretch = Draw.PictureStretch;
	if (!Stretch.bStaged || Factor <= 1.0f || Stretch.ObjectAnchors.Num() * 3 != Draw.Objects.Num() || Stretch.CornerMoves.Num() != Draw.DynamicCorners.Num())
	{
		return false;
	}
	Objects = Draw.Objects;
	Corners = Draw.DynamicCorners;
	const float Further = Factor - 1.0f;
	// A picture's first row gives its points' place across the view: scaled, they are drawn as far across again.
	for (const int32 Picture : Stretch.Pictures)
	{
		Objects[3 * Picture] *= Factor;
	}
	for (int32 Object = 0; Object < Stretch.ObjectAnchors.Num(); ++Object)
	{
		const int32 Anchor = Stretch.ObjectAnchors[Object];
		if (Anchor != FFieldPictureStretch::NoAnchor)
		{
			Objects[3 * Object].W += Further * Draw.Objects[3 * Anchor].W;
		}
	}
	for (int32 Corner = 0; Corner < Corners.Num(); ++Corner)
	{
		if (Stretch.CornerMoves[Corner] == 0)
		{
			continue;
		}
		// Moved along its object's first row, so that only its place across the view changes, by its anchor's place there.
		const FVector4f& Across = Draw.Objects[3 * static_cast<int32>(Corners[Corner].Object)];
		const FVector3f Axis(Across.X, Across.Y, Across.Z);
		const float Length = Axis.SizeSquared();
		if (Length > 0.0f)
		{
			const float AnchorX = FVector3f::DotProduct(Axis, Stretch.CornerAnchors[Corner]) + Across.W;
			Corners[Corner].Position += Axis * (Further * AnchorX / Length);
		}
	}
	return true;
}
