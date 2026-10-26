#include "Shell/DeviceFilesSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/Paths.h"
#include "Shell/GameModsSubsystem.h"

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Async/Async.h"
#include "GenericPlatform/GenericPlatformOutputDevices.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/DateTime.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"
#include "Shell/GameAssetsSubsystem.h"
#include "Shell/GameDiscsSubsystem.h"
#include "Shell/PlayerFolders.h"
#include "Shell/PlayerSettingsSubsystem.h"
#include "WeltallLog.h"
#endif

namespace
{
	std::string Utf8(const FString& Text)
	{
		return std::string(TCHAR_TO_UTF8(*Text));
	}

	std::filesystem::path PathOf(const FString& Text)
	{
		const std::string Bytes = Utf8(Text);
		return std::filesystem::path(std::u8string(Bytes.begin(), Bytes.end()));
	}

#if PLATFORM_ANDROID
	/** The transfer as Android's thread last told it, which the game's reads; and the mods on the device as last listed. */
	struct FSharedTransfer
	{
		FCriticalSection Lock;
		Weltall::FTransferStatus Status;
		/** Where an ImportFile transfer copies the file picked. */
		std::filesystem::path Folder;
		/** The folder holding Mods and Assets, where a mod added goes. */
		std::filesystem::path Root;
		std::vector<Weltall::FModItem> Mods;
		/** Listings taken, and the one Mods came from: an earlier listing that ends later never replaces a later one. */
		uint64 Listings = 0;
		uint64 ModsListing = 0;
	};

	FSharedTransfer& Shared()
	{
		static FSharedTransfer Transfer;
		return Transfer;
	}

	/** The subsystem, for what Android's thread hands the game's. */
	TWeakObjectPtr<UDeviceFilesSubsystem>& Owner()
	{
		static TWeakObjectPtr<UDeviceFilesSubsystem> Subsystem;
		return Subsystem;
	}

	FString TextOf(const std::filesystem::path& Path)
	{
		const std::u8string Bytes = Path.u8string();
		return FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Bytes.c_str())));
	}

	/** The folder a file picked to import is copied into, emptied for each. */
	FString ImportFolder()
	{
		return FPaths::Combine(FPlatformMisc::GamePersistentDownloadDir(), TEXT("Import"));
	}

	/**
	 * The folder of the game's log, as a path outside the engine's own files: this session's log, and the earlier
	 * sessions' the engine keeps (a crash's among them, set aside as the game started again).
	 */
	FString LogFolder()
	{
		GLog->Flush();
		return FPaths::GetPath(IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FGenericPlatformOutputDevices::GetAbsoluteLogFilename()));
	}

	FString Dated(const TCHAR* Name)
	{
		return FString::Printf(TEXT("Weltall %s %s.zip"), Name, *FDateTime::Now().ToString(TEXT("%Y-%m-%d")));
	}

	bool IsModTransfer(Weltall::EDeviceTransfer Kind)
	{
		return Kind == Weltall::EDeviceTransfer::ModZip || Kind == Weltall::EDeviceTransfer::ModFolder;
	}

	/** The mods below Root listed after a change there, then what it took out deleted: slow for a large mod, so off the game's thread. */
	void ListModsAfterChange(const std::filesystem::path& Root)
	{
		FSharedTransfer& Transfer = Shared();
		uint64 Listing = 0;
		{
			FScopeLock Lock(&Transfer.Lock);
			Listing = ++Transfer.Listings;
		}
		std::vector<Weltall::FModItem> Listed = Weltall::ListMods(Root);
		{
			FScopeLock Lock(&Transfer.Lock);
			if (Listing > Transfer.ModsListing)
			{
				Transfer.Mods = std::move(Listed);
				Transfer.ModsListing = Listing;
				++Transfer.Status.Revision;
			}
		}
		Weltall::DeleteRemovedMods(Root);
	}

	/** What the folder copied in for a mod transfer held put in place, told on the status Android's thread is about to give. */
	void AddCopiedMod(Weltall::ETransferState& State, Weltall::ETransferFailure& Failure, std::optional<Weltall::FAddedMod>& Added, const FString& Picked)
	{
		std::filesystem::path Root;
		{
			FSharedTransfer& Transfer = Shared();
			FScopeLock Lock(&Transfer.Lock);
			Root = Transfer.Root;
		}
		const Weltall::FModAddition Addition = Weltall::AddMod(Weltall::IncomingModFolder(Root), Root, Utf8(Picked));
		if (!Addition.Added)
		{
			State = Weltall::ETransferState::Failed;
			Failure = Addition.Failure == Weltall::EModAddFailure::NotAMod ? Weltall::ETransferFailure::NotAMod : Weltall::ETransferFailure::Unwritable;
			UE_LOG(LogWeltall, Warning, TEXT("Nothing was added from %s: %hs"), *Picked, Addition.Detail.c_str());
			return;
		}
		Added = Addition.Added;
		UE_LOG(LogWeltall, Log, TEXT("Added from %s: %hs, %d files (content %d%s)"), *Picked, Added->Name.c_str(), Added->Files, static_cast<int32>(Added->Content),
			Added->bOnRestart ? TEXT(", in use from the next start") : TEXT(""));
	}
#endif
}

#if PLATFORM_ANDROID
JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeWeltallFiles(JNIEnv* Env, jobject, jint State, jint Failure, jstring Name,
	jint FileNumber, jint FileCount, jlong Copied, jlong Total, jstring Detail)
{
	const FString Named = FJavaHelper::FStringFromParam(Env, Name);
	Weltall::ETransferState Now = static_cast<Weltall::ETransferState>(State);
	Weltall::ETransferFailure Why = static_cast<Weltall::ETransferFailure>(Failure);
	FSharedTransfer& Transfer = Shared();
	Weltall::EDeviceTransfer Kind = Weltall::EDeviceTransfer::Discs;
	{
		FScopeLock Lock(&Transfer.Lock);
		Kind = Transfer.Status.Kind;
	}
	std::optional<Weltall::FAddedMod> Added;
	const bool bModCopied = IsModTransfer(Kind) && Now == Weltall::ETransferState::Done;
	if (bModCopied)
	{
		AddCopiedMod(Now, Why, Added, Named);
	}
	{
		FScopeLock Lock(&Transfer.Lock);
		Weltall::FTransferStatus& Status = Transfer.Status;
		Status.State = Now;
		Status.Failure = Why;
		Status.Name = Utf8(Named);
		Status.File = FileNumber;
		Status.Files = FileCount;
		Status.Copied = static_cast<uint64>(Copied);
		Status.Total = static_cast<uint64>(Total);
		Status.Added = Added;
		if (Status.Kind == Weltall::EDeviceTransfer::ImportFile && Now == Weltall::ETransferState::Done)
		{
			Status.Picked = Transfer.Folder / PathOf(Named);
		}
		++Status.Revision;
	}
	if (Now == Weltall::ETransferState::Failed && !bModCopied)
	{
		UE_LOG(LogWeltall, Warning, TEXT("A transfer through the file picker failed (%d) at %s: %s"), Failure, *Named, *FJavaHelper::FStringFromParam(Env, Detail));
	}
	else if (Now == Weltall::ETransferState::Done)
	{
		UE_LOG(LogWeltall, Log, TEXT("A transfer through the file picker copied %d files, %lld bytes"), FileCount, static_cast<long long>(Copied));
	}
	if (bModCopied)
	{
		if (Added)
		{
			AsyncTask(ENamedThreads::GameThread, [Content = Added->Content]()
			{
				if (UDeviceFilesSubsystem* Subsystem = Owner().Get())
				{
					Subsystem->FollowAddedMod(Content);
				}
			});
		}
		std::filesystem::path Root;
		{
			FScopeLock Lock(&Transfer.Lock);
			Root = Transfer.Root;
		}
		ListModsAfterChange(Root);
	}
}
#endif

std::filesystem::path UDeviceFilesSubsystem::PlayerFoldersRoot()
{
	return PathOf(FPaths::GetPath(UGameModsSubsystem::ModsFolder()));
}

void UDeviceFilesSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
#if PLATFORM_ANDROID
	// The mods' changes that waited for this start are made as the mods are looked for.
	Collection.InitializeDependency<UGameModsSubsystem>();
	Owner() = this;
	Async(EAsyncExecution::ThreadPool, [Root = PlayerFoldersRoot()]() { ListModsAfterChange(Root); });
#endif
}

Weltall::FTransferStatus UDeviceFilesSubsystem::Status() const
{
#if PLATFORM_ANDROID
	FSharedTransfer& Transfer = Shared();
	FScopeLock Lock(&Transfer.Lock);
	return Transfer.Status;
#else
	return {};
#endif
}

std::vector<Weltall::FModItem> UDeviceFilesSubsystem::Mods() const
{
#if PLATFORM_ANDROID
	FSharedTransfer& Transfer = Shared();
	FScopeLock Lock(&Transfer.Lock);
	return Transfer.Mods;
#else
	return {};
#endif
}

Weltall::EModRemoval UDeviceFilesSubsystem::RemoveMod(const std::filesystem::path& Path)
{
#if PLATFORM_ANDROID
	if (Status().Running())
	{
		return Weltall::EModRemoval::Failed;
	}
	const std::filesystem::path Root = PlayerFoldersRoot();
	const Weltall::EModRemoval Removal = Weltall::RemoveMod(Root, Path);
	UE_LOG(LogWeltall, Log, TEXT("Removing %s: %s"), *TextOf(Path),
		Removal == Weltall::EModRemoval::Removed ? TEXT("removed") : Removal == Weltall::EModRemoval::OnRestart ? TEXT("removed from the next start") : TEXT("failed"));
	if (Removal == Weltall::EModRemoval::Removed && !Path.empty() && *Path.begin() == "Assets")
	{
		GetGameInstance()->GetSubsystem<UGameAssetsSubsystem>()->ReloadReplacements();
	}
	Async(EAsyncExecution::ThreadPool, [Root]() { ListModsAfterChange(Root); });
	return Removal;
#else
	return Weltall::EModRemoval::Failed;
#endif
}

void UDeviceFilesSubsystem::FollowAddedMod(Weltall::EAddedContent Content)
{
#if PLATFORM_ANDROID
	UGameInstance* Game = GetGameInstance();
	if (Content == Weltall::EAddedContent::PerfectWorks || Content == Weltall::EAddedContent::Folders)
	{
		Game->GetSubsystem<UGameModsSubsystem>()->FindAgain();
	}
	// Added to be used: Perfect Works switched on from the next start, a pack's External assets at once (which finds the
	// replacements again), as the options screen does where it is open.
	UPlayerSettingsSubsystem* PlayerSettings = Game->GetSubsystem<UPlayerSettingsSubsystem>();
	Weltall::FPlayerSettings Settings = PlayerSettings->Settings();
	bool& Switch = Content == Weltall::EAddedContent::PerfectWorks ? Settings.PerfectWorks.bEnabled : Settings.bExternalAssets;
	if (!Switch)
	{
		Switch = true;
		PlayerSettings->Change(Settings);
	}
	else if (Content != Weltall::EAddedContent::PerfectWorks)
	{
		Game->GetSubsystem<UGameAssetsSubsystem>()->ReloadReplacements();
	}
#endif
}

bool UDeviceFilesSubsystem::Start(Weltall::EDeviceTransfer Kind, const std::filesystem::path& File)
{
#if PLATFORM_ANDROID
	using Weltall::EDeviceTransfer;
	const std::filesystem::path Root = PlayerFoldersRoot();
	FString Path;
	FString Title;
	switch (Kind)
	{
	case EDeviceTransfer::Discs:
	case EDeviceTransfer::DiscFolder:
		Path = UGameDiscsSubsystem::GamesFolder();
		break;
	case EDeviceTransfer::BackUp:
		Path = PlayerSavesFolder();
		Title = Dated(TEXT("saves"));
		break;
	case EDeviceTransfer::Restore:
		Path = PlayerSavesFolder();
		break;
	case EDeviceTransfer::Log:
		Path = LogFolder();
		Title = Dated(TEXT("logs"));
		break;
	case EDeviceTransfer::ImportFile:
		Path = ImportFolder();
		break;
	case EDeviceTransfer::CardImage:
		Path = TextOf(File);
		Title = FPaths::GetCleanFilename(Path);
		break;
	case EDeviceTransfer::ModZip:
	case EDeviceTransfer::ModFolder:
		// Android's thread empties it first, then copies what was picked into it.
		Path = TextOf(Weltall::IncomingModFolder(Root));
		break;
	}
	{
		FSharedTransfer& Transfer = Shared();
		FScopeLock Lock(&Transfer.Lock);
		Weltall::FTransferStatus& Status = Transfer.Status;
		if (Status.Running())
		{
			return false;
		}
		const uint64 Revision = Status.Revision;
		Status = {};
		Status.Kind = Kind;
		Status.State = Weltall::ETransferState::Picking;
		Status.Name = Utf8(Title);
		Status.Revision = Revision + 1;
		Transfer.Folder = PathOf(Path);
		Transfer.Root = Root;
	}
	if (Kind == EDeviceTransfer::ImportFile)
	{
		IFileManager::Get().DeleteDirectory(*Path, false, true);
	}
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	static const jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_WeltallFiles",
		"(ILjava/lang/String;Ljava/lang/String;)V", false);
	FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method, static_cast<jint>(Kind), *FJavaHelper::ToJavaString(Env, Path),
		*FJavaHelper::ToJavaString(Env, Title));
	UE_LOG(LogWeltall, Log, TEXT("The file picker opened for transfer %d (%s)"), static_cast<int32>(Kind), *Path);
	return true;
#else
	return false;
#endif
}
