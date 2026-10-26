#include "BattlingFightOps.h"
#include "BattlingMath.h"

namespace Weltall::BattlingFightOps
{
	namespace
	{
		using namespace BattlingMath;

		enum : uint8
		{
			StateFight = 1,
			StateBetweenRounds = 2,
			StateLeaving = 3,
			StateReplay = 4,
			StateWinner = 6,
			StateResult = 8,
		};

		constexpr int32 FullReplay = 0xFF;
		constexpr int32 ResultTicks = 0x96;
		constexpr int32 VictoryFromTick = 0x78;
		constexpr int32 OrbitRadius = 0xC00;
		constexpr int32 ResultReach = 0x3D00;
		constexpr uint8 VictoryAnimation = 16;
		constexpr uint8 DownAnimation = 9;
		constexpr uint8 Siebzehn = 7;
		constexpr uint16 ButtonL2 = 0x01;
		constexpr uint16 ButtonConfirm = 0x20;
		constexpr uint16 ButtonRight = 0x2000;
		constexpr uint16 ButtonLeft = 0x8000;
		constexpr int32 FighterProjection = 0xC0;
		constexpr int32 ResultProjection = 0x200;
		constexpr int32 WinnerProjection = 0x400;
		constexpr int32 WinnerWidth = 0x280;
		constexpr int32 FightWidth = 0x140;
		/** The winner screen's text: its scale, its two columns and its top, lower without the level's line. */
		constexpr int32 WinnerTextScale = 0x1C0;
		constexpr int32 WinnerHeadingsX = 0x18;
		constexpr int32 WinnerValuesX = 0xA8;
		constexpr int32 WinnerTextTop = 0x86;
		constexpr int32 WinnerTextTopWithoutLevel = 0x9A;
		/** The set's time in ticks, 30 a second. */
		constexpr int32 TicksPerMinute = 0x708;
		constexpr int32 TicksPerSecond = 0x1E;

		FBattlingFighter& Winner(FBattlingFight& Fight)
		{
			return Fight.Fighters[Fight.Winner == 0 ? 0 : 1];
		}

		void StartReplay(FBattlingFight& Fight, int32 Ticks)
		{
			Fight.ReplayTicks = Ticks - 2;
			if (Ticks < FullReplay)
			{
				Fight.RecordIndex = static_cast<uint8>(Fight.RecordIndex - Ticks);
			}
			else
			{
				Fight.ReplayTicks = FullReplay;
			}
			for (std::size_t Side = 0; Side < 2; ++Side)
			{
				const FBattlingFighter& Fighter = Fight.Fighters[Side];
				Fight.ReplayPositions[Side] = {Fighter.Position[0], 0x100, Fighter.Position[2]};
				Fight.ReplayBases[Side] = {Fighter.Base[0], 0x100, Fighter.Base[2]};
			}
		}

		/** The result screen's camera on the winner: its gear's angle and heights, from a random side. */
		void StartResultCamera(FBattlingFight& Fight, const FBattlingFighter& Fighter)
		{
			const FBattlingGearFile& File = *Fighter.File;
			Fight.ResultAngle = File.ResultAngle;
			Fight.ResultTargetHeight = File.ResultTargetHeight;
			Fight.ResultEyeHeight = File.ResultEyeHeight;
			Fight.ResultDistance = File.ResultDistance;
			Fight.ProjectionDistance = ResultProjection;
			const int32 Angle = Fight.Random.Next();
			Fight.Target = Fighter.Position;
			Fight.Eye[1] = (Fight.Target[1] - 0x400) - Fight.ResultEyeHeight;
			Fight.Eye[0] = Fight.Target[0] + (GteSin(Angle) * 4 * Fight.ResultDistance >> 12);
			Fight.Eye[2] = Fight.Target[2] + (GteCos(Angle) * 4 * Fight.ResultDistance >> 12);
		}

		void StartResult(FBattlingFight& Fight)
		{
			Fight.ReplayTicks = ResultTicks;
			Fight.Settings.MotionSpeed = 0x100;
			Fight.ProjectionDistance = ResultProjection;
			for (std::size_t Side = 0; Side < 2; ++Side)
			{
				FBattlingFighter& Fighter = Fight.Fighters[Side];
				Fighter.Position = Fight.ReplayPositions[Side];
				Fighter.Base = Fight.ReplayBases[Side];
				Fighter.PreviousBase = Fight.ReplayBases[Side];
				Fighter.PreviousPosition = Fight.ReplayPositions[Side];
			}
			StartResultCamera(Fight, Fight.Fighters[Fight.Winner == 0 ? 0 : 1]);
		}

		void StartWinner(FBattlingFight& Fight, int32 Side)
		{
			FBattlingFighter& Fighter = Fight.Fighters[static_cast<std::size_t>(Side)];
			Fight.DisplayWidth = WinnerWidth;
			Fight.ProjectionDistance = WinnerProjection;
			Fight.WinnerSide = Side;
			ResetFighter(Fight, Fighter);
			const FBattlingGearFile& File = *Fighter.File;
			Fight.WinnerText = 1;
			Fight.WinnerPitch = File.WinnerPitch;
			Fight.WinnerSpin = File.WinnerSpin;
			Fight.WinnerCameraHeight = File.WinnerCameraHeight;
			Fight.WinnerCameraDistance = File.WinnerCameraDistance;
			Fight.WinnerScale = File.WinnerScale;
			Fight.WinnerYaw = File.WinnerYaw;
		}

		/** A gear set back as the replay's record says: its place and yaw from this tick's record, its animation from the next. */
		void ReplayFighter(FBattlingFight& Fight, FBattlingFighter& Fighter)
		{
			const FBattlingReplayRecord& Record = Fighter.Records[Fighter.Record];
			const FBattlingReplayRecord& Replayed = Fighter.Records[Fighter.Replayed];
			Fighter.Position = {Record.X, Record.Y, Record.Z};
			Fighter.Animation = Replayed.Animation;
			Fighter.Yaw = static_cast<int32>(static_cast<uint32>(Record.YawFlags) << 20) >> 20;
			FBattlingAnimationPlayer& Player = Fighter.Player(Replayed.Animation);
			if ((Replayed.YawFlags & 0x1000) != 0)
			{
				ResetParts(Fighter);
				Player.Start(*Fighter.File, Replayed.Animation);
			}
			RunAnimationEvents(Fight, Fighter, Player.Frame(), Fighter.Records[Fighter.Replayed].Frames, 0);
			if (Replayed.Blend != 0)
			{
				Player.Step(*Fighter.File, Fighter.Skeleton, Replayed.Frames, Replayed.Blend);
			}
		}

		/** The guard, the dash and the back-dash's dust as the record kept them. */
		void LoadRecordFlags(FBattlingFighter& Fighter)
		{
			const FBattlingReplayRecord& Record = Fighter.Records[Fighter.Record];
			uint32 Flags = Fighter.Flags & ~(BattlingFlags::GuardActive | BattlingFlags::DashInput | BattlingFlags::BackDashDust);
			Flags |= Record.bGuard ? BattlingFlags::GuardActive : 0;
			Flags |= (Record.YawFlags & 0x2000) != 0 ? BattlingFlags::DashInput : 0;
			Flags |= (Record.YawFlags & 0x4000) != 0 ? BattlingFlags::BackDashDust : 0;
			Fighter.Flags = Flags;
		}

		void RandomOrbit(FBattlingFight& Fight)
		{
			Fight.OrbitAngle = Fight.Random.Next();
			Fight.OrbitSpeed = Fight.Random.Next() % 12 + 4;
			if ((Fight.Random.Next() & 1) != 0)
			{
				Fight.OrbitSpeed = -Fight.OrbitSpeed;
			}
			ReplayCamera(Fight, false);
		}

		/** The result screen's camera: the target eased toward the gear's centre, the eye toward its place around the gear. */
		void ResultCamera(FBattlingFight& Fight, const FBattlingFighter& Fighter)
		{
			const FBattlingPoint Look{Fighter.Centre[0], Fighter.Position[1] - Fight.ResultTargetHeight, Fighter.Centre[2]};
			Fight.Target[0] += DivideRoundingAway(Look[0], Fight.Target[0], 8);
			Fight.Target[2] += DivideRoundingAway(Look[2], Fight.Target[2], 8);
			Fight.Target[1] += DivideRoundingAway(Look[1], Fight.Target[1] + Fight.TargetLift, 8);
			const int32 Angle = Fighter.Yaw + Fight.ResultAngle;
			const FBattlingPoint Place{Fighter.Position[0] + (GteSin(Angle) * Fight.ResultDistance >> 12), Fighter.Position[1] - Fight.ResultEyeHeight,
				Fighter.Position[2] + (GteCos(Angle) * Fight.ResultDistance >> 12)};
			FBattlingPoint Step{DivideRoundingAway(Place[0], Fight.Eye[0], 16), DivideRoundingAway(Place[1], Fight.Eye[1], 16),
				DivideRoundingAway(Place[2], Fight.Eye[2], 16)};
			// The clamp's unset height is the camera's, as for the fight's camera: the flat vector it means.
			ClampToArena(Fight.Eye, Step, ResultReach, 0);
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Fight.Eye[Axis] += Step[Axis];
			}
			while (Distance(Fight.Eye, Fighter.Position) < 0x200)
			{
				Fight.Eye[1] -= 2;
				Fight.Eye[0] -= 2;
			}
		}

		/** Two digits at least, as %02d prints them. */
		std::string TwoDigits(int32 Value)
		{
			std::string Text = std::to_string(Value);
			return Value >= 0 && Value < 10 ? "0" + Text : Text;
		}

		/** The winner screen's text: its headings, then the gear's name, the computer's level, the set's score and its time. */
		void PrintWinnerText(FBattlingFight& Fight, const FBattlingFighter& Fighter)
		{
			const FBattlingTexts& Texts = Fight.Tables.Texts;
			const uint32 Buffer = Fight.Pass & 1;
			const bool bLevel = Fight.Settings.Mode != EBattlingMode::TwoPlayers && Fight.Settings.Mode != EBattlingMode::ComputerVersusComputer;
			const int32 Top = bLevel ? WinnerTextTop : WinnerTextTopWithoutLevel;
			FBattlingLabels& Labels = Fight.Labels;
			Labels.SetScale(WinnerTextScale);
			Labels.SetPosition(WinnerHeadingsX, Top);
			Labels.Print(Texts.Winner, Buffer);
			if (bLevel)
			{
				Labels.Print(Texts.Level, Buffer);
			}
			Labels.Print(Texts.Matches, Buffer);
			Labels.Print(Texts.Time, Buffer);
			Labels.SetPosition(WinnerValuesX, Top);
			Labels.Print(Fight.Tables.GearNames[Fighter.Gear], Buffer);
			if (bLevel)
			{
				Labels.Print(Texts.LevelNames[std::min<std::size_t>(Fight.Settings.Level, Texts.LevelNames.size() - 1)], Buffer);
			}
			const FBattlingFighter& Opponent = Fight.Fighters[Fighter.SideIndex() == 0 ? 1 : 0];
			Labels.Print(FormatBattlingText(Texts.ScoreFormat, {std::to_string(Fighter.RoundsWon), std::to_string(Fight.Round), Fight.Tables.GearNames[Opponent.Gear]}),
				Buffer);
			const int32 Ticks = Fight.SetTime;
			Labels.Print(FormatBattlingText(Texts.TimeFormat, {TwoDigits(Ticks / TicksPerMinute), TwoDigits(Ticks % TicksPerMinute / TicksPerSecond),
							 TwoDigits(Ticks % TicksPerSecond * 99 / TicksPerSecond)}),
				Buffer);
		}

		void WinnerPose(FBattlingFight& Fight, FBattlingFighter& Fighter)
		{
			Fight.Settings.MotionSpeed = 0x100;
			Fighter.AnimationSpeed = 0x10;
			Fighter.SpeedBonus = 0;
			Fighter.Animation = Fight.ReplayTicks < VictoryFromTick ? VictoryAnimation : 0;
			Fighter.Flags |= BattlingFlags::ReleaseHeld;
		}

		void LoserPose(FBattlingFight& Fight, FBattlingFighter& Fighter)
		{
			Fight.Settings.MotionSpeed = 0x100;
			Fighter.AnimationSpeed = 0x10;
			Fighter.SpeedBonus = 0;
			Fighter.Animation = DownAnimation;
			Fighter.Flags |= BattlingFlags::NoBlend | BattlingFlags::ReleaseHeld;
		}

		/** The result screen's second tick: the effects and sounds cleared, the victory animation set by the winner's gear. */
		void ResetResult(FBattlingFight& Fight)
		{
			ClearSplashesAndDust(Fight);
			ClearSparks(Fight);
			ClearEffects(Fight);
			Fight.Sound.Reset();
			Fight.Menus.InvalidateHelp();
			const bool bLoops = Winner(Fight).File->bVictoryLoops;
			Fight.AnimationKinds[VictoryAnimation] = bLoops ? FBattlingAnimationKind{1, VictoryAnimation} : FBattlingAnimationKind{2, 0xFF};
			for (FBattlingFighter& Fighter : Fight.Fighters)
			{
				Fighter.Position[1] = 0x100;
				Fighter.PreviousPosition[1] = 0x100;
			}
		}

		uint16 MenuPadPressed(FBattlingFight& Fight, const std::array<FBattlingPad, 2>& Pads)
		{
			return Pads[Fight.MenuPad == 1 ? 1 : 0].Pressed;
		}
	}

	std::array<FBattlingPad, 2> MergePads(const FBattlingTickInput& Input)
	{
		std::array<FBattlingPad, 2> Merged{};
		if (!Input.Blanks.empty())
		{
			for (std::size_t Pad = 0; Pad < 2; ++Pad)
			{
				const FBattlingPad& Latest = Input.Blanks.back()[Pad];
				Merged[Pad].Type = Latest.Type;
				Merged[Pad].LeftX = Latest.LeftX;
				Merged[Pad].LeftY = Latest.LeftY;
				Merged[Pad].bMotor = Latest.bMotor;
			}
		}
		if (Input.bOverflowed)
		{
			return Merged;
		}
		for (const std::array<FBattlingPad, 2>& Blank : Input.Blanks)
		{
			for (std::size_t Pad = 0; Pad < 2; ++Pad)
			{
				Merged[Pad].Held = static_cast<uint16>(Merged[Pad].Held | Blank[Pad].Held);
				Merged[Pad].StickHeld = static_cast<uint16>(Merged[Pad].StickHeld | Blank[Pad].StickHeld);
				Merged[Pad].Pressed = static_cast<uint16>(Merged[Pad].Pressed | Blank[Pad].Pressed);
				Merged[Pad].Repeated = static_cast<uint16>(Merged[Pad].Repeated | Blank[Pad].Repeated);
			}
		}
		return Merged;
	}

	void SetState(FBattlingFight& Fight, uint8 State)
	{
		Fight.State = State;
		switch (State)
		{
		case StateReplay:
			StartReplay(Fight, Fight.RoundTicks);
			break;
		case StateWinner:
			StartWinner(Fight, Fight.Fighters[0].RoundsWon < Fight.Fighters[1].RoundsWon ? 1 : 0);
			break;
		case StateResult:
			StartResult(Fight);
			break;
		default:
			break;
		}
	}

	void ReplayCamera(FBattlingFight& Fight, bool bEased)
	{
		const FBattlingFighter& Won = Fight.Fighters[Fight.Winner == 0 ? 0 : 1];
		const FBattlingFighter& Lost = Fight.Fighters[Fight.Winner == 0 ? 1 : 0];
		// As the replay nears its end the target slides from the loser to the winner.
		int32 Share = 0x100;
		if (Fight.ReplayTicks < 0xB1)
		{
			Share = 0xA0 < Fight.ReplayTicks ? (Fight.ReplayTicks - 0xA0) * 0x10 : 0;
		}
		FBattlingPoint Move{};
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Move[Axis] = Over256((Lost.Position[Axis] - Won.Position[Axis]) * Share) + Won.Position[Axis] - Fight.Target[Axis];
		}
		Move[1] -= 0xA0;
		if (bEased)
		{
			for (int32& Component : Move)
			{
				Component /= 3;
			}
		}
		Fight.CameraRadius = OrbitRadius;
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Fight.Target[Axis] += Move[Axis];
		}
		const int32 Angle = Fight.OrbitAngle + Fight.ReplayTicks * Fight.OrbitSpeed;
		const int32 AcrossX = GteSin(Angle) * Fight.CameraRadius >> 12;
		int32 Height = Fight.Target[1] - (Fight.ReplayTicks * 6 + 0x200);
		const int32 AcrossZ = (GteCos(Angle) * Fight.CameraRadius >> 12) + Fight.Target[2];
		FBattlingPoint Step{AcrossX + Fight.Target[0] - Fight.Eye[0], 0, AcrossZ - Fight.Eye[2]};
		ClampToArena(Fight.Eye, Step, ResultReach, 0);
		Fight.Eye[0] += Step[0];
		Fight.Eye[2] += Step[2];
		const int32 Ground = TerrainHeight(Fight, Fight.Eye, false);
		if (Ground < Height)
		{
			Height = Ground;
		}
		Fight.Eye[1] = Height;
	}

	void TickReplay(FBattlingFight& Fight, const FBattlingTickInput& Input)
	{
		FBattlingFighter& One = Fight.Fighters[0];
		FBattlingFighter& Two = Fight.Fighters[1];
		const uint8 Index = Fight.RecordIndex;
		Fight.RecordIndex = static_cast<uint8>(Index + 1);
		for (FBattlingFighter& Fighter : Fight.Fighters)
		{
			Fighter.Record = Index;
			Fighter.Replayed = static_cast<uint8>(Index + 1);
		}
		if (Fight.ReplayTicks == FullReplay)
		{
			for (FBattlingFighter& Fighter : Fight.Fighters)
			{
				FBattlingReplayRecord& Replayed = Fighter.Records[Fighter.Replayed];
				Replayed.YawFlags |= 0x1000;
				Replayed.Blend = 1;
				Replayed.Frames = 1;
			}
		}
		// The replay's label blinks, eight passes on and eight off.
		if ((Fight.Pass & 8) != 0)
		{
			Fight.Labels.SetPosition(0x10, 0x10);
			Fight.Labels.Print(Fight.Tables.Texts.Replay, Fight.Pass & 1);
		}
		UpdateProjectiles(Fight, One);
		UpdateProjectiles(Fight, Two);
		BeginTick(Fight, One);
		BeginTick(Fight, Two);
		for (FBattlingFighter& Fighter : Fight.Fighters)
		{
			Fighter.PreviousBase = Fighter.Base;
			Fighter.PreviousPosition = Fighter.Position;
		}
		Fight.Effects.SparkRequest = 0;
		if (1 < Fight.CameraEase)
		{
			Fight.CameraEase = 1;
		}
		const auto Pads = MergePads(Input);
		const FBattlingPad& Pad = Pads[Fight.MenuPad == 1 ? 1 : 0];
		if ((Pad.Pressed & ButtonConfirm) != 0)
		{
			SetState(Fight, StateResult);
		}
		if ((Pad.Held & ButtonLeft) != 0)
		{
			Fight.OrbitAngle += 0x20;
		}
		if ((Pad.Held & ButtonRight) != 0)
		{
			Fight.OrbitAngle -= 0x20;
		}
		ReplayFighter(Fight, One);
		ReplayFighter(Fight, Two);
		One.Skeleton.RootAngles[1] = static_cast<int16>(One.Yaw);
		Two.Skeleton.RootAngles[1] = static_cast<int16>(Two.Yaw);
		for (FBattlingFighter& Fighter : Fight.Fighters)
		{
			const uint32 Ground = (TerrainCell(Fight, Fighter.Position) >> 24) & 3;
			Fighter.Flags = (Fighter.Flags & ~BattlingFlags::Ground) | (Ground << 29);
		}
		LoadRecordFlags(One);
		LoadRecordFlags(Two);
		UpdateBody(Fight, One);
		UpdateBody(Fight, Two);
		CheckHits(Fight, One);
		CheckHits(Fight, Two);
		TickEffects(Fight);
		if (Fight.Settings.bRubberBand)
		{
			SpawnLineEffect(Fight, One.Centre, Two.Centre, 0x13);
		}
		if (Fight.ReplayTicks == FullReplay)
		{
			RandomOrbit(Fight);
		}
		if (Fight.ReplayTicks == 0)
		{
			SetState(Fight, StateResult);
		}
		--Fight.ReplayTicks;
	}

	void TickResult(FBattlingFight& Fight, const FBattlingTickInput& Input)
	{
		FBattlingFighter& One = Fight.Fighters[0];
		FBattlingFighter& Two = Fight.Fighters[1];
		if (Fight.ReplayTicks == ResultTicks - 1)
		{
			ResetResult(Fight);
		}
		One.Flags &= ~BattlingFlags::Computer;
		Two.Flags &= ~BattlingFlags::Computer;
		BeginTick(Fight, One);
		BeginTick(Fight, Two);
		Fight.Effects.SparkRequest = 0;
		const auto Pads = MergePads(Input);
		if ((MenuPadPressed(Fight, Pads) & ButtonConfirm) != 0)
		{
			SetState(Fight, StateBetweenRounds);
		}
		if (--Fight.ReplayTicks == -1)
		{
			SetState(Fight, StateBetweenRounds);
		}
		FBattlingLabels& Labels = Fight.Labels;
		const uint32 Buffer = Fight.Pass & 1;
		const EBattlingMode Mode = Fight.Settings.Mode;
		const FBattlingTexts& Texts = Fight.Tables.Texts;
		Labels.SetPosition(0xA0, 0xA0);
		if (Fight.Winner == 1)
		{
			ResultCamera(Fight, Two);
			if (Mode == EBattlingMode::TwoPlayers)
			{
				Labels.PrintCentered(Texts.PlayerTwoVictory, Buffer);
			}
			else if (Mode == EBattlingMode::VersusComputer)
			{
				Labels.PrintCentered(Texts.YouWereDefeated, Buffer);
				Labels.PrintCentered(FormatBattlingText(Texts.ByFormat, {Fight.Tables.GearNames[Two.Gear]}), Buffer);
			}
			else if (Mode == EBattlingMode::ComputerVersusComputer)
			{
				Labels.PrintCentered(Texts.ComputerTwoVictory, Buffer);
			}
		}
		else if (Fight.Winner == 0)
		{
			ResultCamera(Fight, One);
			if (Mode == EBattlingMode::TwoPlayers)
			{
				Labels.PrintCentered(Texts.PlayerOneVictory, Buffer);
			}
			else if (Mode == EBattlingMode::VersusComputer)
			{
				Labels.PrintCentered(Texts.YouWereVictorious, Buffer);
			}
			else if (Mode == EBattlingMode::ComputerVersusComputer)
			{
				Labels.PrintCentered(Texts.ComputerOneVictory, Buffer);
			}
		}
		else if (Fight.Winner == 2)
		{
			Labels.PrintCentered(Texts.DrawGame, Buffer);
		}
		const int32 Ground = TerrainHeight(Fight, Fight.Eye, false);
		if (Ground - Fight.ResultEyeHeight < Fight.Eye[1])
		{
			Fight.Eye[1] = Ground - Fight.ResultEyeHeight;
		}
		One.Stick = 0;
		Two.Stick = 0;
		QueueClear(One);
		QueueClear(Two);
		BeginTick(Fight, One);
		BeginTick(Fight, Two);
		Fight.Effects.SparkRequest = 0;
		UpdateVelocity(Fight, One);
		UpdateVelocity(Fight, Two);
		Move(Fight, Two);
		Move(Fight, One);
		Separate(Fight);
		Two.Skeleton.RootAngles[1] = static_cast<int16>(Two.Yaw);
		One.Skeleton.RootAngles[1] = static_cast<int16>(One.Yaw);
		if ((One.Flags & BattlingFlags::KnockedOut) == 0)
		{
			WinnerPose(Fight, One);
			LoserPose(Fight, Two);
		}
		else
		{
			WinnerPose(Fight, Two);
			LoserPose(Fight, One);
		}
		Animate(Fight, One);
		Animate(Fight, Two);
		UpdateBody(Fight, One);
		UpdateBody(Fight, Two);
		RunAnimationEvents(Fight, One, One.FrameBefore, One.FramesStepped, Fight.Menus.L2HeldPasses);
		RunAnimationEvents(Fight, Two, Two.FrameBefore, Two.FramesStepped, Fight.Menus.L2HeldPasses);
		TickEffects(Fight);
	}

	void PlayAnimationNow(FBattlingFight& Fight, FBattlingFighter& Fighter, uint8 Animation)
	{
		Fighter.Record = 0;
		Fighter.AnimationSpeed = 0x10;
		Fighter.Animation = Animation;
		Fighter.SpeedBonus = 0;
		Fight.Settings.MotionSpeed = 0x100;
		Animate(Fight, Fighter);
		RunAnimationEvents(Fight, Fighter, Fighter.FrameBefore, Fighter.FramesStepped, 0);
	}

	void TickWinner(FBattlingFight& Fight, const FBattlingTickInput& Input)
	{
		FBattlingFighter& Fighter = Fight.Fighters[static_cast<std::size_t>(Fight.WinnerSide)];
		const auto Pads = MergePads(Input);
		if (Fighter.Gear != Siebzehn)
		{
			if ((Pads[0].Held & ButtonRight) != 0)
			{
				--Fight.WinnerSpin;
			}
			if ((Pads[0].Held & ButtonLeft) != 0)
			{
				++Fight.WinnerSpin;
			}
		}
		Fight.WinnerSpin = std::clamp(Fight.WinnerSpin, -0x80, 0x80);
		const uint16 Pressed = MenuPadPressed(Fight, Pads);
		if ((Pressed & ButtonConfirm) != 0)
		{
			Fight.bWinnerEnding = true;
			return;
		}
		if ((Pressed & ButtonL2) != 0)
		{
			++Fight.WinnerText;
		}
		if ((Fight.WinnerText & 1) != 0)
		{
			PrintWinnerText(Fight, Fighter);
		}
		Fight.WinnerYaw = static_cast<int16>(Fight.WinnerYaw + Fight.WinnerSpin);
		PlayAnimationNow(Fight, Fighter, 0);
		Fighter.Skeleton.RootAngles[1] = 0;
		const int16 Scale = static_cast<int16>(Fight.WinnerScale);
		Fighter.Skeleton.Scale = {Scale, Scale, Scale};
		Fighter.Skeleton.Pose();
	}

	void EndWinner(FBattlingFight& Fight)
	{
		Fight.bWinnerEnding = false;
		Fight.DisplayWidth = FightWidth;
		Fight.ProjectionDistance = FighterProjection;
		Fight.Menus.InvalidateHelp();
		SetState(Fight, StateLeaving);
		Fight.Labels.SetScale(FBattlingLabels::OwnSize);
		Fight.Labels.Clear();
	}
}
