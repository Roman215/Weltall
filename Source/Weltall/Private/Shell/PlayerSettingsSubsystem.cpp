#include "Shell/PlayerSettingsSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Shell/PlayerFolders.h"
#include "WeltallLog.h"

#include <filesystem>
#include <system_error>

namespace
{
	/**
	 * What changes without being written at once (the framing and the battle speed the game keeps, the options screen's
	 * changes while it is open) is written once it has rested this long.
	 */
	constexpr float UnsavedRestSeconds = 2.0f;
}

FString UPlayerSettingsSubsystem::SettingsFile()
{
	return FPaths::Combine(PlayerSettingsFolder(), TEXT("Settings.txt"));
}

void UPlayerSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Text;
	if (FFileHelper::LoadFileToString(Text, *SettingsFile()))
	{
		Current = Weltall::ReadPlayerSettings(TCHAR_TO_UTF8(*Text));
	}
	else
	{
		// The first start writes the defaults, so the file lists every setting there is.
		Save();
	}
	// A phone's system may end the app at any moment once it is in the background, with no word to it.
	BackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(this, &UPlayerSettingsSubsystem::SaveUnsaved);
}

void UPlayerSettingsSubsystem::Deinitialize()
{
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(BackgroundHandle);
	SaveUnsaved();
	Super::Deinitialize();
}

void UPlayerSettingsSubsystem::Change(const Weltall::FPlayerSettings& Settings)
{
	Current = Settings;
	Save();
	OnSettingsChanged.Broadcast();
}

void UPlayerSettingsSubsystem::Follow(const Weltall::FPlayerSettings& Settings)
{
	Current = Settings;
	UnsavedFor = 0.0f;
	OnSettingsChanged.Broadcast();
}

void UPlayerSettingsSubsystem::KeepCameraOffsets(const Weltall::FFieldCameraOffsets& Field, const Weltall::FWorldMapCameraOffsets& WorldMap)
{
	if (Field != Current.Camera || WorldMap != Current.WorldMapCamera)
	{
		Current.Camera = Field;
		Current.WorldMapCamera = WorldMap;
		UnsavedFor = 0.0f;
	}
}

void UPlayerSettingsSubsystem::KeepBattleSpeed(Weltall::EBattleSpeed Speed)
{
	if (Speed != Current.BattleSpeed)
	{
		Current.BattleSpeed = Speed;
		UnsavedFor = 0.0f;
	}
}

void UPlayerSettingsSubsystem::SaveUnsaved()
{
	if (UnsavedFor.IsSet())
	{
		Save();
	}
}

void UPlayerSettingsSubsystem::Save()
{
	UnsavedFor.Reset();
	const FString Target = SettingsFile();
	const FString Written = Target + TEXT(".new");
	const std::string Text = Weltall::WritePlayerSettings(Current);
	IFileManager::Get().MakeDirectory(*PlayerSettingsFolder(), true);
	if (!FFileHelper::SaveStringToFile(FString(UTF8_TO_TCHAR(Text.c_str())), *Written, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogWeltall, Error, TEXT("The settings cannot be written to %s"), *Written);
		return;
	}
	// Renaming over the old file replaces it in one step on every platform.
	std::error_code Error;
	std::filesystem::rename(std::filesystem::path(*Written), std::filesystem::path(*Target), Error);
	if (Error)
	{
		UE_LOG(LogWeltall, Error, TEXT("The settings cannot be put in place at %s: %hs"), *Target, Error.message().c_str());
	}
}

void UPlayerSettingsSubsystem::Tick(float DeltaTime)
{
	*UnsavedFor += DeltaTime;
	if (*UnsavedFor >= UnsavedRestSeconds)
	{
		Save();
	}
}

ETickableTickType UPlayerSettingsSubsystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UPlayerSettingsSubsystem::IsTickable() const
{
	return UnsavedFor.IsSet();
}

UWorld* UPlayerSettingsSubsystem::GetTickableGameObjectWorld() const
{
	return GetGameInstance()->GetWorld();
}

TStatId UPlayerSettingsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPlayerSettingsSubsystem, STATGROUP_Tickables);
}
