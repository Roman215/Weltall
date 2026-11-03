#include "Battle/Show/BattleShowPlayback.h"

#include "Battle/BattleContext.h"
#include "Battle/BattleParty.h"
#include "Battle/BattleProgram.h"
#include "Battle/Show/BattleShow.h"
#include "Battle/Show/BattleShowVram.h"
#include "Battle/Show/IdsBattleSprite.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"
#include "Graphics/SpriteFile.h"

#include <algorithm>

namespace Weltall
{


	namespace
	{
		/** The playback's codes: F3-F8 and FA the controller's own, F9 a pause, FB a borrowed look, FC a stance, FD a walk, FE the walk home, FF a wait. */
		constexpr uint8 StayCode = 0xF3;
		constexpr uint8 CastCode = 0xF4;
		constexpr uint8 ItemCastCode = 0xF5;
		constexpr uint8 FrameCode = 0xF6;
		constexpr uint8 WaitCode = 0xF7;
		constexpr uint8 HideBoxCode = 0xF8;
		constexpr uint8 PauseCode = 0xF9;
		constexpr uint8 ShowBoxCode = 0xFA;
		constexpr uint8 BorrowCode = 0xFB;
		constexpr uint8 StanceCode = 0xFC;
		constexpr uint8 WalkCode = 0xFD;
		constexpr uint8 HomeCode = 0xFE;
		constexpr uint8 HoldCode = 0xFF;
		/** A party member's codes below 10h are its attack file's animations; from 10h they cast an effect. */
		constexpr uint8 FirstEffectCode = 0x10;
		constexpr uint8 NoIdsAnimation = 0xFF;
		/** The result a countered target's record gives it (the reaction pose follows). */
		constexpr uint8 Countered = 7;
		/** A 3D performer's sequences: its approach, its walk, its walk home, its collapse, its return when the playback is dropped. */
		constexpr int32 ApproachSequence = 0x1A;
		constexpr int32 WalkSequence = 2;
		constexpr int32 HomeSequence = 4;
		constexpr int32 CollapseSequence = 0x15;
		constexpr int32 CancelSequence = 0x1F;
		/** The distance the actor stands from its target, on its own side. */
		/** The playback's end's resume points past its numbered steps: Billy's ammunition window's two frames, then the collapse. */
		constexpr uint8 FinishCollapse = 0x10;
		constexpr uint8 FinishAmmunitionWork = 0x11;
		constexpr uint8 FinishAmmunitionClosed = 0x12;
		constexpr int16 StandOff = 0x50;
		constexpr int16 CameraPitch = 0xC0;
		/** The animations the wait for the poses lets play on. */
		constexpr std::array<int32, 10> SettledAnimations{0, 1, 5, 7, 0xE, 0xF, 0x14, 0x15, 0x16, 0x18};
		/** The step's states. */
		constexpr int32 LastLeg = 2;
		constexpr int32 Animating = 4;
		constexpr int32 WalkingHome = 5;
		constexpr int32 RouteLeg = 6;
		constexpr int32 AwaitingNumbers = 7;
		constexpr int32 EffectState = 8;
		constexpr int32 ModelRecord = 9;
		constexpr int32 Ending = 10;

		uint16 SlotBit(int32 Slot)
		{
			return static_cast<uint16>(1u << (static_cast<uint32>(Slot) & 0x1F));
		}

		int16 High(int32 Value)
		{
			return static_cast<int16>(Value >> 16);
		}

		int8 Animation(const FSprite& Sprite)
		{
			return Sprite.Animation();
		}

		/** The stance a combatant's sprite goes back to (+B0h: the animation after this one). */
		int32 Stance(const FSprite& Sprite)
		{
			return Sprite.NextAnimation();
		}
	}

	void FBattlePlayback::FAnimationEnd::OnAnimationEvent(FSprite&)
	{
		Owner.Play.bReady = true;
		Owner.Play.Event = Owner.Play.State;
	}

	FBattlePlayback::FBattlePlayback(FBattleShow& InShow) : Show(InShow), AnimationEnd(*this), CastPoseEnd(*this), EventMotionEnd(*this)
	{
	}

	void FBattlePlayback::Reset()
	{
		Nested.Clear();
		Rules.Clear();
		Returned = 0;
		bController = false;
		Play = FBattlePlayController{};
		Show.Battle.Play.Performer = nullptr;
		CollapseSlots.fill(-1);
		StepPerformer = nullptr;
		StepTarget = nullptr;
		RecordSprite = nullptr;
		WaitRecords = 0;
		ActionFileData.reset();
		LoadingEffect.reset();
		bEffectInMemory = false;
		FreedFiles.clear();
		ActionFileOwner = 0;
		FileReads = 0;
		bAttackFileInstalled = false;
		CollapsedSlots = 0;
		CastPoseEnds = 0;
		bItemCast = false;
		GearDrop = 0;
		AskedEffect = 0;
		bAskedEffectLoaded = false;
		EventFiles = nullptr;
		bEventSounds = false;
		EventObjects.fill(nullptr);
		EventObjectFiles.clear();
		VoiceSounds.clear();
		VoiceSound = 0;
	}

	uint16 FBattlePlayback::Stayed() const
	{
		return Show.Context->ReactingWhileKnockedOut;
	}

	uint16& FBattlePlayback::StayedWord()
	{
		return Show.Context->ReactingWhileKnockedOut;
	}

	int32 FBattlePlayback::ListenerKind(const FSprite& Sprite) const
	{
		if (Sprite.Listener == nullptr)
		{
			return 0;
		}
		if (Sprite.Listener == &AnimationEnd)
		{
			return 1;
		}
		return Sprite.Listener == &CastPoseEnd ? 2 : 3;
	}

	// The routine stacks ------------------------------------------------------------------------------

	bool FBattlePlayback::FinishFrame()
	{
		if (Nested.IsEmpty())
		{
			// A frame the rules' code ran: its tail.
			if (!Nested.Push(static_cast<uint8>(EPlayRoutine::Tail)))
			{
				Fail(static_cast<uint8>(EBattleShowFault::Playback), "the show's routines nest past their room");
				return true;
			}
		}
		return Run(Nested);
	}

	bool FBattlePlayback::ResumeRules()
	{
		return Rules.IsEmpty() || Run(Rules);
	}

	bool FBattlePlayback::Run(FShowRoutineStack& Stack)
	{
		while (!Stack.IsEmpty())
		{
			if (Show.Fault() != EBattleShowFault::None)
			{
				Stack.Clear();
				return true;
			}
			switch (Dispatch(Stack.Top()))
			{
			case EShowFlow::Done:
				Stack.Pop();
				break;
			case EShowFlow::Call:
			case EShowFlow::Again:
				break;
			case EShowFlow::Frame:
				return false;
			}
		}
		return true;
	}

	EShowFlow FBattlePlayback::Call(FShowRoutine& Caller, uint8 Resume, EPlayRoutine Kind, int32 Value, int32 Other)
	{
		Caller.Resume = Resume;
		FShowRoutineStack& Stack = !Nested.IsEmpty() && &Nested.Top() == &Caller ? Nested : Rules;
		if (!Stack.Push(static_cast<uint8>(Kind), Value, Other))
		{
			Fail(static_cast<uint8>(EBattleShowFault::Playback), "the show's routines nest past their room");
			return EShowFlow::Done;
		}
		return EShowFlow::Call;
	}

	EShowFlow FBattlePlayback::Dispatch(FShowRoutine& Routine)
	{
		switch (static_cast<EPlayRoutine>(Routine.Kind))
		{
		case EPlayRoutine::Tail: return RunTail(Routine);
		case EPlayRoutine::Step: return RunStep(Routine);
		case EPlayRoutine::StandAndNext: return RunStandAndNext(Routine);
		case EPlayRoutine::NextRecord: return RunNextRecord(Routine);
		case EPlayRoutine::NextModelRecord: return RunNextModelRecord(Routine);
		case EPlayRoutine::ReactionPose: return RunReactionPose(Routine);
		case EPlayRoutine::Collapse: return RunCollapse(Routine);
		case EPlayRoutine::WaitPoses: return RunWaitPoses(Routine);
		case EPlayRoutine::Finish: return RunFinish(Routine);
		case EPlayRoutine::PrepareNext: return RunPrepareNext(Routine);
		case EPlayRoutine::WaitLoads: return RunWaitLoads(Routine);
		case EPlayRoutine::ReloadImages: return RunReloadImages(Routine);
		case EPlayRoutine::WaitCounted: return RunWaitCounted(Routine);
		case EPlayRoutine::ReadAttackFile: return RunReadAttackFile(Routine);
		case EPlayRoutine::IdleWait: return RunIdleWait(Routine);
		case EPlayRoutine::Begin: return RunBegin(Routine);
		case EPlayRoutine::TurnDamage: return RunTurnDamage(Routine);
		case EPlayRoutine::RefreshPoses: return RunRefreshPoses(Routine);
		case EPlayRoutine::EndPose: return RunEndPose(Routine);
		case EPlayRoutine::CastRecord: return RunCastRecord(Routine);
		case EPlayRoutine::EffectCast: return RunEffectCast(Routine);
		case EPlayRoutine::EffectLoad: return RunEffectLoad(Routine);
		case EPlayRoutine::RemoveOtherParty: return RunRemoveOtherParty(Routine);
		case EPlayRoutine::ReloadGearModels: return RunReloadGearModels(Routine);
		case EPlayRoutine::LoadRequested: return RunLoadRequested(Routine);
		case EPlayRoutine::BoardGear: return RunBoardGear(Routine);
		case EPlayRoutine::EventFinishPerform: return RunEventFinishPerform(Routine);
		case EPlayRoutine::EventAttack: return RunEventAttack(Routine);
		case EPlayRoutine::EventSpecialAttack: return RunEventSpecialAttack(Routine);
		case EPlayRoutine::EventVoice: return RunEventVoice(Routine);
		}
		return EShowFlow::Done;
	}

	void FBattlePlayback::Fail(uint8 Fault, const char* Reason)
	{
		Show.ReportFault(static_cast<EBattleShowFault>(Fault), Reason);
	}

	// The frame's tail and the step ----------------------------------------------------------------------

	EShowFlow FBattlePlayback::RunTail(FShowRoutine& Routine)
	{
		switch (Routine.Resume)
		{
		case 0:
			if (bController)
			{
				return Call(Routine, 1, EPlayRoutine::Step);
			}
			[[fallthrough]];
		case 1:
			// The effect the objects asked for.
			if (AskedEffect != 0)
			{
				const int32 Effect = AskedEffect;
				AskedEffect = 0;
				return Call(Routine, 2, EPlayRoutine::LoadRequested, Effect);
			}
			[[fallthrough]];
		case 2:
			// The objects' idle wait, once the disc's reads are done.
			if (Show.Objects.TakeIdleWait())
			{
				return Call(Routine, 3, EPlayRoutine::IdleWait);
			}
			return EShowFlow::Done;
		default:
			return EShowFlow::Done;
		}
	}

	EShowFlow FBattlePlayback::RunStep(FShowRoutine& Routine)
	{
		FBattlePlayState& State = Show.Battle.Play;
		switch (Routine.Resume)
		{
		case 0:
		{
			StepPerformer = State.Performer;
			StepTarget = State.Targets[0];
			if (StepPerformer == nullptr)
			{
				Fail(static_cast<uint8>(EBattleShowFault::Playback), "a playback's step has no performer");
				return EShowFlow::Done;
			}
			if (State.Caster != nullptr)
			{
				// The effect a cast named starts: the actor plays it, or its sprite runs while the party performer waits in
				// its pose (an item's) and one with a file of its own waits for the effect's tasks.
				State.Caster = nullptr;
				if (StartEffect())
				{
					StepPerformer->Listener = &AnimationEnd;
				}
				else if (Show.Fault() == EBattleShowFault::None)
				{
					if (FBattleSprites::SlotOf(*StepPerformer) < FBattleContext::PartySlotCount)
					{
						Show.Sprites.SetAnimation(*StepPerformer, bItemCast ? ItemAnimation : EffectWaitAnimation);
						bItemCast = false;
					}
					if (StepPerformer->PrimaryFile != nullptr)
					{
						SetState(AwaitingNumbers);
					}
				}
				if (Show.Fault() != EBattleShowFault::None)
				{
					return EShowFlow::Done;
				}
			}
			if (Play.PendingModelRecords != 0)
			{
				// A record a performer's sequence asked for.
				return Call(Routine, 3, EPlayRoutine::NextModelRecord);
			}
			Routine.Resume = 4;
			return EShowFlow::Again;
		}
		case 3:
			--Play.PendingModelRecords;
			[[fallthrough]];
		case 4:
		{
			Routine.Value = Play.Event;
			switch (Play.Event)
			{
			case 0:
				break;
			case Animating:
			{
				// A party member on foot countered by the record before plays its reaction, the frames waiting for it.
				FSprite* Target = State.Targets[0];
				const int32 Slot = Target != nullptr ? FBattleSprites::SlotOf(*Target) : 0;
				const FBattlePerformance* Before = State.Record >= 1 ? &Show.Context->Performances[static_cast<std::size_t>(State.Record - 1)] : nullptr;
				if (Target != nullptr && Slot < FBattleContext::PartySlotCount && !IsModel(Slot) && Before != nullptr
					&& Before->Results[static_cast<std::size_t>(Slot)] == Countered)
				{
					State.Actor = Target;
					Show.Sprites.SetAnimation(*Target, ReactionAnimation);
					ReactionSlot = Slot;
					return Call(Routine, 1, EPlayRoutine::ReactionPose);
				}
				break;
			}
			case LastLeg:
				Arrive(*StepPerformer, StepPerformer->Target);
				break;
			case WalkingHome:
				if (!IsHidden(FBattleSprites::SlotOf(*StepPerformer)))
				{
					Show.Sprites.SetAnimation(*StepPerformer, Stance(*StepPerformer));
				}
				FaceHome(Play.PerformerSlot);
				SetState(Ending);
				break;
			case RouteLeg:
				Approach(*StepPerformer, StepPerformer->Target);
				break;
			default:
				break;
			}
			[[fallthrough]];
		}
		case 1:
			if (Routine.Value != 0)
			{
				Play.Event = 0;
			}
			switch (Play.State)
			{
			case LastLeg:
			case RouteLeg:
			{
				const int32 Distance = DistanceToGoal(*StepPerformer);
				if (Play.LastDistance < Distance)
				{
					if (Play.State == LastLeg)
					{
						Arrive(*StepPerformer, StepTarget);
					}
					else
					{
						Approach(*StepPerformer, StepTarget);
					}
					return EShowFlow::Done;
				}
				Play.LastDistance = Distance;
				return EShowFlow::Done;
			}
			case Animating:
				if (!Play.bReady)
				{
					return EShowFlow::Done;
				}
				return Call(Routine, 2, EPlayRoutine::StandAndNext);
			case AwaitingNumbers:
				if (Show.Numbers.Pending() != 0)
				{
					return EShowFlow::Done;
				}
				SetState(Animating);
				Play.bReady = true;
				return Call(Routine, 2, EPlayRoutine::ReloadImages);
			case EffectState:
				// Waiting for the model records a 3D performer's sequences ask for, or, once an effect's load dropped the gear
				// models, for its tasks and its cast's end: then the gears come back (a model record counted when all went).
				if (GearDrop == 0 || Show.Numbers.Pending() != 0 || !State.bCastEnded)
				{
					return EShowFlow::Done;
				}
				if (GearDrop == DropsEveryGear)
				{
					++Play.PendingModelRecords;
				}
				return Call(Routine, 2, EPlayRoutine::ReloadGearModels);
			case ModelRecord:
				return Call(Routine, 2, EPlayRoutine::NextModelRecord);
			case Ending:
				if (Show.Numbers.Pending() != 0)
				{
					return EShowFlow::Done;
				}
				return Call(Routine, 2, EPlayRoutine::Finish);
			default:
				return EShowFlow::Done;
			}
		default:
			return EShowFlow::Done;
		}
	}

	EShowFlow FBattlePlayback::RunReactionPose(FShowRoutine&)
	{
		FSprite* Target = SlotSprite(ReactionSlot);
		if (Target == nullptr)
		{
			Fail(static_cast<uint8>(EBattleShowFault::EndedTask));
			return EShowFlow::Done;
		}
		return Animation(*Target) == ReactionAnimation ? EShowFlow::Frame : EShowFlow::Done;
	}

	EShowFlow FBattlePlayback::RunStandAndNext(FShowRoutine& Routine)
	{
		switch (Routine.Resume)
		{
		case 0:
		{
			FSprite* Sprite = SetPerformer(Record().Actor);
			if (Sprite == nullptr)
			{
				Fail(static_cast<uint8>(EBattleShowFault::Playback), "a record's actor has neither sprite nor model");
				return EShowFlow::Done;
			}
			AimAtTargets(*Sprite);
			RecordSprite = Sprite;
			if (Show.Context->Party.bStoredCombo)
			{
				// A combo: both update twice a step, face each other, and the camera jumps to frame them, the actor
				// to stand off its target on the side it came from.
				FSprite& Target = *Sprite->Target;
				Target.TimeFlags |= SpriteTimeFlags::UpdateTwice;
				Sprite->TimeFlags |= SpriteTimeFlags::UpdateTwice;
				FaceEachOther(*Sprite, Target);
				FSprite& Faced = *Sprite->Target;
				const uint32 Slots = static_cast<uint32>(SlotBit(FBattleSprites::SlotOf(*Sprite)) | SlotBit(FBattleSprites::SlotOf(Faced)));
				Show.Camera.FrameSlots(Slots, Show.Battle.CameraSubjects(), Show.Gte);
				const int16 TargetX = High(Faced.Position[0]);
				const FBattleStagePoint& Home = Show.Context->Slots[static_cast<std::size_t>(FBattleSprites::SlotOf(Faced))].Position;
				int16 Offset = -StandOff;
				if (static_cast<uint32>(static_cast<int32>(TargetX)) == Home.X && static_cast<uint32>(static_cast<int32>(High(Faced.Position[2]))) == Home.Z)
				{
					if (TargetX <= High(Sprite->Position[0]))
					{
						Offset = StandOff;
					}
				}
				else if (static_cast<int32>(Home.X) <= TargetX)
				{
					Offset = StandOff;
				}
				Sprite->SavedPoint = {static_cast<int16>(TargetX + Offset), 0, High(Faced.Position[2])};
				Show.Camera.FrameSlots(Slots, Show.Battle.CameraSubjects(), Show.Gte);
				Show.Camera.Eye = Show.Camera.DesiredEye;
				Show.Camera.Target = Show.Camera.DesiredTarget;
				FaceEachOther(*Sprite, Faced);
			}
			return Call(Routine, 1, EPlayRoutine::ReloadImages);
		}
		case 1:
			return Call(Routine, 2, EPlayRoutine::NextRecord);
		default:
			return EShowFlow::Done;
		}
	}

	// The records --------------------------------------------------------------------------------------

	EShowFlow FBattlePlayback::RunNextRecord(FShowRoutine& Routine)
	{
		FBattlePlayState& State = Show.Battle.Play;
		FSprite& Sprite = *RecordSprite;
		switch (Routine.Resume)
		{
		case 0:
		{
			if (WaitRecords != 0)
			{
				--WaitRecords;
				return EShowFlow::Done;
			}
			// The member's command played as the record its player has it drawn as plays it.
			const uint8 Code = Show.Context->DrawnCode(FBattleSprites::SlotOf(Sprite), Record().Code);
			switch (Code)
			{
			case CastCode:
			case ItemCastCode:
				return Call(Routine, 3, EPlayRoutine::CastRecord, Code, Play.PerformerSlot);
			case StayCode:
			case FrameCode:
			case WaitCode:
			case HideBoxCode:
			case ShowBoxCode:
				ControlRecord(Sprite, Code);
				++State.Record;
				return EShowFlow::Done;
			case PauseCode:
				// A pause: the escape's animation and the camera still; the record is not passed.
				Show.Sprites.SetAnimation(Sprite, EscapeAnimation);
				Show.Camera.Frame(0, Show.Battle.CameraSubjects(), Show.Gte);
				return EShowFlow::Done;
			case BorrowCode:
			{
				// The performer takes another slot's look: that sprite's resource block, shared from here on, its texture
				// place, and room for as many parts. The room is new: the frame it shows has no parts until the next is built
				// (the console draws what its heap held there; a heap not yet used holds zeros).
				const FSprite* Other = SlotSprite(Record().Parameter);
				if (Other == nullptr)
				{
					Fail(static_cast<uint8>(EBattleShowFault::Playback), "a record gives its performer the look of a slot without a sprite");
					return EShowFlow::Done;
				}

				Play.bReady = true;
				Sprite.Listener = nullptr;
				Sprite.Resource = Other->Resource;
				Sprite.Extra.ModeTexture = Other->Extra.ModeTexture;
				Sprite.DrawFlags |= SpriteDrawFlags::PaletteUploadPending;
				Show.Sprites.SetPartCapacity(Sprite, Other->PartCapacity);
				Sprite.Parts = {};
				++State.Record;
				return EShowFlow::Done;
			}
			case StanceCode:
				Play.bReady = true;
				Sprite.Listener = nullptr;
				Sprite.NextWord = (Sprite.NextWord & ~0xFFu) | (Record().Parameter & 0xFFu);
				++State.Record;
				return EShowFlow::Done;
			case WalkCode:
				Play.bReady = true;
				if (Record().Parameter == 0)
				{
					Play.bReady = false;
					NextWaypoint(Sprite);
					Sprite.Listener = &AnimationEnd;
				}
				++State.Record;
				return EShowFlow::Done;
			case HomeCode:
				Routine.Resume = 2;
				return EShowFlow::Again;
			case HoldCode:
				Sprite.Listener = nullptr;
				Play.bReady = true;
				return Call(Routine, 1, EPlayRoutine::Collapse);
			default:
				break;
			}
			// An animation: the actor's attack, the target's reaction by its result.
			FSprite* Target = Sprite.Target;
			State.HitVoiced = 0;
			State.Actor = &Sprite;
			Sprite.Listener = &AnimationEnd;
			const uint8 Result = Target != nullptr ? Record().Results[static_cast<std::size_t>(FBattleSprites::SlotOf(*Target))] : 0;
			FSpriteExtra& Extra = Sprite.ExtraOf != nullptr ? const_cast<FSprite*>(Sprite.ExtraOf)->Extra : Sprite.Extra;
			Extra.Signal = Result;
			Extra.Owner = 0;
			const uint8 IdsAnimation = Record().IdsAnimation;
			if (IsDrawnAsId(Sprite) && (Code < FirstEffectCode || IdsAnimation != NoIdsAnimation))
			{
				// Fei drawn as Id: his one-press combos, and what would be an attack file's animation, from his own file.
				Show.Sprites.SetAnimation(Sprite, IdsAnimation != NoIdsAnimation ? IdsAnimation : FIdsBattleSprite::AttackAnimation(static_cast<uint8>(Code)));
			}
			else if (!IsResident(Sprite) || IsDrawnAsId(Sprite))
			{
				if (Code >= FirstEffectCode)
				{
					// The character's effect: cast from its own pose below its limit, else from the plain one; the cast named
					// the caster once it is done. Its codes count from the effects of the character whose attack file is drawn.
					const uint8 Character = Show.Context->DrawnOccupant(FBattleSprites::SlotOf(Sprite));
					const FBattleProgramTables& Program = *Show.Context->Program;
					if (Character >= FBattleProgramTables::EffectCharacterCount)
					{
						Fail(static_cast<uint8>(EBattleShowFault::Playback), "an effect code's caster has no effect table");
						return EShowFlow::Done;
					}
					const int32 Effect = Code - FirstEffectCode + Program.EffectBases[Character];
					Sprite.SetAnimationNumber(Effect < Program.EffectCastLimits[Character] ? OwnCastAnimation : CastAnimation);
					return Call(Routine, 4, EPlayRoutine::EffectCast, Effect, Play.PerformerSlot);
				}
				if (!bAttackFileInstalled && ActionFileData)
				{
					Sprite.SoundSet = InstallAttackFile();
					Sprite.AlternateFile = &ActionFileData->Sprite();
				}
				Show.Sprites.SetAnimation(Sprite, Play.Actor == FBattleSprites::SlotOf(Sprite) ? ~static_cast<int32>(Code) : static_cast<int32>(Code));
			}
			else
			{
				Show.Sprites.SetAnimation(Sprite, Code);
				const uint32 Slots = static_cast<uint32>(SlotBit(FBattleSprites::SlotOf(Sprite)) | (Target != nullptr ? SlotBit(FBattleSprites::SlotOf(*Target)) : 0));
				Show.Camera.FrameSlots(Slots, Show.Battle.CameraSubjects(), Show.Gte);
			}
			Play.bReady = false;
			++State.Record;
			return EShowFlow::Done;
		}
		case 1:
			// The hold: once someone collapsed, the actor walks home; otherwise it stands in its stance.
			if (Returned == 0)
			{
				if ((Sprite.AnimationFlags & SpriteAnimationFlags::StateMask) != 0 || IsHidden(FBattleSprites::SlotOf(Sprite)))
				{
					return EShowFlow::Done;
				}
				Show.Sprites.SetAnimation(Sprite, Stance(Sprite));
				return EShowFlow::Done;
			}
			[[fallthrough]];
		case 2:
		{
			// The walk home.
			Play.bReady = false;
			FSprite* Actor = SetPerformer(Play.Actor);
			if (Actor == nullptr)
			{
				Fail(static_cast<uint8>(EBattleShowFault::Playback), "the walk home's actor has neither sprite nor model");
				return EShowFlow::Done;
			}
			AimAtTargets(*Actor);
			Actor->Listener = &AnimationEnd;
			SetState(WalkingHome);
			const FBattleStagePoint& Home = Show.Context->Slots[static_cast<std::size_t>(Play.PerformerSlot)].Position;
			if (static_cast<uint32>(static_cast<int32>(High(Actor->Position[0]))) == Home.X && static_cast<uint32>(static_cast<int32>(High(Actor->Position[2]))) == Home.Z)
			{
				AnimationEnd.OnAnimationEvent(*Actor);
				return EShowFlow::Done;
			}
			Actor->SavedPoint = {static_cast<int16>(Home.X), 0, static_cast<int16>(Home.Z)};
			Show.Sprites.SetAnimation(*Actor, WalkHomeAnimation);
			return EShowFlow::Done;
		}
		case 3:
			// After record F4's or F5's cast.
			++State.Record;
			return EShowFlow::Done;
		case 4:
			// After a character's effect's cast: its caster named, the record played.
			State.Caster = &Sprite;
			Play.bReady = false;
			++State.Record;
			return EShowFlow::Done;
		default:
			return EShowFlow::Done;
		}
	}

	void FBattlePlayback::ControlRecord(FSprite& Sprite, uint8 Code)
	{
		const FBattlePerformance& Played = Record();
		switch (Code)
		{
		case StayCode:
			Play.bReady = true;
			StayedWord() = static_cast<uint16>(StayedWord() | Played.Parameter);
			break;
		case FrameCode:
			Show.Camera.Frame(Played.Targets, Show.Battle.CameraSubjects(), Show.Gte);
			break;
		case WaitCode:
			Play.bReady = true;
			Sprite.Listener = nullptr;
			WaitRecords = Played.Parameter;
			break;
		case HideBoxCode:
		case ShowBoxCode:
			Play.bReady = true;
			Sprite.Listener = nullptr;
			if (Show.Hud != nullptr)
			{
				Show.Hud->ShowMessageBox(static_cast<int8>(Played.Parameter), Code == ShowBoxCode);
			}
			break;
		default:
			break;
		}
	}

	EShowFlow FBattlePlayback::RunNextModelRecord(FShowRoutine& Routine)
	{
		FBattlePlayState& State = Show.Battle.Play;
		switch (Routine.Resume)
		{
		case 0:
			if (WaitRecords != 0)
			{
				--WaitRecords;
				return EShowFlow::Done;
			}
			return Call(Routine, 1, EPlayRoutine::ReloadImages);
		case 1:
		{
			FSprite* Sprite = SetPerformer(Record().Actor);
			if (Sprite == nullptr)
			{
				Fail(static_cast<uint8>(EBattleShowFault::Playback), "a record's actor has neither sprite nor model");
				return EShowFlow::Done;
			}
			AimAtTargets(*Sprite);
			const int32 Slot = Play.PerformerSlot;
			const FBattlePerformance& Played = Record();
			SetState(EffectState);
			Play.bModelRecordRan = false;
			switch (Played.Code)
			{
			case ItemCastCode:
				return Call(Routine, 2, EPlayRoutine::CastRecord, Played.Code, Slot);
			case StayCode:
			case FrameCode:
			case WaitCode:
			case HideBoxCode:
			case ShowBoxCode:
				ControlRecord(*Sprite, Played.Code);
				SetState(ModelRecord);
				++State.Record;
				return EShowFlow::Done;
			case CastCode:
				// The effect the performer's sequences will ask for, and its animation within its file.
				SetState(ModelRecord);
				State.EffectAnimation = Played.Parameter >> 9 & 0x3F;
				Show.Objects.RequestedEffect = static_cast<int16>(Played.Parameter & 0x1FF);
				++State.Record;
				return EShowFlow::Done;
			case BorrowCode:
				SetState(ModelRecord);
				Show.Objects.Swap(FBattleSprites::SlotOf(*Sprite), Played.Parameter);
				++State.Record;
				return EShowFlow::Done;
			case StanceCode:
				Show.Objects.SetStance(Slot, static_cast<uint8>(Played.Parameter));
				SetState(ModelRecord);
				++State.Record;
				return EShowFlow::Done;
			case HomeCode:
				Show.Objects.Perform(Play.Actor, Played.Targets, HomeSequence);
				SetState(Ending);
				return EShowFlow::Done;
			case HoldCode:
				SetState(ModelRecord);
				return EShowFlow::Done;
			default:
				// A sequence (FD the walk): the record passed before it starts, the performer the actor for any other code.
				if (Played.Code != WalkCode)
				{
					Play.bModelRecordRan = true;
					State.Actor = Sprite;
				}
				++State.Record;
				Show.Objects.Perform(Slot, Played.Targets, Played.Code == WalkCode ? WalkSequence : Played.Code);
				return EShowFlow::Done;
			}
		}
		case 2:
			// After record F5's cast.
			SetState(ModelRecord);
			++State.Record;
			return EShowFlow::Done;
		default:
			return EShowFlow::Done;
		}
	}

	// The walk out -------------------------------------------------------------------------------------

	const FBattleWaypoint* FBattlePlayback::Waypoint()
	{
		if (Play.RouteIndex < 0 || Play.RouteIndex >= static_cast<int32>(FBattleContext::RouteLength))
		{
			// A walk past the route's last waypoint (a walk the rules planned none for): the original reads what follows it.
			Fail(static_cast<uint8>(EBattleShowFault::Playback), "a walk goes past the route's last waypoint");
			return nullptr;
		}
		return &Show.Context->Route[static_cast<std::size_t>(Play.RouteIndex)];
	}

	void FBattlePlayback::NextWaypoint(FSprite& Sprite)
	{
		const FBattleWaypoint* Next = Waypoint();
		if (Next == nullptr)
		{
			return;
		}
		if (Next->X == FBattleWaypoint::None && Next->Z == FBattleWaypoint::None)
		{
			Sprite.SavedPoint = {High(Sprite.Position[0]), 0, High(Sprite.Position[2])};
			Approach(Sprite, Sprite.Target);
			return;
		}
		Sprite.SavedPoint = {static_cast<int16>(Next->X), 0, static_cast<int16>(Next->Z)};
		WalkTo(Sprite, Next->bJump != 0 ? JumpAnimation : RunAnimation);
		++Play.RouteIndex;
	}

	void FBattlePlayback::WalkTo(FSprite& Sprite, int32 AnimationNumber)
	{
		Play.LastDistance = DistanceToGoal(Sprite);
		Show.Sprites.SetDirection(Sprite, HeadingToGoal(Sprite));
		Show.Sprites.SetFacing(Sprite, HeadingToGoal(Sprite));
		Show.Sprites.SetAnimation(Sprite, AnimationNumber);
		SetState(RouteLeg);
	}

	void FBattlePlayback::Approach(FSprite& Sprite, FSprite* Target)
	{
		const FBattleWaypoint* Next = Waypoint();
		if (Next == nullptr)
		{
			return;
		}
		if (Next->X != FBattleWaypoint::None || Next->Z != FBattleWaypoint::None)
		{
			NextWaypoint(Sprite);
			return;
		}
		if (Target == nullptr)
		{
			Fail(static_cast<uint8>(EBattleShowFault::Playback), "a walk has no target to go to");
			return;
		}
		// The last leg: from where the route left it to stand off the target on its own side.
		Sprite.Position[0] = Sprite.SavedPoint[0] << 16;
		Sprite.Position[2] = Sprite.SavedPoint[2] << 16;
		const int16 TargetX = High(Target->Position[0]);
		const int16 Offset = TargetX <= High(Sprite.Position[0]) ? StandOff : static_cast<int16>(-StandOff);
		Sprite.SavedPoint = {static_cast<int16>(TargetX + Offset), 0, High(Target->Position[2])};
		if (Sprite.SavedPoint[0] == High(Sprite.Position[0]) && Sprite.SavedPoint[2] == High(Sprite.Position[2]))
		{
			Arrive(Sprite, Target);
			return;
		}
		WalkTo(Sprite, JumpAnimation);
		SetState(LastLeg);
	}

	void FBattlePlayback::Arrive(FSprite& Sprite, FSprite* Target)
	{
		Sprite.Position[0] = Sprite.SavedPoint[0] << 16;
		Sprite.Position[2] = Sprite.SavedPoint[2] << 16;
		Play.bReady = true;
		SetState(Animating);
		Show.Sprites.SetAnimation(Sprite, Stance(Sprite));
		if (Target != nullptr)
		{
			FaceEachOther(Sprite, *Target);
		}
	}

	void FBattlePlayback::FaceEachOther(FSprite& First, FSprite& Second)
	{
		if (&First == &Second)
		{
			return;
		}
		Show.Sprites.SetFacing(First, HeadingTo(First, Second));
		Show.Sprites.SetDirection(First, HeadingTo(First, Second));
		if (Animation(Second) != CollapseAnimation)
		{
			Show.Sprites.SetFacing(Second, HeadingTo(Second, First));
			Show.Sprites.SetDirection(Second, HeadingTo(Second, First));
		}
	}

	// The performer and its targets ------------------------------------------------------------------------

	FSprite* FBattlePlayback::SetPerformer(int32 Slot)
	{
		FSprite*& Performer = Show.Battle.Play.Performer;
		if (Performer != nullptr && Play.PerformerSlot != Slot && !IsHidden(FBattleSprites::SlotOf(*Performer)))
		{
			Show.Sprites.SetAnimation(*Performer, Stance(*Performer));
		}
		Play.PerformerSlot = Slot;
		Performer = SlotSprite(Slot);
		return Performer;
	}

	void FBattlePlayback::AimAtTargets(FSprite& Sprite)
	{
		FBattlePlayState& State = Show.Battle.Play;
		State.TargetMask = Record().Targets;
		State.TargetCount = ListTargetSprites(State.TargetMask, State.Targets, &Sprite);
		if (State.TargetCount == 0)
		{
			State.Targets[0] = &Sprite;
		}
		FSprite& First = *State.Targets[0];
		Sprite.Target = &First;
		First.Target = &Sprite;
		Play.FirstTargetSlot = FBattleSprites::SlotOf(First);
		Show.Sprites.SetFacing(Sprite, HeadingTo(Sprite, *Sprite.Target));
		if (Animation(First) != CollapseAnimation)
		{
			Show.Sprites.SetFacing(First, HeadingTo(*Sprite.Target, Sprite));
		}
	}

	int32 FBattlePlayback::ListTargetSprites(uint16 Mask, std::array<FSprite*, 12>& Listed, FSprite* Aimer)
	{
		int32 Count = 0;
		for (int32 Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
		{
			FSprite* Sprite = SlotSprite(Slot);
			if ((Mask >> Slot & 1) != 0 && Sprite != nullptr)
			{
				Sprite->Target = Aimer;
				Listed[static_cast<std::size_t>(Count++)] = Sprite;
			}
		}
		Listed[static_cast<std::size_t>(Count)] = nullptr;
		return Count;
	}

	void FBattlePlayback::FaceHome(int32 Slot)
	{
		FSprite* Sprite = SlotSprite(Slot);
		if (Sprite == nullptr || Animation(*Sprite) == CollapseAnimation)
		{
			return;
		}
		const int32 Angle = Show.Context->Slots[static_cast<std::size_t>(Slot)].FacesLeft != 0 ? 0x800 : 0;
		Show.Sprites.SetFacing(*Sprite, Angle);
		Show.Sprites.SetDirection(*Sprite, Angle);
	}

	// The end ------------------------------------------------------------------------------------------

	EShowFlow FBattlePlayback::RunCollapse(FShowRoutine& Routine)
	{
		// The knocked-out not yet collapsed, listed as they fall (by slot: the list outlives the frames it waits).
		switch (Routine.Resume)
		{
		case 0:
		{
			const uint16 KnockedOut = Show.Context->Action.KnockedOut;
			Routine.Other = 0;
			std::array<FSprite*, 12> Listed{};
			int32 Count = ListTargetSprites(static_cast<uint16>(KnockedOut & (KnockedOut ^ CollapsedSlots)), Listed, Show.Battle.Play.Actor);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				CollapseSlots[static_cast<std::size_t>(Index)] = FBattleSprites::SlotOf(*Listed[static_cast<std::size_t>(Index)]);
			}
			int32 Collapsing = 0;
			while (--Count >= 0)
			{
				FSprite& Sprite = *Listed[static_cast<std::size_t>(Count)];
				const int32 Slot = FBattleSprites::SlotOf(Sprite);
				if ((Stayed() >> Slot & 1) != 0)
				{
					CollapseSlots[static_cast<std::size_t>(Count)] = -1;
					continue;
				}
				Sprite.Wait = 0;
				if (IsModel(Slot))
				{
					// Busy, it runs its collapse (given its slot's number as the targets' mask); the objects' idle wait follows.
					FBattleObject* Object = Show.Objects.Object(Slot);
					if (Object == nullptr)
					{
						Fail(static_cast<uint8>(EBattleShowFault::Playback), "a collapsing model's slot holds no object");
						return EShowFlow::Done;
					}
					Routine.Other = 1;
					Object->Playing = 1;
					Show.Objects.Perform(Slot, static_cast<uint16>(Slot), CollapseSequence);
				}
				else if (Animation(Sprite) != CollapseAnimation)
				{
					Show.Sprites.SetAnimation(Sprite, CollapseAnimation);
				}
				++Collapsing;
				CollapsedSlots = static_cast<uint16>(CollapsedSlots | SlotBit(Slot));
			}
			Routine.Value = Collapsing;
			[[fallthrough]];
		}
		case 1:
		{
			// Frames while a collapsing sprite of a file of its own still waits in its script.
			bool bWaiting = false;
			for (int32 Index = 0; Index < Routine.Value; ++Index)
			{
				const FSprite* Sprite = SlotSprite(CollapseSlots[static_cast<std::size_t>(Index)]);
				bWaiting = bWaiting || (Sprite != nullptr && Sprite->PrimaryFile != nullptr && Sprite->Wait != 0);
			}
			if (!bWaiting)
			{
				Returned = Routine.Value;
				return Routine.Other != 0 ? Call(Routine, 3, EPlayRoutine::IdleWait) : EShowFlow::Done;
			}
			if (Routine.Value == 0)
			{
				return EShowFlow::Done;
			}
			return Call(Routine, 2, EPlayRoutine::ReloadImages);
		}
		case 2:
			Routine.Resume = 1;
			return EShowFlow::Frame;
		default:
			return EShowFlow::Done;
		}
	}

	EShowFlow FBattlePlayback::RunWaitPoses(FShowRoutine&)
	{
		bool bBusy = false;
		for (int32 Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
		{
			const FSprite* Sprite = SlotSprite(Slot);
			if (Sprite == nullptr)
			{
				continue;
			}
			if (IsModel(Slot))
			{
				bBusy = bBusy || Sprite->Pc != FSprite::Stopped;
				continue;
			}
			const int32 PoseNumber = Pose(Slot);
			if (PoseNumber == OutPose)
			{
				continue;
			}
			const int32 Playing = Animation(*Sprite);
			const bool bSettled = std::find(SettledAnimations.begin(), SettledAnimations.end(), Playing) != SettledAnimations.end();
			if (!bSettled && !IsHidden(Slot) && Playing != PoseAnimations[static_cast<std::size_t>(PoseNumber)] && Playing != Stance(*Sprite))
			{
				bBusy = true;
			}
		}
		return bBusy ? EShowFlow::Frame : EShowFlow::Done;
	}

	EShowFlow FBattlePlayback::RunFinish(FShowRoutine& Routine)
	{
		FBattlePlayState& State = Show.Battle.Play;
		switch (Routine.Resume)
		{
		case 0:
			State.bCastEnded = false;
			State.EffectAnimation = 0;
			State.bHitVoices = false;
			if (EndBattleActionPlayback(*Show.Context))
			{
				// Billy's ammunition window closes: the window's frame, then its work's.
				Routine.Resume = FinishAmmunitionWork;
				return EShowFlow::Frame;
			}
			[[fallthrough]];
		case FinishCollapse:
		{
			Routine.Value = Play.NextReady;
			FSprite* Performer = Show.Battle.Play.Performer;
			if (Performer != nullptr)
			{
				Performer->Listener = nullptr;
			}
			return Call(Routine, 1, EPlayRoutine::Collapse);
		}
		case FinishAmmunitionWork:
			Routine.Resume = FinishAmmunitionClosed;
			return EShowFlow::Frame;
		case FinishAmmunitionClosed:
			CloseBattleAmmunitionWindow(*Show.Context);
			Routine.Resume = FinishCollapse;
			return EShowFlow::Again;
		case 1:
			return Call(Routine, 2, EPlayRoutine::WaitCounted);
		case 2:
			return Call(Routine, 3, EPlayRoutine::WaitPoses);
		case 3:
			return Call(Routine, 4, EPlayRoutine::WaitLoads);
		case 4:
			Show.Camera.ModeAfter = BattleCameraModes::Framing;
			Show.Numbers.CloseTotal();
			return Call(Routine, 5, EPlayRoutine::ReloadImages);
		case 5:
			// The gear models an effect dropped come back.
			return Call(Routine, 6, EPlayRoutine::ReloadGearModels);
		case 6:
			Show.Camera.bIgnoresHeights = false;
			Show.Camera.Pitch = CameraPitch;
			FreeController();
			return Call(Routine, 7, EPlayRoutine::PrepareNext, Routine.Value);
		case 7:
			if (State.bMusicLowered)
			{
				Show.SetMusicVolume(0x7F, 0x50);
			}
			State.bMusicLowered = false;
			for (FSprite* Sprite : Show.Battle.SlotSprites)
			{
				if (Sprite != nullptr)
				{
					Sprite->TimeFlags &= ~SpriteTimeFlags::UpdateTwice;
				}
			}
			return EShowFlow::Done;
		default:
			return EShowFlow::Done;
		}
	}

	EShowFlow FBattlePlayback::RunPrepareNext(FShowRoutine& Routine)
	{
		const int32 Slot = Routine.Value;
		switch (Routine.Resume)
		{
		case 0:
			Show.Sprites.CountedTasks = 0;
			Show.Sprites.bCountingTasks = false;
			return Call(Routine, 1, EPlayRoutine::IdleWait);
		case 1:
			if (Slot < 0 || Slot >= FBattleContext::SlotCount)
			{
				// No slot is ready: the original reads its record past the slot records.
				Fail(static_cast<uint8>(EBattleShowFault::Playback), "no slot is ready as the next turn is prepared");
				return EShowFlow::Done;
			}
			if (IsModel(Slot))
			{
				return Call(Routine, 3, EPlayRoutine::WaitLoads);
			}
			if (Slot >= FBattleContext::PartySlotCount)
			{
				return Call(Routine, 2, EPlayRoutine::WaitLoads);
			}
			if (SlotSprite(Slot) != nullptr)
			{
				return Call(Routine, 2, EPlayRoutine::ReadAttackFile, Slot);
			}
			[[fallthrough]];
		case 2:
			Show.Camera.Pitch = CameraPitch;
			return EShowFlow::Done;
		case 3:
			// A 3D member next: the battle's own sound bank back.
			Show.LoadSystemSoundBank();
			Routine.Resume = 2;
			return EShowFlow::Again;
		default:
			return EShowFlow::Done;
		}
	}

	// The loads ----------------------------------------------------------------------------------------

	EShowFlow FBattlePlayback::RunWaitLoads(FShowRoutine& Routine)
	{
		switch (Routine.Resume)
		{
		case 0:
			return Call(Routine, 1, EPlayRoutine::ReloadImages);
		case 1:
			return Call(Routine, 2, EPlayRoutine::WaitCounted);
		case 2:
			UninstallAttackFile();
			ReleaseActionFile();
			return EShowFlow::Done;
		default:
			return EShowFlow::Done;
		}
	}

	EShowFlow FBattlePlayback::RunReloadImages(FShowRoutine& Routine)
	{
		FBattlePlayState& State = Show.Battle.Play;
		switch (Routine.Resume)
		{
		case 0:
			if (!State.bReloadEffectImages)
			{
				return EShowFlow::Done;
			}
			// The effects' images (44/1) over what an effect put in their place, and a frame.
			if (!Show.UploadCommonImages())
			{
				return EShowFlow::Done;
			}
			Routine.Resume = 1;
			return EShowFlow::Frame;
		default:
			State.bReloadEffectImages = false;
			return EShowFlow::Done;
		}
	}

	EShowFlow FBattlePlayback::RunWaitCounted(FShowRoutine&)
	{
		return Show.Numbers.Pending() != 0 ? EShowFlow::Frame : EShowFlow::Done;
	}

	EShowFlow FBattlePlayback::RunReadAttackFile(FShowRoutine& Routine)
	{
		const int32 Slot = Routine.Value;
		switch (Routine.Resume)
		{
		case 0:
			return Call(Routine, 1, EPlayRoutine::WaitLoads);
		case 1:
		{
			const uint8 Character = Show.Context->DrawnOccupant(Slot);
			TResult<FBattleSpriteFile> File = Show.ReadAttacks(Character);
			if (!File)
			{
				Fail(static_cast<uint8>(EBattleShowFault::File));
				return EShowFlow::Done;
			}
			if (Show.Context->PartyLooks().HasPurpleHair(Character))
			{
				File->RecolourPurpleHair();
			}
			ReleaseActionFile();
			ActionFileData = std::make_unique<FBattleSpriteFile>(std::move(*File));
			ActionFileOwner = Slot;
			++FileReads;
			AddWritable(*ActionFileData);
			return EShowFlow::Done;
		}
		default:
			return EShowFlow::Done;
		}
	}

	EShowFlow FBattlePlayback::RunIdleWait(FShowRoutine& Routine)
	{
		switch (Routine.Resume)
		{
		case 0:
			Routine.Value = 0;
			Routine.Resume = 1;
			[[fallthrough]];
		case 1:
		{
			Routine.Value = Routine.Value != 0 || Show.Objects.HoldsExtension() ? 1 : 0;
			if (Show.Objects.IsPlaying() || Show.Numbers.Pending() != 0)
			{
				return EShowFlow::Frame;
			}
			// The reads are done at once: the extensions it saw are freed.
			if (Routine.Value != 0)
			{
				Show.Objects.FreeExtensions();
			}
			return EShowFlow::Done;
		}
		default:
			return EShowFlow::Done;
		}
	}

	int32 FBattlePlayback::InstallAttackFile()
	{
		bAttackFileInstalled = true;
		return Show.Battle.Install(*ActionFileData);
	}

	void FBattlePlayback::UninstallAttackFile()
	{
		if (!bAttackFileInstalled)
		{
			return;
		}
		if (ActionFileData)
		{
			Show.UnregisterSoundEffects(*ActionFileData);
		}
		bAttackFileInstalled = false;
	}

	namespace
	{
		bool PointsInto(const FBattleSpriteFile& File, const FSpriteFile* Pointer)
		{
			if (Pointer == nullptr)
			{
				return false;
			}
			if (Pointer == &File.Sprite())
			{
				return true;
			}
			for (std::size_t Section = 0; Section < File.Sections().size(); ++Section)
			{
				if (Pointer == File.SectionSprite(Section))
				{
					return true;
				}
			}
			return false;
		}
	}

	void FBattlePlayback::AddWritable(FBattleSpriteFile& File)
	{
		// The scripts write their variables into the file in memory, its sprite resources included.
		std::vector<FSpriteFile*>& Writable = Show.Battle.WritableFiles;
		Writable.push_back(&File.Sprite());
		for (std::size_t Section = 0; Section < File.Sections().size(); ++Section)
		{
			if (FSpriteFile* Resource = File.SectionSprite(Section))
			{
				Writable.push_back(Resource);
			}
		}
	}

	void FBattlePlayback::ReleaseActionFile()
	{
		bEffectInMemory = false;
		if (!ActionFileData)
		{
			return;
		}
		std::vector<FSpriteFile*>& Writable = Show.Battle.WritableFiles;
		const FBattleSpriteFile& Released = *ActionFileData;
		Writable.erase(std::remove_if(Writable.begin(), Writable.end(), [&Released](const FSpriteFile* File) { return PointsInto(Released, File); }), Writable.end());
		FreedFiles.push_back(std::move(ActionFileData));
		DropUnreferencedFiles();
	}

	bool FBattlePlayback::IsFreed(const FSpriteFile* File) const
	{
		return std::any_of(FreedFiles.begin(), FreedFiles.end(), [File](const std::unique_ptr<FBattleSpriteFile>& Freed) { return PointsInto(*Freed, File); });
	}

	void FBattlePlayback::DropUnreferencedFiles()
	{
		const auto Referenced = [this](const FBattleSpriteFile& File) {
			const FBattleSprites& Battle = Show.Battle;
			if (PointsInto(File, Battle.CommonEffects.File) || PointsInto(File, Battle.EffectResource.File))
			{
				return true;
			}
			for (const FKernelTaskEntry& Entry : Show.Sprites.Tasks())
			{
				if (!Entry.Sprite)
				{
					continue;
				}
				const FSprite& Sprite = Entry.Sprite->Sprite;
				for (const FSpriteFile* Pointer : {Sprite.CurrentFile, Sprite.PrimaryFile, Sprite.AlternateFile, Sprite.ScriptFile, Sprite.ByteTableFile, Sprite.ModelFile})
				{
					if (PointsInto(File, Pointer))
					{
						return true;
					}
				}
				if (Sprite.Resource != nullptr && PointsInto(File, Sprite.Resource->File))
				{
					return true;
				}
			}
			return false;
		};
		FreedFiles.erase(std::remove_if(FreedFiles.begin(), FreedFiles.end(), [&Referenced](const std::unique_ptr<FBattleSpriteFile>& Freed) { return !Referenced(*Freed); }),
			FreedFiles.end());
	}

	// The rules' calls ----------------------------------------------------------------------------------

	void FBattlePlayback::Begin(bool bStays, int32 Actor, int32 Target, int32 NextReady)
	{
		Show.Camera.ModeAfter = BattleCameraModes::Still;
		if (bController)
		{
			// The original traps when a playback begins while one plays.
			Fail(static_cast<uint8>(EBattleShowFault::Playback), "a playback begins while one plays");
			return;
		}
		if (!Rules.Push(static_cast<uint8>(EPlayRoutine::Begin), Actor, (bStays ? 0x10000 : 0) | (Target & 0xFF) << 8 | (NextReady & 0xFF)))
		{
			Fail(static_cast<uint8>(EBattleShowFault::Playback), "the show's routines nest past their room");
			return;
		}
		Run(Rules);
	}

	EShowFlow FBattlePlayback::RunBegin(FShowRoutine& Routine)
	{
		const int32 Actor = Routine.Value;
		const bool bStays = (Routine.Other & 0x10000) != 0;
		const int32 NextReady = Routine.Other & 0xFF;
		FBattlePlayState& State = Show.Battle.Play;
		switch (Routine.Resume)
		{
		case 0:
			// Two whole frames first.
			Routine.Resume = 1;
			return EShowFlow::Frame;
		case 1:
			Routine.Resume = 2;
			return EShowFlow::Frame;
		case 2:
		{
			State.bCastEnded = false;
			StayedWord() = static_cast<uint16>(StayedWord() & ~SlotBit(Actor));
			CreateController();
			Play.NextReady = NextReady;
			Play.Actor = Actor;
			FSprite* Sprite = SetPerformer(Actor);
			if (Sprite == nullptr)
			{
				Fail(static_cast<uint8>(EBattleShowFault::Playback), "a playback's actor has neither sprite nor model");
				return EShowFlow::Done;
			}
			AimAtTargets(*Sprite);
			State.EffectAnimation = 0;
			if (IsModel(Actor))
			{
				return Call(Routine, 4, EPlayRoutine::WaitLoads);
			}
			Show.UnloadSystemSoundBank();
			State.bSystemBankLoaded = false;
			Show.Camera.Pitch = CameraPitch;
			if (!IsResident(*Sprite))
			{
				State.bHitVoices = false;
				if (!ActionFileData || ActionFileOwner != FBattleSprites::SlotOf(*Sprite))
				{
					return Call(Routine, 3, EPlayRoutine::ReadAttackFile, FBattleSprites::SlotOf(*Sprite));
				}
			}
			else
			{
				if (!State.bHitVoices)
				{
					Show.LoadEnemySoundBank();
				}
				State.bHitVoices = true;
			}
			[[fallthrough]];
		}
		case 3:
		{
			FSprite* Sprite = Show.Battle.Play.Performer;
			if (!bStays)
			{
				Sprite->Listener = &AnimationEnd;
				NextWaypoint(*Sprite);
			}
			else
			{
				SetState(Animating);
			}
			return EShowFlow::Done;
		}
		case 4:
			// A 3D actor: the battle's own bank, its approach (given the target's slot number as the targets' mask), then its
			// walk on the first record's targets, or at once the next record when it stays.
			Show.LoadSystemSoundBank();
			SetState(EffectState);
			Show.Objects.Perform(Actor, static_cast<uint16>(Routine.Other >> 8 & 0xFF), ApproachSequence);
			Show.Camera.Pitch = CameraPitch;
			if (!bStays)
			{
				Show.Objects.Perform(Actor, Record().Targets, WalkSequence);
			}
			else
			{
				SetState(ModelRecord);
			}
			return EShowFlow::Done;
		default:
			return EShowFlow::Done;
		}
	}

	void FBattlePlayback::Cancel()
	{
		if (!bController)
		{
			return;
		}
		FSprite* Sprite = SetPerformer(Play.Actor);
		if (Sprite == nullptr)
		{
			FreeController();
			return;
		}
		const int32 Slot = FBattleSprites::SlotOf(*Sprite);
		if (IsModel(Slot))
		{
			Show.Objects.Perform(Slot, 0, CancelSequence);
			FreeController();
			return;
		}
		// Back at its place on the floor at once, in its stance.
		const FBattleStagePoint& Home = Show.Context->Slots[static_cast<std::size_t>(Play.PerformerSlot)].Position;
		Sprite->Position[0] = static_cast<int32>(static_cast<uint32>(Home.X) << 16);
		Sprite->Position[2] = static_cast<int32>(static_cast<uint32>(Home.Z) << 16);
		Show.Battle.SnapToFloor(*Sprite);
		Sprite->Position[1] = static_cast<int32>(static_cast<uint32>(static_cast<uint16>(Sprite->FloorY)) << 16);
		Show.Sprites.SetAnimation(*Sprite, Stance(*Sprite));
		Sprite->Listener = nullptr;
		FreeController();
	}

	void FBattlePlayback::CreateController()
	{
		bController = true;
		Play = FBattlePlayController{};
		Show.Battle.Play.Performer = nullptr;
		Show.Battle.Play.Record = 0;
		Show.Numbers.bPlaying = true;
		Show.Sprites.CountedTasks = 0;
		Show.Sprites.bCountingTasks = true;
	}

	void FBattlePlayback::FreeController()
	{
		bController = false;
		Show.Numbers.bPlaying = false;
		Show.Sprites.CountedTasks = 0;
		Show.Sprites.bCountingTasks = false;
	}

	// Reads of the battle's state ----------------------------------------------------------------------------

	FBattlePerformance& FBattlePlayback::Record()
	{
		const int32 Index = Show.Battle.Play.Record;
		if (Index < 0 || Index >= FBattleContext::PerformanceCount)
		{
			Fail(static_cast<uint8>(EBattleShowFault::Playback), "a record lies past the performances' table");
			return Show.Context->Performances[0];
		}
		return Show.Context->Performances[static_cast<std::size_t>(Index)];
	}

	FSprite* FBattlePlayback::SlotSprite(int32 Slot) const
	{
		return Slot >= 0 && Slot < FBattleContext::SlotCount ? Show.Battle.SlotSprites[static_cast<std::size_t>(Slot)] : nullptr;
	}

	bool FBattlePlayback::IsModel(int32 Slot) const
	{
		return Show.Context->Slots[static_cast<std::size_t>(Slot)].InGear != 0;
	}

	bool FBattlePlayback::IsHidden(int32 Slot) const
	{
		return Show.Context->Slots[static_cast<std::size_t>(Slot)].Presence != 0;
	}

	bool FBattlePlayback::IsResident(const FSprite& Sprite)
	{
		return Sprite.Resource != nullptr && Sprite.Resource->File != nullptr && Sprite.Resource->File->IsResident(Sprite.Resource->Tables);
	}

	bool FBattlePlayback::IsDrawnAsId(const FSprite& Sprite) const
	{
		const int32 Slot = FBattleSprites::SlotOf(Sprite);
		return Slot >= 0 && Slot < FBattleContext::PartySlotCount && IsResident(Sprite)
			&& Show.Context->PartyLooks().IsId(Show.Context->PartyCharacters[static_cast<std::size_t>(Slot)]);
	}

	int32 FBattlePlayback::HeadingTo(const FSprite& Sprite, const FSprite& Other) const
	{
		return Show.Battle.HeadingBetween(Other, Sprite);
	}

	int32 FBattlePlayback::HeadingToGoal(const FSprite& Sprite) const
	{
		const int32 Across = Sprite.SavedPoint[2] - High(Sprite.Position[2]);
		const int32 Along = Sprite.SavedPoint[0] - High(Sprite.Position[0]);
		return static_cast<int16>(-GteRatan2(Across, Along, *Show.Battle.Arctangents) & 0xFFF);
	}

	int32 FBattlePlayback::DistanceToGoal(const FSprite& Sprite) const
	{
		// The GTE squares each difference's low 16 bits: far from the goal the distance wraps as the original's does.
		const int32 X = High(Sprite.Position[0]) - Sprite.SavedPoint[0];
		const int32 Z = High(Sprite.Position[2]) - Sprite.SavedPoint[2];
		return SquareRoot0(static_cast<int32>(static_cast<uint32>(GteSquare(X)) + static_cast<uint32>(GteSquare(Z))));
	}

	int32 FBattlePlayback::Pose(int32 Slot) const
	{
		const FBattleCombatant& Combatant = Show.Context->Combatants[static_cast<std::size_t>(Slot)];
		const uint16 Status = Combatant.Status;
		if ((Status & 0x4000) != 0)
		{
			return OutPose;
		}
		if ((Status & 0x8000) != 0)
		{
			return 1;
		}
		if ((Status & 0x2000) != 0)
		{
			return 2;
		}
		if ((Combatant.Mental & 0x1000) != 0)
		{
			return 3;
		}
		if ((Combatant.Mental & 0x2000) != 0)
		{
			return 4;
		}
		if ((Status & 0x800) != 0)
		{
			return 5;
		}
		if ((Status & 0x1000) != 0)
		{
			return 6;
		}
		if ((Status & 2) != 0)
		{
			return 3;
		}
		if (((Combatant.Buffs | Combatant.PermanentBuffs) & 0x8000) != 0)
		{
			return 7;
		}
		return Combatant.Hp < (Combatant.MaxHp >> 3) ? 5 : 0;
	}
}
