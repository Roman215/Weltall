#include "Rendering/FieldSceneRendering.h"

#include "Field/FieldFrame.h"
#include "FieldDrawBuilder.h"
#include "FieldGearDraw.h"
#include "Graphics/Texture.h"

bool IsNearInWorld(const std::array<int32, 3>& A, const std::array<int32, 3>& B, int32 Limit)
{
	for (std::size_t Axis = 0; Axis < 3; ++Axis)
	{
		if (FMath::Abs(A[Axis] - B[Axis]) > Limit)
		{
			return false;
		}
	}
	return true;
}

void AddBlendedFieldObject(TArray<FVector4f>& Objects, const Weltall::FGteMatrix& From, const Weltall::FGteMatrix& To, float Alpha)
{
	AddBlendedFieldObject(Objects, Weltall::FAffine::From(From), Weltall::FAffine::From(To), Alpha);
}

void AddBlendedFieldObject(TArray<FVector4f>& Objects, const Weltall::FAffine& From, const Weltall::FAffine& To, float Alpha)
{
	float Columns[3][3];
	for (int32 Column = 0; Column < 3; ++Column)
	{
		const FVector3f A(From.M[0][Column], From.M[1][Column], From.M[2][Column]);
		const FVector3f B(To.M[0][Column], To.M[1][Column], To.M[2][Column]);
		FVector3f Blended = FMath::Lerp(A, B, Alpha);
		const float Length = FMath::Lerp(A.Size(), B.Size(), Alpha);
		if (Blended.SizeSquared() > 0.0f)
		{
			Blended *= Length / Blended.Size();
		}
		Columns[Column][0] = Blended.X;
		Columns[Column][1] = Blended.Y;
		Columns[Column][2] = Blended.Z;
	}
	for (int32 Row = 0; Row < 3; ++Row)
	{
		const float Translation = FMath::Lerp(From.T[Row], To.T[Row], Alpha);
		Objects.Emplace(Columns[0][Row], Columns[1][Row], Columns[2][Row], Translation);
	}
}

namespace
{
	/** How far (view units) a frame's view may stand from its unrounded twin before the two are taken for different views (the original's eye wrapped). */
	constexpr float UnroundedReach = 256.0f;

	/** A view's rows as UE's row-vector matrix: the transpose, the translation in the last row. */
	FMatrix44f RowVectorMatrix(const Weltall::FAffine& View)
	{
		FMatrix44f Matrix = FMatrix44f::Identity;
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Column = 0; Column < 3; ++Column)
			{
				Matrix.M[Column][Row] = View.M[static_cast<std::size_t>(Row)][static_cast<std::size_t>(Column)];
			}
			Matrix.M[3][Row] = View.T[static_cast<std::size_t>(Row)];
		}
		return Matrix;
	}

	/** Every object's rows moved on by Move, a view-space point's way to where it is drawn. */
	void MoveFieldSceneObjects(FFieldSceneDraw& Draw, const FMatrix44f& Move)
	{
		for (int32 First = 0; First + 2 < Draw.Objects.Num(); First += 3)
		{
			if (First >= Draw.FirstSpriteRow)
			{
				const FVector3f Origin(Draw.Objects[First].W, Draw.Objects[First + 1].W, Draw.Objects[First + 2].W);
				const FVector3f Moved = Move.TransformPosition(Origin);
				for (int32 Axis = 0; Axis < 3; ++Axis)
				{
					Draw.Objects[First + Axis].W = Moved[Axis];
				}
				continue;
			}
			// The object's rows as a matrix taking its own points to view space, then moved.
			FMatrix44f Object = FMatrix44f::Identity;
			for (int32 Row = 0; Row < 3; ++Row)
			{
				const FVector4f& Rows = Draw.Objects[First + Row];
				Object.M[0][Row] = Rows.X;
				Object.M[1][Row] = Rows.Y;
				Object.M[2][Row] = Rows.Z;
				Object.M[3][Row] = Rows.W;
			}
			const FMatrix44f Moved = Object * Move;
			for (int32 Row = 0; Row < 3; ++Row)
			{
				Draw.Objects[First + Row] = FVector4f(Moved.M[0][Row], Moved.M[1][Row], Moved.M[2][Row], Moved.M[3][Row]);
			}
		}
	}

	/** What takes a frame's view space to its unrounded view's, where the frame knows it and the two are one view. */
	bool UnroundingOf(const Weltall::FFieldFrame& Frame, FMatrix44f& Move)
	{
		if (!Frame.bUnrounded)
		{
			return false;
		}
		const FMatrix44f Drawn = RowVectorMatrix(Weltall::FAffine::From(Frame.View.View));
		if (FMath::Abs(Drawn.Determinant()) < UE_SMALL_NUMBER)
		{
			return false;
		}
		const FMatrix44f Unrounded = RowVectorMatrix(Frame.UnroundedView);
		const FVector3f Target(static_cast<float>(Frame.Target[0]), static_cast<float>(Frame.Target[1]), static_cast<float>(Frame.Target[2]));
		if (FVector3f::Distance(Drawn.TransformPosition(Target), Unrounded.TransformPosition(Target)) > UnroundedReach)
		{
			return false;
		}
		Move = Drawn.Inverse() * Unrounded;
		return true;
	}
}

void UnroundFieldSceneView(FFieldSceneDraw& Draw, const Weltall::FFieldFrame& Latest, const Weltall::FFieldFrame* Previous, float Alpha)
{
	FMatrix44f Move;
	if (!UnroundingOf(Latest, Move))
	{
		return;
	}
	// Between ticks, Alpha of the way from the tick before's: both are all but the identity.
	FMatrix44f Before;
	if (Previous != nullptr && UnroundingOf(*Previous, Before))
	{
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Column = 0; Column < 4; ++Column)
			{
				Move.M[Row][Column] = FMath::Lerp(Before.M[Row][Column], Move.M[Row][Column], Alpha);
			}
		}
	}
	MoveFieldSceneObjects(Draw, Move);
}

void MoveFieldSceneView(FFieldSceneDraw& Draw, const Weltall::FAffine& From, const Weltall::FAffine& To)
{
	const FMatrix44f FromMatrix = RowVectorMatrix(From);
	if (FMath::Abs(FromMatrix.Determinant()) < UE_SMALL_NUMBER)
	{
		return;
	}
	// A view-space point back to the world, then into To's view.
	MoveFieldSceneObjects(Draw, FromMatrix.Inverse() * RowVectorMatrix(To));
}

namespace
{
	/** The camera cut rather than moved when its eye or target jumped this far, or its view turned further than this cosine's angle. */
	constexpr int32 CutDistance = 1024;
	constexpr float CutCosine = 0.866f;

	bool IsNear(const std::array<int32, 3>& A, const std::array<int32, 3>& B, int32 Limit)
	{
		return IsNearInWorld(A, B, Limit);
	}

	FVector3f Forward(const Weltall::FFieldView& View)
	{
		const auto& Row = View.View.M[2];
		return FVector3f(Row[0], Row[1], Row[2]).GetSafeNormal();
	}

	/** Whether two ticks' frames follow each other without a cut, so what moves between them can be drawn in between. */
	bool AreContinuous(const Weltall::FFieldFrame& Latest, const Weltall::FFieldFrame& Previous)
	{
		return Latest.Tick == Previous.Tick + 1 && Latest.bSceneDrawn && Previous.bSceneDrawn && IsNear(Latest.Eye, Previous.Eye, CutDistance)
			&& IsNear(Latest.Target, Previous.Target, CutDistance) && FVector3f::DotProduct(Forward(Latest.View), Forward(Previous.View)) >= CutCosine;
	}

	void AddBlendedObject(TArray<FVector4f>& Objects, const Weltall::FGteMatrix& From, const Weltall::FGteMatrix& To, float Alpha)
	{
		AddBlendedFieldObject(Objects, From, To, Alpha);
	}

	void AddObject(TArray<FVector4f>& Objects, const Weltall::FGteMatrix& Matrix)
	{
		AddBlendedObject(Objects, Matrix, Matrix, 1.0f);
	}

	int32 EntryOf(const Weltall::FFieldFrame& Frame, const Weltall::FFieldDrawItem& Item)
	{
		switch (Item.Kind)
		{
		case Weltall::EFieldDrawKind::Character:
			return Frame.Quads[Item.Index].Quad.OtIndex;
		case Weltall::EFieldDrawKind::Particle:
			return Frame.Particles[Item.Index].OtIndex;
		case Weltall::EFieldDrawKind::Backdrop:
			return Frame.Backdrop.Entry;
		case Weltall::EFieldDrawKind::Gear:
			return Item.Gear == Weltall::EFieldGearDraw::Cloth ? Frame.Gears.Cloths[Item.Index].Bucket : Frame.Gears.Polygons[Item.Index].Bucket;
		case Weltall::EFieldDrawKind::SpriteModel:
			return Frame.SpriteModels.Polygons[Item.Index].Bucket;
		default:
			return Frame.Scenery.Polygons[Item.Index].Bucket;
		}
	}

	/**
	 * The screen wave's grid as triangles: each cell's corners Alpha of the way from the tick before's (Then, when it
	 * has as many), showing its own 16 x 16 pixels. The cells are in rows of Columns, the first FirstColumn columns from the screen's left.
	 */
	void AddWaveGrid(std::span<const Weltall::FFieldScreenWaveCell> Cells, std::span<const Weltall::FFieldScreenWaveCell> Then, int32 Columns, int32 FirstColumn,
		float Alpha, TArray<FFieldWaveCorner>& Grid)
	{
		constexpr int32 Triangles[QuadCorners] = {0, 1, 2, 1, 3, 2};
		constexpr float Cell = Weltall::FFieldScreenWave::CellSize;
		const bool bBlends = Then.size() == Cells.size();
		for (int32 Index = 0; Index < static_cast<int32>(Cells.size()); ++Index)
		{
			const Weltall::FFieldScreenWaveCell& Now = Cells[static_cast<std::size_t>(Index)];
			const float Column = static_cast<float>(Index % Columns + FirstColumn) * Cell;
			const float Row = static_cast<float>(Index / Columns) * Cell;
			for (const int32 Corner : Triangles)
			{
				const std::array<int32, 2>& At = Now.Corners[static_cast<std::size_t>(Corner)];
				FVector2f Position(static_cast<float>(At[0]), static_cast<float>(At[1]));
				if (bBlends)
				{
					const std::array<int32, 2>& Was = Then[static_cast<std::size_t>(Index)].Corners[static_cast<std::size_t>(Corner)];
					Position = FMath::Lerp(FVector2f(static_cast<float>(Was[0]), static_cast<float>(Was[1])), Position, Alpha);
				}
				Grid.Add({Position, FVector2f(Column + ((Corner & 1) != 0 ? Cell : 0.0f), Row + ((Corner & 2) != 0 ? Cell : 0.0f))});
			}
		}
	}
}

bool BuildFieldFrameDraw(const Weltall::FFieldScene& Scene, const Weltall::FFieldFrame& Latest, const Weltall::FFieldFrame* Previous, float Alpha,
	const FFieldSceneMesh& Mesh, const FFieldRenderOptions& Options, FFieldSceneDraw& Draw, Weltall::FTexelKindCache* TexelKinds)
{
	Draw.Objects.Reset();
	Draw.FirstSpriteRow = MAX_int32;
	Draw.Colors.Reset();
	Draw.DynamicCorners.Reset();
	Draw.DynamicPolygons.Reset();
	Draw.SpriteCells.Reset();
	Draw.Order.Reset();
	Draw.Runs.Reset();
	Draw.WaveGrid.Reset();
	Draw.WideWaveGrid.Reset();
	Draw.bWave = false;
	Draw.WaveRuns = 0;
	Draw.ParticleCorners = 0;
	Draw.SpriteModelCorners.Reset();
	if (!Draw.Redirects.IsEmpty())
	{
		FMemory::Memset(Draw.Redirects.GetData(), 0xFF, Draw.Redirects.Num() * sizeof(uint32));
	}
	Draw.bGearsDrawn = false;
	Draw.PictureStretch.bStaged = false;
	const Weltall::FMdlColor& Background = Latest.Background;
	// The GPU clears the screen in 15-bit colour: a texel of the same colour meets the background exactly.
	Draw.Background = FColor(Weltall::FilledChannel(Background.R), Weltall::FilledChannel(Background.G), Weltall::FilledChannel(Background.B)).ReinterpretAsLinear();
	Draw.Filter = Options.Filter;
	Draw.SpriteFilter = Options.SpriteFilter;
	Draw.AntiAliasing = Options.AntiAliasing;
	Draw.bDepthBuffer = Options.Sorting != EFieldSorting::PaintersOrder;
	Draw.ProjectionDistance = Latest.View.ProjectionDistance;
	// The world's up is -y: the view's second column turned the other way.
	const Weltall::FGteMatrix& WorldToView = Latest.View.View;
	Draw.FloorUp = -FVector3f(WorldToView.M[0][1], WorldToView.M[1][1], WorldToView.M[2][1]).GetSafeNormal();
	const Weltall::FVramRect& Clip = Latest.DrawClip;
	Draw.bClipped = Clip.X != 0 || Clip.Y != 0 || Clip.Width != Weltall::FFieldScreenWave::ScreenWidth || Clip.Height != Weltall::FFieldScreenWave::ScreenHeight;
	Draw.Clip = FIntRect(Clip.X, Clip.Y, Clip.X + Clip.Width, Clip.Y + Clip.Height);
	if (!Latest.bSceneDrawn)
	{
		return false;
	}
	const Weltall::FFieldFrame* Before = Previous != nullptr && AreContinuous(Latest, *Previous) ? Previous : nullptr;
	if (Before != nullptr)
	{
		Draw.ProjectionDistance = FMath::Lerp(static_cast<float>(Before->View.ProjectionDistance), Draw.ProjectionDistance, Alpha);
	}
	const std::vector<Weltall::FFieldObjectView>& Objects = Latest.Scenery.Objects;
	const bool bObjectsPair = Before != nullptr && Before->Scenery.Objects.size() == Objects.size() && Before->SceneryWorld.size() == Objects.size();
	for (std::size_t Object = 0; Object < Objects.size(); ++Object)
	{
		if (bObjectsPair && IsNear(Before->SceneryWorld[Object], Latest.SceneryWorld[Object], FieldJumpDistance))
		{
			AddBlendedFieldObject(Draw.Objects, Before->Scenery.Objects[Object].Exact, Objects[Object].Exact, Alpha);
		}
		else
		{
			AddBlendedFieldObject(Draw.Objects, Objects[Object].Exact, Objects[Object].Exact, 1.0f);
		}
	}
	AddObject(Draw.Objects, Weltall::FGteMatrix::Identity());
	for (const Weltall::FFieldPolygonView& Polygon : Latest.Scenery.Polygons)
	{
		const auto Pack = [](const Weltall::FMdlColor& Color) { return static_cast<uint32>(Color.R | Color.G << 8 | Color.B << 16); };
		const uint32 Flags = Polygon.bFogged ? FieldColorFlags::Modulated : 0;
		Draw.Colors.Emplace(Pack(Polygon.Colors[0]) | Flags, Pack(Polygon.Colors[1]), Pack(Polygon.Colors[2]), Pack(Polygon.Colors[3]));
	}
	Draw.bGearsDrawn = DoGearsFitMesh(Latest, Mesh);
	AddGearColors(Latest, Mesh, Draw);
	AddGearObjects(Latest, Before, Alpha, Draw);
	AddFieldCharacterQuads(Latest, Before, Alpha, Mesh, Draw);
	AddFieldParticleQuads(Latest, Before, Alpha, Mesh, Draw);
	AddFieldSpriteModels(Latest, Before, Alpha, Mesh, Draw);
	AddFieldChangedModels(Scene, Latest, Before, Alpha, Mesh, Draw);
	AddFieldReleasedJoins(Scene, Latest, Before, Mesh, Draw);
	AddFieldBackdrop(Latest, Before, Alpha, Mesh, Draw);
	AddGearCloths(Latest, Before, Alpha, Mesh, Draw);
	AddFieldPictureStretch(Scene, Latest, Before, Alpha, Mesh, Draw);
	FFieldDrawBuilder Builder(Scene, Latest.Scenery, Latest.Characters, Latest.Quads, Latest.Particles, Mesh, Draw, &Latest.Gears, &Latest.SpriteModels, TexelKinds);
	std::span<const Weltall::FFieldDrawItem> Order = Latest.Order;
	std::size_t Waved = Order.size();
	if (Latest.bScreenWave)
	{
		// The wave redraws what the frame drew before its entry; the nearer entries are drawn over it.
		const Weltall::FFieldScene& Placed = Scene;
		Waved = static_cast<std::size_t>(std::find_if(Order.begin(), Order.end(), [&Latest, &Placed](const Weltall::FFieldDrawItem& Item)
		{
			const bool bBackground = Item.Kind == Weltall::EFieldDrawKind::Backdrop
				|| (Item.Kind == Weltall::EFieldDrawKind::Scenery && Placed.Objects[Placed.Polygons[Item.Index].Object].bBackground);
			return !bBackground && EntryOf(Latest, Item) <= Weltall::FieldWaveBucket;
		}) - Order.begin());
	}
	const bool bSceneryByDepth = Options.Sorting == EFieldSorting::DepthBufferWithScenery;
	const auto Build = [&Builder, &Draw, bSceneryByDepth](std::span<const Weltall::FFieldDrawItem> Items)
	{
		if (Draw.bDepthBuffer)
		{
			Builder.DepthBuffer(Items, bSceneryByDepth);
		}
		else
		{
			Builder.PaintersOrder(Items);
		}
	};
	Build(Order.first(Waved));
	if (Latest.bScreenWave)
	{
		Draw.bWave = true;
		Draw.WaveRuns = Draw.Runs.Num();
		const bool bBlends = Before != nullptr && Before->bScreenWave;
		AddWaveGrid(Latest.Wave, bBlends ? std::span<const Weltall::FFieldScreenWaveCell>(Before->Wave) : std::span<const Weltall::FFieldScreenWaveCell>(),
			Weltall::FFieldScreenWave::Columns, 0, Alpha, Draw.WaveGrid);
		AddWaveGrid(Latest.WideWave, bBlends ? std::span<const Weltall::FFieldScreenWaveCell>(Before->WideWave) : std::span<const Weltall::FFieldScreenWaveCell>(),
			Weltall::FFieldScreenWave::WideColumns, -Weltall::FFieldScreenWave::WideMargin, Alpha, Draw.WideWaveGrid);
		Build(Order.subspan(Waved));
	}
	return Before != nullptr;
}
