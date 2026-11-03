#include "Support/PlayedGame.h"

#include "Game/GearPartTables.h"
#include "Game/SaveLoad.h"
#include "Menu/MenuSaveCards.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <utility>

namespace Weltall::Testing
{
	TResult<FPlayedGame> FPlayedGame::Open(const FGameLibrary& Library)
	{
		return Open(Library.Find(EDiscRelease::UsaDisc1), Library.Find(EDiscRelease::UsaDisc2));
	}

	TResult<FPlayedGame> FPlayedGame::Open(const FIso9660* First, const FIso9660* Second)
	{
		FPlayedGame Game;
		for (const EDiscRelease Release : {EDiscRelease::UsaDisc1, EDiscRelease::UsaDisc2})
		{
			FPlayedDisc& Disc = Game.Discs[Release == EDiscRelease::UsaDisc1 ? 0 : 1];
			Disc.Iso = Release == EDiscRelease::UsaDisc1 ? First : Second;
			if (Disc.Iso == nullptr)
			{
				continue;
			}
			auto Identity = IdentifyDisc(*Disc.Iso);
			if (!Identity)
			{
				return Identity.Error();
			}
			Disc.Identity = *Identity;
			auto Executable = ReadBootExecutable(*Disc.Iso);
			if (!Executable)
			{
				return MakeError("a disc's program cannot be read");
			}
			Disc.Executable = std::make_unique<FPsxExecutable>(std::move(*Executable));
			auto Index = FGameFileIndex::Read(*Disc.Executable);
			if (!Index)
			{
				return Index.Error();
			}
			Disc.Index = std::make_unique<FGameFileIndex>(std::move(*Index));
		}
		if (Game.Discs[0].Iso == nullptr)
		{
			return MakeError("disc 1 is not in the Games folder");
		}
		auto Sound = FGameSoundSession::Start(*Game.Discs[0].Iso);
		if (!Sound)
		{
			return Sound.Error();
		}
		Game.Sound = std::move(*Sound);
		Game.Unheard = std::make_unique<FUnheardGameSound>(*Game.Sound->Sound());
		return Game;
	}

	TResult<std::unique_ptr<FGameSession>> FPlayedGame::Start(int32 Number, const FOriginalSave* Save, FGameSoundSession* OwnSound) const
	{
		const FPlayedDisc& Disc = Discs[static_cast<std::size_t>(Number)];
		if (Disc.Iso == nullptr)
		{
			return MakeError("disc " + std::to_string(Number + 1) + " is not in the Games folder");
		}
		auto Started = FGameSession::PowerOn(Disc.Iso->Image(), *Disc.Index, *Disc.Executable, Disc.Identity, OwnSound != nullptr ? *OwnSound : *Sound);
		if (!Started)
		{
			return Started.Error();
		}
		std::unique_ptr<FGameSession> Session = std::move(*Started);
		const FPlayedDisc& Other = Discs[static_cast<std::size_t>(1 - Number)];
		if (Other.Iso != nullptr)
		{
			Session->AddDisc(1 - Number, Other.Iso->Image(), *Other.Index);
		}
		FPlayerSettings Settings = Session->Settings();
		Settings.Camera = {1.0f, 0.0f, 0.0f, false, false};
		// The explorer holds its way on through a door, as the original lets it.
		Settings.Controls.DoorPause = NoDoorPause;
		Settings.WorldMapCamera = {};
		Session->SetSettings(Settings);
		Session->Unreached().bStops = false;
		if (Save == nullptr)
		{
			return Session;
		}
		const auto Tables = FGearPartTables::Load(Disc.Iso->Image(), *Disc.Index);
		if (!Tables)
		{
			return Tables.Error();
		}
		auto Loaded = LoadOriginalSave(*Save, Session->Game(), *Tables, Session->Text().NameTables);
		if (!Loaded)
		{
			return Loaded.Error();
		}
		Session->Game() = std::move(*Loaded);
		return Session;
	}

	std::vector<FOriginalSave> ReadCardFolderSaves(const std::filesystem::path& Folder)
	{
		std::vector<std::filesystem::path> Cards;
		for (const auto& Entry : std::filesystem::recursive_directory_iterator(Folder))
		{
			if (Entry.is_regular_file())
			{
				Cards.push_back(Entry.path());
			}
		}
		std::sort(Cards.begin(), Cards.end());
		std::vector<FOriginalSave> Saves;
		for (const std::filesystem::path& Card : Cards)
		{
			std::ifstream File(Card, std::ios::binary);
			const std::vector<uint8> Bytes((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
			if (Card.extension() == ".sav")
			{
				if (auto Found = FOriginalSave::FromData(Bytes))
				{
					Saves.push_back(std::move(*Found));
				}
			}
			else if (auto Found = ReadCardImageSaves(Bytes))
			{
				Saves.insert(Saves.end(), Found->begin(), Found->end());
			}
		}
		return Saves;
	}
}
