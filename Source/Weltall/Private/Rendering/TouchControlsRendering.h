#pragma once

#include "CoreMinimal.h"
#include "RenderGraphFwd.h"

/** What the touch pad draws in a display frame: each control in the target's pixels, and what of it is held. */
struct FTouchControlsDrawn
{
	struct FControl
	{
		FVector2f Centre = FVector2f::ZeroVector;
		float Radius = 0.0f;
		/** 0 the cross; 1-4 Triangle, Circle, Cross, Square; 5-8 L1, L2, R1, R2; 9 Select; 10 Start; 11 the stick. */
		uint32 Kind = 0;
		/** The cross's directions held (1 up, 2 right, 4 down, 8 left), or 1 for a held button or stick. */
		uint32 Held = 0;
		/** Where the stick's knob is pushed, in its radii from its centre. */
		FVector2f Knob = FVector2f::ZeroVector;
	};

	/** How far the stick's knob reaches past its rim, in its radii: the knob's own radius. */
	static constexpr float KnobRadius = 0.45f;

	TArray<FControl> Controls;
	float Opacity = 1.0f;
};

/** Draws the touch pad's controls over Output, each in its own square. */
void AddTouchControlsPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FTouchControlsDrawn& Drawn);
