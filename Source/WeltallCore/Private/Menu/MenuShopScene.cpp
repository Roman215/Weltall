#include "Menu/MenuShopModule.h"

namespace Weltall
{
	namespace
	{
		constexpr int32 FirstFacesFrame = 0x14E;
		constexpr int32 FaceY = 0xA6;
		/** Faces appear one character a frame: 11 frames. */
		constexpr int32 FaceSteps = 12;
		constexpr int32 FullScale = 0x1000;
		constexpr int32 PiecesEntry = 4;
		constexpr uint8 Blank = 0xFF;
		constexpr int32 DigitWidth = 8;
		constexpr int32 TotalShift = 3;
		constexpr uint8 TotalDrawn = 2;
		constexpr uint8 TotalShown = 1;
		/** The list's cursor: the arrow's five cels, one every six frames, beside its row. */
		constexpr int32 FirstCursorFrame = 0x15B;
		constexpr int32 LastCel = 4;
		constexpr uint8 CelFrames = 5;
		constexpr int32 CursorX = 0x1C;
		constexpr int32 RowsTop = 0x32;
		constexpr int32 RowHeight = 13;
		/** The scroll bar's knob: its frame, its place, its length and the track it moves along. */
		constexpr int32 KnobFrame = 0x107;
		constexpr int32 KnobX = 0xC;
		constexpr int32 KnobY = 0x32;
		constexpr int32 KnobWidth = 8;
		constexpr int32 KnobScrolling = 0x3C;
		constexpr int32 KnobFull = 100;
		constexpr int32 KnobTrack = 4000;
		constexpr int32 PageRows = 8;
		/** A row's name and number, typed into one image never cleared between rows, from (384, 128). */
		constexpr int16 StripX = 0x180;
		constexpr int16 StripY = 0x80;
		constexpr int16 StripColumn = 0x18;
		constexpr int16 StripWidth = 0x28;
		constexpr int32 StripBase = 0x80;
		constexpr int32 NameX = 0x24;
		constexpr int32 NumberX = 0x8C;
		constexpr uint16 FirstDigitGlyph = 0x10;
		constexpr uint16 BlankGlyph = 0xC3;
		/** The cart's mark and quantity beside a row. */
		constexpr int32 MarkX = 0xB4;
		constexpr int32 TensX = 0xBC;
		constexpr int32 OnesX = 0xC4;
		constexpr int32 MarkY = 0x36;
	}

	TMenuTask<> FMenuShopModule::RevealFaces(std::span<const int32> FaceX)
	{
		Scene.bShown = true;
		for (int32 Step = 1; Step < FaceSteps; ++Step)
		{
			Scene.Faces.clear();
			std::size_t Placed = 0;
			for (int32 Character = 0; Character < Step; ++Character)
			{
				if (Available[static_cast<std::size_t>(Character)])
				{
					Resources.Sheet().Draw(FirstFacesFrame + Character, FaceX[Placed], FaceY, FullScale, Scene.Faces);
					++Placed;
				}
			}
			co_await Frame();
		}
	}

	void FMenuShopModule::DrawStatics(std::span<const uint8> Frames, std::span<const int32> X, std::span<const int32> Y)
	{
		Scene.Statics.clear();
		for (std::size_t Static = 0; Static < Frames.size(); ++Static)
		{
			Resources.Sheet().Draw(Frames[Static], X[Static], Y[Static], FullScale, Scene.Statics);
		}
	}

	void FMenuShopModule::DrawDigitsInto(std::vector<FMenuPrimitive>& Digits, uint32 Value, int32 First, int32 Count, int32 X, int32 Y, bool bPacked)
	{
		SplitNumber(Value);
		Digits.clear();
		int32 Placed = 0;
		for (int32 Digit = 0; Digit < Count; ++Digit)
		{
			const uint8 Frame = NumberDigits[static_cast<std::size_t>(First + Digit)];
			if (Frame == Blank)
			{
				continue;
			}
			Resources.Sheet().Draw(Frame, X + (bPacked ? Placed : Digit) * DigitWidth, Y, FullScale, Digits);
			++Placed;
		}
	}

	void FMenuShopModule::DrawTotal(uint32 Total, int32 X, int32 Y)
	{
		DrawDigitsInto(Scene.Total, Total, 0, 9, X + TotalShift, Y, false);
		TotalState = TotalDrawn;
	}

	void FMenuShopModule::DrawListCursor(int32 Row)
	{
		if (++ListCursor.Frames > CelFrames)
		{
			if (--ListCursor.Cel < 0)
			{
				ListCursor.Cel = LastCel;
			}
			ListCursor.Frames = 0;
		}
		Scratch.clear();
		Resources.Sheet().Draw(FirstCursorFrame + ListCursor.Cel, 0, 0, FullScale, Scratch);
		if (!Scratch.empty())
		{
			ListCursor.Quad = Scratch.front();
			const std::array<FMenuCorner, 4>& At = ListCursor.Quad.Corners;
			SetLayerQuad(ListCursor.Quad.Points, At[0].X + CursorX, At[0].Y + Row * RowHeight + RowsTop, At[1].X - At[0].X, At[3].Y - At[0].Y);
		}
		bListCursorShown = true;
	}

	void FMenuShopModule::DrawScrollThumb(int32 Entries, int32 Top)
	{
		int32 Height = KnobScrolling;
		int32 Offset = 0;
		if (Entries <= PageRows)
		{
			Height = KnobFull;
		}
		else
		{
			Offset = Top * 100 / (Entries - PageRows) * KnobTrack / 10000;
		}
		Scratch.clear();
		Resources.Sheet().Draw(KnobFrame, KnobX, KnobY, FullScale, Scratch);
		if (!Scratch.empty())
		{
			ScrollThumb.Quad = Scratch.front();
		}
		SetLayerQuad(ScrollThumb.Quad.Points, KnobX, KnobY + Offset, KnobWidth, Height);
		bScrollThumbShown = true;
	}

	std::array<uint16, 5> FMenuShopModule::NumberGlyphs(uint32 Number)
	{
		std::array<uint16, 5> Glyphs{};
		uint32 Left = Number;
		bool bStarted = false;
		uint32 Divisor = 10000;
		for (std::size_t Digit = 0; Digit < 4; ++Digit, Divisor /= 10)
		{
			const uint32 Place = Left / Divisor;
			if (Place != 0 || bStarted)
			{
				Glyphs[Digit] = static_cast<uint16>(Place + FirstDigitGlyph);
				bStarted = true;
				Left -= Place * Divisor;
			}
			else
			{
				Glyphs[Digit] = BlankGlyph;
			}
		}
		Glyphs[4] = static_cast<uint16>(Left % 10 + FirstDigitGlyph);
		return Glyphs;
	}

	void FMenuShopModule::DrawRow(int32 Row, std::span<const uint8> Name, uint32 Number, uint8 Mode, uint8 NumberMode)
	{
		// The name in one plane, the number in the other; the image is loaded after each row, and again once the name is placed.
		FMenuShopRow& Shown = Scene.Rows[static_cast<std::size_t>(Row)];
		Shown.Name.Width = static_cast<uint8>(RenderString(NameTyping, NameImage, Name, 0));
		GlyphText(NumberGlyphs(Number), GlyphScratch);
		Shown.Number.Width = static_cast<uint8>(RenderString(NameTyping, NameImage, GlyphScratch, 1));
		NameImage.Rect = {static_cast<int16>(StripX + (Row & 1) * StripColumn), static_cast<int16>(StripY + Row / 2 * RowHeight), StripWidth, RowHeight};
		Upload(NameImage);
		const int32 Y = Row * RowHeight + RowsTop;
		SetStripLabelTexture(Shown.Name, Row, StripBase, Mode);
		SetLayerQuad(Shown.Name.Quad.Points, NameX, Y, Shown.Name.Width, RowHeight);
		Upload(NameImage);
		SetStripLabelTexture(Shown.Number, Row, StripBase, NumberMode);
		SetLayerQuad(Shown.Number.Quad.Points, NumberX, Y, Shown.Number.Width, RowHeight);
		// The name's drawn copy follows the buffer; the number's is left where it was.
		RecordLabel(Shown.Name);
		Shown.bShown = true;
	}

	void FMenuShopModule::DrawCartMark(int32 Row, int32 Mark, uint8 Quantity)
	{
		FMenuShopRow& Shown = Scene.Rows[static_cast<std::size_t>(Row)];
		// A pixel higher in the French translation's program.
		const int32 Y = Row * RowHeight + MarkY - (Resources.IsFrench() ? 1 : 0);
		Resources.Sheet().Draw(Mark, MarkX, Y, FullScale, Shown.Cart);
		if (Quantity / 10 != 0)
		{
			Resources.Sheet().Draw(Quantity / 10, TensX, Y, FullScale, Shown.Cart);
		}
		Resources.Sheet().Draw(Quantity % 10, OnesX, Y, FullScale, Shown.Cart);
	}

	void FMenuShopModule::AddScene()
	{
		if (Scene.bShown)
		{
			for (std::size_t Face = 0; Face < Scene.Outlined.size(); ++Face)
			{
				if (Scene.Outlined[Face])
				{
					Table.Add(PiecesEntry, Scene.OutlineTops[Face]);
					Table.Add(PiecesEntry, Scene.OutlineBottoms[Face]);
				}
			}
			AddScreen(Scene.Statics);
			AddScreen(Scene.Worn);
			AddScreen(Scene.Faces);
			for (FMenuShopRow& Row : Scene.Rows)
			{
				if (Row.bShown)
				{
					AddProjected(Row.Name.Quad, true, PiecesEntry);
					AddProjected(Row.Number.Quad, true, PiecesEntry);
				}
			}
			if (Scene.bEquippedShown)
			{
				AddProjected(Scene.Equipped.Quad, true, PiecesEntry);
			}
			if (Scene.bDetailShown)
			{
				AddProjected(Scene.Detail.Quad, true, PiecesEntry);
			}
			if (Scene.bSalePriceShown)
			{
				AddProjected(Scene.SalePrice.Quad, true, PiecesEntry);
			}
			if (Scene.bStoredShown)
			{
				AddProjected(Scene.Stored.Quad, true, PiecesEntry);
			}
			if (Scene.bGoldShown)
			{
				Table.Add(PiecesEntry, Scene.GoldLine);
				for (const std::vector<FMenuPrimitive>& Line : Scene.Gold)
				{
					AddScreen(Line);
				}
				if (Scene.bTradeInShown)
				{
					AddScreen(Scene.TradeIn);
				}
			}
			for (const FMenuShopRow& Row : Scene.Rows)
			{
				AddScreen(Row.Cart);
			}
			for (std::size_t Face = 0; Face < Scene.AttackChanges.size(); ++Face)
			{
				AddScreen(Scene.AttackChanges[Face]);
				AddScreen(Scene.DefenseChanges[Face]);
			}
		}
		if (TotalState == TotalShown)
		{
			AddScreen(Scene.Total);
		}
		if (Scene.bGoldPanelShown)
		{
			AddScreen(Scene.GoldPanel);
		}
	}

	void FMenuShopModule::ClearScene()
	{
		Scene.bShown = false;
		Scene.bEquippedShown = false;
		Scene.bDetailShown = false;
		Scene.bSalePriceShown = false;
		Scene.bGoldShown = false;
		Scene.Statics.clear();
		Scene.Worn.clear();
		Scene.Faces.clear();
		Scene.Outlined = {};
		for (std::size_t Face = 0; Face < Scene.AttackChanges.size(); ++Face)
		{
			Scene.AttackChanges[Face].clear();
			Scene.DefenseChanges[Face].clear();
		}
		for (FMenuShopRow& Row : Scene.Rows)
		{
			Row.bShown = false;
			Row.Cart.clear();
		}
	}
}
