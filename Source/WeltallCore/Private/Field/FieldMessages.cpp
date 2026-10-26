#include "Field/FieldMessages.h"

#include "Field/EventContext.h"
#include "Game/GameStateNames.h"

namespace Weltall
{
	namespace
	{
		/** The player's button, logical Circle: physical Cross on the US release. */
		constexpr uint16 ConfirmButton = 0x20;
		constexpr uint16 UpButton = 0x1000;
		constexpr uint16 DownButton = 0x4000;
		constexpr int32 CursorFrames = 5;
		constexpr int16 MoreCursorWait = 2;
		/** Frames smaller than this while they open are drawn at this size. */
		constexpr int32 SmallestFrame = 16;
		constexpr int32 PortraitSize = 0x40;
		constexpr int32 LineHeight = 14;
		/**
		 * Where the choice cursor sits from the frame's left edge, with a portrait on the left and without; 12 pixels further
		 * left in the French translation's program, whose choices are indented by narrower spaces.
		 */
		constexpr int16 ChoiceCursorBesidePortrait = 0x5A;
		constexpr int16 ChoiceCursorAlone = 0x16;
		constexpr int16 VariableWidthChoiceCursorShift = 12;

		int32 Wrap(uint32 Value)
		{
			return static_cast<int32>(Value);
		}

		/** C's signed division by two, which rounds toward zero. */
		int32 Half(int32 Value)
		{
			return (Value + static_cast<int32>(static_cast<uint32>(Value) >> 31)) >> 1;
		}

		bool TakesButton(const FFieldMessageWindow& Window, bool bFocusPass)
		{
			return bFocusPass || Window.Age == 0;
		}

		/** The player's button: a choice ends on its cursor's line, which its owner keeps, and the text goes on. */
		void Confirm(FEventContext& Context, FFieldMessageWindow& Window)
		{
			Window.bChoice = false;
			if (FEventActorState* Owner = Context.StateOf(Window.Owner))
			{
				Owner->Choice = static_cast<uint8>(static_cast<uint8>(Window.ChoiceCursor) + static_cast<uint8>(Window.ChoiceFirst));
			}
			Window.Text->Continue();
		}

		void TypeText(FFieldMessages& Messages, FFieldMessageWindow& Window)
		{
			// The field queues the message again whenever the queue runs empty: the copy waits unread
			// while the text shows, and the window closes before it is taken.
			if (Window.Text->QueuedCount() == 0)
			{
				Window.Text->Queue(Window.Message);
			}
			Window.Text->Update(Messages.Text);
		}

		void MarkWaiting(FFieldMessageWindow& Window)
		{
			if (Window.Text->StopReason() != EMesStop::Typing && !Window.bChoice)
			{
				Window.bTextWaits = true;
			}
		}

		/** The frame as the opening leaves it this tick; each call moves the opening a step on. */
		FFieldScreenRect OpeningRect(const FFieldMessages& Messages, FFieldMessageWindow& Window)
		{
			int32 X = Window.Rect.X;
			int32 Y = Window.Rect.Y;
			int32 Width = Window.Rect.Width;
			int32 Height = Window.Rect.Height;
			if (Window.OpenTicks != 0)
			{
				const int32 Ticks = Messages.OpenTicks;
				const int32 Done = Ticks - Window.OpenTicks;
				const int32 GrownWidth = Wrap(static_cast<uint32>((Width << 16) / (Ticks * 2)) * static_cast<uint32>(Done));
				const int32 GrownHeight = Wrap(static_cast<uint32>((Height << 16) / (Ticks * 2)) * static_cast<uint32>(Done));
				X = X + Half(Width) - (GrownWidth >> 16);
				Width = Wrap(static_cast<uint32>(GrownWidth) << 1) >> 16;
				Y = Y + Half(Height) - (GrownHeight >> 16);
				Height = Wrap(static_cast<uint32>(GrownHeight) << 1) >> 16;
				if (Width < SmallestFrame)
				{
					X -= Half(SmallestFrame - Width);
					Width = SmallestFrame;
				}
				if (Height < SmallestFrame)
				{
					Y -= Half(SmallestFrame - Height);
					Height = SmallestFrame;
				}
				for (std::size_t Axis = 0; Axis < 2; ++Axis)
				{
					Window.OpenOffset[Axis] = Wrap(static_cast<uint32>(Window.OpenOffset[Axis]) + static_cast<uint32>(Window.OpenStep[Axis]));
				}
				Y += Window.OpenOffset[1] >> 16;
				X += Window.OpenOffset[0] >> 16;
			}
			return {static_cast<int16>(X), static_cast<int16>(Y), static_cast<int16>(Width), static_cast<int16>(Height)};
		}

		/** The window's frame, cursors and portrait, as the field draws them. */
		void PlaceFrame(const FFieldMessages& Messages, FFieldMessageWindow& Window)
		{
			FFieldMessageFrame& Frame = Window.Frame;
			const FFieldScreenRect Rect = OpeningRect(Messages, Window);
			Frame.Rect = Rect;
			Frame.bMoreCursor = false;
			if (Window.bTextWaits && Window.Age == 0 && Window.OpenTicks == 0 && (Window.Flags & FieldMessageWindowFlags::NoBackground) == 0 && !Window.bChoice)
			{
				if (Window.MoreCursorDelay == 0)
				{
					const FMesWindow& Text = *Window.Text;
					Frame.bMoreCursor = true;
					const FMesFont* Font = Messages.Text.Font;
					Frame.MoreCursorX = static_cast<int16>(Window.TextX + (Font != nullptr ? Font->CursorPixels(Text.CursorXUnits()) : Text.CursorXUnits() * 4));
					Frame.MoreCursorY = static_cast<int16>(Window.TextY + Text.CursorRow() * LineHeight + 4);
				}
				else
				{
					--Window.MoreCursorDelay;
				}
			}
			else
			{
				Window.MoreCursorDelay = MoreCursorWait;
			}

			const bool bRight = (Window.Flags & FieldMessageWindowFlags::PortraitOnRight) != 0;
			const int16 Width = Rect.Width - 4 < PortraitSize ? static_cast<int16>(Rect.Width - 8) : static_cast<int16>(PortraitSize);
			const int16 Height = Rect.Height - 4 < PortraitSize ? static_cast<int16>(Rect.Height - 8) : static_cast<int16>(PortraitSize);
			const int16 Left = bRight ? static_cast<int16>(Rect.X + Rect.Width - Width - 4) : static_cast<int16>(Rect.X + 4);
			Frame.PortraitX0 = bRight ? static_cast<int16>(Left + Width) : static_cast<int16>(Left - 1);
			Frame.PortraitX1 = bRight ? Left : static_cast<int16>(Left - 1 + Width);
			Frame.PortraitY0 = static_cast<int16>(Rect.Y + 4);
			Frame.PortraitY1 = static_cast<int16>(Rect.Y + 4 + Height);

			Frame.bChoiceCursor = Window.bChoice && Window.Age == 0 && Window.OpenTicks == 0;
			if (Frame.bChoiceCursor)
			{
				const bool bVariableWidth = Messages.Text.Font != nullptr && Messages.Text.Font->IsVariableWidth();
				Frame.ChoiceCursorX = static_cast<int16>(Rect.X + (Window.bPortrait && !bRight ? ChoiceCursorBesidePortrait : ChoiceCursorAlone)
					- (bVariableWidth ? VariableWidthChoiceCursorShift : 0));
				Frame.ChoiceCursorY = static_cast<int16>((Window.ChoiceCursor + Window.ChoiceFirst) * LineHeight + Rect.Y + 8);
			}
		}

		/** Up and Down move the frontmost window's choice cursor round its lines; the cursor's line is highlighted. */
		void UpdateChoice(FFieldMessageWindow& Window, uint16 Repeat)
		{
			if (!Window.bChoice || Window.OpenTicks != 0)
			{
				return;
			}
			if (Window.Age != 0)
			{
				Window.Text->ClearHighlight();
				return;
			}
			if ((Repeat & DownButton) != 0)
			{
				Window.ChoiceCursor = static_cast<int16>(Window.ChoiceCursor + 1);
				if (Window.ChoiceCount - 1 < Window.ChoiceCursor)
				{
					Window.ChoiceCursor = 0;
				}
			}
			if ((Repeat & UpButton) != 0)
			{
				Window.ChoiceCursor = static_cast<int16>(Window.ChoiceCursor - 1);
				if (Window.ChoiceCursor < 0)
				{
					Window.ChoiceCursor = static_cast<int16>(Window.ChoiceCount - 1);
				}
			}
			Window.Text->SetHighlightRow(Window.ChoiceCursor + Window.ChoiceFirst);
		}

		/**
		 * One window's turn: the button and the text once it has opened, then its frame and choice.
		 * The focused windows' pass tests the text's stop before typing, the others' after.
		 */
		void UpdateWindow(FEventContext& Context, FFieldMessageWindow& Window, bool bFocusPass, uint16 Pressed, uint16 Repeat)
		{
			FFieldMessages& Messages = Context.Messages;
			Window.bTextWaits = false;
			if (Window.OpenTicks == 0)
			{
				if (bFocusPass)
				{
					MarkWaiting(Window);
				}
				if ((Pressed & ConfirmButton) != 0 && TakesButton(Window, bFocusPass))
				{
					Confirm(Context, Window);
				}
				TypeText(Messages, Window);
				if (!bFocusPass)
				{
					MarkWaiting(Window);
				}
			}
			PlaceFrame(Messages, Window);
			UpdateChoice(Window, Repeat);
		}
	}

	void FFieldMessages::EnterMap()
	{
		for (FFieldMessageWindow& Window : Windows)
		{
			Window.Text.reset();
			Window.bOpen = false;
			Window.Age = FFieldMessageWindow::Free;
			Window.bFocus = false;
			Window.bCloseRequested = false;
			Window.Owner = FFieldMessageWindow::NoActor;
			Window.Speaker = FFieldMessageWindow::NoActor;
			Window.bChoice = false;
			Window.bTextWaits = false;
			Window.MoreCursorDelay = MoreCursorWait;
			Window.Frame = {};
		}
		ResetPortraits();
		OpenTicks = FullRateOpenTicks;
		OpenMask = 0;
	}

	void FFieldMessages::ResetPortraits()
	{
		Portraits.fill({});
		LastPortraitSlot = 0;
	}

	int32 FFieldMessages::FindOwnWindow(int32 Actor) const
	{
		for (int32 Index = 0; Index < WindowCount; ++Index)
		{
			const FFieldMessageWindow& Window = Windows[static_cast<std::size_t>(Index)];
			if (Window.Owner == Actor && Window.bOpen)
			{
				return Index;
			}
		}
		return -1;
	}

	void FFieldMessages::RequestClose(int32 Actor)
	{
		const int32 Index = FindOwnWindow(Actor);
		if (Index >= 0)
		{
			Windows[static_cast<std::size_t>(Index)].bCloseRequested = true;
		}
	}

	bool FFieldMessages::OffersChoice() const
	{
		for (const FFieldMessageWindow& Window : Windows)
		{
			if (Window.bChoice)
			{
				return true;
			}
		}
		return false;
	}

	void FFieldMessages::Close(int32 Index)
	{
		FFieldMessageWindow& Window = Windows[static_cast<std::size_t>(Index)];
		if (!Window.bOpen)
		{
			return;
		}
		Window.Text.reset();
		Window.bChoice = false;
		Window.bOpen = false;
		Window.bCloseRequested = false;
		Window.Age = FFieldMessageWindow::Free;
		// The mask's high byte goes too, though no window sets it.
		OpenMask &= static_cast<uint16>((1u << Index) ^ 0xFF);
		Window.Owner = FFieldMessageWindow::NoActor;
		Window.bFocus = false;
	}

	void FFieldMessages::CloseAll()
	{
		for (int32 Index = 0; Index < WindowCount; ++Index)
		{
			Close(Index);
		}
	}

	void FFieldMessages::Tick()
	{
		for (int32 Index = 0; Index < WindowCount; ++Index)
		{
			FFieldMessageWindow& Window = Windows[static_cast<std::size_t>(Index)];
			if (!Window.bOpen)
			{
				continue;
			}
			if (Window.OpenTicks == 0 && !Window.Text->HasText())
			{
				Close(Index);
			}
			if (Window.bCloseRequested)
			{
				Close(Index);
			}
			// A window that has just closed still counts its opening down.
			if (Window.OpenTicks != 0)
			{
				--Window.OpenTicks;
			}
		}
	}

	void UpdateFieldMessages(FEventContext& Context, uint16 Pressed, uint16 Repeat)
	{
		FFieldMessages& Messages = Context.Messages;
		++Messages.CursorTicks;
		if ((Messages.CursorTicks & 3) == 0)
		{
			++Messages.CursorFrame;
		}
		if (Messages.CursorFrame >= CursorFrames)
		{
			Messages.CursorFrame = 0;
		}
		std::optional<FGameStateNames> Names;
		if (Context.GameState != nullptr)
		{
			Names.emplace(*Context.GameState);
		}
		Messages.Text.State = Names ? &*Names : nullptr;

		// Every focused window takes its turn first; one that is not the last focused takes a second
		// turn among the others, typing twice in the tick.
		int32 LastFocused = -1;
		for (int32 Index = 0; Index < FFieldMessages::WindowCount; ++Index)
		{
			if (Messages.Windows[static_cast<std::size_t>(Index)].bFocus)
			{
				LastFocused = Index;
			}
		}
		for (FFieldMessageWindow& Window : Messages.Windows)
		{
			if (Window.bOpen && Window.bFocus)
			{
				UpdateWindow(Context, Window, true, Pressed, Repeat);
			}
		}
		std::array<int16, FFieldMessages::WindowCount> Ages;
		Ages.fill(FFieldMessageWindow::Free);
		int16 Next = 0;
		for (int16 Age = 0; Age < FFieldMessages::WindowCount; ++Age)
		{
			for (int32 Index = 0; Index < FFieldMessages::WindowCount; ++Index)
			{
				FFieldMessageWindow& Window = Messages.Windows[static_cast<std::size_t>(Index)];
				if (Window.Age != Age)
				{
					continue;
				}
				Ages[static_cast<std::size_t>(Index)] = Next++;
				if (Window.bOpen && Index != LastFocused)
				{
					UpdateWindow(Context, Window, false, Pressed, Repeat);
				}
			}
		}
		for (std::size_t Index = 0; Index < Ages.size(); ++Index)
		{
			Messages.Windows[Index].Age = Ages[Index];
		}
		Messages.Text.State = nullptr;
	}
}
