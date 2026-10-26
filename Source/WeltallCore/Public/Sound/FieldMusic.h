#pragma once

#include "Foundation/Result.h"
#include "Sound/FieldSongTable.h"

#include <functional>
#include <memory>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FGameSoundSystem;
	class FSoundDriver;
	class FSoundSequenceFile;
	struct FResidentSound;
	struct FSoundBank;
	struct FSoundSequence;

	/** How a song the field loads starts: at full volume, or silent for the scripts to fade it in. */
	enum class EFieldSongStart : uint8
	{
		FullVolume,
		Silent,
	};

	/** Fixes of the field's music: each is on unless the original's behavior is asked for. */
	struct FFieldMusicOptions
	{
		/**
		 * A script's start applies to the song it asks for only. In the original, asking silently for
		 * the song already playing leaves the next song that loads silent, even one no script chose
		 * (a battle's, the song of the field's return).
		 */
		bool bStartOnlyForItsSong = true;
	};

	/**
	 * The field's music by song number, as the Field program plays it and the kernel keeps it
	 * across the game's modes: each song's bank (as the song table says, shared between songs or the
	 * resident battle themes' bank) and sequence from directory 28, the battle themes' bank given up
	 * for the songs that need its room and loaded again before the next song that does not, the song
	 * a battle interrupts kept to go on afterwards. What waits for the disc in the original is done
	 * at once. Game thread only.
	 */
	class FFieldMusic
	{
	public:
		static constexpr int32 NoSong = 0xFF;
		/** The song the field plays after a lost battle. */
		static constexpr int32 DefeatSong = 0x0E;

		WELTALLCORE_API FFieldMusic(FGameSoundSystem& InSound, FResidentSound& InResident, const FCdImage& InImage, const FGameFileIndex& InIndex, FFieldSongTable InSongs);
		WELTALLCORE_API ~FFieldMusic();
		FFieldMusic(const FFieldMusic&) = delete;
		FFieldMusic& operator=(const FFieldMusic&) = delete;

		/**
		 * How the next song loaded starts, whether the one a script asks for (which sets it before it
		 * waits for a song still loading) or a later one.
		 */
		WELTALLCORE_API void SetStart(int32 Song, EFieldSongStart Start);
		/**
		 * A script asks for a song once the map runs: unless it is the song already, the music stops and
		 * Song is chosen, for StartChosenSong (the field's poll) to load; NoSong leaves the music off.
		 * False when the field has no song Song.
		 */
		WELTALLCORE_API bool RequestSong(int32 Song);
		/** SetStart and RequestSong, and the song's load at once. Returns whether a song was loaded. */
		WELTALLCORE_API TResult<bool> ChangeSong(int32 Song, EFieldSongStart Start);
		/**
		 * A map's first scripts ask for a song: a held song is dropped, and a different song stops the
		 * music and is chosen, for the map's start to load (StartChosenSong, EnterField).
		 */
		WELTALLCORE_API void ChooseSong(int32 Song);
		/** The song the Field program starts with before a map's first scripts: none on a new game, else the one it left with. */
		WELTALLCORE_API void SetSong(int32 Song);
		/** The song chosen, if one is, loads: a map entered from another map, or the field's poll. */
		WELTALLCORE_API TResult<bool> StartChosenSong();
		/**
		 * The Field program's first map: unless the song asked for plays on, the music stops and it is
		 * chosen for StartChosenSong (the field's poll) to load; the song a battle kept is to go on where
		 * it was, fading back in. After a lost battle, the defeat song.
		 */
		WELTALLCORE_API void ChooseEnteringSong(bool bBattleLost);
		/** ChooseEnteringSong, and the song's load at once. */
		WELTALLCORE_API TResult<bool> EnterField(bool bBattleLost);
		/** A battle from the field: its song replaces the map's, which is kept for EnterField. */
		WELTALLCORE_API TResult<bool> StartBattleSong(int32 Song);
		/** Stops the music and unloads its bank. */
		WELTALLCORE_API void Stop();
		/** Stops the music and loads Song from its start, whatever played: the French translation's credits' own song. */
		WELTALLCORE_API TResult<bool> PlayAnew(int32 Song);
		/** Loads the battle themes' bank again if a song took its room, stopping the music (the field does it before the world map). */
		WELTALLCORE_API TResult<bool> RestoreBattleThemes();

		/** The song asked for, NoSong for none. */
		int32 Song() const { return CurrentSong; }
		int32 SongCount() const { return Songs.Count(); }
		/** Whether a song is chosen and not loaded yet. */
		bool IsLoading() const { return bLoading; }
		/** Whether a song has loaded since the music last stopped. */
		bool IsLoaded() const { return bLoaded; }
		/** The song's sequence once loaded, for the scripts' volume, pitch, tempo, pan and track changes; null before. */
		FSoundSequence* Sequence() const { return bLoaded ? Playing : nullptr; }
		/** Whether the battle themes' bank is out of sound RAM. */
		bool AreBattleThemesEvicted() const { return bBattleThemesEvicted; }

		void SetOptions(const FFieldMusicOptions& InOptions) { Options = InOptions; }

		/**
		 * What else keeps its instruments where the songs' banks go (the world map's), given the locked
		 * driver: it gives the room up before a song's bank loads there, which the original loads over it.
		 */
		void SetBankRoomRelease(std::function<void(FSoundDriver&)> Release) { ReleaseBankRoom = std::move(Release); }

	private:
		TResult<bool> Load(int32 Song);
		void StopWith(FSoundDriver& Driver);
		void DropKept(FSoundDriver& Driver);

		FGameSoundSystem& Sound;
		FResidentSound& Resident;
		const FCdImage& Image;
		const FGameFileIndex& Index;
		FFieldSongTable Songs;
		FFieldMusicOptions Options;
		std::function<void(FSoundDriver&)> ReleaseBankRoom;

		int32 CurrentSong = NoSong;
		/** The song whose sequence is loaded, -1 for none. */
		int32 PlayingSong = -1;
		bool bLoading = false;
		bool bLoaded = false;
		bool bSilentStart = false;
		bool bBattleThemesEvicted = false;
		/** The next stop keeps the sequence, and the next load goes on with it. */
		bool bKeepNext = false;
		FSoundSequence* Playing = nullptr;
		std::unique_ptr<FSoundSequenceFile> PlayingFile;
		FSoundSequence* Kept = nullptr;
		std::unique_ptr<FSoundSequenceFile> KeptFile;
		const FSoundBank* MusicBank = nullptr;
	};
}
