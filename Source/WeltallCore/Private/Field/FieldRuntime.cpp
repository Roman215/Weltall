#include "Field/FieldRuntime.h"

#include "Field/FieldGearBoarding.h"
#include "Field/FieldLight.h"
#include "Field/FieldMainLoop.h"
#include "Field/FieldReturn.h"
#include "Field/FieldSprites.h"
#include "Field/FieldTick.h"
#include "Game/GameState.h"
#include "Input/KernelPad.h"
#include "Input/KernelVibration.h"
#include "Timing/PlayClock.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		// What the main loop and the loops it runs hold in the register the sprites' stray steps wait by:
		// the byte it compares requests with.
		constexpr int32 MainLoopStrayWait = 0xFF;
		/** The fade's frames, the pause's and Select's picture's wait two vertical blanks each. */
		constexpr int32 FrameVblanks = 2;
		/** A movie's first picture comes in a vertical blank, a pass over the field in one, full screen in two; a skip's fade lasts five. */
		constexpr int32 MovieFirstPictureVblanks = 1;
		constexpr int32 OverFieldPassVblanks = 1;
		constexpr int32 FullScreenPassVblanks = 2;
		constexpr int32 MovieSkipVblanks = 5;
		/** The menu's shading takes a vertical blank a step. */
		constexpr int32 MenuShadeVblanks = 1;
		/** The variable the field sets on every pass of its main loop on the retail discs. */
		constexpr uint32 RetailVariable = 0x050;

		FError RequestPending(EFieldRequest Kind)
		{
			return MakeError("the field waits for its request (" + std::to_string(static_cast<int32>(Kind)) + ") to be taken");
		}
	}

	int32 FFieldRuntime::VblanksPerTick() const
	{
		switch (Phase)
		{
		case EPhase::EntryFade:
		case EPhase::PadWait:
		case EPhase::Paused:
		case EPhase::MapFadeOut:
		case EPhase::MapPictureIn:
		case EPhase::MapPictureHeld:
		case EPhase::MapPictureOut:
			return FrameVblanks;
		case EPhase::MovieFirstPicture:
			return MovieFirstPictureVblanks;
		case EPhase::MovieSkipFade:
			return MovieSkipVblanks;
		case EPhase::MoviePass:
			if (FieldContext.MovieView == EFieldMovieView::FullScreen)
			{
				return FullScreenPassVblanks;
			}
			if (FieldContext.MovieView == EFieldMovieView::OverField)
			{
				return OverFieldPassVblanks;
			}
			return 2 + FieldContext.FrameDelay;
		case EPhase::MenuShadeIn:
		case EPhase::MenuShadeOut:
			return MenuShadeVblanks;
		default:
			return 2 + FieldContext.FrameDelay;
		}
	}

	std::optional<FError> FFieldRuntime::Tick(std::span<const FPadState> HeldPerVblank, IFieldCharacterDrawing* Drawing)
	{
		if (Phase == EPhase::Stopped)
		{
			return RequestPending(PendingRequest.Kind);
		}
		if (static_cast<int32>(HeldPerVblank.size()) != VblanksPerTick())
		{
			return MakeError("the field's frame lasts " + std::to_string(VblanksPerTick()) + " vertical blanks, not " + std::to_string(HeldPerVblank.size()));
		}
		FieldContext.Render.BeginTick();
		if (FieldContext.Gears.Models)
		{
			FieldContext.Gears.Models->TakeVramChanges();
		}
		// A movie's frames count their vertical blanks where the original's waits fall among its steps.
		const bool bMovieFrame = Phase == EPhase::MovieFirstPicture || Phase == EPhase::MoviePass || Phase == EPhase::MovieSkipFade;
		if (!bMovieFrame)
		{
			RunVblanks(HeldPerVblank);
		}
		struct FDrawingScope
		{
			IFieldCharacterDrawing*& Slot;
			~FDrawingScope() { Slot = nullptr; }
		};
		FrameDrawing = Drawing;
		const FDrawingScope Scope{FrameDrawing};
		LastFrameKind = EFieldFrame::Tick;
		FieldContext.Render.Cover.bShown = false;
		FieldContext.Render.MapPicture.bShown = false;
		FieldContext.Render.MenuShade.bShown = false;
		switch (Phase)
		{
		case EPhase::Entry:
			return TickEntry();
		case EPhase::EntryFade:
			return TickEntryFade();
		case EPhase::MainLoop:
			return RunIteration(EStep::Top);
		case EPhase::PadWait:
			LastFrameKind = EFieldFrame::Paused;
			return TickPadWait();
		case EPhase::Paused:
			LastFrameKind = EFieldFrame::Paused;
			return TickPause();
		case EPhase::MapFadeOut:
			LastFrameKind = EFieldFrame::MapFade;
			return TickMapFade();
		case EPhase::MapEntry:
			return TickMapEntry();
		case EPhase::DissolveToStill:
		case EPhase::DissolveHold:
		case EPhase::DissolveFromStill:
			return TickDissolve();
		case EPhase::MapPictureIn:
		case EPhase::MapPictureHeld:
		case EPhase::MapPictureOut:
			LastFrameKind = EFieldFrame::MapPicture;
			return TickMapPicture();
		case EPhase::MovieLead:
		case EPhase::MovieLeadAgain:
			return TickMovieLead();
		case EPhase::MovieFirstPicture:
		case EPhase::MoviePass:
		case EPhase::MovieSkipFade:
			return TickMovie(HeldPerVblank);
		case EPhase::MenuShadeIn:
		case EPhase::MenuShadeOut:
			LastFrameKind = EFieldFrame::MenuShade;
			return TickMenuShade();
		case EPhase::Stopped:
			break;
		}
		return std::nullopt;
	}

	void FFieldRuntime::RunVblanks(std::span<const FPadState> HeldPerVblank)
	{
		FGameState& Game = *Services.Game;
		for (const FPadState Held : HeldPerVblank)
		{
			// The console counts the vertical blank before it reads the pad: the repeat keeps to the count.
			Game.SetPlayTime(Game.PlayTime() + 1);
			Services.Pad->Vblank(Held, Game.PlayTime());
			if (DefeatHoldVblanks != 0)
			{
				--DefeatHoldVblanks;
			}
			else if (Held.Buttons == 0)
			{
				bDefeatHeldUntilReleased = false;
			}
			Services.Clock->Vblank();
			if (Services.Vibration != nullptr)
			{
				Services.Vibration->Vblank();
			}
		}
	}

	FFieldTickInput FFieldRuntime::TakeInput()
	{
		const FKernelPadWords Words = Services.Pad->TakeMerged();
		FFieldTickInput Input;
		if (!bDefeatHeldUntilReleased)
		{
			Input.Pad = {Words.Held, Words.Pressed, Words.Repeated, Words.StickHeld, Words.StickX, Words.StickY};
		}
		Input.Clock = *Services.Clock;
		Input.ViewTurn = ViewTurn;
		Input.Drawing = FrameDrawing;
		return Input;
	}

	void FFieldRuntime::SetLightLeft(const FGteMatrix& Light)
	{
		if (Light != GteLight)
		{
			bGteColorsKnown = bGteBackColorKnown = false;
		}
		GteLight = GteLightUnderGears = Light;
		FieldContext.Gears.DrawnLighting = LightingLeft();
	}

	std::optional<FGte> FFieldRuntime::LightingLeft() const
	{
		if (!bGteColorsKnown || !bGteBackColorKnown)
		{
			return std::nullopt;
		}
		FGte Lighting;
		Lighting.LightMatrix = GteLight;
		Lighting.ColorMatrix = GteColors;
		Lighting.BackColor = GteBackColor;
		return Lighting;
	}

	void FFieldRuntime::TakeGearColors()
	{
		FGte Colors;
		LoadFieldGearColors(FieldContext.Render.GearLights, Colors);
		GteColors = Colors.ColorMatrix;
		GteBackColor = Colors.BackColor;
		bGteColorsKnown = bGteBackColorKnown = true;
	}

	void FFieldRuntime::TakeGearLoadColors()
	{
		if (FieldContext.Gears.Count != 0)
		{
			FGte Colors;
			LoadFieldGearColors(FieldContext.Render.GearLights, Colors);
			GteBackColor = Colors.BackColor;
			bGteBackColorKnown = true;
		}
		FieldContext.Gears.DrawnLighting = LightingLeft();
	}

	std::optional<FError> FFieldRuntime::RunFrame(int32 StrayWait, bool bPollMusic)
	{
		ActorSprites->System().StrayWait = StrayWait;
		if (auto Error = RunFieldFrame(FieldContext, TakeInput()))
		{
			return Error;
		}
		// The tick's drawing: the models, each relit one under the map's ambient, then the gears while the field counts some.
		const FMdlLights MapLights = FieldMapLights(MapData->Lights);
		const std::optional<FGteMatrix> ModelsLight = FieldModelsLight(FieldContext, MapLights);
		if (ModelsLight)
		{
			GteBackColor = MapLights.Ambient;
			bGteBackColorKnown = true;
		}
		GteLightUnderGears = ModelsLight.value_or(GteLight);
		GteLight = GteLightUnderGears;
		if (FieldContext.Gears.Models && FieldContext.Gears.Count != 0)
		{
			GteLight = FieldGearsLight(*FieldContext.Gears.Models, FieldContext.Render.GearLights).value_or(GteLight);
			TakeGearColors();
		}
		FieldContext.Gears.DrawnLighting = LightingLeft();
		if (auto Error = ActorSprites->System().FaultError())
		{
			return Error;
		}
		return bPollMusic ? PollFieldMusic(FieldContext) : std::nullopt;
	}

	std::optional<FError> FFieldRuntime::RunIteration(EStep From)
	{
		switch (From)
		{
		case EStep::Top:
			// Neither the wait for a controller nor the pause counts as play time.
			PauseStartPlayTime = Services.Game->PlayTime();
			if (!Services.Pad->IsConnected())
			{
				PauseSound(true);
				Phase = EPhase::PadWait;
				LastFrameKind = EFieldFrame::Paused;
				return std::nullopt;
			}
			[[fallthrough]];
		case EStep::AfterPadWait:
			if (FieldMainLoop::PausesOnStart(FieldContext))
			{
				PauseSound(true);
				Phase = EPhase::Paused;
				LastFrameKind = EFieldFrame::Paused;
				return std::nullopt;
			}
			Services.Game->SetPlayTime(PauseStartPlayTime);
			[[fallthrough]];
		case EStep::AfterPause:
		{
			FieldContext.Variables.Set(RetailVariable, 1);
			if (IsSoftReset())
			{
				return std::nullopt;
			}
			if (auto Error = RunFrame(MainLoopStrayWait, false))
			{
				return Error;
			}
			if (FieldContext.Requests.Dissolve != 0 && BeginDissolve())
			{
				return std::nullopt;
			}
			return AfterDissolve();
		}
		case EStep::AfterDissolve:
			return AfterDissolve();
		case EStep::AfterMapChange:
			return AfterMapChange();
		default:
			return IdleChecks(From);
		}
	}

	std::optional<FError> FFieldRuntime::AfterDissolve()
	{
		if (FieldMainLoop::BattleStarts(FieldContext))
		{
			FFieldRequest Battle;
			Battle.Kind = EFieldRequest::Battle;
			FieldContext.Messages.CloseAll();
			return LeaveField(Battle);
		}
		if (FieldMainLoop::MapChangeGoesAhead(FieldContext))
		{
			bool bFramesFollow = false;
			if (auto Error = BeginMapChange(bFramesFollow))
			{
				return Error;
			}
			if (bFramesFollow)
			{
				return std::nullopt;
			}
			if (auto Error = EndMapChange())
			{
				return Error;
			}
		}
		return AfterMapChange();
	}

	std::optional<FError> FFieldRuntime::AfterMapChange()
	{
		const EFieldRequest Program = FieldMainLoop::ProgramStarting(FieldContext);
		if (Program != EFieldRequest::None)
		{
			FFieldRequest Request;
			Request.Kind = Program;
			return LeaveField(Request);
		}
		if (!FieldMainLoop::TakesUpPlayer(FieldContext))
		{
			return EndIteration();
		}
		std::array<bool, 3> Boarding{};
		if (FieldMainLoop::TakeGearBoarding(FieldContext, bBoardingButtonsHeld, Boarding))
		{
			if (auto Error = BoardOrLeaveFieldGears(FieldContext, Boarding, true))
			{
				return Error;
			}
		}
		return IdleChecks(EStep::AfterBoarding);
	}

	std::optional<FError> FFieldRuntime::IdleChecks(EStep From)
	{
		switch (From)
		{
		case EStep::AfterBoarding:
			if (FieldMainLoop::SelectShowsPicture(FieldContext))
			{
				if (const std::optional<FFieldMapPicture> Picture = FieldMainLoop::MapPicture(FieldContext))
				{
					BeginMapPicture(*Picture);
					return std::nullopt;
				}
			}
			[[fallthrough]];
		case EStep::AfterMapPicture:
			if (FieldMainLoop::MoviePlays(FieldContext))
			{
				FieldContext.Movie.StopRequests = 0;
				FieldContext.Movie.Frame = 0;
				Phase = EPhase::MovieLead;
				return std::nullopt;
			}
			[[fallthrough]];
		case EStep::AfterMovie:
			if (FieldMainLoop::MenuOpens(FieldContext))
			{
				FieldContext.Messages.CloseAll();
				if (!FieldMainLoop::MenuStaysShut(FieldContext))
				{
					OpenMenu();
					return std::nullopt;
				}
				FieldContext.MenuRequest = FEventContext::NoMenu;
			}
			[[fallthrough]];
		case EStep::AfterMenu:
			FieldMainLoop::TakeMenuButton(FieldContext);
			break;
		default:
			break;
		}
		return EndIteration();
	}

	std::optional<FError> FFieldRuntime::EndIteration()
	{
		return PollFieldMusic(FieldContext);
	}

	std::optional<FError> FFieldRuntime::ResumeAfterRequest()
	{
		if (Phase == EPhase::MovieFirstPicture || Phase == EPhase::MoviePass || Phase == EPhase::MovieSkipFade)
		{
			return EndMovieNow();
		}
		if (Phase != EPhase::Stopped || ResumeStep != EStep::AfterMenu)
		{
			return MakeError("the field has no menu or movie to go on after");
		}
		return ResumeAfterMenu(0);
	}

	std::optional<FError> FFieldRuntime::ResumeAfterMenu(uint8 ModeResult)
	{
		if (Phase != EPhase::Stopped || ResumeStep != EStep::AfterMenu)
		{
			return MakeError("the field has no menu to go on after");
		}
		PendingRequest = {};
		BeginMenuLightening(ModeResult);
		return std::nullopt;
	}

	void FFieldRuntime::Stop(const FFieldRequest& Request, EStep ResumeAt)
	{
		PendingRequest = Request;
		ResumeStep = ResumeAt;
		Phase = EPhase::Stopped;
	}

	std::optional<FError> FFieldRuntime::LeaveField(FFieldRequest Request)
	{
		// The field saves itself for its return from a battle (unless the battle goes on to a map the
		// scripts named) or Battling, before the leave's own steps; the Field program counts the trips.
		const bool bToBattle = Request.Kind == EFieldRequest::Battle && !FieldContext.Requests.bBattleNamesNextMap;
		if (bToBattle || Request.Kind == EFieldRequest::Battling)
		{
			FFieldSnapshot Saved;
			if (auto Error = SaveFieldForReturn(FieldContext, *ActorSprites, Saved))
			{
				return Error;
			}
			SavedForReturn = std::move(Saved);
		}
		if (auto Error = FieldMainLoop::LeaveField(FieldContext, Request))
		{
			return Error;
		}
		Stop(Request, EStep::Top);
		return std::nullopt;
	}
}
