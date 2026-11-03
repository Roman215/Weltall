#include "WorldMap/WorldMapDrawing.h"

#include "Graphics/GteVectors.h"
#include "Graphics/Sprite.h"
#include "Graphics/SpriteDrawing.h"
#include "WorldMap/WorldMapDrawingTables.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 ScreenCenterX = 160;
		constexpr int32 ScreenWidth = 320;
		constexpr int32 ScreenHeight = 216;
		/** Where the fog is full, whatever its start. */
		constexpr int32 FogFar = 0xE80;
		/** The average depths' scales InitGeom sets: a third and a quarter, each a quarter again for the ordering table. */
		constexpr int16 AverageScale3 = 0x155;
		constexpr int16 AverageScale4 = 0x100;

		/** A block is 800h units across, its quadrants 400h, their cells 80h. */
		constexpr int32 BlockSide = 0x800;
		constexpr int32 QuadrantSide = 0x400;
		constexpr int32 CellSide = 0x80;
		constexpr int32 CellsAcross = 8;
		constexpr int32 VerticesAcross = CellsAcross + 1;
		/** The drawn blocks: 5 x 5 about the camera's, 9 x 9 at twice the reach. */
		constexpr int32 BlocksAcross = FWorldMapDrawingTables::DrawnBlocksAcross;
		constexpr int32 ExtendedBlocksAcross = 9;
		/** A quadrant's word: FFFFh leaves it out. */
		constexpr uint16 LeftOut = 0xFFFF;
		/** The frustum test's answers. */
		constexpr int32 Outside = -1;
		constexpr int32 Partly = 0;
		constexpr int32 Inside = 1;

		/** A triangle is drawn only nearer than this depth, and the program stops a frame's terrain after this many. */
		constexpr uint16 DrawDistance = 0xF00;
		constexpr int32 TriangleLimit = 0x7FE;
		constexpr int32 BucketShift = 4;
		/** The fog levels: a palette row per 80h of the depth cue, the last for 1000h. */
		constexpr int32 FogLevelShift = 7;
		constexpr int32 DepthCueMost = 0xFFF;
		constexpr int32 FogLevels = 32;

		/** A vertex word's fields. */
		constexpr uint32 WaterBit = 0x1000;
		constexpr uint32 DiagonalBit = 0x8000;
		constexpr uint32 PaletteBankBit = 0x800;

		/** What the terrain's load computes: the fog palettes' CLUTs (a row each from y 1B0h) and the seven 8-bit texture pages. */
		constexpr int32 FirstFogRow = 0x1B0;
		constexpr std::array<uint16, 8> TerrainPages{0x88, 0x8A, 0x8C, 0x8E, 0x96, 0x98, 0x9A, 0x88};

		/** The sky's colours, as the program's start sets its quads (the backdrop's pale bottom to the deep blue above). */
		constexpr FDrawColor SkyPale{224, 245, 255};
		constexpr FDrawColor SkyBlue{112, 122, 255};
		constexpr FDrawColor SkyDeep{69, 55, 192};
		constexpr std::array<std::array<FDrawColor, 4>, 4> SkyColors{{
			{SkyBlue, SkyBlue, SkyPale, SkyPale},
			{SkyDeep, SkyDeep, SkyBlue, SkyBlue},
			{SkyDeep, SkyDeep, SkyDeep, SkyDeep},
			{SkyDeep, SkyDeep, SkyDeep, SkyDeep},
		}};
		/** The horizon's band: its dim colour, its page (4-bit, x 380h y 100h, adding), CLUT (110h, 1FEh), and its 128 x 64 texels repeated. */
		constexpr FDrawColor HorizonColor{48, 48, 48};
		constexpr uint16 HorizonPage = 0x3E;
		constexpr uint16 HorizonClut = 0x7F91;
		constexpr uint8 HorizonWidth = 0x80;
		constexpr uint8 HorizonLastRow = 0x3F;
		/** The kernel's shift from a RotTransPers4 depth to an ordering-table entry. */
		constexpr int32 SkyBucketShift = 2;
		/**
		 * Where the cullings for wider views put the sky's quads, behind everything whatever the reach: their
		 * entries moved this far back (in their own order), a quad the GTE cannot project at the far end.
		 */
		constexpr uint16 SkyBehind = 0x1000;
		/**
		 * How far the cullings for wider views carry the sky across: its graded quads three times as wide (their colours
		 * change only downward), and the band's two halves repeated a pair to each side (each half one whole repeat of its
		 * texels), enough for 32:9 with the camera far behind its target.
		 */
		constexpr int16 SkyWidening = 3;
		constexpr int16 BandPairWidth = 8192;
		constexpr uint16 FarBucket = 0x3FF;

		constexpr uint32 RawCode = 0x1000000;
		constexpr uint32 SemiTransparentCode = 0x2000000;

		int32 Wrap(int32 Value, int32 Count)
		{
			const int32 Rest = Value % Count;
			return Rest < 0 ? Rest + Count : Rest;
		}

		/** A point on the ground plane (y 0), the frustum test's corner. */
		struct FGroundPoint
		{
			int32 X = 0;
			int32 Z = 0;
		};

		/**
		 * A quad against the frustum: per plane, the quad's edges (0-1, 1-3, 3-2, 2-0) with both corners
		 * outside. Outside when every edge of one plane is; inside when no edge of any plane is.
		 */
		int32 TestQuad(const FGteMatrix& Camera, const FWorldMapDrawingTables& Tables, const std::array<FGroundPoint, 4>& Corners)
		{
			std::array<FGteLongVector, 4> View{};
			for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
			{
				View[Corner] = RotTrans(Camera, {static_cast<int16>(Corners[Corner].X), 0, static_cast<int16>(Corners[Corner].Z)});
			}
			constexpr std::array<std::array<std::size_t, 2>, 4> Edges{{{0, 1}, {1, 3}, {3, 2}, {2, 0}}};
			bool bAnyEdge = false;
			for (const std::array<int32, 3>& Plane : Tables.FrustumPlanes)
			{
				std::array<bool, 4> Behind{};
				for (std::size_t Corner = 0; Corner < View.size(); ++Corner)
				{
					uint32 Sum = 0;
					for (std::size_t Axis = 0; Axis < 3; ++Axis)
					{
						Sum += static_cast<uint32>(View[Corner][Axis]) * static_cast<uint32>(Plane[Axis]);
					}
					Behind[Corner] = static_cast<int32>(Sum) < 0;
				}
				int32 EdgesBehind = 0;
				for (const std::array<std::size_t, 2>& Edge : Edges)
				{
					EdgesBehind += Behind[Edge[0]] && Behind[Edge[1]] ? 1 : 0;
				}
				if (EdgesBehind == static_cast<int32>(Edges.size()))
				{
					return Outside;
				}
				bAnyEdge = bAnyEdge || EdgesBehind != 0;
			}
			return bAnyEdge ? Partly : Inside;
		}

		/** The texels of a cell's corners (north-west, north-east, south-west, south-east) as its word's tile and flips give them, u | v << 8. */
		std::array<uint16, 4> CellTexels(uint32 Word)
		{
			const uint16 Base = static_cast<uint16>((Word >> 12 & 0xF0) | (Word >> 8 & 0xF000));
			constexpr uint16 Right = 0xF;
			constexpr uint16 Down = 0xF00;
			switch (Word >> 13 & 3)
			{
			case 0:
				return {Base, static_cast<uint16>(Base + Right), static_cast<uint16>(Base + Down), static_cast<uint16>(Base + Right + Down)};
			case 1:
				return {static_cast<uint16>(Base + Right), Base, static_cast<uint16>(Base + Right + Down), static_cast<uint16>(Base + Down)};
			case 2:
				return {static_cast<uint16>(Base + Down), static_cast<uint16>(Base + Right + Down), Base, static_cast<uint16>(Base + Right)};
			default:
				return {static_cast<uint16>(Base + Right + Down), static_cast<uint16>(Base + Down), static_cast<uint16>(Base + Right), Base};
			}
		}

		class FTerrainDrawing
		{
		public:
			FTerrainDrawing(const FWorldMapTerrain& InTerrain, const FWorldMapDrawState& InState, EWorldMapCulling InCulling, FWorldMapFrameBuilder& InBuilder)
				: Terrain(InTerrain), State(InState), Culling(InCulling), Builder(InBuilder), Gte(WorldMapDrawingGte(InState, WorldMapReach(InCulling))),
				  Reach(static_cast<uint16>(DrawDistance * WorldMapReach(InCulling) + InState.EyeBack))
			{
			}

			/** A quadrant's vertices built and drawn: the quadrant's corner at (X, Z), z growing north. */
			void DrawQuadrant(int32 Block, int32 Quadrant, int32 X, int32 Z)
			{
				std::array<FGteVector, VerticesAcross * VerticesAcross> Vertices{};
				std::array<FGteProjection, VerticesAcross * VerticesAcross> Projected{};
				std::array<bool, VerticesAcross * VerticesAcross> bProjected{};
				for (int32 Row = 0; Row < VerticesAcross; ++Row)
				{
					const int32 Down = GteSin(State.WaveRowPhase + Row * 0x200);
					for (int32 Column = 0; Column < VerticesAcross; ++Column)
					{
						const uint32 Word = Terrain.VertexWord(Block, Quadrant, Column, Row);
						int32 Height = static_cast<int8>(Word & 0xFF) * 8;
						if ((Word & WaterBit) != 0)
						{
							Height += GteSin(State.WavePhase + Column * 0x200) * Down * 2 >> 20;
						}
						Vertices[static_cast<std::size_t>(Row * VerticesAcross + Column)] = {static_cast<int16>(X + Column * CellSide), static_cast<int16>(Height),
							static_cast<int16>(Z - Row * CellSide)};
					}
				}
				const auto Project = [&](int32 Index) -> const FGteProjection& {
					const std::size_t At = static_cast<std::size_t>(Index);
					if (!bProjected[At])
					{
						Projected[At] = Gte.Rtps(Vertices[At]);
						bProjected[At] = true;
					}
					return Projected[At];
				};
				for (int32 Row = 0; Row < CellsAcross; ++Row)
				{
					for (int32 Column = 0; Column < CellsAcross; ++Column)
					{
						if (Culling == EWorldMapCulling::Original && Triangles >= TriangleLimit)
						{
							return;
						}
						const uint32 Word = Terrain.VertexWord(Block, Quadrant, Column, Row);
						const std::array<uint16, 4> Texels = CellTexels(Word);
						const int32 NorthWest = Row * VerticesAcross + Column;
						const int32 NorthEast = NorthWest + 1;
						const int32 SouthWest = NorthWest + VerticesAcross;
						const int32 SouthEast = SouthWest + 1;
						const bool bDiagonal = (Word & DiagonalBit) != 0;
						// The first triangle: north-west, then the south-east corner across the diagonal or the north-east, then south-west.
						const std::array<int32, 3> First{NorthWest, bDiagonal ? SouthEast : NorthEast, SouthWest};
						const std::array<uint16, 3> FirstTexels{Texels[0], bDiagonal ? Texels[3] : Texels[1], Texels[2]};
						const std::array<int32, 3> Second{NorthEast, SouthEast, bDiagonal ? NorthWest : SouthWest};
						const std::array<uint16, 3> SecondTexels{Texels[1], Texels[3], bDiagonal ? Texels[0] : Texels[2]};
						const std::array<FGteProjection, 3> A{Project(First[0]), Project(First[1]), Project(First[2])};
						// A fault projecting the first triangle drops the cell's second as well.
						if (Culling == EWorldMapCulling::Original && ((A[0].Flag | A[1].Flag | A[2].Flag) & GteFlag::Error) != 0)
						{
							continue;
						}
						AddTriangle(Word, A, First, FirstTexels, Vertices);
						const std::array<FGteProjection, 3> B{Project(Second[0]), Project(Second[1]), Project(Second[2])};
						if (Culling == EWorldMapCulling::Original && ((B[0].Flag | B[1].Flag | B[2].Flag) & GteFlag::Error) != 0)
						{
							continue;
						}
						AddTriangle(Word, B, Second, SecondTexels, Vertices);
					}
				}
			}

		private:
			void AddTriangle(uint32 Word, const std::array<FGteProjection, 3>& Corners, const std::array<int32, 3>& Indices, const std::array<uint16, 3>& Texels,
				const std::array<FGteVector, VerticesAcross * VerticesAcross>& Vertices)
			{
				if (Culling == EWorldMapCulling::Original)
				{
					bool bAcross = false;
					bool bDown = false;
					for (const FGteProjection& Corner : Corners)
					{
						bAcross = bAcross || static_cast<uint16>(Corner.ScreenX) < ScreenWidth;
						bDown = bDown || static_cast<uint16>(Corner.ScreenY) < ScreenHeight;
					}
					if (!bAcross || !bDown)
					{
						return;
					}
				}
				const uint16 Farthest = std::max({Corners[0].ScreenZ, Corners[1].ScreenZ, Corners[2].ScreenZ});
				if (Farthest >= Reach)
				{
					return;
				}
				// The original drops a triangle that faces away on its screen. The other cullings leave that to the display: told
				// from the GTE's whole units, a triangle barely facing the eye at a crest would be dropped, and what lies behind
				// the ground would show through a hairline.
				if (Culling == EWorldMapCulling::Original
					&& Nclip(Corners[0].ScreenX, Corners[0].ScreenY, Corners[1].ScreenX, Corners[1].ScreenY, Corners[2].ScreenX, Corners[2].ScreenY) <= 0)
				{
					return;
				}
				// The fog's level from the depth cue the GTE left for the last corner.
				const int32 DepthCue = static_cast<uint16>(Corners[2].DepthCue) < 0x1000 ? static_cast<uint16>(Corners[2].DepthCue) : DepthCueMost;
				const int32 Level = (DepthCue >> FogLevelShift) + ((Word & PaletteBankBit) != 0 ? FogLevels : 0);
				FWorldMapPolygon& Polygon = Builder.Add(static_cast<uint16>(WorldMapSortDepth(Farthest, State) >> BucketShift));
				Polygon.Layer = EWorldMapLayer::Terrain;
				Polygon.bOneSided = Culling != EWorldMapCulling::Original;
				Polygon.CornerCount = 3;
				Polygon.bTextured = true;
				Polygon.TexturePage = TerrainPages[Word >> 8 & 7];
				Polygon.Clut = static_cast<uint16>((FirstFogRow + Level) << 6);
				uint8 LowU = 0xFF;
				uint8 LowV = 0xFF;
				uint8 HighU = 0;
				uint8 HighV = 0;
				for (std::size_t Corner = 0; Corner < 3; ++Corner)
				{
					Polygon.View[Corner] = ViewPoint(Gte.Transform, Vertices[static_cast<std::size_t>(Indices[Corner])]);
					Polygon.ScreenX[Corner] = Corners[Corner].ScreenX;
					Polygon.ScreenY[Corner] = Corners[Corner].ScreenY;
					Polygon.Colors[Corner] = FDrawColor{};
					Polygon.U[Corner] = static_cast<uint8>(Texels[Corner]);
					Polygon.V[Corner] = static_cast<uint8>(Texels[Corner] >> 8);
					Polygon.TexCoords[Corner] = {static_cast<float>(Polygon.U[Corner]), static_cast<float>(Polygon.V[Corner])};
					LowU = std::min(LowU, Polygon.U[Corner]);
					LowV = std::min(LowV, Polygon.V[Corner]);
					HighU = std::max(HighU, Polygon.U[Corner]);
					HighV = std::max(HighV, Polygon.V[Corner]);
				}
				Polygon.TexelMin = {LowU, LowV};
				Polygon.TexelMax = {HighU, HighV};
				++Triangles;
			}

			const FWorldMapTerrain& Terrain;
			const FWorldMapDrawState& State;
			EWorldMapCulling Culling;
			FWorldMapFrameBuilder& Builder;
			FGte Gte;
			/** The depth a triangle's farthest corner must be nearer than. */
			uint16 Reach = DrawDistance;
			int32 Triangles = 0;
		};

		/** A quad through the GTE as RotTransPers4 takes it: the four corners' projections, the flag of all four, and the last corner's depth / 4. */
		struct FProjectedQuad
		{
			std::array<FGteProjection, 4> Corners{};
			uint32 Flag = 0;
			int32 Depth = 0;
		};

		FProjectedQuad ProjectQuad(const FGte& Gte, std::span<const FGteVector> Corners)
		{
			FProjectedQuad Quad;
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				Quad.Corners[Corner] = Gte.Rtps(Corners[Corner]);
				Quad.Flag |= Quad.Corners[Corner].Flag;
			}
			Quad.Depth = Quad.Corners[3].ScreenZ >> 2;
			return Quad;
		}

		/** The sky's entry for a quad: its last corner's depth / 4, as sorted (WorldMapSortDepth), in the sky's coarser entries. */
		uint16 SkyBucket(const FProjectedQuad& Quad, const FWorldMapDrawState& State)
		{
			return static_cast<uint16>((WorldMapSortDepth(Quad.Corners[3].ScreenZ, State) >> 2) >> SkyBucketShift);
		}

		void SetQuadCorners(FWorldMapPolygon& Polygon, const FGteMatrix& Matrix, std::span<const FGteVector> Corners, const FProjectedQuad& Quad)
		{
			Polygon.CornerCount = 4;
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				Polygon.View[Corner] = ViewPoint(Matrix, Corners[Corner]);
				Polygon.ScreenX[Corner] = Quad.Corners[Corner].ScreenX;
				Polygon.ScreenY[Corner] = Quad.Corners[Corner].ScreenY;
			}
		}
	}

	void FWorldMapFrameBuilder::Begin(FWorldMapFrame& InFrame)
	{
		Frame = &InFrame;
		Linked.clear();
		Anchors.clear();
	}

	void FWorldMapFrameBuilder::Anchor(EWorldMapAnchorKind Kind, uint32 Id, const FGteMatrix& Matrix, const std::array<float, 3>& Fraction)
	{
		Anchors.push_back({FWorldMapAnchor::MakeKey(Kind, Id), Matrix, Fraction});
	}

	void FWorldMapFrameBuilder::AnchorFine(EWorldMapAnchorKind Kind, uint32 Id, const FGteMatrix& Matrix, const FAffine& Fine)
	{
		FWorldMapAnchor& Made = Anchors.emplace_back();
		Made.Key = FWorldMapAnchor::MakeKey(Kind, Id);
		Made.Matrix = Matrix;
		Made.bFine = true;
		Made.Fine = Fine;
	}

	void FWorldMapFrameBuilder::LinkNearestFirst(std::size_t First)
	{
		// Each has an anchor of its own, later for one added later: the order two of one depth were added in.
		std::sort(Linked.begin() + static_cast<std::ptrdiff_t>(std::min(First, Linked.size())), Linked.end(),
			[](const FWorldMapPolygon& A, const FWorldMapPolygon& B)
			{
				return A.View[0][2] != B.View[0][2] ? A.View[0][2] < B.View[0][2] : A.Anchor < B.Anchor;
			});
	}

	FWorldMapPolygon& FWorldMapFrameBuilder::Add(uint16 Bucket)
	{
		FWorldMapPolygon& Polygon = Linked.emplace_back();
		Polygon.Bucket = Bucket;
		Polygon.Anchor = Anchors.empty() || Anchors.size() > FWorldMapPolygon::NoAnchor ? FWorldMapPolygon::NoAnchor : static_cast<uint16>(Anchors.size() - 1);
		return Polygon;
	}

	void FWorldMapFrameBuilder::Finish()
	{
		// A counting sort by entry, the far end first; each new polygon is linked at its entry's head, so within one the last is drawn first.
		uint16 Highest = 0;
		for (const FWorldMapPolygon& Polygon : Linked)
		{
			Highest = std::max(Highest, Polygon.Bucket);
		}
		Starts.assign(static_cast<std::size_t>(Highest) + 2, 0);
		for (const FWorldMapPolygon& Polygon : Linked)
		{
			++Starts[static_cast<std::size_t>(Highest - Polygon.Bucket) + 1];
		}
		for (std::size_t Entry = 1; Entry < Starts.size(); ++Entry)
		{
			Starts[Entry] += Starts[Entry - 1];
		}
		Frame->Polygons.resize(Linked.size());
		for (std::size_t Index = Linked.size(); Index-- > 0;)
		{
			const FWorldMapPolygon& Polygon = Linked[Index];
			Frame->Polygons[Starts[static_cast<std::size_t>(Highest - Polygon.Bucket)]++] = Polygon;
		}
		Frame->Anchors.assign(Anchors.begin(), Anchors.end());
	}

	int32 WorldMapReach(EWorldMapCulling Culling)
	{
		return Culling == EWorldMapCulling::Extended ? 2 : 1;
	}

	int32 WorldMapBlocksAcross(EWorldMapCulling Culling)
	{
		return Culling == EWorldMapCulling::Extended ? ExtendedBlocksAcross : BlocksAcross;
	}

	FGte WorldMapDrawingGte(const FWorldMapDrawState& State, int32 Reach)
	{
		FGte Gte;
		Gte.Transform = State.Camera;
		Gte.OffsetX = ScreenCenterX << 16;
		Gte.OffsetY = State.ScreenCenterY << 16;
		Gte.ProjectionDistance = static_cast<uint16>(State.ProjectionDistance);
		Gte.AverageScale3 = AverageScale3;
		Gte.AverageScale4 = AverageScale4;
		Gte.SetFogNearFar(State.FogNear * Reach + State.EyeBack, FogFar * Reach + State.EyeBack, State.ProjectionDistance);
		return Gte;
	}

	void DrawWorldMapTerrain(const FWorldMapTerrain& Terrain, const FWorldMapDrawingTables& Tables, const FWorldMapDrawState& State, EWorldMapCulling Culling,
		FWorldMapFrameBuilder& Builder, std::vector<int32>* BlockStatesOut)
	{
		// The drawn square starts two blocks (four at twice the reach) west and north of the camera's block; z grows north here.
		const int32 Across = WorldMapBlocksAcross(Culling);
		const int32 Before = Across / 2;
		const int32 WithinX = State.Target.X >> 12 & 0x7FF;
		const int32 WithinZ = State.Target.Z >> 12 & 0x7FF;
		const int32 FirstX = -Before * BlockSide - WithinX;
		const int32 FirstZ = Before * BlockSide + WithinZ;
		const int32 Quarter = (WithinX < QuadrantSide ? 0 : 1) | (WithinZ < QuadrantSide ? 0 : 2);
		std::vector<int32> BlockStates(static_cast<std::size_t>(Across * Across), Inside);
		std::vector<std::array<uint16, 4>> QuadrantStates(static_cast<std::size_t>(Across * Across), std::array<uint16, 4>{Inside, Inside, Inside, Inside});
		if (Culling != EWorldMapCulling::Extended)
		{
			for (int32 Row = 0; Row < Across; ++Row)
			{
				for (int32 Column = 0; Column < Across; ++Column)
				{
					const std::size_t Block = static_cast<std::size_t>(Row * Across + Column);
					const int32 X = FirstX + Column * BlockSide;
					const int32 Z = FirstZ - Row * BlockSide;
					std::array<uint16, 4>& Quadrants = QuadrantStates[Block];
					if (Culling == EWorldMapCulling::Original)
					{
						BlockStates[Block] = TestQuad(State.Camera, Tables, {{{X, Z}, {X + BlockSide, Z}, {X, Z - BlockSide}, {X + BlockSide, Z - BlockSide}}});
						if (BlockStates[Block] == Partly)
						{
							const int32 MidX = X + QuadrantSide;
							const int32 MidZ = Z - QuadrantSide;
							Quadrants = {static_cast<uint16>(TestQuad(State.Camera, Tables, {{{X, Z}, {MidX, Z}, {X, MidZ}, {MidX, MidZ}}})),
								static_cast<uint16>(TestQuad(State.Camera, Tables, {{{MidX, Z}, {X + BlockSide, Z}, {MidX, MidZ}, {X + BlockSide, MidZ}}})),
								static_cast<uint16>(TestQuad(State.Camera, Tables, {{{X, MidZ}, {MidX, MidZ}, {X, Z - BlockSide}, {MidX, Z - BlockSide}}})),
								static_cast<uint16>(TestQuad(State.Camera, Tables, {{{MidX, MidZ}, {X + BlockSide, MidZ}, {MidX, Z - BlockSide}, {X + BlockSide, Z - BlockSide}}}))};
						}
						else
						{
							Quadrants.fill(static_cast<uint16>(BlockStates[Block]));
						}
					}
					// The camera's quarter of its block leaves out the far row and column of quadrants.
					for (std::size_t Quadrant = 0; Quadrant < 4; ++Quadrant)
					{
						Quadrants[Quadrant] |= Tables.QuadrantMasks[static_cast<std::size_t>(Quarter)][Block][Quadrant];
					}
				}
			}
		}

		if (BlockStatesOut != nullptr)
		{
			*BlockStatesOut = BlockStates;
		}
		Builder.Anchor(EWorldMapAnchorKind::World, 0, State.Camera);
		FTerrainDrawing Drawing(Terrain, State, Culling, Builder);
		const int32 CameraBlockX = State.Target.X >> 23;
		const int32 CameraBlockZ = State.Target.Z >> 23;
		for (int32 Row = 0; Row < Across; ++Row)
		{
			for (int32 Column = 0; Column < Across; ++Column)
			{
				const std::size_t Block = static_cast<std::size_t>(Row * Across + Column);
				if (BlockStates[Block] == Outside)
				{
					continue;
				}
				const int32 Number = Wrap(CameraBlockZ - Before + Row, Terrain.BlocksZ()) * Terrain.BlocksX() + Wrap(CameraBlockX - Before + Column, Terrain.BlocksX());
				const std::array<uint16, 4>& Quadrants = QuadrantStates[Block];
				// With any quadrant left out, only the others; otherwise all four, whatever the tests said of them.
				const bool bSome = (Quadrants[0] | Quadrants[1] | Quadrants[2] | Quadrants[3]) == LeftOut;
				const int32 X = FirstX + Column * BlockSide;
				const int32 Z = FirstZ - Row * BlockSide;
				for (int32 Quadrant = 0; Quadrant < 4; ++Quadrant)
				{
					if (bSome && Quadrants[static_cast<std::size_t>(Quadrant)] == LeftOut)
					{
						continue;
					}
					Drawing.DrawQuadrant(Number, Quadrant, X + ((Quadrant & 1) != 0 ? QuadrantSide : 0), Z - ((Quadrant & 2) != 0 ? QuadrantSide : 0));
				}
			}
		}
	}

	std::optional<uint16> WorldMapHorizonBucket(const FWorldMapDrawingTables& Tables, const FWorldMapDrawState& State)
	{
		FGte Gte = WorldMapDrawingGte(State);
		Gte.Transform = CompMatrix(State.Camera, RotationYxz({0, State.Yaw, 0}));
		const FProjectedQuad Right = ProjectQuad(Gte, std::span<const FGteVector>(Tables.HorizonCorners).subspan(4, 4));
		if ((Right.Flag & GteFlag::Error) != 0)
		{
			return std::nullopt;
		}
		return SkyBucket(Right, State);
	}

	void DrawWorldMapSky(const FWorldMapDrawingTables& Tables, const FWorldMapDrawState& State, EWorldMapCulling Culling, FWorldMapFrameBuilder& Builder)
	{
		// The backdrop turns with the camera's pitch and roll but not its yaw: the camera's matrix after a turn by the yaw.
		FGte Gte = WorldMapDrawingGte(State);
		Gte.Transform = CompMatrix(State.Camera, RotationYxz({0, State.Yaw, 0}));
		Builder.Anchor(EWorldMapAnchorKind::Sky, 0, Gte.Transform);

		// The band: two quads with the same texels, their u scrolled by the yaw within the 128-texel window, linked by the second's depth.
		const std::span<const FGteVector> Band(Tables.HorizonCorners);
		const FProjectedQuad Left = ProjectQuad(Gte, Band.subspan(0, 4));
		const FProjectedQuad Right = ProjectQuad(Gte, Band.subspan(4, 4));
		const bool bBandShown = (Right.Flag & GteFlag::Error) == 0;
		const uint16 Behind = Culling == EWorldMapCulling::Original ? 0 : SkyBehind;
		if (bBandShown || Culling != EWorldMapCulling::Original)
		{
			const uint16 Bucket = static_cast<uint16>((bBandShown ? SkyBucket(Right, State) : FarBucket) + Behind);
			const uint8 Scroll = static_cast<uint8>(State.Yaw >> 2 & 0x7F);
			const int32 Pairs = Culling == EWorldMapCulling::Original ? 0 : (SkyWidening - 1) / 2;
			for (int32 Copy = 0; Copy < 2 * (2 * Pairs + 1); ++Copy)
			{
				const int32 Half = Copy & 1;
				const int32 Pair = Copy / 2 - Pairs;
				std::array<FGteVector, 4> Corners{};
				for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
				{
					Corners[Corner] = Band[static_cast<std::size_t>(Half) * 4 + Corner];
					Corners[Corner][0] = static_cast<int16>(Corners[Corner][0] + Pair * BandPairWidth);
				}
				FWorldMapPolygon& Polygon = Builder.Add(Bucket);
				Polygon.Layer = EWorldMapLayer::Horizon;
				SetQuadCorners(Polygon, Gte.Transform, Corners, Pair != 0 ? ProjectQuad(Gte, Corners) : Half == 0 ? Left : Right);
				Polygon.bTextured = true;
				Polygon.bSemiTransparent = true;
				Polygon.TexturePage = HorizonPage;
				Polygon.Clut = HorizonClut;
				Polygon.WindowWidth = HorizonWidth;
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					const bool bFar = (Corner & 1) != 0;
					const bool bLow = (Corner & 2) != 0;
					Polygon.Colors[Corner] = HorizonColor;
					Polygon.U[Corner] = static_cast<uint8>(bFar ? Scroll | HorizonWidth : Scroll);
					Polygon.V[Corner] = bLow ? HorizonLastRow : 0;
					Polygon.TexCoords[Corner] = {static_cast<float>(Scroll + (bFar ? HorizonWidth : 0)), static_cast<float>(Polygon.V[Corner])};
				}
				Polygon.TexelMin = {0, 0};
				Polygon.TexelMax = {static_cast<uint8>(HorizonWidth - 1), HorizonLastRow};
			}
		}

		for (std::size_t Quad = 0; Quad < SkyColors.size(); ++Quad)
		{
			std::array<FGteVector, 4> Corners{};
			for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
			{
				Corners[Corner] = Tables.SkyCorners[Quad * 4 + Corner];
				if (Culling != EWorldMapCulling::Original)
				{
					Corners[Corner][0] = static_cast<int16>(Corners[Corner][0] * SkyWidening);
				}
			}
			const FProjectedQuad Projected = ProjectQuad(Gte, Corners);
			const bool bShown = (Projected.Flag & GteFlag::Error) == 0;
			if (!bShown && Culling == EWorldMapCulling::Original)
			{
				continue;
			}
			FWorldMapPolygon& Polygon = Builder.Add(static_cast<uint16>((bShown ? SkyBucket(Projected, State) : FarBucket) + Behind));
			Polygon.Layer = EWorldMapLayer::Sky;
			SetQuadCorners(Polygon, Gte.Transform, Corners, Projected);
			Polygon.bGouraud = true;
			Polygon.Colors = SkyColors[Quad];
		}
	}

	FGteMatrix WorldMapSpriteRotation(const FSprite& Sprite)
	{
		FGteMatrix Rotation = ScaleRows(RotationXyz({Sprite.Rotation[0], Sprite.Rotation[1], Sprite.Rotation[2]}),
			{Sprite.TransformScale[0], Sprite.TransformScale[1], Sprite.TransformScale[2]});
		if (Sprite.SizeFactor != 0)
		{
			const int32 Factor = Sprite.SizeFactor >> 1;
			Rotation = ScaleColumns(Rotation, {Factor, Factor, Factor});
		}
		return Rotation;
	}

	void DrawWorldMapSprite(const FSprite& Sprite, int32 Slot, const FWorldMapDrawState& State, int32 OtIndex, FWorldMapFrameBuilder& Builder)
	{
		std::vector<FSpriteQuad> Quads;
		const FGteMatrix Placed = SpriteDrawMatrix(Sprite, WorldMapSpriteRotation(Sprite), State.Camera);
		DrawSpriteParts(Sprite, Placed, OtIndex, &Quads);
		Builder.Anchor(EWorldMapAnchorKind::Sprite, static_cast<uint32>(Slot), Placed);
		FGte Gte = WorldMapDrawingGte(State);
		for (const FSpriteQuad& Quad : Quads)
		{
			Gte.Transform = Quad.Matrix;
			std::array<FGteProjection, 4> Screen{};
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				Screen[Corner] = Gte.Rtps(Quad.Corners[Corner]);
			}
			FWorldMapPolygon& Polygon = Builder.Add(static_cast<uint16>(Quad.OtIndex));
			Polygon.Layer = EWorldMapLayer::Sprite;
			Polygon.CornerCount = 4;
			Polygon.bTextured = true;
			Polygon.bRaw = (Quad.ColorCode & RawCode) != 0;
			Polygon.bSemiTransparent = (Quad.ColorCode & SemiTransparentCode) != 0;
			Polygon.TexturePage = Quad.TexturePage;
			Polygon.Clut = Quad.Clut;
			// A sprite's parts may lie in VRAM's port rows (a member drawn as Id).
			Polygon.bPortRows = true;
			// The packet ends at the last texel; a part mirrored on screen starts a column lower, one at the page's first column a column narrower.
			int32 First = Quad.U0;
			int32 Across = Quad.U1 - Quad.U0 - 1;
			if (Screen[3].ScreenX < Screen[0].ScreenX && --First < 0)
			{
				First = 0;
				--Across;
			}
			const FDrawColor Color{static_cast<uint8>(Quad.ColorCode), static_cast<uint8>(Quad.ColorCode >> 8), static_cast<uint8>(Quad.ColorCode >> 16)};
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				const bool bFar = (Corner & 1) != 0;
				const bool bLow = (Corner & 2) != 0;
				const std::array<float, 3> View = Quad.ViewCorner(Corner);
				Polygon.View[Corner] = View;
				Polygon.ScreenX[Corner] = Screen[Corner].ScreenX;
				Polygon.ScreenY[Corner] = Screen[Corner].ScreenY;
				Polygon.Colors[Corner] = Color;
				Polygon.U[Corner] = static_cast<uint8>(bFar ? First + Across : First);
				Polygon.V[Corner] = static_cast<uint8>(bLow ? Quad.V1 - 1 : Quad.V0);
				Polygon.TexCoords[Corner] = {static_cast<float>(bFar ? Quad.U1 : Quad.U0), static_cast<float>(bLow ? Quad.V1 : Quad.V0)};
			}
			Polygon.TexelMin = {static_cast<uint8>(std::min(Quad.U0, Quad.U1)), static_cast<uint8>(std::min(Quad.V0, Quad.V1))};
			Polygon.TexelMax = {static_cast<uint8>(std::max(Quad.U0, Quad.U1) - 1), static_cast<uint8>(std::max(Quad.V0, Quad.V1) - 1)};
		}
	}

	void MakeWorldMapFogPalettes(std::span<const uint16> Palette, int32 Levels, const std::array<uint8, 3>& Fog, std::span<uint16> Palettes)
	{
		constexpr std::size_t Colors = 256;
		for (int32 Level = 0; Level < Levels; ++Level)
		{
			const int32 Toward = Level * 0x1000 / Levels;
			const int32 Kept = 0x1000 - Toward;
			for (std::size_t Index = 0; Index < Colors; ++Index)
			{
				const uint16 Color = Palette[Index];
				uint16& Out = Palettes[static_cast<std::size_t>(Level) * Colors + Index];
				if (Color == 0)
				{
					Out = 0;
					continue;
				}
				const auto Channel = [&](int32 Eight, uint8 FogChannel) { return static_cast<uint16>((Eight * Kept + FogChannel * Toward) >> 15); };
				Out = static_cast<uint16>((Color & 0x8000) | Channel((Color & 0x1F) * 8, Fog[0]) | Channel((Color >> 2 & 0xF8), Fog[1]) << 5
					| Channel((Color >> 7 & 0xF8), Fog[2]) << 10);
			}
		}
	}
}
