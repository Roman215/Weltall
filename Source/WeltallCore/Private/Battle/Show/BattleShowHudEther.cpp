#include "Battle/BattleContext.h"
#include "Battle/BattleGameState.h"
#include "Battle/Show/BattleHudGpu.h"
#include "Battle/Show/BattleHudLists.h"
#include "Battle/Show/BattleShowHud.h"
#include "Text/MesRender.h"
#include "Text/MessageTable.h"

#include <algorithm>

namespace Weltall
{
	using namespace BattleHudGpu;
	using namespace BattleHudLists;

	namespace
	{
		/** The Ether list's sixteen entries: the abilities from command 16h on foot, the gear's ether commands from 15h. */
		constexpr int32 Entries = 16;
		constexpr int32 FirstAbility = 0x16;
		constexpr int32 FirstGearAbility = 0x15;
		/** The kernel's name tables of the abilities: by character on foot, by gear in a gear; sixteen each. */
		constexpr int32 AbilityNameTable = 20;
		constexpr int32 GearAbilityNameTable = 48;
		/** The Ether list's rows: 16 pixels apart, typed from (380h, 100h). */
		constexpr int32 EtherRow = 16;
		/** Elly shows each ability's success rate. */
		constexpr uint8 Elly = 1;
		constexpr uint8 MaxRate = 100;
		/** The labels drawn beside the EP: the rate's and the EP's. */
		constexpr int32 RateLabel = 6;
		constexpr int32 PointsLabel = 5;
		constexpr int32 SlashFrame = 0x71;
		/** The help files: the Ether's on foot (by character) and in a gear (by gear), 32 lines each. */
		constexpr int32 FootHelp = 0;
		constexpr int32 GearHelp = 1;
		constexpr int32 HelpPerOwner = 0x20;

		bool HighBit(uint16 Word, int32 Bit)
		{
			return Bit >= 0 && Bit < 16 && (Word & (0x8000u >> Bit)) != 0;
		}

		/** A digit's texels among the labels' digits. */
		uint8 DigitU(uint32 Digit)
		{
			return static_cast<uint8>(Digit * 8 + LabelsU);
		}
	}

	const FBattleCommand* FBattleHud::EtherCommand(uint8 Slot, int32 Entry)
	{
		const bool bOnFoot = Battle->ApRecords[Slot].InGear == 0;
		const int32 Index = bOnFoot ? FBattleContext::PartyCommandIndex(Slot, FirstAbility + Entry) : FBattleContext::GearCommandIndex(Slot, FirstGearAbility + Entry);
		if (Index < 0 || Index >= static_cast<int32>(Battle->Commands.size()))
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return nullptr;
		}
		return &Battle->Commands[static_cast<std::size_t>(Index)];
	}

	bool FBattleHud::EtherLearned(uint8 Slot, int32 Entry)
	{
		const std::size_t Field = Battle->ApRecords[Slot].InGear == 0 ? GameStateLayout::Skills::Abilities : GameStateLayout::Skills::GearAbilities;
		const std::size_t Offset = BattleGameState::SkillsOffset(Battle->PartyCharacters[Slot], Field);
		if (Battle->GameState == nullptr || Offset + 2 > FGameState::BlockSize)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return false;
		}
		return HighBit(BattleGameState::Word(*Battle->GameState, Offset), Entry);
	}

	void FBattleHud::BuildEtherList(uint8 Slot)
	{
		if (Slot >= PartySlots)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		OpenListWork();
		RenderLabels();
		DrawScrollBar(0x20, 0x30, 0x98, 0x38, 0xC);
		FVramImage Blank = BlankImage(WideUnits);
		Blank.Rect.X = HelpX;
		Upload(Blank);
		const bool bOnFoot = Battle->ApRecords[Slot].InGear == 0;
		const FBattleCombatant& Member = Battle->Combatants[Slot];
		const auto Place = [&](FVramImage Image, int16 X, int16 Y, int16 Width, int16 Height) {
			Image.Rect = {X, Y, Width, Height};
			Image.Units.resize(static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height), 0);
			Upload(Image);
		};
		const FVramImage BlankDigit = BlankImage(DigitUnits);
		for (int32 Entry = 0; Entry < Entries && FaultState == EBattleHudFault::None; ++Entry)
		{
			const bool bLearned = EtherLearned(Slot, Entry);
			const FBattleCommand* Command = EtherCommand(Slot, Entry);
			const uint32 Cost = bLearned && Command != nullptr ? Command->EpCost : 0;
			const int16 Across = static_cast<int16>(Entry % 2);
			const int16 Down = static_cast<int16>(Entry / 2 * EtherRow);
			Place(BlankImage(0x1B, EtherRow), static_cast<int16>(0x380 + Across * NarrowUnits), static_cast<int16>(0x100 + Down), 0x1B, EtherRow);
			if (bLearned)
			{
				const int32 Owner = bOnFoot ? Member.Character : Member.Gear;
				FVramImage Name = BlankImage(NarrowUnits);
				RenderMesString(Text, ListTyping, TableString(bOnFoot ? AbilityNameTable : GearAbilityNameTable, Owner * Entries + Entry), Name, 0);
				Place(Name, static_cast<int16>(0x380 + Across * NarrowUnits), static_cast<int16>(0x102 + Down), NarrowUnits, TextRows);
			}
			if (Across == 0)
			{
				Place(BlankImage(0x1B, EtherRow), HelpX, static_cast<int16>(0x100 + Down), 0x1B, EtherRow);
			}
			// A cost past 99 would take a tens digit from past the digits' images.
			if (Cost / 10 >= DigitImages.size())
			{
				FaultState = EBattleHudFault::IndexOutsideHud;
				return;
			}
			Place(Cost / 10 != 0 ? DigitImages[Cost / 10] : BlankDigit, static_cast<int16>(0x3C0 + Across * 0x10), static_cast<int16>(0x102 + Down), DigitUnits, TextRows);
			Place(bLearned ? DigitImages[Cost % 10] : BlankDigit, static_cast<int16>(0x3C2 + Across * 0x10), static_cast<int16>(0x102 + Down), DigitUnits, TextRows);
		}
		DrawEtherPoints(Slot);
	}

	void FBattleHud::DrawEtherPoints(uint8 Slot)
	{
		auto& Quads = ListWork->Quads;
		SetLabelQuad(Quads[FBattleHudListWork::Rate][Buffer], 0xA8, 0xA6, RateLabel);
		SetLabelQuad(Quads[FBattleHudListWork::Points][Buffer], 0xEC, 0xC6, PointsLabel);
		const FBattleCombatant& Member = Battle->Combatants[Slot];
		if (Member.Ep / 10 != 0)
		{
			SetTextQuad(Quads[14][Buffer], 0x104, 0xC6, static_cast<uint8>(DigitU(Member.Ep / 10u) & 0xF8), 0, 8);
		}
		SetTextQuad(Quads[15][Buffer], 0x10C, 0xC6, DigitU(Member.Ep % 10u), 0, 8);
		if (Member.MaxEp / 10 != 0)
		{
			SetTextQuad(Quads[16][Buffer], 0x11C, 0xC6, static_cast<uint8>(DigitU(Member.MaxEp / 10u) & 0xF8), 0, 8);
		}
		SetTextQuad(Quads[17][Buffer], 0x124, 0xC6, DigitU(Member.MaxEp % 10u), 0, 8);
		DrawSheet(SlashFrame, std::span<FBattleHudPair>(&Quads[18], 1), 0, 0x118, 0xD1, FullScale);
		ListWork->FiguresBuffer = Buffer;
	}

	void FBattleHud::ShowEtherCells()
	{
		if (!ListWork)
		{
			FaultState = EBattleHudFault::ListNotMade;
			return;
		}
		for (FMenuPrimitive& Info : ListWork->Quads[FBattleHudListWork::Info])
		{
			Info.Clut = EvenTextClut;
		}
		ListWork->bCellsShown = true;
	}

	void FBattleHud::DrawEtherListFrame(uint8 Slot)
	{
		const FBattlePartyInput& Party = Battle->Party;
		const uint8 Cell = Party.List.Cell;
		const uint8 Row = Party.List.TopRow;
		if (Cell != ListLoop.DrawnCell || Row != ListLoop.DrawnRow)
		{
			DrawEtherInfo(Slot, Cell, Row);
			ListLoop.DrawnCell = Cell;
			ListLoop.DrawnRow = Row;
		}
		if (Party.ListScroll != ListLoop.DrawnScroll)
		{
			DrawEtherThumb(Party.ListScroll);
			SetEtherCells(Party.ListScroll);
			ListLoop.DrawnScroll = Party.ListScroll;
		}
		DrawListCursor(static_cast<int16>((Cell & 1) != 0 ? 0xAE : 0x2A), static_cast<int16>((Cell >> 1) * EtherRow + Resources.Tables.Places.EtherCursorY));
	}

	void FBattleHud::DrawEtherInfo(uint8 Slot, uint8 Cell, uint8 Row)
	{
		if (!ListWork)
		{
			FaultState = EBattleHudFault::ListNotMade;
			return;
		}
		if (Slot >= PartySlots)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		const int32 Entry = static_cast<uint8>(Cell + Row * 2);
		if (!EtherLearned(Slot, Entry))
		{
			ListWork->bInfoShown = false;
			return;
		}
		SetEtherCostQuads(Slot, Entry);
		SetEtherLabelQuads(Slot, Entry);
		RenderEtherHelp(Slot, Entry);
		ListWork->InfoBuffer = Buffer;
		ListWork->bInfoShown = true;
	}

	void FBattleHud::SetEtherCostQuads(uint8 Slot, int32 Entry)
	{
		auto& Quads = ListWork->Quads;
		SetQuad(Quads[FBattleHudListWork::Info][Buffer], Resources.Tables.Places.EtherCostX, 0xA4, static_cast<uint8>(Entry % 2 * 0x78 & 0xF8), static_cast<uint8>((Entry >> 1 & 0xF) << 4), 0x60, 0x10);
		ListWork->FigureCount = 0;
		if (Battle->PartyCharacters[Slot] != Elly)
		{
			return;
		}
		// The rate is read from the ability on foot, even in a gear.
		const int32 Index = FBattleContext::PartyCommandIndex(Slot, FirstAbility + Entry);
		if (Index < 0 || Index >= static_cast<int32>(Battle->Commands.size()))
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		const uint8 Rate = std::min<uint8>(static_cast<uint8>(Battle->Commands[static_cast<std::size_t>(Index)].EtherBonus + Battle->Combatants[Slot].Ether), MaxRate);
		const std::array<uint8, 3> Digits{static_cast<uint8>(Rate / 100), static_cast<uint8>(Rate / 10 % 10), static_cast<uint8>(Rate % 10)};
		for (int32 Place = 0; Place < 3; ++Place)
		{
			// Leading zeros are left out, the units always drawn.
			if (Place < 2 && Rate < (Place == 0 ? 100 : 10))
			{
				continue;
			}
			SetTextQuad(Quads[static_cast<std::size_t>(FBattleHudListWork::Figures + ListWork->FigureCount)][Buffer], static_cast<int16>(0x8C + 8 * Place), 0xA6,
				DigitU(Digits[static_cast<std::size_t>(Place)]), 0, 8);
			++ListWork->FigureCount;
		}
	}

	void FBattleHud::SetEtherLabelQuads(uint8 Slot, int32 Entry)
	{
		const FBattleCommand* Command = EtherCommand(Slot, Entry);
		if (Command == nullptr)
		{
			return;
		}
		const uint16 Targeting = Command->Targeting;
		const int32 Kind = (Targeting & 0x4000) != 0 ? GearKindLabel : (Targeting & 0x1000) != 0 ? AllKindLabel : OneKindLabel;
		const int32 Targets = (Targeting & 0xF) == 1 ? AllTargetsLabel : (Targeting & 0xF) == 2 ? PartyTargetLabel : OneTargetLabel;
		SetLabelQuad(ListWork->Quads[FBattleHudListWork::Labels][Buffer], Resources.Tables.Places.EtherKindX, 0xAA, Kind);
		SetLabelQuad(ListWork->Quads[FBattleHudListWork::Labels + 1][Buffer], Resources.Tables.Places.EtherTargetsX, 0xAA, Targets);
	}

	std::array<int32, 2> FBattleHud::RenderHelpLines(int32 File, int32 Entry)
	{
		FVramImage Help = BlankImage(WideUnits);
		const TResult<FMessageTable> Table = FMessageTable::Parse(Resources.HelpTexts[static_cast<std::size_t>(File)]);
		std::array<int32, 2> Widths{};
		for (int32 Line = 0; Line < 2; ++Line)
		{
			const std::span<const uint8> Lines = Table ? Table->String(Entry | Line) : std::span<const uint8>{};
			if (Lines.empty())
			{
				FaultState = EBattleHudFault::MessageOutsideTable;
				return Widths;
			}
			Widths[static_cast<std::size_t>(Line)] = RenderMesString(Text, BoxTyping, Lines, Help, Line);
		}
		Help.Rect.X = HelpX;
		Help.Rect.Y = 0;
		Upload(Help);
		return Widths;
	}

	void FBattleHud::RenderEtherHelp(uint8 Slot, int32 Entry)
	{
		const bool bOnFoot = Battle->ApRecords[Slot].InGear == 0;
		const uint8 Character = Battle->PartyCharacters[Slot];
		int32 Owner = Character;
		if (!bOnFoot)
		{
			const std::size_t Offset = GameStateLayout::CharacterOffset(Character) + GameStateLayout::Character::Gear;
			if (Battle->GameState == nullptr || Offset >= FGameState::BlockSize)
			{
				FaultState = EBattleHudFault::IndexOutsideHud;
				return;
			}
			Owner = BattleGameState::Byte(*Battle->GameState, Offset);
		}
		const std::array<int32, 2> Widths = RenderHelpLines(bOnFoot ? FootHelp : GearHelp, Owner * HelpPerOwner + Entry * 2);
		SetTextQuad(ListWork->Quads[FBattleHudListWork::Help][Buffer], 0x30, 0xB6, 0, 0, static_cast<uint8>(Widths[0]));
		SetTextQuad(ListWork->Quads[FBattleHudListWork::Help + 1][Buffer], 0x30, 0xC6, 0, 0, static_cast<uint8>(Widths[1]));
	}

	void FBattleHud::DrawEtherThumb(int32 Scroll)
	{
		ListThumb.Count = 0;
		for (int32 Piece = 0; Piece < 8; ++Piece)
		{
			ListThumb.Count += DrawSheet(ThumbFrame, ListThumb.Parts, ListThumb.Count, 0x20, static_cast<int16>(0x38 + 8 * Piece + Scroll), FullScale);
		}
		ListThumb.Buffer = Buffer;
		ListThumb.bShown = true;
	}

	void FBattleHud::SetEtherCells(int32 Scroll)
	{
		if (!ListWork)
		{
			FaultState = EBattleHudFault::ListNotMade;
			return;
		}
		auto& Quads = ListWork->Quads;
		Quads[0][Buffer].Clut = EvenTextClut;
		Quads[1][Buffer].Clut = EvenTextClut;
		const uint8 V = static_cast<uint8>(Scroll);
		SetQuad(Quads[0][Buffer], 0x34, 0x34, 0, V, 0x60, 0x60);
		SetQuad(Quads[1][Buffer], 0xB8, 0x34, 0x78, V, 0x60, 0x60);
		SetQuad(Quads[2][Buffer], 0x9C, 0x34, 0, V, 0x10, 0x60);
		SetQuad(Quads[3][Buffer], 0x120, 0x34, 0x40, V, 0x10, 0x60);
		ListWork->CellsBuffer = Buffer;
	}
}
