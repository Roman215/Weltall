#include "Game/GameState.h"
#include "Menu/MenuMain.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The Normal Menu's commands from the bottom: Exit, File, Gear, Abilities, Items, Equip, Status, and the port's Options. */
		constexpr int32 OptionsCommand = 7;
		constexpr int32 GearCommand = 2;
		/** The gears' names follow the characters' in the state's name table. */
		constexpr int32 FirstGearName = 11;
		constexpr uint8 NoName = 0xFF;
		/** The strips the members' names are rendered into, and after them their gears'. */
		constexpr int32 GearNameStrips = 6;
		/** Names are typed a line of up to 37 units into strips 40 units wide. */
		constexpr int16 NameStripX = 0x180;
		constexpr int16 NameStripWidth = 0x28;
		constexpr int16 NameStripRows = 13;
		/** The gold's window and digits, the time's. */
		constexpr int32 GoldWindow = 0;
		constexpr int32 TimeWindow = 1;
		constexpr int32 PiecesEntry = 4;
		constexpr int32 FullScale = 0x1000;
		constexpr int32 DigitWidth = 8;
		constexpr int32 GoldMarkFrame = 0x10;
		constexpr int32 ColonFrame = 0xEE;
		constexpr uint8 Blank = 0xFF;

		namespace Effect
		{
			constexpr uint8 Buzzer = 4;
			constexpr uint8 NormalMenuOpens = 0x5E;
		}
	}

	void FMenuMain::OpenNormalMenuWindows()
	{
		InitWindowPrims(GoldWindow);
		InitWindowPrims(TimeWindow);
	}

	TMenuTask<> FMenuMain::OpenNormalMenu()
	{
		OpenNormalMenuWindows();
		PlaySound(Effect::NormalMenuOpens);
		RenderPartyNames();
		co_await RevealCommands(NormalArc);
		co_await SlideParty(true, false);
		ShowPlayTime();
	}

	TMenuTask<> FMenuMain::MainMenu()
	{
		const FMenuMainTables& Tables = Resources.Main();
		const int32 LastCommand = static_cast<int32>(NormalArc.size()) - 1;
		bool bGoesOn = true;
		do
		{
			co_await Frame();
			switch (Code)
			{
			case EMenuCode::Up:
				Command = Command + 1 > LastCommand ? 0 : Command + 1;
				break;
			case EMenuCode::Down:
				Command = Command == 0 ? LastCommand : Command - 1;
				break;
			case EMenuCode::Confirm:
				if (Command == GearCommand && MembersWithGears == 0)
				{
					PlaySound(Effect::Buzzer);
					break;
				}
				Arc.bDimmed = true;
				HideSelection();
				ClearFlags(MenuFlag::ArcLabels, static_cast<int32>(ArcLabels.size()));
				bGoesOn = co_await RunCommand(0);
				break;
			case EMenuCode::Cancel:
				bGoesOn = false;
				break;
			case EMenuCode::Right:
				// The port's: the party arranged on its panels, the arc as it stands afterwards.
				if (Services.bArrangesParty && CanArrangeParty())
				{
					co_await ArrangeParty();
					DrawnCommand = 0xFF;
				}
				break;
			default:
				break;
			}
			if (Command != DrawnCommand)
			{
				if (Command == OptionsCommand)
				{
					DrawCommands(NormalArc, Command, FMenuMainTables::ArcPlaces);
					ClearFlags(MenuFlag::ArcLabels, static_cast<int32>(ArcLabels.size()));
				}
				else
				{
					DrawCommands(NormalArc, Command);
					PlaceArcLabel(ArcLabels, Tables.MainLabelOffsets, MenuFlag::ArcLabels, static_cast<int32>(ArcLabels.size()), Command, 0);
				}
				DrawnCommand = Command;
			}
		} while (bGoesOn);
	}

	void FMenuMain::DrawNormalMenu()
	{
		GrowWindows();
		AddLabels();
		AddTargetPointers();
		AddSelection();
		DrawFileDetails();
		DrawCards();
		AddPartyPanels();
		AddStatusSheet();
		AddStats();
		AddEquipNames();
		AddGoldAndTime();
		AddTargetPanels();
		AddCursors();
		AddItemList();
		AddOptionsScreen();
		AddAbilityList();
		AddEquipList();
		AddDeathblows();
		AddScrollKnob();
		AddMemberHints();
		AddWindows();
		if (Flags[MenuFlag::Commands] != 0)
		{
			AddArc(Arc, true);
		}
		if (Flags[MenuFlag::SubCommands] != 0)
		{
			AddArc(SubArc, false);
		}
	}

	void FMenuMain::RenderPartyNames()
	{
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			if (Member(Slot) != FGameState::EmptySlot)
			{
				RenderMemberNames(Slot);
			}
		}
	}

	void FMenuMain::RenderMemberNames(int32 Slot)
	{
		const uint8 Character = Member(Slot);
		RenderName(Character, Slot * 2);
		const uint8 Gear = Services.Game->CharacterGear(Character).value_or(FGameState::EmptySlot);
		RenderName(Gear == FGameState::EmptySlot ? NoName : static_cast<uint8>(Gear + FirstGearName), GearNameStrips + Slot * 2);
	}

	void FMenuMain::RenderName(uint8 Name, int32 Strip)
	{
		// The pair of names the name belongs to, one to a plane, into a strip of its own.
		std::fill(NameImage.Units.begin(), NameImage.Units.end(), uint16{0});
		if (Name != NoName)
		{
			const int32 First = Name >> 1 << 1;
			RenderString(NameTyping, NameImage, ShownName(First), 0);
			RenderString(NameTyping, NameImage, ShownName(First + 1), 1);
		}
		const FMenuMainTables& Tables = Resources.Main();
		const std::size_t Pair = static_cast<std::size_t>(Strip / 2);
		NameImage.Rect = {static_cast<int16>(NameStripX + static_cast<int16>(Tables.NameStripX[Pair])), static_cast<int16>(Tables.NameStripY[Pair]), NameStripWidth,
			NameStripRows};
		Upload(NameImage);
	}

	void FMenuMain::ShowGold()
	{
		OpenWindow(GoldWindow, 0xD4, 0xB2, 0x60, 0x10, false, false, PiecesEntry, false);
		DrawGold(0xD8, 0xB6);
	}

	void FMenuMain::DrawGold(int32 X, int32 Y)
	{
		SplitNumber(Services.Game->Gold());
		GoldDigits.clear();
		for (int32 Digit = 0; Digit < static_cast<int32>(NumberDigits.size()); ++Digit)
		{
			if (NumberDigits[static_cast<std::size_t>(Digit)] != Blank)
			{
				Resources.Sheet().Draw(NumberDigits[static_cast<std::size_t>(Digit)], X + Digit * DigitWidth, Y, FullScale, GoldDigits);
			}
		}
		GoldMark.clear();
		Resources.Sheet().Draw(GoldMarkFrame, X + 0x50, Y, FullScale, GoldMark);
		Flags[MenuFlag::Gold] = 1;
	}

	void FMenuMain::ShowPlayTime()
	{
		OpenWindow(TimeWindow, 0xCC, 0xC6, 0x50, 0x10, false, false, PiecesEntry, false);
		DrawPlayTime(PlayTimeX, PlayTimeY);
		Flags[MenuFlag::PlayTime] = 1;
	}

	void FMenuMain::DrawPlayTime(int32 X, int32 Y)
	{
		// HHH:MM:SS, a digit every 8 pixels and a colon's width between the groups.
		constexpr std::array<int32, 7> DigitX = {0, 8, 16, 32, 40, 56, 64};
		for (std::size_t Digit = 0; Digit < TimeDigits.size(); ++Digit)
		{
			TimeDigits[Digit].clear();
			Resources.Sheet().Draw(static_cast<int32>(PlayTimeDigits[Digit]), X + DigitX[Digit], Y, FullScale, TimeDigits[Digit]);
		}
		TimeColons[0].clear();
		TimeColons[1].clear();
		Resources.Sheet().Draw(ColonFrame, X + 0x18, Y, FullScale, TimeColons[0]);
		Resources.Sheet().Draw(ColonFrame, X + 0x30, Y, FullScale, TimeColons[1]);
	}

	void FMenuMain::AddGoldAndTime()
	{
		if (Flags[MenuFlag::Gold] != 0)
		{
			AddScreen(GoldDigits);
			if (!GoldMark.empty())
			{
				Table.Add(PiecesEntry, GoldMark.front());
			}
		}
		if (Flags[MenuFlag::PlayTime] != 0)
		{
			for (const std::vector<FMenuPrimitive>& Digit : TimeDigits)
			{
				if (!Digit.empty())
				{
					Table.Add(PiecesEntry, Digit.front());
				}
			}
			// Each colon is two dots; each digit one piece.
			for (const std::vector<FMenuPrimitive>& Colon : TimeColons)
			{
				AddScreen(Colon);
			}
		}
	}

}
