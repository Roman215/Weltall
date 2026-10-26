#include "Battling/BattlingFight.h"

#include "BattlingFightOps.h"
#include "BattlingMath.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		using namespace BattlingFightOps;
		using namespace BattlingMath;

		constexpr int32 RoundStartZ = 0x3F80;
		constexpr std::array<int32, 2> RoundStartX{16000, 0x4080};
		constexpr int32 IntroTicks = 90;
		constexpr int32 FightStartTick = 0x1E;
		constexpr int32 KnockOutTicks = 0x3C;
		/** The intro's labels: the round's name until this many ticks are left, then READY until the fight starts. */
		constexpr int32 ReadyTick = 0x3C;
		constexpr int32 SetTimeCap = 179999;
		constexpr uint16 ButtonStart = 0x800;
		constexpr uint8 ArgentoGear = 22;
		/** El-Fenrir's part of the program's own hangs on its joint 12, turned so; the joint after, the file's own, stays hidden. */
		constexpr uint8 ElFenrir = 13;
		constexpr int32 ElFenrirPartJoint = 12;
		constexpr FGteVector ElFenrirPartAngles{0, 0xC00, 0x400};
		constexpr uint8 EndingAnimation = 10;

		enum : uint8
		{
			StatePause = 0,
			StateFight = 1,
			StateBetweenRounds = 2,
			StateLeaving = 3,
			StateReplay = 4,
			StateDemonstration = 5,
			StateWinner = 6,
			StateTutorial = 7,
			StateResult = 8,
		};

		constexpr uint16 FightProjection = 0xC0;
		constexpr uint16 ReplayProjection = 0x800;
		constexpr uint16 ResultProjection = 0x200;

		constexpr int16 FireCovers = 0xFF;
		constexpr int16 FireFadeStep = 4;
		constexpr int32 WrittenTempo = 0x100;
		constexpr int32 FadeInTempo = 0x158;
		constexpr int32 WrittenMotionSpeed = 0x100;
		constexpr int32 FadeInMotionSpeed = 0x200;

		/** The fire over the bout fades a step a pass; as it ends the song and the gears go back to their own pace. */
		void FadeIn(FBattlingFight& Fight)
		{
			Fight.bFireDrawn = false;
			if (Fight.FireFade == 0)
			{
				Fight.bFireActive = false;
				return;
			}
			Fight.FireFade = static_cast<int16>(Fight.FireFade - FireFadeStep);
			if (Fight.FireFade < 1)
			{
				Fight.FireFade = 0;
				Fight.bFireActive = false;
				Fight.Sound.SetSongTempo(WrittenTempo);
				Fight.Settings.MotionSpeed = WrittenMotionSpeed;
				Fight.Fire.Free();
				return;
			}
			Fight.bFireDrawn = true;
			Fight.FireDrawnFade = Fight.FireFade;
			std::copy_n(Fight.Fire.Picture().begin(), Fight.FireDrawn.size(), Fight.FireDrawn.begin());
		}

		/** The scripted bout's ending begins once either gear is at or below 60h/FFh of its HP and neither stands in the lake. */
		bool EndingDue(FBattlingFight& Fight)
		{
			const FBattlingFighter& One = Fight.Fighters[0];
			const FBattlingFighter& Two = Fight.Fighters[1];
			return (!HpAbove(One.Hp, One.MaxHp, 0x60) || !HpAbove(Two.Hp, Two.MaxHp, 0x60)) && Fight.Settings.bScripted && !InLakeZone(Fight, One.Position)
				&& !InLakeZone(Fight, Two.Position);
		}

		void MarkDefeated(FBattlingFight& Fight, uint8 Gear)
		{
			Fight.DefeatedGears[static_cast<std::size_t>(Gear >> 3)] |= static_cast<uint8>(1u << (Gear & 7));
			for (int32 Other = 0; Other < BattlingGears::Count; ++Other)
			{
				const bool bDefeated = (Fight.DefeatedGears[static_cast<std::size_t>(Other >> 3)] & (1u << (Other & 7))) != 0;
				if (Other != ArgentoGear && !bDefeated)
				{
					return;
				}
			}
			Fight.bArgentoUnlocked = true;
			Fight.Menus.ListArgento();
		}

		void ReadQueuedPads(FBattlingFight& Fight, const FBattlingTickInput& Input)
		{
			if (!Fight.bFightStarted)
			{
				// Merged and dropped: nothing reads the fighters' pads before the fight starts.
				Fight.PadPressed = MergePads(Input)[0].Pressed;
				return;
			}
			if (Input.bOverflowed)
			{
				return;
			}
			const EBattlingMode Mode = Fight.Settings.Mode;
			for (const std::array<FBattlingPad, 2>& Blank : Input.Blanks)
			{
				const bool bStart = ((Blank[0].Pressed | Blank[1].Pressed) & ButtonStart) != 0 && Fight.IntroCounter < 0x14;
				const bool bUnplugged = Blank[0].Type == 0 || (Blank[1].Type == 0 && Mode == EBattlingMode::TwoPlayers);
				if (bStart || bUnplugged)
				{
					bool bPause = true;
					if ((Blank[0].Pressed & ButtonStart) == 0 && Blank[0].Type != 0)
					{
						if (Fight.Fighters[1].IsComputer())
						{
							bPause = false;
						}
						else
						{
							Fight.MenuPad = 1;
						}
					}
					else
					{
						Fight.MenuPad = 0;
					}
					if (bPause && !Fight.Settings.bScripted)
					{
						OpenPause(Fight, Mode == EBattlingMode::Practice ? EBattlingMenu::PracticePause : EBattlingMenu::Pause);
					}
				}
				ReadPad(Fight, Fight.Fighters[0], Blank);
				ReadPad(Fight, Fight.Fighters[1], Blank);
				Fight.PadPressed = Blank[0].Pressed;
			}
		}
	}

	namespace BattlingFightOps
	{
		void WaitBlanks(FBattlingFight& Fight)
		{
			for (int32 Blank = 0; Blank < Fight.PassBlanks; ++Blank)
			{
				++Fight.VBlank;
				if (Fight.bFireActive && (Fight.VBlank & 1) != 0)
				{
					Fight.Fire.Step(Fight.Random);
				}
			}
		}

		void FightTick(FBattlingFight& Fight, const FBattlingTickInput& Input)
		{
			if (Fight.SkipCounter != 0)
			{
				--Fight.SkipCounter;
				return;
			}
			FBattlingFighter& One = Fight.Fighters[0];
			FBattlingFighter& Two = Fight.Fighters[1];
			One.Record = Fight.RecordIndex;
			Two.Record = Fight.RecordIndex;
			++Fight.RecordIndex;
			One.Replayed = One.Record;
			Fight.SkipCounter = Fight.Settings.bPracticeFrameRateAsLabelled ? 0 : Fight.Settings.FrameSkip;
			Two.Replayed = Two.Record;
			One.Flags = (One.Flags & ~BattlingFlags::Computer) | (Fight.Settings.Computer[0] ? BattlingFlags::Computer : 0);
			Two.Flags = (Two.Flags & ~BattlingFlags::Computer) | (Fight.Settings.Computer[1] ? BattlingFlags::Computer : 0);
			UpdateProjectiles(Fight, One);
			UpdateProjectiles(Fight, Two);
			BeginTick(Fight, One);
			BeginTick(Fight, Two);
			Fight.Effects.SparkRequest = 0;
			CheckHits(Fight, One);
			CheckHits(Fight, Two);
			ReadQueuedPads(Fight, Input);
			if (Fight.bFightStarted)
			{
				if (Fight.SetTime != SetTimeCap)
				{
					++Fight.SetTime;
				}
				if (Fight.Settings.Mode == EBattlingMode::Practice)
				{
					TickPracticeAi(Fight, Two);
				}
				else
				{
					TickAi(Fight, One);
					TickAi(Fight, Two);
				}
			}
			if (Fight.bRoundOver)
			{
				One.Stick = 0;
				Two.Stick = 0;
				QueueClear(One);
				QueueClear(Two);
			}
			CheckRound(Fight);
			if (Fight.Fault != EBattlingFault::None)
			{
				return;
			}
			TickStatus(Fight, One);
			TickStatus(Fight, Two);
			ProcessCommands(Fight, One);
			ProcessCommands(Fight, Two);
			UpdateVelocity(Fight, One);
			UpdateVelocity(Fight, Two);
			Move(Fight, Two);
			Move(Fight, One);
			Separate(Fight);
			Two.Skeleton.RootAngles[1] = static_cast<int16>(Two.Yaw);
			One.Skeleton.RootAngles[1] = static_cast<int16>(One.Yaw);
			TickEnding(Fight);
			if (Fight.Tutorial.bLeft)
			{
				return;
			}
			Animate(Fight, One);
			Animate(Fight, Two);
			RecordFlags(One);
			RecordFlags(Two);
			UpdateBody(Fight, One);
			UpdateBody(Fight, Two);
			// The events' effects count on from the task's register its menus' L2 count lies in (120 in a demonstration the
			// shortcut started, where none runs).
			RunAnimationEvents(Fight, One, One.FrameBefore, One.FramesStepped, Fight.Menus.L2HeldPasses);
			RunAnimationEvents(Fight, Two, Two.FrameBefore, Two.FramesStepped, Fight.Menus.L2HeldPasses);
			TickEffects(Fight);
			if (Fight.Settings.bRubberBand)
			{
				SpawnLineEffect(Fight, One.Centre, Two.Centre, 0x13);
			}
		}

		uint8 GradeResult(const FBattlingFight& Fight, bool bPlayerOneWon)
		{
			const FBattlingFighter& One = Fight.Fighters[0];
			const FBattlingFighter& Two = Fight.Fighters[1];
			if (bPlayerOneWon)
			{
				if (HpAbove(One.Hp, One.MaxHp, 0xE0))
				{
					return 2;
				}
				return HpAbove(One.Hp, One.MaxHp, 0x10) ? 1 : 3;
			}
			if (One.AttackPresses == 0)
			{
				if (One.R1Presses == 0)
				{
					return 0x88;
				}
			}
			else if (One.R1Presses == 0)
			{
				return 0x82;
			}
			const int32 EtherLimit = One.MaxHp * 0xB0 / 0xFF;
			if (EtherLimit < One.OverheatFromEther)
			{
				return 0x83;
			}
			if (One.MaxHp * 0xA0 / 0xFF < One.OverheatFromDash)
			{
				return 0x84;
			}
			if (EtherLimit < One.OverheatFromDash + One.OverheatFromEther)
			{
				return 0x85;
			}
			if (HpAbove(Two.Hp, Two.MaxHp, 0xE0))
			{
				return 0x86;
			}
			return HpAbove(Two.Hp, Two.MaxHp, 0x10) ? 0x81 : 0x87;
		}

		std::string FormatRound(const FBattlingTexts& Texts, int32 Round)
		{
			return FormatBattlingText(Texts.RoundFormat, {std::to_string(Round)});
		}

		/**
		 * Practice's information, player 1's alone (player 2's only with a debug flag the disc leaves off), at the top left: the
		 * move it last began and its strength, and the first and last frames its attack's events fired on.
		 */
		void PrintPracticeInfo(FBattlingFight& Fight)
		{
			const FBattlingPracticeInfo& Info = Fight.Practice[0];
			const uint32 Buffer = Fight.Pass & 1;
			FBattlingLabels& Labels = Fight.Labels;
			constexpr int32 Left = 0x14;
			constexpr int32 Top = 10;
			Labels.SetPosition(Left, Top);
			const bool bEther = Info.Move == FBattlingPracticeInfo::EtherShown;
			// Ether's name is narrowed to fit before the strength.
			Labels.SetScale(bEther ? 0xE7 : FBattlingLabels::OwnSize);
			const FBattlingTexts& Texts = Fight.Tables.Texts;
			Labels.Print(bEther ? BattlingLabelTexts::Ether : std::string_view(Texts.MoveNames[Info.Move]), Buffer);
			Labels.SetScale(FBattlingLabels::OwnSize);
			Labels.SetPosition(Left + 0x34, Top);
			Labels.Print(FormatBattlingText(Texts.StrengthFormat, {std::to_string(Info.Power)}), Buffer);
			Labels.SetPosition(Left, Top + FBattlingLabels::LineHeight);
			Labels.Print(FormatBattlingText(Texts.FramesFormat, {std::to_string(Info.FirstFrame >> 4), std::to_string(Info.LastFrame >> 4)}), Buffer);
		}

		void CheckRound(FBattlingFight& Fight)
		{
			FBattlingFighter& One = Fight.Fighters[0];
			FBattlingFighter& Two = Fight.Fighters[1];
			++Fight.RoundTicks;
			const uint32 Buffer = Fight.Pass & 1;
			FBattlingLabels& Labels = Fight.Labels;
			Labels.SetPosition(0xA0, 100);
			if (EndingDue(Fight))
			{
				// The breakdown's animation loops on itself.
				Fight.AnimationKinds[EndingAnimation] = {1, EndingAnimation};
				StartEnding(Fight);
			}
			if (!Fight.bRoundOver)
			{
				const bool bOneDown = (One.Flags & BattlingFlags::KnockedOut) != 0;
				const bool bTwoDown = (Two.Flags & BattlingFlags::KnockedOut) != 0;
				if (!bOneDown && bTwoDown)
				{
					Fight.bRoundOver = true;
					Fight.bFightStarted = false;
					Fight.Winner = 0;
					Fight.MenuPad = 0;
					++One.RoundsWon;
					Fight.Result = GradeResult(Fight, true);
					if (Two.IsComputer() && Fight.Settings.Mode != EBattlingMode::ComputerVersusComputer)
					{
						MarkDefeated(Fight, Two.Gear);
					}
				}
				else if (bOneDown && !bTwoDown)
				{
					Fight.bFightStarted = false;
					++Two.RoundsWon;
					Fight.bRoundOver = true;
					Fight.Winner = 1;
					Fight.MenuPad = Two.IsComputer() ? 0 : 1;
					Fight.Result = GradeResult(Fight, false);
				}
				else if (bOneDown && bTwoDown)
				{
					Fight.bRoundOver = true;
					Fight.bFightStarted = false;
					Fight.Winner = 2;
					++Fight.Draws;
				}
			}
			else
			{
				const int32 Ticks = Fight.RoundOverTicks++;
				if (KnockOutTicks < Ticks)
				{
					SetState(Fight, Fight.Winner == 2 ? StateBetweenRounds : StateReplay);
				}
				Labels.PrintCentered(Fight.Winner == 2 ? Fight.Tables.Texts.DrawGame : Fight.Tables.Texts.KnockOut, Buffer);
			}
			if (Fight.IntroCounter != 0)
			{
				// The intro's labels: none in the tutorial, practice's own words, and the rubber band's line below.
				const bool bPractice = Fight.Settings.Mode == EBattlingMode::Practice;
				const bool bLabelled = Fight.State != StateTutorial;
				const FBattlingTexts& Texts = Fight.Tables.Texts;
				const auto PrintBand = [&](std::string_view Band) {
					if (Fight.Settings.bRubberBand)
					{
						Labels.PrintCentered("", Buffer);
						Labels.PrintCentered(Band, Buffer);
					}
				};
				if (Fight.IntroCounter < FightStartTick)
				{
					if (bLabelled && !bPractice)
					{
						Labels.PrintCentered(Texts.Fight, Buffer);
					}
					Fight.bFightStarted = true;
				}
				else if (bLabelled && Fight.IntroCounter < ReadyTick)
				{
					Labels.PrintCentered(bPractice ? Texts.Start : Texts.Ready, Buffer);
					PrintBand(bPractice ? Texts.RubberBandMode : Texts.RubberBandBattle);
				}
				else if (bLabelled)
				{
					const std::string Round = bPractice ? Texts.Practice : FormatRound(Texts, Fight.Round);
					Labels.PrintCentered(Round, Buffer);
					PrintBand(bPractice ? Texts.RubberBandMode : Texts.RubberBandBattle);
				}
				--Fight.IntroCounter;
			}
			Fight.Distance = Distance(One.Position, Two.Position);
			Fight.DistanceXZ = DistanceXZ(One.Position, Two.Position);
			Fight.FightAngle = GteRatan2(One.Position[0] - Two.Position[0], One.Position[2] - Two.Position[2], Fight.Tables.Arctangents);
		}

		void PlayFighterSound(FBattlingFight& Fight, const FBattlingFighter& Fighter, int32 Index, const FBattlingPoint* Point, EBattlingSoundPlace Place)
		{
			Fight.Sound.PlayFighter(Fight.Tables, *Fighter.SoundSet, Fighter.SideIndex(), Index, Point, Place, Fight.Random);
		}

		void StopFighterSound(FBattlingFight& Fight, const FBattlingFighter& Fighter, int32 Index)
		{
			Fight.Sound.StopFighter(Fight.Tables, *Fighter.SoundSet, Fighter.SideIndex(), Index);
		}
	}

	FBattlingFight::FBattlingFight(const FBattlingProgramTables& InTables, const FBattlingTerrain& InTerrain, FPsxRandom& InRandom,
		IBattlingSoundDriver& InDriver)
		: Tables(InTables)
		, Terrain(InTerrain)
		, Random(InRandom)
		, Sound(InDriver)
		, AnimationKinds(InTables.AnimationKinds)
		, Labels(InTables)
	{
		Fighters[1].Flags = BattlingFlags::Side;
	}

	void FBattlingFight::LoadFighter(int32 Side, int32 Gear, const FBattlingGearFile& File, const FBattlingGearStats& Stats, FMdlModel Model)
	{
		FBattlingFighter& Fighter = Fighters[static_cast<std::size_t>(Side)];
		Fighter.Flags = (Fighter.Flags & ~BattlingFlags::Side) | (Side != 0 ? BattlingFlags::Side : 0);
		Fighter.Gear = static_cast<uint8>(Gear);
		Fighter.File = &File;
		Fighter.Stats = &Stats;
		Fighter.Skeleton.Build(File, std::move(Model));
		Fighter.Players.assign(File.Animations.size() + 1, {});
		for (std::size_t Number = 0; Number < Fighter.Players.size(); ++Number)
		{
			Fighter.Players[Number].Start(File, static_cast<int32>(Number));
		}
		Fighter.Kind = 0;
		switch (Gear)
		{
		case 0x03:
		case 0x0E:
		case 0x1B:
		case 0x22:
		case 0x23:
		case 0x27:
		case 0x29:
		case 0x2A:
			Fighter.Kind |= 4;
			break;
		case ElFenrir:
			Fighter.Skeleton.Attach(ElFenrirPartJoint, Tables.ElFenrirPart, ElFenrirPartAngles);
			break;
		case 0x24:
		case 0x25:
			Fighter.Kind |= 1 | 2;
			break;
		case 0x26:
			Fighter.Kind |= 2;
			break;
		default:
			break;
		}
		Fighter.SoundSet = &Tables.SoundSetOf(Gear);
		Fighter.Colour = File.Colour;
		Fighter.FarMoves = 0;
		Fighter.NearMoves = 0;
		for (int32 Index = 0; Index < BattlingGears::Moves; ++Index)
		{
			if (Stats.Strengths[static_cast<std::size_t>(Index)] == 0)
			{
				continue;
			}
			const int32 ChainState = Settings.bRangedMovesByOwnState ? Index + 1 : Index;
			if (File.Moves[static_cast<std::size_t>(ChainState)].FromAfar == 0)
			{
				++Fighter.NearMoves;
			}
			else
			{
				++Fighter.FarMoves;
			}
		}
	}

	void FBattlingFight::StartRound()
	{
		FBattlingFighter& One = Fighters[0];
		FBattlingFighter& Two = Fighters[1];
		One.CameraOffset = 0x400;
		Two.CameraOffset = -0x400;
		ResetFighter(*this, One);
		ResetFighter(*this, Two);
		ClearSparks(*this);
		ClearSplashesAndDust(*this);
		ClearEffects(*this);
		StartCamera(*this);
		CameraSide = 1;
		CameraSideAngle = -1;
		bRoundOver = false;
		RoundOverTicks = 0;
		Winner = 0;
		IntroCounter = IntroTicks;
		RoundTicks = 0;
		Effects.SparkRequest = 0;
		Result = 0;
		Sound.Reset();
		Menus.InvalidateHelp();
		bEndingActive = false;
		bFightStarted = false;
		AnimationKinds[EndingAnimation] = {0, 0};
		SkipCounter = 0;
		++Round;
		Tutorial.Text.Clear();
		Settings.FrameSkip = 0;
		if (Settings.bFromMenus)
		{
			const int32 Cycle = (Round - 1) % 5;
			Lighting = Cycle == 3 ? 1 : (Cycle == 4 ? 2 : 0);
		}
		for (int32 Side = 0; Side < 2; ++Side)
		{
			Fighters[static_cast<std::size_t>(Side)].Position = {RoundStartX[static_cast<std::size_t>(Side)], 0, RoundStartZ};
		}
	}

	void FBattlingFight::InitSparkTiles()
	{
		BattlingFightOps::InitSparkTiles(*this);
	}

	void FBattlingFight::InitSplashTiles()
	{
		BattlingFightOps::InitSplashTiles(*this);
	}

	void FBattlingFight::LightFire()
	{
		FireFade = FireCovers;
		bFireActive = false;
		Fire.Init();
	}

	void FBattlingFight::PutOutFire()
	{
		FireFade = 0;
		bFireActive = false;
		Fire.Free();
	}

	void FBattlingFight::StartFadeIn()
	{
		if (FireFade == 0)
		{
			return;
		}
		bFireActive = true;
		Sound.SetSongTempo(FadeInTempo);
		Settings.MotionSpeed = FadeInMotionSpeed;
	}

	FGteMatrix FBattlingFight::DisplayAspect() const
	{
		FGteMatrix Aspect = FGteMatrix::Identity();
		Aspect.M[0][0] = static_cast<int16>(((DisplayWidth << 12) / 0x140) * DisplayHeight / DisplayWidth);
		Aspect.M[1][1] = static_cast<int16>(((DisplayHeight * 0x1000) / 0xF0) * DisplayHeight / DisplayWidth);
		return Aspect;
	}

	FGte FBattlingFight::View() const
	{
		// The camera looks from the eye at the target with up along y, the world moved by the target; then the display's
		// aspect scales the axes.
		const FGteLongVector Relative{static_cast<int16>(Eye[0] - Target[0]), static_cast<int16>(Eye[1] - Target[1]), static_cast<int16>(Eye[2] - Target[2])};
		const FGteLongVector Forward = VectorNormal({-Relative[0], -Relative[1], -Relative[2]});
		const FGteLongVector Right = VectorNormal(OuterProduct12({0, 0x1000, 0}, Forward));
		const FGteLongVector Up = VectorNormal(OuterProduct12(Forward, Right));
		FGteMatrix Matrix;
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Matrix.M[0][Axis] = static_cast<int16>(Right[Axis]);
			Matrix.M[1][Axis] = static_cast<int16>(Up[Axis]);
			Matrix.M[2][Axis] = static_cast<int16>(Forward[Axis]);
		}
		const FGteLongVector Moved = ApplyMatrix(Matrix, {static_cast<int16>(Relative[0]), static_cast<int16>(Relative[1]), static_cast<int16>(Relative[2])});
		Matrix = MulMatrix(DisplayAspect(), Matrix);
		Matrix.T = {-Moved[0], -Moved[1], -Moved[2]};
		FGte Gte;
		Gte.Transform = Matrix;
		Gte.OffsetX = (DisplayWidth / 2) << 16;
		Gte.OffsetY = (DisplayHeight / 2) << 16;
		Gte.ProjectionDistance = ProjectionDistance;
		return Gte;
	}

	void FBattlingFight::RunPass(const FBattlingTickInput& Input)
	{
		++Pass;
		bFrameDrawn = false;
		bWinnerDrawn = false;
		bPauseDrawn = false;
		bMenusDrawn = false;
		VibrationCount = 0;
		Menus.bCaptureAfterResume = false;
		Menus.HelpTyped = {};
		PassBlanks = 2;
		if (bWinnerEnding)
		{
			EndWinner(*this);
			Sound.Poll();
			WaitBlanks(*this);
			return;
		}
		LatchedState = State;
		if (Fault != EBattlingFault::None || Tutorial.bLeft)
		{
			return;
		}
		FadeIn(*this);
		Tutorial.bPointerDrawn = false;
		Tutorial.bMenuDrawn = false;
		if (Text != nullptr)
		{
			Tutorial.TextDrawnY = Tutorial.TextY;
			Tutorial.Text.Update(*Text);
		}
		if (LatchedState == StateBetweenRounds)
		{
			// The set ends when a side reaches the wins it needs: a human's win shows the winner screen, the computer's leaves.
			const int32 Needed = Settings.WinsPerSet;
			const bool bOneDone = Fighters[0].RoundsWon == Needed;
			const bool bTwoDone = Fighters[1].RoundsWon == Needed;
			if (Needed == 0 || (!bOneDone && !bTwoDone))
			{
				SetState(*this, StateFight);
				StartRound();
				LatchedState = StateFight;
			}
			else
			{
				SetState(*this, Settings.Computer[bOneDone ? 0 : 1] ? StateLeaving : StateWinner);
				Sound.Poll();
				WaitBlanks(*this);
				return;
			}
		}
		switch (LatchedState)
		{
		case StatePause:
		{
			TickMenus(*this, Input);
			bPauseDrawn = true;
			const std::span<const FBattlingLabelGlyph> Letters = Labels.Take(Pass & 1);
			FrameLabels.assign(Letters.begin(), Letters.end());
			Sound.Poll();
			PassBlanks = 1;
			WaitBlanks(*this);
			return;
		}
		case StateFight:
			FightTick(*this, Input);
			if (Tutorial.bLeft)
			{
				return;
			}
			if (Settings.bPracticeFrameRateAsLabelled)
			{
				PassBlanks = static_cast<uint8>(2 + Settings.FrameSkip);
			}
			ProjectionDistance = FightProjection;
			if (Fault == EBattlingFault::None && State == StateFight)
			{
				FollowFight(*this);
			}
			break;
		case StateTutorial:
			TickTutorial(*this, Input);
			ProjectionDistance = FightProjection;
			// Its last pass waits a vertical blank before the program leaves or goes back to its menus.
			if (State == StateLeaving)
			{
				PassBlanks = 1;
			}
			break;
		case StateDemonstration:
			// A press on pad 1 ends it, the menus again from the next pass; its pads merged, the fight reads none of them.
			if (MergePads(Input)[0].Pressed != 0)
			{
				Settings.Computer = {false, false};
				State = StateLeaving;
				PassBlanks = 1;
				WaitBlanks(*this);
				return;
			}
			FightTick(*this, FBattlingTickInput{});
			ProjectionDistance = FightProjection;
			FollowFight(*this);
			break;
		case StateLeaving:
			// From the arena's menus a set's end goes back to them, within the pass.
			if (Settings.bFromMenus)
			{
				StartMenus();
				if (!CountMenusDown(*this))
				{
					RunMenusLoop(*this, Input);
				}
			}
			return;
		case StateReplay:
			ProjectionDistance = ReplayProjection;
			TickReplay(*this, Input);
			if (Fault == EBattlingFault::None && State != StateResult)
			{
				ReplayCamera(*this, true);
			}
			break;
		case StateResult:
			ProjectionDistance = ResultProjection;
			TickResult(*this, Input);
			break;
		case StateWinner:
			TickWinner(*this, Input);
			if (!bWinnerEnding)
			{
				bWinnerDrawn = true;
				const std::span<const FBattlingLabelGlyph> Letters = Labels.Take(Pass & 1);
				FrameLabels.assign(Letters.begin(), Letters.end());
			}
			Sound.Poll();
			WaitBlanks(*this);
			return;
		default:
			return;
		}
		if (Fault != EBattlingFault::None)
		{
			return;
		}
		bFrameDrawn = true;
		FrameVBlank = VBlank;
		if (Settings.Mode == EBattlingMode::Practice)
		{
			PrintPracticeInfo(*this);
		}
		const std::span<const FBattlingLabelGlyph> Taken = Labels.Take(Pass & 1);
		FrameLabels.assign(Taken.begin(), Taken.end());
		const bool bDemonstration = LatchedState == StateDemonstration;
		if (bDemonstration)
		{
			// The demonstration's drawing clears the labels unprinted.
			FrameLabels.clear();
		}
		if (Labels.Overflowed())
		{
			Fault = EBattlingFault::LabelsOverflowed;
			return;
		}
		// Drawing each gear fades its hit light, then builds its joints' matrices.
		for (FBattlingFighter& Fighter : Fighters)
		{
			const uint8 Flash = Fighter.HitFlash;
			Fighter.HitLight = Flash;
			if (Flash != 0)
			{
				Fighter.HitFlash = static_cast<uint8>(Flash - 0x18);
				if (Flash < Fighter.HitFlash)
				{
					Fighter.HitFlash = 0;
				}
			}
			Fighter.Skeleton.Pose();
		}
		if (!bDemonstration)
		{
			Sound.UpdatePositions(View(), Target);
		}
		StepDrawnEffects(*this, !bDemonstration);
		Sound.Poll();
		WaitBlanks(*this);
	}
}
