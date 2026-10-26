#include "Battle/BattleEventSteps.h"
#include "Battle/BattleGameState.h"
#include "Battle/BattlePartySteps.h"
#include "Battle/BattleRules.h"
#include "Battle/BattleSteps.h"
#include "Battle/BattleTurn.h"
#include "Text/MessageOverrides.h"

namespace Weltall::BattleEventSteps
{
	namespace
	{
		constexpr uint16 FirstCombatantRecord = 0xF3;
		constexpr uint8 ObjectRecordBias = 0x0D;
		constexpr uint8 SlotBitBias = 0x0D;
		constexpr uint8 ChuChu = 7;
		constexpr uint8 BoardedInBattle = 2;
		constexpr uint8 ConfirmEvent = 4;
		constexpr uint8 NoPortrait = 0xFF;
		constexpr uint8 MessageInputMode = 2;
		constexpr uint8 CommandInputMode = 1;
		constexpr uint8 PerformPlaying = 2;
		constexpr uint8 PerformEnded = 1;
		constexpr uint8 InfinityLevel = 4;
		constexpr uint8 InfinityTurns = 3;
		/** Infinity's turns share the Atk UP countdown. */
		constexpr std::size_t InfinityDuration = 6;
		constexpr uint16 InfinityLearned = 0x4000;
		/** The stance flag that keeps a gear's normal idle pose at low HP. */
		constexpr uint16 NoLowHpPose = 0x0001;
		constexpr uint8 CockpitFrames = 2;
		constexpr uint16 CentreX = 0xA0;
		constexpr uint16 TopY = 0x10;
		constexpr uint16 BottomY = 0x8C;

		namespace Style
		{
			constexpr uint16 PortraitRight = 0x1;
			constexpr uint16 NoPortrait = 0x2;
			constexpr uint16 Bottom = 0x4;
			constexpr uint16 NoWindow = 0x8;
			constexpr uint16 AtSize = 0x10;
		}

		enum EMessageStep : uint8
		{
			MessageStart,
			MessageGrowing,
			MessageDrawn,
			MessageQueued,
			MessageFreed,
			MessageClosed,
		};

		/** The frames a window growing 32 pixels a frame each way from nothing takes to reach its size (at least the one it starts in). */
		uint16 GrowFrames(uint16 Width, uint16 Height)
		{
			const uint32 Across = (static_cast<uint32>(Width) + 31) / 32;
			const uint32 Down = (static_cast<uint32>(Height) + 31) / 32;
			const uint32 Frames = Across > Down ? Across : Down;
			return static_cast<uint16>(Frames == 0 ? 1 : Frames);
		}

		FBattleEventRecord* EventRecord(FBattleContext& Context, uint32 Number)
		{
			if (Number >= FBattleEventWork::RecordCount)
			{
				Context.Fault = EBattleFault::EventRecordOutsideWork;
				return nullptr;
			}
			return &Context.Event.Records[Number];
		}

		/** The record an object operand names (F3h and up for objects 0-12). */
		FBattleEventRecord* ObjectRecord(FBattleContext& Context, uint16 Operand)
		{
			return EventRecord(Context, static_cast<uint8>(Operand + ObjectRecordBias));
		}

		/** The slot bit of a sequence's target operand (F3h and up for slots 0-10). */
		uint16 TargetMask(FBattleContext& Context, uint16 Operand)
		{
			return BattleRules::GameSlotBit(Context, static_cast<uint8>(Operand + SlotBitBias));
		}

		/** The frames a blocking call of the show takes: one more while it blocks, else the instruction goes on. */
		bool WaitForShow(FBattleContext& Context, uint8 NextStep)
		{
			Context.Event.Run.Step = NextStep;
			return ShowBlocks(Context);
		}

		/** A performance record cleared (the one the playback's index is at, set to the first). */
		void ClearFirstRecord(FBattleContext& Context)
		{
			Context.PerformanceIndex = 0;
			FBattlePerformance& Record = Context.Performances[0];
			for (std::size_t Slot = 0; Slot < FBattlePerformance::SlotCount; ++Slot)
			{
				Record.Amounts[Slot] = 0;
				Record.WholeAmounts[Slot] = 0;
				Record.Results[Slot] = 0xFF;
				Record.Totals[Slot] = 0;
				Record.TotalKinds[Slot] = 0xFF;
				Record.WholeTotals[Slot] = 0;
				Record.WholeTotalKinds[Slot] = 0xFF;
			}
		}

		/** An instruction that waits on a combatant's motion: it begins it once, then waits until the show reports its end. */
		int32 AwaitMotion(FBattleContext& Context, uint8 Slot, int32 Length, EBattleEventCue Kind, std::initializer_list<uint16> Args)
		{
			FBattleEventWork& Event = Context.Event;
			if (!Event.MotionStarted[Slot])
			{
				Event.Moving[Slot] = true;
				Event.MotionStarted[Slot] = true;
				Cue(Context, Kind, Args);
				return 0;
			}
			if (!Event.Moving[Slot])
			{
				Event.MotionStarted[Slot] = false;
				return Length;
			}
			return 0;
		}

		void MenuCue(FBattleContext& Context, EBattleMenuCue Kind, uint8 Slot = 0, uint8 Arg = 0)
		{
			if (Context.Show != nullptr)
			{
				Context.Show->PlayMenuCue(Kind, Slot, Arg);
			}
		}

		void DrawArrow(FBattleContext& Context)
		{
			FBattleEventWork& Event = Context.Event;
			--Event.ArrowFrame;
			if (Event.ArrowFrame < 0)
			{
				Event.ArrowFrame = 4;
			}
			const bool bPerfectWorks = Event.Layout == EBattleMessageLayout::PerfectWorks;
			const bool bFrench = Event.Layout == EBattleMessageLayout::French;
			// The French translation's cursor counts pixels, which its kernel gives the arrow 4 past.
			const int32 Cursor = bFrench ? Event.Text->CursorXUnits() + 4 : Event.Text->CursorXUnits() * 4 + (bPerfectWorks ? 0 : 2);
			Event.ArrowX = static_cast<int16>(Cursor + static_cast<int32>(Event.TextX));
			Event.ArrowY = static_cast<int16>(Event.Text->CurrentLine() * 14 + static_cast<int32>(Event.TextY) + (bPerfectWorks || bFrench ? 4 : 5));
			Event.bArrowShown = true;
			MenuCue(Context, EBattleMenuCue::EventArrow, 0, 1);
		}

		/** A frame of the window's growth; the HUD finds it grown in the last. */
		int32 GrowthFrame(FBattleContext& Context)
		{
			FBattleEventRun& Run = Context.Event.Run;
			--Run.Frames;
			if (Run.Frames == 0)
			{
				Context.Party.bFirstWindowGrown = true;
			}
			return TakeFrame;
		}

		/**
		 * A message with its window and portrait: opened (taking the frames its window takes to grow and
		 * one more), then each call the "more" arrow and the confirm button while it waits for them, and
		 * once its text is done it closes over a frame or two. 1 when it has closed, 0 while it shows.
		 */
		int32 ShowMessage(FBattleContext& Context, uint16 Number, uint8 Portrait, uint8 StyleByte)
		{
			FBattleEventWork& Event = Context.Event;
			FBattleEventRun& Run = Event.Run;
			switch (Run.Step)
			{
			case MessageStart:
			{
				Run.Style = StyleByte == 0 ? Event.Window.Style : StyleByte;
				if (Event.bMessageOpen)
				{
					break;
				}
				const uint16 Style = Run.Style;
				const bool bPerfectWorks = Event.Layout == EBattleMessageLayout::PerfectWorks;
				const bool bFrench = Event.Layout == EBattleMessageLayout::French;
				const bool bPortraitAsked = (Style & Style::NoPortrait) == 0 && Portrait != NoPortrait;
				const uint16 TextWidth = static_cast<uint16>(Event.Window.Width * (bPerfectWorks || bFrench ? 4 : 12));
				uint16 BoxWidth = static_cast<uint16>(TextWidth + 0x18);
				uint16 X = Event.Window.X;
				uint16 Y = Event.Window.Y;
				Event.ArrowFrame = 4;
				if (Event.Window.Y == FBattleEventWindow::Automatic)
				{
					Y = (Style & Style::Bottom) != 0 ? BottomY : TopY;
				}
				if (bPerfectWorks && bPortraitAsked)
				{
					Event.Window.Lines = 4;
				}
				if (bFrench && (Portrait != NoPortrait || Event.Window.Lines >= 5))
				{
					Event.Window.Lines = 4;
				}
				const uint16 Lines = bPerfectWorks || Event.Window.Lines < 5 ? Event.Window.Lines : uint16{4};
				const uint16 Height = static_cast<uint16>(Lines * 13 + 20);
				if (Event.Window.X == FBattleEventWindow::Automatic)
				{
					X = static_cast<uint16>(CentreX - (BoxWidth >> 1));
				}
				Run.MessageX = X;
				Run.MessageY = Y;
				Run.bPortrait = false;
				if ((Style & Style::NoWindow) == 0)
				{
					if (bPortraitAsked)
					{
						BoxWidth = static_cast<uint16>(TextWidth + 0x58);
						Run.bPortrait = true;
					}
					if (Event.Window.X == FBattleEventWindow::Automatic)
					{
						Run.MessageX = static_cast<uint16>(CentreX - (BoxWidth >> 1));
					}
					if (Run.bPortrait)
					{
						Cue(Context, EBattleEventCue::ShowPortrait, {Portrait, Style, Run.MessageX, Run.MessageY, BoxWidth});
					}
					const bool bGrows = (Style & Style::AtSize) == 0;
					Cue(Context, EBattleEventCue::OpenWindow, {Run.MessageX, Run.MessageY, BoxWidth, Height, static_cast<uint16>(bGrows ? 1 : 0)});
					if (bGrows)
					{
						Context.Party.bFirstWindowGrown = false;
						Run.Frames = GrowFrames(BoxWidth, Height);
						Run.Step = MessageGrowing;
						return GrowthFrame(Context);
					}
					Run.Step = MessageDrawn;
					return TakeFrame;
				}
				Run.Step = MessageQueued;
				[[fallthrough]];
			}
			case MessageGrowing:
			case MessageDrawn:
			{
				if (Run.Step == MessageGrowing && Run.Frames > 0)
				{
					return GrowthFrame(Context);
				}
				if (Run.Step == MessageDrawn && !Context.Party.bFirstWindowGrown)
				{
					Context.Fault = EBattleFault::EventHangs;
					return 0;
				}
				if (Run.Step != MessageQueued && Run.bPortrait)
				{
					Event.bPortraitShown = true;
					if ((Run.Style & Style::PortraitRight) == 0)
					{
						Run.MessageX = static_cast<uint16>(Run.MessageX + 0x40);
					}
				}
				if (Number >= Event.Messages.Count())
				{
					Context.Fault = EBattleFault::EventMessageOutsideTable;
					return 0;
				}
				Event.TextY = static_cast<uint32>(Run.MessageY) + 8;
				Event.TextX = static_cast<uint32>(Run.MessageX) + 12;
				if (!Event.Text)
				{
					Event.Text.emplace(0, 0);
				}
				const int32 WidthUnits = Event.Layout == EBattleMessageLayout::Original ? Event.Window.Width * 3 : Event.Window.Width;
				Event.Text->Reinitialize(WidthUnits, Event.Window.Lines);
				if (Event.Replaced != nullptr)
				{
					Event.Replaced->CheckWindow(Number, Event.Messages.String(Number), WidthUnits, Event.Window.Lines);
				}
				MenuCue(Context, EBattleMenuCue::EventText);
				Event.Text->HideBox();
				Event.Text->Reset();
				Event.Text->Queue(Event.Messages.String(Number));
				Event.bMessageShown = true;
				Event.bMessageOpen = true;
				Run.Step = MessageQueued;
				return TakeFrame;
			}
			case MessageQueued:
				break;
			case MessageFreed:
				Event.bPortraitShown = false;
				if ((Run.Style & Style::NoWindow) == 0)
				{
					Cue(Context, EBattleEventCue::CloseWindow, {});
					Run.Step = MessageClosed;
					return TakeFrame;
				}
				[[fallthrough]];
			case MessageClosed:
				Event.bMessageOpen = false;
				Event.Window = FBattleEventWindow{};
				Run.Step = 0;
				return 1;
			default:
				return 0;
			}
			if (Event.Text && Event.Text->IsStopped())
			{
				if ((Run.Style & Style::NoWindow) == 0)
				{
					DrawArrow(Context);
				}
				Event.bKeyWait = true;
				if (Context.Party.Event == ConfirmEvent)
				{
					Event.Text->Continue();
					Event.bKeyWait = false;
					Event.bArrowShown = false;
					MenuCue(Context, EBattleMenuCue::EventArrow, 0, 0);
				}
			}
			if (!Event.Text || !Event.Text->HasText())
			{
				Event.bMessageShown = false;
				if (Event.Text)
				{
					Event.Text->Clear();
				}
				Run.Step = MessageFreed;
				return TakeFrame;
			}
			Run.Step = 0;
			return 0;
		}

		/** The event's music: the song stops, the music loaded before is freed over a frame, then File plays at Volume. */
		int32 StartMusic(FBattleContext& Context, std::size_t Pc, uint8 Volume)
		{
			FBattleEventWork& Event = Context.Event;
			FBattleEventRun& Run = Event.Run;
			if (Run.Step == 0)
			{
				FetchWords(Context, Pc, 1);
				Cue(Context, EBattleEventCue::StopSong, {});
				if (Event.bMusicLoaded)
				{
					Cue(Context, EBattleEventCue::FreeMusic, {});
					Run.Step = 1;
					return TakeFrame;
				}
			}
			const uint16 File = Event.Operands[0];
			Event.bMusicLoaded = true;
			Event.MusicFile = File;
			Event.MusicVolume = Volume;
			Cue(Context, EBattleEventCue::StartMusic, {File, Volume});
			Run.Step = 0;
			return 3;
		}

		void BoardFeiGear(FBattleContext& Context)
		{
			constexpr uint8 Fei = 0;
			Context.Combatants[Fei].Gear = 0;
			BattleGameState::Byte(*Context.GameState, GameStateLayout::CharacterOffset(Fei) + GameStateLayout::Character::Gear) = 0;
			BattleParty::PlaceBoardedGear(Context, Fei);
			if (Context.Fault == EBattleFault::None && Context.Show != nullptr)
			{
				Context.Show->BoardGear(Fei);
			}
		}

		void FinishFeiBoarding(FBattleContext& Context)
		{
			constexpr uint8 Fei = 0;
			Context.ChangedSlots[Fei] = 1;
			Context.Combatants[Fei].BattleFlags |= BattleFlags::InGear;
			BattleSteps::LeaveGroup(Context, Fei);
			Context.ApRecords[Fei].InGear = BoardedInBattle;
			Context.Slots[Fei].InGear = 1;
			MenuCue(Context, EBattleMenuCue::BoardedPanel, Fei);
		}

		/** SetOrder with FEh: the actor runs first, the others keep their order. */
		void MoveToFront(FBattleContext& Context, uint8 Actor)
		{
			std::array<uint8, FBattleEventWork::RecordCount>& Order = Context.Event.Order;
			const std::array<uint8, FBattleEventWork::RecordCount> Before = Order;
			Order[0] = Actor;
			std::size_t Next = 1;
			for (const uint8 Other : Before)
			{
				if (Other != Actor)
				{
					if (Next >= Order.size())
					{
						Context.Fault = EBattleFault::EventRecordOutsideWork;
						return;
					}
					Order[Next++] = Other;
				}
			}
		}
	}

	int32 ExecuteBattleOp(FBattleContext& Context, uint8 Actor, uint8 Opcode, std::size_t Pc)
	{
		FBattleEventWork& Event = Context.Event;
		FBattleEventRun& Run = Event.Run;
		FBattleEventRecord& Self = Event.Records[Actor];
		const std::array<uint16, FBattleEventWork::OperandCount>& Operand = Event.Operands;
		switch (Opcode)
		{
		case 0x18:
		{
			const int32 Shown = ShowMessage(Context, CodeWord(Context, Pc + 1), Self.Portrait, CodeByte(Context, Pc + 3));
			return Shown == 1 ? 4 : Shown;
		}
		case 0x19:
		{
			const int32 Shown = ShowMessage(Context, CodeWord(Context, Pc + 2), CodeByte(Context, Pc + 1), CodeByte(Context, Pc + 4));
			return Shown == 1 ? 5 : Shown;
		}
		case 0x1A:
		{
			FetchWords(Context, Pc, 5);
			const FBattleEventWindow Defaults;
			FBattleEventWindow& Window = Event.Window;
			Window.X = Operand[0] != 0 ? Operand[0] : Defaults.X;
			Window.Y = Operand[1] != 0 ? Operand[1] : Defaults.Y;
			Window.Width = Operand[2] != 0 ? Operand[2] : Defaults.Width;
			Window.Lines = Operand[3] != 0 ? Operand[3] : Defaults.Lines;
			Window.Style = Operand[4];
			return 11;
		}
		case 0x1B:
		{
			uint8 Portrait = CodeByte(Context, Pc + 1);
			if (Portrait >= FirstCombatantRecord)
			{
				const std::size_t Slot = Portrait - FirstCombatantRecord;
				if (Slot >= Context.PartyCharacters.size())
				{
					Context.Fault = EBattleFault::IndexOutsideTable;
					return 0;
				}
				Portrait = Context.PartyCharacters[Slot];
			}
			Self.Portrait = Portrait;
			return 2;
		}
		case 0x1C:
		case 0x1D:
			Context.Party.InputMode = Opcode == 0x1C ? MessageInputMode : CommandInputMode;
			return 1;
		case 0x1E:
		case 0x1F:
		{
			FetchWords(Context, Pc, 1);
			const uint16 Shade = Opcode == 0x1E ? uint16{0xFF} : uint16{0};
			Cue(Context, EBattleEventCue::Fade, {Operand[0], 2, Shade, Shade, Shade});
			return 3;
		}
		case 0x20:
			Event.Requests.bRunsAfterBattle = true;
			Event.bDone = true;
			return 1;
		case 0x21:
			Event.Requests.bNoResults = true;
			return 1;
		case 0x22:
			Event.ReturnCountdown = 2;
			return 1;
		case 0x23:
		{
			if (Run.Step == 0)
			{
				FetchWords(Context, Pc, 3);
			}
			const uint32 Number = static_cast<uint32>(Operand[0]) - FirstCombatantRecord;
			FBattleEventRecord* Performer = EventRecord(Context, Number);
			if (Performer == nullptr)
			{
				return 0;
			}
			if (Run.Step == 0)
			{
				if (Performer->PerformState == 0)
				{
					Performer->PerformState = PerformPlaying;
					const uint16 Targets = TargetMask(Context, Operand[1]);
					if (IsShownSlot(Context, Number) && Context.Fault == EBattleFault::None)
					{
						Cue(Context, EBattleEventCue::PerformSignalled, {static_cast<uint16>(Number), Targets, Operand[2]});
					}
					return 0;
				}
				if (Performer->PerformState != PerformEnded)
				{
					return 0;
				}
				Cue(Context, EBattleEventCue::FinishPerform, {});
				if (WaitForShow(Context, 1))
				{
					return TakeFrame;
				}
			}
			else if (ShowBlocks(Context))
			{
				return TakeFrame;
			}
			Performer->PerformState = 0;
			Run.Step = 0;
			return 7;
		}
		case 0x24:
			FetchWords(Context, Pc, 2);
			Event.Requests.Chain = FBattleChain{static_cast<uint8>(Operand[0] + 1), static_cast<uint8>(Operand[1])};
			return 5;
		case 0x25:
			Event.Requests.bDefeatScripted = true;
			return 1;
		case 0x26:
		{
			FetchWords(Context, Pc, 4);
			FGameState& State = *Context.GameState;
			BattleGameState::SetWord(State, GameStateLayout::PlaceOffset, Operand[0]);
			BattleGameState::SetWord(State, GameStateLayout::CameraYawOffset, Operand[1]);
			BattleGameState::SetWord(State, GameStateLayout::WorldMapEntranceOffset, Operand[2]);
			BattleGameState::SetWord(State, GameStateLayout::SceneOffset, Operand[3]);
			Event.Requests.bFieldStartsFresh = true;
			return 9;
		}
		case 0x27:
			FetchWords(Context, Pc, 4);
			Event.Requests.Movie = FBattleMovieRequest{static_cast<uint8>(Operand[0] | 0x80), static_cast<uint8>(Operand[1]), static_cast<uint8>(Operand[2]), Operand[3]};
			return 9;
		case 0x28:
			Cue(Context, EBattleEventCue::Fade, {CodeByte(Context, Pc + 5), CodeByte(Context, Pc + 1), CodeByte(Context, Pc + 2), CodeByte(Context, Pc + 3), CodeByte(Context, Pc + 4)});
			return 6;
		case 0x29:
			FetchWords(Context, Pc, 4);
			Cue(Context, EBattleEventCue::Shake, {Operand[0], Operand[1], Operand[2], Operand[3]});
			return 9;
		case 0x2A:
		{
			FetchWords(Context, Pc, 2);
			const FBattleEventRecord* Object = ObjectRecord(Context, Operand[0]);
			if (Object != nullptr && Object->bObjectLoaded)
			{
				Cue(Context, EBattleEventCue::ObjectAnim, {static_cast<uint8>(Operand[0] + ObjectRecordBias), Operand[1]});
			}
			return 5;
		}
		case 0x2B:
			if (!Self.bWaiting)
			{
				FetchWords(Context, Pc, 1);
				Self.bWaiting = true;
				Self.WaitCount = static_cast<int16>(Operand[0] << 1);
			}
			if (Self.WaitCount == 0)
			{
				Self.bWaiting = false;
				return 3;
			}
			return 0;
		case 0x2C:
		{
			const uint8 Argument = CodeByte(Context, Pc + 1);
			Self.OrderArgument = Argument;
			if (Argument == 0xFE)
			{
				MoveToFront(Context, Actor);
			}
			return 3;
		}
		case 0x2D:
		case 0x2E:
			return StartMusic(Context, Pc, Opcode == 0x2D ? uint8{0x7F} : uint8{0});
		case 0x2F:
			FetchWords(Context, Pc, 2);
			Event.MusicVolume = static_cast<uint8>(Operand[0]);
			Cue(Context, EBattleEventCue::MusicVolume, {Operand[0], Operand[1]});
			return 5;
		case 0x30:
			FetchWords(Context, Pc, 1);
			Cue(Context, EBattleEventCue::MusicVolume, {static_cast<uint16>(Operand[0] == 0 ? Event.MusicVolume : 0), 0});
			return 3;
		case 0x31:
			FetchWords(Context, Pc, 4);
			Cue(Context, EBattleEventCue::PlaySound, {Operand[0], Operand[1], Operand[2], static_cast<uint16>(Operand[3] == 0 ? 0 : 1)});
			return 9;
		case 0x32:
		case 0x34:
			return 1;
		case 0x33:
			if (Run.Step == 0)
			{
				if (!Event.bMusicLoaded)
				{
					return 1;
				}
				Cue(Context, EBattleEventCue::StopMusic, {});
				Run.Step = 1;
				return TakeFrame;
			}
			Cue(Context, EBattleEventCue::FreeMusic, {});
			Event.bMusicLoaded = false;
			Run.Step = 0;
			return 1;
		case 0x35:
		{
			FetchWords(Context, Pc, 2);
			FBattleEventRecord* Object = ObjectRecord(Context, Operand[0]);
			if (Object != nullptr && !Object->bObjectLoaded)
			{
				Cue(Context, EBattleEventCue::LoadObject, {static_cast<uint8>(Operand[0] + ObjectRecordBias), Operand[1]});
				Object->bObjectLoaded = true;
			}
			return 5;
		}
		case 0x36:
		case 0x40:
		{
			FetchWords(Context, Pc, 1);
			FBattleEventRecord* Object = ObjectRecord(Context, Operand[0]);
			if (Object != nullptr && Object->bObjectLoaded)
			{
				Cue(Context, EBattleEventCue::FreeObject, {static_cast<uint8>(Operand[0] + ObjectRecordBias)});
				Object->bObjectLoaded = false;
			}
			if (Opcode == 0x40 && Context.Fault == EBattleFault::None)
			{
				Cue(Context, EBattleEventCue::LockCamera, {});
			}
			return 3;
		}
		case 0x37:
			Event.Requests.bEndedBattle = true;
			Event.bDone = true;
			Context.End = BattleEnd::Won;
			return 1;
		case 0x38:
		{
			FetchWords(Context, Pc, 3);
			const uint32 Slot = static_cast<uint32>(Operand[0]) - FirstCombatantRecord;
			const uint16 Targets = TargetMask(Context, Operand[1]);
			if (IsShownSlot(Context, Slot) && Context.Fault == EBattleFault::None)
			{
				Cue(Context, EBattleEventCue::Perform, {static_cast<uint16>(Slot), Targets, Operand[2]});
			}
			return 7;
		}
		case 0x39:
			if (Run.Step == 0)
			{
				BoardFeiGear(Context);
				if (Context.Fault != EBattleFault::None)
				{
					return 0;
				}
				if (WaitForShow(Context, 1))
				{
					return TakeFrame;
				}
			}
			else if (ShowBlocks(Context))
			{
				return TakeFrame;
			}
			FinishFeiBoarding(Context);
			Run.Step = 0;
			return 1;
		case 0x3A:
		case 0x3B:
		case 0x3C:
		case 0x3D:
		{
			FetchWords(Context, Pc, Opcode == 0x3A ? 2 : 1);
			const uint8 Slot = CombatantSlot(Context, Operand[0]);
			if (IsShownSlot(Context, Slot))
			{
				const EBattleEventCue Kind = Opcode == 0x3A ? EBattleEventCue::SetAnim : Opcode == 0x3B ? EBattleEventCue::RestoreAnim : Opcode == 0x3C ? EBattleEventCue::StopMotion : EBattleEventCue::StopTurn;
				if (Opcode == 0x3A)
				{
					Cue(Context, Kind, {Slot, Operand[1]});
				}
				else
				{
					Cue(Context, Kind, {Slot});
				}
			}
			return Opcode == 0x3A ? 5 : 3;
		}
		case 0x3E:
		case 0x3F:
		{
			FetchWords(Context, Pc, 4);
			const uint8 Slot = CombatantSlot(Context, Operand[0]);
			if (!IsShownSlot(Context, Slot))
			{
				return 0;
			}
			return AwaitMotion(Context, Slot, 9, Opcode == 0x3E ? EBattleEventCue::Walk : EBattleEventCue::Jump, {Slot, Operand[1], Operand[2], Operand[3]});
		}
		case 0x41:
			FetchWords(Context, Pc, 3);
			Cue(Context, EBattleEventCue::SoundVolume, {Operand[0], Operand[1], static_cast<uint16>(Operand[2] == 0 ? 0 : 1)});
			return 7;
		case 0x42:
			if (Run.Step == 0)
			{
				PrepareGearTurn(Context, 0);
				Cue(Context, EBattleEventCue::OpenCommandHud, {0});
				if (Context.PartyCharacters[0] == ChuChu)
				{
					return 1;
				}
				Run.Step = 1;
				Run.Frames = CockpitFrames;
			}
			// The cockpit opens over two frames: one after its instruments, one after its fuel, and then it shows.
			if (Run.Frames > 0)
			{
				if (Run.Frames < CockpitFrames)
				{
					MenuCue(Context, EBattleMenuCue::CockpitFuel);
				}
				--Run.Frames;
				return TakeFrame;
			}
			MenuCue(Context, EBattleMenuCue::ShowCockpit);
			Run.Step = 0;
			return 1;
		case 0x43:
			Cue(Context, EBattleEventCue::CloseCommandHud, {0});
			if (Context.ApRecords[0].InGear == BoardedInBattle)
			{
				Context.ApRecords[0].InGear = 1;
			}
			return 1;
		case 0x44:
			Cue(Context, EBattleEventCue::CancelPerformSignals, {});
			return 1;
		case 0x45:
		{
			FetchWords(Context, Pc, 4);
			const uint8 Slot = CombatantSlot(Context, Operand[0]);
			const uint8 Target = CombatantSlot(Context, Operand[1]);
			ClearFirstRecord(Context);
			if (Target >= FBattlePerformance::SlotCount)
			{
				Context.Fault = EBattleFault::PerformanceOutsideRecords;
				return 0;
			}
			Context.Performances[0].Results[Target] = static_cast<uint8>(Operand[3]);
			if (!IsShownSlot(Context, Slot))
			{
				return 0;
			}
			return AwaitMotion(Context, Slot, 9, EBattleEventCue::Attack, {Slot, Target, Operand[2]});
		}
		case 0x46:
			if (Run.Step == 0)
			{
				ClearFirstRecord(Context);
				FetchWords(Context, Pc, 3);
				const uint8 Slot = CombatantSlot(Context, Operand[0]);
				const uint8 Target = CombatantSlot(Context, Operand[1]);
				if (!IsShownSlot(Context, Slot))
				{
					return 0;
				}
				Cue(Context, EBattleEventCue::SpecialAttack, {Slot, Target, Operand[2]});
				if (WaitForShow(Context, 1))
				{
					return TakeFrame;
				}
			}
			else if (ShowBlocks(Context))
			{
				return TakeFrame;
			}
			Run.Step = 0;
			return 7;
		case 0x47:
		case 0x48:
			if (Run.Step == 0)
			{
				if (Opcode == 0x48)
				{
					FetchWords(Context, Pc, 2);
					Cue(Context, EBattleEventCue::PlayVoice, {Operand[0], Operand[1]});
				}
				else
				{
					Cue(Context, EBattleEventCue::FinishLoads, {});
				}
				if (WaitForShow(Context, 1))
				{
					return TakeFrame;
				}
			}
			else if (ShowBlocks(Context))
			{
				return TakeFrame;
			}
			Run.Step = 0;
			return Opcode == 0x48 ? 5 : 1;
		case 0x49:
			FetchWords(Context, Pc, 1);
			Event.Requests.FieldReturn = static_cast<uint8>(Operand[0]);
			return 3;
		case 0x4A:
		{
			constexpr uint8 Fei = 0;
			FBattleCombatant& Member = Context.Combatants[Fei];
			Member.AttackLevel = InfinityLevel;
			Member.Durations[InfinityDuration] = InfinityTurns;
			const std::size_t Offset = BattleGameState::SkillsOffset(Fei, GameStateLayout::Skills::Special);
			BattleGameState::SetWord(*Context.GameState, Offset, static_cast<uint16>(BattleGameState::Word(*Context.GameState, Offset) | InfinityLearned));
			return 1;
		}
		case 0x4B:
		{
			FetchWords(Context, Pc, 1);
			const uint8 Slot = CombatantSlot(Context, Operand[0]);
			if (IsShownSlot(Context, Slot))
			{
				Context.Combatants[Slot].Absorption |= NoLowHpPose;
			}
			return 3;
		}
		default:
			Context.Fault = EBattleFault::UnknownEventOpcode;
			return 0;
		}
	}
}
