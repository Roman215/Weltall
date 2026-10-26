#include "Battle/BattleContext.h"
#include "Battle/BattleGameState.h"
#include "Battle/Show/BattleHudGpu.h"
#include "Battle/Show/BattleHudLists.h"
#include "Battle/Show/BattleShowHud.h"
#include "Text/MesRender.h"

namespace Weltall
{
	using namespace BattleHudGpu;
	using namespace BattleHudLists;

	namespace
	{
		/** The Combo list's cells: seven Deathblows and the cell that accepts the queue, two to a row of 16 pixels. */
		constexpr int32 ComboCells = 8;
		constexpr int32 AcceptCell = 7;
		constexpr int16 ComboRow = 16;
		constexpr uint8 NoDeathblow = 0xFF;
		/** The battle label typed into the accepting cell, and the kernel's table of the labels. */
		constexpr int32 ComboLabelTable = 18;
		constexpr int32 AcceptLabel = 10;
		/** The queue's pictures: a digit of the stored AP, a queued cell's picture (by its cell), the arrow between two. */
		constexpr int32 DigitFrames = 0x67;
		constexpr int32 QueuedFrames = 0x39;
		constexpr int32 QueueArrowFrame = 10;
		constexpr int16 QueueStep = 0x20;
		/** The stored AP's bar: 4 pixels a point from x 80h to F0h (28 points). */
		constexpr int16 BarLeft = 0x80;
		constexpr int16 BarRight = 0xF0;
		constexpr int16 BarTop = 0x34;
		constexpr int16 BarBottom = 0x3C;

		constexpr uint8 Billy = 4;
		/** The kernel's table of the weapons' names, which the ammunition's are among. */
		constexpr int32 WeaponNameTable = 23;
		/** Billy's two guns' ammunition in his record, and his gear's two in its record. */
		constexpr std::array<std::size_t, 2> GunAmmunition{0x6F, 0x72};
		constexpr std::array<std::size_t, 2> GearGunAmmunition{0x04, 0x07};
		constexpr std::array<uint32, 2> CountDivisors{100, 10};
		constexpr int32 AmmunitionRows = 2;
		constexpr std::array<int16, 4> AmmunitionWindow{0x98, 0xA8, 0x8C, 0x28};
	}

	void FBattleHud::BuildComboList(uint8 Slot)
	{
		if (Slot >= PartySlots)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		const FBattleComboMenu& Combo = Battle->Party.Combo;
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
		for (int32 Cell = 0; Cell < ComboCells && FaultState == EBattleHudFault::None; ++Cell)
		{
			const int16 Across = static_cast<int16>(Cell % 2);
			const int16 Down = static_cast<int16>(Cell / 2 * ComboRow);
			const int16 NameX = static_cast<int16>(0x380 + Across * NarrowUnits);
			const bool bAccept = Cell == AcceptCell;
			const uint8 Deathblow = bAccept ? NoDeathblow : Combo.Offered[static_cast<std::size_t>(Cell)];
			const uint32 Cost = bAccept ? 0 : Combo.Costs[static_cast<std::size_t>(Cell)];
			Place(BlankImage(0x1B, ComboRow), NameX, static_cast<int16>(0x100 + Down), 0x1B, ComboRow);
			if (bAccept || Deathblow != NoDeathblow)
			{
				const std::span<const uint8> Name = bAccept ? TableString(ComboLabelTable, AcceptLabel) : TableString(Battle->PartyCharacters[Slot], Deathblow);
				FVramImage Typed = BlankImage(NarrowUnits);
				RenderMesString(Text, ListTyping, Name, Typed, 0);
				Place(Typed, NameX, static_cast<int16>(0x102 + Down), NarrowUnits, TextRows);
			}
			if (Across == 0)
			{
				Place(BlankImage(0x1B, ComboRow), HelpX, static_cast<int16>(0x100 + Down), 0x1B, ComboRow);
			}
			// A cost past 99 would take a tens digit from past the digits' images.
			if (Cost / 10 >= DigitImages.size())
			{
				FaultState = EBattleHudFault::IndexOutsideHud;
				return;
			}
			const int16 CostX = static_cast<int16>(HelpX + Across * 0x10);
			const int16 CostY = static_cast<int16>(0x102 + Down);
			Place(Cost / 10 != 0 ? DigitImages[Cost / 10] : BlankDigit, CostX, CostY, DigitUnits, TextRows);
			Place(Deathblow != NoDeathblow ? DigitImages[Cost % 10] : BlankDigit, static_cast<int16>(CostX + 2), CostY, DigitUnits, TextRows);
		}
		DrawComboCells();
		auto& Quads = ListWork->Quads;
		Quads[0][Buffer].Clut = EvenTextClut;
		Quads[1][Buffer].Clut = EvenTextClut;
		SetQuad(Quads[0][Buffer], 0x36, 0x62, 0, 0, 0x60, 0x40);
		SetQuad(Quads[1][Buffer], 0xC2, 0x62, 0x78, 0, 0x60, 0x40);
		SetQuad(Quads[2][Buffer], 0x96, 0x62, 0, 0, 0x10, 0x40);
		SetQuad(Quads[3][Buffer], 0x122, 0x62, 0x40, 0, 0x10, 0x40);
		ListWork->CellsBuffer = Buffer;
		ListWork->bCellsShown = true;
	}

	void FBattleHud::DrawComboCells()
	{
		for (FMenuPrimitive& Bar : ListWork->ComboBar)
		{
			Bar = {};
			BattleHudGpu::Set(Bar, GouraudQuad);
			SetColor(Bar, 0, 0xFF, 0, 0);
			SetColor(Bar, 1, 0xFF, 0, 0);
			SetColor(Bar, 2, 0, 0, 0);
			SetColor(Bar, 3, 0, 0, 0);
		}
		for (FMenuPrimitive& Rest : ListWork->ComboRest)
		{
			Rest = {};
			BattleHudGpu::Set(Rest, FlatQuad);
			SetColor(Rest, 0, 0, 0, 0);
		}
		// The game reads sixteen cells where the Combo command lists seven: the eighth is a byte its routine never
		// sets, the rest are zeroes, and every cell but an unlearned Deathblow's draws its sprite.
		const FBattleComboMenu& Combo = Battle->Party.Combo;
		const auto& Sprites = Resources.Tables.ComboSprites;
		ScrollBar.Count = 0;
		for (std::size_t Cell = 0; Cell < Sprites.size(); ++Cell)
		{
			if (Cell < Combo.Offered.size() && Combo.Offered[Cell] == NoDeathblow)
			{
				continue;
			}
			ScrollBar.Count += DrawSheet(Sprites[Cell].Frame, ScrollBar.Parts, ScrollBar.Count, Sprites[Cell].X, Sprites[Cell].Y, FullScale);
		}
		ScrollBar.Buffer = Buffer;
		ScrollBar.bShown = true;
	}

	void FBattleHud::DrawComboListFrame()
	{
		const FBattleComboMenu& Combo = Battle->Party.Combo;
		if (Combo.Queued != ListLoop.DrawnQueued)
		{
			DrawComboQueue(Combo.Pool);
			ListLoop.DrawnQueued = Combo.Queued;
		}
		DrawListCursor(static_cast<int16>((Combo.Cursor & 1) != 0 ? 0xAA : 0x1E), static_cast<int16>((Combo.Cursor >> 1) * ComboRow + 100));
	}

	void FBattleHud::DrawComboQueue(uint8 Pool)
	{
		if (!ListWork)
		{
			FaultState = EBattleHudFault::ListNotMade;
			return;
		}
		const FBattlePartyInput& Party = Battle->Party;
		ListThumb.Count = 0;
		if (Pool / 10 != 0)
		{
			ListThumb.Count += DrawSheet(DigitFrames + Pool / 10, ListThumb.Parts, ListThumb.Count, 0x56, Resources.Tables.Places.ComboTensY, FullScale);
		}
		ListThumb.Count += DrawSheet(DigitFrames + Pool % 10, ListThumb.Parts, ListThumb.Count, 0x5E, Resources.Tables.Places.ComboUnitsY, FullScale);
		for (std::size_t Queued = 0; Queued < Party.ComboQueue.size() && Party.ComboQueue[Queued] != NoDeathblow; ++Queued)
		{
			const int16 X = static_cast<int16>(0x2A + QueueStep * static_cast<int32>(Queued));
			ListThumb.Count += DrawSheet(QueuedFrames + Party.ComboCells[Queued], ListThumb.Parts, ListThumb.Count, X, 0x4A, FullScale);
			if (Queued != 0)
			{
				ListThumb.Count += DrawSheet(QueueArrowFrame, ListThumb.Parts, ListThumb.Count, X - 0x10, 0x4A, FullScale);
			}
		}
		ListThumb.Buffer = Buffer;
		ListThumb.bShown = true;
		const int16 Filled = static_cast<int16>(Pool * 4 + BarLeft);
		SetCorners(ListWork->ComboBar[Buffer], BarLeft, BarTop, Filled, BarTop, BarLeft, BarBottom, Filled, BarBottom);
		SetCorners(ListWork->ComboRest[Buffer], Filled, BarTop, BarRight, BarTop, Filled, BarBottom, BarRight, BarBottom);
		ListWork->FiguresBuffer = Buffer;
		ListWork->bComboShown = true;
	}

	void FBattleHud::HideComboList()
	{
		ListThumb.bShown = false;
		ScrollBar.bShown = false;
		ListCursor.bShown = false;
		Arrows.bShown = false;
	}

	uint8 FBattleHud::StateByte(std::size_t Offset)
	{
		if (Battle->GameState == nullptr || Offset >= FGameState::BlockSize)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return 0;
		}
		return BattleGameState::Byte(*Battle->GameState, Offset);
	}

	void FBattleHud::BuildAmmunition(uint8 Slot)
	{
		if (Slot >= PartySlots)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		// The two guns' ammunition and how much is left: Billy's own on foot, his gear's in it.
		using namespace GameStateLayout;
		const bool bOnFoot = Battle->ApRecords[Slot].InGear == 0;
		const std::size_t Record = CharacterOffset(Billy);
		std::array<uint8, AmmunitionRows> Kinds{};
		std::array<uint32, AmmunitionRows> Counts{};
		for (std::size_t Row = 0; Row < Kinds.size(); ++Row)
		{
			if (bOnFoot)
			{
				Kinds[Row] = StateByte(Record + GunAmmunition[Row]);
				Counts[Row] = StateByte(AmmunitionCountsOffset + Kinds[Row]);
			}
			else
			{
				Kinds[Row] = StateByte(GearOffset(StateByte(Record + Character::Gear)) + GearGunAmmunition[Row]);
				Counts[Row] = StateByte(StoryFlagsOffset + Kinds[Row]);
			}
		}
		if (FaultState != EBattleHudFault::None)
		{
			return;
		}
		OpenListWork();
		RenderLabels();
		const auto Place = [&](FVramImage Image, int16 X, int16 Y, int16 Width, int16 Height) {
			Image.Rect = {X, Y, Width, Height};
			Image.Units.resize(static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height), 0);
			Upload(Image);
		};
		const FVramImage BlankDigit = BlankImage(DigitUnits);
		for (int32 Row = 0; Row < AmmunitionRows && FaultState == EBattleHudFault::None; ++Row)
		{
			const std::size_t At = static_cast<std::size_t>(Row);
			const int16 Top = static_cast<int16>(0x100 + Row * ComboRow);
			const int16 TextY = static_cast<int16>(Top + 2);
			Place(BlankImage(0x1B, ComboRow), 0x380, Top, 0x1B, ComboRow);
			if (Kinds[At] != 0)
			{
				FVramImage Name = BlankImage(NarrowUnits);
				RenderMesString(Text, ListTyping, TableString(WeaponNameTable, Kinds[At]), Name, 0);
				Place(Name, 0x380, TextY, NarrowUnits, TextRows);
			}
			Place(BlankImage(0x1B, ComboRow), HelpX, Top, 0x1B, ComboRow);
			// The hundreds and tens, blank until the first not 0; then the units.
			uint32 Count = Counts[At];
			bool bStarted = false;
			for (std::size_t Place10 = 0; Place10 < CountDivisors.size(); ++Place10)
			{
				const uint32 Digit = Count / CountDivisors[Place10];
				const int16 X = static_cast<int16>(HelpX + 2 * static_cast<int32>(Place10));
				if (Digit != 0 || bStarted)
				{
					Place(DigitImages[Digit], X, TextY, DigitUnits, TextRows);
					bStarted = true;
					Count -= Digit * CountDivisors[Place10];
				}
				else
				{
					Place(BlankDigit, X, TextY, DigitUnits, TextRows);
				}
			}
			Place(Kinds[At] != 0 ? DigitImages[Count % 10] : BlankDigit, 0x3C4, TextY, DigitUnits, TextRows);
		}
		auto& Quads = ListWork->Quads;
		Quads[0][Buffer].Clut = EvenTextClut;
		SetQuad(Quads[0][Buffer], 0xA0, 0xAC, 0, 0, 0x60, 0x20);
		SetQuad(Quads[2][Buffer], 0x104, 0xAC, 0, 0, 0x18, 0x20);
		ListWork->CellsBuffer = Buffer;
		ListWork->bCellsShown = true;
		OpenWindow(0, AmmunitionWindow[0], AmmunitionWindow[1], static_cast<uint16>(AmmunitionWindow[2]), static_cast<uint16>(AmmunitionWindow[3]), false);
	}
}
