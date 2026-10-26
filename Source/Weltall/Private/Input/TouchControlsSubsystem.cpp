#include "Input/TouchControlsSubsystem.h"

#include "Engine/GameInstance.h"
#include "HAL/PlatformMisc.h"
#include "Input/ButtonGlyphSubsystem.h"
#include "Rendering/TouchControlsRendering.h"
#include "Shell/PlayerSettingsSubsystem.h"

namespace
{
	using Weltall::EPadButton;
	using Weltall::FPadState;

	/** The shader's number for a control: the cross, the button's symbol or label, or the stick. */
	uint32 KindOf(const Weltall::FTouchControl& Control)
	{
		if (Control.Kind == Weltall::ETouchControlKind::DirectionPad)
		{
			return 0;
		}
		if (Control.Kind == Weltall::ETouchControlKind::Stick)
		{
			return 11;
		}
		switch (Control.Button)
		{
		case EPadButton::Triangle: return 1;
		case EPadButton::Circle: return 2;
		case EPadButton::Cross: return 3;
		case EPadButton::Square: return 4;
		case EPadButton::L1: return 5;
		case EPadButton::L2: return 6;
		case EPadButton::R1: return 7;
		case EPadButton::R2: return 8;
		case EPadButton::Select: return 9;
		default: return 10;
		}
	}

	/** The shader's bits for the cross's held directions. */
	uint32 DirectionsOf(uint16 Buttons)
	{
		return ((Buttons & FPadState::Bit(EPadButton::Up)) != 0 ? 1u : 0u) | ((Buttons & FPadState::Bit(EPadButton::Right)) != 0 ? 2u : 0u)
			| ((Buttons & FPadState::Bit(EPadButton::Down)) != 0 ? 4u : 0u) | ((Buttons & FPadState::Bit(EPadButton::Left)) != 0 ? 8u : 0u);
	}
}

void UTouchControlsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UButtonGlyphSubsystem>();
	Collection.InitializeDependency<UPlayerSettingsSubsystem>();
}

const Weltall::FControlBindings& UTouchControlsSubsystem::Controls() const
{
	return GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>()->Settings().Controls;
}

void UTouchControlsSubsystem::FollowWindow(const FIntPoint& Size)
{
	const Weltall::ETouchMovement Movement = Controls().TouchMovement;
	if ((Size != LaidOutFor || Movement != LaidOutMovement) && Size.X > 0 && Size.Y > 0)
	{
		Pad = Weltall::FTouchPad::Standard(static_cast<float>(Size.X) / static_cast<float>(Size.Y), Movement);
		LaidOutFor = Size;
		LaidOutMovement = Movement;
	}
}

void UTouchControlsSubsystem::Touch(const FTouchId& Finger, ETouchType::Type Type, const FVector2D& Location, const FIntPoint& WindowSize)
{
	FollowWindow(WindowSize);
	if (LaidOutFor.Y <= 0)
	{
		return;
	}
	const uint64 Id = static_cast<uint64>(static_cast<uint32>(Finger.GetDeviceId().GetId())) << 32 | static_cast<uint32>(Finger.GetIndex());
	const float X = static_cast<float>(Location.X) / static_cast<float>(LaidOutFor.Y);
	const float Y = static_cast<float>(Location.Y) / static_cast<float>(LaidOutFor.Y);
	switch (Type)
	{
	case ETouchType::Began:
		if (Pad.Press(Id, X, Y) && Controls().bTouchVibration)
		{
			FPlatformMisc::TriggerMobileHaptics();
		}
		break;
	case ETouchType::Ended:
		Pad.Release(Id);
		break;
	default:
		Pad.Move(Id, X, Y);
		break;
	}
}

bool UTouchControlsSubsystem::IsShown() const
{
	const UButtonGlyphSubsystem* Glyphs = GetGameInstance()->GetSubsystem<UButtonGlyphSubsystem>();
	return Glyphs != nullptr && Glyphs->IsTouchInUse();
}

Weltall::FPadState UTouchControlsSubsystem::TakeHeld()
{
	const Weltall::FPadState Held = Pad.Take(Controls());
	return IsShown() ? Held : Weltall::FPadState{};
}

void UTouchControlsSubsystem::Draw(const FIntPoint& WindowSize, FTouchControlsDrawn& Into)
{
	Into.Controls.Reset();
	if (!IsShown())
	{
		return;
	}
	FollowWindow(WindowSize);
	Into.Opacity = static_cast<float>(Controls().TouchOpacity) / static_cast<float>(Weltall::HighestTouchOpacity);
	const float Height = static_cast<float>(LaidOutFor.Y);
	const std::span<const Weltall::FTouchControl> Controls = Pad.Controls();
	for (std::size_t Index = 0; Index < Controls.size(); ++Index)
	{
		const Weltall::FTouchControl& Control = Controls[Index];
		const uint16 Held = Pad.HeldOn(Index);
		FTouchControlsDrawn::FControl& Drawn = Into.Controls.AddDefaulted_GetRef();
		Drawn.Centre = FVector2f(Control.X * Height, Control.Y * Height);
		Drawn.Radius = Control.Radius * Height;
		Drawn.Kind = KindOf(Control);
		Drawn.Held = Control.Kind != Weltall::ETouchControlKind::Button ? DirectionsOf(Held) : (Held != 0 ? 1u : 0u);
		if (Control.Kind == Weltall::ETouchControlKind::Stick)
		{
			const std::optional<std::array<float, 2>> Push = Pad.StickPush();
			Drawn.Knob = Push ? FVector2f((*Push)[0], (*Push)[1]) : FVector2f::ZeroVector;
			Drawn.Held = Push ? 1u : 0u;
		}
	}
}
