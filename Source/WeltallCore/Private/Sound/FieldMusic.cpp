#include "Sound/FieldMusic.h"

#include "Sound/GameSoundSystem.h"
#include "Sound/ResidentSound.h"
#include "Sound/SoundDiscFiles.h"

#include <optional>
#include <string>

namespace Weltall
{
	namespace
	{
		constexpr int32 MusicDirectory = 28;
		constexpr int32 BattleThemesFile = 3;
		/** Song n's bank is file 13h + 2n of the directory, its sequence the file after. */
		constexpr int32 FirstBankFile = 0x13;
		constexpr int32 FirstSequenceFile = 0x14;
		constexpr int32 FullVolume = 0x7F;
		/** A kept song fades back in over a second's interrupts. */
		constexpr int32 ResumeInterrupts = 0xF0;
		constexpr int32 NoPlayingSong = -1;
	}

	FFieldMusic::FFieldMusic(FGameSoundSystem& InSound, FResidentSound& InResident, const FCdImage& InImage, const FGameFileIndex& InIndex, FFieldSongTable InSongs)
		: Sound(InSound), Resident(InResident), Image(InImage), Index(InIndex), Songs(std::move(InSongs))
	{
	}

	FFieldMusic::~FFieldMusic()
	{
		// The driver must let go of the sequence files before they go.
		const FLockedGameSound Locked = Sound.Lock();
		bKeepNext = false;
		StopWith(Locked.Driver());
		DropKept(Locked.Driver());
	}

	void FFieldMusic::SetStart(int32 Song, EFieldSongStart Start)
	{
		if (!Options.bStartOnlyForItsSong || Song != CurrentSong || bLoading)
		{
			bSilentStart = Start == EFieldSongStart::Silent;
		}
	}

	bool FFieldMusic::RequestSong(int32 Song)
	{
		if (Song != NoSong && (Song < 0 || Song >= Songs.Count()))
		{
			return false;
		}
		if (Song != CurrentSong)
		{
			Stop();
			CurrentSong = Song;
			bLoading = Song != NoSong;
		}
		return true;
	}

	TResult<bool> FFieldMusic::ChangeSong(int32 Song, EFieldSongStart Start)
	{
		SetStart(Song, Start);
		if (!RequestSong(Song))
		{
			return MakeError("the field has no song " + std::to_string(Song));
		}
		return StartChosenSong();
	}

	void FFieldMusic::ChooseSong(int32 Song)
	{
		const FLockedGameSound Locked = Sound.Lock();
		DropKept(Locked.Driver());
		if (Song != CurrentSong)
		{
			StopWith(Locked.Driver());
			bLoading = true;
		}
		CurrentSong = Song;
	}

	void FFieldMusic::SetSong(int32 Song)
	{
		CurrentSong = Song;
	}

	TResult<bool> FFieldMusic::StartChosenSong()
	{
		if (!bLoading)
		{
			return false;
		}
		return Load(CurrentSong);
	}

	void FFieldMusic::ChooseEnteringSong(bool bBattleLost)
	{
		bLoading = false;
		const FLockedGameSound Locked = Sound.Lock();
		if (bBattleLost)
		{
			CurrentSong = DefeatSong;
			DropKept(Locked.Driver());
		}
		if (PlayingSong == CurrentSong)
		{
			DropKept(Locked.Driver());
			return;
		}
		StopWith(Locked.Driver());
		bLoading = CurrentSong != NoSong;
		if (Kept != nullptr)
		{
			bKeepNext = true;
		}
	}

	TResult<bool> FFieldMusic::EnterField(bool bBattleLost)
	{
		ChooseEnteringSong(bBattleLost);
		return StartChosenSong();
	}

	TResult<bool> FFieldMusic::StartBattleSong(int32 Song)
	{
		if (PlayingSong == Song)
		{
			return false;
		}
		if (PlayingSong != NoPlayingSong)
		{
			bKeepNext = true;
		}
		Stop();
		CurrentSong = Song;
		bLoading = true;
		return Load(Song);
	}

	void FFieldMusic::Stop()
	{
		const FLockedGameSound Locked = Sound.Lock();
		StopWith(Locked.Driver());
	}

	TResult<bool> FFieldMusic::PlayAnew(int32 Song)
	{
		Stop();
		CurrentSong = Song;
		bLoading = true;
		return Load(Song);
	}

	TResult<bool> FFieldMusic::RestoreBattleThemes()
	{
		if (!bBattleThemesEvicted)
		{
			return false;
		}
		Stop();
		TResult<FSoundBankFile> Themes = ReadSoundFile<FSoundBankFile>(Image, Index, MusicDirectory, BattleThemesFile);
		if (!Themes)
		{
			return Themes.Error();
		}
		const FLockedGameSound Locked = Sound.Lock();
		Resident.BattleThemeBank = Locked->LoadBank(*Themes);
		bBattleThemesEvicted = false;
		return true;
	}

	TResult<bool> FFieldMusic::Load(int32 Song)
	{
		Stop();
		if (Song == NoSong)
		{
			bLoading = false;
			return false;
		}
		if (Song < 0 || Song >= Songs.Count())
		{
			bLoading = false;
			return MakeError("the field has no song " + std::to_string(Song));
		}
		const FFieldSong Entry = Songs[Song];
		const bool bResume = bKeepNext && Kept != nullptr;

		// Every song but the battle themes loads its bank anew: stopping the music unloaded the last one.
		std::optional<FSoundBankFile> Bank;
		std::optional<FSoundBankFile> Themes;
		std::unique_ptr<FSoundSequenceFile> SequenceFile;
		if (Entry.BankSong != FFieldSong::ResidentBank)
		{
			TResult<FSoundBankFile> File = ReadSoundFile<FSoundBankFile>(Image, Index, MusicDirectory, FirstBankFile + 2 * Entry.BankSong);
			if (!File)
			{
				bLoading = false;
				return File.Error();
			}
			Bank = std::move(*File);
		}
		if (Entry.NeedsBattleThemes() && bBattleThemesEvicted)
		{
			TResult<FSoundBankFile> File = ReadSoundFile<FSoundBankFile>(Image, Index, MusicDirectory, BattleThemesFile);
			if (!File)
			{
				bLoading = false;
				return File.Error();
			}
			Themes = std::move(*File);
		}
		if (!bResume)
		{
			TResult<FSoundSequenceFile> File = ReadSoundFile<FSoundSequenceFile>(Image, Index, MusicDirectory, FirstSequenceFile + 2 * Song);
			if (!File)
			{
				bLoading = false;
				return File.Error();
			}
			SequenceFile = std::make_unique<FSoundSequenceFile>(std::move(*File));
		}

		const FLockedGameSound Locked = Sound.Lock();
		FSoundDriver& Driver = Locked.Driver();
		if (Entry.EvictsBattleThemes() && !bBattleThemesEvicted)
		{
			if (Resident.BattleThemeBank != nullptr)
			{
				Driver.UnloadBank(*Resident.BattleThemeBank);
				Resident.BattleThemeBank = nullptr;
			}
			bBattleThemesEvicted = true;
		}
		if (Bank)
		{
			if (ReleaseBankRoom)
			{
				ReleaseBankRoom(Driver);
			}
			MusicBank = Driver.LoadNewBank(*Bank);
		}
		if (Themes)
		{
			Resident.BattleThemeBank = Driver.LoadBank(*Themes);
			bBattleThemesEvicted = false;
		}
		if (bResume)
		{
			Playing = Kept;
			PlayingFile = std::move(KeptFile);
			Kept = nullptr;
			bKeepNext = false;
			Driver.ResumeSequence(*Playing, FullVolume, ResumeInterrupts);
		}
		else
		{
			PlayingFile = std::move(SequenceFile);
			Playing = Driver.LoadSequence(*PlayingFile);
			if (Playing != nullptr && bSilentStart)
			{
				Driver.PlaySequence(*Playing, 0, 0);
				Driver.SetSequenceVolume(*Playing, 0, 0);
			}
			else if (Playing != nullptr)
			{
				Driver.PlaySequence(*Playing, FullVolume, 0);
			}
		}
		PlayingSong = Song;
		bSilentStart = false;
		bLoaded = true;
		bLoading = false;
		return true;
	}

	void FFieldMusic::StopWith(FSoundDriver& Driver)
	{
		if (bLoaded)
		{
			if (Playing != nullptr)
			{
				Driver.StopSequence(*Playing);
				if (bKeepNext)
				{
					DropKept(Driver);
					Kept = Playing;
					KeptFile = std::move(PlayingFile);
				}
				else
				{
					Driver.FreeSequence(*Playing);
				}
				Playing = nullptr;
				bKeepNext = false;
			}
			PlayingFile.reset();
			if (MusicBank != nullptr)
			{
				Driver.UnloadBank(*MusicBank);
				MusicBank = nullptr;
			}
		}
		PlayingSong = NoPlayingSong;
		bLoaded = false;
	}

	void FFieldMusic::DropKept(FSoundDriver& Driver)
	{
		if (Kept != nullptr)
		{
			Driver.StopSequence(*Kept);
			Driver.FreeSequence(*Kept);
			Kept = nullptr;
			KeptFile.reset();
		}
	}
}
