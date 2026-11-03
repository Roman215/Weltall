#pragma once

#include "Field/EventVariables.h"
#include "Field/FieldActorMembers.h"
#include "Field/FieldCamera.h"
#include "Field/FieldKernelState.h"
#include "Field/FieldMovement.h"
#include "Field/FieldRenderState.h"
#include "Field/FieldReturn.h"
#include "Field/FieldScrolls.h"
#include "Graphics/GteMatrix.h"
#include "Graphics/MdlModel.h"

namespace Weltall
{
	/**
	 * Every member of the field's state as it saves itself for a battle, and of the kernel's globals it keeps between its
	 * runs, in the order a save state keeps them (Game/SaveStateMembers.h).
	 */
	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FGteMatrix& Value)
	{
		Ar(Value.M);
		Ar(Value.T);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FMdlColor& Value)
	{
		Ar(Value.R);
		Ar(Value.G);
		Ar(Value.B);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FEventVariables& Value)
	{
		for (uint32 Address = 0; Address < FEventVariables::AddressEnd; Address += 2)
		{
			uint16 Variable = Value.Raw(Address);
			Ar(Variable);
			Value.Set(Address, Variable);
		}
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FFieldCameraRamp& Value)
	{
		Ar(Value.Ticks);
		Ar(Value.Value);
		Ar(Value.Step);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FFieldCameraMove& Value)
	{
		Ar(Value.Ticks);
		Ar(Value.At);
		Ar(Value.Step);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FFieldView& Value)
	{
		Ar(Value.View);
		Ar(Value.Scene);
		Ar(Value.ScreenRotation);
		Ar(Value.Billboard);
		Ar(Value.Rotation);
		Ar(Value.Pitch);
		Ar(Value.ProjectionDistance);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FFieldCamera& Value)
	{
		Ar(Value.Eye);
		Ar(Value.Target);
		Ar(Value.EyeGoal);
		Ar(Value.TargetGoal);
		Ar(Value.TargetFrom);
		Ar(Value.TargetTo);
		Ar(Value.EyeFrom);
		Ar(Value.EyeTo);
		Ar(Value.PathMoves);
		Ar(Value.TargetMove);
		Ar(Value.EyeMove);
		Ar(Value.Pitch);
		Ar(Value.ProjectionDistance);
		Ar(Value.DistanceScale);
		Ar(Value.Rotation);
		Ar(Value.TurningYaw);
		Ar(Value.YawGoal);
		Ar(Value.TurnStep);
		Ar(Value.TurnTicks);
		Ar(Value.bTurnByButtons);
		Ar(Value.FollowedActor);
		Ar(Value.ScreenAngles);
		Ar(Value.WorldAngles);
		Ar(Value.WorldRotation);
		Ar(Value.Flags);
		Ar(Value.ZoomRamp);
		Ar(Value.PitchRamp);
		Ar(Value.ProjectionRamp);
		Ar(Value.SnapTicks);
		Ar(Value.ForbiddenDirections);
		Ar(Value.LeftDirections);
		Ar(Value.Mode);
		Ar(Value.TargetDivisor);
		Ar(Value.EyeDivisor);
		Ar(Value.ScriptYaw);
		Ar(Value.ScriptPitch);
		Ar(Value.ScriptDistance);
		Ar(Value.ScriptScale);
		Ar(Value.Bounds);
		Ar(Value.FollowHeight);
		Ar(Value.bHeightKept);
		Ar(Value.FollowTicks);
		Ar(Value.ReturnTicks);
		Ar(Value.bShaking);
		Ar(Value.ShakeTicks);
		Ar(Value.bShakeStops);
		Ar(Value.ShakeAmplitude);
		Ar(Value.ShakeGrowth);
		Ar(Value.Shake);
		Ar(Value.ViewYaw);
		Ar(Value.GoalViewYaw);
		Ar(Value.ViewPitch);
		Ar(Value.LookAt);
		Ar(Value.View);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FFieldFog& Value)
	{
		Ar(Value.bOn);
		Ar(Value.Color);
		Ar(Value.FarColor);
		Ar(Value.Near);
		Ar(Value.Far);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FFieldPanorama& Value)
	{
		Ar(Value.bEnabled);
		Ar(Value.bHidden);
		Ar(Value.VramX);
		Ar(Value.VramY);
		Ar(Value.Width);
		Ar(Value.Height);
		Ar(Value.ClutX);
		Ar(Value.ClutY);
		Ar(Value.TexturePage);
		Ar(Value.Scroll);
		Ar(Value.Position);
		Ar(Value.Colors);
		Ar(Value.BandScale);
		Ar(Value.DepthDivisor);
		Ar(Value.DepthOffset);
		Ar(Value.bBands);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FFieldGearLights::FPointLight& Value)
	{
		Ar(Value.Actor);
		Ar(Value.Near);
		Ar(Value.Far);
		Ar(Value.Radius);
		Ar(Value.Position);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FFieldGearLights& Value)
	{
		Ar(Value.Words);
		Ar(Value.Ambient);
		Ar(Value.Points);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FFieldFade& Value)
	{
		Ar(Value.Color);
		Ar(Value.Step);
		Ar(Value.BlendMode);
		Ar(Value.bActive);
		Ar(Value.Ticks);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FFieldScreenWave& Value)
	{
		Ar(Value.bOn);
		Ar(Value.bOffWhenDone);
		Ar(Value.Ticks);
		Ar(Value.Parameters);
		Ar(Value.Steps);
		Ar(Value.PhaseX);
		Ar(Value.PhaseY);
		Ar(Value.bDrawn);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FFieldMovement& Value)
	{
		Ar(Value.JumpAnimation);
		Ar(Value.ActionMode);
		Ar(Value.JumpInterval);
		Ar(Value.JumpCooldown);
		Ar(Value.FloorPushAngle);
		Ar(Value.FloorPushScale);
		Ar(Value.bFirstTickDone);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FFieldSnapshot& Value)
	{
		Ar(Value.ScriptsKey);
		Ar(Value.Actors);
		Ar(Value.WalkAttributes);
		Ar(Value.Variables);
		Ar(Value.Camera);
		Ar(Value.BackgroundColor);
		Ar(Value.FarBucket);
		Ar(Value.Fog);
		Ar(Value.Panorama);
		Ar(Value.GearLights);
		Ar(Value.SceneryDrift);
		Ar(Value.DriftStep);
		Ar(Value.DriftTotal);
		Ar(Value.Fades);
		Ar(Value.ScreenWave);
		Ar(Value.EncounterPeriod);
		Ar(Value.EncountersPerPeriod);
		Ar(Value.EncounterCountdown);
		Ar(Value.EncountersPending);
		Ar(Value.ScriptMask);
		Ar(Value.bReversed);
		Ar(Value.bPartyPlaced);
		Ar(Value.bFollowClose);
		Ar(Value.bLeaderApart);
		Ar(Value.Movement);
		Ar(Value.SoundListener);
		Ar(Value.SoundRange);
		Ar(Value.SoundKeepMask);
		Ar(Value.GearCount);
		Ar(Value.GearFileNumbers);
		Ar(Value.GearAnimations);
		Ar(Value.GearLoadScales);
		Ar(Value.GearTextureShifts);
		Ar(Value.GearPoolSize);
		Ar(Value.PlayerLock);
		Ar(Value.bMenuLocked);
		Ar(Value.bCompassHidden);
		Ar(Value.EncounterBattleKind);
		Ar(Value.ScriptedBattleKind);
		Ar(Value.GearBoardingOverride);
		Ar(Value.GearTurnSpeed);
		Ar(Value.bPartyWalking);
		Ar(Value.OpenWindows);
		Ar(Value.WindowOpenTicks);
		Ar(Value.FrameDelay);
		Ar(Value.LeaderActor);
		Ar(Value.PilotSpriteBase);
		Ar(Value.PartyWalkHurries);
		Ar(Value.LeaderFlagsBeforeWalk);
		Ar(Value.PauseDisabled);
		Ar(Value.PartySpriteFiles);
		Ar(Value.PartyCharacters);
		Ar(Value.PartyActors);
		Ar(Value.GearActors);
		Ar(Value.Riding);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FFieldClock& Value)
	{
		Ar(Value.Ticks);
		Ar(Value.Mode);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FSpriteSystem::FRingPlace& Value)
	{
		Ar(Value.X);
		Ar(Value.Row);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FFieldKernelState& Value)
	{
		Ar(Value.EncounterWeights);
		Ar(Value.BattleFormation);
		Ar(Value.bGearsLocked);
		Ar(Value.BattlingResult);
		Ar(Value.Clock);
		Ar(Value.MenuParam);
		Ar(Value.MenusPending);
		Ar(Value.SpriteRingPlace);
		Ar(Value.Saved);
	}
}
