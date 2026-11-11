#pragma once

#include "CoreMinimal.h"
#include "Game/BattleSpeedMark.h"
#include "Screens/GameScreen.h"

class FBattleSpeedMarkFrame;
class FVramTexture;

/**
 * The fast-forward indicator, drawn in the top right corner.
 */
class FBattleSpeedMarkView
{
public:
	FBattleSpeedMarkView();
	~FBattleSpeedMarkView();

	TSharedRef<const IScreenFrame> Capture(Weltall::EBattleSpeed Speed, const Weltall::FMesFont& Font, TSharedRef<const IScreenFrame> Background) const;

private:
	mutable Weltall::FBattleSpeedMarkDrawing Drawing;
	TSharedRef<FVramTexture> Vram;
	/** Frames handed out; one the rendering thread no longer holds is filled again. */
	mutable TArray<TSharedRef<FBattleSpeedMarkFrame>> DisplayFrames;
};
