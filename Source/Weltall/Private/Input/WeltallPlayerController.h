#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Field/FieldPlayerCamera.h"
#include "Input/ControlBindings.h"
#include "Input/PadState.h"

#include "WeltallPlayerController.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UPadActions;

/**
 * The player: whatever keyboard, mouse or gamepads they use, plugged in before or during play, the
 * game sees a PlayStation controller, pressed through the keys the player's bindings name.
 */
UCLASS()
class AWeltallPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** The controller buttons the player holds now. */
	Weltall::FPadState HeldPad() const;

	/** What the player did to the field's view in the last DeltaSeconds. */
	Weltall::FFieldViewInput ReadView(float DeltaSeconds);

	/** The port's own keys (the save states' and the message log's) pressed since the last call, each press once. */
	TArray<Weltall::EControlAction, TFixedAllocator<Weltall::PortActionCount>> ReadPortKeys();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** The gamepads' motors are the game's own, each port's on its controller, in place of the engine's effects. */
	virtual void UpdateForceFeedback(IInputInterface* InputInterface, const int32 ControllerId) override;

private:
	/** The player's settings changed: the keys follow their bindings. */
	void FollowBindings();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem() const;

	UPROPERTY()
	TObjectPtr<UPadActions> Pad;

	FDelegateHandle SettingsChanged;
};
