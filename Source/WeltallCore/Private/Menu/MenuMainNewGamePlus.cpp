#include "Game/GameState.h"
#include "Game/NewGamePlus.h"
#include "Game/OriginalSave.h"
#include "Game/SaveStore.h"
#include "Menu/MenuMain.h"
#include "Menu/MenuNewGamePlusPage.h"
#include "Menu/MenuResources.h"
#include "Menu/OptionsText.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The options screen's two windows: the list's and the help's under it. */
		constexpr int32 ListWindow = 3;
		constexpr int32 HelpWindow = 4;
		constexpr int32 PiecesEntry = 4;
	}

	bool FMenuMain::HasClearSave() const
	{
		if (Services.Saves == nullptr)
		{
			return false;
		}
		for (int32 Slot = 0; Slot < FSaveStore::SlotCount; ++Slot)
		{
			const TResult<std::optional<std::vector<uint8>>> Bytes = Services.Saves->ReadBytes(Slot);
			if (Bytes && *Bytes)
			{
				const TResult<FOriginalSave> Save = FOriginalSave::FromData(**Bytes);
				if (Save && Save->ClearGame() == EClearGame::ClearFile)
				{
					return true;
				}
			}
		}
		return false;
	}

	TMenuTask<FMenuMain::ENewGame> FMenuMain::ChooseNewGame()
	{
		if (!HasClearSave())
		{
			co_return ENewGame::New;
		}
		// The load screen loads the clear save over the title's game, which a step back puts back.
		const FGameState Title = *Services.Game;
		const auto PutBack = [this, &Title]
		{
			*Services.Game = Title;
			bGameLoaded = false;
		};
		while (true)
		{
			const EOfferAnswer Answer = co_await AskOffer(OptionsText::NewGamePlus::Offer, true);
			if (Answer != EOfferAnswer::Right)
			{
				co_return Answer == EOfferAnswer::Left ? ENewGame::New : ENewGame::Back;
			}
			while (co_await PickClearSave())
			{
				const bool bStarted = co_await NewGamePlusScreen();
				bGameLoaded = false;
				if (bStarted)
				{
					co_return ENewGame::Plus;
				}
				PutBack();
			}
		}
	}

	TResult<FGameState> FMenuMain::LoadNewGame() const
	{
		TResult<FGameState> NewGame = LoadNewGameState(*Services.Image, *Services.Index, *Services.Text.Tables);
		if (NewGame)
		{
			// The kernel's own values go on: the play time (from 0 with the fix), what the last program left.
			NewGame->SetPlayTime(Services.Options.bNewGameClockFromZero ? 0 : Services.Game->PlayTime());
			NewGame->ProgramResult = Services.Game->ProgramResult;
			NewGame->BattleActingTurns = Services.Game->BattleActingTurns;
		}
		return NewGame;
	}

	TMenuTask<bool> FMenuMain::NewGamePlusScreen()
	{
		FMenuNewGamePlusPage Page;
		OptionsState = {};
		FMenuOptionsState& State = OptionsState;
		State.bSinglePage = true;
		State.Title = std::string(FMenuNewGamePlusPage::Title());
		State.Rows = Page.Rows();
		AllocCursor(0);
		Options = {};
		bool bChanged = true;
		bool bFirst = true;
		while (true)
		{
			co_await Frame();
			if (bChanged)
			{
				State.Rows = Page.Rows();
				DrawOptionsPage();
				DrawOptionsHelp();
				bChanged = false;
			}
			DrawCursor((State.Row - State.Top + 1) * 2, 0, 0, 0);
			if (bFirst)
			{
				OpenWindow(ListWindow, 0x0C, 10, 0x124, 0x84, false, true, PiecesEntry, true);
				OpenWindow(HelpWindow, 8, 0x8F, 0x130, 0x30, false, true, PiecesEntry, false);
				OpenLayer();
				bFirst = false;
			}
			const int32 Was = State.Row;
			switch (Code)
			{
			case EMenuCode::Up:
				State.Row = std::max(State.Row - 1, 0);
				break;
			case EMenuCode::Down:
				State.Row = std::min(State.Row + 1, FMenuNewGamePlusPage::StartRow);
				break;
			case EMenuCode::Left:
			case EMenuCode::Right:
				bChanged = Page.Switch(State.Row);
				break;
			case EMenuCode::Confirm:
				if (State.Row != FMenuNewGamePlusPage::StartRow)
				{
					bChanged = Page.Switch(State.Row);
					break;
				}
				{
					TResult<FGameState> NewGame = LoadNewGame();
					CloseOptions();
					if (!NewGame)
					{
						co_await Fail(NewGame.Error().Message);
						co_return false;
					}
					// A new game, as the title's New Game makes it, with what the clear save carries over.
					NewGamePlus::Carry(*Services.Game, *NewGame, Page.Carry(), Resources.Items());
					NewGame->ProgramResult = 0;
					*Services.Game = std::move(*NewGame);
					co_return true;
				}
			case EMenuCode::Cancel:
				CloseOptions();
				co_return false;
			default:
				break;
			}
			// The rows in view follow the cursor.
			const int32 LastTop = std::max(static_cast<int32>(State.Rows.size()) - FMenuOptionsScreen::Rows, 0);
			State.Top = std::clamp(std::clamp(State.Top, State.Row - FMenuOptionsScreen::Rows + 1, State.Row), 0, LastTop);
			bChanged = bChanged || State.Row != Was;
		}
	}
}
