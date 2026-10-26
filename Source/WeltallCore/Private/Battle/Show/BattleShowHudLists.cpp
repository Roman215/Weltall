#include "Battle/BattleContext.h"
#include "Battle/Show/BattleHudGpu.h"
#include "Battle/Show/BattleHudLists.h"
#include "Battle/Show/BattleShowHud.h"
#include "Text/MesRender.h"
#include "Text/MessageTable.h"

namespace Weltall
{
	using namespace BattleHudGpu;
	using namespace BattleHudLists;

	namespace
	{
		/** The kernel's name tables the lists read: the battle labels, the items' names. */
		constexpr int32 LabelTable = 18;
		constexpr int32 ItemNameTable = 22;
		constexpr int32 FirstLabel = 10;
		constexpr int32 LabelsPerPlane = 11;
		/** The labels' rows: at (3DEh, 10Dh + 13 per label), the digits' images beside them at (3DEh + 2 per digit, 100h). */
		constexpr int16 LabelX = 0x3DE;
		constexpr int16 LabelY = 0x10D;
		constexpr int16 DigitY = 0x100;

		/** The text quads' pages by mode: 0 at (380h, 0), 1 at (380h, 100h), 2 at (3C0h, 100h), 3 at (3C0h, 0). */
		constexpr std::array<std::array<int16, 2>, 4> TextPages{{{0x380, 0}, {0x380, 0x100}, {0x3C0, 0x100}, {0x3C0, 0}}};
		/** Each quad of the lists' work: its plane and page mode, as the HUD makes them (the last is a sprite's). */
		constexpr std::array<std::array<uint8, 2>, FBattleHudListWork::QuadCount - 1> ListQuadModes{{
			{0, 1}, {0, 1}, {0, 2}, {0, 2}, {0, 1}, {0, 2}, {0, 2}, {0, 2}, {0, 3},
			{1, 3}, {0, 2}, {1, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
		}};

		/** The scroll bar's link and end, the cursor's first frame. */
		constexpr int32 ScrollLinkFrame = 0x65;
		constexpr int32 ScrollEndFrame = 0x64;
		constexpr int32 CursorFrames = 0xE0;

		/** Each list's windows, as its routine opens them: x, y, width, height (the Ether's and Specials' FBattleHudPlaces). */
		constexpr std::array<std::array<int16, 4>, 2> ItemWindows{{{0x20, 0x58, 0x118, 0x78}, {0x10, 0x2C, 0x128, 0x28}}};
		/** The Combo list's two windows grow to their sizes; Billy's ammunition has one. */
		constexpr std::array<std::array<int16, 4>, 2> ComboWindows{{{0x16, 0x5C, 0x122, 0x4C}, {0x10, 0x2C, 0xE8, 0x2C}}};
		constexpr std::array<std::array<int16, 4>, 1> AmmunitionWindows{{{0x98, 0xA8, 0x8C, 0x28}}};

		/** The item list's rows: 13 pixels apart, eight shown, the second plane's from the list's 17th entry on. */
		constexpr int32 ItemRow = 13;
		constexpr int32 ItemRowsShown = 0x68;
		constexpr uint8 ItemListWindow = 2;
		constexpr uint8 EtherListWindow = 1;
		constexpr uint8 SpecialListWindow = 4;
		constexpr uint8 ComboListWindow = 3;
		constexpr int32 ItemRowImages = 32;
		constexpr int32 SecondPlaneItems = 16;
		/** An item's target labels by its target kind. */
		constexpr std::array<int32, 3> TargetLabels{OneTargetLabel, AllTargetsLabel, PartyTargetLabel};

		std::span<const std::array<int16, 4>> ListWindows(EBattleMenuList List, const FBattleHudPlaces& Code)
		{
			switch (List)
			{
			case EBattleMenuList::Items:
				return ItemWindows;
			case EBattleMenuList::Ether:
				return Code.EtherWindows;
			case EBattleMenuList::Combo:
				return ComboWindows;
			case EBattleMenuList::Ammunition:
				return AmmunitionWindows;
			default:
				return Code.SpecialWindows;
			}
		}
	}

	void FBattleHud::OpenListWindow(EBattleMenuList List, int32 Window)
	{
		const std::span<const std::array<int16, 4>> Places = ListWindows(List, Resources.Tables.Places);
		if (Window < 0 || Window >= static_cast<int32>(Places.size()))
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		if (Window == 0)
		{
			ListLoop = {};
		}
		const std::array<int16, 4>& Place = Places[static_cast<std::size_t>(Window)];
		OpenWindow(Window, Place[0], Place[1], static_cast<uint16>(Place[2]), static_cast<uint16>(Place[3]), List == EBattleMenuList::Combo);
	}

	void FBattleHud::OpenListWork()
	{
		ListWork.emplace();
		for (std::size_t Quad = 0; Quad < ListQuadModes.size(); ++Quad)
		{
			const auto [Plane, Mode] = ListQuadModes[Quad];
			for (FMenuPrimitive& Row : ListWork->Quads[Quad])
			{
				BattleHudGpu::Set(Row, TexturedQuad);
				SetColor(Row, 0, FullLevel, FullLevel, FullLevel);
				SetSemiTrans(Row, false);
				SetShadeTex(Row, true);
				Row.Page = MenuGpu::TexturePage(0, 0, TextPages[Mode][0], TextPages[Mode][1]);
				Row.Clut = Plane != 0 ? OddTextClut : EvenTextClut;
			}
		}
	}

	std::span<const uint8> FBattleHud::TableString(int32 Table, int32 Entry)
	{
		const std::span<const uint8> String = Text.Tables != nullptr ? Text.Tables->String(Table, Entry) : std::span<const uint8>{};
		if (String.empty())
		{
			FaultState = EBattleHudFault::MessageOutsideTable;
		}
		return String;
	}

	void FBattleHud::RenderLabels()
	{
		for (int32 Label = 0; Label < LabelsPerPlane; ++Label)
		{
			FVramImage Row = BlankImage(NarrowUnits);
			const uint8 V = static_cast<uint8>(TextRows + TextRows * Label);
			const int32 First = RenderMesString(Text, ListTyping, TableString(LabelTable, FirstLabel + Label), Row, 0);
			const int32 Second = RenderMesString(Text, ListTyping, TableString(LabelTable, FirstLabel + LabelsPerPlane + Label), Row, 1);
			Labels[static_cast<std::size_t>(Label)] = {static_cast<uint8>(First), 0, LabelsU, V};
			Labels[static_cast<std::size_t>(LabelsPerPlane + Label)] = {static_cast<uint8>(Second), 1, LabelsU, V};
			Row.Rect.X = LabelX;
			Row.Rect.Y = static_cast<int16>(LabelY + TextRows * Label);
			Upload(Row);
		}
		for (int32 Digit = 0; Digit < DigitLabels; ++Digit)
		{
			FVramImage Image = DigitImages[static_cast<std::size_t>(Digit)];
			Image.Rect.X = static_cast<int16>(LabelX + 2 * Digit);
			Image.Rect.Y = DigitY;
			Upload(Image);
		}
	}

	void FBattleHud::DrawScrollBar(int16 X, int16 TopY, int16 BottomY, int16 FirstY, int32 Links)
	{
		ScrollBar.Count = 0;
		for (int32 Link = 0; Link < Links; ++Link)
		{
			ScrollBar.Count += DrawSheet(ScrollLinkFrame, ScrollBar.Parts, ScrollBar.Count, X, FirstY + 8 * Link, FullScale);
		}
		ScrollBar.Count += DrawSheet(ScrollEndFrame, ScrollBar.Parts, ScrollBar.Count, X, TopY, FullScale);
		// The bar's bottom end is its top one turned upside down.
		Scratch.clear();
		const int32 Parts = Sheet->DrawFlipped(ScrollEndFrame, X, BottomY, FullScale, false, true, Scratch);
		for (int32 Part = 0; Part < Parts; ++Part)
		{
			if (ScrollBar.Count + Part >= static_cast<int32>(ScrollBar.Parts.size()))
			{
				FaultState = EBattleHudFault::PartsOutsidePrimitives;
				return;
			}
			ScrollBar.Parts[static_cast<std::size_t>(ScrollBar.Count + Part)][Buffer] = Scratch[static_cast<std::size_t>(Part)];
		}
		ScrollBar.Count += Parts;
		ScrollBar.Buffer = Buffer;
		ScrollBar.bShown = true;
	}

	void FBattleHud::BuildItemList(bool bFirst)
	{
		if (bFirst)
		{
			OpenListWork();
			RenderLabels();
			DrawScrollBar(0x20, 0x5C, 0xCC, 0x60, 0xE);
		}
		const FBattleContext& Context = *Battle;
		FVramImage Blank = BlankImage(WideUnits);
		Blank.Rect.X = 0x3C0;
		Upload(Blank);
		const FVramImage BlankDigit = BlankImage(DigitUnits);
		const auto Digit = [&](bool bShown, uint32 Value, int16 X, int16 Y) {
			// A count past 99 would take a tens digit from past the digits' images.
			if (Value >= DigitImages.size())
			{
				FaultState = EBattleHudFault::IndexOutsideHud;
				return;
			}
			FVramImage Image = bShown ? DigitImages[Value] : BlankDigit;
			Image.Rect.X = X;
			Image.Rect.Y = Y;
			Upload(Image);
		};
		for (int32 Row = 0; Row < ItemRowImages; ++Row)
		{
			const std::size_t First = static_cast<std::size_t>(Row);
			const std::size_t Second = First + SecondPlaneItems;
			FVramImage Names = BlankImage(NarrowUnits);
			if (Context.ListedItems[First] != 0)
			{
				RenderMesString(Text, ListTyping, TableString(ItemNameTable, Context.ListedItems[First]), Names, 0);
			}
			if (Context.ListedItems[Second] != 0)
			{
				RenderMesString(Text, ListTyping, TableString(ItemNameTable, Context.ListedItems[Second]), Names, 1);
			}
			const int16 Across = static_cast<int16>(Row % 2);
			const int16 Y = static_cast<int16>(0x100 + Row / 2 * ItemRow);
			Names.Rect.X = static_cast<int16>(0x380 + Across * NarrowUnits);
			Names.Rect.Y = Y;
			Upload(Names);
			const uint8 FirstCount = Context.ListedCounts[First];
			const uint8 SecondCount = Context.ListedCounts[Second];
			Digit(FirstCount / 10 != 0, FirstCount / 10u, static_cast<int16>(0x3C0 + Across * 0x10), Y);
			Digit(Context.ListedItems[First] != 0, FirstCount % 10u, static_cast<int16>(0x3C2 + Across * 0x10), Y);
			Digit(SecondCount / 10 != 0, SecondCount / 10u, static_cast<int16>(0x3C4 + Across * 0x10), Y);
			Digit(Context.ListedItems[Second] != 0, SecondCount % 10u, static_cast<int16>(0x3C6 + Across * 0x10), Y);
		}
	}

	void FBattleHud::ShowListCells()
	{
		if (!ListWork)
		{
			FaultState = EBattleHudFault::ListNotMade;
			return;
		}
		ListWork->bCellsShown = true;
	}

	void FBattleHud::DrawItemListFrame()
	{
		const FBattlePartyInput& Party = Battle->Party;
		const uint8 Cell = Party.List.Cell;
		const uint8 Row = Party.List.TopRow;
		if (Cell != ListLoop.DrawnCell || Row != ListLoop.DrawnRow)
		{
			DrawItemInfo(Cell, Row);
			ListLoop.DrawnCell = Cell;
			ListLoop.DrawnRow = Row;
		}
		if (Party.ListScroll != ListLoop.DrawnScroll)
		{
			DrawItemThumb(Party.ListScroll / ItemRow * 2);
			SetItemCells(Party.ListScroll);
			ListLoop.DrawnScroll = Party.ListScroll;
		}
		const int32 Odd = Cell & 1;
		DrawListCursor(static_cast<int16>(Odd << 7 | (Odd * 2 + 0x2A)), static_cast<int16>((Cell >> 1) * ItemRow + Resources.Tables.Places.ItemCursorY));
	}

	void FBattleHud::DrawItemInfo(uint8 Cell, uint8 Row)
	{
		if (!ListWork)
		{
			FaultState = EBattleHudFault::ListNotMade;
			return;
		}
		const std::size_t Entry = static_cast<std::size_t>(Row) * 2 + Cell;
		if (Entry >= Battle->ListedItems.size())
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		const uint8 Item = Battle->ListedItems[Entry];
		ListWork->bCountShown = false;
		SetItemNameQuads(Cell, Row);
		if (Item != 0)
		{
			RenderItemHelp(Item);
			SetItemCountQuads(Item);
			ListWork->bCountShown = true;
		}
		ListWork->InfoBuffer = Buffer;
		ListWork->bInfoShown = true;
	}

	void FBattleHud::SetItemNameQuads(uint8 Cell, uint8 Row)
	{
		int32 Entry = Row * 2 + Cell;
		uint8 Plane = 0;
		if (Entry > SecondPlaneItems)
		{
			Entry -= SecondPlaneItems;
			Plane = 1;
		}
		FMenuPrimitive& Name = ListWork->Quads[FBattleHudListWork::Info][Buffer];
		Name.Clut = Plane != 0 ? OddTextClut : EvenTextClut;
		const uint8 V = static_cast<uint8>(Entry / 2 * ItemRow);
		SetTextQuad(Name, 0x18, 0x33, static_cast<uint8>(Entry % 2 * 0x78 & 0xF8), V, 0x60);
		SetTextQuad(ListWork->Quads[FBattleHudListWork::Figures][Buffer], 0x84, 0x33, static_cast<uint8>((Entry % 2 * 0x40 & 0xD0) | Plane * 0x10), V, 0x10);
	}

	void FBattleHud::RenderItemHelp(uint8 Item)
	{
		FVramImage Help = BlankImage(WideUnits);
		const TResult<FMessageTable> Table = FMessageTable::Parse(Resources.ItemHelp);
		const std::span<const uint8> Line = Table ? Table->String(Item) : std::span<const uint8>{};
		if (Line.empty())
		{
			FaultState = EBattleHudFault::MessageOutsideTable;
			return;
		}
		const int32 Width = RenderMesString(Text, BoxTyping, Line, Help, 0);
		Help.Rect.X = HelpX;
		Help.Rect.Y = 0;
		Upload(Help);
		SetTextQuad(ListWork->Quads[FBattleHudListWork::Help][Buffer], 0x30, 0x42, 0, 0, static_cast<uint8>(Width));
	}

	void FBattleHud::SetLabelQuad(FMenuPrimitive& Quad, int16 X, int16 Y, int32 Label)
	{
		const FBattleHudLabel& Typed = Labels[static_cast<std::size_t>(Label)];
		SetTextQuad(Quad, X, Y, Typed.U, Typed.V, Typed.Width);
		Quad.Clut = Typed.Plane != 0 ? OddTextClut : EvenTextClut;
	}

	void FBattleHud::SetItemCountQuads(uint8 Item)
	{
		if (Item >= Battle->Items.size())
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		const uint16 Targeting = Battle->Items[Item].Targeting;
		// A target kind past the three leaves the game's label to a register's stale value.
		if ((Targeting & 0xF) >= TargetLabels.size())
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		const int32 Kind = (Targeting & 0x4000) != 0 ? GearKindLabel : (Targeting & 0x1000) != 0 ? AllKindLabel : OneKindLabel;
		SetLabelQuad(ListWork->Quads[FBattleHudListWork::Labels][Buffer], 0xA8, 0x33, Kind);
		SetLabelQuad(ListWork->Quads[FBattleHudListWork::Labels + 1][Buffer], Resources.Tables.Places.ItemTargetsX, 0x33, TargetLabels[Targeting & 0xF]);
	}

	void FBattleHud::DrawItemThumb(int32 Offset)
	{
		ListThumb.Count = 0;
		for (int32 Bullet = 0; Bullet < 9; ++Bullet)
		{
			ListThumb.Count += DrawSheet(ThumbFrame, ListThumb.Parts, ListThumb.Count, 0x20, static_cast<int16>(0x64 + 8 * Bullet + Offset), FullScale);
		}
		ListThumb.Buffer = Buffer;
		ListThumb.bShown = true;
	}

	void FBattleHud::SetItemCells(int32 Scroll)
	{
		if (!ListWork)
		{
			FaultState = EBattleHudFault::ListNotMade;
			return;
		}
		uint16 Clut = EvenTextClut;
		uint8 Beyond = 0;
		if (Scroll >= ItemRowsShown)
		{
			Scroll -= ItemRowsShown;
			Clut = OddTextClut;
			Beyond = 0x10;
		}
		auto& Quads = ListWork->Quads;
		Quads[0][Buffer].Clut = Clut;
		Quads[1][Buffer].Clut = Clut;
		const uint8 V = static_cast<uint8>(Scroll);
		SetQuad(Quads[0][Buffer], Resources.Tables.Places.ItemCellsX, 0x60, 0, V, 0x60, 0x68);
		SetQuad(Quads[1][Buffer], 0xB4, 0x60, 0x78, V, 0x60, 0x68);
		SetQuad(Quads[2][Buffer], Resources.Tables.Places.ItemCellsBeyondX, 0x60, Beyond, V, 0x10, 0x68);
		SetQuad(Quads[3][Buffer], 0x11C, 0x60, static_cast<uint8>(Beyond | 0x40), V, 0x10, 0x68);
		ListWork->CellsBuffer = Buffer;
	}

	void FBattleHud::DrawListCursor(int16 X, int16 Y)
	{
		if (++ListLoop.CursorTicks > 2)
		{
			if (--ListLoop.CursorFrame < 0)
			{
				ListLoop.CursorFrame = 4;
			}
			ListLoop.CursorTicks = 0;
		}
		ListCursor.Count = DrawSheet(CursorFrames + ListLoop.CursorFrame, ListCursor.Parts, 0, X, Y, FullScale);
		ListCursor.Buffer = Buffer;
		ListCursor.bShown = true;
	}

	void FBattleHud::HideListWindows(EBattleMenuList List)
	{
		ListThumb.bShown = false;
		ScrollBar.bShown = false;
		ListCursor.bShown = false;
		for (std::size_t Window = 0; Window < ListWindows(List, Resources.Tables.Places).size(); ++Window)
		{
			Windows[Window].bShown = false;
		}
	}

	void FBattleHud::ShowListWindows(EBattleMenuList List)
	{
		ListThumb.bShown = true;
		ScrollBar.bShown = true;
		ListCursor.bShown = true;
		for (std::size_t Window = 0; Window < ListWindows(List, Resources.Tables.Places).size(); ++Window)
		{
			Windows[Window].bShown = true;
		}
	}

	void FBattleHud::LinkListSprites()
	{
		if (ListThumb.bShown)
		{
			LinkPairs(ListThumb.Parts, ListThumb.Count, ListThumb.Buffer);
		}
		if (ListCursor.bShown)
		{
			LinkPairs(ListCursor.Parts, ListCursor.Count, ListCursor.Buffer);
		}
		if (ScrollBar.bShown)
		{
			LinkPairs(ScrollBar.Parts, ScrollBar.Count, ScrollBar.Buffer);
		}
	}

	void FBattleHud::LinkListQuad(int32 Quad, uint8 From)
	{
		Link(ListWork->Quads[static_cast<std::size_t>(Quad)][From & 1]);
	}

	void FBattleHud::LinkListText()
	{
		const uint8 Window = Battle->Party.ListWindow;
		if (Window == 0)
		{
			return;
		}
		if (!ListWork)
		{
			FaultState = EBattleHudFault::ListNotMade;
			return;
		}
		const FBattleHudListWork& Work = *ListWork;
		if (Work.bCellsShown)
		{
			for (int32 Cell = 0; Cell < 4; ++Cell)
			{
				LinkListQuad(FBattleHudListWork::Cells + Cell, Work.CellsBuffer);
			}
		}
		if (Window == EtherListWindow)
		{
			if (Work.bCellsShown)
			{
				for (int32 Quad = FBattleHudListWork::Points; Quad < FBattleHudListWork::QuadCount; ++Quad)
				{
					LinkListQuad(Quad, Work.FiguresBuffer);
				}
			}
			if (!Work.bInfoShown)
			{
				return;
			}
			if (Work.FigureCount != 0)
			{
				LinkListQuad(FBattleHudListWork::Rate, Work.FiguresBuffer);
			}
			LinkListQuad(FBattleHudListWork::Info, Work.InfoBuffer);
			for (int32 Figure = 0; Figure < Work.FigureCount; ++Figure)
			{
				LinkListQuad(FBattleHudListWork::Figures + Figure, Work.InfoBuffer);
			}
			LinkListQuad(FBattleHudListWork::Help, Work.InfoBuffer);
			LinkListQuad(FBattleHudListWork::Help + 1, Work.InfoBuffer);
			LinkListQuad(FBattleHudListWork::Labels, Work.InfoBuffer);
			LinkListQuad(FBattleHudListWork::Labels + 1, Work.InfoBuffer);
		}
		if (Window == SpecialListWindow && Work.bInfoShown)
		{
			if (Work.FigureCount != 0)
			{
				LinkListQuad(FBattleHudListWork::Rate, Work.FiguresBuffer);
			}
			LinkListQuad(FBattleHudListWork::Info, Work.InfoBuffer);
			LinkListQuad(FBattleHudListWork::Help, Work.InfoBuffer);
			LinkListQuad(FBattleHudListWork::Help + 1, Work.InfoBuffer);
			LinkListQuad(FBattleHudListWork::Labels, Work.InfoBuffer);
			LinkListQuad(FBattleHudListWork::Labels + 1, Work.InfoBuffer);
		}
		if (Window == ComboListWindow && Work.bComboShown)
		{
			Link(Work.ComboBar[Work.FiguresBuffer & 1]);
			Link(Work.ComboRest[Work.FiguresBuffer & 1]);
		}
		if (Window == ItemListWindow)
		{
			if (Work.bInfoShown)
			{
				LinkListQuad(FBattleHudListWork::Info, Work.InfoBuffer);
				LinkListQuad(FBattleHudListWork::Figures, Work.InfoBuffer);
			}
			// The count's labels and the help follow their own flag, whatever the info's.
			if (Work.bCountShown)
			{
				LinkListQuad(FBattleHudListWork::Labels, Work.InfoBuffer);
				LinkListQuad(FBattleHudListWork::Labels + 1, Work.InfoBuffer);
				LinkListQuad(FBattleHudListWork::Help, Work.InfoBuffer);
				LinkListQuad(FBattleHudListWork::Help + 1, Work.InfoBuffer);
			}
		}
	}
}
