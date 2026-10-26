#include "Media/GameLibrary.h"

#include "Foundation/HiddenFiles.h"
#include "Media/CdImage.h"
#include "Media/CueSheet.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace Weltall
{
	namespace
	{
		std::string LowerExtension(const std::filesystem::path& Path)
		{
			std::string Extension = Path.extension().string();
			std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](char C) { return static_cast<char>(std::tolower(static_cast<unsigned char>(C))); });
			return Extension;
		}

		bool IsSheet(const std::filesystem::path& Path)
		{
			return LowerExtension(Path) == ".cue";
		}

		bool IsDiscImage(const std::filesystem::path& Path)
		{
			const std::string Extension = LowerExtension(Path);
			return Extension == ".cue" || Extension == ".bin" || Extension == ".iso" || Extension == ".img" || Extension == ".chd";
		}

		/** Disc images of kinds the game does not read, and what to do with them. */
		struct FUnreadKind
		{
			std::string_view Extension;
			std::string_view Text;
		};

		constexpr std::array<FUnreadKind, 9> UnreadKinds{{
			{".pbp", "A PSP eboot, which the game cannot read: convert it to BIN/CUE."},
			{".ecm", "An ECM-packed image: unpack it to its .bin first (unecm)."},
			{".zip", "An archive: take the disc image out of it."},
			{".7z", "An archive: take the disc image out of it."},
			{".rar", "An archive: take the disc image out of it."},
			{".mds", "An Alcohol 120% image, which the game cannot read: convert it to BIN/CUE."},
			{".mdf", "An Alcohol 120% image, which the game cannot read: convert it to BIN/CUE."},
			{".nrg", "A Nero image, which the game cannot read: convert it to BIN/CUE."},
			{".cdi", "A DiscJuggler image, which the game cannot read: convert it to BIN/CUE."},
		}};

		const FUnreadKind* UnreadKindOf(const std::filesystem::path& Path)
		{
			const std::string Extension = LowerExtension(Path);
			for (const FUnreadKind& Kind : UnreadKinds)
			{
				if (Kind.Extension == Extension)
				{
					return &Kind;
				}
			}
			return nullptr;
		}

		/** The files a sheet's tracks lie in: the sheet stands for them. */
		/**
		 * The files Sheet names added to Files; the first of them that is not there, if one is not (a .cue copied without
		 * its .bin).
		 */
		std::optional<std::filesystem::path> AddSheetFiles(const std::filesystem::path& Sheet, std::set<std::filesystem::path>& Files)
		{
			std::ifstream Stream(Sheet, std::ios::binary);
			std::ostringstream Text;
			Text << Stream.rdbuf();
			std::optional<std::filesystem::path> Missing;
			if (const TResult<std::vector<FCueTrack>> Tracks = ParseCueSheet(Text.str(), Sheet.parent_path()))
			{
				for (const FCueTrack& Track : *Tracks)
				{
					Files.insert(Track.File.lexically_normal());
					std::error_code Error;
					if (!Missing && !std::filesystem::is_regular_file(Track.File, Error))
					{
						Missing = Track.File;
					}
				}
			}
			return Missing;
		}

		/**
		 * Whether the image holds every file of the game's disc: one still being copied, or a rip cut short, ends early.
		 * A copy sized before it is written holds zeros where it has not been yet, which in a raw image are no sector
		 * at all. The game's own file index names the files its directories do not.
		 */
		bool HoldsEveryFile(const FIso9660& Disc)
		{
			uint64 End = 0;
			for (const FIsoEntry& Entry : Disc.Entries())
			{
				End = std::max(End, static_cast<uint64>(Entry.Lba) + Entry.SectorCount());
			}
			const TResult<FPsxExecutable> Executable = ReadBootExecutable(Disc);
			const TResult<FGameFileIndex> Index = Executable ? FGameFileIndex::Read(*Executable) : TResult<FGameFileIndex>(MakeError("no program"));
			if (!Index)
			{
				return false;
			}
			for (const FIndexRecord& Record : Index->Records())
			{
				if (Record.IsFile())
				{
					End = std::max(End, Record.Lba + (static_cast<uint64>(Record.Size) + CdSector::Form1DataSize - 1) / CdSector::Form1DataSize);
				}
			}
			if (End > Disc.Image().SectorCount())
			{
				return false;
			}
			if (!Disc.Image().IsRaw() || End == 0)
			{
				return true;
			}
			const TResult<FCdSectorHeader> Last = Disc.Image().ReadHeader(static_cast<uint32>(End - 1));
			return Last && Last->Mode != 0;
		}

		/** The note on an original a patched copy of it plays in place of. */
		constexpr const char* ReplacedByPatch = "Not played: a patched copy of this disc is in the folder and plays instead.";

		/** Why a disc image the game read is not one it plays, as a player would put it. */
		std::string NotPlayed(const FIso9660& Disc)
		{
			const TResult<std::string> Boot = FindBootExecutable(Disc);
			if (!Boot)
			{
				return "Not a PlayStation disc.";
			}
			if (*Boot == "SLUS_006.64" || *Boot == "SLUS_006.69")
			{
				return "A USA disc of Xenogears, but changed (patched, or damaged in the rip).";
			}
			if (*Boot == "SLPS_011.60" || *Boot == "SLPS_011.61")
			{
				return "A Japanese disc of Xenogears: the game plays the USA release.";
			}
			return "Not a disc of Xenogears' USA release (its program is " + *Boot + ").";
		}
	}

	FGameLibrary FGameLibrary::Scan(const std::filesystem::path& Folder)
	{
		FGameLibrary Library;
		std::vector<std::filesystem::path> Candidates;
		std::error_code Error;
		for (auto Entry = std::filesystem::recursive_directory_iterator(Folder, std::filesystem::directory_options::skip_permission_denied, Error);
			 !Error && Entry != std::filesystem::recursive_directory_iterator(); Entry.increment(Error))
		{
			if (IsHiddenFile(Entry->path()))
			{
				Entry.disable_recursion_pending();
				continue;
			}
			if (!Entry->is_regular_file(Error))
			{
				continue;
			}
			if (IsDiscImage(Entry->path()))
			{
				Candidates.push_back(Entry->path());
			}
			else if (const FUnreadKind* Kind = UnreadKindOf(Entry->path()))
			{
				Library.LibraryNotes.push_back({Entry->path(), std::string(Kind->Text)});
			}
		}
		// Sheets first: a disc is opened through its sheet, and the files the sheet names are not opened again.
		std::sort(Candidates.begin(), Candidates.end(), [](const std::filesystem::path& A, const std::filesystem::path& B) {
			return IsSheet(A) != IsSheet(B) ? IsSheet(A) : A < B;
		});

		std::set<std::filesystem::path> SheetFiles;
		for (const std::filesystem::path& Path : Candidates)
		{
			if (IsSheet(Path))
			{
				if (const std::optional<std::filesystem::path> Missing = AddSheetFiles(Path, SheetFiles))
				{
					const std::u8string Name = Missing->filename().u8string();
					Library.LibraryNotes.push_back({Path, "The file it names, " + std::string(Name.begin(), Name.end()) + ", is not beside it."});
					continue;
				}
			}
			else if (SheetFiles.contains(Path.lexically_normal()))
			{
				continue;
			}
			auto Image = FCdImage::Open(Path);
			if (!Image)
			{
				Library.LibraryNotes.push_back({Path, "Cannot be read: " + Image.Error().Message + "."});
				continue;
			}
			const bool bCooked = !Image->IsRaw();
			auto Disc = FIso9660::Open(std::move(*Image));
			if (!Disc)
			{
				Library.LibraryNotes.push_back({Path, "Not a CD's image the game can read."});
				continue;
			}
			const TResult<FDiscIdentity> Identity = IdentifyDisc(*Disc);
			if (!Identity)
			{
				Library.LibraryNotes.push_back({Path, NotPlayed(*Disc)});
				continue;
			}
			// A patched disc (a fan translation's) plays in place of the original of its release; of two alike, the first found.
			const auto Existing = Library.Discs.find(Identity->Release);
			if (Existing != Library.Discs.end() && (!Identity->bPatched || Existing->second.bPatched))
			{
				if (!Identity->bPatched && Existing->second.bPatched)
				{
					Library.LibraryNotes.push_back({Path, ReplacedByPatch});
				}
				continue;
			}
			if (!HoldsEveryFile(*Disc))
			{
				Library.LibraryNotes.push_back({Path, "Ends too soon: still being copied, or a rip cut short."});
				continue;
			}
			if (Existing != Library.Discs.end())
			{
				Library.LibraryNotes.push_back({Existing->second.File, ReplacedByPatch});
				Library.Discs.erase(Existing);
			}
			if (Identity->bPatched)
			{
				Library.LibraryNotes.push_back({Path, "A patched copy of disc " + std::to_string(Identity->DiscNumber) + " (a fan translation, for example). It plays in place of the original.", true});
			}
			if (bCooked && IsPlayableRelease(Identity->Release))
			{
				Library.LibraryNotes.push_back({Path, "Plays, but the movies are silent: an image of the data alone keeps no sound for them. "
													  "A whole rip (BIN/CUE, or a CHD made from one) has it.",
					true});
			}
			if (!IsPlayableRelease(Identity->Release))
			{
				Library.LibraryNotes.push_back({Path, "A Japanese disc: available for asset extraction. Playing requires a USA or supported undub disc."});
			}
			Library.Discs.emplace(Identity->Release, FFound{std::make_unique<FIso9660>(std::move(*Disc)), Path, Identity->bPatched});
		}
		return Library;
	}

	const FIso9660* FGameLibrary::Find(EDiscRelease Release) const
	{
		const auto Found = Discs.find(Release);
		return Found == Discs.end() ? nullptr : Found->second.Disc.get();
	}

	const FIso9660* FGameLibrary::PlayableDisc(int32 Number) const
	{
		if (Number != 1 && Number != 2)
		{
			return nullptr;
		}
		// A translation's copy before the originals, so that its text is played whichever voices it carries.
		const std::array<EDiscRelease, 2> Releases = Number == 1 ? std::array{EDiscRelease::UsaDisc1, EDiscRelease::UndubDisc1}
															  : std::array{EDiscRelease::UsaDisc2, EDiscRelease::UndubDisc2};
		for (const bool bPatched : {true, false})
		{
			for (const EDiscRelease Release : Releases)
			{
				const auto Found = Discs.find(Release);
				if (Found != Discs.end() && Found->second.bPatched == bPatched)
				{
					return Found->second.Disc.get();
				}
			}
		}
		return nullptr;
	}

	std::filesystem::path FGameLibrary::FileOf(EDiscRelease Release) const
	{
		const auto Found = Discs.find(Release);
		return Found == Discs.end() ? std::filesystem::path() : Found->second.File;
	}

	std::vector<EDiscRelease> FGameLibrary::Releases() const
	{
		std::vector<EDiscRelease> Result;
		for (const auto& [Release, Found] : Discs)
		{
			Result.push_back(Release);
		}
		return Result;
	}

	void FGameLibrary::Adopt(FGameLibrary&& Newer)
	{
		for (auto& [Release, Found] : Newer.Discs)
		{
			Discs.try_emplace(Release, std::move(Found));
		}
		LibraryNotes = std::move(Newer.LibraryNotes);
	}
}
