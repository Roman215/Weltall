#include "BattlingFightOps.h"
#include "BattlingMath.h"

namespace Weltall::BattlingFightOps
{
	namespace
	{
		using namespace BattlingMath;

		constexpr uint8 StateLeaving = 3;
		constexpr uint8 StateTutorial = 7;

		constexpr uint16 ButtonConfirm = 0x0020;
		constexpr uint16 ButtonUp = 0x1000;
		constexpr uint16 ButtonDown = 0x4000;

		constexpr int32 SoundCursor = 0x1E;
		constexpr int32 SoundConfirm = 0x21;
		constexpr int32 SoundLessonMenu = 0x37;
		constexpr std::array<int32, 3> SoundsBreakdown{0x2A, 0x2B, 0x2C};

		constexpr int32 LessonMenuMessage = 0x42;
		/** Leonard's lines as the scripted bout ends: player 1 the stronger, or not. */
		constexpr int32 AheadMessage = 0x43;
		constexpr int32 BehindMessage = 0x44;
		constexpr int32 LessonPrompt = 0;
		constexpr int32 Welcome = 9;
		constexpr int8 LastLesson = 7;

		constexpr int32 TextSpeed = 3;
		constexpr int32 EndingTextSpeed = 1;
		constexpr int32 MenuSpeed = 0x1E;
		/** The window's place for the lesson menu's prompt (two lines, low) and for a lesson (four). */
		constexpr int32 PromptLines = 2;
		constexpr int16 PromptY = 0xB4;
		constexpr int16 LessonY = 0x9A;

		constexpr int32 StickPush = 0xF0;
		constexpr int32 StickFull = 0xFF;
		constexpr std::array<int32, 2> PointerRest{0xA0, 0x6D};
		/** The pointer blinks: drawn on the passes whose count has this bit. */
		constexpr uint32 PointerBlink = 4;

		constexpr uint8 EndingAnimation = 10;
		constexpr int16 FullHeat = 0x1000;
		constexpr int32 FadeFull = 0xFF;
		constexpr int32 FlashStep = 8;
		constexpr int32 FadeStep = 3;
		constexpr uint8 EffectSpark = 9;
		constexpr uint8 EffectBurst = 0xB;
		constexpr uint8 EffectFlare = 8;

		constexpr int32 ArenaMiddle = 0x3F80;
		constexpr int32 EyeReach = 0x3D00;
		constexpr int32 SettleStep = 0xC0;
		constexpr int32 SettleRise = 0x40;
		constexpr int32 SettleLimit = 0x13;

		enum : uint8
		{
			OpEnd = 0x00,
			OpWait = 0x01,
			OpPlayerOne = 0x02,
			OpPlayerTwo = 0x03,
			OpClearQueue = 0x04,
			OpAttackA = 0x05,
			OpAttackB = 0x06,
			OpJump = 0x07,
			OpEther = 0x08,
			OpBackDash = 0x09,
			OpBackDash2 = 0x0A,
			OpApproach = 0x0B,
			OpRetreat = 0x0C,
			OpRetreatOrLeaveWater = 0x0D,
			OpSteerLeft = 0x0E,
			OpSteerRight = 0x0F,
			OpMessage = 0x12,
			OpContinueMessage = 0x13,
			OpDash = 0x14,
			OpPointer = 0x15,
			OpSpecial = 0x16,
			OpCamera = 0x17,
			OpEndingStep = 0x18,
			OpWaitConfirm = 0x19,
			OpWaitConfirmPage = 0x1A,
			OpHidePointer = 0x1B,
			OpFullHp = 0x1C,
			OpHpOne = 0x1D,
			OpLeave = 0x1E,
			OpSetHeat = 0x1F,
			OpHoldHeat = 0x20,
			OpPlaceFighters = 0x21,
			OpGuard = 0x22,
		};

		/** The queue's commands the script pushes. */
		enum : uint8
		{
			CommandA = 1,
			CommandB = 2,
			CommandEther = 3,
			CommandJump = 4,
			CommandBackDash = 5,
		};

		/** How a timed instruction stands after a tick: just begun (it yields without acting), running, or done. */
		enum class ETimer : uint8
		{
			Begun,
			Running,
			Done,
		};

		ETimer CountDown(FBattlingTutorial& Tutorial, uint8 Ticks)
		{
			const bool bIdle = Tutorial.Counter == 0;
			--Tutorial.Counter;
			if (bIdle)
			{
				Tutorial.Counter = Ticks;
				return ETimer::Begun;
			}
			return Tutorial.Counter != 0 ? ETimer::Running : ETimer::Done;
		}

		void Push(FBattlingFighter& Fighter, int32 Direction)
		{
			Fighter.Stick = StickPush;
			Fighter.WantedDirection = Direction;
			Fighter.SteerOffset = 0;
		}

		/** In the lake: turn toward the arena's middle along whichever bank is nearer. */
		void FaceCentre(FBattlingFight& Fight, FBattlingFighter& Fighter)
		{
			const int32 Angle = GteRatan2(Fighter.Position[0] - ArenaMiddle, Fighter.Position[2] - ArenaMiddle, Fight.Tables.Arctangents);
			const int32 Heading = 0x200 < (Angle & 0xFFF) ? 0x800 : 0xC00;
			Fighter.WantedDirection = Heading - Fight.FightAngle;
			Fighter.Stick = StickFull;
			Fighter.SteerOffset = 0;
		}

		/** Steps the gear away from any much higher ground around it, up to twenty times, then stands it on the ground. */
		void SettleOnGround(FBattlingFight& Fight, FBattlingFighter& Fighter)
		{
			for (int32 Steps = 0;;)
			{
				int32 Lowest = TerrainHeight(Fight, Fighter.Position, true);
				Fighter.Position[1] = Lowest;
				std::size_t Found = 0;
				for (std::size_t Direction = 0; Direction < Fight.Tables.SettleSteps.size(); ++Direction)
				{
					const std::array<int8, 2>& Step = Fight.Tables.SettleSteps[Direction];
					const FBattlingPoint Probe{Fighter.Position[0] + Step[0] * SettleStep, Fighter.Position[1], Fighter.Position[2] + Step[1] * SettleStep};
					const int32 Height = TerrainHeight(Fight, Probe, true);
					if (Height < Lowest - SettleRise)
					{
						Lowest = Height;
						Found = Direction;
					}
				}
				if (Fighter.Position[1] - SettleRise <= Lowest)
				{
					Fighter.Position[1] = TerrainHeight(Fight, Fighter.Position, true);
					break;
				}
				Fighter.Position[0] -= Fight.Tables.SettleSteps[Found][0] * SettleStep;
				Fighter.Position[2] -= Fight.Tables.SettleSteps[Found][1] * SettleStep;
				if (SettleLimit < ++Steps)
				{
					break;
				}
			}
			Fighter.GroundHeight = TerrainHeight(Fight, Fighter.Position, true);
			const uint32 Ground = (TerrainCell(Fight, Fighter.Position) >> 24) & 3;
			Fighter.Flags = (Fighter.Flags & ~BattlingFlags::Ground) | (Ground << 29);
		}

		/** Moves both gears and the eye together to one of the arena's four layouts (other numbers leave them in place). */
		void PlaceFighters(FBattlingFight& Fight, uint8 Layout)
		{
			FBattlingFighter& One = Fight.Fighters[0];
			FBattlingFighter& Two = Fight.Fighters[1];
			const int32 MiddleX = (One.Position[0] + Two.Position[0]) / 2;
			const int32 MiddleZ = (One.Position[2] + Two.Position[2]) / 2;
			int32 X = MiddleX;
			int32 Z = MiddleZ;
			switch (Layout)
			{
			case 0:
				X = 0x4000;
				Z = 0x4000;
				break;
			case 1:
				X = 0x6000;
				Z = 0x6000;
				break;
			case 2:
				X = 0x2000;
				Z = 0x2000;
				break;
			case 3:
				X = 0x4000;
				Z = 0x2400;
				break;
			default:
				break;
			}
			for (FBattlingPoint* Point : {&One.Position, &Two.Position, &Fight.Eye})
			{
				(*Point)[0] = (*Point)[0] - MiddleX + X;
				(*Point)[2] = (*Point)[2] - MiddleZ + Z;
			}
			One.Position[1] = 0;
			Two.Position[1] = 0;
			Fight.Eye[1] = -0x300;
			ClearSplashesAndDust(Fight);
		}

		void Special(FBattlingFight& Fight, uint8 Which)
		{
			FBattlingFighter& One = Fight.Fighters[0];
			FBattlingFighter& Two = Fight.Fighters[1];
			switch (Which)
			{
			case 0:
				Fight.Sound.StopSong();
				Fight.Sound.StopAll();
				break;
			case 1:
				SettleOnGround(Fight, One);
				Fight.Tutorial.Message = (Two.Hp << 8) / Two.MaxHp < (One.Hp << 8) / One.MaxHp ? AheadMessage : BehindMessage;
				break;
			case 2:
				SettleOnGround(Fight, Two);
				break;
			case 3:
				Fight.Eye[1] = std::max(-Fight.DistanceXZ, -0x600);
				break;
			default:
				break;
			}
		}

		void StartScript(FBattlingFight& Fight, uint16 Start)
		{
			Fight.Tutorial.Script = Start;
			Fight.Tutorial.Counter = 0;
			for (FBattlingFighter& Fighter : Fight.Fighters)
			{
				Fighter.Stick = 0;
				Fighter.Flags &= ~BattlingFlags::DashInput;
			}
		}

		void StartLesson(FBattlingFight& Fight, int32 Lesson)
		{
			FBattlingTutorial& Tutorial = Fight.Tutorial;
			Tutorial.bMenuOpen = Lesson == LessonPrompt;
			if (Tutorial.bMenuOpen)
			{
				Fight.Sound.Play(SoundLessonMenu, Fight.VBlank);
				Tutorial.Text.SetLines(PromptLines);
				Tutorial.TextY = PromptY;
			}
			else
			{
				Tutorial.Text.SetLines(FBattlingTutorial::TextLines);
				Tutorial.TextY = LessonY;
			}
			StartScript(Fight, Fight.Tables.Lessons[static_cast<std::size_t>(Lesson)]);
			Tutorial.PointerGoal = PointerRest;
			Tutorial.Pointer = PointerRest;
			Tutorial.HeldHeat = 0;
			for (FBattlingFighter& Fighter : Fight.Fighters)
			{
				Fighter.Hp = Fighter.MaxHp;
			}
		}

		/** The player's confirm this tick, or auto-advancing text's turning of the page. */
		bool Confirmed(const FBattlingFight& Fight)
		{
			return (Fight.PadPressed & ButtonConfirm) != 0 || Fight.Tutorial.bPageRead;
		}

		/**
		 * Whether the script waits for the button at a page of a lesson that its wait goes on from: the lesson menu's
		 * prompt waits too, but the menu answers the button there.
		 */
		bool WaitsForReader(const FBattlingFight& Fight)
		{
			const FBattlingTutorial& Tutorial = Fight.Tutorial;
			const uint8 Op = Fight.Tables.Scripts[Tutorial.Script];
			const EMesStop Stop = Tutorial.Text.StopReason();
			if (Tutorial.bMenuOpen || Stop == EMesStop::Typing)
			{
				return false;
			}
			return Op == OpWaitConfirm || (Op == OpWaitConfirmPage && (Stop == EMesStop::Page || Stop == EMesStop::Hold));
		}

		/** Runs the script until an instruction yields for this tick. */
		void StepScript(FBattlingFight& Fight)
		{
			FBattlingTutorial& Tutorial = Fight.Tutorial;
			const std::span<const uint8> Script = Fight.Tables.Scripts;
			// Before a script chooses a fighter the program writes through a null pointer into the BIOS's memory; the port
			// writes nothing. The scripts choose one before any instruction that drives it (the content test checks).
			FBattlingFighter* Fighter = Tutorial.Fighter < 0 ? nullptr : &Fight.Fighters[static_cast<std::size_t>(Tutorial.Fighter)];
			if (Fighter != nullptr)
			{
				Fighter->Stick = 0;
				if (Tutorial.HeldHeat != 0)
				{
					Tutorial.HeldHeat = std::max(Tutorial.HeldHeat, Fighter->Heat);
					Fighter->Heat = Tutorial.HeldHeat;
				}
			}
			const auto Operand = [&](int32 Index) { return Script[Tutorial.Script + static_cast<std::size_t>(Index)]; };
			const auto Advance = [&](int32 Length) { Tutorial.Script = static_cast<uint16>(Tutorial.Script + Length); };
			const auto Choose = [&](int8 Side) {
				Tutorial.Fighter = Side;
				Fighter = &Fight.Fighters[static_cast<std::size_t>(Side)];
			};
			for (;;)
			{
				const uint8 Op = Operand(0);
				switch (Op)
				{
				case OpWait:
				{
					const ETimer Timer = CountDown(Tutorial, Operand(1));
					if (Timer != ETimer::Done)
					{
						return;
					}
					Advance(2);
					continue;
				}
				case OpPlayerOne:
				case OpPlayerTwo:
					Choose(Op == OpPlayerOne ? int8{0} : int8{1});
					Advance(1);
					continue;
				case OpClearQueue:
					QueueClear(*Fighter);
					break;
				case OpAttackA:
					QueuePush(*Fighter, CommandA);
					break;
				case OpAttackB:
					QueuePush(*Fighter, CommandB);
					break;
				case OpJump:
					QueuePush(*Fighter, CommandJump);
					break;
				case OpEther:
					QueuePush(*Fighter, CommandEther);
					break;
				case OpBackDash:
				case OpBackDash2:
					QueuePush(*Fighter, CommandBackDash);
					break;
				case OpApproach:
					if (0xFF < Fight.DistanceXZ)
					{
						Push(*Fighter, 0);
						return;
					}
					break;
				case OpRetreat:
					if (Fight.DistanceXZ < 0x401)
					{
						Push(*Fighter, 0x800);
						return;
					}
					break;
				case OpRetreatOrLeaveWater:
					if (InLakeZone(Fight, Fighter->Position))
					{
						FaceCentre(Fight, *Fighter);
						return;
					}
					if (Fight.DistanceXZ < 0x801)
					{
						Push(*Fighter, 0x800);
						return;
					}
					break;
				case OpSteerLeft:
				case OpSteerRight:
				case OpDash:
				{
					const ETimer Timer = CountDown(Tutorial, Operand(1));
					if (Timer == ETimer::Begun)
					{
						return;
					}
					if (Timer == ETimer::Running)
					{
						Push(*Fighter, Op == OpSteerRight ? 0x400 : -0x400);
						if (Op == OpDash)
						{
							Fighter->Flags |= BattlingFlags::DashInput;
						}
						return;
					}
					Advance(2);
					continue;
				}
				case OpMessage:
					Tutorial.Message = Operand(1);
					Advance(2);
					continue;
				case OpContinueMessage:
					Tutorial.Text.Continue();
					break;
				case OpPointer:
					Tutorial.PointerGoal = {Operand(1) << 1, Operand(2)};
					Tutorial.bPointerShown = true;
					Advance(3);
					continue;
				case OpSpecial:
					Special(Fight, Operand(1));
					Advance(2);
					continue;
				case OpCamera:
					Tutorial.Camera = static_cast<EBattlingScriptCamera>(Operand(1));
					Advance(2);
					continue;
				case OpEndingStep:
					Tutorial.EndingStep = Operand(1);
					Advance(2);
					continue;
				case OpWaitConfirm:
				{
					const EMesStop Stop = Tutorial.Text.StopReason();
					if (Stop == EMesStop::Typing || !Confirmed(Fight))
					{
						return;
					}
					// A page or a hold goes on to the next and waits again; the end goes on to the next instruction.
					if (Stop == EMesStop::End)
					{
						Advance(1);
					}
					Tutorial.Text.Continue();
					continue;
				}
				case OpWaitConfirmPage:
				{
					const EMesStop Stop = Tutorial.Text.StopReason();
					if (Stop == EMesStop::Typing || !Confirmed(Fight) || (Stop != EMesStop::Page && Stop != EMesStop::Hold))
					{
						return;
					}
					Tutorial.Text.Continue();
					Advance(1);
					return;
				}
				case OpHidePointer:
					Tutorial.PointerGoal = PointerRest;
					Tutorial.Pointer = PointerRest;
					Tutorial.bPointerShown = false;
					Advance(1);
					continue;
				case OpFullHp:
				case OpHpOne:
				{
					FBattlingFighter& Target = Operand(1) == 0 ? *Fighter : Opponent(Fight, *Fighter);
					Target.Hp = Op == OpFullHp ? Target.MaxHp : static_cast<int16>(1);
					Advance(2);
					continue;
				}
				case OpLeave:
					SetState(Fight, StateLeaving);
					Tutorial.Menu.Clear();
					break;
				case OpSetHeat:
					Fighter->Heat = static_cast<int16>(Operand(1) << 4);
					Advance(2);
					continue;
				case OpHoldHeat:
					Tutorial.HeldHeat = static_cast<int16>(Operand(1) << 4);
					Advance(2);
					continue;
				case OpPlaceFighters:
					PlaceFighters(Fight, Operand(1));
					Advance(2);
					continue;
				case OpGuard:
					Fighter->Flags = Operand(1) == 0 ? Fighter->Flags & ~(BattlingFlags::GuardInput | BattlingFlags::GuardCounter)
													 : Fighter->Flags | BattlingFlags::GuardInput;
					Advance(2);
					continue;
				default:
					// The end, which the tutorial's tick restarts the lesson menu from; the ending stays there.
					return;
				}
				Advance(1);
			}
		}

		/** Runs the script for the tick of Input's vertical blanks; auto-advancing text turns a page its reader has had long enough. */
		void StepScriptReading(FBattlingFight& Fight, const FBattlingTickInput& Input)
		{
			FBattlingTutorial& Tutorial = Fight.Tutorial;
			const EMesAutoAdvance Pace = Fight.Text != nullptr ? Fight.Text->AutoAdvance : EMesAutoAdvance::Off;
			const int32 Vblanks = static_cast<int32>(Input.Blanks.size());
			Tutorial.bPageRead = Tutorial.Reading.Tick(Pace, WaitsForReader(Fight), MesShownGlyphs(Tutorial.Text), Vblanks);
			StepScript(Fight);
			Tutorial.bPageRead = false;
		}

		void EaseTarget(FBattlingFight& Fight, const FBattlingPoint& Goal, int32 Divisor)
		{
			Fight.Target[0] += DivideRoundingAway(Goal[0], Fight.Target[0], Divisor);
			Fight.Target[2] += DivideRoundingAway(Goal[2], Fight.Target[2], Divisor);
			Fight.Target[1] += DivideRoundingAway(Goal[1], Fight.Target[1] + Fight.TargetLift, Divisor);
		}

		void EaseEye(FBattlingFight& Fight, const FBattlingPoint& Goal, int32 Divisor)
		{
			FBattlingPoint Step{DivideRoundingAway(Goal[0], Fight.Eye[0], Divisor), DivideRoundingAway(Goal[1], Fight.Eye[1], Divisor),
				DivideRoundingAway(Goal[2], Fight.Eye[2], Divisor)};
			// As at the fight's camera, the clamp's unset height is whatever the stack held: the port takes the flat vector.
			ClampToArena(Fight.Eye, Step, EyeReach, 0);
			Fight.Eye[0] += Step[0];
			Fight.Eye[1] += Step[1];
			Fight.Eye[2] += Step[2];
		}

		/** The fight's camera as the script machine places it: the target between the gears, the eye eased in all three axes. */
		void FightPosition(FBattlingFight& Fight)
		{
			const FBattlingPoint& One = Fight.Fighters[0].Position;
			const FBattlingPoint& Two = Fight.Fighters[1].Position;
			const int32 Angle = GteRatan2(One[0] - Two[0], One[2] - Two[2], Fight.Tables.Arctangents);
			const int32 Apart = Distance(One, Two);
			Fight.Target = {(One[0] + Two[0]) / 2, (One[1] + Two[1]) / 2 - 0xA0, (One[2] + Two[2]) / 2};
			Fight.CameraRadius = (Apart << 1) / 3 + 0xC0;
			const int32 Radius = Fight.CameraRadius;
			const auto Reach = [&](int32 Turn) {
				return Length2(Fight.Target[0] + (GteSin(Angle + Turn) * Radius >> 12) - Fight.Eye[0],
					Fight.Target[2] + (GteCos(Angle + Turn) * Radius >> 12) - Fight.Eye[2]);
			};
			Fight.CameraSide = Reach(-0x400) <= Reach(0x400) ? 1 : 0;
			Fight.CameraSideAngle = Fight.CameraSide != 0 ? -0x400 : 0x400;
			const int32 Quarter = std::min((Apart < 0 ? Apart + 3 : Apart) >> 2, 0x300);
			FBattlingPoint Goal{Fight.Target[0] + (GteSin(Angle + Fight.CameraSideAngle) * Radius >> 12), Fight.Target[1] - Fight.CameraLift - Quarter,
				Fight.Target[2] + (GteCos(Angle + Fight.CameraSideAngle) * Radius >> 12)};
			Goal[1] = std::min(Goal[1], TerrainHeight(Fight, Goal, false) - 0x100);
			const int32 Ease = Fight.CameraEase;
			FBattlingPoint Step{(Goal[0] - Fight.Eye[0]) / Ease, (Goal[1] - Fight.Eye[1]) / Ease, (Goal[2] - Fight.Eye[2]) / Ease};
			ClampToArena(Fight.Eye, Step, EyeReach, 0);
			Fight.CameraEase = 100;
			Fight.Eye[0] += Step[0];
			Fight.Eye[1] += Step[1];
			Fight.Eye[2] += Step[2];
		}

		void UpdateCamera(FBattlingFight& Fight)
		{
			const FBattlingFighter& One = Fight.Fighters[0];
			const FBattlingFighter& Two = Fight.Fighters[1];
			switch (Fight.Tutorial.Camera)
			{
			case EBattlingScriptCamera::Fight:
				FightPosition(Fight);
				break;
			case EBattlingScriptCamera::OnPlayerOne:
			case EBattlingScriptCamera::OnPlayerTwo:
			{
				const FBattlingFighter& On = Fight.Tutorial.Camera == EBattlingScriptCamera::OnPlayerOne ? One : Two;
				Fight.TargetLift = 0;
				EaseTarget(Fight, {On.Position[0], On.Position[1] - 0xA0, On.Position[2]}, 4);
				break;
			}
			case EBattlingScriptCamera::BehindPlayerOneHigh:
			case EBattlingScriptCamera::BehindPlayerOneLow:
			{
				Fight.TargetLift = Fight.Tutorial.Camera == EBattlingScriptCamera::BehindPlayerOneHigh ? 0xA0 : 0x80;
				const int32 Lift = Fight.TargetLift;
				EaseTarget(Fight, {One.Centre[0], One.Centre[1] + Lift, One.Centre[2]}, 0x10);
				EaseEye(Fight,
					{One.Position[0] + (GteSin(One.Yaw + 0xA80) * 0xD0 >> 12), One.Position[1] - 0x20 - Lift,
						One.Position[2] + (GteCos(One.Yaw + 0xA80) * 0xD0 >> 12)},
					0x46);
				Fight.Eye[1] = std::min(Fight.Eye[1], TerrainHeight(Fight, Fight.Eye, false) - 0x40 - Lift);
				break;
			}
			case EBattlingScriptCamera::FacingPlayerTwo:
				Fight.Target = {Two.Position[0], Two.Position[1] - 0xC0, Two.Position[2]};
				Fight.Eye = {Fight.Target[0] + (GteSin(Two.Yaw + 0x900) * 0xE0 >> 12), Two.Position[1] - 0xD0,
					Fight.Target[2] + (GteCos(Two.Yaw + 0x900) * 0xE0 >> 12)};
				break;
			}
		}

		/** A random joint of player 1's gear, where it stands in the arena. */
		FBattlingPoint RandomJoint(FBattlingFight& Fight)
		{
			const FBattlingFighter& One = Fight.Fighters[0];
			const std::vector<FBattlingJoint>& Joints = One.Skeleton.Joints;
			const FBattlingJoint& Joint = Joints[static_cast<std::size_t>(Fight.Random.Next() % static_cast<int32>(Joints.size()))];
			return {Joint.World.T[0] + One.Position[0], Joint.World.T[1] + One.Position[1], Joint.World.T[2] + One.Position[2]};
		}

		void StepEndingSparks(FBattlingFight& Fight)
		{
			for (FBattlingTutorial::FSpark& Spark : Fight.Tutorial.Sparks)
			{
				if (--Spark.Timer == -1)
				{
					Spark.Point = RandomJoint(Fight);
					Spark.Timer = Fight.Random.Next() % 16 + 8;
				}
				if ((Spark.Timer & 1) != 0)
				{
					SpawnEffect(Fight, Spark.Point, EffectSpark);
				}
			}
		}

		/** Player 1's gear held seized at full heat as it breaks down, sparking now and then. */
		void HoldBreakdown(FBattlingFight& Fight)
		{
			FBattlingFighter& One = Fight.Fighters[0];
			One.AnimationSpeed = 0x10;
			One.Animation = EndingAnimation;
			One.SpeedBonus = 0;
			One.Heat = FullHeat;
			One.Flags |= BattlingFlags::ReleaseHeld;
			if ((Fight.Random.Next() & 1) != 0)
			{
				const FBattlingPoint Point = RandomJoint(Fight);
				SpawnEffect(Fight, Point, EffectBurst);
				SpawnEffect(Fight, Point, EffectFlare);
			}
		}
	}

	void TickTutorial(FBattlingFight& Fight, const FBattlingTickInput& Input)
	{
		FBattlingTutorial& Tutorial = Fight.Tutorial;
		Tutorial.bPointerDrawn = Tutorial.bPointerShown && (Fight.Pass & PointerBlink) != 0;
		Tutorial.PointerDrawn = Tutorial.Pointer;
		const FBattlingPad Pad = MergePads(Input)[0];
		Fight.PadPressed = Pad.Pressed;
		if (Tutorial.bMenuOpen)
		{
			if ((Pad.Repeated & ButtonUp) != 0)
			{
				Fight.Sound.Play(SoundCursor, Fight.VBlank);
				--Tutorial.MenuCursor;
			}
			if ((Pad.Repeated & ButtonDown) != 0)
			{
				Fight.Sound.Play(SoundCursor, Fight.VBlank);
				++Tutorial.MenuCursor;
			}
			if (LastLesson < Tutorial.MenuCursor)
			{
				Tutorial.MenuCursor = 0;
			}
			if (Tutorial.MenuCursor < 0)
			{
				Tutorial.MenuCursor = LastLesson;
			}
			Tutorial.Menu.SetColor(static_cast<uint8>((Fight.Pass * 7) & 0x3F), 0xC0, 0x10);
			Tutorial.Menu.SetHighlightRow(Tutorial.MenuCursor);
			if ((Pad.Pressed & ButtonConfirm) != 0)
			{
				Fight.Sound.Play(SoundConfirm, Fight.VBlank);
				StartLesson(Fight, Tutorial.MenuCursor + 1);
				Tutorial.Text.Clear();
			}
		}
		if (Fight.Tables.Scripts[Tutorial.Script] == OpEnd)
		{
			StartLesson(Fight, LessonPrompt);
		}
		StepScriptReading(Fight, Input);
		Tutorial.Pointer[0] += DivideRoundingAway(Tutorial.PointerGoal[0], Tutorial.Pointer[0], 4);
		Tutorial.Pointer[1] += DivideRoundingAway(Tutorial.PointerGoal[1], Tutorial.Pointer[1], 4);
		Tutorial.bMenuDrawn = Tutorial.bMenuOpen;
		if (Tutorial.bMenuOpen)
		{
			Tutorial.Menu.Update(*Fight.Text);
		}
		if (Tutorial.Message != Tutorial.MessageShown)
		{
			Tutorial.Text.SetSpeed(TextSpeed);
			Tutorial.Text.Clear();
			Tutorial.Text.Queue(Fight.Messages->String(Tutorial.Message));
			Tutorial.MessageShown = Tutorial.Message;
		}
		// The queued vertical blanks are spent: the fight tick finds none.
		FightTick(Fight, FBattlingTickInput{});
		UpdateCamera(Fight);
	}

	void StartEnding(FBattlingFight& Fight)
	{
		if (Fight.bEndingActive)
		{
			return;
		}
		FBattlingTutorial& Tutorial = Fight.Tutorial;
		StartScript(Fight, Fight.Tables.EndingScript);
		Fight.bEndingActive = true;
		Fight.bFightStarted = false;
		Fight.Settings.Computer = {false, false};
		Tutorial.EndingStep = 0;
		Tutorial.Message = 0;
		Tutorial.MessageShown = 0;
		for (FBattlingTutorial::FSpark& Spark : Tutorial.Sparks)
		{
			Spark.Timer = 1;
		}
		Fight.Fighters[1].Stun = 0;
		Fight.Fighters[0].Stun = 0;
	}

	void TickEnding(FBattlingFight& Fight, const FBattlingTickInput& Input)
	{
		if (!Fight.bEndingActive)
		{
			return;
		}
		FBattlingTutorial& Tutorial = Fight.Tutorial;
		Tutorial.bTintDrawn = false;
		switch (Tutorial.EndingStep)
		{
		case 11:
			for (const int32 Sound : SoundsBreakdown)
			{
				Fight.Sound.Play(Sound, Fight.VBlank);
			}
			Tutorial.EndingStep = 1;
			[[fallthrough]];
		case 1:
			StepEndingSparks(Fight);
			break;
		case 2:
			Fight.Fighters[0].Hp = 1;
			Tutorial.Fade = FadeFull;
			Tutorial.EndingStep = 3;
			[[fallthrough]];
		case 3:
			// The flash is drawn as it stood; it fades below zero and back each tick, which draws nothing visible.
			Tutorial.bTintDrawn = true;
			Tutorial.bTintSubtracts = false;
			Tutorial.Tint = Tutorial.Fade;
			Tutorial.Fade = static_cast<int16>(std::max<int32>(Tutorial.Fade, 0) - FlashStep);
			HoldBreakdown(Fight);
			break;
		case 4:
			Tutorial.Fade = static_cast<int16>(Tutorial.Fade + FadeStep);
			if (FadeFull < Tutorial.Fade)
			{
				Tutorial.Fade = FadeFull;
				Fight.Result = 0x7F;
				Tutorial.bLeft = true;
				return;
			}
			Tutorial.bTintDrawn = true;
			Tutorial.bTintSubtracts = true;
			Tutorial.Tint = Tutorial.Fade;
			HoldBreakdown(Fight);
			break;
		default:
			break;
		}
		StepScriptReading(Fight, Input);
		if (Tutorial.Message != Tutorial.MessageShown)
		{
			Tutorial.Text.SetSpeed(EndingTextSpeed);
			Tutorial.Text.FreeQueue();
			Tutorial.Text.Queue(Fight.Messages->String(Tutorial.Message));
			Tutorial.Text.SetLines(PromptLines);
			Tutorial.TextY = PromptY;
			Tutorial.MessageShown = Tutorial.Message;
		}
		UpdateCamera(Fight);
		Fight.Fighters[0].Stick = 0;
	}
}

namespace Weltall
{
	void FBattlingFight::StartTutorial()
	{
		using namespace BattlingFightOps;
		Settings.Computer = {false, false};
		SetState(*this, StateTutorial);
		Settings.Mode = EBattlingMode::Tutorial;
		Settings.bRubberBand = false;
		Tutorial.Menu.Reinitialize(FBattlingTutorial::MenuWidth, FBattlingTutorial::MenuLines);
		Tutorial.Menu.Queue(Messages->String(LessonMenuMessage));
		Tutorial.Menu.SetSpeed(MenuSpeed);
		StartScript(*this, 0);
		Tutorial.MenuCursor = 0;
		Tutorial.Camera = EBattlingScriptCamera::Fight;
		Tutorial.EndingStep = 0;
		StartLesson(*this, Welcome);
	}
}
