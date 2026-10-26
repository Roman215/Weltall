#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Menu/MenuMain.h"
#include "Menu/MenuResources.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 FirstPortraitFrame = 0x14B;
		constexpr int32 FullScale = 0x1000;
		constexpr int32 PiecesEntry = 4;
		/** The portrait's place on the sheet, and the square it is drawn in on the layer. */
		constexpr int32 PortraitX = 0x18;
		constexpr int32 PortraitY = 0x0E;
		constexpr int32 PortraitSize = 0x30;
		/** The name, from its strip in the page at (384, 0). */
		constexpr int32 NamePageX = 0x180;
		constexpr int32 NameWidth = 0x48;
		constexpr int32 NameRows = 13;
		constexpr int32 TextClutY = 0x1D1;
		constexpr int32 OddClutX = 16;
		constexpr uint8 Unshaded = 0x80;
		constexpr uint8 Dimmed = 0x40;
		constexpr int32 SheetLabelsPerKind = 24;
		constexpr int32 NoLabel = 0xFFFF;
		constexpr int32 DigitWidth = 8;
		constexpr uint8 Blank = 0xFF;

		/** The sheet's numbers by their places in the table. */
		namespace Number
		{
			constexpr std::size_t Level = 0;
			constexpr std::size_t Hp = 2;
			constexpr std::size_t MaxHp = 3;
			constexpr std::size_t Ep = 4;
			constexpr std::size_t MaxEp = 5;
			constexpr std::size_t Exp = 6;
			constexpr std::size_t Next = 8;
			constexpr std::size_t Weight = 10;
		}

		/** The record's total EXP and its weight's tenths, beside the layout's named fields. */
		constexpr std::size_t ExpField = 0x3C;
		constexpr std::size_t WeightTenths = 0x79;
		/** The weight in pounds: kilograms × 2.2, in tenths, kept in 16 bits. */
		constexpr uint32 PoundsPerTenKilograms = 22;
		/** The tenths digit follows the integer part's three digits and the point. */
		constexpr int32 TenthsX = 0x20;
		/** A gear's sheet: shifted left, its name wider, its weight a word of the gear's record; its ride badges. */
		constexpr int32 GearShift = 0x18;
		constexpr std::size_t GearWeightField = 0x68;
		constexpr int32 RideBadgeX = 0x78;
		constexpr int32 RideBadgeY = 0x5A;
		constexpr int32 RideBadgeStep = 0x20;
		constexpr uint8 DimmedBadge = 0x20;

		/** The stats: where the sheet puts them, a row apart, and the bars' length for the largest value. */
		constexpr int32 StatsX = 0x98;
		constexpr int32 StatsY = 0x26;
		constexpr int32 StatRowHeight = 8;
		constexpr int32 BarHeight = 6;
		constexpr int32 FirstValueDigit = 5;
		constexpr int32 ValueDigits = 4;
		/** A bar is its colour at the top, black at the bottom: a stat's, a gain's, a loss's. */
		constexpr FDrawColor BarColour{0xFF, 0x80, 0x80};
		constexpr FDrawColor GainColour{0xFF, 0, 0};
		constexpr FDrawColor LossColour{0, 0, 0xFF};
		constexpr FDrawColor Black{0, 0, 0};
		/** The Equip screen's place for the stats. */
		constexpr int32 EquipStatsX = 0x80;
		constexpr int32 EquipStatsY = 0x90;
		/** A change: its sign (+ or -) right of the value, its digits after, packed, tinted as its bar. */
		constexpr int32 PlusFrame = 0xE3;
		constexpr int32 MinusFrame = 0xE5;
		constexpr int32 ChangeSignX = 0x20;
		constexpr int32 ChangeDigitsX = 0x28;
		constexpr int32 FirstChangeDigit = 6;
		constexpr int32 ChangeDigits = 3;
		constexpr FDrawColor GainTint{0x80, 0x40, 0x40};
		constexpr FDrawColor LossTint{0x40, 0x40, 0x80};

		void SetBar(FMenuPrimitive& Bar, FDrawColor Colour)
		{
			Bar.Code = MenuGpu::GouraudQuad;
			Bar.Corners[0].Color = Colour;
			Bar.Corners[1].Color = Colour;
			Bar.Corners[2].Color = Black;
			Bar.Corners[3].Color = Black;
		}

		/** The equipment's names: the weapon's, the three accessories', the gear's (its name's strip). */
		constexpr std::size_t NameCount = 5;
		constexpr int32 NamesX = 0xD0;
		constexpr std::size_t GearName = 4;
		constexpr int32 GearNameWidth = 0x60;
		constexpr int32 NamesBase = 0x0C;
		constexpr int16 NameStripX = 0x140;
		constexpr int16 NameStripY = 0x27;
		constexpr int16 NameStripWidth = 0x28;
		/** The kernel's name tables of the weapons' and the accessories' names. */
		constexpr int32 WeaponNames = 23;
		constexpr int32 AccessoryNames = 17;
		/** The gears' weapons' and parts' names. */
		constexpr int32 GearWeaponNames = 51;
		constexpr int32 GearPartNames = 50;
		/** The Equip screen's place for the names. */
		constexpr int32 EquipNamesX = 0x28;
		/** A gear's name is the state's name after the characters'. */
		constexpr int32 FirstGearName = 11;

		void Shade(std::vector<FMenuPrimitive>& Parts, std::size_t First, uint8 Grey)
		{
			for (std::size_t Part = First; Part < Parts.size(); ++Part)
			{
				Parts[Part].Code = static_cast<uint8>(Parts[Part].Code & ~MenuGpu::RawTexture);
				Parts[Part].Corners[0].Color = {Grey, Grey, Grey};
			}
		}
	}

	TMenuTask<> FMenuMain::DrawStatusSheet(int32 Slot, bool bGear)
	{
		// Over three frames: the sheet, then the stats, then the equipment's names.
		BuildStatusSheet(Slot, bGear);
		DrawRideBadge(Slot, bGear);
		co_await Frame();
		DrawStats(false, false, bGear);
		co_await Frame();
		DrawEquipNames(Slot, 0, false, bGear);
	}

	void FMenuMain::DrawRideBadge(int32 Slot, bool bGear)
	{
		// A gear's sheet: "Walk" and "Gear" side by side, the one not in use dimmed.
		StatusSheet.RideBadge.clear();
		if (!bGear)
		{
			return;
		}
		const FMenuMainTables& Tables = Resources.Main();
		for (std::size_t Badge = 0; Badge < Tables.RideBadges.size(); ++Badge)
		{
			const std::size_t First = StatusSheet.RideBadge.size();
			Resources.Sheet().Draw(Tables.RideBadges[Badge], RideBadgeX + static_cast<int32>(Badge) * RideBadgeStep, RideBadgeY, FullScale, StatusSheet.RideBadge);
			StatusSheet.RideBadge.resize(First + 1);
		}
		SetLayerSprites(StatusSheet.RideBadge);
		FMenuPrimitive& Unused = StatusSheet.RideBadge[Services.Game->PartySlotInGear(GameSlot(Slot)) == 0 ? 1 : 0];
		Unused.Code = static_cast<uint8>((Unused.Code | MenuGpu::SemiTransparent) & ~MenuGpu::RawTexture);
		Unused.Page = static_cast<uint16>(Unused.Page | MenuGpu::PageBlendAdd);
		Unused.Corners[0].Color = {DimmedBadge, DimmedBadge, DimmedBadge};
	}

	void FMenuMain::BuildStatusSheet(int32 Slot, bool bGear)
	{
		using namespace GameStateLayout;
		const FMenuMainTables& Tables = Resources.Main();
		const FMenuSpriteSheet& Sprites = Resources.Sheet();
		const uint8 Character = Member(Slot);
		const std::span<const uint8> Record = Services.Game->Block().subspan(CharacterOffset(Character), CharacterSize);
		const uint8 GearId = Record[Character::Gear];
		const std::span<const uint8> GearRecord = bGear ? Services.Game->Block().subspan(GearOffset(GearId), GearSize) : std::span<const uint8>{};
		// A gear's sheet lies a little to the left, its numbers wider.
		const int32 Shift = bGear ? GearShift : 0;

		StatusSheet.Portrait.clear();
		Sprites.Draw(FirstPortraitFrame + Slot, PortraitX - Shift, PortraitY, FullScale, StatusSheet.Portrait);
		if (!StatusSheet.Portrait.empty())
		{
			SetLayerQuad(StatusSheet.Portrait.front().Points, StatusSheet.Portrait.front().Corners[0].X, StatusSheet.Portrait.front().Corners[0].Y, PortraitSize, PortraitSize);
		}

		FMenuPrimitive& Name = StatusSheet.Name;
		const int32 NameShown = bGear ? GearNameWidth : NameWidth;
		const std::size_t Strip = static_cast<std::size_t>(bGear ? 3 + Slot : Slot);
		Name.Code = MenuGpu::TexturedQuad;
		Name.Page = MenuGpu::TexturePage(0, 0, NamePageX, 0);
		const bool bOdd = bGear ? ((GearId + FirstGearName) & 1) != 0 : (Character & 1) != 0;
		Name.Clut = MenuGpu::Clut(bOdd ? OddClutX : 0, TextClutY);
		const uint8 U = static_cast<uint8>((Tables.NameStripX[Strip] & 0x3F) << 2);
		const uint8 V = static_cast<uint8>(Tables.NameStripY[Strip]);
		Name.Corners[0] = {0, 0, U, V, {Unshaded, Unshaded, Unshaded}};
		Name.Corners[1] = {static_cast<int16>(NameShown), 0, static_cast<uint8>(U + NameShown), V, {}};
		Name.Corners[2] = {0, NameRows, U, static_cast<uint8>(V + NameRows), {}};
		Name.Corners[3] = {static_cast<int16>(NameShown), NameRows, static_cast<uint8>(U + NameShown), static_cast<uint8>(V + NameRows), {}};
		SetLayerQuad(Name.Points, Tables.SheetName[0] - Shift, Tables.SheetName[1], NameShown, NameRows);

		StatusSheet.Labels.clear();
		const std::size_t FirstLabel = bGear ? SheetLabelsPerKind : 0;
		for (std::size_t Label = FirstLabel; Label < FirstLabel + SheetLabelsPerKind; ++Label)
		{
			const int32 Frame = Tables.SheetLabels[Label];
			if (Frame != NoLabel)
			{
				Sprites.Draw(Frame, Tables.SheetLabelX[Label], Tables.SheetLabelY[Label], FullScale, StatusSheet.Labels);
			}
		}
		SetLayerSprites(StatusSheet.Labels);

		// Each number right-aligned in its places, a maximum packed from the left. The ether level, ether
		// EXP and ether EXP to next are drawn too and never linked: they are left out.
		const auto Place = [&Tables](std::size_t Which) { return Tables.SheetNumbers[Which]; };
		const auto Digits = [this, Shift](std::vector<FMenuPrimitive>& Parts, uint32 Value, int32 First, int32 Count, const std::array<int32, 2>& At, int32 X, int32 Y,
								bool bPacked)
		{
			DrawDigits(Parts, Value, First, Count, At[0] + X - Shift, At[1] + Y, bPacked);
			SetLayerSprites(Parts);
		};
		if (!bGear)
		{
			Digits(StatusSheet.Hp, LittleEndian::ReadU16(Record, Character::Hp), 6, 3, Place(Number::Hp), 0, 0, false);
			Digits(StatusSheet.MaxHp, LittleEndian::ReadU16(Record, Character::MaxHp), 6, 3, Place(Number::MaxHp), 0, 0, true);
			Digits(StatusSheet.Ep, LittleEndian::ReadU16(Record, Character::Ep), 7, 2, Place(Number::Ep), 0, 0, false);
			Digits(StatusSheet.MaxEp, LittleEndian::ReadU16(Record, Character::MaxEp), 7, 2, Place(Number::MaxEp), 0, 0, true);
		}
		else
		{
			// A gear's HP and fuel in the character's HP and EP.
			Digits(StatusSheet.Hp, LittleEndian::ReadU32(GearRecord, GameStateLayout::Gear::Hp), 4, 5, Place(Number::Hp), 8, 0, false);
			Digits(StatusSheet.MaxHp, LittleEndian::ReadU32(GearRecord, GameStateLayout::Gear::MaxHp), 4, 5, Place(Number::MaxHp), -0x20, 8, true);
			Digits(StatusSheet.Ep, LittleEndian::ReadU16(GearRecord, GameStateLayout::Gear::Fuel), 5, 4, Place(Number::Ep), 0, 8, false);
			Digits(StatusSheet.MaxEp, LittleEndian::ReadU16(GearRecord, GameStateLayout::Gear::MaxFuel), 5, 4, Place(Number::MaxEp), -0x20, 0x10, true);
		}
		Digits(StatusSheet.Level, Record[Character::Level], 6, 3, Place(Number::Level), 0, 0, false);
		Digits(StatusSheet.Next, LittleEndian::ReadU32(Record, Character::ExpToNext), 2, 7, Place(Number::Next), 0, 0, false);

		if (!bGear)
		{
			const uint32 Kilograms = static_cast<uint32>(Record[Character::Weight]) + Record[Character::WeightGain];
			const uint32 Pounds = (Kilograms * 10 + Record[WeightTenths]) * PoundsPerTenKilograms & 0xFFFF;
			uint32 Whole = Pounds / 100;
			uint32 Tenths = Pounds % 100 / 10;
			if (Resources.IsFrench())
			{
				// The French translation's program shows the kilograms, its tenths' division left out: 0 unless the pounds' 16
				// bits wrapped below the kilograms' hundreds.
				Whole = Kilograms;
				Tenths = static_cast<int32>(Pounds) - static_cast<int32>(Kilograms * 100) < 0 ? 1 : 0;
			}
			DrawDigits(StatusSheet.Weight, Whole, 6, 3, Place(Number::Weight)[0], Place(Number::Weight)[1], false);
			Sprites.Draw(static_cast<int32>(Tenths), Place(Number::Weight)[0] + TenthsX, Place(Number::Weight)[1], FullScale, StatusSheet.Weight);
			SetLayerSprites(StatusSheet.Weight);
		}
		else
		{
			// A gear's weight is its frame's capacity, whole.
			Digits(StatusSheet.Weight, LittleEndian::ReadU16(GearRecord, GearWeightField), 4, 5, Place(Number::Weight), 8, 8, false);
		}

		Digits(StatusSheet.Exp, LittleEndian::ReadU32(Record, ExpField), 1, 8, Place(Number::Exp), 0, 0, false);
		Flags[MenuFlag::StatusSheet] = 1;
	}

	void FMenuMain::DrawStats(bool bEquipPlace, bool bCompare, bool bGear)
	{
		const FMenuMainTables& Tables = Resources.Main();
		const FMenuSpriteSheet& Sprites = Resources.Sheet();
		const int32 X = bEquipPlace ? EquipStatsX : StatsX;
		const int32 Y = bEquipPlace ? EquipStatsY : StatsY;
		const std::size_t Count = bGear ? FCharacterStats::Count - 1 : FCharacterStats::Count;
		const std::size_t LabelKind = bGear ? FCharacterStats::Count : 0;
		// The labels, every other one dimmed.
		StatBars.Labels.clear();
		for (std::size_t Stat = 0; Stat < Count; ++Stat)
		{
			const std::size_t First = StatBars.Labels.size();
			Sprites.Draw(Tables.StatLabels[LabelKind + Stat], X + Tables.StatLabelX[Stat], Y + Tables.StatLabelY[Stat], FullScale, StatBars.Labels);
			if ((Stat & 1) != 0)
			{
				Shade(StatBars.Labels, First, Dimmed);
			}
		}
		SetLayerSprites(StatBars.Labels);

		// Each bar as long as its value against the largest shown (of both, when compared), 64 pixels for
		// the largest; compared, the change as a second bar and a signed number.
		const std::array<uint16, 9>& Old = bCompare ? EquipList.SavedStats : ShownStats;
		uint16 Largest = 0;
		for (std::size_t Stat = 0; Stat < FCharacterStats::Count; ++Stat)
		{
			Largest = std::max({Largest, Old[Stat], ShownStats[Stat]});
		}
		const auto Length = [Largest](int32 Value) { return Largest == 0 ? 0 : Value * 100 / Largest * 0x1900 / 10000; };
		for (std::size_t Stat = 0; Stat < Count; ++Stat)
		{
			const int32 Value = Old[Stat];
			const int32 Change = static_cast<int32>(ShownStats[Stat]) - Value;
			const int32 RowY = Y + static_cast<int32>(Stat) * StatRowHeight;
			FMenuPrimitive& Bar = StatBars.Bars[Stat];
			SetBar(Bar, BarColour);
			SetLayerQuad(Bar.Points, X + Tables.StatRow[0], RowY + Tables.StatRow[1], Length(Value), BarHeight);

			std::vector<FMenuPrimitive>& Digits = StatBars.Values[Stat];
			Digits.clear();
			SplitNumber(static_cast<uint32>(Value));
			for (int32 Digit = 0; Digit < ValueDigits; ++Digit)
			{
				const uint8 Frame = NumberDigits[static_cast<std::size_t>(FirstValueDigit + Digit)];
				if (Frame != Blank)
				{
					Sprites.Draw(Frame, X + Tables.StatRow[2] + Digit * DigitWidth, RowY + Tables.StatRow[3], FullScale, Digits);
				}
			}
			SetLayerSprites(Digits);
			if ((Stat & 1) != 0)
			{
				Shade(Digits, 0, Dimmed);
			}
			std::vector<FMenuPrimitive>& Changes = StatBars.Changes[Stat];
			Changes.clear();
			if (bCompare)
			{
				const bool bLoss = Change < 0;
				const int32 ChangeLength = Length(bLoss ? -Change : Change);
				SetBar(StatBars.ChangeBars[Stat], bLoss ? LossColour : GainColour);
				const int32 BarX = X + Tables.StatRow[0] + Length(Value) - (bLoss ? ChangeLength : 0);
				SetLayerQuad(StatBars.ChangeBars[Stat].Points, BarX, RowY + Tables.StatRow[1], ChangeLength, BarHeight);
				if (Change != 0)
				{
					Sprites.Draw(bLoss ? MinusFrame : PlusFrame, X + Tables.StatRow[2] + ChangeSignX, RowY + Tables.StatRow[3], FullScale, Changes);
					SplitNumber(static_cast<uint32>(bLoss ? -Change : Change));
					int32 At = ChangeDigitsX;
					for (int32 Digit = 0; Digit < ChangeDigits; ++Digit)
					{
						const uint8 Frame = NumberDigits[static_cast<std::size_t>(FirstChangeDigit + Digit)];
						if (Frame != Blank)
						{
							Sprites.Draw(Frame, X + Tables.StatRow[2] + At, RowY + Tables.StatRow[3], FullScale, Changes);
							At += DigitWidth;
						}
					}
					SetLayerSprites(Changes);
					for (FMenuPrimitive& Part : Changes)
					{
						Part.Code = static_cast<uint8>(Part.Code & ~MenuGpu::RawTexture);
						Part.Corners[0].Color = bLoss ? LossTint : GainTint;
					}
					StatBars.bChangesShown = true;
				}
			}
			StatBars.Shown[Stat] = true;
		}
		Flags[MenuFlag::Stats] = 1;
	}

	void FMenuMain::DrawEquipNames(int32 Slot, int32 Kind, bool bSaved, bool bGear)
	{
		using namespace GameStateLayout;
		const FMenuMainTables& Tables = Resources.Main();
		const std::size_t Count = Kind == 0 ? NameCount : NameCount - 1;
		const int32 X = Kind == 0 ? NamesX : EquipNamesX;
		const std::size_t FirstY = Kind == 0 ? 0 : (Kind == 1 ? 5 : 9);
		if (Kind != 0)
		{
			EquipNameShown[GearName] = false;
		}
		// The record's (or its gear's) weapon, guns and accessories, or the ones kept aside.
		const auto Worn = [&](std::size_t Block, std::size_t At) -> uint8
		{
			if (bSaved)
			{
				return EquipList.SavedSlots[Block * 5 + At];
			}
			constexpr std::array<std::size_t, 3> CharacterBlocks = {0x6A, 0x6F, 0x74};
			constexpr std::array<std::size_t, 3> GearBlocks = {0x0C, 0x04, 0x09};
			return RecordByte(Slot, bGear, (bGear ? GearBlocks : CharacterBlocks)[Block] + At);
		};
		const FMesNameTables& Names = *Services.Text.Tables;
		const auto WeaponName = [&](uint8 Id) { return Names.String(bGear ? GearWeaponNames : WeaponNames, Id); };
		// The names are typed in pairs, one to a plane, into a strip of the same buffer.
		std::fill(NameImage.Units.begin(), NameImage.Units.end(), uint16{0});
		bool bUploads = false;
		for (std::size_t Index = 0; Index < Count; ++Index)
		{
			FMenuLabel& Label = EquipNames[Index];
			std::span<const uint8> Text;
			bool bTyped = true;
			if (Index == 0)
			{
				Text = WeaponName(Kind == 2 ? Worn(1, 0) : Worn(0, 0));
			}
			else if (Index != GearName || bGear)
			{
				if (Kind == 2)
				{
					Text = WeaponName(Worn(1, Index));
				}
				else if (!bGear)
				{
					Text = Names.String(AccessoryNames, Worn(2, Index - 1));
				}
				else if (Kind == 0)
				{
					// A gear's sheet: its weapon's second name, then its parts.
					Text = Index == 1 ? WeaponName(Worn(0, 3)) : Names.String(GearPartNames, Worn(2, Index - 2));
				}
				else
				{
					Text = Names.String(GearPartNames, Worn(2, Index - 1));
				}
			}
			else
			{
				bTyped = false;
			}
			if (bTyped)
			{
				Label.Width = static_cast<uint8>(RenderString(NameTyping, NameImage, Text, static_cast<int32>(Index % 2)));
			}
			if (Index % 2 == 1 || (bGear && Kind == 0 && Index == GearName))
			{
				bUploads = true;
			}
			else if (!bGear)
			{
				bUploads = false;
			}
			if (bUploads)
			{
				NameImage.Rect = {static_cast<int16>(NameStripX + (static_cast<int32>(Index) * 16 & 0x20)), static_cast<int16>(NameStripY + static_cast<int32>(Index) / 4 * 13),
					NameStripWidth, 13};
				Upload(NameImage);
			}
			SetLabelTexture(Label, static_cast<int32>(Index), NamesBase);
			RecordLabel(Label);
			if (Index == GearName && !bGear)
			{
				// The gear's name is the one rendered for the party's gears.
				const uint8 Gear = RecordByte(Slot, false, Character::Gear);
				Label.Quad.Page = MenuGpu::TexturePage(0, 0, NamePageX, 0);
				Label.Quad.Clut = MenuGpu::Clut(((Gear + FirstGearName) & 1) != 0 ? OddClutX : 0, TextClutY);
				const uint8 U = static_cast<uint8>(Tables.NameStripX[3 + static_cast<std::size_t>(Slot)] << 2);
				const uint8 V = static_cast<uint8>(Tables.NameStripY[3 + static_cast<std::size_t>(Slot)]);
				Label.Quad.Corners[0].U = U;
				Label.Quad.Corners[0].V = V;
				Label.Quad.Corners[1].U = static_cast<uint8>(U + GearNameWidth);
				Label.Quad.Corners[1].V = V;
				Label.Quad.Corners[2].U = U;
				Label.Quad.Corners[2].V = static_cast<uint8>(V + NameRows);
				Label.Quad.Corners[3].U = static_cast<uint8>(U + GearNameWidth);
				Label.Quad.Corners[3].V = static_cast<uint8>(V + NameRows);
				Label.Width = GearNameWidth;
			}
			SetLayerQuad(Label.Quad.Points, X, Tables.EquipNameY[Index + FirstY], Label.Width, NameRows);
			EquipNameShown[Index] = true;
		}
		Flags[MenuFlag::EquipNames] = 1;
	}

	void FMenuMain::AddStatusSheet()
	{
		if (Flags[MenuFlag::StatusSheet] == 0)
		{
			return;
		}
		if (!StatusSheet.Portrait.empty())
		{
			AddProjected(StatusSheet.Portrait.front(), true, PiecesEntry);
		}
		AddProjected(StatusSheet.Name, true, PiecesEntry);
		AddLayered(StatusSheet.RideBadge);
		AddLayered(StatusSheet.Labels);
		AddLayered(StatusSheet.Hp);
		AddLayered(StatusSheet.MaxHp);
		AddLayered(StatusSheet.Ep);
		AddLayered(StatusSheet.MaxEp);
		AddLayered(StatusSheet.Level);
		AddLayered(StatusSheet.Exp);
		AddLayered(StatusSheet.Next);
		AddLayered(StatusSheet.Weight);
	}

	void FMenuMain::AddStats()
	{
		if (Flags[MenuFlag::Stats] == 0)
		{
			return;
		}
		AddLayered(StatBars.Labels);
		for (std::size_t Stat = 0; Stat < FCharacterStats::Count; ++Stat)
		{
			if (StatBars.Shown[Stat])
			{
				if (StatBars.bChangesShown)
				{
					AddProjected(StatBars.ChangeBars[Stat], true, PiecesEntry);
					AddLayered(StatBars.Changes[Stat]);
				}
				AddProjected(StatBars.Bars[Stat], true, PiecesEntry);
				AddLayered(StatBars.Values[Stat]);
			}
		}
	}

	void FMenuMain::AddEquipNames()
	{
		if (Flags[MenuFlag::EquipNames] == 0)
		{
			return;
		}
		for (std::size_t Index = 0; Index < NameCount; ++Index)
		{
			if (EquipNameShown[Index])
			{
				AddProjected(EquipNames[Index].Quad, true, PiecesEntry);
			}
		}
	}
}
