#include "Game/BattleSpeedMark.h"
#include "Game/GameSession.h"
#include "Input/KernelVibration.h"
#include "Game/GameState.h"
#include "Misc/AutomationTest.h"
#include "OffscreenRender.h"
#include "RenderingThread.h"
#include "Tests/GameLoopTestGame.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A picture twice the game's screen. */
	const FIntPoint BattleSpeedTestSize(640, 448);
	/** Map 15: a forest, its formation 1. */
	constexpr uint16 BattleSpeedTestMap = 15;
	constexpr uint8 BattleSpeedTestFormation = 1;
	constexpr Weltall::FPadState BattleSpeedTestL1{Weltall::FPadState::Bit(Weltall::EPadButton::L1)};
	constexpr Weltall::FPadState BattleSpeedTestR1{Weltall::FPadState::Bit(Weltall::EPadButton::R1)};

	/** The game on map 15 with a party of three. */
	struct FBattleSpeedTestGame : FGameLoopTestGame
	{
		/** The screen's session, its while the console is not restarted. */
		Weltall::FGameSession* Session = nullptr;

		bool Start(const Weltall::FIso9660& Disc)
		{
			return !FGameLoopTestGame::Start(Disc, BattleSpeedTestMap, [this](Weltall::FGameSession& Staged)
			{
				Session = &Staged;
				Staged.Game().SetParty({0, 1, 2});
				Staged.Game().SetJoinedCharacters(0x1FF);
			});
		}

		/** Seconds of display frames with Held down; returns how far the play time went through them. */
		uint32 Run(double Seconds, Weltall::FPadState Held = {})
		{
			const uint32 Before = Screen->GameState()->PlayTime();
			FGameLoopTestGame::Run(Seconds, Held);
			return Screen->GameState()->PlayTime() - Before;
		}

		/**
		 * The display frames 240 simulated vertical blanks take, measured by the first port's motor countdown.
		 */
		int32 FramesOf240Vblanks()
		{
			Session->Vibration().Start(0, 240);
			// The motor comes on at the next vertical blank the battle counts: a display frame away, or two in its transition.
			for (int32 Waited = 0; !Screen->Motors()[0] && Waited < 4; ++Waited)
			{
				Frame();
			}
			int32 Frames = 0;
			for (; Screen->Motors()[0] && Frames < 1000; Frame())
			{
				++Frames;
			}
			return Frames;
		}

		/** A button pressed for a moment and let go. */
		void Press(Weltall::FPadState Button)
		{
			Run(0.1, Button);
			Run(0.1);
		}

		/** The picture's pixels under the mark's screen, a row after another; none when the picture cannot be read. */
		TArray<FColor> MarkPixels() const
		{
			const TArray<FColor> Pixels = RenderScreenFrameOffscreen(*Screen->CaptureFrame(), BattleSpeedTestSize);
			FlushRenderingCommands();
			TArray<FColor> Mark;
			if (Pixels.Num() == BattleSpeedTestSize.X * BattleSpeedTestSize.Y)
			{
				const Weltall::FDisplayRect Place = Weltall::PlaceBattleSpeedMark(BattleSpeedTestSize.X, BattleSpeedTestSize.Y);
				for (int32 Row = 0; Row < Place.Height; ++Row)
				{
					Mark.Append(&Pixels[(Place.Y + Row) * BattleSpeedTestSize.X + Place.X], Place.Width);
				}
			}
			return Mark;
		}
	};

	/** Whether the mark's pixels show its arrowheads: white inside the first, dark at its left edge (two pixels a line). */
	bool ShowsArrows(const TArray<FColor>& Mark)
	{
		const int32 Width = Weltall::BattleSpeedMarkScreen.Width * 2;
		if (Mark.Num() != Width * Weltall::BattleSpeedMarkScreen.Height * 2)
		{
			return false;
		}
		const FColor Inside = Mark[10 * Width + 22];
		const FColor Edge = Mark[10 * Width + 16];
		return Inside.R > 240 && Inside.G > 240 && Inside.B > 240 && Edge.R < 32 && Edge.G < 32 && Edge.B < 32;
	}

	/** The white pixels of the mark's lower half, where its speed is written. */
	TArray<bool> SpeedText(const TArray<FColor>& Mark)
	{
		TArray<bool> White;
		for (int32 Pixel = Mark.Num() / 2; Pixel < Mark.Num(); ++Pixel)
		{
			White.Add(Mark[Pixel].R > 240 && Mark[Pixel].G > 240 && Mark[Pixel].B > 240);
		}
		return White;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameLoopBattleSpeedTest, "Weltall.Screens.ABattleRunsAtTheSpeedItsButtonsStepAndMarksIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameLoopBattleSpeedTest::RunTest(const FString&)
{
	Weltall::FGameLibrary Library;
	const Weltall::FIso9660* Disc = FindTestDisc(Library);
	if (Disc == nullptr)
	{
		AddInfo(TEXT("Skipped: the first disc is not in the Games folder"));
		return true;
	}
	FBattleSpeedTestGame Game;
	if (!TestTrue(TEXT("The game enters the map"), Game.Start(*Disc)))
	{
		return false;
	}
	TArray<Weltall::EBattleSpeed> Kept;
	Game.Screen->SetBattleSpeedStepped([&Kept](Weltall::EBattleSpeed Speed) { Kept.Add(Speed); });
	// 240 vertical blanks at a 60 Hz display and the console's 59.8 a second: 241 frames at the game's pace.
	const auto IsAbout = [](int32 Frames, int32 Times) { return Frames >= 238 / Times && Frames <= 244 / Times; };
	// Two seconds of the play time, whatever the pace.
	const auto IsTwoSeconds = [](uint32 PlayTime) { return PlayTime >= 118 && PlayTime <= 121; };

	// On the field R1 turns the camera: the game keeps its pace, unmarked.
	Game.Run(3.0);
	Game.Press(BattleSpeedTestR1);
	TestTrue(TEXT("Two seconds on the field"), IsTwoSeconds(Game.Run(2.0)));
	TestFalse(TEXT("The mark on the field"), ShowsArrows(Game.MarkPixels()));
	TestEqual(TEXT("Steps kept from the field"), Kept.Num(), 0);

	// In the battle each press of R1 is a step: twice the vertical blanks, then four times, the mark saying which,
	// while the play time goes on counting the player's seconds.
	TestTrue(TEXT("The field starts the battle"), Game.Screen->RequestBattle(BattleSpeedTestFormation, 0));
	// The field takes about a second to hand over to the Battle program, whose transition is then under way.
	Game.Run(2.0);
	TestTrue(TEXT("The pace of the battle"), IsAbout(Game.FramesOf240Vblanks(), 1));
	TestFalse(TEXT("The mark at the game's pace"), ShowsArrows(Game.MarkPixels()));
	Game.Press(BattleSpeedTestR1);
	TestTrue(TEXT("Twice the pace"), IsAbout(Game.FramesOf240Vblanks(), 2));
	TestTrue(TEXT("Two seconds at twice the pace"), IsTwoSeconds(Game.Run(2.0)));
	const TArray<FColor> Twice = Game.MarkPixels();
	TestTrue(TEXT("The mark at twice the pace"), ShowsArrows(Twice));
	Game.Press(BattleSpeedTestR1);
	TestTrue(TEXT("Four times the pace"), IsAbout(Game.FramesOf240Vblanks(), 4));
	TestTrue(TEXT("Two seconds at four times the pace"), IsTwoSeconds(Game.Run(2.0)));
	const TArray<FColor> FourTimes = Game.MarkPixels();
	TestTrue(TEXT("The mark at four times the pace"), ShowsArrows(FourTimes));
	TestTrue(TEXT("The mark's speed is written"), SpeedText(Twice).Contains(true) && SpeedText(FourTimes).Contains(true));
	TestTrue(TEXT("The mark's speed differs"), SpeedText(Twice) != SpeedText(FourTimes));
	TestTrue(TEXT("The steps are kept"), Kept == TArray<Weltall::EBattleSpeed>{Weltall::EBattleSpeed::Double, Weltall::EBattleSpeed::Quadruple});

	// L1 steps back to the game's own pace, and the mark goes.
	Game.Press(BattleSpeedTestL1);
	TestTrue(TEXT("Back at twice the pace"), IsAbout(Game.FramesOf240Vblanks(), 2));
	Game.Press(BattleSpeedTestL1);
	TestTrue(TEXT("Back at the game's pace"), IsAbout(Game.FramesOf240Vblanks(), 1));
	TestFalse(TEXT("The mark back at the game's pace"), ShowsArrows(Game.MarkPixels()));
	TestTrue(TEXT("The settings' speed"), Game.Screen->Settings().BattleSpeed == Weltall::EBattleSpeed::Normal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameLoopExplorationSpeedTest, "Weltall.Screens.OptInFieldFastForwardUsesTheBattleStepsAndIndicator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameLoopExplorationSpeedTest::RunTest(const FString&)
{
	Weltall::FGameLibrary Library;
	const Weltall::FIso9660* Disc = FindTestDisc(Library);
	if (Disc == nullptr)
	{
		AddInfo(TEXT("Skipped: the first disc is not in the Games folder"));
		return true;
	}
	FBattleSpeedTestGame Game;
	if (!TestTrue(TEXT("The game enters the map"), Game.Start(*Disc)))
	{
		return false;
	}
	Game.Run(3.0);
	TestFalse(TEXT("The camera buttons keep their ordinary use by default"), Game.Screen->UsesShouldersForSpeed());
	Weltall::FPlayerSettings Settings = Game.Screen->Settings();
	Settings.Balance.bExplorationFastForward = true;
	Game.Screen->ApplySettings(Settings);
	TestTrue(TEXT("The toggle assigns the camera buttons to speed"), Game.Screen->UsesShouldersForSpeed());
	TestFalse(TEXT("Enabling the toggle starts at normal speed"), ShowsArrows(Game.MarkPixels()));

	Game.Press(BattleSpeedTestR1);
	const int32 Twice = Game.FramesOf240Vblanks();
	TestTrue(TEXT("The first press doubles the field rate"), Twice >= 118 && Twice <= 122);
	TestTrue(TEXT("The field shows the speed indicator"), ShowsArrows(Game.MarkPixels()));
	Game.Press(BattleSpeedTestR1);
	const int32 FourTimes = Game.FramesOf240Vblanks();
	TestTrue(TEXT("The second press quadruples the field rate"), FourTimes >= 59 && FourTimes <= 62);
	const uint32 PlayTime = Game.Run(2.0);
	TestTrue(TEXT("The play time still counts real seconds"), PlayTime >= 118 && PlayTime <= 121);
	TestTrue(TEXT("The battle setting is independent"), Game.Screen->Settings().BattleSpeed == Weltall::EBattleSpeed::Normal);

	Game.Press(BattleSpeedTestL1);
	TestTrue(TEXT("One step back still shows fast-forward"), ShowsArrows(Game.MarkPixels()));
	Game.Press(BattleSpeedTestL1);
	TestFalse(TEXT("The mark is gone at normal speed"), ShowsArrows(Game.MarkPixels()));
	Game.Press(BattleSpeedTestR1);
	Settings.Balance.bExplorationFastForward = false;
	Game.Screen->ApplySettings(Settings);
	TestFalse(TEXT("Turning off the toggle restores the camera buttons"), Game.Screen->UsesShouldersForSpeed());
	Settings.Balance.bExplorationFastForward = true;
	Game.Screen->ApplySettings(Settings);
	TestFalse(TEXT("Re-enabling starts at normal speed"), ShowsArrows(Game.MarkPixels()));
	return true;
}

#endif
