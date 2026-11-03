#include "Game/MessageLogScreen.h"

#include "Menu/OptionsText.h"
#include "Text/MesFont.h"
#include "Text/MesGlyphs.h"
#include "Text/MesRender.h"
#include "Text/MessageLog.h"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace Weltall
{
	namespace
	{
		/**
		 * The pictures: the shown lines typed as the kernel types a window's, two lines a strip of 13 rows (one in each half
		 * of the 4-bit texels), from VRAM's corner; a line wider than a page's 256 texels goes on in the page beside it. The
		 * palettes for even and odd lines below them.
		 */
		constexpr int32 ImagePitch = 80;
		constexpr int32 PageUnits = 64;
		constexpr int32 PagePixels = 256;
		constexpr int32 MostShownLines = 18;
		constexpr int32 StripRows = FMesFont::CellHeight;
		constexpr int32 ClutY = 496;
		constexpr int32 OddClutX = 16;
		/** Glyphs past the image's width are never typed: a glyph's cell is three units. */
		constexpr int32 GlyphUnits = 3;

		/** The scroll bar, at the text's right; its knob's least height. */
		constexpr FScreenRect ScrollTrack{306, FMessageLogScreen::TextArea.Y, 2, FMessageLogScreen::TextArea.Height};
		constexpr int32 SmallestKnob = 8;
		constexpr FDrawColor TrackColor{24, 24, 28};
		constexpr FDrawColor KnobColor{112, 112, 120};
		/** A speaker's name as the line above a quoted speech, drawn in a warmer white than the speech. */
		constexpr FDrawColor SpeakerTint{140, 126, 84};
		constexpr std::size_t LongestSpeaker = 24;
		constexpr FDrawColor Neutral{};
		/** A clip far past any display's edge, on the 320 x 224 screen's scale: a fill spanning the display reaches all of it. */
		constexpr int16 Beyond = 8192;
		constexpr FScreenRect WholeDisplay{-Beyond, -Beyond, 320 + 2 * Beyond, 224 + 2 * Beyond};

		/** How quickly the shown scroll comes to where it is scrolled: most of the way in a tenth of a second. */
		constexpr double GlideRate = 24.0;

		constexpr uint16 UpBit = FPadState::Bit(EPadButton::Up);
		constexpr uint16 DownBit = FPadState::Bit(EPadButton::Down);
		constexpr uint16 PageUpBits = FPadState::Bit(EPadButton::L1) | FPadState::Bit(EPadButton::Left);
		constexpr uint16 PageDownBits = FPadState::Bit(EPadButton::R1) | FPadState::Bit(EPadButton::Right);
		constexpr uint16 CloseBits = FPadState::Bit(EPadButton::Cross) | FPadState::Bit(EPadButton::Circle);

		FDrawPrimitive Fill(const FScreenRect& Rect, FDrawColor Color, EBlendMode Blend)
		{
			FDrawPrimitive Primitive = MakeGouraudRect(Rect, {Color, Color, Color, Color});
			Primitive.Blend = EDrawBlend::SemiTransparent;
			Primitive.UntexturedBlend = Blend;
			return Primitive;
		}

		int32 LinePixels(const FMesFont& Font, std::span<const FMesPlacedGlyph> Line)
		{
			int32 Units = 0;
			for (const FMesPlacedGlyph& Glyph : Line)
			{
				Units = std::max(Units, Glyph.XUnits + Font.WidthUnits(Glyph.Code));
			}
			return Units * Font.UnitPixels();
		}

		/** A line of plain text as the font types it on a line of its own. */
		std::vector<FMesPlacedGlyph> PlainLine(const FMesFont& Font, std::string_view Text)
		{
			std::vector<FMesPlacedGlyph> Line;
			int16 X = 0;
			for (const char Character : Text)
			{
				const FMesCode Code = MesCodeOfCharacter(Character).value_or(*MesCodeOfCharacter(' '));
				Line.push_back({Code, X, 0});
				X = static_cast<int16>(X + Font.WidthUnits(Code));
			}
			return Line;
		}
	}

	FMessageLogScreen::FMessageLogScreen(const FMessageLog& InLog, const FMesFont& InFont, std::span<const uint16, 32> TextClut, std::array<uint8, 3> InWindowColour)
		: Font(InFont), WindowColour(InWindowColour), Image(static_cast<std::size_t>(ImagePitch * StripRows * MostShownLines / 2), 0)
	{
		Messages = &InLog.Messages();
		if (Messages->empty())
		{
			Placeholder.emplace_back().Lines.push_back(PlainLine(Font, OptionsText::MessageLog::Empty));
			Messages = &Placeholder;
		}
		for (std::size_t Message = 0; Message < Messages->size(); ++Message)
		{
			if (!Rows.empty())
			{
				Rows.push_back({static_cast<int32>(Message) - 1, -1, ContentHeight, MessageGap});
				ContentHeight += MessageGap;
			}
			for (std::size_t Line = 0; Line < (*Messages)[Message].Lines.size(); ++Line)
			{
				Rows.push_back({static_cast<int32>(Message), static_cast<int32>(Line), ContentHeight, LineSpacing});
				ContentHeight += LineSpacing;
			}
		}
		PictureVram.Upload({{0, ClutY, 32, 1}, std::vector<uint16>(TextClut.begin(), TextClut.end())});
	}

	int32 FMessageLogScreen::MostScroll() const
	{
		return std::max(0, ContentHeight - TextArea.Height);
	}

	int32 FMessageLogScreen::LineUp(int32 Scrolled) const
	{
		// The top of the view, counted from the log's top: the line starting above it comes whole into view.
		const int32 ViewTop = ContentHeight - TextArea.Height - Scrolled;
		for (auto Row = Rows.rbegin(); Row != Rows.rend(); ++Row)
		{
			if (Row->Line >= 0 && Row->Top < ViewTop)
			{
				return std::min(MostScroll(), ContentHeight - TextArea.Height - Row->Top);
			}
		}
		return MostScroll();
	}

	int32 FMessageLogScreen::LineDown(int32 Scrolled) const
	{
		const int32 ViewBottom = ContentHeight - Scrolled;
		for (const FRow& Row : Rows)
		{
			if (Row.Line >= 0 && Row.Top + Row.Height > ViewBottom)
			{
				return std::max(0, ContentHeight - Row.Top - Row.Height);
			}
		}
		return 0;
	}

	bool FMessageLogScreen::Update(double Seconds, FPadState Held)
	{
		const uint16 Buttons = Held.Buttons;
		// What was held as the screen opened counts once it has been let go.
		const uint16 Pressed = bFirstFrame ? 0 : static_cast<uint16>(Buttons & ~Previous);
		bFirstFrame = false;
		Previous = Buttons;
		if ((Pressed & CloseBits) != 0)
		{
			return false;
		}
		const int32 Direction = (Buttons & UpBit) != 0 ? -1 : (Buttons & DownBit) != 0 ? 1 : 0;
		if (Direction == 0)
		{
			Repeating = 0;
		}
		else if ((Pressed & (Direction < 0 ? UpBit : DownBit)) != 0 || Direction != Repeating)
		{
			Scroll(-Direction);
			Repeating = Direction;
			RepeatIn = RepeatDelaySeconds;
		}
		else
		{
			for (RepeatIn -= Seconds; RepeatIn <= 0.0; RepeatIn += RepeatSeconds)
			{
				Scroll(-Direction);
			}
		}
		if ((Pressed & PageUpBits) != 0)
		{
			// A page less a line, so its top line stays in view at the bottom.
			int32 Scrolled = Target;
			for (int32 Line = 0; Line < TextArea.Height / LineSpacing - 1; ++Line)
			{
				Scrolled = LineUp(Scrolled);
			}
			Target = Scrolled;
		}
		if ((Pressed & PageDownBits) != 0)
		{
			int32 Scrolled = Target;
			for (int32 Line = 0; Line < TextArea.Height / LineSpacing - 1; ++Line)
			{
				Scrolled = LineDown(Scrolled);
			}
			Target = Scrolled;
		}
		const float Gap = static_cast<float>(Target) - ShownScroll;
		ShownScroll = std::abs(Gap) < 0.5f ? static_cast<float>(Target) : ShownScroll + Gap * static_cast<float>(1.0 - std::exp(-Seconds * GlideRate));
		return true;
	}

	void FMessageLogScreen::Scroll(int32 Lines)
	{
		for (int32 Line = 0; Line < std::abs(Lines); ++Line)
		{
			Target = Lines > 0 ? LineUp(Target) : LineDown(Target);
		}
	}

	bool FMessageLogScreen::IsAtNewest() const
	{
		return Target == 0;
	}

	float FMessageLogScreen::ContentTop() const
	{
		// A log shorter than the area stands at its top; a longer one is scrolled up from its end at the bottom.
		if (ContentHeight <= TextArea.Height)
		{
			return TextArea.Y;
		}
		return static_cast<float>(TextArea.Y + TextArea.Height - ContentHeight) + ShownScroll;
	}

	bool FMessageLogScreen::IsSpeaker(int32 Message, int32 Line) const
	{
		// As the English script writes a speaker: a short line of a name, above the speech's opening quote.
		const FLoggedMessage& Logged = (*Messages)[static_cast<std::size_t>(Message)];
		if (Line != 0 || Logged.Lines.size() < 2 || Logged.Lines[0].empty() || Logged.Lines[0].size() > LongestSpeaker || Logged.Lines[1].empty())
		{
			return false;
		}
		const std::optional<FMesCode> Quote = MesCodeOfCharacter('"');
		if (!Quote || !(Logged.Lines[1].front().Code == *Quote))
		{
			return false;
		}
		return std::none_of(Logged.Lines[0].begin(), Logged.Lines[0].end(), [&Quote](const FMesPlacedGlyph& Glyph) { return Glyph.Code == *Quote; });
	}

	void FMessageLogScreen::TypeRows(std::span<const int32> Shown)
	{
		if (std::equal(Shown.begin(), Shown.end(), Typed.begin(), Typed.end()))
		{
			return;
		}
		Typed.assign(Shown.begin(), Shown.end());
		std::fill(Image.begin(), Image.end(), uint16{0});
		const std::span<uint16> Units(Image);
		for (std::size_t Slot = 0; Slot < Shown.size(); ++Slot)
		{
			const FRow& Row = Rows[static_cast<std::size_t>(Shown[Slot])];
			const std::span<uint16> Strip = Units.subspan(Slot / 2 * ImagePitch * StripRows);
			for (const FMesPlacedGlyph& Glyph : (*Messages)[static_cast<std::size_t>(Row.Message)].Lines[static_cast<std::size_t>(Row.Line)])
			{
				if (Glyph.XUnits * Font.UnitPixels() / 4 + GlyphUnits <= ImagePitch)
				{
					DrawMesGlyph(&Font, Glyph.Code, Strip, ImagePitch, Glyph.XUnits, static_cast<int32>(Slot & 1), Glyph.StepBudget);
				}
			}
		}
		PictureVram.Upload({{0, 0, ImagePitch, StripRows * MostShownLines / 2}, Image});
	}

	void FMessageLogScreen::Build(FDrawList& Out, std::vector<FTextButtonCell>& Cells)
	{
		Out.Primitives.clear();
		Cells.clear();
		// The whole display darkened twice as a window darkens what lies behind it, so a window's own text behind fades
		// under the log's; then the text over it.
		FDrawPrimitive Dark = Fill({0, 0, 320, 224}, {WindowColour[0], WindowColour[1], WindowColour[2]}, EBlendMode::Subtract);
		Dark.bSpansDisplay = true;
		Dark.Clip = WholeDisplay;
		Out.Primitives.push_back(Dark);
		Out.Primitives.push_back(Dark);

		const float Top = ContentTop();
		std::vector<int32> Shown;
		for (std::size_t Row = 0; Row < Rows.size() && static_cast<int32>(Shown.size()) < MostShownLines; ++Row)
		{
			const float RowTop = Top + static_cast<float>(Rows[Row].Top);
			if (Rows[Row].Line >= 0 && RowTop + LineSpacing > TextArea.Y && RowTop < TextArea.Y + TextArea.Height)
			{
				Shown.push_back(static_cast<int32>(Row));
			}
		}
		TypeRows(Shown);
		for (std::size_t Slot = 0; Slot < Shown.size(); ++Slot)
		{
			const FRow& Row = Rows[static_cast<std::size_t>(Shown[Slot])];
			const FLoggedMessage& Message = (*Messages)[static_cast<std::size_t>(Row.Message)];
			const std::vector<FMesPlacedGlyph>& Line = Message.Lines[static_cast<std::size_t>(Row.Line)];
			const std::span<const FMesPlacedGlyph> Next = static_cast<std::size_t>(Row.Line) + 1 < Message.Lines.size()
				? std::span<const FMesPlacedGlyph>(Message.Lines[static_cast<std::size_t>(Row.Line) + 1]) : std::span<const FMesPlacedGlyph>();
			const int32 Width = std::min(LinePixels(Font, Line), ImagePitch * 4);
			const bool bChosen = Message.Chosen == Row.Line;
			const FDrawColor Tint = bChosen ? FDrawColor{Message.ChosenColor[0], Message.ChosenColor[1], Message.ChosenColor[2]}
				: IsSpeaker(Row.Message, Row.Line) ? SpeakerTint : Neutral;
			const float Y = Top + static_cast<float>(Row.Top);
			const std::size_t FirstCell = Cells.size();
			for (int32 Half = 0; Half < 2; ++Half)
			{
				const int32 Pixels = Half == 0 ? std::min(Width, PagePixels) : Width - PagePixels;
				if (Pixels <= 0)
				{
					continue;
				}
				const FTexturePage Page{Half * PageUnits, 0, ETextureDepth::Indexed4, EBlendMode::Average};
				const FClutPosition Clut{(Slot & 1) != 0 ? OddClutX : 0, ClutY};
				FDrawPrimitive Sprite = MakeSprite(TextArea.X + Half * PagePixels, 0, Pixels, StripRows, {0, Page, Clut}, 0,
					static_cast<uint8>(Slot / 2 * StripRows), Tint);
				for (FDrawVertex& Corner : Sprite.Corners)
				{
					Corner.Y += Y;
				}
				Sprite.bModulate = !(Tint == Neutral);
				Sprite.Clip = TextArea;
				Out.Primitives.push_back(Sprite);
				ButtonFinder.AddKept(Line, Next, Half * PagePixels, Pixels, &Font, static_cast<uint32>(Out.Primitives.size() - 1), Cells);
			}
			for (std::size_t Cell = FirstCell; Cell < Cells.size(); ++Cell)
			{
				Cells[Cell].LineEnd = static_cast<uint32>(Out.Primitives.size());
			}
		}

		if (ContentHeight > TextArea.Height)
		{
			Out.Primitives.push_back(Fill(ScrollTrack, TrackColor, EBlendMode::Add));
			const int32 Knob = std::max(SmallestKnob, ScrollTrack.Height * TextArea.Height / ContentHeight);
			const float Along = 1.0f - ShownScroll / static_cast<float>(MostScroll());
			FDrawPrimitive Thumb = Fill({ScrollTrack.X, 0, ScrollTrack.Width, static_cast<int16>(Knob)}, KnobColor, EBlendMode::Add);
			for (FDrawVertex& Corner : Thumb.Corners)
			{
				Corner.Y += static_cast<float>(ScrollTrack.Y) + Along * static_cast<float>(ScrollTrack.Height - Knob);
			}
			Out.Primitives.push_back(Thumb);
		}
	}
}
