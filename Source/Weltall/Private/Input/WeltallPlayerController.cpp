#include "Input/WeltallPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Input/PadActions.h"
#include "Input/PadPortsSubsystem.h"
#include "Shell/PlayerSettingsSubsystem.h"

Weltall::FPadState AWeltallPlayerController::HeldPad() const
{
	const UEnhancedPlayerInput* Input = Cast<UEnhancedPlayerInput>(PlayerInput);
	return Input != nullptr && Pad != nullptr ? Pad->ReadHeld(*Input) : Weltall::FPadState{};
}

Weltall::FFieldViewInput AWeltallPlayerController::ReadView(float DeltaSeconds, bool bCameraShoulders)
{
	const UEnhancedPlayerInput* Input = Cast<UEnhancedPlayerInput>(PlayerInput);
	return Input != nullptr && Pad != nullptr ? Pad->ReadView(*Input, DeltaSeconds, bCameraShoulders) : Weltall::FFieldViewInput{};
}

TArray<Weltall::EControlAction, TFixedAllocator<Weltall::PortActionCount>> AWeltallPlayerController::ReadPortKeys()
{
	const UEnhancedPlayerInput* Input = Cast<UEnhancedPlayerInput>(PlayerInput);
	return Input != nullptr && Pad != nullptr ? Pad->ReadPortKeys(*Input) : TArray<Weltall::EControlAction, TFixedAllocator<Weltall::PortActionCount>>();
}

void AWeltallPlayerController::BeginPlay()
{
	Super::BeginPlay();
	Pad = UPadActions::Create(this);
	UPlayerSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>();
	Pad->Map(Settings->Settings().Controls);
	if (UEnhancedInputLocalPlayerSubsystem* Input = InputSubsystem())
	{
		Input->AddMappingContext(Pad->Mapping(), 0);
	}
	SettingsChanged = Settings->OnSettingsChanged.AddUObject(this, &AWeltallPlayerController::FollowBindings);
	SetInputMode(FInputModeGameOnly());
}

void AWeltallPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPlayerSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>())
	{
		Settings->OnSettingsChanged.Remove(SettingsChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void AWeltallPlayerController::UpdateForceFeedback(IInputInterface* InputInterface, const int32 ControllerId)
{
	UPadPortsSubsystem* Ports = GetGameInstance()->GetSubsystem<UPadPortsSubsystem>();
	if (Ports != nullptr && InputInterface != nullptr)
	{
		Ports->SendMotors(*InputInterface);
	}
}

void AWeltallPlayerController::FollowBindings()
{
	Pad->Map(GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>()->Settings().Controls);
	if (UEnhancedInputLocalPlayerSubsystem* Input = InputSubsystem())
	{
		Input->RequestRebuildControlMappings();
	}
}

UEnhancedInputLocalPlayerSubsystem* AWeltallPlayerController::InputSubsystem() const
{
	const ULocalPlayer* Local = GetLocalPlayer();
	return Local != nullptr ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
}
