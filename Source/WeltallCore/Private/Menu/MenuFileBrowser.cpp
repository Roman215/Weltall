#include "Menu/MenuFileBrowser.h"

#include "Foundation/HiddenFiles.h"
#include "Text/MesGlyphs.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>
#include <system_error>

namespace Weltall
{
	namespace
	{
		/**
		 * The extensions of the files a memory card image or a save's file comes as: raw cards from emulators and
		 * adapters, DexDrive's, the Virtual Game Station's, PSV, and the single saves of MCS, Xploder and Action Replay.
		 */
		constexpr std::array<std::string_view, 17> OfferedExtensions = {".mcr", ".mcd", ".mc", ".mem", ".vgs", ".gme", ".vm1", ".ps", ".psm", ".ddf",
			".srm", ".mcb", ".psv", ".mcs", ".psx", ".ps1", ".xps"};

		std::string Lowered(std::string Text)
		{
			std::transform(Text.begin(), Text.end(), Text.begin(), [](unsigned char Character) { return static_cast<char>(std::tolower(Character)); });
			return Text;
		}

		/** The folder itself when it exists, else the nearest one above it that does; none when no part of it does. */
		std::filesystem::path Existing(std::filesystem::path Folder)
		{
			std::error_code Error;
			while (!Folder.empty())
			{
				if (std::filesystem::is_directory(Folder, Error))
				{
					return Folder;
				}
				if (Folder == Folder.parent_path())
				{
					break;
				}
				Folder = Folder.parent_path();
			}
			return {};
		}
	}

	FFileBrowser::FFileBrowser(const std::filesystem::path& Folder)
	{
		Show(Existing(Folder));
	}

	std::string FFileBrowser::FolderName() const
	{
		if (Shown.empty())
		{
			return "Drives";
		}
		return DisplayName(Shown.has_filename() ? Shown.filename() : Shown);
	}

	std::optional<std::filesystem::path> FFileBrowser::Open(int32 Entry)
	{
		if (Entry < 0 || Entry >= static_cast<int32>(Listed.size()))
		{
			return std::nullopt;
		}
		const FBrowserEntry Opened = Listed[static_cast<std::size_t>(Entry)];
		switch (Opened.Kind)
		{
		case FBrowserEntry::EKind::Up:
			Up();
			return std::nullopt;
		case FBrowserEntry::EKind::Folder:
			Show(Opened.Path);
			return std::nullopt;
		case FBrowserEntry::EKind::File:
			return Opened.Path;
		}
		return std::nullopt;
	}

	bool FFileBrowser::Up()
	{
		if (Shown.empty())
		{
			return false;
		}
		const std::filesystem::path Parent = Shown.parent_path();
		if (Parent == Shown || Parent.empty())
		{
#ifdef _WIN32
			// Above a drive's root: the drives.
			Show({});
			return true;
#else
			return false;
#endif
		}
		Show(Parent);
		return true;
	}

	bool FFileBrowser::IsOffered(const std::filesystem::path& File)
	{
		const std::string Extension = Lowered(DisplayName(File.extension()));
		return std::find(OfferedExtensions.begin(), OfferedExtensions.end(), Extension) != OfferedExtensions.end();
	}

	std::string FFileBrowser::DisplayName(const std::filesystem::path& Name)
	{
		std::string Shown;
		for (const char8_t Unit : Name.u8string())
		{
			// A character past ASCII is several units: each continuation unit is dropped, its lead a question mark.
			if ((Unit & 0xC0) == 0x80)
			{
				continue;
			}
			// The font has no underscore, which names often put for a space.
			const char Character = Unit == u8'_' ? ' ' : static_cast<char>(Unit);
			Shown.push_back(Unit < 0x80 && MesCodeOfCharacter(Character) ? Character : '?');
		}
		return Shown;
	}

	void FFileBrowser::Show(const std::filesystem::path& Folder)
	{
		Shown = Folder;
		Listed.clear();
		if (Folder.empty())
		{
#ifdef _WIN32
			for (char Drive = 'A'; Drive <= 'Z'; ++Drive)
			{
				const std::filesystem::path Root = std::string{Drive, ':', '\\'};
				std::error_code Error;
				if (std::filesystem::is_directory(Root, Error))
				{
					Listed.push_back({FBrowserEntry::EKind::Folder, std::string{Drive, ':'}, Root});
				}
			}
#endif
			return;
		}
#ifdef _WIN32
		Listed.push_back({FBrowserEntry::EKind::Up, "..", Folder.parent_path()});
#else
		if (Folder.parent_path() != Folder)
		{
			Listed.push_back({FBrowserEntry::EKind::Up, "..", Folder.parent_path()});
		}
#endif
		std::vector<FBrowserEntry> Folders;
		std::vector<FBrowserEntry> Files;
		std::error_code Error;
		for (std::filesystem::directory_iterator At(Folder, std::filesystem::directory_options::skip_permission_denied, Error), End; !Error && At != End;
			 At.increment(Error))
		{
			const std::filesystem::path& Path = At->path();
			if (IsHiddenFile(Path))
			{
				continue;
			}
			std::error_code KindError;
			if (At->is_directory(KindError))
			{
				Folders.push_back({FBrowserEntry::EKind::Folder, DisplayName(Path.filename()), Path});
			}
			else if (At->is_regular_file(KindError) && IsOffered(Path))
			{
				Files.push_back({FBrowserEntry::EKind::File, DisplayName(Path.filename()), Path});
			}
		}
		const auto ByName = [](const FBrowserEntry& A, const FBrowserEntry& B) { return Lowered(A.Name) < Lowered(B.Name); };
		std::sort(Folders.begin(), Folders.end(), ByName);
		std::sort(Files.begin(), Files.end(), ByName);
		Listed.insert(Listed.end(), Folders.begin(), Folders.end());
		Listed.insert(Listed.end(), Files.begin(), Files.end());
	}
}
