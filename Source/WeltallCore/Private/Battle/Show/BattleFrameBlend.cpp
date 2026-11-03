#include "Battle/Show/BattleFrameBlend.h"

#include "Battle/Show/BattleObjectsFrame.h"
#include "Field/FieldBackdrop.h"
#include "Graphics/SpriteDrawing.h"
#include "Graphics/Texture.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>

namespace Weltall
{
	namespace
	{
		/** The camera cut rather than moved when its eye or target jumped this far, or its view turned further than this cosine's angle. */
		constexpr int32 CutDistance = 1024;
		constexpr float CutCosine = 0.866f;
		/** An anchor or a point jumped rather than moved this far along an axis between frames. */
		constexpr float JumpDistance = 512.0f;
		/** The kinds of the objects' anchors, past the drawers' kinds in the key's top byte. */
		constexpr uint64 PartAnchor = 0xFE;
		constexpr uint64 ShadowAnchor = 0xFF;

		float Lerp(float From, float To, float Alpha)
		{
			return From + (To - From) * Alpha;
		}

		uint64 AnchorKey(uint64 Kind, uint64 Key)
		{
			return Kind << 56 | (Key & 0xFFFFFFFFFFull) << 16;
		}

		/** Where a matrix's origin lies in the world, through the inverse of the view it was drawn under. */
		std::array<float, 3> WorldOrigin(const FAffine& ViewInverse, const FGteMatrix& Matrix)
		{
			const std::array<float, 3> Turned = ViewInverse.Turn({static_cast<float>(Matrix.T[0]), static_cast<float>(Matrix.T[1]), static_cast<float>(Matrix.T[2])});
			return {Turned[0] + ViewInverse.T[0], Turned[1] + ViewInverse.T[1], Turned[2] + ViewInverse.T[2]};
		}

		/** The view turned as it is, its origin at Place: a drawer's own points about its place in the world. */
		FAffine PlacedView(const FGteMatrix& View, const std::array<int16, 3>& Place)
		{
			FAffine Matrix = FAffine::From(View);
			const std::array<float, 3> Origin = ViewPoint(View, Place);
			Matrix.T = Origin;
			return Matrix;
		}

		/** Numbers the anchors that share a key in their order, so that each pairs with the same one a frame later. */
		void CountSharedKeys(std::vector<FBattleFrameAnchor>& Anchors, std::vector<std::pair<uint64, uint32>>& Scratch)
		{
			Scratch.clear();
			for (std::size_t Index = 0; Index < Anchors.size(); ++Index)
			{
				Scratch.emplace_back(Anchors[Index].Key, static_cast<uint32>(Index));
			}
			std::sort(Scratch.begin(), Scratch.end());
			for (std::size_t At = 1; At < Scratch.size(); ++At)
			{
				if ((Scratch[At].first & ~0xFFFFull) == (Scratch[At - 1].first & ~0xFFFFull))
				{
					Scratch[At].first = Scratch[At - 1].first + 1;
					Anchors[Scratch[At].second].Key = Scratch[At].first;
				}
			}
		}

		bool IsNear(const std::array<int16, 3>& A, const std::array<int16, 3>& B)
		{
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				if (std::abs(A[Axis] - B[Axis]) > CutDistance)
				{
					return false;
				}
			}
			return true;
		}

		bool HasJumped(const std::array<float, 3>& A, const std::array<float, 3>& B)
		{
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				if (std::fabs(A[Axis] - B[Axis]) > JumpDistance)
				{
					return true;
				}
			}
			return false;
		}

		bool AreContinuous(const FBattleFrameKeep& Latest, const FBattleFrameKeep& Previous)
		{
			if (Latest.Step != Previous.Step + 1 || !IsNear(Latest.Eye, Previous.Eye) || !IsNear(Latest.Target, Previous.Target))
			{
				return false;
			}
			const std::array<int16, 3>& Now = Latest.View.M[2];
			const std::array<int16, 3>& Then = Previous.View.M[2];
			const float NowLength = std::sqrt(static_cast<float>(Now[0] * Now[0] + Now[1] * Now[1] + Now[2] * Now[2]));
			const float ThenLength = std::sqrt(static_cast<float>(Then[0] * Then[0] + Then[1] * Then[1] + Then[2] * Then[2]));
			if (NowLength <= 0.0f || ThenLength <= 0.0f)
			{
				return false;
			}
			const float Cosine = static_cast<float>(Now[0] * Then[0] + Now[1] * Then[1] + Now[2] * Then[2]) / (NowLength * ThenLength);
			return Cosine >= CutCosine;
		}

		void BlendBackdrop(const FBattleBackdropFrame& Before, const FBattleBackdropFrame& Latest, float Alpha, FBattleFrameBlend& Blend)
		{
			for (std::size_t Slot = 0; Slot < Latest.Panoramas.size(); ++Slot)
			{
				Blend.Backdrop.Panoramas[Slot] = BlendFieldBackdrop(Before.Panoramas[Slot], Latest.Panoramas[Slot], Alpha);
			}
			if (Before.Dome.size() != Latest.Dome.size())
			{
				return;
			}
			for (std::size_t Index = 0; Index < Latest.Dome.size(); ++Index)
			{
				const FBattleDomePacket& Now = Latest.Dome[Index];
				const FBattleDomePacket& Then = Before.Dome[Index];
				if (Now.bDrawMode || Then.bDrawMode || Now.Quad.Kind != Then.Quad.Kind)
				{
					continue;
				}
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					for (std::size_t Axis = 0; Axis < 2; ++Axis)
					{
						Blend.DomeCorners[Index][Corner][Axis] = Lerp(Then.Quad.Corners[Corner][Axis], Now.Quad.Corners[Corner][Axis], Alpha);
					}
				}
			}
		}

		void BlendCloths(const FBattleFrameKeep& Latest, const FBattleFrameKeep& Previous, float Alpha, FBattleFrameBlend& Blend,
			std::vector<std::pair<uint32, FGteVector>>& Points)
		{
			Points.clear();
			for (const FBattleKeptCloth& Cloth : Previous.Cloths)
			{
				for (std::size_t Corner = 0; Corner < 3; ++Corner)
				{
					Points.emplace_back(Cloth.PointKeys[Corner], Cloth.Points[Corner]);
				}
			}
			std::sort(Points.begin(), Points.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
			for (std::size_t Index = 0; Index < Latest.Cloths.size(); ++Index)
			{
				const FBattleKeptCloth& Cloth = Latest.Cloths[Index];
				for (std::size_t Corner = 0; Corner < 3; ++Corner)
				{
					const uint32 Key = Cloth.PointKeys[Corner];
					const auto Found = std::lower_bound(Points.begin(), Points.end(), Key, [](const auto& Entry, uint32 Wanted) { return Entry.first < Wanted; });
					if (Found == Points.end() || Found->first != Key)
					{
						continue;
					}
					std::array<float, 3>& Point = Blend.Cloths[Index][Corner];
					const std::array<float, 3> Then{static_cast<float>(Found->second[0]), static_cast<float>(Found->second[1]), static_cast<float>(Found->second[2])};
					if (HasJumped(Point, Then))
					{
						continue;
					}
					for (std::size_t Axis = 0; Axis < 3; ++Axis)
					{
						Point[Axis] = Lerp(Then[Axis], Point[Axis], Alpha);
					}
				}
			}
		}
	}

	void KeepBattleFrame(const FBattleShowFrame& Frame, FBattleFrameKeep& Kept)
	{
		Kept.Step = Frame.Step;
		Kept.View = Frame.View;
		Kept.Eye = Frame.Eye;
		Kept.Target = Frame.Target;
		Kept.CentreX = Frame.CentreX;
		Kept.CentreY = Frame.CentreY;
		Kept.ProjectionDistance = Frame.ProjectionDistance;
		Kept.ClearColor = Frame.bCleared ? Frame.ClearColor : std::array<uint8, 3>{};
		Kept.bFade = std::any_of(Frame.Order.begin(), Frame.Order.end(), [](const FBattleShowLink& Link) { return Link.Kind == EBattleShowLink::Fade; });
		Kept.Fade = Frame.Fade;
		Kept.Backdrop = Frame.Backdrop;
		Kept.Hud.Primitives.clear();
		std::copy_if(Frame.Hud.Primitives.begin(), Frame.Hud.Primitives.end(), std::back_inserter(Kept.Hud.Primitives),
			[](const FMenuPrimitive& Primitive) { return Primitive.BlendKey != 0; });

		Kept.Anchors.clear();
		for (const FBattleShowDrawer& Drawer : Frame.Drawers)
		{
			const std::array<float, 3> World{static_cast<float>(Drawer.World[0]), static_cast<float>(Drawer.World[1]), static_cast<float>(Drawer.World[2])};
			Kept.Anchors.push_back({AnchorKey(static_cast<uint64>(Drawer.Kind), Drawer.BlendKey), PlacedView(Frame.View, Drawer.World), World});
		}
		// A drawer that draws through matrices of its own moves as the first of them does.
		std::vector<bool> Matched(Frame.Drawers.size(), false);
		const auto Take = [&](uint16 Drawer, const FGteMatrix& Matrix)
		{
			if (Drawer < Matched.size() && !Matched[Drawer])
			{
				Matched[Drawer] = true;
				Kept.Anchors[Drawer].Matrix = FAffine::From(Matrix);
			}
		};
		for (const FBattleShowQuad& Quad : Frame.Quads)
		{
			Take(Quad.Drawer, Quad.Quad.Matrix);
		}
		for (const FBattleShowTriangle& Triangle : Frame.Triangles)
		{
			Take(Triangle.Drawer, Triangle.Matrix);
		}
		for (const FBattleShowModel& Model : Frame.Models)
		{
			Take(Model.Drawer, Model.Matrix);
		}

		Kept.FirstPart = static_cast<uint32>(Kept.Anchors.size());
		Kept.FirstShadow = Kept.FirstPart;
		Kept.Cloths.clear();
		if (Frame.Objects != nullptr)
		{
			FAffine ViewInverse;
			if (!FAffine::From(Frame.View).Invert(ViewInverse))
			{
				ViewInverse = FAffine::From(FGteMatrix::Identity());
			}
			for (const FBattleDrawnPart& Part : Frame.Objects->Parts)
			{
				Kept.Anchors.push_back({AnchorKey(PartAnchor, static_cast<uint64>(Part.Slot) << 16 | Part.Bone), FAffine::From(Part.Matrix), WorldOrigin(ViewInverse, Part.Matrix)});
			}
			Kept.FirstShadow = static_cast<uint32>(Kept.Anchors.size());
			for (const FBattleDrawnShadow& Shadow : Frame.Objects->Shadows)
			{
				Kept.Anchors.push_back({AnchorKey(ShadowAnchor, Shadow.Slot), FAffine::From(Shadow.Matrix), WorldOrigin(ViewInverse, Shadow.Matrix)});
			}
			for (const FGearDrawnClothTriangle& Triangle : Frame.Objects->Cloths)
			{
				Kept.Cloths.push_back({Triangle.Points, Triangle.PointKeys});
			}
		}
		std::vector<std::pair<uint64, uint32>> Scratch;
		CountSharedKeys(Kept.Anchors, Scratch);
	}

	void BlendBattleFrames(const FBattleFrameKeep& Latest, const FBattleFrameKeep* Previous, float Alpha, FBattleFrameBlend& Blend,
		FBattleFrameBlendScratch& Scratch)
	{
		Blend.Moves.assign(Latest.Anchors.size(), UnmovedView);
		Blend.FirstPart = Latest.FirstPart;
		Blend.FirstShadow = Latest.FirstShadow;
		Blend.View = UnmovedView;
		Blend.Cloths.resize(Latest.Cloths.size());
		for (std::size_t Index = 0; Index < Latest.Cloths.size(); ++Index)
		{
			for (std::size_t Corner = 0; Corner < 3; ++Corner)
			{
				const FGteVector& Point = Latest.Cloths[Index].Points[Corner];
				Blend.Cloths[Index][Corner] = {static_cast<float>(Point[0]), static_cast<float>(Point[1]), static_cast<float>(Point[2])};
			}
		}
		Blend.CentreX = static_cast<float>(Latest.CentreX);
		Blend.CentreY = static_cast<float>(Latest.CentreY);
		Blend.ProjectionDistance = static_cast<float>(Latest.ProjectionDistance);
		Blend.Backdrop = Latest.Backdrop;
		Blend.DomeCorners.resize(Latest.Backdrop.Dome.size());
		for (std::size_t Index = 0; Index < Latest.Backdrop.Dome.size(); ++Index)
		{
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				const std::array<int16, 2>& At = Latest.Backdrop.Dome[Index].Quad.Corners[Corner];
				Blend.DomeCorners[Index][Corner] = {static_cast<float>(At[0]), static_cast<float>(At[1])};
			}
		}
		for (std::size_t Channel = 0; Channel < 3; ++Channel)
		{
			Blend.ClearColor[Channel] = FilledChannel(Latest.ClearColor[Channel]);
			Blend.FadeColor[Channel] = Latest.Fade.Color[Channel];
		}
		if (Previous == nullptr || !AreContinuous(Latest, *Previous))
		{
			return;
		}

		Blend.View = BlendedViewMove(FAffine::From(Previous->View), FAffine::From(Latest.View), Alpha);
		Scratch.Keys.clear();
		for (std::size_t Index = 0; Index < Previous->Anchors.size(); ++Index)
		{
			Scratch.Keys.emplace_back(Previous->Anchors[Index].Key, static_cast<uint32>(Index));
		}
		std::sort(Scratch.Keys.begin(), Scratch.Keys.end());
		for (std::size_t Index = 0; Index < Latest.Anchors.size(); ++Index)
		{
			const FBattleFrameAnchor& Now = Latest.Anchors[Index];
			const auto Found = std::lower_bound(Scratch.Keys.begin(), Scratch.Keys.end(), std::pair<uint64, uint32>(Now.Key, 0));
			if (Found == Scratch.Keys.end() || Found->first != Now.Key)
			{
				continue;
			}
			const FBattleFrameAnchor& Then = Previous->Anchors[Found->second];
			if (!HasJumped(Now.World, Then.World))
			{
				Blend.Moves[Index] = BlendedViewMove(Then.Matrix, Now.Matrix, Alpha);
			}
		}

		Blend.CentreX = Lerp(static_cast<float>(Previous->CentreX), Blend.CentreX, Alpha);
		Blend.CentreY = Lerp(static_cast<float>(Previous->CentreY), Blend.CentreY, Alpha);
		Blend.ProjectionDistance = Lerp(static_cast<float>(Previous->ProjectionDistance), Blend.ProjectionDistance, Alpha);
		const bool bFadeBlends = Latest.bFade && Previous->bFade && Latest.Fade.Mode == Previous->Fade.Mode;
		for (std::size_t Channel = 0; Channel < 3; ++Channel)
		{
			Blend.ClearColor[Channel] = Lerp(FilledChannel(Previous->ClearColor[Channel]), Blend.ClearColor[Channel], Alpha);
			if (bFadeBlends)
			{
				Blend.FadeColor[Channel] = Lerp(Previous->Fade.Color[Channel], Blend.FadeColor[Channel], Alpha);
			}
		}
		BlendCloths(Latest, *Previous, Alpha, Blend, Scratch.ClothPoints);
		BlendBackdrop(Previous->Backdrop, Latest.Backdrop, Alpha, Blend);
	}
}
