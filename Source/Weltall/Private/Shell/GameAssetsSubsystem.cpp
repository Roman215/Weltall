#include "Shell/GameAssetsSubsystem.h"

#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Misc/Paths.h"
#include "Rendering/TextureModLibrary.h"
#include "Sound/MusicReplacements.h"
#include "Shell/GameDiscsSubsystem.h"
#include "Shell/PlayerSettingsSubsystem.h"
#include "Shell/TextModFiles.h"
#include "WeltallLog.h"

namespace
{
	FString TexturesFolder()
	{
		return FPaths::Combine(UGameAssetsSubsystem::AssetsFolder(), TEXT("textures"));
	}

	std::filesystem::path MusicFolder()
	{
		return std::filesystem::path(*FPaths::Combine(UGameAssetsSubsystem::AssetsFolder(), TEXT("music")));
	}

	std::filesystem::path TextFolder()
	{
		return std::filesystem::path(*FPaths::Combine(UGameAssetsSubsystem::AssetsFolder(), TEXT("text")));
	}

	FString FontFolder()
	{
		return FPaths::Combine(UGameAssetsSubsystem::AssetsFolder(), TEXT("font"));
	}
}

FString UGameAssetsSubsystem::AssetsFolder()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::GetPath(UGameDiscsSubsystem::GamesFolder()), TEXT("Assets")));
}

void UGameAssetsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UGameDiscsSubsystem>();
	Collection.InitializeDependency<UPlayerSettingsSubsystem>();
	Refresh();
	SettingsChanged = GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>()->OnSettingsChanged.AddUObject(this, &UGameAssetsSubsystem::FollowSettings);
}

void UGameAssetsSubsystem::Deinitialize()
{
	CancelExport();
	if (BackgroundStep.IsValid()) { BackgroundStep.Wait(); BackgroundStep = {}; }
	FTextureModLibrary::WaitForCaptures();
	FTextureModLibrary::WaitForDecodes();
	GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>()->OnSettingsChanged.Remove(SettingsChanged);
	Jobs.clear();
	ResetBackgroundJobs();
	Stores.clear();
	Super::Deinitialize();
}

void UGameAssetsSubsystem::Refresh()
{
	bool bFailed = false;
	const Weltall::FGameLibrary& Library = GetGameInstance()->GetSubsystem<UGameDiscsSubsystem>()->Library();
	for (Weltall::EDiscRelease Release : Library.Releases())
	{
		if (Stores.contains(Release)) { continue; }
		const Weltall::FIso9660* Disc = Library.Find(Release);
		auto Executable = Weltall::ReadBootExecutable(*Disc);
		if (!Executable) { Fail(Executable.Error()); bFailed = true; continue; }
		auto Index = Weltall::FGameFileIndex::Read(*Executable);
		if (!Index) { Fail(Index.Error()); bFailed = true; continue; }
		auto Store = Weltall::FDiscAssetStore::Open(std::filesystem::path(*AssetsFolder()), std::string(Weltall::DiscReleaseId(Release)), Disc->Image(), *Index);
		if (!Store) { Fail(Store.Error()); bFailed = true; continue; }
		Disc->Image().SetAssetStore(*Store);
		Stores.emplace(Release, std::move(*Store));
	}
	bRegistrationFailed = bFailed;
	FollowSettings();
}

void UGameAssetsSubsystem::ReloadReplacements()
{
	const auto& Settings = GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>()->Settings();
	if (!Settings.bExternalAssets)
	{
		return;
	}
	FTextureModLibrary::Configure(TexturesFolder(), false, Settings.bCaptureTextures);
	bMusicFound = false;
	FollowSettings();
}

void UGameAssetsSubsystem::FollowSettings()
{
	const auto& Settings = GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>()->Settings();
	const bool bEnabled = Settings.bExternalAssets;
	FTextureModLibrary::Configure(TexturesFolder(), bEnabled, Settings.bCaptureTextures);
	// The recordings and messages are found again each time the replacements are switched on.
	if (bEnabled && !bMusicFound)
	{
		auto Found = Weltall::FMusicReplacements::Find(MusicFolder());
		if (!Found) UE_LOG(LogWeltall, Warning, TEXT("Music replacements: %hs"), Found.Error().Message.c_str());
		Music = Found ? *Found : nullptr;
		if (Music) UE_LOG(LogWeltall, Log, TEXT("Music replacements found: %d"), static_cast<int32>(Music->Count()));
		FindMessages();
	}
	bMusicFound = bEnabled;
	for (const auto& [Release, Store] : Stores)
	{
		Store->SetMusic(Music);
		Store->SetMessages(Messages);
		Store->SetFontSheet(FontSheet);
		Store->SetEnabled(bEnabled);
	}
}

void UGameAssetsSubsystem::FindMessages()
{
	FontSheet = FindFontSheet(FontFolder());
	Messages = FindMessageOverrides(TextFolder(), FontSheet.get());
}

bool UGameAssetsSubsystem::StartExport(Weltall::EAssetExportKind Kind)
{
	if (Current.State == Weltall::EAssetExportState::Running || BackgroundStep.IsValid()) { return false; }
	GetGameInstance()->GetSubsystem<UGameDiscsSubsystem>()->Rescan();
	Refresh();
	if (bRegistrationFailed) { return false; }
	Jobs.clear();
	ResetBackgroundJobs();
	Active = 0;
	switch (Kind)
	{
	case Weltall::EAssetExportKind::Textures: return StartTextures();
	case Weltall::EAssetExportKind::Dump: return StartDump();
	case Weltall::EAssetExportKind::Music: return StartMusic();
	case Weltall::EAssetExportKind::Text: return StartText();
	}
	return false;
}

bool UGameAssetsSubsystem::StartDump()
{
	Current = {Weltall::EAssetExportState::Running, Weltall::EAssetExportKind::Dump, 0, 0, Current.Revision + 1};
	for (const auto& [Release, Store] : Stores)
	{
		auto Job = Store->Export();
		if (!Job) { Fail(Job.Error()); return false; }
		Jobs.push_back(std::move(*Job));
	}
	if (Jobs.empty())
	{
		Current.State = Weltall::EAssetExportState::Unavailable;
		return false;
	}
	UpdateProgress();
	UE_LOG(LogWeltall, Log, TEXT("Dumping disc assets to %s"), *AssetsFolder());
	return true;
}

bool UGameAssetsSubsystem::ReadFirstDisc(const Weltall::FIso9660*& Disc, Weltall::EDiscRelease& Release, Weltall::FGameFileIndex& Index)
{
	Disc = GetGameInstance()->GetSubsystem<UGameDiscsSubsystem>()->Library().PlayableDisc(1);
	if (Disc == nullptr)
	{
		Current = {Weltall::EAssetExportState::Unavailable, Current.Kind, 0, 0, Current.Revision + 1};
		return false;
	}
	auto Identity = Weltall::IdentifyDisc(*Disc);
	if (!Identity) { Fail(Identity.Error()); return false; }
	auto Executable = Weltall::ReadBootExecutable(*Disc);
	if (!Executable) { Fail(Executable.Error()); return false; }
	auto Read = Weltall::FGameFileIndex::Read(*Executable);
	if (!Read) { Fail(Read.Error()); return false; }
	Release = Identity->Release;
	Index = std::move(*Read);
	return true;
}

bool UGameAssetsSubsystem::StartMusic()
{
	Current.Kind = Weltall::EAssetExportKind::Music;
	const Weltall::FIso9660* Disc = nullptr;
	Weltall::EDiscRelease Release{};
	Weltall::FGameFileIndex Index;
	if (!ReadFirstDisc(Disc, Release, Index)) { return false; }
	auto Job = Weltall::FDiscMusicWaveExport::Open(*Disc, Index, MusicFolder());
	if (!Job) { Fail(Job.Error()); return false; }
	MusicJob = std::move(*Job);
	Current = {Weltall::EAssetExportState::Running, Weltall::EAssetExportKind::Music, 0, MusicJob->Status().TotalSongs, Current.Revision + 1};
	UE_LOG(LogWeltall, Log, TEXT("Exporting music WAVs to %s"), *AssetsFolder());
	return true;
}

bool UGameAssetsSubsystem::StartTextures()
{
	Current.Kind = Weltall::EAssetExportKind::Textures;
	const Weltall::FIso9660* Disc = nullptr;
	Weltall::EDiscRelease Release{};
	Weltall::FGameFileIndex Index;
	if (!ReadFirstDisc(Disc, Release, Index)) { return false; }
	auto Job = FTextureExport::Open(*Disc, Index, TexturesFolder());
	if (!Job) { Fail(Job.Error()); return false; }
	TextureJob = MoveTemp(*Job);
	Current = {Weltall::EAssetExportState::Running, Weltall::EAssetExportKind::Textures, 0, TextureJob->TotalItems(), Current.Revision + 1};
	UE_LOG(LogWeltall, Log, TEXT("Exporting textures to %s"), *TexturesFolder());
	return true;
}

bool UGameAssetsSubsystem::StartText()
{
	Current.Kind = Weltall::EAssetExportKind::Text;
	const Weltall::FIso9660* Disc = nullptr;
	Weltall::EDiscRelease Release{};
	Weltall::FGameFileIndex Index;
	if (!ReadFirstDisc(Disc, Release, Index)) { return false; }
	auto Job = Weltall::FGameTextExport::Open(*Disc, Index, TextFolder(), std::filesystem::path(*FontFolder()));
	if (!Job) { Fail(Job.Error()); return false; }
	TextJob = std::move(*Job);
	Current = {Weltall::EAssetExportState::Running, Weltall::EAssetExportKind::Text, 0, static_cast<uint64>(TextJob->TotalFiles()), Current.Revision + 1};
	UE_LOG(LogWeltall, Log, TEXT("Exporting text to %s"), UTF8_TO_TCHAR(TextFolder().generic_u8string().c_str()));
	return true;
}

void UGameAssetsSubsystem::CancelExport()
{
	if (Current.State != Weltall::EAssetExportState::Running) { return; }
	for (auto& Job : Jobs) { Job->Cancel(); }
	if (!BackgroundStep.IsValid())
	{
		ResetBackgroundJobs();
	}
	Current.State = Weltall::EAssetExportState::Cancelled;
	++Current.Revision;
}

void UGameAssetsSubsystem::Fail(const Weltall::FError& Error)
{
	for (auto& Job : Jobs) { Job->Cancel(); }
	Current.State = Weltall::EAssetExportState::Failed;
	++Current.Revision;
	UE_LOG(LogWeltall, Error, TEXT("Disc assets: %hs"), Error.Message.c_str());
}

void UGameAssetsSubsystem::UpdateProgress()
{
	uint64 Done = 0;
	uint64 Total = 0;
	for (const auto& Job : Jobs)
	{
		Done += Job->Status().CompletedBytes;
		Total += Job->Status().TotalBytes;
	}
	if (Done != Current.Completed || Total != Current.Total)
	{
		Current.Completed = Done;
		Current.Total = Total;
		++Current.Revision;
	}
}

void UGameAssetsSubsystem::Finish(const TCHAR* Done)
{
	Current.State = Weltall::EAssetExportState::Complete;
	++Current.Revision;
	UE_LOG(LogWeltall, Log, TEXT("%s %s"), Done, *AssetsFolder());
}

void UGameAssetsSubsystem::ResetBackgroundJobs()
{
	MusicJob.reset();
	TextureJob.Reset();
	TextJob.reset();
}

void UGameAssetsSubsystem::TickBackground()
{
	if (!BackgroundStep.IsValid())
	{
		if (Current.State != Weltall::EAssetExportState::Running)
		{
			ResetBackgroundJobs();
			return;
		}
		if (MusicJob)
		{
			BackgroundStep = Async(EAsyncExecution::ThreadPool, [Job = MusicJob.get()]() { return Job->Step(32768); });
		}
		else if (TextJob)
		{
			BackgroundStep = Async(EAsyncExecution::ThreadPool, [Job = TextJob.get()]() { return Job->Step(); });
		}
		else
		{
			BackgroundStep = Async(EAsyncExecution::ThreadPool, [Job = TextureJob.Get()]() { return Job->Step(); });
		}
		return;
	}
	if (!BackgroundStep.IsReady()) { return; }
	auto Stepped = BackgroundStep.Get();
	BackgroundStep = {};
	if (Current.State != Weltall::EAssetExportState::Running)
	{
		ResetBackgroundJobs();
		return;
	}
	if (!Stepped)
	{
		Fail(Stepped.Error());
		ResetBackgroundJobs();
		return;
	}
	const uint64 Done = MusicJob ? MusicJob->Status().CompletedSongs : TextJob ? static_cast<uint64>(TextJob->CompletedFiles()) : TextureJob->CompletedItems();
	if (Current.Completed != Done)
	{
		Current.Completed = Done;
		++Current.Revision;
	}
	if (!*Stepped) { return; }
	const TCHAR* Exported = MusicJob ? TEXT("Music WAVs exported to") : TextJob ? TEXT("Text and font exported to") : TEXT("Textures exported to");
	if (TextJob) { SaveFontSheetPicture(FontFolder(), TextJob->FontPicture()); }
	ResetBackgroundJobs();
	Finish(Exported);
}

void UGameAssetsSubsystem::Tick(float)
{
	if (MusicJob || TextureJob || TextJob)
	{
		TickBackground();
		return;
	}
	if (Active >= Jobs.size()) { return; }
	auto Stepped = Jobs[Active]->Step();
	if (!Stepped) { Fail(Stepped.Error()); return; }
	UpdateProgress();
	if (*Stepped && ++Active == Jobs.size())
	{
		Finish(TEXT("Disc assets dumped to"));
	}
}

ETickableTickType UGameAssetsSubsystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UGameAssetsSubsystem::IsTickable() const
{
	return Current.State == Weltall::EAssetExportState::Running || BackgroundStep.IsValid();
}

UWorld* UGameAssetsSubsystem::GetTickableGameObjectWorld() const
{
	return GetGameInstance()->GetWorld();
}

TStatId UGameAssetsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGameAssetsSubsystem, STATGROUP_Tickables);
}
