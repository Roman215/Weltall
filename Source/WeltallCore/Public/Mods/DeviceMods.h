#pragma once

#include "Foundation/Types.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	// The mods and packs of a device whose game folders nothing outside the game reaches (Android): a folder the file
	// picker copied in, told by what it holds and put in place beside Games; what is there, listed; one removed. A
	// Perfect Works release on the device is in the game's use (it reads its files by path while it plays), so a change
	// to it waits for the next start (FinishModChanges). What is taken out goes to a hidden folder first, for
	// DeleteRemovedMods to delete off the game's thread.

	/** What a folder added held, as the game took it. */
	enum class EAddedContent : uint8
	{
		/** Perfect Works Build: the folder holding its gamefiles, put in Mods/PerfectWorks. */
		PerfectWorks,
		/** Pictures named as Beetle PSX HW's packs or by Weltall's keys, put in Assets/textures under the pack's name. */
		Textures,
		/** WAVs named by Weltall's song keys, put in Assets/music under the pack's name. */
		Music,
		/** A Weltall folder's Mods and Assets, each in its place there. */
		Folders,
	};

	struct FAddedMod
	{
		EAddedContent Content = EAddedContent::Folders;
		/** The pack's name, or Perfect Works' version (empty where its README names none). */
		std::string Name;
		/** The files put in place. */
		int32 Files = 0;
		/** Perfect Works was already in the game's use: the one added takes its place when the game next starts. */
		bool bOnRestart = false;
	};

	enum class EModAddFailure : uint8
	{
		None,
		/** Nothing in the folder is a mod or pack the game reads. */
		NotAMod,
		/** It could not be put in place. */
		Unwritable,
	};

	struct FModAddition
	{
		/** What was added; none where nothing was, Failure saying why. */
		std::optional<FAddedMod> Added;
		EModAddFailure Failure = EModAddFailure::None;
		/** What went wrong, for the log. */
		std::string Detail;
	};

	/**
	 * Incoming's content put in place below Root (the folder holding Games, Mods and Assets), told by what it holds: a
	 * Weltall folder's Mods or Assets at its top; else Perfect Works anywhere in it; else pictures, then music, named as
	 * the game reads them, as a pack called Name (the file or folder picked; a ".zip" ending left out). A folder of the
	 * same name is replaced. Whatever is left of Incoming goes with the removed.
	 */
	WELTALLCORE_API FModAddition AddMod(const std::filesystem::path& Incoming, const std::filesystem::path& Root, std::string_view Name);

	/** What a listed mod or pack is. */
	enum class EModItemKind : uint8
	{
		PerfectWorks,
		/** Another folder in Mods. */
		Mod,
		/** A folder in Assets/textures. */
		Textures,
		/** A folder in Assets/music. */
		Music,
		/** Anything else at the top of Assets (text, font, a disc's dumped files). */
		Assets,
	};

	struct FModItem
	{
		EModItemKind Kind = EModItemKind::Mod;
		/** Where it is, from Root: what RemoveMod takes. */
		std::filesystem::path Path;
		/** As the player knows it: the pack's folder, "Perfect Works" and its version, or its place ("Assets/text"). */
		std::string Name;
		uint64 Bytes = 0;
		/** Perfect Works only: removed, or replaced by one added, when the game next starts. */
		bool bRemovedOnRestart = false;
		bool bReplacedOnRestart = false;

		bool operator==(const FModItem&) const = default;
	};

	/** The mods and packs below Root: Perfect Works, Mods' other folders, the texture and music packs, then the rest of Assets. */
	WELTALLCORE_API std::vector<FModItem> ListMods(const std::filesystem::path& Root);

	enum class EModRemoval : uint8
	{
		Removed,
		/** Perfect Works, in the game's use: removed when the game next starts. */
		OnRestart,
		/** Not one ListMods gives, or it could not be moved. */
		Failed,
	};

	/** The mod or pack at Path (as ListMods gives it) taken out from below Root. */
	WELTALLCORE_API EModRemoval RemoveMod(const std::filesystem::path& Root, const std::filesystem::path& Path);

	/** The changes to Perfect Works that waited for this start made, and what an earlier session left half added removed. Before mods are looked for. */
	WELTALLCORE_API void FinishModChanges(const std::filesystem::path& Root);

	/** Everything taken out deleted: slow for a large mod, so off the game's thread. */
	WELTALLCORE_API void DeleteRemovedMods(const std::filesystem::path& Root);

	/** Where the file picker copies what the player picks, below Root, before AddMod. */
	WELTALLCORE_API std::filesystem::path IncomingModFolder(const std::filesystem::path& Root);
}
