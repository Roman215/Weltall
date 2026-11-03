#include "Mods/ItalianPatch.h"

#include "Foundation/FileByteSource.h"
#include "Foundation/HiddenFiles.h"
#include "Foundation/LittleEndian.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <system_error>

namespace Weltall
{
	namespace
	{
		/** The variants' bits: the voices, the disc and the commands, one of each pair in every variant. */
		constexpr uint32 JapaneseVoices = 0x01;
		constexpr uint32 EnglishVoices = 0x02;
		constexpr uint32 FirstDisc = 0x04;
		constexpr uint32 SecondDisc = 0x08;
		constexpr uint32 UsCommands = 0x10;
		constexpr uint32 JapaneseCommands = 0x20;
		/** Record 0's entry is a note of the team's, no file. */
		constexpr int32 NoteRecord = 0;
		constexpr std::size_t HeaderBytes = 16;
		constexpr std::size_t EntryBytes = 16;

		/** A movie patch's PPF 3.0 header: its mark and method, then its 50-byte description, as the team wrote them. */
		constexpr std::string_view PpfMark = "PPF30";
		constexpr uint8 PpfMethod = 2;
		constexpr std::size_t PpfDescriptionAt = 6;
		constexpr std::size_t PpfDescriptionBytes = 50;
		constexpr std::string_view MovieDescription = "Xenogears: Traduzione filmati";
		constexpr std::string_view JapaneseSound = "audio jap";

		std::string LowerExtension(const std::filesystem::path& Path)
		{
			std::string Extension = Path.extension().string();
			std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
			return Extension;
		}

		/** The regular files of Folder that are not hidden, in the order of their names. */
		std::vector<std::filesystem::path> SortedFiles(const std::filesystem::path& Folder, bool bFolders)
		{
			std::vector<std::filesystem::path> Found;
			std::error_code Error;
			for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(Folder, Error))
			{
				if (!IsHiddenFile(Entry.path()) && (bFolders ? Entry.is_directory(Error) : Entry.is_regular_file(Error)))
				{
					Found.push_back(Entry.path());
				}
			}
			std::sort(Found.begin(), Found.end());
			return Found;
		}

		/** A movie patch's disc (1 or 2) and sound, by its description; none for another file. */
		std::optional<std::pair<int32, bool>> MoviePatchOf(const std::filesystem::path& Path)
		{
			const auto File = FFileByteSource::Open(Path);
			std::array<uint8, PpfDescriptionAt + PpfDescriptionBytes> Header{};
			if (!File || (*File)->Size() < Header.size() || !(*File)->Read(0, Header))
			{
				return std::nullopt;
			}
			const std::string_view Bytes(reinterpret_cast<const char*>(Header.data()), Header.size());
			if (Bytes.substr(0, PpfMark.size()) != PpfMark || Header[PpfMark.size()] != PpfMethod)
			{
				return std::nullopt;
			}
			std::string_view Description = Bytes.substr(PpfDescriptionAt, PpfDescriptionBytes);
			Description = Description.substr(0, Description.find_last_not_of(" \0", std::string_view::npos, 2) + 1);
			if (Description.substr(0, MovieDescription.size()) != MovieDescription || Description.size() < 3)
			{
				return std::nullopt;
			}
			const std::string_view Disc = Description.substr(Description.size() - 3);
			if (Disc != "CD1" && Disc != "CD2")
			{
				return std::nullopt;
			}
			return std::pair{Disc == "CD1" ? 1 : 2, Description.find(JapaneseSound) != std::string_view::npos};
		}
	}

	std::optional<FItalianPatch> FItalianPatch::Find(const std::filesystem::path& Folder)
	{
		for (const std::filesystem::path& ModFolder : SortedFiles(Folder, true))
		{
			for (const std::filesystem::path& Candidate : SortedFiles(ModFolder, false))
			{
				if (LowerExtension(Candidate) != ".xpt")
				{
					continue;
				}
				if (std::optional<FItalianPatch> Patch = Read(Candidate))
				{
					return Patch;
				}
			}
		}
		return std::nullopt;
	}

	std::optional<FItalianPatch> FItalianPatch::Read(const std::filesystem::path& Path)
	{
		const auto Source = FFileByteSource::Open(Path);
		std::array<uint8, HeaderBytes> Header{};
		if (!Source || (*Source)->Size() < HeaderBytes || !(*Source)->Read(0, Header))
		{
			return std::nullopt;
		}
		const uint32 Count = LittleEndian::ReadU32(Header, 0);
		const uint64 Size = (*Source)->Size();
		if (LittleEndian::ReadU32(Header, 4) != 1 || LittleEndian::ReadU32(Header, 8) != 0 || LittleEndian::ReadU32(Header, 12) != 0 || Count == 0
			|| HeaderBytes + uint64{Count} * EntryBytes > Size)
		{
			return std::nullopt;
		}
		std::vector<uint8> Table(Count * EntryBytes);
		if (!(*Source)->Read(HeaderBytes, Table))
		{
			return std::nullopt;
		}
		FItalianPatch Patch;
		Patch.File = Path;
		for (uint32 Number = 0; Number < Count; ++Number)
		{
			const std::size_t At = Number * EntryBytes;
			FEntry Entry;
			Entry.Record = LittleEndian::ReadU16(Table, At);
			Entry.Offset = LittleEndian::ReadU32(Table, At + 2);
			Entry.Size = LittleEndian::ReadU32(Table, At + 6);
			Entry.Bits = LittleEndian::ReadU32(Table, At + 10);
			if (Entry.Size == 0 || Entry.Offset > Size || Entry.Size > Size - Entry.Offset)
			{
				return std::nullopt;
			}
			Patch.Entries.push_back(Entry);
		}
		for (const std::filesystem::path& Beside : SortedFiles(Path.parent_path(), false))
		{
			if (LowerExtension(Beside) != ".ppf")
			{
				continue;
			}
			if (const std::optional<std::pair<int32, bool>> Movie = MoviePatchOf(Beside))
			{
				std::optional<std::filesystem::path>& Slot = Patch.Movies[static_cast<std::size_t>(Movie->first - 1)][Movie->second ? 1 : 0];
				if (!Slot)
				{
					Slot = Beside;
				}
			}
		}
		return Patch;
	}

	bool FItalianPatch::InVariant(const FEntry& Entry, const FItalianOptions& Options, int32 Disc)
	{
		const uint32 Voices = Options.bJapaneseVoices ? JapaneseVoices : EnglishVoices;
		const uint32 DiscBit = Disc == 2 ? SecondDisc : FirstDisc;
		const uint32 Commands = Options.bJapaneseCommands ? JapaneseCommands : UsCommands;
		return (Entry.Bits & Voices) != 0 && (Entry.Bits & DiscBit) != 0 && (Entry.Bits & Commands) != 0;
	}

	std::vector<FModFile> FItalianPatch::Files(const FItalianOptions& Options, int32 Disc) const
	{
		std::vector<FModFile> Files;
		for (const FEntry& Entry : Entries)
		{
			if (Entry.Record == NoteRecord || Entry.Record == ExecutableRecord || !InVariant(Entry, Options, Disc))
			{
				continue;
			}
			FModFile Packed;
			Packed.Record = Entry.Record;
			Packed.Path = File;
			Packed.Offset = Entry.Offset;
			Packed.Size = Entry.Size;
			Files.push_back(std::move(Packed));
		}
		return Files;
	}

	TResult<std::vector<uint8>> FItalianPatch::Executable(const FItalianOptions& Options, int32 Disc) const
	{
		const auto Found = std::find_if(Entries.begin(), Entries.end(),
			[&](const FEntry& Entry) { return Entry.Record == ExecutableRecord && InVariant(Entry, Options, Disc); });
		if (Found == Entries.end())
		{
			return MakeError("the Italian translation's patch has no executable for disc " + std::to_string(Disc));
		}
		const auto Source = FFileByteSource::Open(File);
		if (!Source)
		{
			return Source.Error();
		}
		std::vector<uint8> Bytes(static_cast<std::size_t>(Found->Size));
		if (!(*Source)->Read(Found->Offset, Bytes))
		{
			return MakeError("cannot read the Italian translation's patch: " + File.string());
		}
		return Bytes;
	}

	const std::optional<std::filesystem::path>& FItalianPatch::MoviePatch(int32 Disc, bool bJapaneseSound) const
	{
		return Movies[static_cast<std::size_t>(std::clamp(Disc, 1, 2) - 1)][bJapaneseSound ? 1 : 0];
	}

	TResult<std::shared_ptr<const FDiscRawPatch>> FItalianPatch::OpenMovies(int32 Disc, EItalianMovies Sound) const
	{
		const bool bJapanese = Sound == EItalianMovies::Japanese;
		const std::optional<std::filesystem::path>& Path = MoviePatch(Disc, bJapanese);
		if (Sound == EItalianMovies::Original || !Path)
		{
			return std::shared_ptr<const FDiscRawPatch>();
		}
		std::shared_ptr<const FDiscRawPatch>& Kept = Opened[static_cast<std::size_t>(std::clamp(Disc, 1, 2) - 1)][bJapanese ? 1 : 0];
		if (!Kept)
		{
			TResult<std::shared_ptr<const FDiscRawPatch>> Patch = FDiscRawPatch::Open(*Path);
			if (!Patch)
			{
				return Patch.Error();
			}
			Kept = std::move(*Patch);
		}
		return Kept;
	}
}
