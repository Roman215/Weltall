#include "Menu/MenuButtonIcons.h"

#include "MenuIconFont.h"
#include "MenuIconLabels.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace Weltall
{
	namespace
	{
		/** A colour's 5-bit components, as the GPU's 15-bit texels hold them. */
		struct FColor5
		{
			int32 R = 0;
			int32 G = 0;
			int32 B = 0;
		};

		constexpr FColor5 Grey(int32 Level)
		{
			return {Level, Level, Level};
		}

		// The menu sheet's buttons and hint boxes: their outline, body, rim and text.
		constexpr FColor5 Outline = Grey(2);
		constexpr FColor5 BoxRim = Grey(17);
		constexpr FColor5 BoxBody = Grey(9);
		constexpr FColor5 BoxEdge = Grey(1);
		constexpr FColor5 LabelWhite = Grey(31);
		constexpr FColor5 LabelShadow = Grey(13);
		constexpr FColor5 SymbolShadow = Grey(3);
		// A keyboard's key cap.
		constexpr FColor5 KeyTop = Grey(26);
		constexpr FColor5 KeySide = Grey(15);
		constexpr FColor5 KeyInk = Grey(4);
		// The PlayStation's symbols in the menu sheet's colours, and the other controllers' letters.
		constexpr FColor5 CrossBlue{6, 13, 26};
		constexpr FColor5 CircleRed{26, 6, 5};
		constexpr FColor5 TriangleGreen{10, 26, 17};
		constexpr FColor5 SquarePink{24, 12, 25};
		constexpr FColor5 XboxGreen{9, 25, 7};
		constexpr FColor5 XboxRed{29, 7, 6};
		constexpr FColor5 XboxBlue{7, 15, 31};
		constexpr FColor5 XboxYellow{31, 26, 5};
		constexpr FColor5 PlainLetter = Grey(29);
		constexpr FColor5 PadArm = Grey(12);

		constexpr int32 FaceSize = 12 * MenuIconTexelsPerPixel;
		constexpr int32 HintWidth = 16 * MenuIconTexelsPerPixel;
		constexpr int32 HintHeight = 9 * MenuIconTexelsPerPixel;
		/** How far in from a picture's sides its label keeps. */
		constexpr int32 LabelMargin = 2;

		/** A texel of the colour, flagged semi-transparent as the menu sheet's are (which only a semi-transparent primitive heeds). */
		uint16 Texel(FColor5 Color)
		{
			const auto Clamp = [](int32 Value) { return static_cast<uint16>(std::clamp(Value, 0, 31)); };
			return static_cast<uint16>(0x8000 | Clamp(Color.B) << 10 | Clamp(Color.G) << 5 | Clamp(Color.R));
		}

		class FCanvas
		{
		public:
			FCanvas(int32 Width, int32 Height)
			{
				Picture.Width = Width;
				Picture.Height = Height;
				Picture.Texels.assign(static_cast<std::size_t>(Width * Height), 0);
			}

			void Set(int32 X, int32 Y, FColor5 Color)
			{
				if (X >= 0 && Y >= 0 && X < Picture.Width && Y < Picture.Height)
				{
					Picture.Texels[static_cast<std::size_t>(Y * Picture.Width + X)] = Texel(Color);
				}
			}

			int32 Width() const { return Picture.Width; }
			int32 Height() const { return Picture.Height; }

			FMenuIconPicture Picture;
		};

		/** The body's grey at a point from the centre: lighter to the top left, as the sheet's buttons are lit. */
		FColor5 BodyShade(float Dx, float Dy, float Radius)
		{
			const float Light = -(Dx + Dy) / Radius;
			return Grey(9 + static_cast<int32>(std::lround(std::clamp(Light * 3.0f, -3.0f, 4.0f))));
		}

		/** The face button's round body: an outline, then the lit grey. */
		void DrawRoundButton(FCanvas& Canvas)
		{
			const float Centre = static_cast<float>(Canvas.Width()) / 2.0f;
			const float Radius = Centre - 0.3f;
			for (int32 Y = 0; Y < Canvas.Height(); ++Y)
			{
				for (int32 X = 0; X < Canvas.Width(); ++X)
				{
					const float Dx = static_cast<float>(X) + 0.5f - Centre;
					const float Dy = static_cast<float>(Y) + 0.5f - Centre;
					const float Distance = std::sqrt(Dx * Dx + Dy * Dy);
					if (Distance <= Radius)
					{
						Canvas.Set(X, Y, Distance > Radius - 1.2f ? Outline : BodyShade(Dx, Dy, Radius));
					}
				}
			}
		}

		/** A rounded rectangle over the whole canvas in the round button's colours: a face slot's other buttons. */
		void DrawChip(FCanvas& Canvas)
		{
			constexpr float Corner = 5.0f;
			const float Width = static_cast<float>(Canvas.Width());
			const float Height = static_cast<float>(Canvas.Height());
			for (int32 Y = 0; Y < Canvas.Height(); ++Y)
			{
				for (int32 X = 0; X < Canvas.Width(); ++X)
				{
					const float Px = static_cast<float>(X) + 0.5f;
					const float Py = static_cast<float>(Y) + 0.5f;
					const float Cx = std::clamp(Px, Corner, Width - Corner);
					const float Cy = std::clamp(Py, Corner, Height - Corner);
					const float Distance = std::sqrt((Px - Cx) * (Px - Cx) + (Py - Cy) * (Py - Cy));
					if (Distance > Corner)
					{
						continue;
					}
					const bool bEdge = Distance > Corner - 1.2f || Px < 1.0f || Py < 1.0f || Px > Width - 1.0f || Py > Height - 1.0f;
					Canvas.Set(X, Y, bEdge ? Outline : BodyShade(Px - Width / 2.0f, Py - Height / 2.0f, std::max(Width, Height) / 2.0f));
				}
			}
		}

		/** The hints' box as the sheet draws it, each of its pixels two texels: a light rim at the top and left, dark at the bottom and right. */
		void DrawBox(FCanvas& Canvas)
		{
			constexpr int32 Rim = MenuIconTexelsPerPixel;
			for (int32 Y = 0; Y < Canvas.Height(); ++Y)
			{
				for (int32 X = 0; X < Canvas.Width(); ++X)
				{
					FColor5 Color = BoxBody;
					if (X >= Canvas.Width() - Rim || Y >= Canvas.Height() - Rim)
					{
						Color = BoxEdge;
					}
					else if (X < Rim || Y < Rim)
					{
						Color = BoxRim;
					}
					Canvas.Set(X, Y, Color);
				}
			}
		}

		/** A keyboard's key: a light top over a darker side, rounded, outlined. */
		void DrawKeyCap(FCanvas& Canvas)
		{
			constexpr float Corner = 3.0f;
			constexpr int32 Side = 4;
			const float Width = static_cast<float>(Canvas.Width());
			const float Height = static_cast<float>(Canvas.Height());
			for (int32 Y = 0; Y < Canvas.Height(); ++Y)
			{
				for (int32 X = 0; X < Canvas.Width(); ++X)
				{
					const float Px = static_cast<float>(X) + 0.5f;
					const float Py = static_cast<float>(Y) + 0.5f;
					const float Cx = std::clamp(Px, Corner, Width - Corner);
					const float Cy = std::clamp(Py, Corner, Height - Corner);
					const float Distance = std::sqrt((Px - Cx) * (Px - Cx) + (Py - Cy) * (Py - Cy));
					if (Distance > Corner)
					{
						continue;
					}
					const bool bEdge = Distance > Corner - 1.2f || Px < 1.0f || Py < 1.0f || Px > Width - 1.0f || Py > Height - 1.0f;
					Canvas.Set(X, Y, bEdge ? Outline : (Y >= Canvas.Height() - Side ? KeySide : KeyTop));
				}
			}
		}

		int32 TextWidth(std::string_view Text, int32 Scale, bool bBig)
		{
			int32 Width = 0;
			for (const char Code : Text)
			{
				const MenuIconFont::FGlyph Glyph = bBig ? MenuIconFont::BigGlyph(Code) : MenuIconFont::SmallGlyph(Code);
				Width += (Glyph.Width() + 1) * Scale;
			}
			return Width > 0 ? Width - Scale : 0;
		}

		/** Text at (X, Y), each font texel Scale texels square, its shadow Offset down and right (none at 0). */
		void DrawText(FCanvas& Canvas, std::string_view Text, int32 X, int32 Y, int32 Scale, bool bBig, FColor5 Color, FColor5 Shadow, int32 Offset)
		{
			for (const int32 Pass : {0, 1})
			{
				if (Pass == 0 && Offset == 0)
				{
					continue;
				}
				const FColor5 Drawn = Pass == 0 ? Shadow : Color;
				const int32 Shift = Pass == 0 ? Offset : 0;
				int32 Left = X;
				for (const char Code : Text)
				{
					const MenuIconFont::FGlyph Glyph = bBig ? MenuIconFont::BigGlyph(Code) : MenuIconFont::SmallGlyph(Code);
					for (int32 Row = 0; Row < Glyph.Height(); ++Row)
					{
						for (int32 Column = 0; Column < Glyph.Width(); ++Column)
						{
							if (Glyph.Rows[static_cast<std::size_t>(Row)][static_cast<std::size_t>(Column)] != '#')
							{
								continue;
							}
							for (int32 Sy = 0; Sy < Scale; ++Sy)
							{
								for (int32 Sx = 0; Sx < Scale; ++Sx)
								{
									Canvas.Set(Left + Column * Scale + Sx + Shift, Y + Row * Scale + Sy + Shift, Drawn);
								}
							}
						}
					}
					Left += (Glyph.Width() + 1) * Scale;
				}
			}
		}

		/** A label centred in the area from Top, Height texels high: twice the font's size where it fits across Room, else its own. */
		void DrawLabel(FCanvas& Canvas, std::string_view Label, int32 Top, int32 Height, int32 Room, FColor5 Color, FColor5 Shadow, bool bShadow)
		{
			constexpr int32 FontHeight = 5;
			const int32 Scale = TextWidth(Label, 2, false) <= Room && FontHeight * 2 <= Height ? 2 : 1;
			const int32 Width = TextWidth(Label, Scale, false);
			const int32 X = (Canvas.Width() - Width) / 2;
			const int32 Y = Top + (Height - FontHeight * Scale) / 2;
			DrawText(Canvas, Label, X, Y, Scale, false, Color, Shadow, bShadow ? Scale : 0);
		}

		/** The PlayStation's symbols as the port draws them, in a Size-texel square at (Left, Top): strokes twice Half texels wide. */
		void DrawPlayStationSymbol(FCanvas& Canvas, std::string_view Icon, int32 Left, int32 Top, int32 Size, float Half = 1.1f)
		{
			const float S = static_cast<float>(Size);
			const auto SegmentDistance = [](float Px, float Py, float Ax, float Ay, float Bx, float By)
			{
				const float Dx = Bx - Ax;
				const float Dy = By - Ay;
				const float T = std::clamp(((Px - Ax) * Dx + (Py - Ay) * Dy) / (Dx * Dx + Dy * Dy), 0.0f, 1.0f);
				const float Ex = Px - (Ax + T * Dx);
				const float Ey = Py - (Ay + T * Dy);
				return std::sqrt(Ex * Ex + Ey * Ey);
			};
			FColor5 Color = CrossBlue;
			if (Icon == "Circle")
			{
				Color = CircleRed;
			}
			else if (Icon == "Triangle")
			{
				Color = TriangleGreen;
			}
			else if (Icon == "Square")
			{
				Color = SquarePink;
			}
			const float Low = 0.16f * S;
			const float High = 0.84f * S;
			for (const int32 Pass : {0, 1})
			{
				for (int32 Y = 0; Y < Size; ++Y)
				{
					for (int32 X = 0; X < Size; ++X)
					{
						const float Px = static_cast<float>(X) + 0.5f;
						const float Py = static_cast<float>(Y) + 0.5f;
						float Distance = 0.0f;
						if (Icon == "Circle")
						{
							Distance = std::fabs(std::sqrt((Px - S / 2) * (Px - S / 2) + (Py - S / 2) * (Py - S / 2)) - (S / 2 - Low));
						}
						else if (Icon == "Triangle")
						{
							Distance = std::min({SegmentDistance(Px, Py, S / 2, Low - 1.0f, Low - 1.0f, High), SegmentDistance(Px, Py, Low - 1.0f, High, High + 1.0f, High),
								SegmentDistance(Px, Py, High + 1.0f, High, S / 2, Low - 1.0f)});
						}
						else if (Icon == "Square")
						{
							Distance = std::min({SegmentDistance(Px, Py, Low, Low, High, Low), SegmentDistance(Px, Py, High, Low, High, High),
								SegmentDistance(Px, Py, High, High, Low, High), SegmentDistance(Px, Py, Low, High, Low, Low)});
						}
						else
						{
							Distance = std::min(SegmentDistance(Px, Py, Low, Low, High, High), SegmentDistance(Px, Py, High, Low, Low, High));
						}
						if (Distance <= Half)
						{
							Canvas.Set(Left + X + (Pass == 0 ? 1 : 0), Top + Y + (Pass == 0 ? 1 : 0), Pass == 0 ? SymbolShadow : Color);
						}
					}
				}
			}
		}

		/** The D-pad, the arm of its direction lit, in a Size-texel square at (Left, Top). */
		void DrawDirectionalPad(FCanvas& Canvas, char Direction, int32 Left, int32 Top, int32 Size)
		{
			const int32 Arm = Size / 3;
			const int32 Middle = (Size - Arm) / 2;
			for (int32 Y = 0; Y < Size; ++Y)
			{
				for (int32 X = 0; X < Size; ++X)
				{
					const bool bAcross = Y >= Middle && Y < Middle + Arm;
					const bool bDown = X >= Middle && X < Middle + Arm;
					if (!bAcross && !bDown)
					{
						continue;
					}
					const bool bLit = (Direction == 'u' && bDown && Y < Middle) || (Direction == 'd' && bDown && Y >= Middle + Arm)
						|| (Direction == 'l' && bAcross && X < Middle) || (Direction == 'r' && bAcross && X >= Middle + Arm);
					const bool bEdge = X == 0 || Y == 0 || X == Size - 1 || Y == Size - 1 || (!bAcross && (X == Middle || X == Middle + Arm - 1))
						|| (!bDown && (Y == Middle || Y == Middle + Arm - 1));
					Canvas.Set(Left + X + 1, Top + Y + 1, SymbolShadow);
					Canvas.Set(Left + X, Top + Y, bEdge ? Outline : bLit ? LabelWhite : PadArm);
				}
			}
		}

		bool IsViewOrMenu(std::string_view Icon)
		{
			return Icon == "View" || Icon == "Menu";
		}

		/**
		 * The View and Menu buttons' symbols as their controllers print them, in a Size-texel square at (Left, Top): two
		 * overlapping windows, the front one at the bottom left; three bars. Strokes two texels wide, shadowed as the D-pad.
		 */
		void DrawViewOrMenu(FCanvas& Canvas, std::string_view Icon, int32 Left, int32 Top, int32 Size)
		{
			constexpr int32 Stroke = 2;
			const auto Inside = [](int32 X, int32 Y, int32 BoxLeft, int32 BoxTop, int32 Side)
			{
				return X >= BoxLeft && Y >= BoxTop && X < BoxLeft + Side && Y < BoxTop + Side;
			};
			const auto OnFrame = [&Inside](int32 X, int32 Y, int32 BoxLeft, int32 BoxTop, int32 Side)
			{
				return Inside(X, Y, BoxLeft, BoxTop, Side) && !Inside(X, Y, BoxLeft + Stroke, BoxTop + Stroke, Side - 2 * Stroke);
			};
			const int32 Side = Size * 2 / 3;
			const int32 Back = Size - Side;
			const int32 BarWidth = Size - 2;
			const int32 BarGap = (Size - 3 * Stroke) / 4;
			for (const int32 Pass : {0, 1})
			{
				const int32 Shift = Pass == 0 ? 1 : 0;
				for (int32 Y = 0; Y < Size; ++Y)
				{
					for (int32 X = 0; X < Size; ++X)
					{
						bool bInk = false;
						if (Icon == "View")
						{
							// The back window shows only where the front one does not cover it.
							bInk = OnFrame(X, Y, 0, Back, Side) || (OnFrame(X, Y, Back, 0, Side) && !Inside(X, Y, 0, Back, Side));
						}
						else
						{
							const int32 Bar = (Y - BarGap) / (Stroke + BarGap);
							bInk = X >= 1 && X < 1 + BarWidth && Y >= BarGap && Bar < 3 && (Y - BarGap) % (Stroke + BarGap) < Stroke;
						}
						if (bInk)
						{
							Canvas.Set(Left + X + Shift, Top + Y + Shift, Pass == 0 ? SymbolShadow : LabelWhite);
						}
					}
				}
			}
		}

		/** The letter a controller's face button carries, and its colour; none for other icons and the PlayStation's. */
		std::optional<std::pair<char, FColor5>> FaceLetter(EButtonIconSet Set, std::string_view Icon)
		{
			if (Set == EButtonIconSet::PlayStation || Set == EButtonIconSet::Keyboard || Icon.size() != 1)
			{
				return std::nullopt;
			}
			const char Letter = Icon[0];
			if (Letter != 'A' && Letter != 'B' && Letter != 'X' && Letter != 'Y')
			{
				return std::nullopt;
			}
			if (Set != EButtonIconSet::Xbox)
			{
				return std::make_pair(Letter, PlainLetter);
			}
			const FColor5 Colors[] = {XboxGreen, XboxRed, XboxBlue, XboxYellow};
			const std::size_t Index = Letter == 'A' ? 0 : Letter == 'B' ? 1 : Letter == 'X' ? 2 : 3;
			return std::make_pair(Letter, Colors[Index]);
		}

		bool IsPlayStationSymbol(EButtonIconSet Set, std::string_view Icon)
		{
			return Set == EButtonIconSet::PlayStation && (Icon == "Cross" || Icon == "Circle" || Icon == "Triangle" || Icon == "Square");
		}

		/** A D-pad direction's arrow code, '+' for the whole pad (none lit), 0 for any other icon. */
		char PadDirection(std::string_view Icon)
		{
			if (Icon == "DPad")
			{
				return '+';
			}
			constexpr std::string_view Prefix = "DPad_";
			return Icon.starts_with(Prefix) ? MenuIconLabels::ArrowOf(Icon.substr(Prefix.size())) : 0;
		}

		/** A face button's colour among the four dots of a cluster: the controller's own, grey where its buttons have none. */
		FColor5 ClusterColor(EButtonIconSet Set, EFaceButtonPlace Place)
		{
			const std::size_t Index = static_cast<std::size_t>(Place) - 1;
			if (Set == EButtonIconSet::PlayStation)
			{
				return std::array{CrossBlue, CircleRed, SquarePink, TriangleGreen}[Index];
			}
			if (Set == EButtonIconSet::Xbox)
			{
				return std::array{XboxGreen, XboxRed, XboxBlue, XboxYellow}[Index];
			}
			return PlainLetter;
		}

		/** The four face buttons as dots where the controller has them, each darkened in its colour but Lit's, which shines. */
		void DrawCluster(FCanvas& Canvas, EButtonIconSet Set, EFaceButtonPlace Lit)
		{
			constexpr float Radius = 5.2f;
			constexpr float Reach = 6.4f;
			const float Centre = static_cast<float>(Canvas.Width()) / 2.0f;
			for (const EFaceButtonPlace Place : {EFaceButtonPlace::Bottom, EFaceButtonPlace::Right, EFaceButtonPlace::Left, EFaceButtonPlace::Top})
			{
				const float DotX = Centre + (Place == EFaceButtonPlace::Right ? Reach : Place == EFaceButtonPlace::Left ? -Reach : 0.0f);
				const float DotY = Centre + (Place == EFaceButtonPlace::Bottom ? Reach : Place == EFaceButtonPlace::Top ? -Reach : 0.0f);
				const FColor5 Color = ClusterColor(Set, Place);
				for (int32 Y = 0; Y < Canvas.Height(); ++Y)
				{
					for (int32 X = 0; X < Canvas.Width(); ++X)
					{
						const float Dx = static_cast<float>(X) + 0.5f - DotX;
						const float Dy = static_cast<float>(Y) + 0.5f - DotY;
						const float Distance = std::sqrt(Dx * Dx + Dy * Dy);
						if (Distance > Radius)
						{
							continue;
						}
						if (Place != Lit)
						{
							Canvas.Set(X, Y, Distance > Radius - 1.2f ? Outline : FColor5{Color.R / 3 + 2, Color.G / 3 + 2, Color.B / 3 + 2});
							continue;
						}
						// Lit from the top left: the colour whitening toward its highlight.
						const float Shine = std::clamp(1.0f - std::sqrt((Dx + 1.2f) * (Dx + 1.2f) + (Dy + 1.2f) * (Dy + 1.2f)) / Radius, 0.0f, 1.0f);
						const auto Lighten = [Shine](int32 Channel) { return Channel + static_cast<int32>(std::lround(static_cast<float>(31 - Channel) * (0.25f + 0.75f * Shine))); };
						Canvas.Set(X, Y, {Lighten(Color.R), Lighten(Color.G), Lighten(Color.B)});
					}
				}
			}
		}

		/** The canvas's width: the slot's, widened (by whole menu pixels) until the label fits between its margins. */
		int32 WidthFor(std::string_view Label, int32 SlotWidth)
		{
			const int32 Needed = TextWidth(Label, 1, false) + 2 * (LabelMargin + 1);
			return std::max(SlotWidth, (Needed + MenuIconTexelsPerPixel - 1) / MenuIconTexelsPerPixel * MenuIconTexelsPerPixel);
		}
	}

	FMenuIconPicture DrawMenuButtonIcon(EButtonIconSet Set, std::string_view Icon, EMenuIconSlot Slot)
	{
		if (Set == EButtonIconSet::Keyboard && Icon == "+" && Slot == EMenuIconSlot::FaceButton)
		{
			FCanvas Canvas(FaceSize, FaceSize);
			constexpr int32 Scale = 4;
			DrawText(Canvas, Icon, (FaceSize - 3 * Scale) / 2, (FaceSize - 5 * Scale) / 2, Scale, false, LabelWhite, SymbolShadow, 1);
			return std::move(Canvas.Picture);
		}
		if (Slot == EMenuIconSlot::Cluster)
		{
			if (const EFaceButtonPlace Place = FaceButtonPlace(Set, Icon); Place != EFaceButtonPlace::None)
			{
				FCanvas Canvas(FaceSize, FaceSize);
				DrawCluster(Canvas, Set, Place);
				return std::move(Canvas.Picture);
			}
			Slot = EMenuIconSlot::FaceButton;
		}
		if (Slot == EMenuIconSlot::Symbol)
		{
			// A face button's letter or symbol alone, as large as the slot takes it.
			if (const std::optional<std::pair<char, FColor5>> Bare = FaceLetter(Set, Icon))
			{
				constexpr int32 Scale = 3;
				FCanvas Canvas(FaceSize, FaceSize);
				const char Code[] = {Bare->first, 0};
				const int32 Left = (FaceSize - 5 * Scale) / 2;
				const int32 Top = (FaceSize - 7 * Scale) / 2;
				// Outlined all round, so the letter reads over whatever the battle shows behind it.
				for (int32 Dy = -1; Dy <= 1; ++Dy)
				{
					for (int32 Dx = -1; Dx <= 1; ++Dx)
					{
						DrawText(Canvas, Code, Left + Dx, Top + Dy, Scale, true, Outline, Outline, 0);
					}
				}
				DrawText(Canvas, Code, Left, Top, Scale, true, Bare->second, Outline, 0);
				return std::move(Canvas.Picture);
			}
			if (IsPlayStationSymbol(Set, Icon))
			{
				constexpr int32 Size = 22;
				FCanvas Canvas(FaceSize, FaceSize);
				DrawPlayStationSymbol(Canvas, Icon, (FaceSize - Size) / 2, (FaceSize - Size) / 2, Size, 2.2f);
				return std::move(Canvas.Picture);
			}
			Slot = EMenuIconSlot::FaceButton;
		}
		const bool bFace = Slot == EMenuIconSlot::FaceButton;
		const int32 SlotWidth = bFace ? FaceSize : HintWidth;
		const int32 SlotHeight = bFace ? FaceSize : HintHeight;
		if (Set == EButtonIconSet::Keyboard)
		{
			const std::string Label = MenuIconLabels::KeyLabel(Icon);
			FCanvas Canvas(WidthFor(Label, SlotWidth), SlotHeight);
			DrawKeyCap(Canvas);
			constexpr int32 Side = 4;
			DrawLabel(Canvas, Label, 1, SlotHeight - Side - 1, Canvas.Width() - 2 * LabelMargin - 2, KeyInk, KeyInk, false);
			return std::move(Canvas.Picture);
		}
		const std::optional<std::pair<char, FColor5>> Letter = FaceLetter(Set, Icon);
		const bool bSymbol = IsPlayStationSymbol(Set, Icon);
		const char Direction = PadDirection(Icon);
		const bool bViewOrMenu = IsViewOrMenu(Icon);
		const std::string Label = Letter || bSymbol || Direction != 0 || bViewOrMenu ? std::string() : MenuIconLabels::PadLabel(Icon);
		FCanvas Canvas(Label.empty() ? SlotWidth : WidthFor(Label, SlotWidth), SlotHeight);
		if (!bFace)
		{
			DrawBox(Canvas);
		}
		else if (Letter || bSymbol)
		{
			DrawRoundButton(Canvas);
		}
		else
		{
			DrawChip(Canvas);
		}
		if (Letter)
		{
			constexpr int32 LetterWidth = 5 * 2;
			constexpr int32 LetterHeight = 7 * 2;
			const char Code[] = {Letter->first, 0};
			DrawText(Canvas, Code, (Canvas.Width() - LetterWidth) / 2, (Canvas.Height() - LetterHeight) / 2, 2, true, Letter->second, SymbolShadow, 1);
		}
		else if (bSymbol)
		{
			const int32 Size = bFace ? 16 : 14;
			DrawPlayStationSymbol(Canvas, Icon, (Canvas.Width() - Size) / 2, (Canvas.Height() - Size) / 2, Size);
		}
		else if (Direction != 0)
		{
			const int32 Size = bFace ? 18 : 14;
			DrawDirectionalPad(Canvas, Direction, (Canvas.Width() - Size) / 2, (Canvas.Height() - Size) / 2, Size);
		}
		else if (bViewOrMenu)
		{
			const int32 Size = bFace ? 14 : 12;
			DrawViewOrMenu(Canvas, Icon, (Canvas.Width() - Size) / 2, (Canvas.Height() - Size) / 2, Size);
		}
		else
		{
			const int32 Rim = bFace ? 3 : MenuIconTexelsPerPixel;
			DrawLabel(Canvas, Label, Rim, SlotHeight - 2 * Rim, Canvas.Width() - 2 * LabelMargin - 2, LabelWhite, LabelShadow, true);
		}
		return std::move(Canvas.Picture);
	}
}
