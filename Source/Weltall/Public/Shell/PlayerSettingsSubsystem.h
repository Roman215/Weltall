#pragma once

#include "CoreMinimal.h"
#include "Game/PlayerSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"

#include "PlayerSettingsSubsystem.generated.h"

/**
 * The player's settings, kept as text in the platform's standard folder for a user's settings: read
 * as the game starts, written whole (a new file put in the old one's place, so a write cut short
 * leaves the old settings) whenever they change, and whatever is not written yet as the app goes
 * into the background and as the game ends.
 */
UCLASS()
class WELTALL_API UPlayerSettingsSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnSettingsChanged);

	/** The settings' file: Weltall's folder among the user's settings (Windows' local application data, XDG's config home elsewhere). */
	static FString SettingsFile();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	const Weltall::FPlayerSettings& Settings() const { return Current; }

	/** The player changed the settings: they are written at once and whatever follows them is told. */
	void Change(const Weltall::FPlayerSettings& Settings);

	/**
	 * The player is changing the settings (the options screen is open): whatever follows them is told, and they are
	 * written once they have rested a moment, or as the options screen closes (Change).
	 */
	void Follow(const Weltall::FPlayerSettings& Settings);

	/**
	 * The player's framing of the field and of the world map as the game has them now: kept, and written once the
	 * player has stopped moving the view for a moment. Nothing is told: the game is where they came from.
	 */
	void KeepCameraOffsets(const Weltall::FFieldCameraOffsets& Field, const Weltall::FWorldMapCameraOffsets& WorldMap);

	/** The battle speed as the player stepped it in a battle: kept, and written with the framing. Nothing is told. */
	void KeepBattleSpeed(Weltall::EBattleSpeed Speed);

	/** Told after every change the player makes to the settings. */
	FOnSettingsChanged OnSettingsChanged;

	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual TStatId GetStatId() const override;

private:
	void Save();
	/** Writes the settings when something has changed since they were last written. */
	void SaveUnsaved();

	Weltall::FPlayerSettings Current;
	/** Seconds since the settings last changed (the framing, the battle speed, the options screen's changes), while that is not written yet. */
	TOptional<float> UnsavedFor;
	FDelegateHandle BackgroundHandle;
};
