#include "Field/FieldCameraUpdate.h"

#include "Field/EventContext.h"
#include "Field/FieldCameraFloor.h"
#include "Field/FieldWalkmesh.h"
#include "Foundation/ProcessorDivide.h"
#include "Game/PsxRandom.h"
#include "Graphics/GteMath.h"

namespace Weltall
{
	namespace
	{
		constexpr int16 FollowMode = 0;
		constexpr int16 ScriptMode = 1;
		constexpr int16 ReturnMode = 2;
		/** The divisor a follow eases back to, by 2 every fourth tick. */
		constexpr int32 RestingDivisor = 8;
		/** Easing back to following ends after this many ticks, or once both points are this close to their goals. */
		constexpr int32 MaxReturnTicks = 64;
		constexpr int32 ReturnDistance = 128;
		constexpr int32 QuarterTurn = 0x400;

		int32 Sum(int32 A, int32 B)
		{
			return static_cast<int32>(static_cast<uint32>(A) + static_cast<uint32>(B));
		}

		int32 Difference(int32 A, int32 B)
		{
			return static_cast<int32>(static_cast<uint32>(A) - static_cast<uint32>(B));
		}

		int16 WholePart(int32 Value)
		{
			return static_cast<int16>(Value >> 16);
		}

		/** A scripted move's tick: the point steps while it has ticks, and the goal follows the point. */
		void StepMove(FFieldCamera& Camera, FFieldCameraMove& Move, uint16 Bit, FFieldPoint& Goal)
		{
			if ((Camera.PathMoves & Bit) == 0)
			{
				return;
			}
			if (Move.Ticks != 0)
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Move.At[Axis] = Sum(Move.At[Axis], Move.Step[Axis]);
				}
			}
			Move.Ticks = static_cast<int16>(Move.Ticks - 1);
			if (Move.Ticks == 0)
			{
				Camera.PathMoves = static_cast<uint16>(Camera.PathMoves & ~Bit);
			}
			Goal = Move.At;
		}

		int32 EasedDivisor(int32 Divisor)
		{
			return Divisor > RestingDivisor ? Divisor - 2 : RestingDivisor;
		}

		/**
		 * A ramp's tick: the value steps while it has ticks (or always). Ending it clears its flag
		 * and, as the original's 16-bit mask does, the flag word's upper half.
		 */
		void StepRamp(FFieldCameraRamp& Ramp, int16& Value, uint32& Flags, uint32 Flag, bool bAlwaysSteps)
		{
			if (bAlwaysSteps || Ramp.Ticks != 0)
			{
				Ramp.Value = Sum(Ramp.Value, Ramp.Step);
				Value = static_cast<int16>(Ramp.Value >> 16);
			}
			Ramp.Ticks = static_cast<int16>(Ramp.Ticks - 1);
			if (Ramp.Ticks == 0)
			{
				Flags &= 0xFFFFu & ~Flag;
			}
		}

		/**
		 * The follow's goals for the followed actor: the target on it inside the camera's area, else
		 * where its way back crosses the area's edge at a kept height; the eye from the target; then
		 * the zoom's and the pitch's ramps, which the next tick's goals show.
		 */
		void FollowActor(FFieldCamera& Camera, const FFieldWalkmesh& Walkmesh, const FEventActorState& Actor)
		{
			const FFieldPoint& Position = Actor.Position;
			const FFieldCameraAreaPoint Area = FindFieldCameraAreaPoint(Walkmesh, Camera.Bounds, WholePart(Position[0]), WholePart(Position[2]));
			if (Area.bInside)
			{
				Camera.TargetGoal = FollowTarget(Position);
				Camera.bHeightKept = false;
			}
			else
			{
				Camera.TargetGoal[0] = Area.X * 0x10000;
				Camera.TargetGoal[2] = Area.Z * 0x10000;
				if (Camera.FollowHeight != 0)
				{
					Camera.TargetGoal[1] = FollowTarget(Position)[1];
				}
				else if (!Camera.bHeightKept)
				{
					Camera.bHeightKept = true;
					Camera.TargetGoal[1] = Actor.FloorHeight * 0x10000;
				}
			}
			Camera.EyeGoal = FollowEye(Camera, Camera.TargetGoal);
			if ((Camera.Flags & FieldCameraFlags::ZoomRamp) != 0)
			{
				StepRamp(Camera.ZoomRamp, Camera.DistanceScale, Camera.Flags, FieldCameraFlags::ZoomRamp, false);
			}
			if ((Camera.Flags & FieldCameraFlags::PitchRamp) != 0)
			{
				StepRamp(Camera.PitchRamp, Camera.Pitch, Camera.Flags, FieldCameraFlags::PitchRamp, true);
			}
		}

		/** Following, or easing back to it: the yaw, the follow, the eye kept above the camera layer, and the return's end. */
		void Follow(FFieldCamera& Camera, const FFieldWalkmesh& Walkmesh, const FEventActorState& Actor, uint16 PadHeld)
		{
			if (Camera.Mode == FollowMode)
			{
				Camera.ReturnTicks = 0;
				if ((Camera.FollowTicks & 3) == 0)
				{
					Camera.TargetDivisor = EasedDivisor(Camera.TargetDivisor);
					Camera.EyeDivisor = EasedDivisor(Camera.EyeDivisor);
				}
				Camera.FollowTicks = Sum(Camera.FollowTicks, 1);
			}
			else
			{
				Camera.FollowTicks = 0;
				Camera.ReturnTicks += 1;
				if (Camera.ReturnTicks > MaxReturnTicks)
				{
					Camera.Mode = FollowMode;
				}
			}
			UpdateFieldCameraYaw(Camera, PadHeld);
			FollowActor(Camera, Walkmesh, Actor);
			if ((Camera.Flags & FieldCameraFlags::EyeMayGoBelowCameraLayer) == 0)
			{
				Camera.EyeGoal = KeepEyeAboveCameraLayer(Walkmesh, Camera.EyeGoal);
			}
			if (Camera.Mode == ReturnMode)
			{
				const int32 TargetLeft = FieldLength2D(WholePart(Camera.TargetGoal[0]) - WholePart(Camera.Target[0]), WholePart(Camera.TargetGoal[2]) - WholePart(Camera.Target[2]));
				const int32 EyeLeft = FieldLength2D(WholePart(Camera.EyeGoal[0]) - WholePart(Camera.Eye[0]), WholePart(Camera.EyeGoal[2]) - WholePart(Camera.Eye[2]));
				if (TargetLeft < ReturnDistance && EyeLeft < ReturnDistance)
				{
					Camera.Mode = FollowMode;
				}
			}
		}

		/** One axis's easing: a divisor's share of the way to the goal, unless the whole parts are within the divisor. */
		void EaseToward(int32& Current, int32 Goal, int32 Divisor)
		{
			if (WholePart(Current) == WholePart(Goal))
			{
				return;
			}
			const int32 Way = Difference(Goal, Current);
			const int32 Whole = Way >> 16;
			if (static_cast<int32>(static_cast<uint32>(Whole) * static_cast<uint32>(Whole)) < static_cast<int32>(static_cast<uint32>(Divisor) * static_cast<uint32>(Divisor)))
			{
				return;
			}
			Current = Sum(Current, ProcessorDivide(Way, Divisor));
		}

		void Shake(FFieldCamera& Camera, FPsxRandom& Random)
		{
			Camera.Shake = {};
			if (!Camera.bShaking)
			{
				return;
			}
			if (Camera.ShakeTicks != 0)
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Camera.ShakeAmplitude[Axis] = Sum(Camera.ShakeAmplitude[Axis], Camera.ShakeGrowth[Axis]);
				}
			}
			else if (Camera.bShakeStops)
			{
				Camera.ShakeAmplitude = {};
				Camera.bShaking = false;
				Camera.bShakeStops = false;
			}
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Camera.Shake[Axis] = Random.Next() * WholePart(Camera.ShakeAmplitude[Axis]);
			}
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				if (Camera.Shake[Axis] < 0)
				{
					Camera.Shake[Axis] = 0;
					Camera.ShakeAmplitude[Axis] = 0;
				}
			}
			if (Camera.ShakeTicks > 0)
			{
				Camera.ShakeTicks = static_cast<int16>(Camera.ShakeTicks - 1);
			}
		}

		FFieldPoint Shaken(const FFieldPoint& Point, const FFieldPoint& Shake)
		{
			return {Sum(Point[0], Shake[0]), Sum(Point[1], Shake[1]), Sum(Point[2], Shake[2])};
		}

		/** The view's directions before the camera moves: across the ground from the eye to the target (and between their goals), and down. */
		void TakeViewAngles(FFieldCamera& Camera, const FGteArctangents& Table)
		{
			const int32 AcrossX = Difference(Camera.Target[0], Camera.Eye[0]);
			const int32 AcrossZ = Difference(Camera.Target[2], Camera.Eye[2]);
			Camera.ViewYaw = static_cast<int16>(GteRatan2(AcrossZ, AcrossX, Table) - QuarterTurn);
			Camera.GoalViewYaw = static_cast<int16>(
				GteRatan2(Difference(Camera.TargetGoal[2], Camera.EyeGoal[2]), Difference(Camera.TargetGoal[0], Camera.EyeGoal[0]), Table) - QuarterTurn);
			Camera.ViewPitch = GteRatan2(FieldLength2D(AcrossX >> 16, AcrossZ >> 16), Difference(Camera.Target[1], Camera.Eye[1]) >> 16, Table);
		}
	}

	void SmoothFieldCamera(FFieldCamera& Camera, FPsxRandom& Random)
	{
		if ((Camera.Flags & FieldCameraFlags::ProjectionRamp) != 0)
		{
			FFieldCameraRamp& Ramp = Camera.ProjectionRamp;
			if (Ramp.Ticks != 0)
			{
				Ramp.Value = Sum(Ramp.Value, Ramp.Step);
				Camera.ProjectionDistance = Ramp.Value >> 16;
			}
			// This ramp ends a tick after its last step.
			Ramp.Ticks = static_cast<int16>(Ramp.Ticks - 1);
			if (Ramp.Ticks < 0)
			{
				Camera.Flags &= 0xFFFFu & ~FieldCameraFlags::ProjectionRamp;
				Ramp.Ticks = 0;
			}
		}
		if (Camera.SnapTicks != 0)
		{
			Camera.TargetDivisor = 1;
			Camera.EyeDivisor = 1;
			Camera.SnapTicks -= 1;
		}
		for (const std::size_t Axis : {0u, 2u, 1u})
		{
			EaseToward(Camera.Eye[Axis], Camera.EyeGoal[Axis], Camera.EyeDivisor);
		}
		for (const std::size_t Axis : {0u, 2u, 1u})
		{
			EaseToward(Camera.Target[Axis], Camera.TargetGoal[Axis], Camera.TargetDivisor);
		}
		Shake(Camera, Random);
	}

	bool UpdateFieldCamera(FEventContext& Context)
	{
		FFieldCamera& Camera = Context.Camera;
		Camera.WorldRotation = RotationXyz(Camera.WorldAngles);
		const bool bFollows = Camera.Mode == FollowMode || Camera.Mode == ReturnMode;
		const FEventActorState* Actor = Context.StateOf(Camera.FollowedActor);
		if (bFollows && (Actor == nullptr || Context.Walkmesh == nullptr || Context.Walkmesh->CameraLayer().Triangles.empty()))
		{
			return false;
		}
		TakeViewAngles(Camera, *Context.Arctangents);
		if (Camera.Mode == ScriptMode)
		{
			Camera.FollowTicks = 0;
			Camera.ReturnTicks = 0;
			StepMove(Camera, Camera.TargetMove, FieldCameraPathMoves::Target, Camera.TargetGoal);
			StepMove(Camera, Camera.EyeMove, FieldCameraPathMoves::Eye, Camera.EyeGoal);
		}
		else if (bFollows)
		{
			Follow(Camera, *Context.Walkmesh, *Actor, static_cast<uint16>(Context.PadHeld & ~Context.PadZoomHeld));
		}
		SmoothFieldCamera(Camera, *Context.Random);
		Camera.Rotation[1] = static_cast<int16>(Camera.Rotation[1] & 0xFFF);

		const FGteMatrix LookAt = FieldLookAt(Shaken(Camera.Eye, Camera.Shake), Shaken(Camera.Target, Camera.Shake));
		// Once the map has settled, each frame is drawn with the view of the tick before.
		const FGteMatrix Drawn = Context.SettleTicks != 0 ? LookAt : Camera.LookAt;
		Camera.LookAt = LookAt;
		Camera.View = BuildFieldView(Camera, Drawn);
		return true;
	}
}
