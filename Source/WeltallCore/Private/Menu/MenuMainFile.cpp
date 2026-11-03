#include "Game/GameState.h"
#include "Game/OriginalSave.h"
#include "Game/SaveLoad.h"
#include "Game/SaveStore.h"
#include "Game/SaveWrite.h"
#include "Menu/MenuMain.h"
#include "Story/StoryWayBack.h"
#include "Text/MesFont.h"

namespace Weltall
{
	// The original's File screen on two memory cards, without what it does only because a card is slow:
	// the messages over the card's work ("Checking Memory Cards.", "Loading", "Saving", "Copying",
	// "Deleting"), the access bar, and the frames spent reading, writing and waiting for the card; where the
	// original's lists are read in those frames, they are read at once.

	namespace
	{
		constexpr int32 FileLabelCount = 6;
		constexpr int32 FileArcOffset = 7;
		/** The sub-arc's rows: the title's (Load), where saving is not allowed (Copy, Delete), Save's, and the port's with Load over Save. */
		constexpr int32 TitleFileRow = 7;
		constexpr int32 NoSaveFileRow = 9;
		constexpr int32 SaveFileRow = 0;
		constexpr int32 LoadSaveFileRow = FMenuMainTables::SubArcRows - 1;
		/** The sub-arc's commands from the bottom; Load over Save in the port's row. */
		constexpr int32 DeleteCommand = 0;
		constexpr int32 CopyCommand = 1;
		constexpr int32 LoadSaveCommand = 2;
		constexpr int32 TopLoadCommand = 3;
		constexpr uint8 Free = FMenuFileScreen::Free;
		constexpr int32 CellsPerCard = FMenuFileScreen::CellsPerCard;
		constexpr int32 PerCard = FMenuFileScreen::PerCard;
		/** A cursor place chosen, or the cells left. */
		constexpr int32 Chosen = 1;
		constexpr int32 Cancelled = 2;
		/** The cursor's modes: any file, a save, any cell. */
		constexpr int32 AnyFile = 0;
		constexpr int32 AnySave = 1;
		constexpr int32 AnyCell = 2;
		/** Where the cards' lists are read: as the original's loops ask them, and after a command. */
		constexpr uint8 ListsAfterCommand = 1;
		constexpr uint8 ListsAsked = 2;

		namespace Text
		{
			constexpr uint8 NoData = 0x62;
			constexpr uint8 LoadThisFile = 0x65;
			constexpr uint8 CannotRead = 0x3E;
			constexpr uint8 Finished = 0x5C;
			constexpr uint8 NoFreeBlocks = 0xAC;
			constexpr uint8 SaveToFreeBlock = 0x5F;
			constexpr uint8 Overwrite = 0x38;
			constexpr uint8 SaveFailed = 0x35;
			/** "Copy this file to the Memory Card in Slot 1?", "...the same file exists...", by the card copied to; then slot 2's. */
			constexpr std::array<uint8, 2> CopyToCard = {0x47, 0xBB};
			constexpr std::array<uint8, 2> SameFileOnCard = {0x4D, 0xC1};
			constexpr uint8 CopyFailed = 0x44;
			constexpr uint8 DeleteThisFile = 0x56;
			constexpr uint8 SureToDelete = 0x59;
		}

		namespace Effect
		{
			constexpr uint8 Buzzer = 4;
			constexpr uint8 CardDone = 0x34;
		}
	}

	TMenuTask<bool> FMenuMain::FileScreen(bool bFromTitle, bool bStaysAfterSave)
	{
		bool bGoesOn = true;
		bool bFirst = true;
		bool bResult = true;
		bSoftResetAllowed = false;
		// Load at the title and in play where saving is not allowed, Save where it is (under the port's Load), else Copy.
		SubCommand = FileArcRow(false) == NoSaveFileRow ? CopyCommand : LoadSaveCommand;
		DrawnSubCommand = 0xFF;
		RenderFileLabels();
		co_await Frame();
		while (true)
		{
			Flags[MenuFlag::FileDetails] = 0;
			Files.Drawn = 1;
			if (bFirst)
			{
				InitCardCells();
				InitDetailsBand();
				OpenLayer();
				if (Request.Mode == EMenuMode::Normal)
				{
					co_await SlideParty(false, false);
				}
				co_await RevealSubCommands(FileArcRow(false));
				Flags[MenuFlag::PlayTime] = 0;
				bFirst = false;
				Flags[MenuFlag::Windows + 1] = 0;
				co_await OpenStores();
				Flags[MenuFlag::Cards] = 1;
			}
			// After a copy or a delete the cards are read again at once, where the original covers its reading with a message.
			const bool bRescan = Files.bRescan;
			co_await ScanStores(!bRescan);
			Files.bRescan = false;
			if (SubCommand != DrawnSubCommand)
			{
				if (SubCommand < static_cast<int32>(Resources.Main().FileLabelOffsets.size()))
				{
					PlaceArcLabel(FileLabels, Resources.Main().FileLabelOffsets, MenuFlag::FileLabels, FileLabelCount, SubCommand, FileArcOffset);
				}
				else
				{
					ClearFlags(MenuFlag::FileLabels, FileLabelCount);
				}
				DrawSubCommands(FileArcRow(true));
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
				HideSelection();
				ClearFlags(MenuFlag::FileLabels, FileLabelCount);
				bGoesOn = co_await RunFileCommand(bStaysAfterSave);
				DrawnSubCommand = 0xFF;
				RenderFileLabels();
				if (Request.Mode != EMenuMode::LoadGame)
				{
					bGoesOn = false;
					if (bFromTitle)
					{
						bResult = false;
					}
				}
				break;
			case EMenuCode::Cancel:
				bGoesOn = false;
				if (bFromTitle)
				{
					bResult = false;
				}
				break;
			case EMenuCode::L1:
				TurnStorePair(-1);
				break;
			case EMenuCode::R1:
				TurnStorePair(1);
				break;
			default:
				break;
			}
			if (!bGoesOn)
			{
				break;
			}
		}
		Flags[MenuFlag::Cards] = 0;
		Flags[MenuFlag::FileDetails] = 0;
		Flags[MenuFlag::Pointer] = 0;
		Flags[MenuFlag::Selection] = 0;
		ClearFlags(MenuFlag::FileLabels, FileLabelCount);
		UpdateStoreLists();
		co_return bResult;
	}

	int32 FMenuMain::FileArcRow(bool bDrawn) const
	{
		const bool bPlay = Request.Mode == EMenuMode::Normal;
		if (Request.Mode == EMenuMode::LoadGame || (bPlay && Services.bLoadsInPlay && Request.Param == 0))
		{
			return TitleFileRow;
		}
		if (bPlay && Services.bLoadsInPlay)
		{
			return LoadSaveFileRow;
		}
		// Where saving is not allowed the original reveals Copy and Delete alone, then draws them from Save's row.
		return bPlay && Request.Param == 0 && !bDrawn ? NoSaveFileRow : SaveFileRow;
	}

	bool FMenuMain::FileCommandLoads(int32 FileCommand) const
	{
		return FileCommand == TopLoadCommand || (FileCommand == LoadSaveCommand && FileArcRow(true) == TitleFileRow);
	}

	void FMenuMain::RenderFileLabels()
	{
		const FMenuMainTables& Tables = Resources.Main();
		RenderLabels(FileLabels, Tables.FileLabels[Request.Mode == EMenuMode::LoadGame ? 1 : 0], 4);
	}

	TMenuTask<> FMenuMain::OpenStores()
	{
		while (LayerState != EMenuLayer::Still)
		{
			co_await Frame();
		}
		Files.ListTaken = {false, false};
		Files.Listing = ListsAsked;
		UpdateStoreLists();
		co_await ScanStores(false);
	}

	TMenuTask<> FMenuMain::ScanStores(bool bShowsFrame)
	{
		Files.Listing = ListsAsked;
		if (bShowsFrame)
		{
			co_await Frame();
		}
		else
		{
			UpdateStoreLists();
		}
		bSoundOn = false;
		for (int32 Card = 0; Card < FMenuFileScreen::Cards; ++Card)
		{
			const std::size_t At = static_cast<std::size_t>(Card);
			const uint8 Count = Files.Listed[At];
			if (Count == 0)
			{
				for (int32 File = 0; File < PerCard; ++File)
				{
					Files.Files[static_cast<std::size_t>(Card * PerCard + File)].bShown = false;
				}
				Files.Scanned[At] = Free;
				continue;
			}
			if (Count == Files.Scanned[At])
			{
				continue;
			}
			// A card listed anew is read again whole, its lists read as the original's frames between its reads read them.
			UpdateStoreLists();
			for (int32 Cell = 0; Cell < PerCard; ++Cell)
			{
				Files.Cells[static_cast<std::size_t>(Card * PerCard + Cell)] = Free;
			}
			Files.CellsFilled = 0;
			Files.UsedBlocks[At] = 0;
			for (int32 File = 0; File < Count; ++File)
			{
				ReadStoreFile(Card, File);
				LoadFileIcon(Card * PerCard + File);
				Files.Files[static_cast<std::size_t>(Card * PerCard + File)].bShown = true;
				UpdateStoreLists();
			}
			for (int32 File = Count; File < PerCard; ++File)
			{
				Files.Files[static_cast<std::size_t>(Card * PerCard + File)].bShown = false;
			}
			Files.Scanned[At] = Count;
		}
		MarkGameSaves(0);
		MarkGameSaves(1);
		if (Flags[MenuFlag::TargetPointers] != 0 && Files.bPointerOnCursor && !TargetPointers[0].empty() && CursorCell() != FMenuFileScreen::NoCell)
		{
			// The pointer over the cursor's cell, half the cell's width to its right and above it.
			const FMenuMainTables& Tables = Resources.Main();
			const std::size_t Cell = static_cast<std::size_t>(CursorCell());
			const int16 X = static_cast<int16>(Tables.CellX[Cell]);
			const int16 Y = static_cast<int16>(Tables.CellY[Cell]);
			std::array<FMenuCorner, 4>& Corners = TargetPointers[0].front().Corners;
			Corners[0].X = static_cast<int16>(X + 8);
			Corners[0].Y = static_cast<int16>(Y - 6);
			Corners[1].X = static_cast<int16>(X + 0x18);
			Corners[1].Y = static_cast<int16>(Y - 6);
			Corners[2].X = static_cast<int16>(X + 8);
			Corners[2].Y = static_cast<int16>(Y + 10);
			Corners[3].X = static_cast<int16>(X + 0x18);
			Corners[3].Y = static_cast<int16>(Y + 10);
		}
		bSoundOn = true;
	}

	TMenuTask<bool> FMenuMain::RunFileCommand(bool bStaysAfterSave)
	{
		InitTargetPointers(0);
		Flags[MenuFlag::TargetPointers] = 0;
		bool bGoesOn = true;
		bool bDone = false;
		switch (SubCommand)
		{
		case CopyCommand:
			bDone = co_await FileCopy();
			break;
		case DeleteCommand:
			bDone = co_await FileDelete();
			break;
		case LoadSaveCommand:
		case TopLoadCommand:
			if (FileCommandLoads(SubCommand))
			{
				bDone = co_await FileLoad();
			}
			else
			{
				bDone = co_await FileSave(bStaysAfterSave);
			}
			break;
		default:
			break;
		}
		if (bDone)
		{
			bGoesOn = false;
		}
		Flags[MenuFlag::TargetPointers] = 0;
		co_return bGoesOn;
	}

	TMenuTask<bool> FMenuMain::FileLoad()
	{
		bool bLoaded = true;
		bool bGoesOn = true;
		bool bFirst = true;
		ResetCellCursor();
		do
		{
			co_await ScanStores(true);
			if (bFirst)
			{
				bFirst = false;
				TargetPointerShown[0] = true;
			}
			if (!IsCellSelectable(AnySave))
			{
				Files.Cursor = FirstSelectableCell(AnySave);
				if (Files.Cursor == Free)
				{
					Flags[MenuFlag::TargetPointers] = 0;
					Flags[MenuFlag::FileDetails] = 0;
					Files.Drawn = 1;
					bLoaded = false;
					co_await Ask(Text::NoData, NoSecondText, false);
					break;
				}
			}
			Flags[MenuFlag::TargetPointers] = 1;
			const int32 Result = UpdateCellCursor(AnySave);
			if (Result == Chosen && bPicksClearSave && Files.Files[Files.Cells[static_cast<std::size_t>(CursorCell())]].ClearGame != EClearGame::ClearFile)
			{
				// A New Game+ starts from a save of the clear file's game alone: the others buzz.
				PlaySound(Effect::Buzzer);
			}
			else if (Result == Chosen)
			{
				Files.bPointerOnCursor = false;
				Files.Listing = 0;
				if (co_await Ask(Text::LoadThisFile) == 0)
				{
					Files.DetailsCursor = Free;
				}
				else
				{
					bSoundOn = false;
					Flags[MenuFlag::TargetPointers] = 0;
					Flags[MenuFlag::FileDetails] = 0;
					if (!LoadStoreSave(Files.Files[Files.Cells[static_cast<std::size_t>(CursorCell())]].Data))
					{
						co_await Ask(Text::CannotRead, NoSecondText, false);
						Files.Listing = ListsAsked;
					}
					else
					{
						bGameLoaded = true;
						bGoesOn = false;
						bSoundOn = true;
						PlaySound(Effect::CardDone);
						bSoundOn = false;
						co_await Ask(Text::Finished, NoSecondText, false);
					}
					Files.ListTaken = {false, false};
					Files.Scanned = {Free, Free};
				}
				Files.bPointerOnCursor = true;
			}
			else if (Result == Cancelled)
			{
				bGoesOn = false;
				bLoaded = false;
				bSoundOn = true;
			}
		} while (bGoesOn);
		Files.Listing = ListsAfterCommand;
		co_return bLoaded;
	}

	TMenuTask<bool> FMenuMain::FileSave(bool bStaysAfterSave)
	{
		bool bFirst = true;
		bool bGoesOn = true;
		ResetCellCursor();
		while (true)
		{
			co_await ScanStores(true);
			if (bFirst)
			{
				bFirst = false;
				TargetPointerShown[0] = true;
			}
			if (!IsCellSelectable(AnyCell))
			{
				Files.Cursor = FirstSelectableCell(AnyCell);
				if (Files.Cursor == Free)
				{
					co_await Ask(Text::NoFreeBlocks, NoSecondText, false);
					break;
				}
			}
			Flags[MenuFlag::TargetPointers] = 1;
			const int32 Result = UpdateCellCursor(AnyCell);
			if (Result == Chosen)
			{
				Files.bPointerOnCursor = false;
				Files.Listing = 0;
				const int32 Card = Files.Cursor >= CellsPerCard ? 1 : 0;
				// Each choice picks its own slot: the original keeps an overwritten save's slot for every later save of
				// the visit, so a later save into a free cell takes that slot again and fails on its own new save.
				int32 Replaced = Free;
				bool bWrites = true;
				const uint8 File = Files.Cells[static_cast<std::size_t>(CursorCell())];
				if (File == Free)
				{
					if (co_await Ask(Text::SaveToFreeBlock) == 0)
					{
						bWrites = false;
						Files.DetailsCursor = Free;
					}
				}
				else if (co_await Ask(Text::Overwrite) == 0)
				{
					bWrites = false;
					Files.DetailsCursor = Free;
				}
				else
				{
					// The original erases the save here, before the new one is written; the store puts the new one in its place.
					Replaced = Files.Files[File].Slot;
				}
				if (bWrites)
				{
					Flags[MenuFlag::TargetPointers] = 0;
					Flags[MenuFlag::FileDetails] = 0;
					Files.DetailsCursor = Free;
					Files.Scanned = {Free, Free};
					const int32 Slot = PickSaveSlot(Card, Replaced);
					bSoundOn = false;
					const int32 Disc = bSaveAsSecondDisc ? 1 : Services.Disc - 1;
					const FWayBackPoint* WayBack = Services.WayBack != nullptr ? Services.WayBack->For(*Services.Game) : nullptr;
					const FOriginalSave Save = WriteOriginalSave(*Services.Game, Slot, Disc, *Services.Text.Tables, Services.Text.Font->LeadByte(), WayBack);
					const bool bSaved = Services.Saves != nullptr && !Services.Saves->Write(StoreSlot(Card, Slot), Save);
					if (bSaved)
					{
						bFileSaved = true;
						bSoundOn = true;
						PlaySound(Effect::CardDone);
						bSoundOn = false;
					}
					co_await Ask(bSaved ? Text::Finished : Text::SaveFailed, NoSecondText, false);
					Files.Listing = ListsAsked;
					Files.ListTaken = {false, false};
				}
				Files.bPointerOnCursor = true;
				bSoundOn = true;
				if (!bStaysAfterSave)
				{
					bGoesOn = false;
				}
			}
			else if (Result == Cancelled)
			{
				bGoesOn = false;
			}
			if (!bGoesOn)
			{
				break;
			}
		}
		Files.Listing = ListsAfterCommand;
		co_return false;
	}

	TMenuTask<bool> FMenuMain::FileCopy()
	{
		bool bFirst = true;
		ResetCellCursor();
		while (true)
		{
			co_await ScanStores(true);
			if (bFirst)
			{
				bFirst = false;
				TargetPointerShown[0] = true;
			}
			if (!IsCellSelectable(AnyFile))
			{
				Files.Cursor = FirstSelectableCell(AnyFile);
				if (Files.Cursor == Free)
				{
					TargetPointerShown[0] = false;
					Flags[MenuFlag::FileDetails] = 0;
					co_await Ask(Text::NoData, NoSecondText, false);
					break;
				}
			}
			Flags[MenuFlag::TargetPointers] = 1;
			const int32 Result = UpdateCellCursor(AnyFile);
			if (Result == Cancelled)
			{
				break;
			}
			if (Result != Chosen)
			{
				continue;
			}
			Files.bPointerOnCursor = false;
			Files.Listing = 0;
			// A save is copied to the other card under its own name: the same slot there.
			const int32 Target = Files.Cursor / CellsPerCard == 0 ? 1 : 0;
			const std::size_t To = static_cast<std::size_t>(Target);
			if (co_await Ask(Text::CopyToCard[To]) == 0)
			{
				Files.DetailsCursor = Free;
			}
			else if (Files.UsedBlocks[To] > CellsPerCard - 1)
			{
				Files.UsedBlocks[To] = 0;
				Flags[MenuFlag::TargetPointers] = 0;
				co_await Ask(Text::NoFreeBlocks, NoSecondText, false);
			}
			else
			{
				const FMenuStoreFile& Source = Files.Files[Files.Cells[static_cast<std::size_t>(CursorCell())]];
				bool bSame = false;
				for (int32 File = 0; File < Files.Listed[To]; ++File)
				{
					bSame = bSame || Files.Files[static_cast<std::size_t>(Target * PerCard + File)].Name == Source.Name;
				}
				if (bSame)
				{
					co_await Ask(Text::SameFileOnCard[To], NoSecondText, false);
				}
				else
				{
					bSoundOn = false;
					Flags[MenuFlag::TargetPointers] = 0;
					Flags[MenuFlag::FileDetails] = 0;
					Files.DetailsCursor = Free;
					Files.Scanned = {Free, Free};
					const bool bCopied = Services.Saves != nullptr && !Services.Saves->Copy(StoreSlot(Target, Source.Slot), Source.Data);
					if (bCopied)
					{
						bSoundOn = true;
						PlaySound(Effect::CardDone);
						bSoundOn = false;
					}
					co_await Ask(bCopied ? Text::Finished : Text::CopyFailed, NoSecondText, false);
					Files.Listing = ListsAsked;
					Files.Cells.fill(Free);
					Files.CellIsSave.fill(false);
					for (FMenuStoreFile& File : Files.Files)
					{
						File.bShown = false;
					}
					Files.ListTaken = {false, false};
					Files.bRescan = true;
				}
			}
			Files.bPointerOnCursor = true;
			bSoundOn = true;
			break;
		}
		Files.Listing = ListsAfterCommand;
		co_return false;
	}

	TMenuTask<bool> FMenuMain::FileDelete()
	{
		bool bFirst = true;
		bool bGoesOn = true;
		ResetCellCursor();
		do
		{
			co_await ScanStores(true);
			if (bFirst)
			{
				bFirst = false;
				TargetPointerShown[0] = true;
			}
			if (!IsCellSelectable(AnyFile))
			{
				Files.Cursor = FirstSelectableCell(AnyFile);
				if (Files.Cursor == Free)
				{
					TargetPointerShown[0] = false;
					Flags[MenuFlag::FileDetails] = 0;
					co_await Ask(Text::NoData, NoSecondText, false);
					break;
				}
			}
			Flags[MenuFlag::TargetPointers] = 1;
			const int32 Result = UpdateCellCursor(AnyFile);
			if (Result == Chosen)
			{
				Files.bPointerOnCursor = false;
				Files.Listing = 0;
				if (co_await Ask(Text::DeleteThisFile, Text::SureToDelete) != 0)
				{
					Flags[MenuFlag::TargetPointers] = 0;
					Flags[MenuFlag::FileDetails] = 0;
					Files.DetailsCursor = Free;
					const int32 Card = Files.Cursor < CellsPerCard ? 0 : 1;
					const FMenuStoreFile& File = Files.Files[Files.Cells[static_cast<std::size_t>(CursorCell())]];
					// As the original, the delete says it is finished whatever became of the file.
					if (Services.Saves != nullptr)
					{
						static_cast<void>(Services.Saves->Remove(StoreSlot(Card, File.Slot)));
					}
					bSoundOn = true;
					PlaySound(Effect::CardDone);
					bSoundOn = false;
					co_await Ask(Text::Finished, NoSecondText, false);
					Files.Listing = ListsAsked;
					Files.Cells.fill(Free);
					Files.CellIsSave.fill(false);
					for (FMenuStoreFile& Each : Files.Files)
					{
						Each.bShown = false;
					}
					Files.ListTaken = {false, false};
					Files.Scanned = {Free, Free};
					Files.bRescan = true;
				}
				Files.bPointerOnCursor = true;
				bGoesOn = false;
			}
			else if (Result == Cancelled)
			{
				bGoesOn = false;
			}
		} while (bGoesOn);
		Files.Listing = ListsAfterCommand;
		bSoundOn = true;
		co_return false;
	}

	bool FMenuMain::LoadStoreSave(const std::vector<uint8>& Data)
	{
		// The checksum the original checks before it loads: a save that fails it, or the load, cannot be read.
		TResult<FOriginalSave> Save = FOriginalSave::FromData(Data);
		if (!Save)
		{
			return false;
		}
		const bool bExpansionPlayed = !Services.PlaysExpansion || Services.PlaysExpansion(Save->Expansion());
		TResult<FGameState> Loaded = LoadOriginalSave(*Save, *Services.Game, Resources.GearParts(), *Services.Text.Tables, Services.LoadOptions, bExpansionPlayed);
		if (!Loaded)
		{
			return false;
		}
		if (Save->StoryMode() == EStoryMode::Expanded && !bExpansionPlayed && Services.Tell)
		{
			Services.Tell(std::string(OptionsText::SecondDisc::ExpansionMissing), "the save's Disc 2 expansion, Mods/" + Save->Expansion()
				+ ", is not there or is turned off: it goes on in the original Disc 2 where the expansion rejoins it");
		}
		*Services.Game = std::move(*Loaded);
		return true;
	}

	void FMenuMain::CloseFileScreen()
	{
		Files.Drawn = 0;
		ClearFileTitle();
		Flags[MenuFlag::FileDetails] = 0;
		for (FMenuStoreFile& File : Files.Files)
		{
			File.bShown = false;
		}
		Files.Scanned = {Free, Free};
		Files.UsedBlocks[0] = 0;
	}

	void FMenuMain::TurnStorePair(int32 Direction)
	{
		constexpr int32 Pairs = FSaveStore::StoreCount / FMenuFileScreen::Cards;
		const int32 Pair = Files.FirstStore / FMenuFileScreen::Cards;
		Files.FirstStore = (Pair + Direction + Pairs) % Pairs * FMenuFileScreen::Cards;
		Files.ListTaken = {false, false};
		Files.Scanned = {Free, Free};
		Files.Cursor = 0;
		Files.DetailsCursor = Free;
		Flags[MenuFlag::FileDetails] = 0;
	}

	int32 FMenuMain::StoreSlot(int32 Card, int32 Slot) const
	{
		return (Files.FirstStore + Card) * FSaveStore::SlotsPerStore + Slot;
	}
}
