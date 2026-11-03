#include "Support/GameLoopSession.h"

#include "Media/DiscIdentification.h"
#include "Timing/PsxTiming.h"

#include <catch_amalgamated.hpp>

#include <vector>

namespace Weltall::Testing
{
	std::unique_ptr<FGameLoopSession> FGameLoopSession::Start(const FIso9660& Disc)
	{
		auto Game = std::make_unique<FGameLoopSession>();
		auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		Game->Executable = std::make_unique<FPsxExecutable>(std::move(*Executable));
		auto Index = FGameFileIndex::Read(*Game->Executable);
		REQUIRE(Index);
		Game->Index = std::make_unique<FGameFileIndex>(std::move(*Index));
		const auto Identity = IdentifyDisc(Disc);
		REQUIRE(Identity);
		auto Sound = FGameSoundSession::Start(Disc);
		if (!Sound)
		{
			FAIL(Sound.Error().Message);
		}
		Game->Sound = std::move(*Sound);
		Game->Unheard = std::make_unique<FUnheardGameSound>(*Game->Sound->Sound());
		auto Session = FGameSession::PowerOn(Disc.Image(), *Game->Index, *Game->Executable, *Identity, *Game->Sound);
		if (!Session)
		{
			FAIL(Session.Error().Message);
		}
		Game->Session = std::move(*Session);
		// The tests play as the original does unless they say otherwise: the run button runs, doors open for the button
		// alone, a direction held walks on into the next map, the camera follows as the game's, the gears are drawn as
		// their sprites.
		FPlayerSettings Settings = Game->Session->Settings();
		Settings.Controls.bAlwaysRun = false;
		Settings.Controls.bWalkIntoDoors = false;
		Settings.Controls.DoorPause = NoDoorPause;
		Settings.Camera.bSteadyFollow = false;
		Settings.Field.GearModels = EFieldGearModels::Off;
		Game->Session->SetSettings(Settings);
		return Game;
	}

	void FGameLoopSession::AddDisc(const FIso9660& Disc)
	{
		auto OtherExecutable = ReadBootExecutable(Disc);
		REQUIRE(OtherExecutable);
		auto Other = FGameFileIndex::Read(*OtherExecutable);
		REQUIRE(Other);
		OtherIndex = std::make_unique<FGameFileIndex>(std::move(*Other));
		const auto Identity = IdentifyDisc(Disc);
		REQUIRE(Identity);
		Session->AddDisc(DiscNumberOfRelease(Identity->Release) - 1, Disc.Image(), *OtherIndex);
	}

	void FGameLoopSession::PowerOn()
	{
		auto Started = FGameLoop::PowerOn(*Session, *Executable);
		if (!Started)
		{
			FAIL(Started.Error().Message);
		}
		Loop = std::move(*Started);
	}

	void FGameLoopSession::EnterField(uint16 Map)
	{
		auto Started = FGameLoop::EnterField(*Session, Map);
		if (!Started)
		{
			FAIL(Started.Error().Message);
		}
		Loop = std::move(*Started);
	}

	std::optional<FError> FGameLoopSession::Frame(FPadState Held)
	{
		const std::vector<FPadState> PerVblank(static_cast<std::size_t>(Loop->VblanksPerFrame()), Held);
		Unheard->Advance(PsxTiming::FrameSeconds * static_cast<double>(PerVblank.size()));
		++Frames;
		Vblanks += static_cast<int32>(PerVblank.size());
		return Loop->Frame(PerVblank);
	}

	int32 FGameLoopSession::RunUntil(const std::function<bool()>& Done, int32 Limit, FPadState Held, bool bStopExpected)
	{
		for (int32 Run = 0; Run < Limit; ++Run)
		{
			if (Done())
			{
				return Run;
			}
			if (auto Error = Frame(Held))
			{
				if (!bStopExpected)
				{
					FAIL("frame " << Frames << ": " << Error->Message);
				}
				return Run + 1;
			}
		}
		FAIL("not done after " << Limit << " frames");
		return Limit;
	}
}
