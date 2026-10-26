#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Game/GameTextExport.h"
#include "Media/DiscAssets.h"
#include "Media/DiscIdentification.h"
#include "Sound/DiscMusicWaveExport.h"
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
	/** The player's font sheet (Assets/font) and messages (Assets/text), read again each time external assets are switched on. */
	void FindMessages();
	/** The first disc's release and index, with Disc pointing at it, or false (failing the export) without them. */
	bool ReadFirstDisc(const Weltall::FIso9660*& Disc, Weltall::EDiscRelease& Release, Weltall::FGameFileIndex& Index);
	void TickBackground();
	void ResetBackgroundJobs();
	void Finish(const TCHAR* Done);

	std::map<Weltall::EDiscRelease, std::shared_ptr<Weltall::FDiscAssetStore>> Stores;
	std::vector<std::unique_ptr<Weltall::FDiscAssetExport>> Jobs;
	std::size_t Active = 0;
	/** The export that steps on a worker thread, a step at a time: music, textures or the text. */
	std::unique_ptr<Weltall::FDiscMusicWaveExport> MusicJob;
	TUniquePtr<FTextureExport> TextureJob;
	std::unique_ptr<Weltall::FGameTextExport> TextJob;
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
