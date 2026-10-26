#include "Foundation/LittleEndian.h"
#include "Game/EtherHealing.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Menu/MenuMain.h"
#include "Menu/MenuResources.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		using namespace GameStateLayout;
		using LittleEndian::ReadU16;

		constexpr int32 Entries = 12;
		/** Entries 12 and 13 are the EP (a gear's own abilities: the fuel) and its maximum. */
		constexpr int32 PointsEntry = 12;
		constexpr int32 MaxPointsEntry = 13;
		constexpr int32 LastEntry = Entries - 1;
		/** The command records: a character's Ethers from its 23rd, a gear's list from its 22nd, its own from its 38th. */
		constexpr std::size_t CommandSize = 0x28;
		constexpr std::size_t FirstEther = 0x370;
		constexpr std::size_t FirstGearEther = 0x348;
		constexpr std::size_t FirstGearOwn = 0x5C8;
		constexpr std::size_t EpCost = 0x13;
		constexpr std::size_t FuelCost = 0x24;
		/** The skills' words: a character's abilities, its gear's ether commands, the gear's special commands. */
		constexpr std::size_t AbilitiesWord = 0x02;
		constexpr std::size_t GearEtherWord = 0x06;
		constexpr std::size_t SpecialWord = 0x1A;
		/** The kernel's name tables: the Ethers (16 a character), a gear's list (16 a gear), a gear's own (4 a gear). */
		constexpr int32 EtherNames = 20;
		constexpr int32 GearEtherNames = 48;
		constexpr int32 GearOwnNames = 52;
		/** The list's rows: an entry's name, two columns 136 apart, 16 rows apart. */
		constexpr int32 ColumnStep = 0x88;
		constexpr int32 NameX = 0x24;
		constexpr int32 RowY = 0x12;
		constexpr int32 RowHeight = 13;
		constexpr int16 StripX = 0x180;
		constexpr int16 StripY = 0x80;
		constexpr int16 StripColumn = 0x18;
		constexpr int16 StripWidth = 0x28;
		constexpr int32 StripBase = 0x80;
		constexpr uint16 FirstDigitGlyph = 0x10;
		constexpr uint16 BlankGlyph = 0xC3;
		/** The two lines of the highlighted entry, in their strip at (320, 78), and its name again. */
		constexpr int32 HelpLineX = 0x1C;
		constexpr std::array<int32, 2> HelpLineY = {0x9E, 0xAE};
		constexpr int16 HelpStripX = 0x140;
		constexpr int16 HelpStripY = 0x4E;
		constexpr int16 HelpStripWidth = 0x3C;
		constexpr int32 HelpNameX = 0x12;
		constexpr int32 HelpNameY = 0x8E;
		constexpr uint8 Bright = 0x80;
		constexpr int32 PiecesEntry = 4;
		/** The tags: 6 "EP:" (a gear's own: "Fuel:") and 7 "/", beside the points. */
		constexpr int32 PointsTag = 6;
		constexpr int32 PointsTagCount = 2;
		constexpr int32 GearTags = 8;
	}

	std::span<const uint8> FMenuMain::AbilityRecord(int32 Slot, int32 Index, int32 Kind) const
	{
		const uint8 Character = Member(Slot);
		const uint8 Gear = Services.Game->CharacterGear(Character).value_or(0);
		const std::span<const uint8> Commands = Kind == 0 ? Resources.CharacterCommands(Character) : Resources.GearCommands(Gear);
		const std::size_t First = Kind == 0 ? FirstEther : (Kind == 1 ? FirstGearEther : FirstGearOwn);
		const std::size_t At = First + static_cast<std::size_t>(Index) * CommandSize;
		return At + CommandSize <= Commands.size() ? Commands.subspan(At, CommandSize) : std::span<const uint8>{};
	}

	int32 FMenuMain::AbilityCost(int32 Slot, int32 Index, int32 Kind) const
	{
		const std::span<const uint8> Record = AbilityRecord(Slot, Kind == 2 ? Index / 2 : Index, Kind);
		if (Record.empty())
		{
			return 0;
		}
		if (Kind == 2)
		{
			return GearOwnCost(Slot, Index / 2);
		}
		const uint8 Cost = Record[EpCost];
		if (!Services.Options.bEtherHealsAsBattle)
		{
			return Cost;
		}
		const std::span<const uint8> Caster = Services.Game->Block().subspan(CharacterOffset(Member(Slot)), CharacterSize);
		return EtherCost(Cost, ReadU16(Caster, 0x8A), ReadU16(Caster, 0x32));
	}

	int32 FMenuMain::GearOwnCost(int32 Slot, int32 Ability) const
	{
		// The gear's first ability, its repair, costs fuel by the gear's size: a ninth of a fifth of its
		// maximum HP, in tens; the others as their records say.
		const uint8 Gear = Services.Game->CharacterGear(Member(Slot)).value_or(0);
		if (Ability == 0)
		{
			const uint32 MaxHp = LittleEndian::ReadU32(Services.Game->Block(), GearOffset(Gear) + GameStateLayout::Gear::MaxHp);
			const uint16 Cost = static_cast<uint16>((MaxHp / 10 << 1) / 9);
			return Cost / 10 * 10;
		}
		const std::span<const uint8> Record = AbilityRecord(Slot, Ability, 2);
		return Record.empty() ? 0 : ReadU16(Record, FuelCost);
	}

	void FMenuMain::OpenAbilities(int32 Kind)
	{
		AbilityList = {};
		RenderLabels(HelpTags, std::span<const uint8>(Resources.Main().LabelSets).subspan(Kind == 2 ? GearTags : 0, HelpTags.size()), 4);
		InitTargetPointers(2);
		DrawMemberHints(2, Kind != 0);
	}

	TMenuTask<> FMenuMain::CloseAbilities()
	{
		for (const int32 Window : {3, 4, 5, 6})
		{
			CloseWindow(Window);
		}
		Flags[MenuFlag::TargetPointers] = 0;
		Flags[MenuFlag::AbilityList] = 0;
		co_await Frame();
		Flags[MenuFlag::PlayTime] = 1;
		Flags[MenuFlag::Windows + 1] = 1;
	}

	TMenuTask<> FMenuMain::AbilitiesScreen(int32 Slot, bool bFromArc, int32 Kind)
	{
		bool bFirst = true;
		bool bGoesOn = true;
		int32 Cursor = 0;
		int32 DrawnCursor = 0xFF;
		int32 DrawnSlot = -1;
		const std::size_t Kinded = static_cast<std::size_t>(Kind);
		OpenAbilities(Kind);
		AllocCursor(0);
		while (true)
		{
			co_await Frame();
			if (Slot != DrawnSlot)
			{
				BuildAbilityList(Slot, Kind);
				DrawnCursor = 0xFF;
				DrawnSlot = Slot;
			}
			DrawCursor(Cursor, 0, 2, 0);
			if (Cursor != DrawnCursor)
			{
				DrawAbilityHelp(Slot, Cursor, Kind);
				DrawnCursor = Cursor;
			}
			if (bFirst)
			{
				const FMenuMainTables& Tables = Resources.Main();
				OpenWindow(6, 0x10, 10, static_cast<uint16>(Tables.AbilityListWidth[Kinded]), 0x70, false, true, PiecesEntry, false);
				OpenWindow(5, 0x0C, 0x86, 0xAC, 0x38, false, true, PiecesEntry, false);
				OpenWindow(4, static_cast<int16>(Tables.AbilityPointsX[Kinded]), 0xA6, static_cast<uint16>(Tables.AbilityPointsWidth[Kinded]), 0x18, false, true,
					PiecesEntry, false);
				// 16 pixels wider in the French translation's program, for its longer word.
				OpenWindow(3, 200, 0x86, Resources.IsFrench() ? 0x60 : 0x50, 0x18, false, true, PiecesEntry, false);
				bFirst = false;
				if (bFromArc)
				{
					OpenLayer();
					co_await SlideParty(false, false);
					co_await Frame();
				}
				Flags[MenuFlag::PlayTime] = 0;
				Flags[MenuFlag::Windows + 1] = 0;
			}
			switch (Code)
			{
			case EMenuCode::Right:
				// A gear's own abilities are one column: only Up and Down move.
				if (Kind != 2)
				{
					Cursor = std::min(Cursor + 1, LastEntry);
				}
				break;
			case EMenuCode::Down:
				if (Cursor + 2 <= LastEntry)
				{
					Cursor += 2;
				}
				break;
			case EMenuCode::Left:
				if (Kind != 2)
				{
					Cursor = std::max(Cursor - 1, 0);
				}
				break;
			case EMenuCode::Up:
				if (Cursor - 2 >= 0)
				{
					Cursor -= 2;
				}
				break;
			case EMenuCode::Confirm:
				if ((AbilityList.Status[static_cast<std::size_t>(Cursor)] & 0x80) != 0)
				{
					co_await UseAbility(Slot, Cursor, Kind);
					DrawnSlot = -1;
					DrawnCursor = 0xFF;
				}
				break;
			case EMenuCode::Cancel:
				bGoesOn = false;
				break;
			case EMenuCode::R1:
			case EMenuCode::L1:
				Slot = NextMember(Slot, Code == EMenuCode::L1, Kind != 0);
				break;
			default:
				break;
			}
			if (!bGoesOn)
			{
				ClearFlags(MenuFlag::HelpTags, static_cast<int32>(HelpTags.size()));
				Flags[MenuFlag::Cursors] = 0;
				co_return;
			}
		}
	}

	void FMenuMain::BuildAbilityList(int32 Slot, int32 Kind)
	{
		const uint8 Character = Member(Slot);
		const std::span<const uint8> Record = Services.Game->Block().subspan(CharacterOffset(Character), CharacterSize);
		const uint8 Gear = Services.Game->CharacterGear(Character).value_or(0);
		const std::size_t Skills = LearnedSkillsOffsetOf(Character);
		const uint16 EtherMask = Resources.Main().EtherMasks[std::min<std::size_t>(Character, Resources.Main().EtherMasks.size() - 1)];
		int32 Points = ReadU16(Record, Character::Ep);
		bool bGearPoints = false;
		std::fill(NameImage.Units.begin(), NameImage.Units.end(), uint16{0});
		for (int32 Entry = 0; Entry <= MaxPointsEntry; ++Entry)
		{
			const std::size_t At = static_cast<std::size_t>(Entry);
			// Listed once learned; a gear's own abilities are every other entry.
			bool bListed = Entry > LastEntry;
			if (Kind == 0 || Kind == 1)
			{
				const uint16 Word = ReadU16(Services.Game->Block(), Skills + (Kind == 0 ? AbilitiesWord : GearEtherWord));
				bListed = bListed || (Resources.Main().SkillBits[At] & Word) != 0;
			}
			else if ((Entry & 1) == 0)
			{
				const uint16 Word = ReadU16(Services.Game->Block(), Skills + SpecialWord);
				bListed = bListed || (Resources.Main().SkillBits[At / 2] & Word) != 0;
			}
			else
			{
				bListed = false;
			}
			if (!bListed)
			{
				AbilityList.Status[At] = 0;
				continue;
			}
			int32 Digits = 2;
			int32 Value = 0;
			if (Entry <= LastEntry)
			{
				FMenuLabel& Name = AbilityList.Names[At];
				const std::span<const uint8> Text = Kind == 0 ? Services.Text.Tables->String(EtherNames, Character * 16 + Entry)
					: Kind == 1 ? Services.Text.Tables->String(GearEtherNames, Gear * 16 + Entry)
								: Services.Text.Tables->String(GearOwnNames, Gear * 4 + Entry / 2);
				Name.Width = static_cast<uint8>(RenderString(NameTyping, NameImage, Text, 0));
				if (Kind == 2)
				{
					Points = ReadU16(Services.Game->Block(), GearOffset(Gear) + GameStateLayout::Gear::Fuel);
					Digits = 4;
				}
				Value = AbilityCost(Slot, Entry, Kind);
			}
			else if (Entry == PointsEntry)
			{
				if (Kind == 2)
				{
					bGearPoints = true;
					Value = ReadU16(Services.Game->Block(), GearOffset(Gear) + GameStateLayout::Gear::Fuel);
					Digits = 5;
				}
				else
				{
					Value = ReadU16(Record, Character::Ep);
				}
			}
			else
			{
				Value = ReadU16(Record, Character::MaxEp);
			}
			// The number, its leading zeros blank.
			std::array<uint16, 5> Glyphs{};
			bool bStarted = false;
			int32 Divisor = 1;
			for (int32 Digit = 1; Digit < Digits; ++Digit)
			{
				Divisor *= 10;
			}
			for (int32 Digit = 0; Digit < Digits - 1; ++Digit, Divisor /= 10)
			{
				const int32 Place = Value / Divisor;
				if (Place != 0 || bStarted)
				{
					Glyphs[static_cast<std::size_t>(Digit)] = static_cast<uint16>(Place + FirstDigitGlyph);
					bStarted = true;
					Value -= Place * Divisor;
				}
				else
				{
					Glyphs[static_cast<std::size_t>(Digit)] = BlankGlyph;
				}
			}
			Glyphs[static_cast<std::size_t>(Digits - 1)] = static_cast<uint16>(Value % 10 + FirstDigitGlyph);
			GlyphText(std::span<const uint16>(Glyphs.data(), static_cast<std::size_t>(Digits)), GlyphScratch);
			FMenuLabel& Cost = AbilityList.Costs[At];
			Cost.Width = static_cast<uint8>(RenderString(NameTyping, NameImage, GlyphScratch, 1));
			NameImage.Rect = {static_cast<int16>(StripX + (Entry & 1) * StripColumn), static_cast<int16>(StripY + Entry / 2 * RowHeight), StripWidth,
				static_cast<int16>(RowHeight)};
			Upload(NameImage);
			// Usable from the menu: a character's Ethers its mask allows, a gear's first own ability, while the cost can be paid.
			uint8 Usable = 0x80;
			if (Entry <= LastEntry)
			{
				const int32 Cost_ = AbilityCost(Slot, Entry, Kind);
				const bool bAllowed = Kind == 0 ? (EtherMask >> Entry & 1) != 0 : Kind == 2 && Entry == 0;
				Usable = bAllowed && Cost_ <= Points ? 0x80 : 0;
				FMenuLabel& Name = AbilityList.Names[At];
				SetStripLabelTexture(Name, Entry, StripBase, static_cast<uint8>(Usable | 1));
				SetLayerQuad(Name.Quad.Points, (Entry % 2 * ColumnStep + NameX) & 0xFFFC, (Entry / 2 * 0x10 + RowY) & 0xFFFE, Name.Width, RowHeight);
			}
			SetStripLabelTexture(Cost, Entry, StripBase, static_cast<uint8>(Usable | 2));
			SetLayerQuad(Cost.Quad.Points, Resources.Main().CostX[At], Resources.Main().CostY[At], Cost.Width, RowHeight);
			RecordLabel(AbilityList.Names[At]);
			RecordLabel(Cost);
			AbilityList.Status[At] = static_cast<uint8>(Usable | 1);
		}
		const int32 Place = bGearPoints ? 5 : 2;
		for (int32 Tag = PointsTag; Tag < PointsTag + PointsTagCount; ++Tag)
		{
			FMenuLabel& Placed = HelpTags[static_cast<std::size_t>(Tag)];
			const std::size_t Column = static_cast<std::size_t>(Tag + (Place == 5 ? 8 : 0));
			SetLayerQuad(Placed.Quad.Points, Resources.Main().AbilityTagX[Column], Resources.Main().AbilityTagY[1], Placed.Width, RowHeight);
			RecordLabel(Placed);
			Flags[static_cast<std::size_t>(MenuFlag::HelpTags + Tag)] = 1;
		}
		DrawMemberName(AbilityList.MemberName, Slot, 1, Kind != 0);
		Flags[MenuFlag::AbilityList] = 1;
	}

	void FMenuMain::DrawAbilityHelp(int32 Slot, int32 Index, int32 Kind)
	{
		const std::size_t At = static_cast<std::size_t>(Index);
		if (AbilityList.Status[At] == 0)
		{
			AbilityList.bHelpShown = false;
			ClearFlags(MenuFlag::HelpTags, PointsTag);
			return;
		}
		const uint8 Character = Member(Slot);
		const uint8 Gear = Services.Game->CharacterGear(Character).value_or(0);
		using EText = FMenuResources::EText;
		const int32 Text = Kind == 2 ? Gear * 8 + (Index & 0xFE) : (Kind == 1 ? Gear : Character) * 32 + Index * 2;
		const FMessageTable& Lines = Resources.Text(Kind == 0 ? EText::AbilityHelp : (Kind == 1 ? EText::GearAbilityHelp : EText::GearOptionHelp));
		std::fill(HelpImage.Units.begin(), HelpImage.Units.end(), uint16{0});
		for (int32 Line = 0; Line < 2; ++Line)
		{
			AbilityList.HelpLines[static_cast<std::size_t>(Line)].Width = static_cast<uint8>(RenderString(HelpTyping, HelpImage, Lines.String(Text + Line), Line));
		}
		HelpImage.Rect = {HelpStripX, HelpStripY, HelpStripWidth, static_cast<int16>(RowHeight)};
		Upload(HelpImage);
		for (int32 Line = 0; Line < 2; ++Line)
		{
			FMenuLabel& Label = AbilityList.HelpLines[static_cast<std::size_t>(Line)];
			SetLabelTexture(Label, Line, 0);
			RecordLabel(Label);
			SetTexelRect(Label.Quad, 0, HelpStripY, Label.Width, RowHeight);
			SetLayerQuad(Label.Quad.Points, HelpLineX, HelpLineY[static_cast<std::size_t>(Line)], Label.Width, RowHeight);
		}
		// The entry's name again, bright.
		AbilityList.HelpName = AbilityList.Names[At];
		SetLayerQuad(AbilityList.HelpName.Quad.Points, HelpNameX, HelpNameY, AbilityList.Names[At].Width, RowHeight);
		RecordLabel(AbilityList.HelpName);
		AbilityList.HelpName.Quad.Corners[0].Color = {Bright, Bright, Bright};
		AbilityList.HelpName.Quad.Code = static_cast<uint8>(AbilityList.HelpName.Quad.Code & ~MenuGpu::SemiTransparent);

		// The side and the extent it works on.
		ClearFlags(MenuFlag::HelpTags, static_cast<int32>(HelpTags.size()));
		const std::span<const uint8> Record = AbilityRecord(Slot, Kind == 2 ? Index / 2 : Index, Kind);
		const uint16 Targeting = Record.empty() ? 0 : ReadU16(Record, 0);
		const int32 Side = (Targeting & 0x4000) != 0 ? 2 : ((Targeting & 0x1000) == 0 ? 1 : 0);
		for (const int32 Tag : {Side, (Targeting & 3) + 3})
		{
			FMenuLabel& Placed = HelpTags[static_cast<std::size_t>(Tag)];
			SetLayerQuad(Placed.Quad.Points, Resources.Main().AbilityTagX[static_cast<std::size_t>(Tag)], Resources.Main().AbilityTagY[0], Placed.Width, RowHeight);
			RecordLabel(Placed);
			Flags[static_cast<std::size_t>(MenuFlag::HelpTags + Tag)] = 1;
		}
		AbilityList.bHelpShown = true;
		Flags[static_cast<std::size_t>(MenuFlag::HelpTags + PointsTag)] = 1;
		Flags[static_cast<std::size_t>(MenuFlag::HelpTags + PointsTag + 1)] = 1;
	}

	void FMenuMain::AddAbilityList()
	{
		if (Flags[MenuFlag::AbilityList] == 0)
		{
			return;
		}
		for (std::size_t Entry = 0; Entry < AbilityList.Status.size(); ++Entry)
		{
			if (AbilityList.Status[Entry] != 0)
			{
				AddProjected(AbilityList.Names[Entry].Quad, true, PiecesEntry);
				AddProjected(AbilityList.Costs[Entry].Quad, true, PiecesEntry);
			}
		}
		if (AbilityList.bHelpShown)
		{
			AddProjected(AbilityList.HelpName.Quad, true, PiecesEntry);
			for (FMenuLabel& Line : AbilityList.HelpLines)
			{
				AddProjected(Line.Quad, true, PiecesEntry);
			}
		}
		AddProjected(AbilityList.MemberName, true, PiecesEntry);
	}
}
