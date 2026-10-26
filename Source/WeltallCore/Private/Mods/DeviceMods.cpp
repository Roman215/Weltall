#include "Mods/DeviceMods.h"

#include "Foundation/AssetKey.h"
#include "Foundation/HiddenFiles.h"
#include "Graphics/BeetleTextureNames.h"
#include "Mods/PerfectWorks.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <fstream>
#include <system_error>

namespace Weltall
{
	namespace
	{
		namespace fs = std::filesystem;

		constexpr std::string_view RemovedPrefix = ".Removed-";
		/** How many folders deep below the folder added a Perfect Works release is looked for. */
		constexpr int32 DeepestPerfectWorks = 3;
		/** A pack's name as a folder's, at most this many bytes. */
		constexpr std::size_t LongestName = 120;

		std::string Utf8(const fs::path& Path)
		{
			const std::u8string Text = Path.u8string();
			return std::string(Text.begin(), Text.end());
		}

		fs::path PathOf(std::string_view Text)
		{
			return fs::path(std::u8string(Text.begin(), Text.end()));
		}

		std::string Lower(std::string Text)
		{
			std::transform(Text.begin(), Text.end(), Text.begin(), [](unsigned char Character) { return static_cast<char>(Character >= 'A' && Character <= 'Z' ? Character + 32 : Character); });
			return Text;
		}

		fs::path ModsFolder(const fs::path& Root)
		{
			return Root / "Mods";
		}

		fs::path AssetsFolder(const fs::path& Root)
		{
			return Root / "Assets";
		}

		fs::path PerfectWorksFolder(const fs::path& Root)
		{
			return ModsFolder(Root) / "PerfectWorks";
		}

		/** The release added while the game used another, put in place when it next starts. */
		fs::path NextPerfectWorks(const fs::path& Root)
		{
			return ModsFolder(Root) / ".PerfectWorks.next";
		}

		/** The mark of a release in the game's use that the player removed: it goes when the game next starts. */
		fs::path PerfectWorksRemovedMark(const fs::path& Root)
		{
			return ModsFolder(Root) / ".PerfectWorks.removed";
		}

		/** A file or folder the player gave: not hidden, nor the folder of macOS's companions that a zip made there carries. */
		bool IsPlayers(const fs::path& Path)
		{
			return !IsHiddenFile(Path) && Path.filename() != "__MACOSX";
		}

		/** Folder's own files and folders the player gave, by name. */
		std::vector<fs::directory_entry> Entries(const fs::path& Folder)
		{
			std::vector<fs::directory_entry> Listed;
			std::error_code Error;
			for (const fs::directory_entry& Entry : fs::directory_iterator(Folder, Error))
			{
				if (IsPlayers(Entry.path()))
				{
					Listed.push_back(Entry);
				}
			}
			std::sort(Listed.begin(), Listed.end(), [](const fs::directory_entry& A, const fs::directory_entry& B) { return A.path().filename() < B.path().filename(); });
			return Listed;
		}

		bool IsFolder(const fs::path& Path)
		{
			std::error_code Error;
			return fs::is_directory(fs::symlink_status(Path, Error)) && !Error;
		}

		bool Exists(const fs::path& Path)
		{
			std::error_code Error;
			return fs::exists(fs::symlink_status(Path, Error));
		}

		/** Every file below Folder the player gave, visited in no particular order; true to go on. */
		template <typename TVisit>
		void VisitFiles(const fs::path& Folder, TVisit Visit)
		{
			std::error_code Error;
			for (auto Entry = fs::recursive_directory_iterator(Folder, Error); !Error && Entry != fs::recursive_directory_iterator(); Entry.increment(Error))
			{
				if (!IsPlayers(Entry->path()))
				{
					Entry.disable_recursion_pending();
					continue;
				}
				std::error_code FileError;
				if (Entry->is_regular_file(FileError) && !Visit(*Entry))
				{
					return;
				}
			}
		}

		int32 CountFiles(const fs::path& Folder)
		{
			int32 Count = 0;
			VisitFiles(Folder, [&Count](const fs::directory_entry&)
			{
				++Count;
				return true;
			});
			return Count;
		}

		/** Every byte below Path, hidden files' too: what it takes on the device. */
		uint64 BytesBelow(const fs::path& Path)
		{
			std::error_code Error;
			if (!IsFolder(Path))
			{
				const uintmax_t Size = fs::file_size(Path, Error);
				return Error ? 0 : static_cast<uint64>(Size);
			}
			uint64 Bytes = 0;
			for (auto Entry = fs::recursive_directory_iterator(Path, Error); !Error && Entry != fs::recursive_directory_iterator(); Entry.increment(Error))
			{
				std::error_code SizeError;
				if (Entry->is_regular_file(SizeError))
				{
					const uintmax_t Size = Entry->file_size(SizeError);
					Bytes += SizeError ? 0 : static_cast<uint64>(Size);
				}
			}
			return Bytes;
		}

		/** Path moved out of the game's sight beside the other removed, for DeleteRemovedMods; false where it could not be. */
		bool SetAside(const fs::path& Root, const fs::path& Path, std::string& Detail)
		{
			if (!Exists(Path))
			{
				return true;
			}
			const auto Now = std::chrono::system_clock::now().time_since_epoch().count();
			for (int64 Attempt = 0; Attempt < 1000; ++Attempt)
			{
				const fs::path Aside = Root / PathOf(std::string(RemovedPrefix) + std::to_string(Now + Attempt));
				if (Exists(Aside))
				{
					continue;
				}
				std::error_code Error;
				fs::rename(Path, Aside, Error);
				if (Error)
				{
					Detail = "cannot move " + Utf8(Path) + " aside: " + Error.message();
					return false;
				}
				return true;
			}
			Detail = "no free name to move " + Utf8(Path) + " aside";
			return false;
		}

		/** From into Into's place: a file there replaced, a folder set aside first. */
		bool Replace(const fs::path& Root, const fs::path& From, const fs::path& Into, std::string& Detail)
		{
			std::error_code Error;
			fs::create_directories(Into.parent_path(), Error);
			if (Error)
			{
				Detail = "cannot make " + Utf8(Into.parent_path()) + ": " + Error.message();
				return false;
			}
			if ((IsFolder(From) || IsFolder(Into)) && !SetAside(Root, Into, Detail))
			{
				return false;
			}
			fs::rename(From, Into, Error);
			if (Error)
			{
				Detail = "cannot move " + Utf8(From) + " to " + Utf8(Into) + ": " + Error.message();
				return false;
			}
			return true;
		}

		/** What a zip of one folder, or a folder picked around one, wraps: down while a folder holds one folder and nothing else. */
		fs::path ContentOf(const fs::path& Incoming)
		{
			fs::path Content = Incoming;
			while (true)
			{
				const std::vector<fs::directory_entry> Listed = Entries(Content);
				if (Listed.size() != 1 || !IsFolder(Listed.front().path()))
				{
					return Content;
				}
				Content = Listed.front().path();
			}
		}

		bool HoldsWeltallFolders(const fs::path& Folder)
		{
			return IsFolder(Folder / "Mods") || IsFolder(Folder / "Assets");
		}

		/** A release of Perfect Works Build below Folder, the nearest its top: the folder holding gamefiles, or gamefiles itself (bBare). */
		struct FFoundRelease
		{
			fs::path Folder;
			bool bBare = false;
		};

		std::optional<FFoundRelease> FindPerfectWorks(const fs::path& Folder)
		{
			std::deque<std::pair<fs::path, int32>> Left{{Folder, 0}};
			while (!Left.empty())
			{
				const auto [Looked, Depth] = Left.front();
				Left.pop_front();
				if (FPerfectWorksRelease::IsRelease(Looked))
				{
					return FFoundRelease{Looked, false};
				}
				if (IsFolder(Looked / "bug_fix1") && IsFolder(Looked / "script1"))
				{
					return FFoundRelease{Looked, true};
				}
				if (Depth == DeepestPerfectWorks)
				{
					continue;
				}
				for (const fs::directory_entry& Entry : Entries(Looked))
				{
					if (IsFolder(Entry.path()))
					{
						Left.emplace_back(Entry.path(), Depth + 1);
					}
				}
			}
			return std::nullopt;
		}

		/** A picture a texture pack names as Beetle PSX HW's do, or by a Weltall key; a sheet's layout beside its picture. */
		bool IsTextureFile(const fs::path& File)
		{
			static constexpr std::array<std::string_view, 5> Pictures{".png", ".jpg", ".jpeg", ".bmp", ".tga"};
			const std::string Extension = Lower(Utf8(File.extension()));
			const std::string Stem = Utf8(File.stem());
			if (std::find(Pictures.begin(), Pictures.end(), Extension) != Pictures.end() && BeetleTextureNameOf(Stem))
			{
				return true;
			}
			if (Extension == ".png" && AssetKeyOfName(Stem))
			{
				return true;
			}
			std::error_code Error;
			return Extension == ".tsv" && fs::is_regular_file(fs::path(File).replace_extension(".png"), Error);
		}

		bool IsMusicFile(const fs::path& File)
		{
			return Lower(Utf8(File.extension())) == ".wav" && AssetKeyOfName(Utf8(File.stem()));
		}

		bool HoldsAny(const fs::path& Folder, bool (*Kind)(const fs::path&))
		{
			bool bFound = false;
			VisitFiles(Folder, [&bFound, Kind](const fs::directory_entry& Entry)
			{
				bFound = Kind(Entry.path());
				return !bFound;
			});
			return bFound;
		}

		/** A pack's name as a folder may take it: the ".zip" left out, characters folders may not have made '_'. */
		std::string PackName(std::string_view Picked)
		{
			std::string Name(Picked);
			if (Lower(Name).ends_with(".zip"))
			{
				Name.resize(Name.size() - 4);
			}
			for (char& Character : Name)
			{
				if (static_cast<unsigned char>(Character) < 0x20 || std::string_view("\\/:*?\"<>|").find(Character) != std::string_view::npos)
				{
					Character = '_';
				}
			}
			if (Name.size() > LongestName)
			{
				std::size_t Kept = LongestName;
				while (Kept > 0 && (static_cast<unsigned char>(Name[Kept]) & 0xC0) == 0x80)
				{
					--Kept;
				}
				Name.resize(Kept);
			}
			const std::size_t First = Name.find_first_not_of(" .");
			const std::size_t Last = Name.find_last_not_of(" .");
			Name = First == std::string::npos ? std::string() : Name.substr(First, Last - First + 1);
			return Name.empty() ? std::string("Pack") : Name;
		}

		/** A release put in Mods/PerfectWorks, or beside it to replace it at the next start where the game has one. */
		bool AddPerfectWorks(const FFoundRelease& Release, const fs::path& Root, FAddedMod& Added, std::string& Detail)
		{
			const bool bInUse = FPerfectWorksRelease::IsRelease(PerfectWorksFolder(Root));
			const fs::path Target = bInUse ? NextPerfectWorks(Root) : PerfectWorksFolder(Root);
			if (Release.bBare)
			{
				// Its gamefiles alone, in a folder of their own.
				if (!SetAside(Root, Target, Detail) || !Replace(Root, Release.Folder, Target / "gamefiles", Detail))
				{
					return false;
				}
			}
			else if (!Replace(Root, Release.Folder, Target, Detail))
			{
				return false;
			}
			if (bInUse)
			{
				std::error_code Error;
				fs::remove(PerfectWorksRemovedMark(Root), Error);
			}
			Added.Name = FPerfectWorksRelease::VersionIn(Target);
			Added.Files += CountFiles(Target);
			Added.bOnRestart = Added.bOnRestart || bInUse;
			return true;
		}

		/** A Weltall folder's Mods (each folder in it whole) and Assets (each file) put in their places. */
		bool AddFolders(const fs::path& Content, const fs::path& Root, FAddedMod& Added, std::string& Detail)
		{
			for (const fs::directory_entry& Entry : Entries(Content / "Mods"))
			{
				if (Entry.path().filename() == "PerfectWorks" && IsFolder(Entry.path()))
				{
					const std::string Name = Added.Name;
					if (!AddPerfectWorks({Entry.path(), false}, Root, Added, Detail))
					{
						return false;
					}
					Added.Name = Name;
					continue;
				}
				const int32 Files = IsFolder(Entry.path()) ? CountFiles(Entry.path()) : 1;
				if (!Replace(Root, Entry.path(), ModsFolder(Root) / Entry.path().filename(), Detail))
				{
					return false;
				}
				Added.Files += Files;
			}
			const fs::path Assets = Content / "Assets";
			std::vector<fs::path> Files;
			VisitFiles(Assets, [&Files](const fs::directory_entry& Entry) { return Files.push_back(Entry.path()), true; });
			for (const fs::path& File : Files)
			{
				if (!Replace(Root, File, AssetsFolder(Root) / File.lexically_relative(Assets), Detail))
				{
					return false;
				}
				++Added.Files;
			}
			return true;
		}

		/** The mods and packs below Root, each one's bytes counted where bSizes asks. */
		std::vector<FModItem> ModsBelow(const fs::path& Root, bool bSizes)
		{
			std::vector<FModItem> Items;
			const auto Add = [&Items, &Root, bSizes](EModItemKind Kind, const fs::path& Path, std::string Name)
			{
				FModItem Item;
				Item.Kind = Kind;
				Item.Path = Path.lexically_relative(Root);
				Item.Name = std::move(Name);
				Item.Bytes = bSizes ? BytesBelow(Path) : 0;
				Items.push_back(std::move(Item));
			};
			const fs::path PerfectWorks = PerfectWorksFolder(Root);
			const bool bRelease = FPerfectWorksRelease::IsRelease(PerfectWorks);
			if (bRelease)
			{
				const std::string Version = FPerfectWorksRelease::VersionIn(PerfectWorks);
				Add(EModItemKind::PerfectWorks, PerfectWorks, Version.empty() ? "Perfect Works" : "Perfect Works " + Version);
				Items.back().bRemovedOnRestart = Exists(PerfectWorksRemovedMark(Root));
				Items.back().bReplacedOnRestart = IsFolder(NextPerfectWorks(Root));
			}
			for (const fs::directory_entry& Entry : Entries(ModsFolder(Root)))
			{
				if (IsFolder(Entry.path()) && !(bRelease && Entry.path().filename() == PerfectWorks.filename()))
				{
					Add(EModItemKind::Mod, Entry.path(), "Mods/" + Utf8(Entry.path().filename()));
				}
			}
			const fs::path Assets = AssetsFolder(Root);
			for (const auto& [Kind, Folder] : {std::pair{EModItemKind::Textures, std::string_view("textures")}, std::pair{EModItemKind::Music, std::string_view("music")}})
			{
				for (const fs::directory_entry& Entry : Entries(Assets / Folder))
				{
					if (IsFolder(Entry.path()))
					{
						Add(Kind, Entry.path(), Utf8(Entry.path().filename()));
					}
				}
			}
			for (const fs::directory_entry& Entry : Entries(Assets))
			{
				const fs::path Name = Entry.path().filename();
				if (!IsFolder(Entry.path()) || (Name != "textures" && Name != "music"))
				{
					Add(EModItemKind::Assets, Entry.path(), "Assets/" + Utf8(Name));
				}
			}
			return Items;
		}
	}

	FModAddition AddMod(const fs::path& Incoming, const fs::path& Root, std::string_view Name)
	{
		FModAddition Result;
		FAddedMod Added;
		Added.Name = PackName(Name);
		const fs::path Content = ContentOf(Incoming);
		bool bDone = false;
		if (HoldsWeltallFolders(Content))
		{
			Added.Content = EAddedContent::Folders;
			bDone = AddFolders(Content, Root, Added, Result.Detail);
		}
		else if (const std::optional<FFoundRelease> Release = FindPerfectWorks(Incoming))
		{
			Added.Content = EAddedContent::PerfectWorks;
			bDone = AddPerfectWorks(*Release, Root, Added, Result.Detail);
		}
		else if (const bool bTextures = HoldsAny(Content, IsTextureFile); bTextures || HoldsAny(Content, IsMusicFile))
		{
			Added.Content = bTextures ? EAddedContent::Textures : EAddedContent::Music;
			Added.Files = CountFiles(Content);
			bDone = Replace(Root, Content, AssetsFolder(Root) / (bTextures ? "textures" : "music") / PathOf(Added.Name), Result.Detail);
		}
		else
		{
			Result.Failure = EModAddFailure::NotAMod;
			Result.Detail = "no mod or pack the game reads";
		}
		if (bDone)
		{
			Result.Added = std::move(Added);
		}
		else if (Result.Failure == EModAddFailure::None)
		{
			Result.Failure = EModAddFailure::Unwritable;
		}
		std::string Ignored;
		SetAside(Root, Incoming, Ignored);
		return Result;
	}

	std::vector<FModItem> ListMods(const fs::path& Root)
	{
		return ModsBelow(Root, true);
	}

	EModRemoval RemoveMod(const fs::path& Root, const fs::path& Path)
	{
		const std::vector<FModItem> Items = ModsBelow(Root, false);
		const auto Item = std::find_if(Items.begin(), Items.end(), [&Path](const FModItem& Listed) { return Listed.Path == Path; });
		if (Item == Items.end())
		{
			return EModRemoval::Failed;
		}
		std::string Detail;
		if (Item->Kind == EModItemKind::PerfectWorks)
		{
			// The game reads the release by its path while it plays: it goes when the game next starts.
			if (!SetAside(Root, NextPerfectWorks(Root), Detail))
			{
				return EModRemoval::Failed;
			}
			std::error_code Error;
			fs::create_directories(ModsFolder(Root), Error);
			const bool bMarked = std::ofstream(PerfectWorksRemovedMark(Root), std::ios::binary).good();
			return bMarked ? EModRemoval::OnRestart : EModRemoval::Failed;
		}
		return SetAside(Root, Root / Path, Detail) ? EModRemoval::Removed : EModRemoval::Failed;
	}

	void FinishModChanges(const fs::path& Root)
	{
		std::string Detail;
		if (IsFolder(NextPerfectWorks(Root)))
		{
			Replace(Root, NextPerfectWorks(Root), PerfectWorksFolder(Root), Detail);
		}
		else if (Exists(PerfectWorksRemovedMark(Root)))
		{
			SetAside(Root, PerfectWorksFolder(Root), Detail);
		}
		std::error_code Error;
		fs::remove(PerfectWorksRemovedMark(Root), Error);
		SetAside(Root, IncomingModFolder(Root), Detail);
	}

	void DeleteRemovedMods(const fs::path& Root)
	{
		std::error_code Error;
		for (const fs::directory_entry& Entry : fs::directory_iterator(Root, Error))
		{
			if (Utf8(Entry.path().filename()).starts_with(RemovedPrefix))
			{
				std::error_code Ignored;
				fs::remove_all(Entry.path(), Ignored);
			}
		}
	}

	fs::path IncomingModFolder(const fs::path& Root)
	{
		return Root / ".Adding";
	}
}
