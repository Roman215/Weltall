#include "Field/EventMachine.h"
#include "Field/FieldMessages.h"
#include "Field/FieldRuntime.h"
#include "Field/FieldTick.h"
#include "Input/KernelPad.h"

namespace Weltall
{
	namespace
	{
		constexpr int32 MainLoopStrayWait = 0xFF;
		/** L1, R1, Start and Select together start the console again. */
		constexpr uint16 SoftResetButtons = 0x090C;
		/** The logical button that skips a skippable movie: physical Cross on the US discs. */
		constexpr uint16 SkipButton = 0x0020;
	}

	std::optional<FError> FFieldRuntime::TickMovie(std::span<const FPadState> HeldPerVblank)
	{
		LastFrameKind = EFieldFrame::Movie;
		switch (Phase)
		{
		case EPhase::MovieFirstPicture:
			// The movie's first picture: its decode steps check the soft reset and set the frame.
			RunVblanks(HeldPerVblank);
			if (IsSoftReset())
			{
				return std::nullopt;
			}
			FieldContext.Movie.Frame = NextMovieFrame;
			Phase = EPhase::MoviePass;
			return std::nullopt;
		case EPhase::MoviePass:
			return RunMoviePass(HeldPerVblank);
		default:
			// The skip's fade: the program only waits for the sound to go.
			RunVblanks(HeldPerVblank);
			return EndMovieNow();
		}
	}

	std::optional<FError> FFieldRuntime::RunMoviePass(std::span<const FPadState> HeldPerVblank)
	{
		FEventContext& Context = FieldContext;
		switch (Context.MovieView)
		{
		case EFieldMovieView::FullScreen:
			// Each vertical blank is followed by decode steps, which check the soft reset first.
			for (std::size_t Vblank = 0; Vblank < HeldPerVblank.size(); ++Vblank)
			{
				RunVblanks(HeldPerVblank.subspan(Vblank, 1));
				if (IsSoftReset())
				{
					return std::nullopt;
				}
			}
			break;
		case EFieldMovieView::OverField:
			// The first actor's scripts and the message windows before the pass's vertical blank, over the picture.
			Context.DrawBuffer ^= 1;
			ReadFieldInput(Context, TakeInput().Pad);
			if (auto Error = FEventMachine::Update(Context))
			{
				return Error;
			}
			Context.Messages.Tick();
			UpdateFieldMessages(Context, Context.Controls.Pressed, Context.Controls.Repeated, VblanksPerTick());
			RunVblanks(HeldPerVblank);
			if (IsSoftReset())
			{
				return std::nullopt;
			}
			break;
		case EFieldMovieView::InScene:
			// The whole tick, without the music's poll or the main loop's requests.
			LastFrameKind = EFieldFrame::Tick;
			RunVblanks(HeldPerVblank);
			if (auto Error = RunFrame(MainLoopStrayWait, false))
			{
				return Error;
			}
			if (IsSoftReset())
			{
				return std::nullopt;
			}
			break;
		}
		Context.Movie.Frame = NextMovieFrame;
		return CheckMovieEnd();
	}

	std::optional<FError> FFieldRuntime::CheckMovieEnd()
	{
		FEventContext& Context = FieldContext;
		const FFieldMovieRequest& Movie = Context.Requests.Movie;
		if ((Movie.Flags & FFieldMovieRequest::Skippable) != 0)
		{
			ReadFieldInput(Context, TakeInput().Pad);
			if ((Context.Controls.Repeated & SkipButton) != 0)
			{
				Phase = EPhase::MovieSkipFade;
				return std::nullopt;
			}
		}
		const bool bStopped = Context.MovieView == EFieldMovieView::InScene && Context.Movie.StopRequests != 0;
		if (bStopped || (!Movie.bRewinds && Context.Movie.Frame >= Movie.EndFrame))
		{
			return EndMovieNow();
		}
		return std::nullopt;
	}

	std::optional<FError> FFieldRuntime::EndMovieNow()
	{
		if (auto Error = EndMovie())
		{
			return Error;
		}
		PendingRequest = {};
		Phase = EPhase::MainLoop;
		return IdleChecks(EStep::AfterMovie);
	}

	bool FFieldRuntime::IsSoftReset()
	{
		if (Services.Pad->Held() != SoftResetButtons)
		{
			return false;
		}
		FFieldRequest Reset;
		Reset.Kind = EFieldRequest::SoftReset;
		Stop(Reset, EStep::Top);
		return true;
	}
}
