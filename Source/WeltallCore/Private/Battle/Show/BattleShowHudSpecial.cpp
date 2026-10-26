#include "Battle/BattleContext.h"
#include "Battle/BattleGameState.h"
#include "Battle/BattleProgram.h"
#include "Battle/Show/BattleHudGpu.h"
#include "Battle/Show/BattleHudLists.h"
#include "Battle/Show/BattleShowHud.h"
#include "Text/MesRender.h"

#include <algorithm>

namespace Weltall
{
	using namespace BattleHudGpu;
	using namespace BattleHudLists;

	namespace
	{
		/** A gear's four Specials: its commands from 25h, their names in the kernel's table 52 by gear, their help in file 35/3. */
		constexpr int32 Specials = 4;
		constexpr int32 FirstSpecial = 0x25;
		constexpr int32 SpecialNameTable = 52;
		constexpr int32 SpecialHelp = 2;
		/** The commands whose disabled bits switch each Special off: the Booster's, the second's, the third's, the fourth's. */
		constexpr std::array<std::size_t, Specials> SwitchCommands{13, 14, 15, 3};
		constexpr std::array<uint32, 3> CostDivisors{1000, 100, 10};
		/**
		 * The most the cost's four figures show. Repair's cost grows with its level, the sum of the Repair parts fitted:
		 * stacked on a large frame it passes 9999, where the original draws its thousands from the pictures' pointers
		 * past the ten digits'. No engine gives that much fuel, so the Special is out of reach at either cost.
		 */
		constexpr uint32 MostCostShown = 9999;
		constexpr int16 SpecialRow = 16;
	}

	bool FBattleHud::SpecialOffered(uint8 Slot, int32 Special)
	{
		const std::size_t Offset = BattleGameState::SkillsOffset(Battle->PartyCharacters[Slot], GameStateLayout::Skills::Special);
		if (Battle->GameState == nullptr || Offset + 2 > FGameState::BlockSize)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return false;
		}
		const bool bLearned = (BattleGameState::Word(*Battle->GameState, Offset) & (0x8000u >> Special)) != 0;
		const uint16 Switch = Battle->Program->CommandBits[SwitchCommands[static_cast<std::size_t>(Special)]];
		return bLearned && (Battle->Combatants[Slot].DisabledCommands & Switch) == 0;
	}

	const FBattleCommand* FBattleHud::SpecialCommand(uint8 Slot, int32 Special)
	{
		const int32 Index = FBattleContext::GearCommandIndex(Slot, FirstSpecial + Special);
		if (Index < 0 || Index >= static_cast<int32>(Battle->Commands.size()))
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return nullptr;
		}
		return &Battle->Commands[static_cast<std::size_t>(Index)];
	}

	void FBattleHud::BuildSpecialList(uint8 Slot)
	{
		if (Slot >= PartySlots)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		OpenListWork();
		RenderLabels();
		FVramImage Blank = BlankImage(WideUnits);
		Blank.Rect.X = HelpX;
		Upload(Blank);
		const auto Place = [&](FVramImage Image, int16 X, int16 Y, int16 Width, int16 Height) {
			Image.Rect = {X, Y, Width, Height};
			Image.Units.resize(static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height), 0);
			Upload(Image);
		};
		const FVramImage BlankDigit = BlankImage(DigitUnits);
		for (int32 Special = 0; Special < Specials && FaultState == EBattleHudFault::None; ++Special)
		{
			const int16 Top = static_cast<int16>(0x100 + Special * SpecialRow);
			const int16 TextY = static_cast<int16>(Top + 2);
			const bool bOffered = SpecialOffered(Slot, Special);
			const FBattleCommand* Command = SpecialCommand(Slot, Special);
			uint32 Cost = bOffered && Command != nullptr ? std::min<uint32>(Command->FuelCost, MostCostShown) : 0;
			Place(BlankImage(0x1B, SpecialRow), 0x380, Top, 0x1B, SpecialRow);
			if (bOffered)
			{
				FVramImage Name = BlankImage(NarrowUnits);
				RenderMesString(Text, ListTyping, TableString(SpecialNameTable, Battle->Combatants[Slot].Gear * Specials + Special), Name, 0);
				Place(Name, 0x380, TextY, NarrowUnits, TextRows);
			}
			Place(BlankImage(0x1B, SpecialRow), HelpX, Top, 0x1B, SpecialRow);
			// The thousands, hundreds and tens, blank until the first not 0; then the units.
			bool bStarted = false;
			for (std::size_t Place10 = 0; Place10 < CostDivisors.size(); ++Place10)
			{
				const uint32 Digit = Cost / CostDivisors[Place10] & 0xFF;
				const int16 X = static_cast<int16>(HelpX + 2 * static_cast<int32>(Place10));
				if (Digit != 0 || bStarted)
				{
					Place(DigitImages[Digit], X, TextY, DigitUnits, TextRows);
					bStarted = true;
					Cost -= Digit * CostDivisors[Place10];
				}
				else
				{
					Place(BlankDigit, X, TextY, DigitUnits, TextRows);
				}
			}
			Place(bOffered ? DigitImages[Cost % 10] : BlankDigit, 0x3C6, TextY, DigitUnits, TextRows);
		}
		auto& Quads = ListWork->Quads;
		Quads[0][Buffer].Clut = EvenTextClut;
		SetQuad(Quads[0][Buffer], Resources.Tables.Places.SpecialListX, 0x54, 0, 0, 0x60, 0x40);
		SetQuad(Quads[2][Buffer], Resources.Tables.Places.SpecialListRightX, 0x54, 0, 0, 0x20, 0x40);
		ListWork->CellsBuffer = Buffer;
		ListWork->bCellsShown = true;
	}

	void FBattleHud::DrawSpecialListFrame(uint8 Slot)
	{
		const uint8 Cell = Battle->Party.List.Cell;
		if (Cell != ListLoop.DrawnCell)
		{
			DrawSpecialInfo(Slot, Cell);
			ListLoop.DrawnCell = Cell;
		}
		DrawListCursor(Resources.Tables.Places.SpecialCursorX, static_cast<int16>(Cell * SpecialRow + Resources.Tables.Places.SpecialCursorY));
	}

	void FBattleHud::DrawSpecialInfo(uint8 Slot, uint8 Cell)
	{
		if (!ListWork)
		{
			FaultState = EBattleHudFault::ListNotMade;
			return;
		}
		if (Slot >= PartySlots || Cell >= Specials)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		if (!SpecialOffered(Slot, Cell))
		{
			ListWork->bInfoShown = false;
			return;
		}
		auto& Quads = ListWork->Quads;
		SetQuad(Quads[FBattleHudListWork::Info][Buffer], Resources.Tables.Places.SpecialCostX, 0xA4, 0, static_cast<uint8>((Cell & 0xF) << 4), 0x60, 0x10);
		if (const FBattleCommand* Command = SpecialCommand(Slot, Cell))
		{
			const uint16 Targeting = Command->Targeting;
			const int32 Kind = (Targeting & 0x4000) != 0 ? GearKindLabel : (Targeting & 0x1000) != 0 ? AllKindLabel : OneKindLabel;
			const int32 Targets = (Targeting & 0xF) == 1 ? AllTargetsLabel : (Targeting & 0xF) == 2 ? PartyTargetLabel : OneTargetLabel;
			SetLabelQuad(Quads[FBattleHudListWork::Labels][Buffer], 0x24, 0xA6, Kind);
			SetLabelQuad(Quads[FBattleHudListWork::Labels + 1][Buffer], Resources.Tables.Places.SpecialTargetsX, 0xA6, Targets);
		}
		const std::array<int32, 2> Widths = RenderHelpLines(SpecialHelp, (Battle->Combatants[Slot].Gear * Specials + Cell) * 2);
		SetTextQuad(Quads[FBattleHudListWork::Help][Buffer], Resources.Tables.Places.SpecialHelpX, 0xB6, 0, 0, static_cast<uint8>(Widths[0]));
		SetTextQuad(Quads[FBattleHudListWork::Help + 1][Buffer], Resources.Tables.Places.SpecialHelpX, 0xC6, 0, 0, static_cast<uint8>(Widths[1]));
		ListWork->InfoBuffer = Buffer;
		ListWork->bInfoShown = true;
	}
}
