#include "Menu/MenuDrawing.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Weltall
{
	namespace
	{
		constexpr float ScreenCentreX = 160.0f;
		constexpr float ScreenCentreY = 112.0f;
		constexpr float ProjectionDistance = 512.0f;
		constexpr float TurnUnits = 4096.0f;
		constexpr float Pi = 3.14159265358979f;
		/** Far past any display's edge, on the 320 x 224 screen's scale. */
		constexpr int16 Beyond = 8192;
		constexpr FScreenRect WideArea{-Beyond, -Beyond, 320 + 2 * Beyond, 224 + 2 * Beyond};

		/** The layer's transform between two frames, in floating point. */
		struct FLayerView
		{
			std::array<std::array<float, 3>, 3> Rotation{};
			std::array<float, 3> Translation{};
		};

		/** The rotation the game's own routine makes from three angles (Rx Ry Rz), in floating point. */
		FLayerView MakeView(const std::array<float, 3>& Angles, const std::array<float, 3>& Translation)
		{
			const float X = Angles[0] * 2.0f * Pi / TurnUnits;
			const float Y = Angles[1] * 2.0f * Pi / TurnUnits;
			const float Z = Angles[2] * 2.0f * Pi / TurnUnits;
			const float Sx = std::sin(X), Cx = std::cos(X), Sy = std::sin(Y), Cy = std::cos(Y), Sz = std::sin(Z), Cz = std::cos(Z);
			FLayerView View;
			View.Rotation = {{
				{Cz * Cy, -Sz * Cy, Sy},
				{Sz * Cx + Cz * Sy * Sx, Cz * Cx - Sz * Sy * Sx, -Cy * Sx},
				{Sz * Sx - Cz * Sy * Cx, Cz * Sx + Sz * Sy * Cx, Cy * Cx},
			}};
			View.Translation = Translation;
			return View;
		}

		std::pair<float, float> Project(const FLayerView& View, float X, float Y, float Z)
		{
			std::array<float, 3> Moved{};
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				Moved[Row] = View.Rotation[Row][0] * X + View.Rotation[Row][1] * Y + View.Rotation[Row][2] * Z + View.Translation[Row];
			}
			const float Depth = std::max(Moved[2], 1.0f);
			return {ScreenCentreX + ProjectionDistance * Moved[0] / Depth, ScreenCentreY + ProjectionDistance * Moved[1] / Depth};
		}

		float Lerp(float From, float To, float Alpha)
		{
			return From + (To - From) * Alpha;
		}

		/**
		 * The GPU steps a polygon's texels from half a texel in at its pixels' corners, where a renderer samples them at the
		 * pixels' centres: moves Drawn's texels by the difference, from their rates across Primitive's corners on the screen.
		 * Nothing for a picture drawn at its own size; a texel for one drawn mirrored, which the GPU shows from the texel its
		 * corner names. The stepped texels of a picture drawn at another size reach past its first or last by part of a
		 * texel, which the console's own pixels never show and a finer display would: Drawn keeps to the picture's own.
		 * Places are the first three corners on the screen: as the primitive gives them, or as a layer projects them.
		 */
		void StepFromHalfTexel(const FMenuPrimitive& Primitive, const std::array<std::array<float, 2>, 3>& Places, FDrawPrimitive& Drawn)
		{
			const FMenuCorner& Origin = Primitive.Corners[0];
			const FMenuCorner& Second = Primitive.Corners[1];
			const FMenuCorner& Third = Primitive.Corners[2];
			const float AcrossX = Places[1][0] - Places[0][0];
			const float AcrossY = Places[1][1] - Places[0][1];
			const float DownX = Places[2][0] - Places[0][0];
			const float DownY = Places[2][1] - Places[0][1];
			const float Area = AcrossX * DownY - DownX * AcrossY;
			if (Area == 0.0f)
			{
				return;
			}
			const auto Shift = [&](float ToSecond, float ToThird) {
				const float PerX = (ToSecond * DownY - ToThird * AcrossY) / Area;
				const float PerY = (AcrossX * ToThird - DownX * ToSecond) / Area;
				return 0.5f * (1.0f - PerX - PerY);
			};
			const float ShiftU = Shift(static_cast<float>(Second.U - Origin.U), static_cast<float>(Third.U - Origin.U));
			const float ShiftV = Shift(static_cast<float>(Second.V - Origin.V), static_cast<float>(Third.V - Origin.V));
			for (std::size_t Corner = 0; Corner < Drawn.CornerCount; ++Corner)
			{
				Drawn.Corners[Corner].U += ShiftU;
				Drawn.Corners[Corner].V += ShiftV;
			}
			// The picture's own texels: from the least its corners name to the one before the greatest, and a texel
			// on where it is mirrored (a shift past half a texel), since its corners then name the texels before.
			const auto [LowU, HighU] = std::minmax({Primitive.Corners[0].U, Primitive.Corners[1].U, Primitive.Corners[2].U, Primitive.Corners[3].U});
			const auto [LowV, HighV] = std::minmax({Primitive.Corners[0].V, Primitive.Corners[1].V, Primitive.Corners[2].V, Primitive.Corners[3].V});
			const auto Own = [](int32 Low, int32 High, float Shifted) {
				const int32 Mirrored = Shifted > 0.5f ? 1 : 0;
				const int32 First = std::clamp(Low + Mirrored, 0, 255);
				return std::pair<uint8, uint8>{static_cast<uint8>(First), static_cast<uint8>(std::clamp(High + Mirrored - 1, First, 255))};
			};
			const auto [FirstU, LastU] = Own(LowU, HighU, ShiftU);
			const auto [FirstV, LastV] = Own(LowV, HighV, ShiftV);
			Drawn.Texels = FDrawTexels{FirstU, FirstV, LastU, LastV};
		}

		/** An angle Alpha of the way from From to To, the short way round a turn of 4096. */
		float LerpAngle(int16 From, int16 To, float Alpha)
		{
			int32 Delta = (static_cast<int32>(To) - static_cast<int32>(From)) & 0xFFF;
			if (Delta >= 0x800)
			{
				Delta -= 0x1000;
			}
			return static_cast<float>(From) + static_cast<float>(Delta) * Alpha;
		}

		FLayerView ViewBetween(const FMenuLayerTransform& From, const FMenuLayerTransform& To, float Alpha)
		{
			std::array<float, 3> Angles{};
			std::array<float, 3> Translation{};
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Angles[Axis] = LerpAngle(From.Angles[Axis], To.Angles[Axis], Alpha);
				Translation[Axis] = Lerp(static_cast<float>(From.Translation[Axis]), static_cast<float>(To.Translation[Axis]), Alpha);
			}
			return MakeView(Angles, Translation);
		}


		/**
		 * A line's segments as the pixels the GPU draws, both ends included: each a band one pixel across
		 * along its longer axis, from its first point's pixel to past its last one's.
		 */
		void AddLine(const FMenuPrimitive& Line, EBlendMode UntexturedBlend, const FScreenRect& Clip, FDrawList& Out)
		{
			for (int32 Segment = 0; Segment + 1 < Line.LineCorners(); ++Segment)
			{
				FMenuCorner From = Line.Corners[static_cast<std::size_t>(Segment)];
				FMenuCorner To = Line.Corners[static_cast<std::size_t>(Segment + 1)];
				const bool bAcross = std::abs(To.X - From.X) >= std::abs(To.Y - From.Y);
				if ((bAcross && To.X < From.X) || (!bAcross && To.Y < From.Y))
				{
					std::swap(From, To);
				}
				FDrawPrimitive Drawn;
				Drawn.CornerCount = 4;
				Drawn.Blend = (Line.Code & MenuGpu::SemiTransparent) != 0 ? EDrawBlend::SemiTransparent : EDrawBlend::Opaque;
				Drawn.UntexturedBlend = UntexturedBlend;
				Drawn.Clip = Clip;
				const float X0 = static_cast<float>(From.X);
				const float Y0 = static_cast<float>(From.Y);
				const float X1 = static_cast<float>(To.X) + (bAcross ? 1.0f : 0.0f);
				const float Y1 = static_cast<float>(To.Y) + (bAcross ? 0.0f : 1.0f);
				const float SideX = bAcross ? 0.0f : 1.0f;
				const float SideY = bAcross ? 1.0f : 0.0f;
				const std::array<std::array<float, 2>, 4> Places = {{{X0, Y0}, {X0 + SideX, Y0 + SideY}, {X1, Y1}, {X1 + SideX, Y1 + SideY}}};
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					Drawn.Corners[Corner].X = Places[Corner][0];
					Drawn.Corners[Corner].Y = Places[Corner][1];
					Drawn.Corners[Corner].Color = Line.Corners[0].Color;
				}
				Out.Primitives.push_back(Drawn);
			}
		}

		/** A rectangle as a quad over its pixels, its texels one to a pixel from its corner's in the drawing mode's page. */
		void AddRectangle(const FMenuPrimitive& Rectangle, uint16 Page, EBlendMode UntexturedBlend, const FScreenRect& Clip, FDrawList& Out)
		{
			FDrawPrimitive Drawn;
			Drawn.CornerCount = 4;
			Drawn.bModulate = (Rectangle.Code & MenuGpu::RawTexture) == 0;
			Drawn.Blend = (Rectangle.Code & MenuGpu::SemiTransparent) != 0 ? EDrawBlend::SemiTransparent : EDrawBlend::Opaque;
			Drawn.UntexturedBlend = UntexturedBlend;
			Drawn.Clip = Clip;
			if ((Rectangle.Code & MenuGpu::Textured) != 0)
			{
				Drawn.Texture = FDrawTexture{0, FTexturePage::FromAttribute(Page), FClutPosition::FromAttribute(Rectangle.Clut)};
			}
			const FMenuCorner& At = Rectangle.Corners[0];
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				const float Across = (Corner & 1) != 0 ? static_cast<float>(Rectangle.Width) : 0.0f;
				const float Down = (Corner & 2) != 0 ? static_cast<float>(Rectangle.Height) : 0.0f;
				FDrawVertex& Vertex = Drawn.Corners[Corner];
				Vertex.X = static_cast<float>(At.X) + Across;
				Vertex.Y = static_cast<float>(At.Y) + Down;
				Vertex.U = static_cast<float>(At.U) + Across;
				Vertex.V = static_cast<float>(At.V) + Down;
				Vertex.Color = At.Color;
			}
			if (Rectangle.bWholeScreen)
			{
				Drawn.Clip = WideArea;
				Drawn.bSpansDisplay = true;
			}
			Out.Primitives.push_back(Drawn);
		}
	}

	std::pair<float, float> ProjectMenuPoint(const FMenuLayerTransform& Transform, float X, float Y, float Z)
	{
		return Project(ViewBetween(Transform, Transform, 1.0f), X, Y, Z);
	}

	bool FMenuDrawBuilder::WithinReach(const FMenuPrimitive& Before, const FMenuPrimitive& Now) const
	{
		if (BlendReach <= 0.0f || Now.Space != EMenuSpace::Screen)
		{
			return true;
		}
		for (int32 Corner = 0; Corner < Now.PolygonCorners(); ++Corner)
		{
			const FMenuCorner& From = Before.Corners[static_cast<std::size_t>(Corner)];
			const FMenuCorner& To = Now.Corners[static_cast<std::size_t>(Corner)];
			if (std::abs(To.X - From.X) > BlendReach || std::abs(To.Y - From.Y) > BlendReach)
			{
				return false;
			}
		}
		return true;
	}

	void FMenuDrawBuilder::Build(const FMenuFrame& Latest, const FMenuFrame* Previous, float Alpha, FDrawList& Out)
	{
		Out.Primitives.clear();
		Icons.clear();
		TextCells.clear();
		PreviousKeys.clear();
		const bool bTextButtons = !Latest.TextButtons.empty();
		if (bTextButtons)
		{
			ListPlaces.assign(Latest.Primitives.size() + 1, 0);
		}
		if (Previous != nullptr)
		{
			for (std::size_t Index = 0; Index < Previous->Primitives.size(); ++Index)
			{
				if (Previous->Primitives[Index].BlendKey != 0)
				{
					PreviousKeys.emplace_back(Previous->Primitives[Index].BlendKey, static_cast<int32>(Index));
				}
			}
			std::sort(PreviousKeys.begin(), PreviousKeys.end());
		}
		const FLayerView LayerView = ViewBetween(Previous != nullptr ? Previous->Layer : Latest.Layer, Latest.Layer, Alpha);
		const FLayerView FlatView = ViewBetween(MenuFlatTransform, MenuFlatTransform, 1.0f);
		const FScreenRect Area = bWideDrawingArea ? WideArea : FScreenRect{0, 0, static_cast<int16>(Screen.Width), static_cast<int16>(Screen.Height)};
		EBlendMode UntexturedBlend = EBlendMode::Average;
		uint16 ModePage = 0;
		for (const FMenuPrimitive& Primitive : Latest.Primitives)
		{
			if (bTextButtons)
			{
				ListPlaces[static_cast<std::size_t>(&Primitive - Latest.Primitives.data())] = static_cast<uint32>(Out.Primitives.size());
			}
			if (Primitive.IsDrawMode())
			{
				UntexturedBlend = static_cast<EBlendMode>(Primitive.Words[0] >> 5 & 3);
				ModePage = static_cast<uint16>(Primitive.Words[0] & 0x9FF);
				continue;
			}
			if (Primitive.IsRectangle())
			{
				AddRectangle(Primitive, ModePage, UntexturedBlend, Area, Out);
				continue;
			}
			if (Primitive.IsLine())
			{
				AddLine(Primitive, UntexturedBlend, Area, Out);
				continue;
			}
			if (!Primitive.IsPolygon())
			{
				continue;
			}
			const FMenuPrimitive* Partner = nullptr;
			if (Primitive.BlendKey != 0 && !PreviousKeys.empty())
			{
				const auto Found = std::lower_bound(PreviousKeys.begin(), PreviousKeys.end(), std::make_pair(Primitive.BlendKey, 0));
				if (Found != PreviousKeys.end() && Found->first == Primitive.BlendKey)
				{
					const FMenuPrimitive& Candidate = Previous->Primitives[static_cast<std::size_t>(Found->second)];
					if (Candidate.Space == Primitive.Space && WithinReach(Candidate, Primitive))
					{
						Partner = &Candidate;
					}
				}
			}
			FDrawPrimitive Drawn;
			Drawn.CornerCount = static_cast<uint8>(Primitive.PolygonCorners());
			Drawn.bGouraud = (Primitive.Code & MenuGpu::Gouraud) != 0;
			Drawn.bModulate = (Primitive.Code & MenuGpu::RawTexture) == 0;
			Drawn.Blend = (Primitive.Code & MenuGpu::SemiTransparent) != 0 ? EDrawBlend::SemiTransparent : EDrawBlend::Opaque;
			Drawn.UntexturedBlend = UntexturedBlend;
			Drawn.Clip = Area;
			Drawn.BlendKey = Primitive.BlendKey;
			if ((Primitive.Code & MenuGpu::Textured) != 0)
			{
				Drawn.Texture = FDrawTexture{0, FTexturePage::FromAttribute(Primitive.Page), FClutPosition::FromAttribute(Primitive.Clut)};
			}
			for (std::size_t Corner = 0; Corner < Drawn.CornerCount; ++Corner)
			{
				const FMenuCorner& At = Primitive.Corners[Corner];
				FDrawVertex& Vertex = Drawn.Corners[Corner];
				Vertex.Color = Drawn.bGouraud ? At.Color : Primitive.Corners[0].Color;
				Vertex.U = static_cast<float>(At.U);
				Vertex.V = static_cast<float>(At.V);
				if (Primitive.Space == EMenuSpace::Screen)
				{
					Vertex.X = static_cast<float>(At.X);
					Vertex.Y = static_cast<float>(At.Y);
					if (Partner != nullptr)
					{
						Vertex.X = Lerp(static_cast<float>(Partner->Corners[Corner].X), Vertex.X, Alpha);
						Vertex.Y = Lerp(static_cast<float>(Partner->Corners[Corner].Y), Vertex.Y, Alpha);
					}
					continue;
				}
				std::array<float, 3> Point{};
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Point[Axis] = static_cast<float>(Primitive.Points[Corner][Axis]);
					if (Partner != nullptr)
					{
						Point[Axis] = Lerp(static_cast<float>(Partner->Points[Corner][Axis]), Point[Axis], Alpha);
					}
				}
				const auto [X, Y] = Project(Primitive.Space == EMenuSpace::Layer ? LayerView : FlatView, Point[0], Point[1], Point[2]);
				Vertex.X = X;
				Vertex.Y = Y;
			}
			if (Drawn.Texture)
			{
				// A layer's primitive is stepped as the GPU steps the corners the GTE projected it to.
				std::array<std::array<float, 2>, 3> Places{};
				for (std::size_t Corner = 0; Corner < Places.size(); ++Corner)
				{
					const bool bOnScreen = Primitive.Space == EMenuSpace::Screen;
					Places[Corner] = {bOnScreen ? static_cast<float>(Primitive.Corners[Corner].X) : Drawn.Corners[Corner].X,
						bOnScreen ? static_cast<float>(Primitive.Corners[Corner].Y) : Drawn.Corners[Corner].Y};
				}
				StepFromHalfTexel(Primitive, Places, Drawn);
			}
			if (Primitive.bWholeScreen)
			{
				Drawn.Clip = WideArea;
				Drawn.bSpansDisplay = true;
				const std::array<float, 2> Across = {static_cast<float>(WideArea.X), static_cast<float>(WideArea.X + WideArea.Width)};
				const std::array<float, 2> Down = {static_cast<float>(WideArea.Y), static_cast<float>(WideArea.Y + WideArea.Height)};
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					Drawn.Corners[Corner].X = Across[Corner & 1];
					Drawn.Corners[Corner].Y = Down[Corner >> 1];
				}
			}
			if (Primitive.ButtonRole != 0 && Drawn.Texture)
			{
				Icons.push_back({static_cast<uint32>(Out.Primitives.size()), Primitive.ButtonRole, Primitive.ButtonSlot});
			}
			Out.Primitives.push_back(Drawn);
		}
		if (bTextButtons)
		{
			// The frame's cells name its primitives: the list's stand where those were drawn.
			ListPlaces.back() = static_cast<uint32>(Out.Primitives.size());
			const auto Place = [this](uint32 Primitive) { return ListPlaces[std::min<std::size_t>(Primitive, ListPlaces.size() - 1)]; };
			for (FTextButtonCell Cell : Latest.TextButtons)
			{
				Cell.Primitive = Place(Cell.Primitive);
				Cell.LineEnd = Cell.LineEnd != 0 ? Place(Cell.LineEnd) : 0;
				Cell.LineRest = Cell.LineRest != 0 ? Place(Cell.LineRest - 1) + 1 : 0;
				TextCells.push_back(Cell);
			}
		}
	}
}
