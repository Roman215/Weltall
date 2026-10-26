#pragma once

#include "CoreMinimal.h"

class SBorder;
class STextBlock;
class SWidget;

namespace Weltall
{
	struct FError;
}

/** A message the game shows over its viewport: the whole of it, and its text, which can change. */
struct FGameNotice
{
	TSharedRef<SBorder> Widget;
	TSharedRef<STextBlock> Text;
	/** With buttons, the text under them, which can change too. */
	TSharedPtr<STextBlock> Below;
	/** The buttons, side by side; the first takes the focus. */
	TArray<TSharedRef<SWidget>> Buttons;
};

/** A button under a notice's text: its words, and what pressing it does. */
struct FNoticeButton
{
	FText Label;
	TFunction<void()> Pressed;
};

/**
 * White text on black in place of the game's picture, or over the picture darkened (bOverPicture); with buttons, the
 * buttons side by side under the text and more text under them.
 */
WELTALL_API FGameNotice MakeGameNotice(const FText& Text, bool bOverPicture, TArray<FNoticeButton> Buttons = {});

/**
 * What the player is told when the game stops, or cannot start (bStarted false): what happened, why, where the log is
 * (or, where the log is out of the player's reach, the button that saves a copy of it: bLogButton), and the game's
 * version.
 */
WELTALL_API FText GameStoppedText(const Weltall::FError& Error, bool bStarted, bool bLogButton = false);
