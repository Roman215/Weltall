#include "Game/GameState.h"
#include "Game/KernelState.h"
#include "Menu/MenuMain.h"
#include "Menu/MenuResources.h"

namespace Weltall
{
	namespace
	{
		/** The title's commands follow the Normal Menu's in the table of screens: Sound, Continue, New Game; then the port's Exit. */
		constexpr int32 TitleCommandOffset = 7;
		constexpr int32 TitleCommands = 3;
		constexpr int32 TitleExit = 3;
		/** The Normal Menu's Status command. */
		constexpr int32 StatusCommand = 6;
		constexpr int32 EquipCommand = 5;
		constexpr int32 ItemsCommand = 4;
		constexpr int32 AbilitiesCommand = 3;
		constexpr int32 GearCommand = 2;
		constexpr int32 SoundCommand = 7;
		constexpr int32 ContinueCommand = 8;
		constexpr int32 NewGameCommand = 9;
		constexpr int32 ExitCommand = 0;
		constexpr int32 FileCommand = 1;
		constexpr int32 QuitCommand = TitleExit + TitleCommandOffset;
		/** Load Game's result once Continue has loaded a save. */
		constexpr uint8 SaveLoaded = 2;
		/** On the first disc, the title gives up after this many frames without Up or Down. */
		constexpr uint32 TitleIdleFrames = 600;
		/** Load Game's result when the title timed out (2 is a game loaded, which the File screen gives). */
		constexpr uint8 TimedOut = 1;
		/** The inner arc's labels are cleared six at a time, as the original clears them. */
		constexpr int32 SubArcLabelsCleared = 6;
		constexpr int32 TitleArcLabelsBase = 4;
		constexpr int32 SoundLabelsBase = 4;
		/** The Sound option's commands from the bottom: Mono, Wide-Stereo, Stereo; and the output mode each sets. */
		constexpr std::array<int32, 3> OutputModeOfCommand = {0, 2, 1};
		constexpr std::array<int32, 3> CommandOfOutputMode = {0, 2, 1};
	}

	TMenuTask<> FMenuMain::TitleMenu()
	{
		const FMenuMainTables& Tables = Resources.Main();
		bool bGoesOn = true;
		co_await RevealCommands(TitleArc);
		RenderLabels(ArcLabels, Tables.TitleLabels, TitleArcLabelsBase);
		IdleFrames = 0;
		while (true)
		{
			co_await Frame();
			switch (Code)
			{
			case EMenuCode::Up:
				Command = TitleCommandAt((TitlePlace(Command) + 1) % static_cast<int32>(TitleArc.size()));
				IdleFrames = 0;
				break;
			case EMenuCode::Down:
				Command = TitleCommandAt((TitlePlace(Command) + static_cast<int32>(TitleArc.size()) - 1) % static_cast<int32>(TitleArc.size()));
				IdleFrames = 0;
				break;
			case EMenuCode::Confirm:
				Arc.bDimmed = true;
				HideSelection();
				ClearFlags(MenuFlag::ArcLabels, static_cast<int32>(ArcLabels.size()));
				bGoesOn = co_await RunCommand(TitleCommandOffset);
				bSoftResetAllowed = false;
				IdleFrames = 0;
				break;
			default:
				break;
			}
			if (Command != DrawnCommand)
			{
				DrawCommands(TitleArc, TitlePlace(Command));
				if (Command == TitleExit)
				{
					ClearFlags(MenuFlag::ArcLabels, static_cast<int32>(ArcLabels.size()));
				}
				else
				{
					PlaceArcLabel(ArcLabels, Tables.TitleLabelOffsets, MenuFlag::ArcLabels, static_cast<int32>(ArcLabels.size()), Command, TitlePlace(Command) - Command);
				}
				DrawnCommand = Command;
			}
			if (Services.Disc == 1 && IdleFrames > TitleIdleFrames)
			{
				bGoesOn = false;
				Services.Game->ProgramResult = TimedOut;
			}
			if (!bGoesOn)
			{
				ClearFlags(MenuFlag::ArcLabels, static_cast<int32>(ArcLabels.size()));
				co_return;
			}
		}
	}

	TMenuTask<bool> FMenuMain::RunCommand(int32 Offset)
	{
		bool bGoesOn = true;
		bool bReturns = true;
		bCardWatchInAsk = true;
		switch (Command + Offset)
		{
		case FileCommand:
			bReturns = co_await FileScreen(false, bFileStaysAfterSave);
			if (bGameLoaded)
			{
				// Loaded in play: the menu closes on the loaded game, the party's panels staying out, for its map.
				Services.Game->ProgramResult = SaveLoaded;
				bGoesOn = false;
			}
			break;
		case StatusCommand:
			bReturns = co_await StatusScreen();
			break;
		case EquipCommand:
			bReturns = co_await EquipScreen(FirstMember);
			break;
		case ItemsCommand:
			bReturns = co_await ItemsScreen();
			break;
		case AbilitiesCommand:
			co_await AbilitiesScreen(FirstMember, true, 0);
			bReturns = true;
			break;
		case GearCommand:
			bReturns = co_await GearScreen();
			break;
		case SoundCommand:
			// The title's Sound, or the Normal Menu's eighth orb: the options screen where the port has the settings.
			bReturns = Services.Settings != nullptr ? co_await OptionsScreen() : co_await SoundOption();
			if (bGameLoaded)
			{
				// The second disc told as the original from where the original stands: the game goes on there.
				Services.Game->ProgramResult = SaveLoaded;
				bGoesOn = false;
			}
			break;
		case ContinueCommand:
			if (co_await FileScreen(true, false))
			{
				Services.Game->ProgramResult = SaveLoaded;
				bGoesOn = false;
			}
			break;
		case NewGameCommand:
		{
			TResult<FGameState> NewGame = LoadNewGameState(*Services.Image, *Services.Index, *Services.Text.Tables);
			if (!NewGame)
			{
				co_await Fail(NewGame.Error().Message);
			}
			else
			{
				// The kernel's own values go on: the play time (from 0 with the fix), what the last program left.
				NewGame->SetPlayTime(Services.Options.bNewGameClockFromZero ? 0 : Services.Game->PlayTime());
				NewGame->ProgramResult = Services.Game->ProgramResult;
				NewGame->BattleActingTurns = Services.Game->BattleActingTurns;
				*Services.Game = std::move(*NewGame);
				// A new game's state clears the chained battle too (and the battle AI's globals, which the port keeps nowhere yet).
				if (Services.Kernel != nullptr)
				{
					Services.Kernel->ChainedBattle = 0;
				}
			}
			bReturns = false;
			bGoesOn = false;
			break;
		}
		case ExitCommand:
			bReturns = false;
			bGoesOn = false;
			break;
		case QuitCommand:
			// The game closes: nothing was opened, so the title stands as it was meanwhile.
			Services.Quit();
			bReturns = false;
			break;
		default:
			co_await Fail("the Menu's command " + std::to_string(Command + Offset) + " is not built yet");
			break;
		}
		Files.Listing = 0;
		if (bReturns)
		{
			CloseLayer();
			if (Request.Mode == EMenuMode::Normal && bGoesOn)
			{
				co_await SlideParty(true, false);
			}
			else if (Request.Mode == EMenuMode::LoadGame)
			{
				RenderLabels(ArcLabels, Resources.Main().TitleLabels, TitleArcLabelsBase);
			}
		}
		co_await CloseCommand(Offset);
		if (Flags[MenuFlag::MemberHintsMade] != 0)
		{
			Flags[MenuFlag::MemberHints] = 0;
			Flags[MenuFlag::MemberHintsMade] = 0;
		}
		Arc.bDimmed = false;
		Arc.bDimmedApplied = true;
		Flags[MenuFlag::Pointer] = 1;
		Flags[MenuFlag::Selection] = 1;
		DrawnCommand = 0xFF;
		Flags[MenuFlag::SubCommands] = 0;
		bSoftResetAllowed = true;
		co_return bGoesOn;
	}

	int32 FMenuMain::TitlePlace(int32 TitleCommand) const
	{
		const int32 Foot = static_cast<int32>(TitleArc.size()) - TitleCommands;
		return TitleCommand == TitleExit ? 0 : TitleCommand + Foot;
	}

	int32 FMenuMain::TitleCommandAt(int32 Place) const
	{
		const int32 Foot = static_cast<int32>(TitleArc.size()) - TitleCommands;
		return Place < Foot ? TitleExit : Place - Foot;
	}

	TMenuTask<bool> FMenuMain::SoundOption()
	{
		const FMenuMainTables& Tables = Resources.Main();
		bool bStays = true;
		bool bFirst = true;
		bool bApplies = false;
		while (true)
		{
			co_await Frame();
			if (bFirst)
			{
				RenderLabels(SubArcLabels, Tables.SoundLabels, SoundLabelsBase);
				co_await RevealSubCommands(0);
				DrawnSubCommand = 0xFF;
				const int32 Mode = Services.Sound != nullptr ? Services.Sound->OutputMode() : 1;
				SubCommand = CommandOfOutputMode[static_cast<std::size_t>(Mode & 3) % CommandOfOutputMode.size()];
				bFirst = false;
			}
			if (SubCommand != DrawnSubCommand)
			{
				PlaceArcLabel(SubArcLabels, Tables.SoundLabelOffsets, MenuFlag::SubArcLabels, SubArcLabelsCleared, SubCommand, 7);
				DrawSubCommands(0);
				DrawnSubCommand = SubCommand;
			}
			switch (Code)
			{
			case EMenuCode::Up:
				SubCommand = SubCommand + 1 >= SubCommandCount ? 0 : SubCommand + 1;
				break;
			case EMenuCode::Down:
				SubCommand = SubCommand == 0 ? SubCommandCount - 1 : SubCommand - 1;
				break;
			case EMenuCode::Confirm:
				bApplies = true;
				bStays = false;
				break;
			case EMenuCode::Cancel:
				bStays = false;
				break;
			default:
				break;
			}
			if (!bStays)
			{
				if (bApplies && Services.Sound != nullptr)
				{
					Services.Sound->SetOutputMode(OutputModeOfCommand[static_cast<std::size_t>(SubCommand)]);
				}
				ClearFlags(MenuFlag::SubArcLabels, static_cast<int32>(SubArcLabels.size()));
				Flags[MenuFlag::Pointer] = 0;
				Flags[MenuFlag::Selection] = 0;
				co_return true;
			}
		}
	}

	TMenuTask<> FMenuMain::RequireDisc(int32 Disc)
	{
		// "INSERT DISC 1", "INSERT DISC 2".
		constexpr uint8 InsertDiscText = 0x83;
		constexpr uint8 TextsPerDisc = 3;
		OpenLayer();
		while (LayerState != EMenuLayer::Still)
		{
			co_await Frame();
		}
		InitTargetPointers(0);
		// The port reads both discs: where the other disc is asked for, its message stays until confirmed rather than
		// until the drive's lid has opened and closed on it, and no disc is ever wrong. A disc the player has not put
		// where the game looks is looked for at each confirmation, its message saying so until it is there.
		if (Services.Disc != Disc + 1)
		{
			const uint8 Message = static_cast<uint8>(InsertDiscText + TextsPerDisc * Disc);
			co_await ShowMessage(Message);
			while (true)
			{
				do
				{
					co_await Frame();
				} while (Code != EMenuCode::Confirm);
				if (Services.Discs == nullptr || Services.Discs->HasDisc(Disc))
				{
					break;
				}
				if (co_await AddDiscThroughPicker(Disc, Message))
				{
					break;
				}
			}
			co_await CloseMessage();
		}
		Services.Disc = Disc + 1;
		if (Services.Kernel != nullptr)
		{
			Services.Kernel->Disc = Disc;
		}
		Flags[MenuFlag::TargetPointers] = 0;
	}

	void FMenuMain::InitTargetPointers(int32 Kind)
	{
		// The four of the target windows and the messages (Kinds 2 and 0), the Equip screen's one (3).
		Flags[MenuFlag::TargetPointers] = 0;
		if (Kind == 0)
		{
			Flags[MenuFlag::TargetPointers] = 1;
			Files.bPointerOnCursor = true;
		}
		if (Kind == 0 || Kind == 2)
		{
			const FMenuMainTables& Tables = Resources.Main();
			for (std::size_t Place = 0; Place < TargetPointers.size(); ++Place)
			{
				PlaceTargetPointer(Place, Tables.TargetPointerX[Place], Tables.TargetPointerY[Place]);
			}
		}
		else if (Kind == 3)
		{
			PlaceTargetPointer(0, 0, 0);
		}
	}

	void FMenuMain::PlaceTargetPointer(std::size_t Place, int32 X, int32 Y)
	{
		constexpr int32 PointerFrame = 0x108;
		constexpr int32 HalfScale = 0x800;
		TargetPointers[Place].clear();
		Resources.Sheet().Draw(PointerFrame, X, Y, HalfScale, TargetPointers[Place]);
	}
}
