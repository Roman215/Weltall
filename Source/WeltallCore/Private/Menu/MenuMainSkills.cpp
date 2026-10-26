#include "Foundation/LittleEndian.h"
#include "Game/CharacterSkills.h"
#include "Game/GameState.h"
#include "Menu/MenuMain.h"
#include "Menu/MenuResources.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** Chu-Chu and Maria have no Deathblows: the screen refuses them. */
		constexpr uint8 ChuChu = 7;
		constexpr uint8 Maria = 8;
		constexpr uint8 Buzzer = 4;
		constexpr int32 SkillsWindow = 2;
		constexpr int32 PiecesEntry = 4;
		constexpr int32 FullScale = 0x1000;
		/** The title label's place, and the rows'. */
		constexpr int16 TitleX = 0x4C;
		constexpr int16 TitleTop = 0x12;
		constexpr int16 TitleBottom = 0x1F;
		constexpr int32 SkillsLabelsBase = 4;
		constexpr int32 RowHeight = 13;
		constexpr int32 FirstRowY = 0x1F;
		constexpr int16 NameX = 0x5C;
		constexpr int16 CostX = 200;
		constexpr int16 PercentX = 0x10A;
		constexpr int32 ComboX = 0xD4;
		constexpr int32 ComboStep = 0x10;
		constexpr int32 ComboLength = 5;
		constexpr uint8 NoIcon = 0xFF;
		/** The learning bar: from x 212, 54 pixels at 100 %, green fading to black. */
		constexpr int16 BarX = 0xD4;
		constexpr int32 BarTop = 0x23;
		constexpr int32 BarBottom = 0x2B;
		constexpr uint32 BarLength = 0x1518;
		constexpr FDrawColor BarGreen{0, 0xFF, 0};
		constexpr FDrawColor Black{0, 0, 0};
		/** A Deathblow is shown once learned, or from half learned. */
		constexpr uint32 ShownPercent = 50;
		/** The rows' strips: two a row pair from (384, 128), in the page at (384, 128). */
		constexpr int16 StripX = 0x180;
		constexpr int16 StripY = 0x80;
		constexpr int16 StripColumn = 0x18;
		constexpr int16 StripWidth = 0x28;
		constexpr int32 StripBase = 0x80;
		constexpr uint8 NamePlane = 0x81;
		constexpr uint8 CostPlane = 0x82;
		/** A Deathblow's AP cost: its command record's byte, as a digit glyph; none for the upper tier or before it is learned. */
		constexpr std::size_t FirstDeathblowCommand = 0x12F;
		constexpr std::size_t CommandSize = 0x28;
		constexpr uint16 FirstDigitGlyph = 0x10;
		constexpr uint16 NoGlyph = 0x0F;
		constexpr uint16 OneGlyph = 0x11;
		/** The glyph the percentage has for its hundreds under 100. */
		constexpr uint16 PercentPadGlyph = 0xC3;
		/** The member's name, from the name strips, where the Skills screen puts it. */
		constexpr int32 SkillsNamePlace = 2;
		constexpr int32 NameWidth = 0x48;
		constexpr int32 GearNameWidth = 0x60;
		constexpr int32 FirstGearName = 11;
		constexpr int32 NamePageX = 0x180;
		constexpr int32 TextClutY = 0x1D1;
		constexpr int32 OddClutX = 16;
		/** The row kinds: hidden, learned (its combo), learning (its percentage and bar). */
		constexpr int32 Hidden = 0;
		constexpr int32 Learned = 1;
		constexpr int32 Learning = 2;

		bool HasNoDeathblows(uint8 Character)
		{
			return Character == ChuChu || Character == Maria;
		}

		void SetFlatQuad(FMenuPrimitive& Quad, int16 Left, int16 Top, int16 Right, int16 Bottom)
		{
			Quad.Corners[0].X = Left;
			Quad.Corners[0].Y = Top;
			Quad.Corners[1].X = Right;
			Quad.Corners[1].Y = Top;
			Quad.Corners[2].X = Left;
			Quad.Corners[2].Y = Bottom;
			Quad.Corners[3].X = Right;
			Quad.Corners[3].Y = Bottom;
		}
	}

	TMenuTask<> FMenuMain::SkillsScreen(int32 Slot)
	{
		if (HasNoDeathblows(Member(Slot)))
		{
			PlaySound(Buzzer);
			co_return;
		}
		OpenSkills();
		int32 DrawnSlot = 0xFF;
		bool bGoesOn = true;
		do
		{
			co_await Frame();
			if (Slot != DrawnSlot)
			{
				DrawDeathblows(Slot);
				DrawnSlot = Slot;
			}
			switch (Code)
			{
			case EMenuCode::R1:
				do
				{
					Slot = NextMember(Slot, false, false);
				} while (HasNoDeathblows(Member(Slot)));
				break;
			case EMenuCode::L1:
				do
				{
					Slot = NextMember(Slot, true, false);
				} while (HasNoDeathblows(Member(Slot)));
				break;
			case EMenuCode::Cancel:
				bGoesOn = false;
				break;
			default:
				break;
			}
		} while (bGoesOn);
		co_await CloseSkills();
	}

	void FMenuMain::OpenSkills()
	{
		const FMenuMainTables& Tables = Resources.Main();
		RenderLabels(SkillsLabels, Tables.SkillsTexts, SkillsLabelsBase);
		FMenuLabel& Title = SkillsLabels[0];
		SetFlatQuad(Title.Quad, TitleX, TitleTop, static_cast<int16>(TitleX + Title.Width), TitleBottom);
		Flags[MenuFlag::SkillsTitle] = 1;
		OpenWindow(SkillsWindow, 0x44, 10, 0xE4, 0xC4, false, true, PiecesEntry, false);
		// The play time and its window leave.
		Flags[MenuFlag::PlayTime] = 0;
		Flags[MenuFlag::Windows + 1] = 0;
		int32 WithDeathblows = 0;
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			if (Member(Slot) != FGameState::EmptySlot && !HasNoDeathblows(Member(Slot)))
			{
				++WithDeathblows;
			}
		}
		if (WithDeathblows < 2)
		{
			Flags[MenuFlag::MemberHints] = 0;
		}
		else
		{
			DrawMemberHints(3, false);
		}
		for (FMenuPrimitive& Bar : Deathblows.Bars)
		{
			Bar.Code = MenuGpu::GouraudQuad;
			Bar.Corners[0].Color = BarGreen;
			Bar.Corners[1].Color = BarGreen;
			Bar.Corners[2].Color = Black;
			Bar.Corners[3].Color = Black;
		}
	}

	TMenuTask<> FMenuMain::CloseSkills()
	{
		if (Flags[MenuFlag::MemberHintsMade] != 0)
		{
			Flags[MenuFlag::MemberHints] = 0;
			Flags[MenuFlag::MemberHintsMade] = 0;
		}
		Flags[MenuFlag::Deathblows] = 0;
		CloseWindow(SkillsWindow);
		co_await Frame();
		ClearFlags(MenuFlag::SkillsTitle, static_cast<int32>(SkillsLabels.size()));
	}

	void FMenuMain::DrawDeathblows(int32 Slot)
	{
		const FMenuMainTables& Tables = Resources.Main();
		const uint8 Character = Member(Slot);
		const std::span<const uint8> Commands = Resources.CharacterCommands(Character);
		// The rows are typed a pair of planes at a time into one buffer.
		std::fill(NameImage.Units.begin(), NameImage.Units.end(), uint16{0});
		for (int32 Row = 0; Row < CharacterSkills::DeathblowCount; ++Row)
		{
			const std::size_t At = static_cast<std::size_t>(Row);
			Deathblows.BarShown[At] = false;
			const uint16 Word = LittleEndian::ReadU16(Services.Game->Block(), 0x16C0 + static_cast<std::size_t>(Character) * 0x20);
			const bool bLearned = (Tables.SkillBits[At] & Word) != 0;
			uint32 Percent = 0;
			int32 Kind = Learned;
			if (!bLearned)
			{
				Percent = DeathblowPercent(*Services.Game, Character, Row, Resources.Growth());
				Kind = Percent >= ShownPercent ? Learning : Hidden;
			}
			if (Kind != Hidden)
			{
				FMenuLabel& Name = Deathblows.Names[At];
				FMenuLabel& Cost = Deathblows.Costs[At];
				Name.Width = static_cast<uint8>(RenderString(NameTyping, NameImage, Services.Text.Tables->String(Character, Row), 0));
				uint16 Glyph = NoGlyph;
				const std::size_t CostAt = FirstDeathblowCommand + At * CommandSize;
				if (Row < CharacterSkills::FirstUpperTier && bLearned && CostAt < Commands.size())
				{
					Glyph = static_cast<uint8>(Commands[CostAt] + FirstDigitGlyph);
				}
				GlyphText(std::span<const uint16>(&Glyph, 1), GlyphScratch);
				Cost.Width = static_cast<uint8>(RenderString(NameTyping, NameImage, GlyphScratch, 1));
				NameImage.Rect = {static_cast<int16>(StripX + (Row & 1) * StripColumn), static_cast<int16>(StripY + Row / 2 * RowHeight), StripWidth,
					static_cast<int16>(RowHeight)};
				Upload(NameImage);
				SetStripLabelTexture(Name, Row, StripBase, NamePlane);
				SetStripLabelTexture(Cost, Row, StripBase, CostPlane);
				const int16 Top = static_cast<int16>(FirstRowY + Row * RowHeight);
				const int16 Bottom = static_cast<int16>(Top + RowHeight);
				SetFlatQuad(Name.Quad, NameX, Top, static_cast<int16>(NameX + Name.Width), Bottom);
				SetFlatQuad(Cost.Quad, CostX, Top, static_cast<int16>(CostX + Cost.Width), Bottom);
				Deathblows.Shown[At] = true;
			}
			else
			{
				Deathblows.Shown[At] = false;
			}
			Deathblows.Combos[At].clear();
			DrawDeathblowRow(Row, Kind, Percent);
		}
		DrawMemberName(Deathblows.MemberName, Slot, SkillsNamePlace, false);
		Flags[MenuFlag::Deathblows] = 1;
	}

	void FMenuMain::DrawDeathblowRow(int32 Row, int32 Kind, uint32 Percent)
	{
		const std::size_t At = static_cast<std::size_t>(Row);
		const int32 Top = FirstRowY + Row * RowHeight;
		if (Kind == Learned)
		{
			const FMenuMainTables& Tables = Resources.Main();
			for (int32 Icon = 0; Icon < ComboLength; ++Icon)
			{
				const uint8 Frame = Tables.DeathblowCombos[At * ComboLength + static_cast<std::size_t>(Icon)];
				if (Frame != NoIcon)
				{
					Resources.Sheet().Draw(Frame, ComboX + Icon * ComboStep, Top, FullScale, Deathblows.Combos[At]);
				}
			}
			return;
		}
		if (Kind != Learning)
		{
			return;
		}
		// The percentage, three glyphs: the hundreds (1, or a pad under 100), the tens and the ones.
		const uint32 Whole = Percent;
		std::array<uint16, 3> Glyphs{};
		if (Percent / 100 == 0)
		{
			Glyphs[0] = PercentPadGlyph;
		}
		else
		{
			Glyphs[0] = OneGlyph;
			Percent -= 100;
		}
		Glyphs[1] = static_cast<uint16>(Percent / 10 + FirstDigitGlyph);
		Glyphs[2] = static_cast<uint16>(Percent % 10 + FirstDigitGlyph);
		GlyphText(Glyphs, GlyphScratch);
		FMenuLabel& Cost = Deathblows.Costs[At];
		Cost.Width = static_cast<uint8>(RenderString(NameTyping, NameImage, GlyphScratch, 1));
		NameImage.Rect = {static_cast<int16>(StripX + (Row & 1) * StripColumn), static_cast<int16>(StripY + Row / 2 * RowHeight), StripWidth,
			static_cast<int16>(RowHeight)};
		Upload(NameImage);
		SetFlatQuad(Cost.Quad, PercentX, static_cast<int16>(Top), static_cast<int16>(PercentX + Cost.Width), static_cast<int16>(Top + RowHeight));
		SetStripLabelTexture(Cost, Row, StripBase, CostPlane);
		FMenuPrimitive& Bar = Deathblows.Bars[At];
		// The original measures the bar after taking the hundreds off: at 100 % it is empty.
		const uint32 Measured = Services.Options.bDeathblowBarWhole ? Whole : Percent;
		const int16 Right = static_cast<int16>(BarX + Measured * BarLength / 10000);
		SetFlatQuad(Bar, BarX, static_cast<int16>(Row * RowHeight + BarTop), Right, static_cast<int16>(Row * RowHeight + BarBottom));
		Deathblows.BarShown[At] = true;
	}

	void FMenuMain::DrawMemberName(FMenuPrimitive& Name, int32 Slot, int32 Place, bool bGear)
	{
		// The member's name, or its gear's (wider, from the gears' strips), centred on the screen's place.
		const FMenuMainTables& Tables = Resources.Main();
		const uint8 Character = Member(Slot);
		const uint8 Gear = Services.Game->CharacterGear(Character).value_or(FGameState::EmptySlot);
		const int32 Name_ = bGear ? Gear + FirstGearName : Character;
		const std::size_t Strip = static_cast<std::size_t>(bGear ? 3 + Slot : Slot);
		const int32 Width = bGear ? GearNameWidth : NameWidth;
		Name.Code = MenuGpu::TexturedQuad;
		Name.Page = MenuGpu::TexturePage(0, 0, NamePageX, 0);
		Name.Clut = MenuGpu::Clut((Name_ & 1) != 0 ? OddClutX : 0, TextClutY);
		const uint8 U = static_cast<uint8>((Tables.NameStripX[Strip] & 0x3F) << 2);
		const uint8 V = static_cast<uint8>(Tables.NameStripY[Strip]);
		// The French translation's program centres a gear's name as if it were a member's.
		const int32 X = Tables.MemberNameX[static_cast<std::size_t>(Place)] - (Resources.IsFrench() ? NameWidth : Width) / 2;
		const int32 Y = Tables.MemberNameY[static_cast<std::size_t>(Place)];
		Name.Corners[0] = {static_cast<int16>(X), static_cast<int16>(Y), U, V, {0x80, 0x80, 0x80}};
		Name.Corners[1] = {static_cast<int16>(X + Width), static_cast<int16>(Y), static_cast<uint8>(U + Width), V, {}};
		Name.Corners[2] = {static_cast<int16>(X), static_cast<int16>(Y + RowHeight), U, static_cast<uint8>(V + RowHeight), {}};
		Name.Corners[3] = {static_cast<int16>(X + Width), static_cast<int16>(Y + RowHeight), static_cast<uint8>(U + Width),
			static_cast<uint8>(V + RowHeight), {}};
		SetLayerQuad(Name.Points, X, Y, Width, RowHeight);
	}

	void FMenuMain::AddDeathblows()
	{
		if (Flags[MenuFlag::Deathblows] == 0)
		{
			return;
		}
		for (std::size_t Row = 0; Row < Deathblows.Shown.size(); ++Row)
		{
			if (Deathblows.Shown[Row])
			{
				Table.Add(PiecesEntry, Deathblows.Names[Row].Quad);
				Table.Add(PiecesEntry, Deathblows.Costs[Row].Quad);
			}
		}
		AddProjected(Deathblows.MemberName, true, PiecesEntry);
		for (std::size_t Row = 0; Row < Deathblows.Shown.size(); ++Row)
		{
			AddScreen(Deathblows.Combos[Row]);
			if (Deathblows.BarShown[Row])
			{
				Table.Add(PiecesEntry, Deathblows.Bars[Row]);
			}
		}
	}

}
