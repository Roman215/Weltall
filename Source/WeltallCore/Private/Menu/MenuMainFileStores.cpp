#include "Foundation/LittleEndian.h"
#include "Game/OriginalSave.h"
#include "Game/SaveStore.h"
#include "Menu/MenuMain.h"
#include "Menu/MenuSaveHeader.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr uint8 Free = FMenuFileScreen::Free;
		constexpr int32 PerCard = FMenuFileScreen::PerCard;
		constexpr int32 CellsPerCard = FMenuFileScreen::CellsPerCard;
		/** Every save of the game is named for the first disc's product code and its slot's character from '0'. */
		constexpr std::string_view SaveName = "BASLUS-00664";

		/** A card file's header: the icon's kind and the blocks the file takes. */
		namespace Header
		{
			constexpr std::size_t IconKind = 2;
			constexpr std::size_t Blocks = 3;
			constexpr std::size_t Colours = 0x60;
			constexpr std::size_t Pixels = 0x80;
			constexpr uint8 OneFrame = 0x11;
			constexpr uint8 TwoFrames = 0x12;
			constexpr uint8 ThreeFrames = 0x13;
		}

		/** The icons' frames in VRAM: their colours a row from (0, 449) (16 a file), their pixels from (320, 128), a card 16 rows down, a frame 32. */
		constexpr int32 IconColoursY = 0x1C1;
		constexpr int32 IconPixelsX = 0x140;
		constexpr int32 IconPixelsY = 0x80;
		constexpr int32 FrameRows = 0x20;
		constexpr int32 IconFrames = 3;
		constexpr int16 IconUnits = 4;
		constexpr int16 IconRowsHigh = 16;
	}

	void FMenuMain::InitFileStores()
	{
		Files.Cells.fill(Free);
		Files.Scanned = {Free, Free};
		Files.Listed = {0, 0};
		Files.ListTaken = {false, false};
		Files.Listing = 0;
		// The cards open on the pair of stores holding the save last made.
		const std::optional<int32> Last = Services.Saves != nullptr ? Services.Saves->LastSaved() : std::nullopt;
		Files.FirstStore = Last ? *Last / FSaveStore::SlotsPerStore / FMenuFileScreen::Cards * FMenuFileScreen::Cards : 0;
	}

	void FMenuMain::UpdateStoreLists()
	{
		if (Files.Listing == 0 || Files.Listing > 2)
		{
			return;
		}
		for (int32 Card = 0; Card < FMenuFileScreen::Cards; ++Card)
		{
			bool& bTaken = Files.ListTaken[static_cast<std::size_t>(Card)];
			if (!bTaken)
			{
				ListStore(Card);
				bTaken = true;
			}
		}
	}

	void FMenuMain::ListStore(int32 Card)
	{
		const std::size_t At = static_cast<std::size_t>(Card);
		Files.UsedBlocks[At] = 0;
		for (int32 Cell = 0; Cell < PerCard; ++Cell)
		{
			Files.Cells[static_cast<std::size_t>(Card * PerCard + Cell)] = Free;
			Files.SlotUsed[static_cast<std::size_t>(Card * PerCard + Cell)] = false;
		}
		// The store's saves by their slots' order (where a card lists its directory's).
		uint8 Count = 0;
		for (int32 Slot = 0; Slot < CellsPerCard && Services.Saves != nullptr; ++Slot)
		{
			TResult<std::optional<std::vector<uint8>>> Bytes = Services.Saves->ReadBytes(StoreSlot(Card, Slot));
			if (!Bytes || !*Bytes)
			{
				continue;
			}
			FMenuStoreFile& File = Files.Files[static_cast<std::size_t>(Card * PerCard + Count)];
			File.Name = std::string(SaveName) + static_cast<char>('0' + Slot);
			File.Slot = Slot;
			File.Data = std::move(**Bytes);
			++Count;
		}
		Files.Listed[At] = Count;
	}

	void FMenuMain::ReadStoreFile(int32 Card, int32 Number)
	{
		FMenuStoreFile& File = Files.Files[static_cast<std::size_t>(Card * PerCard + Number)];
		File.Head = SaveFileHead(File.Data, File.Slot);
		const TResult<FOriginalSave> Save = FOriginalSave::FromData(File.Data);
		File.ClearGame = Save ? Save->ClearGame() : EClearGame::None;
		// Each block of the file takes the card's next cell.
		for (int32 Block = 0; Block < File.Head[Header::Blocks]; ++Block)
		{
			Files.Cells[static_cast<std::size_t>(Card * PerCard + Files.CellsFilled)] = static_cast<uint8>(Card * PerCard + Number);
			++Files.CellsFilled;
		}
	}

	std::array<uint8, 0x200> FMenuMain::SaveFileHead(const std::vector<uint8>& Data, int32 Slot) const
	{
		// Listed as the port plays it: a clear game's own party, where the original's save beside it has Elly away.
		const TResult<FOriginalSave> Save = FOriginalSave::FromData(Data);
		const std::vector<uint8> Played = Save ? Save->PlayedData() : Data;
		std::array<uint8, 0x200> Head{};
		const std::array<uint8, SaveHeaderSize> Header = MenuSaveHeader(Resources, Slot, Played);
		std::copy(Header.begin(), Header.end(), Head.begin());
		std::copy_n(Played.begin(), std::min(Played.size(), Head.size() - SaveHeaderSize), Head.begin() + SaveHeaderSize);
		return Head;
	}

	void FMenuMain::LoadFileIcon(int32 Number)
	{
		FMenuStoreFile& File = Files.Files[static_cast<std::size_t>(Number)];
		const int32 Card = Number / PerCard;
		FVramImage Colours{{static_cast<int16>(Number * 16), static_cast<int16>(IconColoursY + Card), 16, 1}, std::vector<uint16>(16)};
		for (std::size_t Unit = 0; Unit < Colours.Units.size(); ++Unit)
		{
			Colours.Units[Unit] = LittleEndian::ReadU16(File.Head, Header::Colours + Unit * 2);
		}
		Upload(Colours, EVramUploadKind::MenusAndHud);
		// Three frames whatever the icon has: a one-frame icon's others are the data's start.
		const int32 FirstRow = IconPixelsY + Card * PerCard;
		for (int32 Frame = 0; Frame < IconFrames; ++Frame)
		{
			FVramImage Pixels{{static_cast<int16>(IconPixelsX + (Number - Card * PerCard) * IconUnits), static_cast<int16>(FirstRow + Frame * FrameRows), IconUnits,
				IconRowsHigh}, std::vector<uint16>(static_cast<std::size_t>(IconUnits * IconRowsHigh))};
			for (std::size_t Unit = 0; Unit < Pixels.Units.size(); ++Unit)
			{
				Pixels.Units[Unit] = LittleEndian::ReadU16(File.Head, Header::Pixels + static_cast<std::size_t>(Frame) * 0x80 + Unit * 2);
			}
			Upload(Pixels, EVramUploadKind::MenusAndHud);
		}
		const uint8 First = static_cast<uint8>(FirstRow);
		const uint8 Second = static_cast<uint8>(FirstRow + FrameRows);
		const uint8 Third = static_cast<uint8>(FirstRow + 2 * FrameRows);
		switch (File.Head[Header::IconKind])
		{
		case Header::OneFrame:
			File.IconRows = {First, First, First, First, First, First};
			break;
		case Header::TwoFrames:
			File.IconRows = {First, Second, First, Second, First, Second};
			break;
		case Header::ThreeFrames:
			File.IconRows = {First, Second, Third, First, Second, Third};
			break;
		default:
			break;
		}
		Files.UsedBlocks[static_cast<std::size_t>(Card)] += File.Head[Header::Blocks];
	}

	void FMenuMain::MarkGameSaves(int32 Card)
	{
		for (int32 Cell = 0; Cell < PerCard; ++Cell)
		{
			Files.CellIsSave[static_cast<std::size_t>(Card * PerCard + Cell)] = false;
		}
		for (int32 Cell = Card * PerCard; Cell < Card * PerCard + CellsPerCard; ++Cell)
		{
			const uint8 Number = Files.Cells[static_cast<std::size_t>(Cell)];
			if (Number == Free || !Files.Files[Number].Name.starts_with(SaveName))
			{
				continue;
			}
			Files.CellIsSave[static_cast<std::size_t>(Cell)] = true;
			Files.SlotUsed[static_cast<std::size_t>(Card * PerCard + Files.Files[Number].Slot)] = true;
		}
	}
}
