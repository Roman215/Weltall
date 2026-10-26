#include "Battling/BattlingMenus.h"

#include "BattlingFightOps.h"
#include "Graphics/GteVectors.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace BattlingFightOps;

		constexpr uint16 ButtonL2 = 0x0001;
		constexpr uint16 ButtonConfirm = 0x0020;
		constexpr uint16 ButtonCancel = 0x0040;
		constexpr uint16 ButtonUp = 0x1000;
		constexpr uint16 ButtonDown = 0x4000;

		constexpr int32 SoundCursor = 0x1E;
		constexpr int32 SoundPause = 0x1F;
		constexpr int32 SoundConfirm = 0x21;
		constexpr int32 SoundCancel = 0x22;
		constexpr int32 SoundBuzz = 0x24;

		constexpr uint8 MaxBlurPresses = 5;
		constexpr int32 StickDeadZone = 0x40;
		/** The practice styles' help messages follow one another from here. */
		constexpr uint8 PracticeStyleHelp = 0x15;
		constexpr uint8 LastPracticeStyle = 13;
		constexpr uint8 LastFrameSkip = 4;

		constexpr uint8 StateFight = 1;
		constexpr uint8 StateLeaving = 3;
		constexpr uint8 StateWinner = 6;

		using namespace BattlingMenuItemFlags;
		using EAction = EBattlingMenuAction;
		using EMenu = EBattlingMenu;

		constexpr int8 MenuNumber(EMenu Menu) { return static_cast<int8>(Menu); }

		/** An item's label: the program's (FBattlingTexts::MenuLabels), by the menu's number. */
		std::string_view ItemLabel(const FBattlingLabels& Labels, EMenu Menu, std::size_t Item)
		{
			const std::vector<std::string>& MenuLabels = Labels.Texts().MenuLabels[static_cast<std::size_t>(Menu)];
			return Item < MenuLabels.size() ? std::string_view(MenuLabels[Item]) : std::string_view();
		}

		constexpr FBattlingMenuItem PauseItems[]{
			{0, 13, EAction::Resume, -1},
			{0, 14, EAction::Open, MenuNumber(EMenu::GiveUp)},
		};
		constexpr FBattlingMenuItem GiveUpItems[]{
			{0, 42, EAction::GiveUp, -1},
			{0, 42, EAction::Open, MenuNumber(EMenu::Pause)},
		};
		constexpr FBattlingMenuItem TopItems[]{
			{0, 1, EAction::Open, MenuNumber(EMenu::Bonus)},
			{Separator, 0, EAction::None, -1},
			{0, 2, EAction::ChooseGears, static_cast<int8>(EBattlingMode::Practice)},
			{0, 3, EAction::StartTutorial, -1},
			{0, 5, EAction::Exit, -1},
		};
		constexpr FBattlingMenuItem BonusItems[]{
			{0, 7, EAction::ChooseGears, static_cast<int8>(EBattlingMode::VersusComputer)},
			{0, 8, EAction::ChooseGears, static_cast<int8>(EBattlingMode::TwoPlayers)},
			{0, 9, EAction::ChooseGears, static_cast<int8>(EBattlingMode::ComputerVersusComputer)},
			{Separator, 0, EAction::None, -1},
			{EveryPass | Value, 10, EAction::StepMatches, -1},
			{EveryPass | Value, 11, EAction::StepLevel, -1},
			{EveryPass | Value, 12, EAction::StepRubberBand, -1},
		};
		constexpr FBattlingMenuItem PlayerOneGearItems[]{
			{EveryPass, 37, EAction::ChoosePlayerOneGear, -1},
			{EveryPass, 41, EAction::StepVibrationOne, -1},
		};
		constexpr FBattlingMenuItem PlayerTwoGearItems[]{
			{EveryPass, 38, EAction::ChoosePlayerTwoGear, -1},
			{EveryPass, 41, EAction::StepVibrationTwo, -1},
		};
		constexpr FBattlingMenuItem PracticePauseItems[]{
			{0, 16, EAction::Resume, -1},
			{EveryPass, 18, EAction::StepPracticeStyle, -1},
			{EveryPass, 20, EAction::StepFrameRate, -1},
			{0, 17, EAction::GiveUp, -1},
		};

		const FBattlingMenuDefinition PauseMenu{PauseItems, EMenu::Pause, 0};
		const FBattlingMenuDefinition GiveUpMenu{GiveUpItems, EMenu::Pause, 0};
		const FBattlingMenuDefinition TopMenu{TopItems, EMenu::Top, 0};
		const FBattlingMenuDefinition BonusMenu{BonusItems, EMenu::Top, 0};
		const FBattlingMenuDefinition PlayerOneGearMenu{PlayerOneGearItems, EMenu::Top, 0};
		const FBattlingMenuDefinition PlayerTwoGearMenu{PlayerTwoGearItems, EMenu::Top, 0};
		const FBattlingMenuDefinition PracticePauseMenu{PracticePauseItems, EMenu::PracticePause, 1};

		/** The menus' layout: the items about the screen's middle, a line 14h pixels, the box 0Ah beyond them each way. */
		constexpr int16 ScreenMiddleX = 0xA0;
		constexpr int16 ItemsMiddleY = 0x6D;
		constexpr int16 BoxMiddleX = 0x96;
		constexpr int16 BoxMargin = 0x0A;

		void CloseToFight(FBattlingFight& Fight)
		{
			SetState(Fight, StateFight);
			Fight.Menus.Current = EMenu::None;
			Fight.Menus.InvalidateHelp();
		}

		void Resume(FBattlingFight& Fight)
		{
			Fight.Menus.bCaptureAfterResume = true;
			CloseToFight(Fight);
			// The last movement sound set to "attacking" starts the gears' walking and dashing sounds again, which the pause stopped.
			for (FBattlingFighter& Fighter : Fight.Fighters)
			{
				Fighter.Flags2 |= BattlingFlags2::LastMovementSound;
			}
		}

		void GiveUp(FBattlingFight& Fight)
		{
			OpenPause(Fight, EMenu::None);
			if (Fight.Settings.WinsPerSet == 0 && Fight.Round != 1)
			{
				--Fight.Round;
				SetState(Fight, StateWinner);
			}
			else
			{
				SetState(Fight, StateLeaving);
			}
		}

		void RunAction(FBattlingFight& Fight, const FBattlingMenuItem& Item, const std::array<FBattlingPad, 2>& Pads)
		{
			FBattlingSettings& Settings = Fight.Settings;
			switch (Item.Action)
			{
			case EAction::None:
				break;
			case EAction::Open:
				OpenMenu(Fight.Menus, static_cast<EMenu>(Item.Argument));
				break;
			case EAction::Resume:
				Resume(Fight);
				break;
			case EAction::GiveUp:
				GiveUp(Fight);
				break;
			case EAction::StepPracticeStyle:
				Settings.PracticeStyle = static_cast<uint8>(StepMenuValue(Fight, Settings.PracticeStyle, LastPracticeStyle, MenuStep::Wraps | MenuStep::OnRepeat));
				break;
			case EAction::StepFrameRate:
				Settings.FrameSkip = static_cast<uint8>(StepMenuValue(Fight, Settings.FrameSkip, LastFrameSkip, MenuStep::OnRepeat));
				break;
			case EAction::ChooseGears:
				StartGearChoice(Fight, static_cast<EBattlingMode>(Item.Argument));
				break;
			case EAction::StartTutorial:
				StartTutorialFromMenus(Fight);
				break;
			case EAction::Exit:
				Fight.Menus.bExited = true;
				break;
			case EAction::StepMatches:
				Settings.WinsPerSet = static_cast<uint8>(StepMenuValue(Fight, Settings.WinsPerSet, 3, MenuStep::OnRepeat));
				break;
			case EAction::StepLevel:
				Settings.Level = static_cast<uint8>(StepMenuValue(Fight, Settings.Level, 2, 0));
				break;
			case EAction::StepRubberBand:
				Settings.bRubberBand = StepMenuValue(Fight, Settings.bRubberBand ? 1 : 0, 1, MenuStep::Wraps) != 0;
				break;
			case EAction::ChoosePlayerOneGear:
				ChooseGear(Fight, 0, Pads);
				break;
			case EAction::ChoosePlayerTwoGear:
				ChooseGear(Fight, 1, Pads);
				break;
			case EAction::StepVibrationOne:
			case EAction::StepVibrationTwo:
			{
				bool& bOn = Settings.Vibration[Item.Action == EAction::StepVibrationOne ? 0 : 1];
				bOn = StepMenuValue(Fight, bOn ? 1 : 0, 1, MenuStep::Wraps) != 0;
				break;
			}
			}
		}

		/** The style's name and the frame rate ending inside the box, beside their items. */
		void PrintPracticeValues(FBattlingFight& Fight, const FBattlingMenuPlace& Place, int16 Cursor)
		{
			const FBattlingTexts& Texts = Fight.Tables.Texts;
			const uint32 Buffer = Fight.Pass & 1;
			FBattlingLabels& Labels = Fight.Labels;
			Labels.SetPosition(Place.BoxX + Place.BoxWidth - BoxMargin, Place.Y);
			Labels.SetFlash(false, Fight.VBlank);
			Labels.PrintRight("", Buffer);
			Labels.SetFlash(Cursor == 1, Fight.VBlank);
			Labels.PrintRight(Texts.PracticeStyles[std::min<std::size_t>(Fight.Settings.PracticeStyle, Texts.PracticeStyles.size() - 1)], Buffer);
			Labels.SetFlash(Cursor == 2, Fight.VBlank);
			using BattlingLabelTexts::PracticeFrameRates;
			const uint8 Rate = PracticeFrameRates[std::min<std::size_t>(Fight.Settings.FrameSkip, PracticeFrameRates.size() - 1)];
			Labels.PrintRight(FormatBattlingText(Texts.FrameRateFormat, {std::to_string(Rate)}), Buffer);
			Labels.SetFlash(false, Fight.VBlank);
		}

		/** The open menu's items printed where the layout put them, the cursor's pulsing; the values beside them. */
		void PrintItems(FBattlingFight& Fight)
		{
			const EMenu Menu = Fight.Menus.Current;
			if (Menu == EMenu::None || Menu == EMenu::PlayerOneGear)
			{
				return;
			}
			const FBattlingMenuDefinition& Definition = BattlingMenuDefinition(Menu);
			const FBattlingMenuPlace Place = BattlingMenuPlace(Menu, Fight.Labels);
			const uint32 Buffer = Fight.Pass & 1;
			const int16 Cursor = Fight.Menus.Cursor(Menu);
			FBattlingLabels& Labels = Fight.Labels;
			Labels.SetPosition(Definition.Layout == 0 ? ScreenMiddleX : Place.X, Place.Y);
			for (std::size_t Item = 0; Item < Definition.Items.size(); ++Item)
			{
				Labels.SetFlash(Cursor == static_cast<int16>(Item), Fight.VBlank);
				if (Definition.Layout == 0)
				{
					Labels.PrintOffset(ItemLabel(Labels, Menu, Item), Place.HalfWidths[Item], Buffer);
				}
				else
				{
					Labels.Print(ItemLabel(Labels, Menu, Item), Buffer);
				}
			}
			if (Menu == EMenu::PracticePause)
			{
				PrintPracticeValues(Fight, Place, Cursor);
			}
			else if (Menu == EMenu::Bonus)
			{
				PrintBonusValues(Fight, Place, Cursor);
			}
		}
	}

	uint8 FBattlingMenus::ItemFlags(EBattlingMenu Menu, std::size_t Item) const
	{
		const std::span<const FBattlingMenuItem> Items = BattlingMenuDefinition(Menu).Items;
		if (Item >= Items.size())
		{
			// What lies past a table's last item: another table's first, never a gap.
			return 0;
		}
		const bool bGearMenu = Menu == EMenu::PlayerOneGear || Menu == EMenu::PlayerTwoGear;
		if (bGearMenu && Item == 1 && VibrationHidden[Menu == EMenu::PlayerOneGear ? 0 : 1])
		{
			return static_cast<uint8>(Items[Item].Flags | Separator);
		}
		return Items[Item].Flags;
	}

	uint8 FBattlingMenus::ItemHelp(EBattlingMenu Menu, std::size_t Item) const
	{
		if ((Menu == EMenu::PlayerOneGear || Menu == EMenu::PlayerTwoGear) && Item == 0)
		{
			return GearHelps[Menu == EMenu::PlayerOneGear ? 0 : 1];
		}
		return BattlingMenuDefinition(Menu).Items[Item].Help;
	}

	FBattlingMenuPlace BattlingMenuPlace(EBattlingMenu Menu, const FBattlingLabels& Labels)
	{
		const FBattlingMenuDefinition& Definition = BattlingMenuDefinition(Menu);
		const int16 Count = static_cast<int16>(Definition.Items.size());
		FBattlingMenuPlace Place;
		int16 Widest = 0;
		for (std::size_t Item = 0; Item < Definition.Items.size(); ++Item)
		{
			Place.HalfWidths[Item] = static_cast<int16>(Labels.Width(ItemLabel(Labels, Menu, Item), FBattlingLabels::OwnSize) / 2);
			Widest = std::max(Widest, Place.HalfWidths[Item]);
		}
		// A value's label stands where the widest does, the values ending at the box's right edge.
		for (std::size_t Item = 0; Item < Definition.Items.size(); ++Item)
		{
			if ((Definition.Items[Item].Flags & Value) != 0)
			{
				Place.HalfWidths[Item] = Widest;
			}
		}
		Place.Y = static_cast<int16>(ItemsMiddleY - Count * FBattlingLabels::LineHeight / 2);
		Place.BoxX = static_cast<int16>(BoxMiddleX - Widest - (Definition.Layout >> 1));
		Place.BoxY = static_cast<int16>(Place.Y - BoxMargin);
		Place.BoxWidth = static_cast<int16>(2 * Widest + 2 * BoxMargin + Definition.Layout);
		Place.BoxHeight = static_cast<int16>((Count + 1) * FBattlingLabels::LineHeight);
		Place.X = static_cast<int16>(ScreenMiddleX - (Definition.Layout >> 1) - Widest);
		return Place;
	}

	const FBattlingMenuDefinition& BattlingMenuDefinition(EBattlingMenu Menu)
	{
		switch (Menu)
		{
		case EMenu::GiveUp:
			return GiveUpMenu;
		case EMenu::Top:
			return TopMenu;
		case EMenu::Bonus:
			return BonusMenu;
		case EMenu::PlayerOneGear:
			return PlayerOneGearMenu;
		case EMenu::PlayerTwoGear:
			return PlayerTwoGearMenu;
		case EMenu::PracticePause:
			return PracticePauseMenu;
		case EMenu::None:
		case EMenu::Pause:
			break;
		}
		return PauseMenu;
	}

	std::vector<uint8> BattlingRoster(const FBattlingGearTable& GearTable, int32 StoryProgress)
	{
		std::vector<uint8> Roster;
		for (std::size_t Gear = 0; Gear < GearTable.Gears.size(); ++Gear)
		{
			if (GearTable.Gears[Gear].Threshold <= StoryProgress)
			{
				Roster.push_back(static_cast<uint8>(Gear));
			}
		}
		return Roster;
	}

	namespace BattlingFightOps
	{
		void OpenMenu(FBattlingMenus& Menus, EBattlingMenu Menu)
		{
			Menus.Previous = Menus.Current;
			Menus.Current = Menu;
		}

		void ShowHelp(FBattlingMenus& Menus, uint8 Message, int32 Line)
		{
			const std::size_t At = static_cast<std::size_t>(Line);
			if (Message != Menus.HelpShown[At])
			{
				Menus.HelpShown[At] = Message;
				Menus.HelpTyped[At] = Message;
			}
		}

		int32 StepMenuValue(FBattlingFight& Fight, int32 Stepped, int32 Last, uint32 Flags)
		{
			const FBattlingMenus& Menus = Fight.Menus;
			const int32 Step = (Flags & MenuStep::Reverses) != 0 ? -1 : 1;
			const uint16 Buttons = (Flags & MenuStep::OnRepeat) != 0 ? Menus.InputRepeated : Menus.InputPressed;
			if ((Buttons & MenuButtonRight) != 0)
			{
				Stepped += Step;
			}
			if ((Buttons & MenuButtonLeft) != 0)
			{
				Stepped -= Step;
			}
			bool bSound = (Buttons & (MenuButtonRight | MenuButtonLeft)) != 0;
			if ((Flags & MenuStep::Wraps) != 0)
			{
				Stepped = Stepped == -1 ? Last : (Last < Stepped ? 0 : Stepped);
			}
			else if (Stepped == -1)
			{
				Stepped = 0;
				bSound = false;
			}
			else if (Last < Stepped)
			{
				Stepped = Last;
				bSound = false;
			}
			if (bSound)
			{
				Fight.Sound.Play(MenuSoundValue, Fight.VBlank);
			}
			return Stepped;
		}

		void ReadMenuInput(FBattlingFight& Fight, EBattlingMenu Menu, int32 PadIndex, const std::array<FBattlingPad, 2>& Pads)
		{
			FBattlingMenus& Menus = Fight.Menus;
			const FBattlingMenuDefinition& Definition = BattlingMenuDefinition(Menu);
			const int32 Count = static_cast<int32>(Definition.Items.size());
			int16& Cursor = Menus.Cursor(Menu);
			const std::size_t At = static_cast<std::size_t>(Cursor);
			if (PadIndex == 0 || Menu == EMenu::PracticePause)
			{
				ShowHelp(Menus, Menus.HelpNext, 0);
				Menus.HelpNext = Menus.ItemHelp(Menu, At);
			}
			const FBattlingPad& Pad = Pads[static_cast<std::size_t>(PadIndex)];
			Menus.InputPad = static_cast<uint8>(PadIndex);
			Menus.InputHeld = Pad.Held;
			Menus.InputPressed = Pad.Pressed;
			Menus.InputRepeated = Pad.Repeated;
			// The menus take the d-pad alone: a stick pushed off its centre buzzes, once. The port's stick that holds
			// directions is a d-pad to them, so only one that holds none buzzes.
			const int32 StickX = Pad.LeftX - 0x80;
			const int32 StickY = Pad.LeftY - 0x80;
			if ((Pad.Type == 3 || Pad.Type == 4) && Pad.StickHeld == 0 && StickDeadZone < SquareRoot0(StickY * StickY + StickX * StickX))
			{
				if (!Menus.bStickBuzzed)
				{
					Fight.Sound.Play(SoundBuzz, Fight.VBlank);
					Menus.bStickBuzzed = true;
				}
			}
			else
			{
				Menus.bStickBuzzed = false;
			}
			const FBattlingMenuItem& Item = Definition.Items[At];
			if (Item.Action != EAction::None)
			{
				if ((Menus.ItemFlags(Menu, At) & EveryPass) != 0)
				{
					RunAction(Fight, Item, Pads);
				}
				else if ((Pad.Pressed & ButtonConfirm) != 0)
				{
					Fight.Sound.Play(SoundConfirm, Fight.VBlank);
					RunAction(Fight, Item, Pads);
				}
			}
			if (Menus.Current == EMenu::None)
			{
				return;
			}
			// Player 2's cancel on its gear menu is its gear item's own; a gear menu's cancel opens its parent even when open.
			if ((Pad.Pressed & ButtonCancel) != 0 && (PadIndex != 1 || Menu != EMenu::PlayerTwoGear))
			{
				const EMenu Back = Menus.Back(Menu);
				int32 Sound = SoundBuzz;
				if (Menus.Current != Back || Back == EMenu::PlayerOneGear || Back == EMenu::PlayerTwoGear)
				{
					OpenMenu(Menus, Back);
					Sound = SoundCancel;
				}
				Fight.Sound.Play(Sound, Fight.VBlank);
			}
			const auto IsSeparator = [&Menus, Menu](int32 Index) {
				return Index >= 0 && (Menus.ItemFlags(Menu, static_cast<std::size_t>(Index)) & Separator) != 0;
			};
			if ((Pad.Repeated & ButtonUp) != 0)
			{
				Fight.Sound.Play(SoundCursor, Fight.VBlank);
				if (--Cursor < 0)
				{
					Cursor = static_cast<int16>(Count - 1);
				}
				if (IsSeparator(Cursor))
				{
					--Cursor;
				}
			}
			if ((Pad.Repeated & ButtonDown) != 0)
			{
				Fight.Sound.Play(SoundCursor, Fight.VBlank);
				const int16 From = Cursor;
				Cursor = static_cast<int16>(From + 1);
				if (IsSeparator(Cursor))
				{
					Cursor = static_cast<int16>(From + 2);
				}
			}
			if (Cursor < 0)
			{
				Cursor = static_cast<int16>(Count - 1);
			}
			if (Count <= Cursor)
			{
				Cursor = 0;
			}
			if (IsSeparator(Cursor))
			{
				--Cursor;
			}
		}

		void OpenPause(FBattlingFight& Fight, EBattlingMenu Menu)
		{
			FBattlingMenus& Menus = Fight.Menus;
			Fight.Sound.StopAll();
			Fight.Sound.Play(SoundPause, Fight.VBlank);
			if (Menu == EMenu::None)
			{
				CloseToFight(Fight);
				return;
			}
			Menus.Current = EMenu::None;
			OpenMenu(Menus, Menu);
			if (Menu == EMenu::Pause)
			{
				Menus.Cursor(EMenu::Pause) = 0;
				// GIVE UP asks with the cursor on NO.
				Menus.Cursor(EMenu::GiveUp) = 1;
			}
			else
			{
				Menus.Cursor(EMenu::PracticePause) = 1;
			}
			Fight.State = 0;
			Menus.bCaptureShown = true;
			Menus.BlurPresses = 0;
			Menus.bCaptureRestorePending = true;
		}

		void TickMenus(FBattlingFight& Fight, const FBattlingTickInput& Input)
		{
			FBattlingMenus& Menus = Fight.Menus;
			Menus.bCaptureRestorePending = false;
			const std::array<FBattlingPad, 2> Pads = MergePads(Input);
			Menus.LastHeld = {Pads[0].Held, Pads[1].Held};
			if (Menus.Current == EMenu::PlayerOneGear)
			{
				TickGearChoice(Fight, Pads);
				return;
			}
			const EMenu Menu = Menus.Current;
			if (Menu != EMenu::None)
			{
				if ((Pads[0].Pressed & ButtonL2) != 0 && Menus.BlurPresses < MaxBlurPresses)
				{
					++Menus.BlurPresses;
				}
				ReadMenuInput(Fight, Menu, Fight.MenuPad == 1 ? 1 : 0, Pads);
			}
			if (Menus.Current == EMenu::PracticePause)
			{
				ShowHelp(Menus, static_cast<uint8>(PracticeStyleHelp + Fight.Settings.PracticeStyle), 1);
			}
			PrintItems(Fight);
		}
	}
}
