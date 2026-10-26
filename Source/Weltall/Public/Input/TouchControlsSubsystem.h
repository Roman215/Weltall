#pragma once

#include "CoreMinimal.h"
#include "Input/TouchPad.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "TouchControlsSubsystem.generated.h"

struct FTouchControlsDrawn;

/**
 * The PlayStation controller drawn on a touch screen (Weltall::FTouchPad), laid out for the game window's shape and the
 * player's choice of a cross or a stick: shown while touch is the player's device (UButtonGlyphSubsystem), its fingers'
 * buttons joining the controller's and the keyboard's through the player's bindings, drawn as strongly as the player
 * asked, a press of a control vibrating the device if the player wants it.
 */
UCLASS()
class WELTALL_API UTouchControlsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** A finger on the game's window, at Location in its pixels: the pad follows it, laid out for the window's size. */
	void Touch(const FTouchId& Finger, ETouchType::Type Type, const FVector2D& Location, const FIntPoint& WindowSize);

	/** Whether the pad is shown: while touch is the device the player uses. */
	bool IsShown() const;

	/** The game's buttons the fingers pressed since the last take (a tap shorter than a frame among them), while the pad is shown. */
	Weltall::FPadState TakeHeld();

	/** The pad drawn over a window of WindowSize (laid out for it first), while it is shown. */
	void Draw(const FIntPoint& WindowSize, FTouchControlsDrawn& Into);

private:
	/** The pad laid out for a window of Size and the player's movement control, its fingers lifted when either changed. */
	void FollowWindow(const FIntPoint& Size);

	const Weltall::FControlBindings& Controls() const;

	Weltall::FTouchPad Pad;
	FIntPoint LaidOutFor = FIntPoint::ZeroValue;
	Weltall::ETouchMovement LaidOutMovement = Weltall::ETouchMovement::Cross;
};
