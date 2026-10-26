#pragma once

#include "Foundation/Types.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Weltall
{
	/** An entry of the folder a browser shows: the folder above, a folder in it, or a file it offers. */
	struct FBrowserEntry
	{
		enum class EKind : uint8
		{
			Up,
			Folder,
			File,
		};

		EKind Kind = EKind::File;
		/** The name as the game's font can show it: characters it lacks are question marks. */
		std::string Name;
		std::filesystem::path Path;
	};

	/**
	 * A walk through the computer's folders to pick a memory card image or a save's file: each folder's
	 * subfolders, then the files whose kind ReadMemoryCardImage reads (by their extension), each sorted by name
	 * whatever its case, hidden ones left out, and the way up first. Above a drive's root on Windows: the drives.
	 */
	class FFileBrowser
	{
	public:
		/** Browsing from Folder, or from the nearest folder above it that exists. */
		WELTALLCORE_API explicit FFileBrowser(const std::filesystem::path& Folder);

		/** The folder shown; empty for the drives. */
		const std::filesystem::path& Folder() const { return Shown; }
		/** The folder's name as the game's font can show it: "Drives" for the drives. */
		WELTALLCORE_API std::string FolderName() const;
		const std::vector<FBrowserEntry>& Entries() const { return Listed; }

		/** An entry opened: a folder (or the one above) shown, or the file returned. */
		WELTALLCORE_API std::optional<std::filesystem::path> Open(int32 Entry);
		/** The folder above shown; false at the top. */
		WELTALLCORE_API bool Up();

		/** Whether a file's extension is one the browser offers. */
		WELTALLCORE_API static bool IsOffered(const std::filesystem::path& File);
		/** A name in the game's font's characters: an underscore as a space, any other character it lacks a question mark. */
		WELTALLCORE_API static std::string DisplayName(const std::filesystem::path& Name);

	private:
		void Show(const std::filesystem::path& Folder);

		std::filesystem::path Shown;
		std::vector<FBrowserEntry> Listed;
	};
}
