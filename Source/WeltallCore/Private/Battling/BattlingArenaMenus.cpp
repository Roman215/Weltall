#include "Battling/BattlingMenus.h"

#include "BattlingFightOps.h"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace BattlingFightOps;
		using EMenu = EBattlingMenu;

		constexpr uint16 ButtonL2 = 0x0001;
		constexpr uint16 ButtonConfirm = 0x0020;
		constexpr uint16 ButtonCancel = 0x0040;
		/** Any button but L2 keeps the menus from their demonstration. */
		constexpr uint16 ButtonsButL2 = 0xFFFE;

		constexpr int32 SoundConfirm = 0x21;
		constexpr int32 SoundCancel = 0x22;

		constexpr uint8 StateFight = 1;
		constexpr uint8 StateDemonstration = 5;
		constexpr uint8 StateNone = 0xFF;

		/** The menus' passes before the demonstration, and the passes of L2 on the top menu that start it at once. */
		constexpr int32 IdlePasses = 18000;
		constexpr int32 L2ShortcutPasses = 0x78;
		constexpr int8 DualShock = 4;

		/** The gear choice's places: the vibration's labels under the names, VS and the names under the portraits. */
		constexpr int32 VibrationY = 0x8C;
		constexpr int32 NamesY = 0x78;
		constexpr std::array<int32, 2> NamesX{0x50, 0xF0};
		constexpr int32 VersusX = 0xA0;
		constexpr int16 BoxMargin = 0x0A;

		/** A carousel's step: a portrait slides 24h pixels, its own place 4 a pass, its neighbour's 2. */
		constexpr int32 SlideDistance = 0x24;
		constexpr int32 FarStep = 4;
		constexpr int32 NearStep = 2;
		constexpr int16 Lit = 0x40;
		/** The portraits' neighbours: the sliding one, the two still ones, and where the sliding one starts, by the slide's sign. */
		struct FNeighbours
		{
			std::array<int32, 3> Places;
			int32 SlideFrom;
		};
		constexpr std::array<FNeighbours, 2> Neighbours{{{{1, -1, 2}, SlideDistance}, {{-1, -2, 1}, -SlideDistance}}};

		/** The gears the demonstration does not show; the menus' cursors on one take Weltall or Vierge instead. */
		bool IsKeptFromDemonstration(uint8 Gear)
		{
			switch (Gear)
			{
			case 4:
			case 7:
			case 11:
			case 30:
			case 31:
				return true;
			default:
				return false;
			}
		}

		uint8 RosterGear(const FBattlingMenus& Menus, int32 Place)
		{
			return Menus.Roster[static_cast<std::size_t>(Place)];
		}

		bool IsDefeated(const FBattlingFight& Fight, uint8 Gear)
		{
			return (Fight.DefeatedGears[Gear >> 3] & (1u << (Gear & 7))) != 0;
		}

		/** The demonstration's start: the gears under the cursors (some replaced), both sides computers, state 5. */
		void StartDemonstration(FBattlingFight& Fight)
		{
			FBattlingMenus& Menus = Fight.Menus;
			uint8 One = RosterGear(Menus, Menus.Gears.Cursors[0]);
			uint8 Two = RosterGear(Menus, Menus.Gears.Cursors[1]);
			if (IsKeptFromDemonstration(One))
			{
				One = Two == BattlingGears::Weltall ? BattlingGears::Vierge : BattlingGears::Weltall;
			}
			if (IsKeptFromDemonstration(Two))
			{
				Two = One != BattlingGears::Vierge ? BattlingGears::Vierge : BattlingGears::Weltall;
			}
			Menus.GearsRead = {One, Two};
			Fight.FireFade = 0;
			SetState(Fight, StateDemonstration);
			Fight.Settings.Computer = {true, true};
			Fight.Settings.Mode = EBattlingMode::Demonstration;
			Menus.bDemonstrationChosen = true;
		}

		/** The gear choice's labels: each side's VIBRATION switch under its name, for a controller with a motor on a human side, else a gap. */
		void PrintGearChoice(FBattlingFight& Fight, const std::array<FBattlingPad, 2>& Pads)
		{
			FBattlingMenus& Menus = Fight.Menus;
			FBattlingGearChoice& Gears = Menus.Gears;
			FBattlingLabels& Labels = Fight.Labels;
			const uint32 Buffer = Fight.Pass & 1;
			for (std::size_t Side = 0; Side < 2; ++Side)
			{
				const EMenu Menu = Side == 0 ? EMenu::PlayerOneGear : EMenu::PlayerTwoGear;
				bool bChoosing = (Gears.Chosen & (1u << Side)) == 0;
				if (Side == 1 && !Gears.bSecondChoosing)
				{
					bChoosing = false;
				}
				Gears.NameFlashes[Side] = bChoosing && Menus.Cursor(Menu) == 0;
				Labels.SetPosition(NamesX[Side], VibrationY);
				const bool bShown = (Pads[Side].Type == DualShock || Pads[Side].bMotor) && !Fight.Settings.Computer[Side];
				if (bShown)
				{
					if (bChoosing)
					{
						Labels.SetFlash(Menus.Cursor(Menu) == 1, Fight.VBlank);
					}
					Labels.PrintCentered(Fight.Settings.Vibration[Side] ? Fight.Tables.Texts.VibrationOn : Fight.Tables.Texts.VibrationOff, Buffer);
				}
				Menus.VibrationHidden[Side] = !bShown;
				Labels.SetFlash(false, Fight.VBlank);
			}
			StepPortraits(Fight);
		}
	}

	void FBattlingFight::StartMenus()
	{
		using namespace BattlingFightOps;
		// The portraits read, then the task's start of the menus: the fire lit, the set's counts cleared, nothing chosen yet.
		Sound.StopAll();
		Menus.Current = EMenu::None;
		OpenMenu(Menus, EMenu::Top);
		Menus.Cursor(EMenu::Top) = 0;
		// Perfect Works' arena keeps bonus battling's cursor where it was left.
		if (!ArenaRules(*this))
		{
			Menus.Cursor(EMenu::Bonus) = 0;
		}
		Settings.Mode = EBattlingMode::None;
		Menus.bCaptureShown = false;
		Menus.InvalidateHelp();
		Menus.bBoutChosen = false;
		Menus.bDemonstrationChosen = false;
		Menus.GearsRead = {-1, -1};
		Menus.Gears.bPortraitsUploaded = false;
		Menus.IdleCountdown = IdlePasses;
		LightFire();
		Round = 0;
		Fighters[0].RoundsWon = 0;
		Fighters[1].RoundsWon = 0;
		Draws = 0;
		SetTime = 0;
		MenuPad = 0;
		Tutorial.Text.Clear();
		State = StateNone;
		Settings.bScripted = false;
		Lighting = 0;
	}

	void FBattlingFight::RunMenusPass(const FBattlingTickInput& Input)
	{
		if (BattlingFightOps::CountMenusDown(*this))
		{
			return;
		}
		++Pass;
		bFrameDrawn = false;
		bWinnerDrawn = false;
		bPauseDrawn = false;
		Menus.HelpTyped = {};
		BattlingFightOps::RunMenusLoop(*this, Input);
	}

	void FBattlingFight::RunLoadPass(bool bLoading)
	{
		using namespace BattlingFightOps;
		++Pass;
		bFrameDrawn = false;
		bWinnerDrawn = false;
		bPauseDrawn = false;
		Menus.HelpTyped = {};
		Menus.Gears.PortraitCount = 0;
		if (Settings.Mode != EBattlingMode::Tutorial)
		{
			Fire.Step(Random);
			StepPortraits(*this);
		}
		bMenusDrawn = true;
		const std::span<const FBattlingLabelGlyph> Letters = Labels.Take(Pass & 1);
		FrameLabels.assign(Letters.begin(), Letters.end());
		PassBlanks = bLoading ? 1 : 2;
		WaitBlanks(*this);
	}

	namespace BattlingFightOps
	{
		bool CountMenusDown(FBattlingFight& Fight)
		{
			FBattlingMenus& Menus = Fight.Menus;
			// The idle countdown reads the buttons the pass before merged; pad 1's L2 held on the top menu is the shortcut to zero.
			if (((Menus.LastHeld[0] | Menus.LastHeld[1]) & ButtonsButL2) != 0)
			{
				Menus.IdleCountdown = IdlePasses;
			}
			if ((Menus.LastHeld[0] & ButtonL2) == 0)
			{
				Menus.L2HeldPasses = 0;
			}
			else
			{
				++Menus.L2HeldPasses;
				if (Menus.Current != EMenu::Top)
				{
					Menus.L2HeldPasses = 0;
				}
				else if (Menus.L2HeldPasses == L2ShortcutPasses)
				{
					Menus.IdleCountdown = 0;
				}
			}
			if (Menus.IdleCountdown == 0)
			{
				if (Fight.FireFade != 0)
				{
					StartDemonstration(Fight);
					return true;
				}
			}
			else
			{
				--Menus.IdleCountdown;
			}
			return false;
		}

		void RunMenusLoop(FBattlingFight& Fight, const FBattlingTickInput& Input)
		{
			FBattlingMenus& Menus = Fight.Menus;
			Fight.bMenusDrawn = false;
			Menus.Gears.PortraitCount = 0;
			TickMenus(Fight, Input);
			if ((Fight.Pass & 1) != 0)
			{
				Fight.Fire.Step(Fight.Random);
			}
			Fight.bMenusDrawn = true;
			const std::span<const FBattlingLabelGlyph> Letters = Fight.Labels.Take(Fight.Pass & 1);
			Fight.FrameLabels.assign(Letters.begin(), Letters.end());
			Fight.Sound.Poll();
			// A bout chosen, the program waits as it does while the gears are read.
			Fight.PassBlanks = Menus.bBoutChosen ? 2 : 1;
			WaitBlanks(Fight);
		}

		void StartGearChoice(FBattlingFight& Fight, EBattlingMode Mode)
		{
			FBattlingMenus& Menus = Fight.Menus;
			FBattlingGearChoice& Gears = Menus.Gears;
			if (!Gears.bPortraitsUploaded)
			{
				Gears.bPortraitsUploaded = true;
				Gears.bPortraitsUploadPending = true;
			}
			Menus.Back(EMenu::PlayerOneGear) = Mode == EBattlingMode::Practice ? EMenu::Top : EMenu::Bonus;
			Menus.Back(EMenu::PlayerTwoGear) = EMenu::PlayerOneGear;
			Menus.GearHelps = Mode == EBattlingMode::ComputerVersusComputer ? std::array<uint8, 2>{39, 40} : std::array<uint8, 2>{37, 38};
			Fight.Settings.Mode = Mode;
			SetState(Fight, StateFight);
			Gears.Chosen = 0;
			Gears.NearSlides = {};
			Gears.FarSlides = {};
			Gears.Shown = Gears.Cursors;
			OpenMenu(Menus, EMenu::PlayerOneGear);
		}

		void StartTutorialFromMenus(FBattlingFight& Fight)
		{
			Fight.Menus.bBoutChosen = true;
			Fight.StartTutorial();
			Fight.Menus.GearsRead = {BattlingGears::Weltall, BattlingGears::Vierge};
		}

		void ChooseGear(FBattlingFight& Fight, int32 Side, const std::array<FBattlingPad, 2>& Pads)
		{
			FBattlingMenus& Menus = Fight.Menus;
			FBattlingGearChoice& Gears = Menus.Gears;
			const uint32 Bit = 1u << Side;
			if (Side == 0)
			{
				if (Menus.InputPad == 0 && (Pads[0].Pressed & ButtonCancel) != 0)
				{
					Gears.Chosen &= ~Bit;
					Menus.GearsRead[0] = -1;
					Fight.Sound.Play(SoundCancel, Fight.VBlank);
				}
			}
			else if ((Menus.InputPressed & ButtonCancel) != 0)
			{
				// Against the computer, player 2's cancel takes both choices back.
				if (Fight.Settings.Mode != EBattlingMode::TwoPlayers)
				{
					Gears.Chosen = 0;
					Menus.GearsRead[1] = -1;
					Fight.Sound.Play(SoundCancel, Fight.VBlank);
					return;
				}
				Gears.Chosen &= ~Bit;
			}
			if ((Gears.Chosen & Bit) != 0)
			{
				return;
			}
			// The cursor skips the other side's gear unless L2 is held, but steps once only when both stood on one. Perfect Works'
			// arena never skips it.
			int32& Cursor = Gears.Cursors[static_cast<std::size_t>(Side)];
			const int32& Other = Gears.Cursors[static_cast<std::size_t>(1 - Side)];
			const bool bSkips = Cursor != Other && !ArenaRules(Fight);
			const int32 Last = static_cast<int32>(Menus.Roster.size()) - 1;
			do
			{
				Cursor = StepMenuValue(Fight, Cursor, Last, MenuStep::Wraps | MenuStep::OnRepeat);
			} while (Cursor == Other && (Menus.InputHeld & ButtonL2) == 0 && bSkips);
			const bool bConfirmed = Side == 0 ? Menus.InputPad == 0 && (Pads[0].Pressed & ButtonConfirm) != 0 : (Menus.InputPressed & ButtonConfirm) != 0;
			if (bConfirmed)
			{
				Gears.Chosen |= Bit;
				Fight.Sound.Play(SoundConfirm, Fight.VBlank);
				Menus.GearsRead[static_cast<std::size_t>(Side)] = RosterGear(Menus, Cursor);
			}
		}

		void TickGearChoice(FBattlingFight& Fight, const std::array<FBattlingPad, 2>& Pads)
		{
			FBattlingMenus& Menus = Fight.Menus;
			FBattlingGearChoice& Gears = Menus.Gears;
			FBattlingSettings& Settings = Fight.Settings;
			// A side's input runs on pad 1 in turn (the second once the first has chosen), or both at once in two players' mode.
			const auto InTurn = [&]() {
				if (Gears.Chosen == 1)
				{
					Gears.bSecondChoosing = true;
					ReadMenuInput(Fight, EMenu::PlayerTwoGear, 0, Pads);
				}
				else if (Gears.Chosen == 0)
				{
					Gears.bSecondChoosing = false;
					ReadMenuInput(Fight, EMenu::PlayerOneGear, 0, Pads);
				}
				else if (Gears.Chosen == 3)
				{
					Menus.bBoutChosen = true;
				}
			};
			switch (Settings.Mode)
			{
			case EBattlingMode::VersusComputer:
			case EBattlingMode::Practice:
				InTurn();
				Settings.Computer = {false, true};
				break;
			case EBattlingMode::ComputerVersusComputer:
				InTurn();
				Settings.Computer = {true, true};
				break;
			case EBattlingMode::TwoPlayers:
				Gears.bSecondChoosing = true;
				Menus.Back(EMenu::PlayerOneGear) = (Gears.Chosen & 1) != 0 ? EMenu::PlayerOneGear : EMenu::Bonus;
				Menus.Back(EMenu::PlayerTwoGear) = (Gears.Chosen & 2) != 0 ? EMenu::PlayerOneGear : EMenu::Bonus;
				ReadMenuInput(Fight, EMenu::PlayerOneGear, 0, Pads);
				ReadMenuInput(Fight, EMenu::PlayerTwoGear, 1, Pads);
				if (Gears.Chosen == 3)
				{
					Menus.bBoutChosen = true;
				}
				Settings.Computer = {false, false};
				break;
			default:
				break;
			}
			PrintGearChoice(Fight, Pads);
		}

		void PrintBonusValues(FBattlingFight& Fight, const FBattlingMenuPlace& Place, int16 Cursor)
		{
			const FBattlingTexts& Texts = Fight.Tables.Texts;
			const uint32 Buffer = Fight.Pass & 1;
			FBattlingLabels& Labels = Fight.Labels;
			Labels.SetFlash(false, Fight.VBlank);
			Labels.SetPosition(Place.BoxX + Place.BoxWidth - BoxMargin, Place.Y);
			for (int32 Line = 0; Line < 4; ++Line)
			{
				Labels.PrintRight("", Buffer);
			}
			Labels.SetFlash(Cursor == 4, Fight.VBlank);
			const uint8 Wins = Fight.Settings.WinsPerSet;
			Labels.PrintRight(Wins == 0 ? Texts.NoLimit : std::to_string(Wins), Buffer);
			Labels.SetFlash(Cursor == 5, Fight.VBlank);
			Labels.PrintRight(Texts.LevelNames[std::min<std::size_t>(Fight.Settings.Level, Texts.LevelNames.size() - 1)], Buffer);
			Labels.SetFlash(Cursor == 6, Fight.VBlank);
			Labels.PrintRight(Fight.Settings.bRubberBand ? Texts.On : Texts.Off, Buffer);
			Labels.SetFlash(false, Fight.VBlank);
		}

		void StepPortraits(FBattlingFight& Fight)
		{
			FBattlingMenus& Menus = Fight.Menus;
			FBattlingGearChoice& Gears = Menus.Gears;
			for (std::size_t Side = 0; Side < 2; ++Side)
			{
				// A move of more than three places is the roster's wrap: it slides the other way.
				int32 Moved = Gears.Shown[Side] - Gears.Cursors[Side];
				if (Moved != 0)
				{
					if (std::abs(Moved) > 3)
					{
						Moved = -Moved;
					}
					const int32 From = Moved < 1 ? SlideDistance : -SlideDistance;
					Gears.NearSlides[Side] = From;
					Gears.FarSlides[Side] = From;
				}
			}
			const auto Toward = [](int32& Slide, int32 Step) {
				if (Slide != 0)
				{
					Slide += Slide < 1 ? Step : -Step;
				}
			};
			for (std::size_t Side = 0; Side < 2; ++Side)
			{
				Toward(Gears.NearSlides[Side], NearStep);
				Toward(Gears.FarSlides[Side], FarStep);
			}
			Gears.Shown = Gears.Cursors;
			const int32 Count = static_cast<int32>(Menus.Roster.size());
			const auto Show = [&](int32 Place, int32 Side, int32 Offset, int32 Brightness) {
				if (Count - 1 < Place)
				{
					Place -= Count;
				}
				if (Place < 0)
				{
					Place += Count;
				}
				FBattlingGearChoice::FShown& Shown = Gears.Portraits[static_cast<std::size_t>(Gears.PortraitCount++)];
				Shown = {RosterGear(Menus, Place), static_cast<uint8>(Side), static_cast<int16>(Offset), static_cast<int16>(Brightness)};
			};
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const std::size_t At = static_cast<std::size_t>(Side);
				const int32 Cursor = Gears.Cursors[At];
				const int32 Far = Gears.FarSlides[At];
				Show(Cursor, Side, Far, (SlideDistance - std::abs(Far)) * Lit / SlideDistance);
				if ((Gears.Chosen & (1u << Side)) != 0)
				{
					continue;
				}
				const int32 Near = Gears.NearSlides[At];
				const FNeighbours& Table = Neighbours[Near < 0 ? 0 : 1];
				Show(Cursor + Table.Places[0], Side, Table.SlideFrom + Near, (std::abs(Near) << 6) / SlideDistance);
				Show(Cursor + Table.Places[1], Side, -SlideDistance, 0);
				Show(Cursor + Table.Places[2], Side, SlideDistance, 0);
			}
			FBattlingLabels& Labels = Fight.Labels;
			const uint32 Buffer = Fight.Pass & 1;
			Labels.SetPosition(VersusX, NamesY);
			Labels.PrintCentered(Fight.Tables.Texts.Versus, Buffer);
			for (std::size_t Side = 0; Side < 2; ++Side)
			{
				const uint8 Gear = RosterGear(Menus, Gears.Cursors[Side]);
				Labels.SetPosition(NamesX[Side], NamesY);
				if (IsDefeated(Fight, Gear))
				{
					Labels.SetFlashBlue(Gears.NameFlashes[Side], Fight.VBlank);
				}
				else
				{
					Labels.SetFlash(Gears.NameFlashes[Side], Fight.VBlank);
				}
				Labels.PrintCentered(Fight.Tables.GearNames[Gear], Buffer);
			}
			Labels.SetFlash(false, Fight.VBlank);
		}
	}
}
