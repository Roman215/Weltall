#include "Mods/PerfectWorks.h"

#include "Foundation/HiddenFiles.h"
#include "Foundation/Sha1.h"
#include "Media/PsxExecutable.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <iterator>
#include <map>
#include <string_view>
#include <system_error>

namespace Weltall
{
	namespace
	{
		/**
		 * The first disc's records that hold code: the executable, the programs, the menus' and battle's modules, the
		 * movie player and the gear models' module (with their copies). The mod changes some of them; the port runs its
		 * own code in their place and reads tables from some, which a half-applied change would break.
		 */
		constexpr std::array<std::pair<int32, int32>, 12> ProgramRecords = {{
			{19, 19},     // 24/1 the movie player
			{21, 22},     // the boot files
			{35, 40},     // 1/13-1/18 Battling, Field, Worldmap, Battle, Menu, Movie
			{591, 591},   // 4/169 the movie player's copy
			{595, 595},   // 4/173 the field's development tools
			{2143, 2143}, // 4/1721 the gear models' module
			{2596, 2604}, // 16/4-16/12 the battle result and the menus
			{2606, 2606}, // 18/1 the battle's debug menu
			{2611, 2611}, // 18/6 the battle's development tools
			{2615, 2615}, // 12/4 the battle's setup
			{3087, 3087}, // 32/1 the battle events
			{3957, 3960}, // 36/37-36/40 the world map's copies of the menu and the gear models' module
		}};

		/** Records of 14/2-14/8, the battle's animation extensions. */
		constexpr std::pair<int32, int32> AnimationExtensions = {3381, 3387};

		/** 32/2, the battle events' scripts: the mod's measure their message windows for its change to the battle events' code. */
		constexpr int32 BattleEventScripts = 3088;
		/** 32/1, the battle events' code. */
		constexpr int32 BattleEventCode = 3087;
		/**
		 * The SHA-1 of each version of the mod's battle events' code whose message windows the port re-creates and has
		 * checked against (the oracle's `battle_event_digests --perfect-works`): 0.11.4's, the same on both discs. With
		 * any other its scripts are left out, as they may be measured for code the port lacks, and the battles'
		 * dialogue keeps the disc's text.
		 */
		constexpr std::array<std::string_view, 1> KnownBattleEventCode = {"1b50f6e31037aef08dea344fee7807d8653dad9e"};

		/** 1/16, the Battle program, and 16/5, MENU_MAIN. */
		constexpr int32 BattleCode = 38;
		constexpr int32 MenuMainCode = 2597;
		/**
		 * The SHA-1 of each version of the rebalanced party's Battle and MENU_MAIN whose rules the port re-creates and has
		 * checked against (the oracles' `--perfect-works-rebalance`): 0.11.4's, the same in every folder of the rebalance.
		 */
		constexpr std::array<std::string_view, 1> KnownRebalanceBattle = {"126d2fae4b5942fe39db900492042901a703b00f"};
		constexpr std::array<std::string_view, 1> KnownRebalanceMenu = {"85645bd3f2fccded313aafa3b02605a0753e5a8d"};
		/** The rebalanced enemies' Battle: 0.11.4's, its bug fixes but Def UP's, which the port's Fixes rows make. */
		constexpr std::array<std::string_view, 1> KnownEnemiesBattle = {"4f82551cec4ac04b2e79d92359930bfac65a2e3d"};

		/** 1/13, the Battling program. */
		constexpr int32 BattlingCode = 35;
		/**
		 * The SHA-1 of each version of the mod's Battling the port reads its tables from: the script's (its roster's names, the
		 * only change, 0.11.4's in every folder that has it) and each arena mode's, without and with the script (the arena's
		 * rules re-created and checked against by the oracle's `battling_fight_digests --perfect-works`; with the script, its
		 * roster's names and its knock-out's label too).
		 */
		constexpr std::array<std::string_view, 1> KnownScriptBattling = {"3ada13f4bf1b00f560fa5cf9689cd1479f485fb7"};
		constexpr std::array<std::string_view, 1> KnownBasicBattling = {"f1f14f64791569e184268a0b565cceabe0dcb281"};
		constexpr std::array<std::string_view, 1> KnownBasicScriptBattling = {"6e6cd78acf0fef774f2975af766b2a805a7fcbd4"};
		constexpr std::array<std::string_view, 1> KnownExpertBattling = {"58b9d28f9d65f5b55d2467edeaf38a6538d85eb1"};
		constexpr std::array<std::string_view, 1> KnownExpertScriptBattling = {"c6e87334a07eedbed2342501b9e5a41ee0a3a900"};

		/** The arena modes' folders, without and with the script. */
		constexpr std::array<std::array<std::string_view, 2>, 2> ArenaFolders{{{"filesbasic", "filesbasic_script"}, {"filesexpert", "filesexpert_script"}}};

		/**
		 * 13/1-13/152, the battle scenes. The arena's folders carry two (scene 64 as the bug fixes have it, and in Basic's an
		 * early change of scene 17's Wels), which the arena does not need and which would put back the rebalanced enemies'
		 * and the script's: the arena takes its own files alone.
		 */
		constexpr std::pair<int32, int32> BattleScenes = {2617, 2768};

		/** A file's bytes; none where it cannot be read. */
		std::optional<std::vector<uint8>> ReadWhole(const std::filesystem::path& File)
		{
			std::ifstream In(File, std::ios::binary);
			if (!In)
			{
				return std::nullopt;
			}
			return std::vector<uint8>{std::istreambuf_iterator<char>(In), std::istreambuf_iterator<char>()};
		}

		template <std::size_t Count>
		bool IsKnownCode(const std::filesystem::path& File, const std::array<std::string_view, Count>& Known)
		{
			const std::optional<std::vector<uint8>> Bytes = ReadWhole(File);
			return Bytes && std::find(Known.begin(), Known.end(), FSha1::ToHex(FSha1::Hash(*Bytes))) != Known.end();
		}

		bool IsKnownBattleEventCode(const std::filesystem::path& File)
		{
			return IsKnownCode(File, KnownBattleEventCode);
		}

		/** The file of Record in Folder, by its record's number and whatever extension the mod gave it. */
		std::filesystem::path RecordFile(const std::filesystem::path& Folder, int32 Record)
		{
			char Name[8];
			std::snprintf(Name, sizeof(Name), "%04d", Record);
			return Folder / Name;
		}

		bool IsLeftOut(int32 Record)
		{
			const auto Within = [Record](std::pair<int32, int32> Range) { return Record >= Range.first && Record <= Range.second; };
			return Within(AnimationExtensions) || std::any_of(ProgramRecords.begin(), ProgramRecords.end(), Within);
		}

		/** A Battling the port reads its tables from: the script's or an arena mode's, of a version it knows. */
		bool IsKnownBattling(const std::filesystem::path& File)
		{
			return IsKnownCode(File, KnownScriptBattling) || IsKnownCode(File, KnownBasicBattling) || IsKnownCode(File, KnownBasicScriptBattling)
				|| IsKnownCode(File, KnownExpertBattling) || IsKnownCode(File, KnownExpertScriptBattling);
		}

		/** The record a file of the mod's folders stands for: the number its name begins with ("0852", "2593.unk8", "493.tim"). */
		std::optional<int32> RecordOf(const std::filesystem::path& File)
		{
			const std::string Name = File.filename().string();
			int32 Record = 0;
			const auto [End, Error] = std::from_chars(Name.data(), Name.data() + Name.size(), Record);
			if (Error != std::errc{} || End == Name.data() || (End != Name.data() + Name.size() && *End != '.'))
			{
				return std::nullopt;
			}
			return Record;
		}

		/** The version the release's README names first ("### Version 0.11.4"), or none. */
		std::string ReadVersion(const std::filesystem::path& Readme)
		{
			constexpr std::string_view Heading = "### Version ";
			std::ifstream In(Readme);
			for (std::string Line; std::getline(In, Line);)
			{
				if (Line.rfind(Heading, 0) == 0)
				{
					std::string Version = Line.substr(Heading.size());
					Version.erase(std::find_if(Version.rbegin(), Version.rend(), [](char C) { return C != ' ' && C != '\r'; }).base(), Version.end());
					return Version;
				}
			}
			return {};
		}

		/** Every file of Folder that names a record, in the order the folder lists them, but those Skip names and hidden ones. */
		template <typename TSkip>
		void AddFolder(const std::filesystem::path& Folder, std::map<int32, FModFile>& Files, TSkip Skip)
		{
			std::error_code Error;
			for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(Folder, Error))
			{
				if (IsHiddenFile(Entry.path()) || !Entry.is_regular_file(Error))
				{
					continue;
				}
				if (const std::optional<int32> Record = RecordOf(Entry.path()); Record && !Skip(*Record))
				{
					Files[*Record] = FModFile{*Record, Entry.path(), {}};
				}
			}
		}

		void AddFolder(const std::filesystem::path& Folder, std::map<int32, FModFile>& Files)
		{
			AddFolder(Folder, Files, [](int32) { return false; });
		}

		/** The first file of Folder but hidden ones; none where it has none. */
		std::optional<std::filesystem::path> FirstFile(const std::filesystem::path& Folder)
		{
			std::error_code Error;
			for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(Folder, Error))
			{
				if (!IsHiddenFile(Entry.path()) && Entry.is_regular_file(Error))
				{
					return Entry.path();
				}
			}
			return std::nullopt;
		}
	}

	std::optional<FPerfectWorksSubtitles> FPerfectWorksSubtitles::Find(const std::filesystem::path& GameFiles, int32 Disc)
	{
		const std::string Number = std::to_string(Disc);
		const std::optional<std::filesystem::path> Boot = FirstFile(GameFiles / "sub_executable" / ("disc" + Number));
		std::optional<std::vector<uint8>> Bytes = Boot ? ReadWhole(*Boot) : std::nullopt;
		if (!Bytes)
		{
			return std::nullopt;
		}
		const TResult<FPsxExecutable> Executable = FPsxExecutable::Parse(std::move(*Bytes));
		if (!Executable || !FMovieSubtitleTable::HasUndubCode(*Executable))
		{
			return std::nullopt;
		}
		FPerfectWorksSubtitles Subtitles;
		Subtitles.Movies = FMovieSubtitleTable::Read(*Executable);
		std::map<int32, FModFile> Pictures;
		AddFolder(GameFiles / ("fmv" + Number), Pictures);
		for (const FMovieSubtitleEntry& Entry : Subtitles.Movies.Entries())
		{
			const int32 Record = static_cast<int32>(Entry.Record) + (Disc == 2 ? FDiscModFiles::SecondDiscShift : 0);
			const auto Picture = Pictures.find(Record);
			if (Picture == Pictures.end())
			{
				return std::nullopt;
			}
			Subtitles.Files[Entry.Record] = Picture->second.Path;
		}
		if (Subtitles.Files.empty())
		{
			return std::nullopt;
		}
		return Subtitles;
	}

	TResult<FMovieSubtitles> FPerfectWorksSubtitles::Load(const FMovieSubtitleEntry& Entry) const
	{
		const auto Found = Files.find(Entry.Record);
		const std::optional<std::vector<uint8>> Bytes = Found != Files.end() ? ReadWhole(Found->second) : std::nullopt;
		if (!Bytes)
		{
			return MakeError("Perfect Works Build's subtitles of movie " + std::to_string(Entry.Movie) + " cannot be read");
		}
		return FMovieSubtitles::FromFile(*Bytes, Entry.Lines);
	}

	bool FPerfectWorksRelease::IsRelease(const std::filesystem::path& Folder)
	{
		std::error_code Error;
		const std::filesystem::path GameFiles = Folder / "gamefiles";
		return std::filesystem::is_directory(GameFiles / "bug_fix1", Error) && std::filesystem::is_directory(GameFiles / "script1", Error);
	}

	std::string FPerfectWorksRelease::VersionIn(const std::filesystem::path& Folder)
	{
		return ReadVersion(Folder / "README.md");
	}

	std::optional<FPerfectWorksRelease> FPerfectWorksRelease::Find(const std::filesystem::path& Folder)
	{
		if (!IsRelease(Folder))
		{
			return std::nullopt;
		}
		const std::filesystem::path GameFiles = Folder / "gamefiles";
		FPerfectWorksRelease Release;
		Release.GameFiles = GameFiles;
		Release.VersionName = VersionIn(Folder);
		Release.bKnownBattleEventCode = IsKnownBattleEventCode(GameFiles / "script1" / "3087");
		Release.bKnownRebalanceCode = IsKnownCode(RecordFile(GameFiles / "items1", BattleCode), KnownRebalanceBattle)
			&& IsKnownCode(RecordFile(GameFiles / "items1", MenuMainCode), KnownRebalanceMenu);
		Release.bKnownEnemiesCode = IsKnownCode(RecordFile(GameFiles / "Monsters", BattleCode), KnownEnemiesBattle);
		Release.KnownArenaCode = {{{IsKnownCode(RecordFile(GameFiles / ArenaFolders[0][0], BattlingCode), KnownBasicBattling),
									   IsKnownCode(RecordFile(GameFiles / ArenaFolders[0][1], BattlingCode), KnownBasicScriptBattling)},
			{IsKnownCode(RecordFile(GameFiles / ArenaFolders[1][0], BattlingCode), KnownExpertBattling),
				IsKnownCode(RecordFile(GameFiles / ArenaFolders[1][1], BattlingCode), KnownExpertScriptBattling)}}};
		Release.Subtitles = {FPerfectWorksSubtitles::Find(GameFiles, 1), FPerfectWorksSubtitles::Find(GameFiles, 2)};
		return Release;
	}

	bool FPerfectWorksRelease::KnowsArenaCode(EPerfectWorksArena Arena, bool bScript) const
	{
		return Arena != EPerfectWorksArena::Original && KnownArenaCode[Arena == EPerfectWorksArena::Expert ? 1 : 0][bScript ? 1 : 0];
	}

	EPerfectWorksArena FPerfectWorksRelease::UsesArena(const FPerfectWorksOptions& Options) const
	{
		return Options.bEnabled && !Options.bStoryMode && KnowsArenaCode(Options.Arena, Options.bScript) ? Options.Arena : EPerfectWorksArena::Original;
	}

	bool FPerfectWorksRelease::UsesRebalancedParty(const FPerfectWorksOptions& Options) const
	{
		return Options.bEnabled && Options.bRebalancedParty && !Options.bStoryMode && bKnownRebalanceCode;
	}

	bool FPerfectWorksRelease::UsesRebalancedEnemies(const FPerfectWorksOptions& Options) const
	{
		return Options.bEnabled && Options.bRebalancedEnemies && !Options.bStoryMode && bKnownEnemiesCode;
	}

	std::vector<FModFile> FPerfectWorksRelease::Files(const FPerfectWorksOptions& Options, int32 Disc) const
	{
		if (!Options.bEnabled)
		{
			return {};
		}
		const bool bFirstDisc = Disc == 1;
		std::map<int32, FModFile> Chosen;
		const auto Add = [&](std::string_view FirstDisc, std::string_view SecondDisc) { AddFolder(GameFiles / (bFirstDisc ? FirstDisc : SecondDisc), Chosen); };
		const auto AddBoth = [&](std::string_view Folder) { AddFolder(GameFiles / Folder, Chosen); };

		// The patcher's order: its bug fixes are in its script's and its rebalance's files already, and otherwise always
		// copied. The rebalanced party with the script is one merged folder; the enemies' folder, copied later, is merged
		// with whichever of the two is chosen.
		const bool bParty = UsesRebalancedParty(Options);
		if (bParty)
		{
			Options.bScript ? Add("Script_items", "Script_items2") : Add("items1", "items2");
		}
		else if (Options.bScript)
		{
			Add("script1", "script2");
		}
		if (Options.bStoryMode)
		{
			Options.bScript ? Add("storyfiles_script_cd1", "storyfiles_script_cd2") : Add("storyfiles_cd1", "storyfiles_cd2");
		}
		if (!Options.bScript && !bParty)
		{
			Add("bug_fix1", "bug_fix2");
		}
		if (Options.Portraits == EPerfectWorksPortraits::Resized)
		{
			AddBoth("resized_portraits");
		}
		else if (Options.Portraits == EPerfectWorksPortraits::Fixed)
		{
			AddBoth("portraits");
		}
		if (UsesRebalancedEnemies(Options))
		{
			AddBoth(bParty ? (Options.bScript ? "monsters_both" : "monsters_items") : (Options.bScript ? "monsters_script" : "Monsters"));
		}
		if (Options.bMusic && !Options.bScript)
		{
			Add("music_1", "music_2");
		}
		if (const EPerfectWorksArena Arena = UsesArena(Options); Arena != EPerfectWorksArena::Original)
		{
			const std::string_view Folder = ArenaFolders[Arena == EPerfectWorksArena::Expert ? 1 : 0][Options.bScript ? 1 : 0];
			AddFolder(GameFiles / Folder, Chosen, [](int32 Record) { return Record >= BattleScenes.first && Record <= BattleScenes.second; });
		}
		if (Options.bTitleScreen)
		{
			AddBoth("title_screen");
		}
		if (Options.bRoniPortrait)
		{
			AddBoth(Options.Portraits == EPerfectWorksPortraits::Resized ? "roni_pw/resized" : "roni_pw/default");
		}
		if (Options.bEmeraldaCafe && bFirstDisc)
		{
			AddBoth(Options.bScript ? "emeralda_fix/new_script" : "emeralda_fix/og_script");
		}
		// With the script's maps the songs are its scenes' scripts, spliced into the maps chosen.
		if (Options.bMusic && Options.bScript)
		{
			std::map<int32, FModFile> Songs;
			AddFolder(GameFiles / (bFirstDisc ? "music_1" : "music_2"), Songs);
			for (const auto& [Record, Song] : Songs)
			{
				std::error_code Error;
				const std::filesystem::path Scripts = GameFiles / "music_subfiles" / (Song.Path.filename().string() + "_5");
				if (Chosen.contains(Record) && std::filesystem::is_regular_file(Scripts, Error))
				{
					Chosen[Record].Section5 = Scripts;
				}
			}
		}

		// Its battle events' scripts only with the version of its code that the port re-creates.
		const auto Code = Chosen.find(BattleEventCode);
		if (Code == Chosen.end() || !IsKnownBattleEventCode(Code->second.Path))
		{
			Chosen.erase(BattleEventScripts);
		}

		std::vector<FModFile> Files;
		for (auto& [Record, File] : Chosen)
		{
			if (!IsLeftOut(Record) || (Record == BattlingCode && IsKnownBattling(File.Path)))
			{
				Files.push_back(std::move(File));
			}
		}
		return Files;
	}

	FModProgramChanges FPerfectWorksRelease::Changes(const FPerfectWorksOptions& Options, std::span<const FModFile> Files) const
	{
		FModProgramChanges Changes;
		Changes.bPerfectWorksBattleMessages = std::any_of(Files.begin(), Files.end(), [](const FModFile& File) { return File.Record == BattleEventScripts; });
		Changes.bPerfectWorksRebalance = UsesRebalancedParty(Options);
		Changes.PerfectWorksArena = UsesArena(Options);
		return Changes;
	}
}
