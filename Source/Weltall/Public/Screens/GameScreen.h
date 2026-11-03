#pragma once

#include "CoreMinimal.h"
#include "Field/FieldPlayerCamera.h"
#include "Foundation/Result.h"
#include "Input/ControlBindings.h"
#include "Input/PadState.h"
#include "Presentation/DisplayOptions.h"
#include "Presentation/GameScreen.h"
#include "RenderGraphFwd.h"
#include "Rendering/ScreenArea.h"

#include <array>

/**
 * One displayed frame of a screen, captured on the game thread and drawn on the rendering thread,
 * so the screen can go on changing while the frame is drawn.
 */
class IScreenFrame
{
public:
	virtual ~IScreenFrame() = default;

	/**
	 * Adds the passes that draw the frame's game screen into Area of Output, laid out as Area lays out
	 * screens; a 3D scene takes the screen's rows across the area's width. Area is already black.
	 */
	virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const = 0;

	/** The game screen whose rows the frame shows: a CRT's lines follow them, and the picture's area holds them. */
	virtual Weltall::FScreenSize ScreenSize() const { return {320, 224}; }
};

/**
 * Adds the passes that draw a display frame into Display of Output: black, then the frame (when there is
 * one) in the part of it the picture's shape takes, through the CRT filter when one is chosen.
 */
WELTALL_API void AddDisplayFramePasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FIntRect& Display, const Weltall::FDisplayOptions& Options,
	const IScreenFrame* Frame);

/** The game's two controller ports as the players' devices fill them, at a display frame. */
struct FGamePads
{
	/** Whether the player has a controller: the game's first pad is plugged in or pulled out. */
	bool bConnected = true;
	/** Whether the first pad's controller has a motor. */
	bool bMotor = false;
	/** Whether a second player's gamepad fills the second port; it has a motor. */
	bool bSecondConnected = false;
	/** The buttons held on it. */
	Weltall::FPadState SecondHeld;
};

/** A screen of a game: it runs at the game's own tick and is shown at the display's rate. */
class IGameScreen
{
public:
	virtual ~IGameScreen() = default;

	/** Lets one display frame's real time pass with the controller buttons the player holds. */
	virtual void Update(double DeltaSeconds, Weltall::FPadState Held) = 0;

	/** What the player did to the field's view in the display frame, before its Update; a screen without the field has none. */
	virtual void AddViewInput(const Weltall::FFieldViewInput& Input) {}

	/**
	 * One of the port's own keys pressed (Weltall::IsPortAction: the save states', the message log's), before the display frame's
	 * Update; a screen without the game ignores it.
	 */
	virtual void PressPortKey(Weltall::EControlAction Action) {}

	/** The controllers, before each Update. */
	virtual void SetPads(const FGamePads& Pads) {}

	/** Whether the screen holds its game still (the message log open over it): no time passes for the game, and it makes no sound. */
	virtual bool HoldsGame() const { return false; }

	/** Whether the game reads its second controller now: only then does a second player's gamepad fill the second port. */
	virtual bool ReadsSecondPad() const { return false; }

	/** Whether the game runs each port's motor now. */
	virtual std::array<bool, 2> Motors() const { return {}; }

	/** What the screen shows now. */
	virtual TSharedRef<const IScreenFrame> CaptureFrame() const = 0;
};
