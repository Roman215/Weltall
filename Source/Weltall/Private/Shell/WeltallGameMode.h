#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/GameModeBase.h"
#include "Shell/GameNotice.h"

#include <string>

#include "WeltallGameMode.generated.h"

class SBorder;
class STextBlock;

namespace Weltall
{
	struct FError;
}

/**
 * The game draws its own screens, so the player has a controller but never a pawn in a world. Play
 * begins as the game does from disc 1, or from disc 2 when the player has only that, as the console
 * allows: the console powered on, its notice, the opening movie, then the programs as the game's loop
 * runs them. Without either disc the player is told where to put them, and the game starts once one is there; on
 * Android, given a button that adds them through the device's file picker, it starts once both are there, or with one
 * when the player chooses to. A game that stops, or cannot start, says why and where its log is (on Android, with a
 * button that saves a copy of it).
 */
UCLASS()
class AWeltallGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWeltallGameMode();

	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;

protected:
	virtual void BeginPlay() override;
	/** The game's screen goes before the sound it plays through. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Powers the game on from disc 1, or from disc 2 when it is the only one; false when the player's discs have neither. */
	bool StartGame();
	/** Tells the player the discs are missing, and looks for them every two seconds. */
	void AwaitDiscs();
	/**
	 * Looks in the Games folder again while the game waits for its discs; starts the game once one is there, or, where
	 * the discs are added through the device's picker, once both are, offering to go on with one meanwhile.
	 */
	void LookForDiscs();
	/** The player goes on with the one disc there. */
	void ContinueWithOneDisc();
	/** What the player is told while both discs are missing: where they go, what was found and what was not used. */
	FText DiscsMissing() const;
	/** The discs found and the files not used, as the screen without discs lists them; empty when there are none. */
	std::string DiscsFound() const;
	/**
	 * Where the discs come in through the device's file picker: under the screen's button, the copy as it goes and the
	 * discs found; under the stopped game's button, what became of the log's copy.
	 */
	FText DiscsAdded() const;
	FText LogSaved() const;
	/** Why the game stopped, or cannot start (bStarted false), over its last picture or in its place. */
	void ShowStopped(const Weltall::FError& Error, bool bStarted);
	/** Follows the transfer on the notice's button; a copy of discs that ended looks for them at once. */
	void FollowTransfer();
	/**
	 * A message over the viewport in place of the game's picture, or over it, darkened (bOverPicture), until hidden; with
	 * buttons, the text under them (Below) followed as the device's transfer goes on.
	 */
	void ShowNotice(const FText& Text, bool bOverPicture, TArray<FNoticeButton> Buttons = {}, TFunction<FText()> Below = {});
	/** The screen waiting for discs added through the picker: its buttons (files, a folder), and the one going on with one disc (bContinue). */
	void ShowDiscsPicked(bool bContinue);
	void HideNotice();

	/** The game follows the player's settings as they change. */
	FDelegateHandle SettingsChanged;
	FTimerHandle LookAgain;
	FTimerHandle FollowCopy;
	TSharedPtr<SBorder> Notice;
	TSharedPtr<STextBlock> NoticeText;
	TSharedPtr<STextBlock> NoticeBelow;
	TFunction<FText()> NoticeBelowText;
	uint64 TransferSeen = 0;
	/** The screen waiting for discs offers to go on with the one there. */
	bool bContinueOffered = false;
};
