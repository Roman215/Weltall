#pragma once

#include "Field/EventActorState.h"
#include "Field/FieldReturn.h"
#include "Graphics/SpriteSystem.h"

namespace Weltall
{
	/** Every member of a field actor as the field saves it, in the order a save state keeps them (Game/SaveStateMembers.h). */
	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FSpriteSavedState& Value)
	{
		Ar(Value.Position);
		Ar(Value.Facing);
		Ar(Value.ListStep);
		Ar(Value.Animation);
		Ar(Value.NextAnimation);
		Ar(Value.StepsRun);
		Ar(Value.PresetVerticalSpeed);
		Ar(Value.PresetGravity);
		Ar(Value.TransformScale);
		Ar(Value.Scale);
		Ar(Value.Weight);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FEventThread& Value)
	{
		Ar(Value.Pc);
		Ar(Value.WaitCount);
		Ar(Value.Script);
		Ar(Value.Word);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FEventActorState::FRide& Value)
	{
		Ar(Value.PlatformRotation);
		Ar(Value.Distance);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FEventActorState& Value)
	{
		Ar(Value.Flags0);
		Ar(Value.Flags1);
		Ar(Value.FloorTriangles);
		Ar(Value.Layer);
		Ar(Value.Word12);
		Ar(Value.FloorAttributes);
		Ar(Value.HalfWidth);
		Ar(Value.Height);
		Ar(Value.HalfDepth);
		Ar(Value.Radius);
		Ar(Value.Position);
		Ar(Value.Word2C);
		Ar(Value.Step);
		Ar(Value.Word3C);
		Ar(Value.Velocity);
		Ar(Value.Word4C);
		Ar(Value.FloorNormal);
		Ar(Value.Word5C);
		Ar(Value.InteractX);
		Ar(Value.Word62);
		Ar(Value.InteractZ);
		Ar(Value.Word66);
		Ar(Value.PreviousPosition);
		Ar(Value.StillTicks);
		Ar(Value.ExtraAngle);
		Ar(Value.FloorHeight);
		Ar(Value.LinkedActor);
		Ar(Value.Parent);
		Ar(Value.SpeedDivisor);
		Ar(Value.CallStack);
		Ar(Value.Portrait);
		Ar(Value.Choice);
		Ar(Value.WindowWidth);
		Ar(Value.WindowLines);
		Ar(Value.WindowStyle);
		Ar(Value.WindowX);
		Ar(Value.WindowY);
		Ar(Value.Threads);
		Ar(Value.Pc);
		Ar(Value.Slot);
		Ar(Value.RequestSlot);
		Ar(Value.MoveWork);
		Ar(Value.BytesDC);
		Ar(Value.JumpTicks);
		Ar(Value.DoorStep);
		Ar(Value.PushCount);
		Ar(Value.Character);
		Ar(Value.IdleAnimation);
		Ar(Value.Animation);
		Ar(Value.AnimationRequest);
		Ar(Value.TargetHeight);
		Ar(Value.DrawSize);
		Ar(Value.WordF0);
		Ar(Value.Scale);
		Ar(Value.WordFA);
		Ar(Value.Tints);
		Ar(Value.Counter);
		Ar(Value.MoveDirection);
		Ar(Value.Facing);
		Ar(Value.Yaw);
		Ar(Value.Sound);
		Ar(Value.SoundVolume);
		Ar(Value.SoundMode);
		Ar(Value.Bytes10E);
		Ar(Value.LockedFacing);
		Ar(Value.TurnSpeed);
		Ar(Value.ExternalAnimationFile);
		Ar(Value.ModelId);
		Ar(Value.ModelIndex);
		Ar(Value.GearJoint);
		Ar(Value.Word12A);
		Ar(Value.Status);
		Ar(Value.SpriteTexture);
		Ar(Value.SpriteDraw);
		Ar(Value.Ride);
		Ar(Value.MoveBounds);
		Ar(Value.PartValues);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FFieldSavedActor& Value)
	{
		Ar(Value.Rotation);
		Ar(Value.Flags);
		Ar(Value.Sprite);
		Ar(Value.State);
	}
}
