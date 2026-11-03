#include "EventOps.h"
#include "FieldDirections.h"

#include "Graphics/GteVectors.h"
#include "Text/MessageOverrides.h"

#include <algorithm>

// How a message instruction opens its window: the waits, the window it takes, the band and place,
// the portrait and the opening, as the field's opener does it for every message instruction.
namespace Weltall::EventOps
{
	namespace
	{
		using namespace FieldMessageStyle;
		using EMode = EFieldMessageMode;

		constexpr uint8 NoPortraitId = 0xFF;
		/** What a window shows in its portrait's place when it has none. */
		constexpr uint8 NoPortraitShown = 0x80;
		/** An actor's head, above its model's origin. */
		constexpr int16 HeadHeight = -64;
		constexpr int32 AboveHead = 0x24;
		constexpr int32 BelowHead = 0x30;
		constexpr int32 UpperY = 0x10;
		constexpr int32 LowerY = 0x94;
		constexpr int32 CenteredUpperY = 0x14;
		constexpr int32 FixedWidth = 0x48;
		constexpr int32 PortraitLines = 4;
		constexpr int32 PortraitMinWidth = 0x18;
		constexpr int32 PortraitWidth = 0x11;
		constexpr int32 PortraitIndent = 0x44;
		constexpr int32 LineHeight = 14;
		/** The screen's margins a window keeps to. */
		constexpr int32 LeftEdge = 12;
		constexpr int32 RightEdge = 0x134;
		constexpr int32 TopEdge = 0x10;
		constexpr int32 BottomEdge = 0xD4;
		/** The event variables a window's numbers and its item name come from. */
		constexpr std::array<uint32, 4> NumberVariables = {0x16, 0x18, 0x1A, 0x1C};
		constexpr uint32 PortraitSlotMask = 0x1C;
		constexpr uint32 PortraitSlotShift = 2;
		constexpr uint32 OffScreen = 0x200;
		constexpr uint16 DirectionStopped = 0x8000;

		int32 Wrap(uint32 Value)
		{
			return static_cast<int32>(Value);
		}

		/** The camera's direction, 0 to 7, as the scripts measure directions from it. */
		int32 CameraDirection(const FFieldCamera& Camera)
		{
			return FieldDirections::CameraEighth(Camera);
		}

		struct FScreenPoint
		{
			int32 X = 0;
			int32 Y = 0;
		};

		/** An actor's head on the screen: its model's point 64 units up, through the view the scripts find. */
		FScreenPoint ProjectHead(const FEventContext& Context, uint32 Actor)
		{
			const std::array<int32, 2> Head = FieldMessageSpeakerHead(Context, Actor, Context.ScriptView.View, Context.ScriptView.ProjectionDistance);
			return {Head[0], Head[1]};
		}

		void SetPortraitSlot(FEventActorState& State, int32 Slot)
		{
			State.Status = (State.Status & ~PortraitSlotMask) | (static_cast<uint32>(Slot) & 7) << PortraitSlotShift;
		}

		bool ShowsPortrait(const FFieldMessages& Messages, int16 Character)
		{
			for (const FFieldMessageWindow& Window : Messages.Windows)
			{
				if (Window.bOpen && Window.bPortrait && Window.Portrait == Character)
				{
					return true;
				}
			}
			return false;
		}

		/**
		 * Whether the running actor's face is in the faces' cache, which it points its portrait slot
		 * at; else the load goes on a step (the disc reads at once: a face read is ready at the next
		 * look, and its file's memory goes at the one after), or the face goes to a slot no open
		 * window shows, and waits.
		 */
		bool PortraitReady(FEventContext& Context, uint8 Character)
		{
			FFieldMessages& Messages = Context.Messages;
			for (FFieldPortraitSlot& Slot : Messages.Portraits)
			{
				if (Slot.Load == FFieldPortraitSlot::Reading)
				{
					Slot.Load = FFieldPortraitSlot::Read;
					return false;
				}
				if (Slot.Load == FFieldPortraitSlot::Read)
				{
					Slot.Load = FFieldPortraitSlot::Ready;
					return false;
				}
			}
			for (int32 Slot = 0; Slot < FFieldMessages::PortraitSlotCount; ++Slot)
			{
				if (Messages.Portraits[static_cast<std::size_t>(Slot)].Character == Character)
				{
					SetPortraitSlot(Self(Context), Slot);
					return true;
				}
			}
			for (int32 Tries = 1;; ++Tries)
			{
				Messages.LastPortraitSlot = Messages.LastPortraitSlot + 1 < FFieldMessages::PortraitSlotCount ? Messages.LastPortraitSlot + 1 : 0;
				FFieldPortraitSlot& Slot = Messages.Portraits[static_cast<std::size_t>(Messages.LastPortraitSlot)];
				if (!ShowsPortrait(Messages, Slot.Character))
				{
					SetPortraitSlot(Self(Context), Messages.LastPortraitSlot);
					Slot.Character = Character;
					Slot.Load = FFieldPortraitSlot::Reading;
					Slot.ReadNumber = ++Messages.PortraitReads;
					return false;
				}
				if (Tries >= FFieldMessages::PortraitSlotCount)
				{
					return false;
				}
			}
		}

		/** A free window, the others aging by one; or -1 when all four are open, the oldest then asked to close. */
		int32 ClaimWindow(FFieldMessages& Messages)
		{
			int32 Free = -1;
			int32 Oldest = -1;
			uint16 OldestAge = 0;
			for (int32 Index = 0; Index < FFieldMessages::WindowCount; ++Index)
			{
				const uint16 Age = static_cast<uint16>(Messages.Windows[static_cast<std::size_t>(Index)].Age);
				if (Age == 0xFFFF)
				{
					Free = Free < 0 ? Index : Free;
				}
				else if (OldestAge <= Age)
				{
					Oldest = Index;
					OldestAge = Age;
				}
			}
			if (Free < 0)
			{
				Messages.Windows[static_cast<std::size_t>(Oldest)].bCloseRequested = true;
				return -1;
			}
			for (FFieldMessageWindow& Window : Messages.Windows)
			{
				if (Window.Age != FFieldMessageWindow::Free)
				{
					++Window.Age;
				}
			}
			Messages.Windows[static_cast<std::size_t>(Free)].Age = 0;
			return Free;
		}

		/** Where a window's message goes, before the field lays its window out. */
		struct FPlacement
		{
			int32 Window = 0;
			uint16 Message = 0;
			int32 X = 0;
			int32 Y = 0;
			int32 Width = 0;
			int32 Lines = 0;
			uint32 Speaker = 0;
			EMode Mode = EMode::AtSpeaker;
			/** PortraitOnRight when the portrait goes on the right. */
			uint32 Side = 0;
			/** The instruction's style. */
			uint32 Style = 0;
		};

		/** Lays the window out and opens it with its message, as the field's window opener does. */
		void InitWindow(FEventContext& Context, const FPlacement& Place, const FEventActorState& SpeakerState)
		{
			FFieldMessages& Messages = Context.Messages;
			FEventActorState& Owner = Self(Context);
			FFieldMessageWindow& Window = Messages.Windows[static_cast<std::size_t>(Place.Window)];
			const int32 X = static_cast<int16>(Place.X);
			const int32 FrameY = static_cast<int16>(Place.Y - 8);
			const uint32 Kept = ((Owner.WindowStyle >> 16) != 0 ? Owner.WindowStyle >> 16 : Owner.WindowStyle & 0xFFFF) | Place.Side;
			std::array<int32, 4> Numbers{};
			for (std::size_t Index = 0; Index < Numbers.size(); ++Index)
			{
				Numbers[Index] = GetVar(Context, NumberVariables[Index]);
			}
			FScreenPoint Origin;
			if (Place.Mode == EMode::Fixed)
			{
				Origin = {FieldScreenCenterX, FrameY + 0x20};
			}
			else if (Place.Mode == EMode::Centered)
			{
				Origin = {X + 8 + Place.Width * 2, FrameY + 8 + Place.Lines * 7};
			}
			else
			{
				Origin = ProjectHead(Context, Place.Speaker);
			}
			const bool bHasPortrait = Owner.Portrait != NoPortraitId;
			const bool bPortraitLeft = bHasPortrait && (Kept & (NoPortrait | PortraitOnRight)) == 0;
			if (!bHasPortrait || (Kept & NoPortrait) != 0)
			{
				Window.bPortrait = false;
				Window.Portrait = NoPortraitShown;
			}
			else
			{
				Window.PortraitCell = static_cast<uint8>((Owner.Status >> 1 & 0xE) | (bPortraitLeft ? 1 : 0));
				Window.bPortrait = true;
				Window.Portrait = Owner.Portrait;
			}
			Window.bChoice = false;
			Window.Rect = {static_cast<int16>(X), static_cast<int16>(FrameY), static_cast<int16>(Place.Width * 4 + 16), static_cast<int16>(Place.Lines * LineHeight + 16)};
			Window.Text.emplace(Place.Width, Place.Lines);
			for (std::size_t Index = 0; Index < Numbers.size(); ++Index)
			{
				Window.Text->SetNumber(static_cast<int32>(Index), Numbers[Index]);
			}
			Window.Text->SetSelector(static_cast<uint16>(Numbers[3]));
			Window.TextX = static_cast<int16>(X + (bPortraitLeft ? PortraitIndent : 0) + 8);
			Window.TextY = static_cast<int16>(FrameY + 8);
			if ((Kept & PortraitOnRight) != 0)
			{
				Window.Flags |= FieldMessageWindowFlags::PortraitOnRight;
			}
			Window.Text->SetSpeed(Messages.OpenTicks == FFieldMessages::FullRateOpenTicks ? 1 : 2);
			Window.Text->HideBox();
			Window.Message = Messages.Table.String(Place.Message);
			Window.bOpen = true;
			Window.Owner = static_cast<int16>(Context.Run.Actor);
			Window.Speaker = static_cast<int16>(Place.Speaker);
			++Window.Openings;
			Window.OpenTicks = Messages.OpenTicks;
			Window.bFocus = (Place.Style & TakesFocus) != 0;
			Window.OpenOffset[0] = Wrap(static_cast<uint32>(Origin.X - (Place.Width * 2 + 8) - X) << 16);
			Window.OpenOffset[1] = Wrap(static_cast<uint32>(Origin.Y - (Place.Lines * 7 + 8) - FrameY) << 16);
			for (std::size_t Axis = 0; Axis < 2; ++Axis)
			{
				const int32 Offset = Window.OpenOffset[Axis];
				Window.OpenStep[Axis] = Wrap(0u - static_cast<uint32>((Kept & NoOpening) != 0 ? Offset : Offset / Messages.OpenTicks));
			}
			if ((Kept & NoOpening) != 0)
			{
				Window.OpenTicks = 1;
			}
			// A speaker off screen cannot talk: its window closes at the windows' next tick.
			if ((SpeakerState.Flags1 & OffScreen) != 0 && (Kept & StaysOffScreen) == 0)
			{
				Window.bCloseRequested = true;
			}
		}
	}

	bool OpenMessage(FEventContext& Context, uint32 Speaker, EFieldMessageMode Mode)
	{
		FFieldMessages& Messages = Context.Messages;
		FEventActorState& State = Self(Context);
		Context.Run.Budget += 0x20;
		// The original waits too while the disc is busy (the port's reads at once) and while a menu is
		// asked for, which the scripts cannot do yet.
		if (Messages.bMapEntering || Context.MessagesOpened != 0 || (State.Portrait != NoPortraitId && !PortraitReady(Context, State.Portrait)))
		{
			Context.Run.bYield = true;
			return false;
		}
		++Context.MessagesOpened;
		const int32 Own = Messages.FindOwnWindow(Context.Run.Actor);
		if (Own >= 0)
		{
			Messages.Windows[static_cast<std::size_t>(Own)].bCloseRequested = true;
			Context.Run.bYield = true;
			return false;
		}
		Context.Run.Budget += 8;
		FPlacement Place;
		Place.Message = ReadU16(Context, 1);
		Place.Speaker = Speaker;
		Place.Mode = Mode;
		Place.Window = ClaimWindow(Messages);
		if (Place.Window < 0)
		{
			Context.Run.bYield = true;
			return false;
		}
		int32 OpenCount = 0;
		uint16 OpenFlags = 0;
		for (const FFieldMessageWindow& Window : Messages.Windows)
		{
			if (Window.bOpen)
			{
				++OpenCount;
				OpenFlags |= Window.Flags;
			}
		}
		if (Place.Message >= Messages.Table.Count())
		{
			Fail(Context, EEventFault::TableIndexOutOfRange, Place.Message);
			return false;
		}
		const FEventActorState* SpeakerState = StateOf(Context, Speaker);
		if (SpeakerState == nullptr)
		{
			return false;
		}
		const bool bSized = Mode == EMode::AtSpeaker || Mode == EMode::Centered;
		Place.Width = Messages.Table.WidthUnits(Place.Message);
		Place.Lines = Messages.Table.Lines(Place.Message);
		if (bSized)
		{
			Place.Width = State.WindowWidth != 0 ? State.WindowWidth : Place.Width;
			Place.Lines = State.WindowLines != 0 ? State.WindowLines : Place.Lines;
		}
		const uint32 Default = State.WindowStyle & 0xFFFF;
		State.WindowStyle = Default;
		Place.Style = Default;
		if (const uint8 InstructionStyle = CodeByte(Context, 3); InstructionStyle != 0)
		{
			Place.Style = (Default & 0xFF00) | InstructionStyle;
			State.WindowStyle = Default | Place.Style << 16;
		}
		const bool bPortrait = State.Portrait != NoPortraitId && (Place.Style & NoPortrait) == 0;
		const uint32 Band = (Place.Style >> PlaceShift) & 3;
		FFieldMessageWindow& Window = Messages.Windows[static_cast<std::size_t>(Place.Window)];
		Window.HeadPlace = {};
		bool bCenterKnown = true;
		int32 CenterX = FieldScreenCenterX;
		int32 Y = UpperY;
		if (Band == 3)
		{
			// No band: the window keeps its previous flags, and only a script's own x places it.
			bCenterKnown = false;
		}
		else
		{
			bool bLower = Band == 2;
			if (Band == 0)
			{
				const int32 Facing = ((State.Status >> 9 & 7) - CameraDirection(Context.Camera)) & 7;
				const bool bOtherLower = (OpenFlags & FieldMessageWindowFlags::Lower) != 0;
				bLower = Facing < 5 ? !bOtherLower : bOtherLower || OpenCount != 0;
			}
			Window.Flags = bLower ? FieldMessageWindowFlags::Banded | FieldMessageWindowFlags::Lower : FieldMessageWindowFlags::Banded;
			if (bSized)
			{
				if (Mode == EMode::AtSpeaker)
				{
					const FScreenPoint Head = ProjectHead(Context, Speaker);
					CenterX = Head.X;
					Y = bLower ? Head.Y + BelowHead : Head.Y - Place.Lines * LineHeight - AboveHead;
					Window.HeadPlace.bX = State.WindowX == 0;
					Window.HeadPlace.bY = !bPortrait && State.WindowY == 0;
					Window.HeadPlace.bLower = bLower;
				}
				else
				{
					Y = bLower ? LowerY : CenteredUpperY;
				}
				if (bPortrait)
				{
					Place.Width = std::max(Place.Width, PortraitMinWidth) + PortraitWidth;
					Place.Lines = PortraitLines;
					Y = bLower ? LowerY : UpperY;
				}
			}
			else
			{
				Place.Width = FixedWidth;
				Place.Lines = PortraitLines;
				Y = bLower ? LowerY : UpperY;
			}
		}
		if (!bCenterKnown && !(bSized && State.WindowX != 0))
		{
			Fail(Context, EEventFault::WindowPlaceUndefined, Place.Message);
			return false;
		}
		Window.HeadPlace.Width = static_cast<int16>(Place.Width);
		Window.HeadPlace.Lines = static_cast<int16>(Place.Lines);
		Place.X = CenterX - 8 - Place.Width * 2;
		Place.X = std::max(Place.X, LeftEdge);
		if (Place.X + 0x10 + Place.Width * 4 > RightEdge)
		{
			Place.X = RightEdge - 0x10 - Place.Width * 4;
		}
		Place.Y = std::max(Y, TopEdge);
		if (Place.Y + 8 + Place.Lines * LineHeight > BottomEdge)
		{
			Place.Y = BottomEdge - 8 - Place.Lines * LineHeight;
		}
		if (bSized)
		{
			Place.X = State.WindowX != 0 ? static_cast<int16>(State.WindowX) : Place.X;
			Place.Y = State.WindowY != 0 ? static_cast<int16>(State.WindowY) : Place.Y;
			Place.Width = State.WindowWidth != 0 ? State.WindowWidth : Place.Width;
			Place.Lines = State.WindowLines != 0 ? State.WindowLines : Place.Lines;
			Place.Lines = bPortrait ? PortraitLines : Place.Lines;
		}
		if (Messages.Replaced != nullptr)
		{
			Messages.Replaced->CheckWindow(Place.Message, Messages.Table.String(Place.Message), Place.Width, Place.Lines);
			if (!bSized || State.WindowWidth == 0)
			{
				Messages.Replaced->CheckScreenWidth(Place.Message, Place.Width, (RightEdge - LeftEdge - 0x10) / 4);
			}
		}
		if ((Place.Style & NoBackground) != 0)
		{
			Window.Flags |= FieldMessageWindowFlags::NoBackground;
		}
		if ((Place.Style & PortraitSides) == 0)
		{
			const int32 Facing = (static_cast<int16>(SpeakerState->Facing) >> 9) - CameraDirection(Context.Camera) + 1;
			Place.Side = (Facing & 7) >= 4 ? PortraitOnRight : 0;
		}
		else if ((Place.Style & PortraitRight) != 0)
		{
			Place.Side = PortraitOnRight;
		}
		InitWindow(Context, Place, *SpeakerState);
		Messages.OpenMask = static_cast<uint16>(Messages.OpenMask | 1u << Place.Window);
		State.MoveDirection |= DirectionStopped;
		Advance(Context, 4);
		return true;
	}
}

namespace Weltall
{
	std::array<int32, 2> PlaceFieldMessageAtHead(const FFieldMessageWindow::FHeadPlace& Place, int32 HeadX, int32 HeadY)
	{
		using namespace EventOps;
		int32 X = std::max(HeadX - 8 - Place.Width * 2, LeftEdge);
		if (X + 0x10 + Place.Width * 4 > RightEdge)
		{
			X = RightEdge - 0x10 - Place.Width * 4;
		}
		int32 Y = std::max(Place.bLower ? HeadY + BelowHead : HeadY - Place.Lines * LineHeight - AboveHead, TopEdge);
		if (Y + 8 + Place.Lines * LineHeight > BottomEdge)
		{
			Y = BottomEdge - 8 - Place.Lines * LineHeight;
		}
		return {X, Y};
	}

	std::array<int32, 2> FieldMessageSpeakerHead(const FEventContext& Context, uint32 Actor, const FGteMatrix& View, uint16 H)
	{
		const FFieldActor& Record = Context.Actors[Actor];
		FGteMatrix Model;
		Model.M = Record.Matrix;
		Model.T = Record.Translation;
		FGte Gte;
		Gte.Transform = CompMatrix(View, Model);
		Gte.OffsetX = FieldScreenCenterX << 16;
		Gte.OffsetY = FieldScreenCenterY << 16;
		Gte.ProjectionDistance = H;
		const FGteProjection Head = Gte.Rtps({0, EventOps::HeadHeight, 0});
		return {Head.ScreenX, Head.ScreenY};
	}
}
