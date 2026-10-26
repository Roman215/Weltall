#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"

#include "WeltallViewportClient.generated.h"

/**
 * The game's window: it draws the current screen with the port's own renderer instead of a world,
 * every display frame, black around the picture, the touch screen's pad over it while it is in use.
 */
UCLASS()
class UWeltallViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	virtual void Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice = true) override;
	virtual void Draw(FViewport* InViewport, FCanvas* SceneCanvas) override;
	/** A finger on the window goes to the touch screen's pad. */
	virtual bool InputTouch(FViewport* const InViewport, const FTouchId TouchId, const ETouchType::Type Type, const FVector2D& TouchLocation, const float Force,
		const uint64 Timestamp) override;
};
