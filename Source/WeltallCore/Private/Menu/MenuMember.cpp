#include "Menu/MenuMember.h"

#include "Game/GameState.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 CharacterCount = 16;
		constexpr int32 LastReserved = 10;
		constexpr uint16 CharacterMask = 0x7FF;
		/** L1, R1, Start and Select held together, and nothing else: the console starts again. */
		constexpr uint16 SoftResetButtons = 0x090C;
		constexpr int32 ReserveWindow = 2;
		constexpr int32 PiecesEntry = 4;
		constexpr int32 PartyRows = 3;
		constexpr int32 ReserveRows = 6;
		constexpr int32 ClosingFrames = 2;
		constexpr int16 TextClutX = 0;
		constexpr int16 TextClutY = 0x1D1;
		constexpr int16 WhiteClutY = 0x1C0;
		constexpr uint16 White = 0x7FFF;
		/** The names typed into the page at x 384, two to a strip. */
		constexpr int16 NameStripX = 0x180;
		constexpr int16 NameStripWidth = 0x28;
		constexpr int16 NameStripRows = 13;
		constexpr int32 NamePairs = 6;

		namespace Effect
		{
			constexpr uint8 Cursor = 1;
			constexpr uint8 Confirm = 2;
			constexpr uint8 Cancel = 3;
			constexpr uint8 Buzzer = 4;
		}

		/** Member Change's input: silent (the screen makes its own sounds), the shoulder buttons new presses; L2 ends the queue's reading. */
		constexpr std::array<FMenuInputTest, 13> MemberInput = {{
			{MenuButton::Right, true, EMenuCode::Right, 0},
			{MenuButton::Down, true, EMenuCode::Down, 0},
			{MenuButton::Left, true, EMenuCode::Left, 0},
			{MenuButton::Up, true, EMenuCode::Up, 0},
			{MenuButton::Circle, false, EMenuCode::Confirm, 0},
			{MenuButton::Cross, false, EMenuCode::Cancel, 0},
			{MenuButton::Square, false, EMenuCode::Square, 0},
			{MenuButton::Triangle, false, EMenuCode::Triangle, 0},
			{MenuButton::L1, false, EMenuCode::L1, 0},
			{MenuButton::R1, false, EMenuCode::R1, 0},
			{MenuButton::Start, false, EMenuCode::Start, 0},
			{MenuButton::Select, false, EMenuCode::Select, 0},
			{MenuButton::L2, false, EMenuCode::None, 0},
		}};
	}

	FMenuMember::FMenuMember(const FMenuServices& InServices, const FMenuRequest& InRequest) : FMenuModule(InServices, InRequest)
	{
	}

	TMenuTask<> FMenuMember::Run()
	{
		bSoundOn = true;
		Init();
		Buffer = 0;
		Upload({{TextClutX, TextClutY, 32, 1}, {Resources.TextClut().begin(), Resources.TextClut().end()}});
		RenderLabels(Labels, Resources.Member().Labels, 0);
		FVramImage WhiteClut{{0, WhiteClutY, 16, 1}, std::vector<uint16>(16, 0)};
		WhiteClut.Units[1] = White;
		Upload(WhiteClut);
		InitBackdrop();
		bDrawScene = true;
		co_await ChooseParty();
		co_await Shutdown();
	}

	void FMenuMember::Init()
	{
		const FGameState& Game = *Services.Game;
		const uint16 Allowed = static_cast<uint16>(Game.JoinedCharacters() & Game.MenuCharacters() & CharacterMask);
		for (int32 Character = 0; Character < CharacterCount; ++Character)
		{
			Available[static_cast<std::size_t>(Character)] = (Allowed >> Character & 1) != 0;
		}
		const std::array<uint8, 3> InParty = Game.Party();
		if (Request.bKeepParty)
		{
			for (int32 Character = 0; Character <= LastReserved; ++Character)
			{
				const bool bInParty = std::find(InParty.begin(), InParty.end(), Character) != InParty.end();
				const bool bHasGear = Game.CharacterGear(Character).value_or(0xFF) != 0xFF;
				Available[static_cast<std::size_t>(Character)] = bInParty || (Available[static_cast<std::size_t>(Character)] && (!Request.bRequiresGears || bHasGear));
			}
		}
		bArranged = Services.bArrangesParty && Game.PartyOrder().IsFor(InParty);
		const FPartyOrder::FPlaces Places = bArranged ? Game.PartyOrder().PlacesOf(InParty) : FPartyOrder{}.PlacesOf(InParty);
		for (int32 Row = 0; Row < 3; ++Row)
		{
			const int8 From = Places[static_cast<std::size_t>(Row)];
			GameSlots[static_cast<std::size_t>(Row)] = From;
			const uint8 Character = From == FPartyOrder::Nobody ? FGameState::EmptySlot : InParty[static_cast<std::size_t>(From)];
			const bool bKept = Character != FGameState::EmptySlot && Available[Character];
			Flags[static_cast<std::size_t>(MenuFlag::Party + Row)] = bKept ? Character : FGameState::EmptySlot;
		}
		UploadArchive();
	}

	void FMenuMember::DrawFrame()
	{
		ReadInput(MemberInput);
		if (PadHeld == SoftResetButtons)
		{
			bSoftReset = true;
			return;
		}
		Buffer ^= 1;
		if (bDrawScene)
		{
			GrowWindows();
			AddPointers();
			if (bPanelsShown)
			{
				for (const FMenuMemberPanel& Panel : ReservePanels)
				{
					AddPanel(Panel);
				}
				for (const FMenuMemberPanel& Panel : PartyPanels)
				{
					AddPanel(Panel);
				}
			}
			AddWindows();
		}
		AddBackdrop();
		Drawn.Primitives.clear();
		Table.TakeInto(Drawn.Primitives);
		Drawn.Layer = Layer;
		++Drawn.Number;
	}

	void FMenuMember::BuildReserve()
	{
		// Every available character outside the party, in id order.
		std::size_t Count = 0;
		for (int32 Character = 0; Character <= LastReserved; ++Character)
		{
			bool bOutside = Available[static_cast<std::size_t>(Character)];
			for (int32 Slot = 0; Slot < 3 && bOutside; ++Slot)
			{
				bOutside = Member(Slot) != Character;
			}
			if (bOutside)
			{
				Reserve[Count++] = static_cast<uint8>(Character);
			}
		}
		std::fill(Reserve.begin() + static_cast<std::ptrdiff_t>(Count), Reserve.end(), FGameState::EmptySlot);
		for (int32 Pair = 0; Pair < NamePairs; ++Pair)
		{
			RenderNamePair(Pair);
		}
	}

	void FMenuMember::RenderNamePair(int32 Pair)
	{
		std::fill(NameImage.Units.begin(), NameImage.Units.end(), uint16{0});
		RenderString(NameTyping, NameImage, ShownName(Pair * 2), 0);
		RenderString(NameTyping, NameImage, ShownName(Pair * 2 + 1), 1);
		const FMenuMemberTables& Tables = Resources.Member();
		const std::size_t At = static_cast<std::size_t>(Pair);
		NameImage.Rect = {static_cast<int16>(NameStripX + Tables.NameStripX[At]), static_cast<int16>(Tables.NameStripY[At]), NameStripWidth, NameStripRows};
		Upload(NameImage);
	}

	TMenuTask<> FMenuMember::ChooseParty()
	{
		BuildReserve();
		OpenWindow(ReserveWindow, 0x78, 6, 0xB8, 200, true, false, PiecesEntry, false);
		PointerShown = {};
		while (!Growths[ReserveWindow].bGrown)
		{
			co_await Frame();
		}
		bool bGoesOn = true;
		int32 Column = 0;
		int32 Row = 0;
		int32 Top = 0;
		int32 DrawnTop = 0xFF;
		bool bPicked = false;
		int32 PickedColumn = 0;
		int32 PickedRow = 0;
		int32 PickedTop = 0;
		while (bGoesOn)
		{
			co_await Frame();
			if (Top != DrawnTop)
			{
				co_await BuildPanels(Top);
				DrawnTop = Top;
			}
			PlacePointer(Column, Row, 0);
			switch (Code)
			{
			case EMenuCode::Right:
			case EMenuCode::Left:
				// Across only with nothing picked, from the party to the reserve or back, to its first row.
				if (!bPicked && (Column == 0) == (Code == EMenuCode::Right))
				{
					PlaySound(Effect::Cursor);
					Column ^= 1;
					Row = 0;
				}
				break;
			case EMenuCode::Down:
				PlaySound(Effect::Cursor);
				Row = Column != 0 ? std::min(Row + 1, ReserveRows - 1) : (Row + 1 >= PartyRows ? 0 : Row + 1);
				break;
			case EMenuCode::Up:
				if (Column == 0)
				{
					PlaySound(Effect::Cursor);
					Row = Row == 0 ? PartyRows - 1 : Row - 1;
				}
				else if (Row > 0)
				{
					PlaySound(Effect::Cursor);
					--Row;
				}
				else if (Top > 0)
				{
					// The reserve's first row scrolls back, were it ever scrolled on.
					PlaySound(Effect::Cursor);
					--Top;
				}
				break;
			case EMenuCode::Confirm:
				if (!bPicked)
				{
					bPicked = true;
					PlacePointer(Column, Row, 1);
					PlaySound(Effect::Confirm);
					PickedColumn = Column;
					PickedRow = Row;
					PickedTop = Top;
					Column ^= 1;
					Row = 0;
				}
				else if (Swap(Column, Row, Top, PickedColumn, PickedRow, PickedTop))
				{
					bPicked = false;
					DrawnTop = 0xFF;
					PointerShown[1] = false;
					PlaySound(Effect::Confirm);
				}
				else
				{
					PlaySound(Effect::Buzzer);
				}
				break;
			case EMenuCode::Cancel:
				PlaySound(Effect::Cancel);
				if (bPicked)
				{
					bPicked = false;
					PointerShown[1] = false;
				}
				else
				{
					bGoesOn = false;
				}
				break;
			default:
				break;
			}
		}
		// The pointers taken off a frame before the window closes.
		bPointersShown = false;
		co_await Frame();
		CloseWindow(ReserveWindow);
		// Each row back into the game's slot it shows.
		std::array<uint8, 3> Party{FGameState::EmptySlot, FGameState::EmptySlot, FGameState::EmptySlot};
		for (int32 Shown = 0; Shown < 3; ++Shown)
		{
			const int8 Slot = GameSlots[static_cast<std::size_t>(Shown)];
			if (Slot != FPartyOrder::Nobody)
			{
				Party[static_cast<std::size_t>(Slot)] = Member(Shown);
			}
		}
		Services.Game->SetParty(Party);
	}

	bool FMenuMember::Swap(int32 Column, int32 Row, int32 Top, int32 PickedColumn, int32 PickedRow, int32 PickedTop)
	{
		static_cast<void>(PickedColumn);
		// One of the two is in the party's column, the other in the reserve's.
		const int32 Slot = Column == 0 ? Row : PickedRow;
		const std::size_t Reserved = static_cast<std::size_t>(Column == 0 ? PickedRow + PickedTop : Row + Top);
		uint16 Locked = Services.Game->LockedCharacters();
		if (Request.bKeepParty)
		{
			Locked |= static_cast<uint16>(~(Services.Game->JoinedCharacters() & Services.Game->MenuCharacters()));
		}
		uint8& InParty = Flags[static_cast<std::size_t>(MenuFlag::Party + Slot)];
		uint8& Outside = Reserve[Reserved];
		const auto Movable = [Locked](uint8 Character) { return Character != FGameState::EmptySlot && (Locked >> Character & 1) == 0; };
		if (!Movable(InParty) || !Movable(Outside))
		{
			return false;
		}
		if (bArranged)
		{
			// The newcomer is shown where the one it replaces was.
			FPartyOrder Order = Services.Game->PartyOrder();
			Order.Replace(InParty, Outside);
			Services.Game->SetPartyOrder(Order);
		}
		std::swap(InParty, Outside);
		return true;
	}

	TMenuTask<> FMenuMember::Shutdown()
	{
		// The party written back with its members first, in the game's order.
		const std::array<uint8, 3> Chosen = Services.Game->Party();
		std::array<uint8, 3> Party{FGameState::EmptySlot, FGameState::EmptySlot, FGameState::EmptySlot};
		std::size_t Count = 0;
		for (const uint8 Character : Chosen)
		{
			if (Character != FGameState::EmptySlot)
			{
				Party[Count++] = Character;
			}
		}
		Services.Game->SetParty(Party);
		for (int32 Closing = 0; Closing < ClosingFrames; ++Closing)
		{
			co_await Frame();
		}
		bDrawScene = false;
		co_await Frame();
		do
		{
			co_await Frame();
		} while (Buffer != 0);
	}
}
