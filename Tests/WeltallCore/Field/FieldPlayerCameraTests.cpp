#include "Field/EventContext.h"
#include "Field/FieldCamera.h"
#include "Field/FieldCameraUpdate.h"
#include "Field/FieldPlayerCamera.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace Weltall;

namespace
{
	constexpr int32 Unit = 0x10000;
	constexpr double Pi = 3.14159265358979323846;

	/** A follow camera at rest on a target: its eye where the follow puts it, its view built from the two. */
	FEventContext FollowingAt(int16 Yaw)
	{
		FEventContext Context;
		Context.SettleTicks = 0;
		FFieldCamera& Camera = Context.Camera;
		Camera.Rotation[1] = Yaw;
		Camera.YawGoal = Yaw;
		Camera.Target = {300 * Unit, -200 * Unit, -500 * Unit};
		Camera.Eye = FollowEye(Camera, Camera.Target);
		Camera.EyeGoal = Camera.Eye;
		Camera.TargetGoal = Camera.Target;
		Camera.LookAt = FieldLookAt(Camera.Eye, Camera.Target);
		Camera.View = BuildFieldView(Camera, Camera.LookAt);
		return Context;
	}

	std::array<double, 3> Offset(const FFieldPoint& Eye, const FFieldPoint& Target)
	{
		return {static_cast<double>(Eye[0] - Target[0]) / Unit, static_cast<double>(Eye[1] - Target[1]) / Unit, static_cast<double>(Eye[2] - Target[2]) / Unit};
	}

	double Length(const std::array<double, 3>& Vector)
	{
		return std::sqrt(Vector[0] * Vector[0] + Vector[1] * Vector[1] + Vector[2] * Vector[2]);
	}

	/** Degrees above the target. */
	double Elevation(const std::array<double, 3>& Vector)
	{
		return std::atan2(-Vector[1], std::hypot(Vector[0], Vector[2])) * 180.0 / Pi;
	}

	/** The view's direction across the ground as the camera's tick measures it (4096 a turn). */
	double ViewYaw(const FFieldPoint& Eye, const FFieldPoint& Target)
	{
		return std::atan2(static_cast<double>(Target[2] - Eye[2]), static_cast<double>(Target[0] - Eye[0])) * 2048.0 / Pi;
	}

	bool SameMatrix(const FGteMatrix& A, const FGteMatrix& B)
	{
		return A.M == B.M && A.T == B.T;
	}

	bool SameAffine(const FAffine& A, const FAffine& B)
	{
		return A.M == B.M && A.T == B.T;
	}

	/** Where a view puts a world point across the original's screen (pixels from its middle, H 512). */
	double ScreenX(const FAffine& View, const std::array<double, 3>& Point)
	{
		std::array<double, 3> InView{};
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			InView[Row] = View.T[Row] + View.M[Row][0] * Point[0] + View.M[Row][1] * Point[1] + View.M[Row][2] * Point[2];
		}
		return 512.0 * InView[0] / InView[2];
	}

	void TickTimes(FFieldPlayerCamera& Player, const FEventContext& Context, int32 Ticks)
	{
		for (int32 Tick = 0; Tick < Ticks; ++Tick)
		{
			Player.Present(Context, true, false);
		}
	}
}

TEST_CASE("The player's camera shows the game's view itself until the player moves it", "[Field]")
{
	FEventContext Context = FollowingAt(0x800);
	Context.Camera.Shake = {3 * Unit, 0, -2 * Unit};
	FFieldCameraOffsets Offsets;
	FFieldPlayerCamera Player(Offsets);
	const FFieldPresentedView& Shown = Player.Present(Context, true, true);
	CHECK(Shown.bGameView);
	CHECK(SameMatrix(Shown.View.View, Context.Camera.View.View));
	CHECK(SameMatrix(Shown.View.Scene, Context.Camera.View.Scene));
	CHECK(SameMatrix(Shown.View.Billboard, Context.Camera.View.Billboard));
	CHECK(Shown.Eye == std::array<int32, 3>{Context.Camera.Eye[0] >> 16, Context.Camera.Eye[1] >> 16, Context.Camera.Eye[2] >> 16});
	CHECK(Shown.FarBucketShift == 0);
	CHECK(Player.ViewTurn() == 0);
}

TEST_CASE("The player's turn moves the eye as R1 turns the camera, and the view's yaws the other way", "[Field]")
{
	const FEventContext Before = FollowingAt(0x800);
	const FEventContext Turned = FollowingAt(0xA00);
	const FFieldCamera& Camera = Before.Camera;
	const FFieldPoint Eye = OffsetFieldEye(Camera.Eye, Camera.Target, 1.0f, 0.0f, 45.0f);
	for (std::size_t Axis = 0; Axis < 3; ++Axis)
	{
		CHECK(std::abs(Eye[Axis] - Turned.Camera.Eye[Axis]) < 2 * Unit);
	}

	FFieldCameraOffsets Offsets;
	Offsets.Turn = 45.0f;
	FFieldPlayerCamera Player(Offsets);
	const FFieldPresentedView& Shown = Player.Present(Before, true, true);
	CHECK_FALSE(Shown.bGameView);
	CHECK(Player.ViewTurn() == -0x200);
	const double Change = ViewYaw(OffsetFieldEye(Camera.Eye, Camera.Target, 1.0f, 0.0f, 45.0f), Camera.Target) - ViewYaw(Camera.Eye, Camera.Target);
	CHECK(std::abs(std::remainder(Change - Player.ViewTurn(), 4096.0)) < 2.0);
	FFieldViewInput Input;
	Input.Turn = -45.0f;
	Player.AddInput(Input);
	CHECK(Player.ViewTurn() == 0);
	Player.Present(Before, true, false);
	CHECK(Player.ViewTurn() == 0);
}

TEST_CASE("The player's zoom and tilt pull the eye back and raise it, short of looking straight down", "[Field]")
{
	const FFieldCamera Camera = FollowingAt(0x600).Camera;
	const std::array<double, 3> Game = Offset(Camera.Eye, Camera.Target);
	const std::array<double, 3> Zoomed = Offset(OffsetFieldEye(Camera.Eye, Camera.Target, 2.0f, 0.0f, 0.0f), Camera.Target);
	CHECK(Length(Zoomed) == Catch::Approx(2.0 * Length(Game)).epsilon(0.001));
	CHECK(Elevation(Zoomed) == Catch::Approx(Elevation(Game)).margin(0.01));
	CHECK(Elevation(Game) == Catch::Approx(30.0).margin(0.1));

	const std::array<double, 3> Raised = Offset(OffsetFieldEye(Camera.Eye, Camera.Target, 1.0f, 20.0f, 0.0f), Camera.Target);
	CHECK(Elevation(Raised) == Catch::Approx(50.0).margin(0.1));
	CHECK(Length(Raised) == Catch::Approx(Length(Game)).epsilon(0.001));
	const std::array<double, 3> Lowered = Offset(OffsetFieldEye(Camera.Eye, Camera.Target, 1.0f, -10.0f, 0.0f), Camera.Target);
	CHECK(Elevation(Lowered) == Catch::Approx(20.0).margin(0.1));

	FFieldCamera Steep = Camera;
	Steep.Pitch = 80;
	Steep.Eye = FollowEye(Steep, Steep.Target);
	CHECK(Elevation(Offset(OffsetFieldEye(Steep.Eye, Steep.Target, 1.0f, 20.0f, 0.0f), Steep.Target)) == Catch::Approx(85.0).margin(0.1));
}

TEST_CASE("The far bucket moves back as far as the player pulls the eye back", "[Field]")
{
	const FEventContext Context = FollowingAt(0x800);
	FFieldCameraOffsets Offsets;
	Offsets.Zoom = 2.0f;
	FFieldPlayerCamera Player(Offsets);
	const double Distance = Length(Offset(Context.Camera.Eye, Context.Camera.Target));
	CHECK(Player.Present(Context, true, true).FarBucketShift == static_cast<int32>(std::lround(Distance * 3.0 / 16.0)));
}

TEST_CASE("A script's paths are its shot, and so are its moving ramps, shakes and turns while the player is locked out", "[Field]")
{
	constexpr bool InControl = true;
	constexpr bool LockedOut = false;
	FFieldCamera Camera;
	CHECK_FALSE(IsFieldCameraScripted(Camera, LockedOut));
	Camera.Mode = 1;
	CHECK(IsFieldCameraScripted(Camera, InControl));
	Camera.Mode = 2;
	CHECK_FALSE(IsFieldCameraScripted(Camera, LockedOut));
	Camera.Mode = 0;

	Camera.Flags = FieldCameraFlags::ZoomRamp;
	Camera.ZoomRamp.Ticks = 1;
	Camera.ZoomRamp.Step = 0x10000;
	CHECK_FALSE(IsFieldCameraScripted(Camera, LockedOut));
	Camera.ZoomRamp.Ticks = 20;
	CHECK(IsFieldCameraScripted(Camera, LockedOut));
	CHECK_FALSE(IsFieldCameraScripted(Camera, InControl));
	Camera.ZoomRamp.Step = 0;
	CHECK_FALSE(IsFieldCameraScripted(Camera, LockedOut));
	Camera.Flags = FieldCameraFlags::PitchRamp;
	Camera.PitchRamp.Ticks = 5;
	Camera.PitchRamp.Step = -0x8000;
	CHECK(IsFieldCameraScripted(Camera, LockedOut));
	Camera.Flags = FieldCameraFlags::ProjectionRamp;
	Camera.ProjectionRamp.Ticks = 5;
	Camera.ProjectionRamp.Step = 0x40000;
	CHECK(IsFieldCameraScripted(Camera, LockedOut));
	Camera.Flags = 0;

	Camera.bShaking = true;
	CHECK(IsFieldCameraScripted(Camera, LockedOut));
	CHECK_FALSE(IsFieldCameraScripted(Camera, InControl));
	Camera.bShaking = false;

	Camera.TurnTicks = 7;
	CHECK(IsFieldCameraScripted(Camera, LockedOut));
	CHECK_FALSE(IsFieldCameraScripted(Camera, InControl));
	Camera.bTurnByButtons = true;
	CHECK_FALSE(IsFieldCameraScripted(Camera, LockedOut));
}

TEST_CASE("The player's turns and the camera's own start their turns as the buttons or the masks decide", "[Field]")
{
	FFieldCamera Camera;
	UpdateFieldCameraYaw(Camera, FieldCameraButtons::TurnRight);
	CHECK(Camera.TurnTicks == 7);
	CHECK(Camera.bTurnByButtons);
	for (int32 Tick = 0; Tick < 7; ++Tick)
	{
		UpdateFieldCameraYaw(Camera, 0);
	}
	CHECK(Camera.TurnTicks == 0);
	// A direction the scripts forbid turns the camera out of it by itself.
	Camera.ForbiddenDirections = FieldCameraDirectionBit((Camera.YawGoal & 0xFFF) >> 9);
	UpdateFieldCameraYaw(Camera, 0);
	CHECK(Camera.TurnTicks == 7);
	CHECK_FALSE(Camera.bTurnByButtons);
}

TEST_CASE("The view turns only within the directions the map allows", "[Field]")
{
	FFieldCamera Camera;
	FFieldTurnRange Range = FieldViewTurnRange(Camera);
	CHECK(Range.bAllRound);

	Camera.Flags = FieldCameraFlags::NoButtonTurn;
	Range = FieldViewTurnRange(Camera);
	CHECK_FALSE(Range.bAllRound);
	CHECK(Range.Down == 0.0f);
	CHECK(Range.Up == 0.0f);
	Camera.Flags = 0;

	Camera.LeftDirections = 0xFF;
	CHECK(FieldViewTurnRange(Camera).Up == 0.0f);
	Camera.LeftDirections = 0;

	// Direction 4 (yaw 0x800): 6 and 1 forbidden leave 5 above and 3 and 2 below.
	Camera.YawGoal = 0x800;
	Camera.ForbiddenDirections = static_cast<uint8>(FieldCameraDirectionBit(6) | FieldCameraDirectionBit(1));
	Range = FieldViewTurnRange(Camera);
	CHECK_FALSE(Range.bAllRound);
	CHECK(Range.Down == -90.0f);
	CHECK(Range.Up == 45.0f);

	// One forbidden direction leaves the view at most half a turn either way.
	Camera.ForbiddenDirections = FieldCameraDirectionBit(3);
	Range = FieldViewTurnRange(Camera);
	CHECK(Range.Down == 0.0f);
	CHECK(Range.Up == 180.0f);
}

TEST_CASE("The player's input moves the offsets within their ranges, and not while a script has the camera", "[Field]")
{
	FEventContext Context = FollowingAt(0x800);
	FFieldCameraOffsets Offsets;
	FFieldPlayerCamera Player(Offsets);
	Player.Present(Context, true, true);

	Player.AddInput({30.0f, 12.0f, -1.0f, false});
	Player.AddInput({10.0f, 12.0f, -0.5f, false});
	Player.Present(Context, false, false);
	CHECK(Offsets.Turn == 40.0f);
	CHECK(Offsets.Tilt == FFieldCameraOffsets::MaxTilt);
	CHECK(Offsets.Zoom == FFieldCameraOffsets::MaxZoom);
	CHECK(Player.AppliedTurn() == 40.0f);

	Player.AddInput({150.0f, 0.0f, 0.0f, false});
	Player.Present(Context, false, false);
	CHECK(Offsets.Turn == -170.0f);

	Context.Camera.Mode = 1;
	Player.AddInput({5.0f, -3.0f, 1.0f, false});
	Player.Present(Context, true, false);
	CHECK(Offsets.Turn == -170.0f);
	CHECK(Offsets.Tilt == FFieldCameraOffsets::MaxTilt);

	Player.AddInput({0.0f, 0.0f, 0.0f, true});
	Player.Present(Context, true, false);
	// The reset is of the zoom and turn: the tilt, a setting, and how the view follows and yields stay as set.
	FFieldCameraOffsets Reset = FFieldCameraOffsets::Defaults();
	Reset.bSteadyFollow = false;
	Reset.Tilt = FFieldCameraOffsets::MaxTilt;
	CHECK(Offsets == Reset);
}

TEST_CASE("A script's shot eases the offsets out over 16 ticks, and they ease back when control returns", "[Field]")
{
	FEventContext Context = FollowingAt(0x800);
	FFieldCameraOffsets Offsets{1.5f, 10.0f, 20.0f};
	FFieldPlayerCamera Player(Offsets);
	CHECK_FALSE(Player.Present(Context, true, true).bGameView);
	CHECK(Player.Weight() == 1.0f);

	Context.Camera.Mode = 1;
	TickTimes(Player, Context, 8);
	CHECK(Player.Weight() == Catch::Approx(0.5f));
	// A frame without a tick moves nothing.
	Player.Present(Context, false, false);
	CHECK(Player.Weight() == Catch::Approx(0.5f));
	TickTimes(Player, Context, 8);
	CHECK(Player.Weight() == 0.0f);
	CHECK(Player.Present(Context, true, false).bGameView);
	CHECK(Player.ViewTurn() == 0);

	Context.Camera.Mode = 2;
	TickTimes(Player, Context, 16);
	CHECK(Player.Weight() == 1.0f);

	// A new map takes the offsets at once.
	Context.Camera.Mode = 1;
	Player.Present(Context, true, true);
	CHECK(Player.Weight() == 0.0f);
}

TEST_CASE("A scene that locks the player out for more than a moment eases the offsets out, unless they are set not to yield", "[Field]")
{
	FEventContext Context = FollowingAt(0x800);
	FFieldCameraOffsets Offsets{1.5f, 10.0f, 20.0f};
	FFieldPlayerCamera Player(Offsets);
	Player.Present(Context, true, true);

	// A door or a short word: locked out for a moment, the framing stays.
	Context.PlayerLock = 1;
	TickTimes(Player, Context, FFieldPlayerCamera::LockedMomentTicks);
	CHECK(Player.Weight() == 1.0f);
	Context.PlayerLock = 0;
	TickTimes(Player, Context, 1);
	Context.PlayerLock = 1;
	TickTimes(Player, Context, FFieldPlayerCamera::LockedMomentTicks);
	CHECK(Player.Weight() == 1.0f);

	// Longer: the game's own framing over the easing's ticks, the player's input left alone, and back with control.
	TickTimes(Player, Context, 1 + FFieldPlayerCamera::EaseTicks);
	CHECK(Player.Weight() == 0.0f);
	CHECK(Player.Present(Context, true, false).bGameView);
	FFieldViewInput Input;
	Input.Zoom = 1.0f;
	Player.AddInput(Input);
	Player.Present(Context, true, false);
	CHECK(Offsets.Zoom == 1.5f);
	Context.PlayerLock = 0;
	TickTimes(Player, Context, FFieldPlayerCamera::EaseTicks);
	CHECK(Player.Weight() == 1.0f);

	// Set not to yield, only a script's own camera moves eases them out.
	Offsets.bYieldsToScenes = false;
	Context.PlayerLock = 1;
	TickTimes(Player, Context, 4 * FFieldPlayerCamera::LockedMomentTicks);
	CHECK(Player.Weight() == 1.0f);
}

TEST_CASE("A turn the map stops allowing goes back at the camera's own pace", "[Field]")
{
	FEventContext Context = FollowingAt(0x800);
	FFieldCameraOffsets Offsets;
	Offsets.Turn = 90.0f;
	FFieldPlayerCamera Player(Offsets);
	Player.Present(Context, true, true);
	CHECK(Player.AppliedTurn() == 90.0f);

	Context.Camera.Flags = FieldCameraFlags::NoButtonTurn;
	TickTimes(Player, Context, 8);
	CHECK(Player.AppliedTurn() == Catch::Approx(45.0f));
	TickTimes(Player, Context, 8);
	CHECK(Player.AppliedTurn() == 0.0f);
	// The player's preference stays for where turning is allowed again.
	CHECK(Offsets.Turn == 90.0f);
	Context.Camera.Flags = 0;
	TickTimes(Player, Context, 16);
	CHECK(Player.AppliedTurn() == 90.0f);
}

TEST_CASE("Once the map has settled the player's view is drawn from the tick before, as the game's is", "[Field]")
{
	FEventContext Context = FollowingAt(0x800);
	FFieldCameraOffsets Offsets;
	Offsets.Zoom = 1.5f;
	FFieldPlayerCamera Player(Offsets);
	Player.Present(Context, true, true);
	const FFieldPoint EyeBefore = Context.Camera.Eye;
	const FFieldPoint TargetBefore = Context.Camera.Target;

	Context.Camera.Target[0] += 40 * Unit;
	Context.Camera.Eye[0] += 40 * Unit;
	const FFieldPresentedView& Settled = Player.Present(Context, true, false);
	FFieldCamera Moved = Context.Camera;
	Moved.Eye = OffsetFieldEye(Context.Camera.Eye, Context.Camera.Target, 1.5f, 0.0f, 0.0f);
	CHECK(SameMatrix(Settled.View.View, BuildFieldView(Moved, FieldLookAt(OffsetFieldEye(EyeBefore, TargetBefore, 1.5f, 0.0f, 0.0f), TargetBefore)).View));

	Context.SettleTicks = 2;
	const FFieldPresentedView& Settling = Player.Present(Context, true, false);
	CHECK(SameMatrix(Settling.View.View, BuildFieldView(Moved, FieldLookAt(Moved.Eye, Context.Camera.Target)).View));
}

TEST_CASE("The view turns about the followed actor, which keeps its place on the screen", "[Field]")
{
	FEventContext Context = FollowingAt(0x800);
	// The actor stands away from the target, as where the camera's area holds the target back.
	FEventActorState& Actor = Context.States.emplace_back();
	Actor.Position = {Context.Camera.Target[0], Context.Camera.Target[1] + 32 * Unit, Context.Camera.Target[2] + 300 * Unit};
	Context.Camera.FollowedActor = 0;
	FFieldCameraOffsets Offsets;
	Offsets.Turn = 60.0f;
	FFieldPlayerCamera Player(Offsets);
	const FFieldPresentedView& Shown = Player.Present(Context, true, true);
	const FFieldPoint Pivot = FollowTarget(Actor.Position);
	const FGteVector Point = {static_cast<int16>(Pivot[0] >> 16), static_cast<int16>(Pivot[1] >> 16), static_cast<int16>(Pivot[2] >> 16)};
	const FGteMatrix& Game = Context.Camera.View.View;
	const FGteMatrix& Turned = Shown.View.View;
	const FGteLongVector InGame = ApplyMatrix(Game, Point);
	const FGteLongVector InTurned = ApplyMatrix(Turned, Point);
	for (std::size_t Axis = 0; Axis < 3; ++Axis)
	{
		CHECK(std::abs((InTurned[Axis] + Turned.T[Axis]) - (InGame[Axis] + Game.T[Axis])) < 16);
	}
	// Turned about the target instead, the actor would swing across the screen.
	FFieldCamera AboutTarget = Context.Camera;
	AboutTarget.Eye = OffsetFieldEye(Context.Camera.Eye, Context.Camera.Target, 1.0f, 0.0f, 60.0f);
	const FGteMatrix Swung = BuildFieldView(AboutTarget).View;
	CHECK(std::abs((ApplyMatrix(Swung, Point)[0] + Swung.T[0]) - (InGame[0] + Game.T[0])) > 300);
}

TEST_CASE("A display between frames shows the offsets with the input so far, as the next frame will", "[Field]")
{
	FEventContext Context = FollowingAt(0x400);
	FFieldCameraOffsets Offsets = FFieldCameraOffsets::Defaults();
	FFieldPlayerCamera Player(Offsets);
	const FFieldPresentedView& First = Player.Present(Context, true, true);
	CHECK(Player.LiveOffsets() == First.Offsets);
	CHECK(SameAffine(Player.UnroundedViewWithOffsets(First.Offsets), First.Unrounded));
	FFieldViewInput Input;
	Input.Turn = 12.5f;
	Input.Tilt = 4.0f;
	Input.Zoom = 0.25f;
	Player.AddInput(Input);
	const FFieldViewOffsets Live = Player.LiveOffsets();
	const FAffine LiveView = Player.UnroundedViewWithOffsets(Live);
	CHECK_FALSE(Live == Player.ShownView().Offsets);
	const FFieldPresentedView& Next = Player.Present(Context, false, false);
	CHECK(Next.Offsets == Live);
	CHECK(SameAffine(LiveView, Next.Unrounded));
	CHECK(Player.LiveOffsets() == Next.Offsets);

	// While a script's shot has the camera the input moves nothing: the live offsets are the frame's.
	Context.Camera.Mode = 1;
	Player.Present(Context, true, false);
	Player.AddInput(Input);
	CHECK(Player.LiveOffsets() == Player.ShownView().Offsets);
}

TEST_CASE("A slow turn or zoom close up moves the view between frames in even steps, where the game's look-at would shake it", "[Field]")
{
	const FEventContext Context = FollowingAt(0x800);
	const FFieldCamera& Camera = Context.Camera;
	const std::array<double, 3> Ground = {Camera.Target[0] / double{Unit} + 150.0, Camera.Target[1] / double{Unit} + 32.0, Camera.Target[2] / double{Unit}};
	const auto Steps = [](const std::vector<double>& Places)
	{
		std::vector<double> Made;
		for (std::size_t Index = 1; Index < Places.size(); ++Index)
		{
			Made.push_back(Places[Index] - Places[Index - 1]);
		}
		return Made;
	};
	// A twentieth of the stick's push at a display of four frames a tick, 120 a second.
	FFieldViewInput Turn;
	Turn.Turn = FFieldViewInput::TurnPerSecond * 0.05f / 120.0f;
	FFieldViewInput Zoom;
	Zoom.Zoom = FFieldViewInput::ZoomPerSecond * 0.05f / 120.0f;
	for (const FFieldViewInput& Push : {Turn, Zoom})
	{
		INFO((Push.Turn != 0.0f ? "turning" : "zooming"));
		FFieldCameraOffsets Offsets = FFieldCameraOffsets::Defaults();
		Offsets.Zoom = 0.6f;
		FFieldPlayerCamera Player(Offsets);
		Player.Present(Context, true, true);
		std::vector<double> Live;
		std::vector<double> Game;
		for (int32 Tick = 0; Tick < 30; ++Tick)
		{
			for (int32 Display = 0; Display < 4; ++Display)
			{
				Player.AddInput(Push);
				const FFieldViewOffsets Shows = Player.LiveOffsets();
				const FAffine View = Player.UnroundedViewWithOffsets(Shows);
				Live.push_back(ScreenX(View, Ground));
				// The same points' view as the game's look-at builds it: whole units and the library's table.
				const FFieldViewPoints Points = OffsetFieldView(Camera.Eye, Camera.Target, Camera.Target, Shows.Zoom, Shows.Tilt, Shows.Turn);
				CHECK(SameAffine(View, UnroundedFieldView(Camera, Points.Eye, Points.Target)));
				FFieldCamera Moved = Camera;
				Moved.Eye = Points.Eye;
				Moved.Target = Points.Target;
				Game.push_back(ScreenX(FAffine::From(BuildFieldView(Moved, FieldLookAt(Points.Eye, Points.Target)).View), Ground));
			}
			Player.Present(Context, true, false);
		}
		// Every step the same way, each within a hundredth of the one before (perspective speeds the point up slowly).
		const std::vector<double> LiveSteps = Steps(Live);
		const double Way = LiveSteps.front() < 0.0 ? -1.0 : 1.0;
		for (std::size_t Index = 1; Index < LiveSteps.size(); ++Index)
		{
			INFO("display frame " << Index);
			CHECK(LiveSteps[Index] * Way > 0.0);
			CHECK(std::abs(LiveSteps[Index] - LiveSteps[Index - 1]) < 0.01 * std::abs(LiveSteps[Index]));
		}
		// The game's look-at holds still and jumps, back and forth by a fraction of the original's pixel.
		const std::vector<double> GameSteps = Steps(Game);
		CHECK(std::ranges::count_if(GameSteps, [Way](double Step) { return Step * Way <= 0.0; }) > 30);
	}
}

TEST_CASE("The steady follow closes on the camera's goal across the ground, faster than the game's camera and all the way", "[Field]")
{
	FEventContext Context = FollowingAt(0x800);
	FFieldCameraOffsets Offsets;
	Offsets.bSteadyFollow = true;
	FFieldPlayerCamera Player(Offsets);
	// At rest on its goal the view is the game's own.
	CHECK(Player.Present(Context, true, true).bGameView);

	// The party has walked on, 60 units across and 30 up; the game's camera stays behind (as its own easing holds it).
	FFieldCamera& Camera = Context.Camera;
	const FFieldPoint Target = Camera.Target;
	Camera.TargetGoal = {Target[0] + 60 * Unit, Target[1] - 30 * Unit, Target[2] - 30 * Unit};
	const FFieldPresentedView& First = Player.Present(Context, true, false);
	CHECK_FALSE(First.bGameView);
	CHECK(First.Target == std::array<int32, 3>{300 + 20, -200, -500 - 10});
	// The eye moves with the target: the framing is the game's.
	CHECK(First.Eye == std::array<int32, 3>{(Camera.Eye[0] >> 16) + 20, Camera.Eye[1] >> 16, (Camera.Eye[2] >> 16) - 10});
	// A display frame between ticks moves nothing.
	CHECK(Player.Present(Context, false, false).Target == std::array<int32, 3>{320, -200, -510});
	TickTimes(Player, Context, 40);
	CHECK(Player.ShownView().Target == std::array<int32, 3>{360, -200, -530});

	// A script's shot eases it out with the offsets, to the game's own view.
	Camera.Mode = 1;
	TickTimes(Player, Context, FFieldPlayerCamera::EaseTicks);
	CHECK(Player.ShownView().bGameView);
	Camera.Mode = 0;
	TickTimes(Player, Context, FFieldPlayerCamera::EaseTicks);
	CHECK(Player.ShownView().Target == std::array<int32, 3>{360, -200, -530});

	// A new map takes the game's camera at once; switched off, the view is the game's.
	CHECK(Player.Present(Context, true, true).bGameView);
	TickTimes(Player, Context, 5);
	CHECK_FALSE(Player.ShownView().bGameView);
	Offsets.bSteadyFollow = false;
	CHECK(Player.Present(Context, true, false).bGameView);
}
