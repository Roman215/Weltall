#include "Field/FieldPlayerCamera.h"

#include "Field/EventContext.h"
#include "Field/FieldCameraUpdate.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Weltall
{
	namespace
	{
		constexpr int16 ScriptMode = 1;
		constexpr uint8 AllDirections = 0xFF;
		constexpr int32 DirectionShift = 9;
		constexpr float DegreesPerDirection = 45.0f;
		constexpr float HalfTurn = 180.0f;
		constexpr double RadiansPerDegree = 3.14159265358979323846 / 180.0;
		constexpr double YawUnitsPerDegree = 4096.0 / 360.0;
		constexpr double FixedOne = 65536.0;
		constexpr float HighestElevation = 85.0f;
		/** A view unit is a third of a world unit, and an ordering-table entry sixteen view units deep. */
		constexpr double EntriesPerWorldUnit = 3.0 / 16.0;

		float Wrapped(float Degrees)
		{
			float Turn = std::remainder(Degrees, 2.0f * HalfTurn);
			return Turn <= -HalfTurn ? Turn + 2.0f * HalfTurn : Turn;
		}

		float Approach(float From, float To, float Step)
		{
			return From < To ? std::min(From + Step, To) : std::max(From - Step, To);
		}

		/** Weight eased in and out: smooth at both ends. */
		float Eased(float Weight)
		{
			return Weight * Weight * (3.0f - 2.0f * Weight);
		}

		int32 Sum(int32 A, int32 B)
		{
			return static_cast<int32>(static_cast<uint32>(A) + static_cast<uint32>(B));
		}

		FFieldPoint Shaken(const FFieldPoint& Point, const FFieldPoint& Shake)
		{
			return {Sum(Point[0], Shake[0]), Sum(Point[1], Shake[1]), Sum(Point[2], Shake[2])};
		}

		std::array<int32, 3> Whole(const FFieldPoint& Point)
		{
			return {Point[0] >> 16, Point[1] >> 16, Point[2] >> 16};
		}

		double Distance(const FFieldPoint& A, const FFieldPoint& B)
		{
			double Sum = 0.0;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				const double Along = (static_cast<double>(A[Axis]) - static_cast<double>(B[Axis])) / FixedOne;
				Sum += Along * Along;
			}
			return std::sqrt(Sum);
		}

		int32 ToFixed(double WholeUnits)
		{
			constexpr double Largest = static_cast<double>(std::numeric_limits<int32>::max());
			return static_cast<int32>(std::clamp(std::round(WholeUnits * FixedOne), -Largest, Largest));
		}

		/** What the view turns about: the followed actor where the camera follows one, else the target. */
		FFieldPoint ViewPivot(const FEventContext& Context)
		{
			const FFieldCamera& Camera = Context.Camera;
			const std::size_t Followed = Camera.FollowedActor;
			if (Camera.Mode != ScriptMode && Followed < Context.States.size())
			{
				return FollowTarget(Context.States[Followed].Position);
			}
			return Camera.Target;
		}

		int32 CountOpenSteps(uint8 Closed, int32 Direction, int32 Step)
		{
			int32 Count = 0;
			while (Count < 7 && (FieldCameraDirectionBit(Direction + Step * (Count + 1)) & Closed) == 0)
			{
				++Count;
			}
			return Count;
		}
	}

	bool IsFieldCameraScripted(const FFieldCamera& Camera, bool bPlayerInControl)
	{
		if (Camera.Mode == ScriptMode)
		{
			return true;
		}
		// A ramp of a single tick is a script setting the camera at once, and one of no step (a map
		// setting again the pitch it has) moves nothing.
		const auto Ramping = [&Camera](uint32 Flag, const FFieldCameraRamp& Ramp) { return (Camera.Flags & Flag) != 0 && Ramp.Ticks > 1 && Ramp.Step != 0; };
		return !bPlayerInControl
			&& (Ramping(FieldCameraFlags::ZoomRamp, Camera.ZoomRamp) || Ramping(FieldCameraFlags::PitchRamp, Camera.PitchRamp)
				|| Ramping(FieldCameraFlags::ProjectionRamp, Camera.ProjectionRamp) || Camera.bShaking || (Camera.TurnTicks != 0 && !Camera.bTurnByButtons));
	}

	FFieldTurnRange FieldViewTurnRange(const FFieldCamera& Camera)
	{
		if ((Camera.Flags & FieldCameraFlags::NoButtonTurn) != 0 || Camera.ForbiddenDirections == AllDirections || Camera.LeftDirections == AllDirections)
		{
			return {0.0f, 0.0f, false};
		}
		const uint8 Closed = Camera.ForbiddenDirections | Camera.LeftDirections;
		if (Closed == 0)
		{
			return {};
		}
		const int32 Direction = (Camera.YawGoal & 0xFFF) >> DirectionShift;
		if ((FieldCameraDirectionBit(Direction) & Closed) != 0)
		{
			return {0.0f, 0.0f, false};
		}
		const float Down = DegreesPerDirection * static_cast<float>(CountOpenSteps(Closed, Direction, -1));
		const float Up = DegreesPerDirection * static_cast<float>(CountOpenSteps(Closed, Direction, 1));
		return {-std::min(Down, HalfTurn), std::min(Up, HalfTurn), false};
	}

	FFieldPoint OffsetFieldEye(const FFieldPoint& Eye, const FFieldPoint& Target, float Zoom, float Tilt, float Turn)
	{
		std::array<double, 3> Offset{};
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Offset[Axis] = (static_cast<double>(Eye[Axis]) - static_cast<double>(Target[Axis])) / FixedOne;
		}
		const double Across = std::hypot(Offset[0], Offset[2]);
		const double Length = std::sqrt(Across * Across + Offset[1] * Offset[1]) * Zoom;
		std::array<double, 3> Moved{Offset[0] * Zoom, Offset[1] * Zoom, Offset[2] * Zoom};
		if (Across > 1.0 / FixedOne)
		{
			// Up is -y. The turn is the kernel's rotation about y, as the camera's own yaw turns the eye.
			const double Elevation = std::atan2(-Offset[1], Across) / RadiansPerDegree;
			const double Raised = std::min<double>(Elevation + Tilt, std::max<double>(Elevation, HighestElevation)) * RadiansPerDegree;
			const double Sin = std::sin(Turn * RadiansPerDegree);
			const double Cos = std::cos(Turn * RadiansPerDegree);
			const double X = Offset[0] / Across;
			const double Z = Offset[2] / Across;
			const double NewAcross = Length * std::cos(Raised);
			Moved = {(Cos * X + Sin * Z) * NewAcross, -Length * std::sin(Raised), (Cos * Z - Sin * X) * NewAcross};
		}
		FFieldPoint Result{};
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Result[Axis] = Sum(Target[Axis], ToFixed(Moved[Axis]));
		}
		return Result;
	}

	FFieldViewPoints OffsetFieldView(const FFieldPoint& Eye, const FFieldPoint& Target, const FFieldPoint& Pivot, float Zoom, float Tilt, float Turn)
	{
		FFieldViewPoints Turned{Eye, Target};
		if (Turn != 0.0f)
		{
			const double Sin = std::sin(Turn * RadiansPerDegree);
			const double Cos = std::cos(Turn * RadiansPerDegree);
			for (FFieldPoint* Point : {&Turned.Eye, &Turned.Target})
			{
				const double X = (static_cast<double>((*Point)[0]) - static_cast<double>(Pivot[0])) / FixedOne;
				const double Z = (static_cast<double>((*Point)[2]) - static_cast<double>(Pivot[2])) / FixedOne;
				(*Point)[0] = Sum(Pivot[0], ToFixed(Cos * X + Sin * Z));
				(*Point)[2] = Sum(Pivot[2], ToFixed(Cos * Z - Sin * X));
			}
		}
		Turned.Eye = OffsetFieldEye(Turned.Eye, Turned.Target, Zoom, Tilt, 0.0f);
		return Turned;
	}

	void FFieldPlayerCamera::ApplyInput(const FFieldCamera& Camera, bool bScripted)
	{
		const FFieldViewInput Input = Pending;
		Pending = {};
		if (Input.bReset)
		{
			const bool bYields = Offsets.bYieldsToScenes;
			const bool bSteady = Offsets.bSteadyFollow;
			const float Tilt = Offsets.Tilt;
			Offsets = FFieldCameraOffsets::Defaults();
			Offsets.bYieldsToScenes = bYields;
			Offsets.bSteadyFollow = bSteady;
			Offsets.Tilt = Tilt;
			return;
		}
		// While a script's shot has the camera the player's input does nothing: the offsets are eased out.
		if (bScripted)
		{
			return;
		}
		Offsets.Zoom = std::clamp(Offsets.Zoom * std::exp2(-Input.Zoom), FFieldCameraOffsets::MinZoom, FFieldCameraOffsets::MaxZoom);
		Offsets.Tilt = std::clamp(Offsets.Tilt + Input.Tilt, FFieldCameraOffsets::MinTilt, FFieldCameraOffsets::MaxTilt);
		if (Input.Turn != 0.0f)
		{
			const FFieldTurnRange Range = FieldViewTurnRange(Camera);
			const float From = Range.bAllRound ? Wrapped(Offsets.Turn) : std::clamp(Wrapped(Offsets.Turn), Range.Down, Range.Up);
			Offsets.Turn = Range.bAllRound ? Wrapped(From + Input.Turn) : std::clamp(From + Input.Turn, Range.Down, Range.Up);
			TurnInput += std::abs(Input.Turn);
		}
	}

	FFieldPoint FFieldPlayerCamera::StepSteadyFollow(const FFieldCamera& Camera, bool bTicked, bool bSnap)
	{
		constexpr std::array<std::size_t, 2> Ground = {0, 2};
		FFieldPoint Shift{};
		for (std::size_t Axis = 0; Axis < Ground.size(); ++Axis)
		{
			const int32 Target = Camera.Target[Ground[Axis]];
			int32& Steady = SteadyTarget[Axis];
			if (bSnap || !Offsets.bSteadyFollow)
			{
				Steady = Target;
			}
			else if (bTicked)
			{
				const int32 Gap = Camera.TargetGoal[Ground[Axis]] - Steady;
				Steady = std::abs(Gap) < SteadyFollowDivisor ? Camera.TargetGoal[Ground[Axis]] : Steady + Gap / SteadyFollowDivisor;
			}
			Shift[Ground[Axis]] = ToFixed(static_cast<double>(Steady - Target) / FixedOne * Eased(OffsetWeight));
		}
		return Shift;
	}

	std::array<FFieldPoint, 2> FFieldPlayerCamera::StepSmoothed(const FFieldPoint& Eye, const FFieldPoint& Target, bool bTicked, bool bSnap)
	{
		const auto Jumped = [](const FFieldPoint& From, const FFieldPoint& To)
		{
			return std::abs(static_cast<int64>(To[0]) - From[0]) > SmoothedCut || std::abs(static_cast<int64>(To[1]) - From[1]) > SmoothedCut
				|| std::abs(static_cast<int64>(To[2]) - From[2]) > SmoothedCut;
		};
		const std::array<FFieldPoint, 2> Now = {Eye, Target};
		if (bSnap || Jumped(SmoothedPoints[0][0], Eye) || Jumped(SmoothedPoints[0][1], Target))
		{
			SmoothedPoints.fill(Now);
		}
		else if (bTicked)
		{
			std::copy_backward(SmoothedPoints.begin(), SmoothedPoints.end() - 1, SmoothedPoints.end());
			SmoothedPoints[0] = Now;
		}
		std::array<FFieldPoint, 2> Average{};
		for (std::size_t Point = 0; Point < 2; ++Point)
		{
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				int64 Sum = 0;
				for (const std::array<FFieldPoint, 2>& Kept : SmoothedPoints)
				{
					Sum += Kept[Point][Axis];
				}
				Average[Point][Axis] = static_cast<int32>(Sum / SmoothedTicks);
			}
		}
		return Average;
	}

	const FFieldPresentedView& FFieldPlayerCamera::Present(const FEventContext& Context, bool bTicked, bool bNewMap)
	{
		const FFieldCamera& Game = Context.Camera;
		FFieldCamera Camera = Game;
		if (bTicked)
		{
			LockedTicks = Context.PlayerLock != 0 ? LockedTicks + 1 : 0;
		}
		const bool bScripted = IsFieldCameraScripted(Camera, Context.PlayerLock == 0) || (Offsets.bYieldsToScenes && LockedTicks > LockedMomentTicks);
		ApplyInput(Camera, bScripted);
		const FFieldTurnRange Range = FieldViewTurnRange(Camera);
		Basis.bScripted = bScripted;
		Basis.Range = Range;
		const float TurnTarget = Range.bAllRound ? Wrapped(Offsets.Turn) : std::clamp(Wrapped(Offsets.Turn), Range.Down, Range.Up);
		const float WeightTarget = bScripted ? 0.0f : 1.0f;
		const bool bSnap = !bStarted || bNewMap || Context.SettleTicks != 0;
		if (bSnap)
		{
			OffsetWeight = WeightTarget;
			TurnShown = TurnTarget;
		}
		else
		{
			if (bTicked)
			{
				OffsetWeight = Approach(OffsetWeight, WeightTarget, 1.0f / static_cast<float>(EaseTicks));
			}
			// The player's own turn shows at once; one the map forces moves at the camera's pace.
			const float Step = (bTicked ? ReturnTurnPerTick : 0.0f) + TurnInput;
			TurnShown = Range.bAllRound ? Wrapped(TurnShown + Approach(0.0f, Wrapped(TurnTarget - TurnShown), Step)) : Approach(TurnShown, TurnTarget, Step);
		}
		TurnInput = 0.0f;

		// The steady follow moves the eye and the target together across the ground: the rest is the game's camera. A
		// script's shot, the offsets eased out, shows the camera's points averaged over its last ticks.
		const FFieldPoint Shift = StepSteadyFollow(Game, bTicked, bSnap);
		Camera.Eye = Shaken(Game.Eye, Shift);
		Camera.Target = Shaken(Game.Target, Shift);
		const std::array<FFieldPoint, 2> Smoothed = StepSmoothed(Camera.Eye, Camera.Target, bTicked, bSnap);
		const double Smoothing = 1.0 - Eased(OffsetWeight);
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Camera.Eye[Axis] += static_cast<int32>(std::lround((Smoothed[0][Axis] - static_cast<double>(Camera.Eye[Axis])) * Smoothing));
			Camera.Target[Axis] += static_cast<int32>(std::lround((Smoothed[1][Axis] - static_cast<double>(Camera.Target[Axis])) * Smoothing));
		}
		Basis.bFollowMoved = Shift != FFieldPoint{};
		const FFieldPoint Eye = Shaken(Camera.Eye, Camera.Shake);
		const FFieldPoint Target = Shaken(Camera.Target, Camera.Shake);
		const FFieldPoint Pivot = ViewPivot(Context);
		if (bSnap || bTicked)
		{
			DrawnEye = bSnap ? Eye : LastEye;
			DrawnTarget = bSnap ? Target : LastTarget;
			DrawnPivot = bSnap ? Pivot : LastPivot;
			LastEye = Eye;
			LastTarget = Target;
			LastPivot = Pivot;
		}
		bStarted = true;

		Basis.Camera = Camera;
		Basis.Eye = Eye;
		Basis.Target = Target;
		Basis.Pivot = Pivot;
		// Once the map has settled the game draws each frame with the look-at of the tick before.
		Basis.bOwnTick = Context.SettleTicks != 0;
		const float Shows = Eased(OffsetWeight);
		TurnApplied = TurnShown * Shows;
		BuildShown(Shown, {std::pow(Offsets.Zoom, Shows), Offsets.Tilt * Shows, TurnApplied});
		return Shown;
	}

	FFieldViewPoints FFieldPlayerCamera::DrawnPoints(const FFieldViewOffsets& Shows) const
	{
		// The game's view is drawn from this tick's points while the map settles, then from the tick before's.
		return Basis.bOwnTick ? OffsetFieldView(Basis.Eye, Basis.Target, Basis.Pivot, Shows.Zoom, Shows.Tilt, Shows.Turn)
							  : OffsetFieldView(DrawnEye, DrawnTarget, DrawnPivot, Shows.Zoom, Shows.Tilt, Shows.Turn);
	}

	void FFieldPlayerCamera::BuildShown(FFieldPresentedView& Out, const FFieldViewOffsets& Shows) const
	{
		const FFieldCamera& Camera = Basis.Camera;
		const float Zoom = Shows.Zoom;
		const float Tilt = Shows.Tilt;
		const float Turn = Shows.Turn;
		Out.Offsets = Shows;
		Out.bGameView = Zoom == 1.0f && Tilt == 0.0f && Turn == 0.0f && !Basis.bFollowMoved;
		const FFieldViewPoints Drawn = DrawnPoints(Shows);
		Out.Unrounded = UnroundedFieldView(Camera, Drawn.Eye, Drawn.Target);
		Out.bUnrounded = true;
		if (Out.bGameView)
		{
			Out.View = Camera.View;
			Out.Eye = Whole(Camera.Eye);
			Out.Target = Whole(Camera.Target);
			Out.FarBucketShift = 0;
			return;
		}
		const FFieldViewPoints Now = OffsetFieldView(Camera.Eye, Camera.Target, Basis.Pivot, Zoom, Tilt, Turn);
		FFieldCamera Moved = Camera;
		Moved.Eye = Now.Eye;
		Moved.Target = Now.Target;
		Out.View = BuildFieldView(Moved, FieldLookAt(Drawn.Eye, Drawn.Target));
		Out.Eye = Whole(Moved.Eye);
		Out.Target = Whole(Moved.Target);
		Out.FarBucketShift = static_cast<int32>(std::lround((Distance(Moved.Eye, Moved.Target) - Distance(Camera.Eye, Camera.Target)) * EntriesPerWorldUnit));
	}

	FAffine FFieldPlayerCamera::UnroundedViewWithOffsets(const FFieldViewOffsets& Shows) const
	{
		const FFieldViewPoints Drawn = DrawnPoints(Shows);
		return UnroundedFieldView(Basis.Camera, Drawn.Eye, Drawn.Target);
	}

	FFieldViewOffsets FFieldPlayerCamera::LiveOffsets() const
	{
		const FFieldViewInput& Input = Pending;
		if (!bStarted || Basis.bScripted || Input.bReset || (Input.Turn == 0.0f && Input.Tilt == 0.0f && Input.Zoom == 0.0f))
		{
			return Shown.Offsets;
		}
		// The offsets as the next frame's ApplyInput leaves them, and the turn as its Present shows it.
		const FFieldTurnRange& Range = Basis.Range;
		const float Zoom = std::clamp(Offsets.Zoom * std::exp2(-Input.Zoom), FFieldCameraOffsets::MinZoom, FFieldCameraOffsets::MaxZoom);
		const float Tilt = std::clamp(Offsets.Tilt + Input.Tilt, FFieldCameraOffsets::MinTilt, FFieldCameraOffsets::MaxTilt);
		float Turn = Offsets.Turn;
		if (Input.Turn != 0.0f)
		{
			const float From = Range.bAllRound ? Wrapped(Offsets.Turn) : std::clamp(Wrapped(Offsets.Turn), Range.Down, Range.Up);
			Turn = Range.bAllRound ? Wrapped(From + Input.Turn) : std::clamp(From + Input.Turn, Range.Down, Range.Up);
		}
		const float Target = Range.bAllRound ? Wrapped(Turn) : std::clamp(Wrapped(Turn), Range.Down, Range.Up);
		const float Step = std::abs(Input.Turn);
		const float NextTurn = Range.bAllRound ? Wrapped(TurnShown + Approach(0.0f, Wrapped(Target - TurnShown), Step)) : Approach(TurnShown, Target, Step);
		const float Shows = Eased(OffsetWeight);
		return {std::pow(Zoom, Shows), Tilt * Shows, NextTurn * Shows};
	}

	int16 FFieldPlayerCamera::ViewTurn() const
	{
		// Turning the eye the way R1 does turns the view's yaws the other way.
		return static_cast<int16>(-std::lround(LiveOffsets().Turn * YawUnitsPerDegree));
	}
}
