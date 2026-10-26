#include "Foundation/LittleEndian.h"
#include "Menu/MenuMain.h"
#include "Text/MesFont.h"
#include "Text/MesGlyphs.h"

namespace Weltall
{
	namespace
	{
		constexpr uint8 Free = FMenuFileScreen::Free;
		constexpr int32 FullScale = 0x1000;
		constexpr int32 PiecesEntry = 4;
		constexpr uint8 Blank = 0xFF;
		constexpr int32 DigitWidth = 8;
		constexpr int32 NoLabel = 0xFFFF;
		/** Each member's details, 80 pixels right of the one before. */
		constexpr int32 MemberSpacing = 0x50;

		/** A save's summary in its data: HP and max HP (16 bits a member), EP, max EP and level (8), the party, the card slot. */
		namespace Summary
		{
			constexpr std::size_t PlayTime = 0x00;
			constexpr std::size_t Hp = 0x04;
			constexpr std::size_t MaxHp = 0x0A;
			constexpr std::size_t Ep = 0x10;
			constexpr std::size_t MaxEp = 0x13;
			constexpr std::size_t Level = 0x16;
			constexpr std::size_t Party = 0x1C;
			constexpr std::size_t CardSlot = 0x23;
			/** Every character's name, ten glyph indices of 16 bits each. */
			constexpr std::size_t Names = 0x24;
			constexpr std::size_t NameBytes = 0x14;
			/** The story's progress, event variable 0. */
			constexpr std::size_t StoryProgress = 0x1134;
		}
		constexpr std::size_t DataInHead = 0x100;

		/** The members' small portraits, by character from frame 0x14E. */
		constexpr int32 FirstFaceFrame = 0x14E;
		/** The details' numbers: level, ether level (split, never drawn), HP, max HP, EP, max EP. */
		constexpr std::size_t LevelNumber = 0;
		constexpr std::size_t HpNumber = 2;
		constexpr std::size_t MaxHpNumber = 3;
		constexpr std::size_t EpNumber = 4;
		constexpr std::size_t MaxEpNumber = 5;
		/** A name: rendered in plane 0 of a strip at (384 + x, y), drawn 72 x 13 seven rows under the labels. */
		constexpr int32 NamePageX = 0x180;
		constexpr int16 NameStripWidth = 0x28;
		constexpr int16 NameRows = 13;
		constexpr int32 NameWidth = 0x48;
		constexpr int32 NameBelowLabels = 7;
		constexpr int32 FirstDetailStrip = 6;
		constexpr int32 TextClutY = 0x1D1;
		/** The play time's colons and digits, and "No" with the slot's number. */
		constexpr int32 ColonFrame = 0xEE;
		constexpr int32 TimeY = 0x7A;
		constexpr int32 NumberLabelFrames[2] = {0x17, 0x32};
		constexpr int32 NumberLabelX[2] = {8, 0x10};
		constexpr int32 NumberLabelY = 0x66;
		constexpr int32 NumberDigitX[2] = {0x10, 0x18};
		constexpr int32 NumberDigitY = 0x6E;
		/** The time's slots: each colon takes two, a digit one. */
		constexpr std::size_t TimeSlots = 11;
		constexpr std::size_t ColonSlots = 2;
		constexpr std::size_t NumberSlots = 4;
		/** The place's 16 cells, each 12 x 16 of the title image's second line. */
		constexpr int32 PlaceCells = 16;
		constexpr int32 GlyphWidth = 12;
		constexpr int32 GlyphCell = 16;
		/** The title image: 256 x 32 pixels of 4 bits at (320, 224), two lines of 16 cells; white from the colours at (0, 448). */
		constexpr int16 TitleX = 0x140;
		constexpr int16 TitleY = 0xE0;
		constexpr int16 TitleUnits = 0x40;
		constexpr int16 TitleRows = 0x20;
		constexpr int32 TitleGlyphs = 32;
		constexpr std::size_t TitleBytes = 0x40;
		constexpr std::size_t TitleInHead = 4;
		constexpr int32 TitlePageX = 0x140;
		constexpr int32 TitlePageY = 0x80;
		constexpr int32 WhiteClutY = 0x1C0;
		constexpr uint16 Ink = 1;
		/** A glyph of the message font sits two rows into its 16-row cell. */
		constexpr int32 GlyphTop = 2;
		/**
		 * The French translation's program renders the save's chapter title with the message font as one line in the title
		 * image's second, 60 units wide, and shows it whole: its 16 cells 16 pixels wide, from a texel row higher.
		 */
		constexpr int16 FrenchTitleY = 0xF0;
		constexpr int16 FrenchTitleRows = 13;
		constexpr int32 FrenchCellTop = 0xEF;
		/** The band behind the details: purple to blue, darker at its foot. */
		constexpr int16 BandTop = 0x4A;
		constexpr int16 BandBottom = 0x8A;
		constexpr int16 BandRight = 0x140;
	}

	void FMenuMain::ShowFileDetails(uint8 File)
	{
		ClearFileTitle();
		if (File == Free)
		{
			return;
		}
		BuildSaveDetails(File);
		RenderFileTitle(File);
		Flags[MenuFlag::FileDetails] = 1;
	}

	void FMenuMain::BuildSaveDetails(int32 File)
	{
		const FMenuMainTables& Tables = Resources.Main();
		const FMenuSpriteSheet& Sheet = Resources.Sheet();
		const std::span<const uint8> Data = std::span<const uint8>(Files.Files[static_cast<std::size_t>(File)].Head).subspan(DataInHead);
		FMenuSaveDetails& Details = Files.Details;
		BuildMemberLabels();
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			const std::size_t At = static_cast<std::size_t>(Slot);
			const uint8 Character = Data[Summary::Party + At];
			Details.MemberShown[At] = Character != Blank;
			if (Character == Blank)
			{
				continue;
			}
			const int32 X = Slot * MemberSpacing;
			Details.Portraits[At].clear();
			Sheet.Draw(FirstFaceFrame + Character, Tables.DetailPortraitX[At], Tables.DetailPortraitY[At], FullScale, Details.Portraits[At]);
			const auto Place = [&Tables, X](std::size_t Number, int32 Axis) { return (Axis == 0 ? X : 0) + Tables.DetailNumbers[Number][static_cast<std::size_t>(Axis)]; };
			DrawDigits(Details.Levels[At], Data[Summary::Level + At], 6, 3, Place(LevelNumber, 0), Place(LevelNumber, 1), false);
			// The whole HP, where the original draws its last three digits.
			const uint32 Hp = LittleEndian::ReadU16(Data, Summary::Hp + At * 2);
			const uint32 MaxHp = LittleEndian::ReadU16(Data, Summary::MaxHp + At * 2);
			const int32 HpDigits = Hp > 999 ? 4 : 3;
			DrawDigits(Details.Hp[At], Hp, 9 - HpDigits, HpDigits, Place(HpNumber, 0) - (HpDigits - 3) * DigitWidth, Place(HpNumber, 1), false);
			DrawDigits(Details.MaxHp[At], MaxHp, MaxHp > 999 ? 5 : 6, MaxHp > 999 ? 4 : 3, Place(MaxHpNumber, 0), Place(MaxHpNumber, 1), true);
			DrawDigits(Details.Ep[At], Data[Summary::Ep + At], 7, 2, Place(EpNumber, 0), Place(EpNumber, 1), false);
			DrawDigits(Details.MaxEp[At], Data[Summary::MaxEp + At], 7, 2, Place(MaxEpNumber, 0), Place(MaxEpNumber, 1), true);
			RenderMemberName(Slot, Character, File);
		}
		BuildTimeAndNumber(Data);
		const bool bFrench = Resources.IsFrench();
		const int32 Width = bFrench ? GlyphCell : GlyphWidth;
		const uint8 CellTop = static_cast<uint8>(bFrench ? FrenchCellTop : 0xF0);
		for (int32 Cell = 0; Cell < PlaceCells; ++Cell)
		{
			FMenuPrimitive& Glyph = Details.Place[static_cast<std::size_t>(Cell)];
			Glyph = {};
			Glyph.Code = MenuGpu::TexturedQuad;
			Glyph.Page = MenuGpu::TexturePage(0, 0, TitlePageX, TitlePageY);
			Glyph.Clut = MenuGpu::Clut(0, WhiteClutY);
			const int16 Left = static_cast<int16>(Tables.PlaceCell[0] + Cell * Width);
			const int16 Top = static_cast<int16>(Tables.PlaceCell[1]);
			const uint8 U = static_cast<uint8>(Cell * GlyphCell);
			Glyph.Corners[0] = {Left, Top, U, CellTop, {0x80, 0x80, 0x80}};
			Glyph.Corners[1] = {static_cast<int16>(Left + Width), Top, static_cast<uint8>(U + Width), CellTop, {}};
			Glyph.Corners[2] = {Left, static_cast<int16>(Top + GlyphCell), U, 0xFF, {}};
			Glyph.Corners[3] = {static_cast<int16>(Left + Width), static_cast<int16>(Top + GlyphCell), static_cast<uint8>(U + Width), 0xFF, {}};
		}
	}

	void FMenuMain::BuildMemberLabels()
	{
		const FMenuMainTables& Tables = Resources.Main();
		FMenuSaveDetails& Details = Files.Details;
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			const std::size_t At = static_cast<std::size_t>(Slot);
			const int32 X = Slot * MemberSpacing;
			Details.Labels[At].clear();
			for (std::size_t Label = 0; Label < Tables.DetailLabels.size(); ++Label)
			{
				if (Tables.DetailLabels[Label] != NoLabel)
				{
					Resources.Sheet().Draw(Tables.DetailLabels[Label], X + Tables.DetailLabelX[Label], Tables.DetailLabelY[Label], FullScale, Details.Labels[At]);
				}
			}
			FMenuPrimitive& Name = Details.Names[At];
			Name = {};
			Name.Code = MenuGpu::TexturedQuad;
			Name.Page = MenuGpu::TexturePage(0, 0, NamePageX, 0);
			Name.Clut = MenuGpu::Clut(0, TextClutY);
			const std::size_t Strip = static_cast<std::size_t>(FirstDetailStrip + Slot);
			const int16 Left = static_cast<int16>(X + Tables.DetailLabelX[0]);
			const int16 Top = static_cast<int16>(Tables.DetailLabelY[0] + NameBelowLabels);
			const uint8 U = static_cast<uint8>((Tables.NameStripX[Strip] & 0x3F) << 2);
			const uint8 V = static_cast<uint8>(Tables.NameStripY[Strip]);
			Name.Corners[0] = {Left, Top, U, V, {0x80, 0x80, 0x80}};
			Name.Corners[1] = {static_cast<int16>(Left + NameWidth), Top, static_cast<uint8>(U + NameWidth), V, {}};
			Name.Corners[2] = {Left, static_cast<int16>(Top + NameRows), U, static_cast<uint8>(V + NameRows), {}};
			Name.Corners[3] = {static_cast<int16>(Left + NameWidth), static_cast<int16>(Top + NameRows), static_cast<uint8>(U + NameWidth),
				static_cast<uint8>(V + NameRows), {}};
		}
	}

	void FMenuMain::RenderMemberName(int32 Slot, uint8 Character, int32 File)
	{
		// The name the save keeps for the character, up to ten glyphs, ended by a zero one.
		const std::span<const uint8> Head(Files.Files[static_cast<std::size_t>(File)].Head);
		const std::size_t First = DataInHead + Summary::Names + static_cast<std::size_t>(Character) * Summary::NameBytes;
		std::vector<uint16> Glyphs;
		for (std::size_t Glyph = 0; Glyph < Summary::NameBytes / 2 && First + Glyph * 2 + 1 < Head.size(); ++Glyph)
		{
			const uint16 Index = LittleEndian::ReadU16(Head, First + Glyph * 2);
			if (Index == 0)
			{
				break;
			}
			Glyphs.push_back(Index);
		}
		GlyphText(Glyphs, GlyphScratch);
		const FMenuMainTables& Tables = Resources.Main();
		const std::size_t Strip = static_cast<std::size_t>(FirstDetailStrip + Slot);
		DetailNameImage.Rect = {static_cast<int16>(NamePageX + Tables.NameStripX[Strip]), static_cast<int16>(Tables.NameStripY[Strip]), NameStripWidth, NameRows};
		DetailNameImage.Units.assign(static_cast<std::size_t>(NameStripWidth * NameRows), 0);
		RenderString(NameTyping, DetailNameImage, GlyphScratch, 0);
		Upload(DetailNameImage);
	}

	void FMenuMain::BuildTimeAndNumber(std::span<const uint8> Data)
	{
		const FMenuMainTables& Tables = Resources.Main();
		const FMenuSpriteSheet& Sheet = Resources.Sheet();
		FMenuSaveDetails& Details = Files.Details;
		std::array<FMenuPrimitive, TimeSlots> Slots{};
		std::vector<FMenuPrimitive> Parts;
		const auto Place = [&Slots, &Parts](std::size_t First) {
			for (std::size_t Part = 0; Part < Parts.size() && First + Part < Slots.size(); ++Part)
			{
				Slots[First + Part] = Parts[Part];
			}
			Parts.clear();
		};
		Sheet.Draw(ColonFrame, Tables.DetailColonX[0], TimeY, FullScale, Parts);
		Place(0);
		Sheet.Draw(ColonFrame, Tables.DetailColonX[1], TimeY, FullScale, Parts);
		Place(ColonSlots);
		const std::array<uint32, 7> Digits = SplitPlayTime(LittleEndian::ReadU32(Data, Summary::PlayTime));
		for (std::size_t Digit = 0; Digit < Digits.size(); ++Digit)
		{
			Sheet.Draw(static_cast<int32>(Digits[Digit]), Tables.DetailTimeX[Digit], TimeY, FullScale, Parts);
			Place(2 * ColonSlots + Digit);
		}
		Details.Time.assign(Slots.begin(), Slots.end());
		// "No" and the slot's number, a slot each.
		const int32 Number = Data[Summary::CardSlot] + 1;
		Slots = {};
		Sheet.Draw(NumberLabelFrames[0], NumberLabelX[0], NumberLabelY, FullScale, Parts);
		Place(0);
		Sheet.Draw(NumberLabelFrames[1], NumberLabelX[1], NumberLabelY, FullScale, Parts);
		Place(1);
		Sheet.Draw(Number / 10, NumberDigitX[0], NumberDigitY, FullScale, Parts);
		Place(2);
		Sheet.Draw(Number % 10, NumberDigitX[1], NumberDigitY, FullScale, Parts);
		Place(3);
		Details.Number.assign(Slots.begin(), Slots.begin() + NumberSlots);
	}

	void FMenuMain::InitDetailsBand()
	{
		FMenuPrimitive& Band = Files.Details.Band;
		Band = {};
		Band.Code = MenuGpu::GouraudQuad;
		Band.Corners[0] = {0, BandTop, 0, 0, {0x80, 0, 0x80}};
		Band.Corners[1] = {BandRight, BandTop, 0, 0, {0, 0, 0x80}};
		Band.Corners[2] = {0, BandBottom, 0, 0, {0x10, 0, 0x10}};
		Band.Corners[3] = {BandRight, BandBottom, 0, 0, {0, 0, 0x10}};
	}

	void FMenuMain::ClearFileTitle()
	{
		TitleImage.Rect = {TitleX, TitleY, TitleUnits, TitleRows};
		TitleImage.Units.assign(static_cast<std::size_t>(TitleUnits * TitleRows), 0);
		MenuVram.Upload(TitleImage);
		Flags[MenuFlag::FileDetails] = 0;
	}

	void FMenuMain::RenderFileTitle(int32 File)
	{
		if (Resources.IsFrench())
		{
			RenderFrenchFileTitle(File);
			return;
		}
		// The card title in the game's own font (the console draws it from the BIOS's font, which the port has
		// not), a glyph in each 16-pixel cell, 16 to a line.
		const std::span<const uint8> Head(Files.Files[static_cast<std::size_t>(File)].Head);
		TitleImage.Rect = {TitleX, TitleY, TitleUnits, TitleRows};
		TitleImage.Units.assign(static_cast<std::size_t>(TitleUnits * TitleRows), 0);
		std::size_t At = TitleInHead;
		for (int32 Place = 0; Place < TitleGlyphs && At < TitleInHead + TitleBytes && Head[At] != 0; ++Place)
		{
			const bool bWide = Head[At] >= 0x80;
			const std::optional<FMesCode> Glyph = bWide ? MesCodeOfFullWidth(Head[At], Head[At + 1]) : std::nullopt;
			At += bWide ? 2 : 1;
			if (!Glyph)
			{
				continue;
			}
			const FMesFont::FCell Cell = Services.Text.Font->Cell(*Glyph);
			const int32 Left = Place % 16 * GlyphCell;
			const int32 Top = Place / 16 * GlyphCell + GlyphTop;
			for (int32 Row = 0; Row < FMesFont::CellHeight; ++Row)
			{
				for (int32 Column = 0; Column < FMesFont::CellWidth; ++Column)
				{
					// The glyph's body (1, or 3 where it borders another body pixel), not its outline.
					if ((Cell[static_cast<std::size_t>(Row * FMesFont::CellWidth + Column)] & 1) == 0)
					{
						continue;
					}
					const int32 X = Left + Column;
					uint16& Unit = TitleImage.Units[static_cast<std::size_t>((Top + Row) * TitleUnits + X / 4)];
					Unit = static_cast<uint16>(Unit | Ink << (X % 4 * 4));
				}
			}
		}
		Upload(TitleImage);
	}

	void FMenuMain::DrawFileDetails()
	{
		if (Flags[MenuFlag::FileDetails] == 0)
		{
			return;
		}
		FMenuSaveDetails& Details = Files.Details;
		for (std::size_t Slot = 0; Slot < 3; ++Slot)
		{
			if (!Details.MemberShown[Slot])
			{
				continue;
			}
			AddScreen(Details.Labels[Slot]);
			AddScreen(Details.Levels[Slot]);
			AddScreen(Details.Hp[Slot]);
			AddScreen(Details.MaxHp[Slot]);
			AddScreen(Details.Ep[Slot]);
			AddScreen(Details.MaxEp[Slot]);
			if (!Details.Portraits[Slot].empty())
			{
				Table.Add(PiecesEntry, Details.Portraits[Slot].front());
			}
			Table.Add(PiecesEntry, Details.Names[Slot]);
		}
		AddScreen(Details.Time);
		AddScreen(Details.Number);
		for (const FMenuPrimitive& Glyph : Details.Place)
		{
			Table.Add(PiecesEntry, Glyph);
		}
		Table.Add(PiecesEntry, Details.Band);
	}

	void FMenuMain::RenderFrenchFileTitle(int32 File)
	{
		const std::vector<uint8>& Data = Files.Files[static_cast<std::size_t>(File)].Data;
		const uint16 Progress = Data.size() >= Summary::StoryProgress + 2 ? LittleEndian::ReadU16(Data, Summary::StoryProgress) : uint16{0};
		const std::span<const uint8> Title = Resources.FrenchChapterTitle(Progress);
		FVramImage Line{{TitleX, FrenchTitleY, TitleUnits, FrenchTitleRows}, std::vector<uint16>(static_cast<std::size_t>(TitleUnits * FrenchTitleRows), 0)};
		if (!Title.empty())
		{
			RenderString(FrenchTitleTyping, Line, Title, 0);
		}
		Upload(Line);
	}
}
