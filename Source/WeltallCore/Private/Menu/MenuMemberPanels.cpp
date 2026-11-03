#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Menu/MenuMember.h"

namespace Weltall
{
	namespace
	{
		using namespace GameStateLayout;

		constexpr int32 PartyStep = 0x38;
		constexpr int32 ReserveStep = 0x20;
		constexpr int32 LastReserved = 10;
		constexpr int32 FirstFacesFrame = 0x14E;
		constexpr int32 PointerFrame = 0x108;
		constexpr int32 FullScale = 0x1000;
		constexpr int32 HalfScale = 0x800;
		constexpr int32 PiecesEntry = 4;
		constexpr int32 NoLabel = 0xFFFF;
		constexpr int32 DigitWidth = 8;
		constexpr uint8 Blank = 0xFF;
		constexpr uint8 Unshaded = 0x80;
		constexpr int32 NamePageX = 0x180;
		constexpr int32 NameWidth = 0x48;
		constexpr int32 NameRows = 13;
		constexpr int32 TextClutY = 0x1D1;
		constexpr int32 OddClutX = 16;
		/** The reserve's pointer places follow the party's three. */
		constexpr int32 ReservePointers = 3;
		/** The panels' label that begins the EP's: the sheet's M (of "MP") in the original, its E with the fix. */
		constexpr std::size_t EpInitial = 6;
		constexpr int32 LetterE = 0x0E;
	}

	TMenuTask<> FMenuMember::BuildPanels(int32 Top)
	{
		const FMenuMemberTables& Tables = Resources.Member();
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			FMenuMemberPanel& Panel = PartyPanels[static_cast<std::size_t>(Slot)];
			if (Member(Slot) == FGameState::EmptySlot)
			{
				Panel.bShown = false;
			}
			else
			{
				BuildPanel(Panel, Member(Slot), Slot, Tables.Party, PartyStep);
			}
		}
		bPanelsShown = true;
		// The reserve's rows one a frame.
		for (int32 Row = 0; Row < static_cast<int32>(ReservePanels.size()); ++Row)
		{
			const int32 Entry = Top + Row;
			if (Entry > LastReserved)
			{
				co_return;
			}
			FMenuMemberPanel& Panel = ReservePanels[static_cast<std::size_t>(Row)];
			const uint8 Character = Reserve[static_cast<std::size_t>(Entry)];
			if (Character == FGameState::EmptySlot)
			{
				Panel.bShown = false;
			}
			else
			{
				co_await Frame();
				BuildPanel(Panel, Character, Row, Tables.Reserve, ReserveStep);
			}
		}
	}

	void FMenuMember::BuildPanel(FMenuMemberPanel& Panel, uint8 Character, int32 Row, const FMenuPanelPlaces& Places, int32 Step)
	{
		const FMenuMemberTables& Tables = Resources.Member();
		const FMenuSpriteSheet& Sheet = Resources.Sheet();
		const int32 Down = Step * Row;
		Panel.Labels.clear();
		for (std::size_t Label = 0; Label < FMenuPanelPlaces::Labels; ++Label)
		{
			const int32 Frame = Label == EpInitial && Services.Options.bMemberChangeSaysEp ? LetterE : Tables.PanelLabels[Label];
			if (Frame != NoLabel)
			{
				Sheet.Draw(Frame, Places.X[Label], Places.Y[Label] + Down, FullScale, Panel.Labels);
			}
		}
		Panel.Portrait.clear();
		Sheet.Draw(FirstFacesFrame + DrawnCharacter(Character), Places.X[FMenuPanelPlaces::Portrait], Places.Y[FMenuPanelPlaces::Portrait] + Down, FullScale, Panel.Portrait);

		// The name from its pair's strip, in its plane.
		FMenuPrimitive& Name = Panel.Name;
		Name.Code = MenuGpu::TexturedQuad;
		Name.Corners[0].Color = {Unshaded, Unshaded, Unshaded};
		Name.Page = MenuGpu::TexturePage(0, 0, NamePageX, 0);
		Name.Clut = MenuGpu::Clut((Character & 1) != 0 ? OddClutX : 0, TextClutY);
		const int16 Left = static_cast<int16>(Places.X[FMenuPanelPlaces::Name]);
		const int16 Top = static_cast<int16>(Places.Y[FMenuPanelPlaces::Name] + Down);
		Name.Corners[0].X = Left;
		Name.Corners[0].Y = Top;
		Name.Corners[1].X = static_cast<int16>(Left + NameWidth);
		Name.Corners[1].Y = Top;
		Name.Corners[2].X = Left;
		Name.Corners[2].Y = static_cast<int16>(Top + NameRows);
		Name.Corners[3].X = static_cast<int16>(Left + NameWidth);
		Name.Corners[3].Y = static_cast<int16>(Top + NameRows);
		const std::size_t Pair = static_cast<std::size_t>(Character / 2);
		SetTexelRect(Name, (Tables.NameStripX[Pair] << 2) & 0xFF, Tables.NameStripY[Pair] & 0xFF, NameWidth, NameRows);

		// The level, HP and EP right-aligned in their places, the maxima packed from the left.
		const std::span<const uint8> Record = Services.Game->Block().subspan(CharacterOffset(Character), CharacterSize);
		const auto Place = [&Places, Down](std::size_t Which) { return std::array<int32, 2>{Places.X[Which], Places.Y[Which] + Down}; };
		const auto Number = [&](std::vector<FMenuPrimitive>& Digits, uint32 Value, int32 First, int32 Count, std::size_t Which, bool bPacked)
		{
			const std::array<int32, 2> At = Place(Which);
			DrawDigits(Digits, Value, First, Count, At[0], At[1], bPacked);
		};
		Number(Panel.Level, Record[Character::Level], 6, 3, FMenuPanelPlaces::Level, false);
		Number(Panel.Hp, LittleEndian::ReadU16(Record, Character::Hp), 6, 3, FMenuPanelPlaces::Hp, false);
		Number(Panel.MaxHp, LittleEndian::ReadU16(Record, Character::MaxHp), 6, 3, FMenuPanelPlaces::MaxHp, true);
		Number(Panel.Ep, LittleEndian::ReadU16(Record, Character::Ep), 7, 2, FMenuPanelPlaces::Ep, false);
		Number(Panel.MaxEp, LittleEndian::ReadU16(Record, Character::MaxEp), 7, 2, FMenuPanelPlaces::MaxEp, true);
		Panel.bShown = true;
	}

	void FMenuMember::DrawDigits(std::vector<FMenuPrimitive>& Digits, uint32 Value, int32 First, int32 Count, int32 X, int32 Y, bool bPacked)
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

	void FMenuMember::PlacePointer(int32 Column, int32 Row, int32 Which)
	{
		const FMenuMemberTables& Tables = Resources.Member();
		const std::size_t Place = static_cast<std::size_t>((Column != 0 ? ReservePointers : 0) + Row);
		std::vector<FMenuPrimitive>& Placed = Pointers[static_cast<std::size_t>(Which)];
		Placed.clear();
		Resources.Sheet().Draw(PointerFrame, Tables.PointerX[Place], Tables.PointerY[Place], HalfScale, Placed);
		PointerShown[static_cast<std::size_t>(Which)] = true;
		bPointersShown = true;
	}

	void FMenuMember::AddPointers()
	{
		if (!bPointersShown)
		{
			return;
		}
		for (std::size_t Which = 0; Which < Pointers.size(); ++Which)
		{
			if (PointerShown[Which] && !Pointers[Which].empty())
			{
				Table.Add(PiecesEntry, Pointers[Which].front());
			}
		}
	}

	void FMenuMember::AddPanel(const FMenuMemberPanel& Panel)
	{
		if (!Panel.bShown)
		{
			return;
		}
		if (!Panel.Portrait.empty())
		{
			Table.Add(PiecesEntry, Panel.Portrait.front());
		}
		Table.Add(PiecesEntry, Panel.Name);
		for (const std::vector<FMenuPrimitive>* Parts : {&Panel.Labels, &Panel.Level, &Panel.Hp, &Panel.MaxHp, &Panel.Ep, &Panel.MaxEp})
		{
			AddScreen(*Parts);
		}
	}
}
