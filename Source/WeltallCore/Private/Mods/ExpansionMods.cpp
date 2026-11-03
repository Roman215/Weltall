#include "Mods/ExpansionMods.h"

#include "Foundation/FolderNames.h"
#include "Foundation/HiddenFiles.h"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace Weltall
{
	namespace
	{
		constexpr std::string_view Blanks = " \t";
		constexpr std::string_view ByteOrderMark = "\xEF\xBB\xBF";
		constexpr std::string_view InfoFile = "mod.txt";
		constexpr std::string_view DocumentsFolder = "disc2";

		std::string_view Trimmed(std::string_view Text)
		{
			Text.remove_prefix(std::min(Text.find_first_not_of(Blanks), Text.size()));
			Text.remove_suffix(Text.size() - std::min(Text.find_last_not_of(Blanks) + 1, Text.size()));
			return Text;
		}

		std::string NameOf(const std::filesystem::path& Path)
		{
			const std::u8string Name = Path.filename().u8string();
			return std::string(Name.begin(), Name.end());
		}
	}

	FModInfo ParseModInfo(std::string_view Text)
	{
		if (Text.starts_with(ByteOrderMark))
		{
			Text.remove_prefix(ByteOrderMark.size());
		}
		FModInfo Info;
		while (!Text.empty())
		{
			const std::size_t End = std::min(Text.find('\n'), Text.size());
			std::string_view Line = Text.substr(0, End);
			Text.remove_prefix(std::min(End + 1, Text.size()));
			if (Line.ends_with('\r'))
			{
				Line.remove_suffix(1);
			}
			Line = Trimmed(Line);
			const std::size_t WordEnd = std::min(Line.find_first_of(Blanks), Line.size());
			const std::string_view Word = Line.substr(0, WordEnd);
			const std::string Rest(Trimmed(Line.substr(WordEnd)));
			if (Word == "name")
			{
				Info.Name = Rest;
			}
			else if (Word == "author")
			{
				Info.Author = Rest;
			}
			else if (Word == "version")
			{
				Info.Version = Rest;
			}
		}
		return Info;
	}

	std::vector<FExpansionMod> FindExpansionMods(const std::filesystem::path& Folder)
	{
		namespace fs = std::filesystem;
		std::vector<FExpansionMod> Found;
		std::error_code Error;
		for (fs::directory_iterator Entry(Folder, Error), End; !Error && Entry != End; Entry.increment(Error))
		{
			std::error_code Kind;
			if (IsHiddenFile(Entry->path()) || !Entry->is_directory(Kind))
			{
				continue;
			}
			const fs::path Documents = FolderIgnoringCase(Entry->path(), DocumentsFolder);
			if (!fs::is_directory(Documents, Kind))
			{
				continue;
			}
			FExpansionMod Mod{NameOf(Entry->path()), Documents, {}};
			std::ifstream File(Entry->path() / fs::path(InfoFile), std::ios::binary);
			if (File)
			{
				Mod.Info = ParseModInfo(std::string{std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>()});
			}
			if (Mod.Info.Name.empty())
			{
				Mod.Info.Name = Mod.Folder;
			}
			Found.push_back(std::move(Mod));
		}
		std::sort(Found.begin(), Found.end(), [](const FExpansionMod& A, const FExpansionMod& B) { return A.Folder < B.Folder; });
		return Found;
	}
}
