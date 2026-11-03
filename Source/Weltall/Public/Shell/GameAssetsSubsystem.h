#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Game/GameTextExport.h"
#include "Media/DiscAssets.h"
#include "Media/DiscIdentification.h"
#include "Sound/DiscMusicWaveExport.h"
#include "Story/Disc2Export.h"
#include "Story/ShareableExpansion.h"
#include "Sound/MusicReplacements.h"
#include "Menu/MenuAssetPages.h"
#include "Shell/TextureExport.h"
#include "Text/MessageOverrides.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"

#include <filesystem>
#include <map>
#include <memory>
#include <vector>

#include "GameAssetsSubsystem.generated.h"

/** Disc exports and replacements in Assets beside the player's Games folder. */
UCLASS()
class WELTALL_API UGameAssetsSubsystem : public UGameInstanceSubsystem, public FTickableGameObject, public Weltall::IMenuAssets
{
	GENERATED_BODY()

public:
	static FString AssetsFolder();
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	void Refresh();
	/** The replacements found again, as External assets switched off and on: after a pack was added or removed. */
	void ReloadReplacements();

	virtual Weltall::FMenuAssetProgress Progress() const override { return Current; }
	virtual bool StartExport(Weltall::EAssetExportKind Kind) override;
	virtual Weltall::EShareableStart StartShareable(std::string_view Folder, bool bReplace) override;
	virtual void CancelExport() override;

	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual TStatId GetStatId() const override;

private:
	void FollowSettings();
	void Fail(const Weltall::FError& Error);
	void UpdateProgress();
	bool StartDump();
	bool StartMusic();
	bool StartTextures();
	bool StartText();
	/** Export Disc 2, into the Mods folder (the Modding page's). */
	bool StartDisc2();
	/** Make shareable of the expansion in Shared, beside it in the Mods folder (the Modding page's). */
	bool StartShared();
	/** The player's font sheet (Assets/font) and messages (Assets/text), read again each time external assets are switched on. */
	void FindMessages();
	/** Disc Number's release and index, with Disc pointing at it, or false (the export unavailable or failed) without them. */
	bool ReadDisc(int32 Number, const Weltall::FIso9660*& Disc, Weltall::EDiscRelease& Release, Weltall::FGameFileIndex& Index);
	void TickBackground();
	void ResetBackgroundJobs();
	void Finish(const TCHAR* Done, const FString& Folder);

	std::map<Weltall::EDiscRelease, std::shared_ptr<Weltall::FDiscAssetStore>> Stores;
	std::vector<std::unique_ptr<Weltall::FDiscAssetExport>> Jobs;
	std::size_t Active = 0;
	/** The export that steps on a worker thread, a step at a time: music, textures, the text or Disc 2. */
	std::unique_ptr<Weltall::FDiscMusicWaveExport> MusicJob;
	TUniquePtr<FTextureExport> TextureJob;
	std::unique_ptr<Weltall::FGameTextExport> TextJob;
	std::unique_ptr<Weltall::FDisc2Export> Disc2Job;
	std::unique_ptr<Weltall::FShareableExpansion> SharedJob;
	/** The expansion's folder Make shareable copies. */
	std::filesystem::path Shared;
	TFuture<Weltall::TResult<bool>> BackgroundStep;
	/** The music recordings every disc's store reads, found when external assets were last switched on. */
	std::shared_ptr<const Weltall::FMusicReplacements> Music;
	bool bMusicFound = false;
	std::shared_ptr<const Weltall::FMessageOverrides> Messages;
	std::shared_ptr<const Weltall::FMesFontSheet> FontSheet;
	Weltall::FMenuAssetProgress Current;
	FDelegateHandle SettingsChanged;
	bool bRegistrationFailed = false;
};
