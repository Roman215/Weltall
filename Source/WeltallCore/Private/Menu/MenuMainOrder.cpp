#include "Game/GameState.h"
#include "Menu/MenuMain.h"

#include <algorithm>
#include <utility>

namespace Weltall
{
	namespace
	{
		/** The pointer under the cursor, and the one marking the member picked. */
		constexpr std::size_t CursorPointer = 0;
		constexpr std::size_t PickPointer = 1;
		/** The pointer's tip just left of its panel's portrait, low down: the arrow over the portrait's edge. */
		constexpr int32 PointerX = -4;
		constexpr int32 PointerY = 36;
		/** A swap slides the panels as the slide in moves them: eight pixels a frame along the longer axis. */
		constexpr int32 SlideSteps = 8;
		/** The panels not under the cursor or picked, dimmed as the lists dim behind a window. */
		constexpr int32 Dimmed = 1;
		/** The pick's pointer, a faded red beside the cursor's: its texels modulated to this much of their own. */
		constexpr uint8 FadedPointer = 0x50;
		constexpr int32 NoPick = -1;

		namespace Effect
		{
			constexpr uint8 Confirm = 2;
			constexpr uint8 Buzzer = 4;
			/** The menus' chime for an item or an Ether used. */
			constexpr uint8 LookSwitched = 0x37;
		}

		/** Slot's bit among the panels lit; none for no slot. */
		constexpr uint32 Lit(int32 Slot)
		{
			return Slot < 0 ? 0 : 1u << Slot;
		}
	}

	bool FMenuMain::CanArrangeParty() const
	{
		if (Members > 1)
		{
			return true;
		}
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			if (Member(Slot) != FGameState::EmptySlot && FCharacterLooks::CanSwitch(Member(Slot), Services.Game->IsClearGame()))
			{
				return true;
			}
		}
		return false;
	}

	int32 FMenuMain::NextPanel(int32 Slot, bool bUp, int32 Skipped) const
	{
		for (int32 Step = 1; Step <= 3; ++Step)
		{
			const int32 Next = (Slot + (bUp ? 3 - Step : Step)) % 3;
			if (Next != Skipped && Member(Next) != FGameState::EmptySlot)
			{
				return Next;
			}
		}
		return Slot;
	}

	void FMenuMain::PointAtPanel(std::size_t Target, int32 Slot)
	{
		const std::array<int32, 2>& Place = PanelPlaces[static_cast<std::size_t>(Slot)];
		PlaceTargetPointer(Target, Place[0] + PointerX, Place[1] + PointerY);
		TargetPointerShown[Target] = true;
	}

	void FMenuMain::ShadePanels(uint32 LitSlots)
	{
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			if (Member(Slot) == FGameState::EmptySlot)
			{
				continue;
			}
			DrawPartyPanel(Slot, Member(Slot));
			if ((LitSlots & Lit(Slot)) != 0)
			{
				continue;
			}
			FMenuPartyPanel& Panel = Panels[static_cast<std::size_t>(Slot)];
			ShadePiece(Panel.Name, Dimmed);
			for (std::vector<FMenuPrimitive>* Pieces : {&Panel.Portrait, &Panel.Labels, &Panel.Level, &Panel.Hp, &Panel.MaxHp, &Panel.Ep, &Panel.MaxEp, &Panel.Next})
			{
				for (FMenuPrimitive& Piece : *Pieces)
				{
					ShadePiece(Piece, Dimmed);
				}
			}
		}
	}

	TMenuTask<> FMenuMain::ArrangeParty()
	{
		Arc.bDimmed = true;
		HideSelection();
		int32 Cursor = FirstMember;
		int32 Picked = NoPick;
		Flags[MenuFlag::TargetPointers] = 1;
		TargetPointerShown.fill(false);
		PointAtPanel(CursorPointer, Cursor);
		ShadePanels(Lit(Cursor));
		bool bGoesOn = true;
		while (bGoesOn)
		{
			co_await Frame();
			switch (Code)
			{
			case EMenuCode::Up:
			case EMenuCode::Down:
				// With a member picked, only the others: the cursor goes where a swap can.
				Cursor = NextPanel(Cursor, Code == EMenuCode::Up, Picked);
				break;
			case EMenuCode::Confirm:
				if (Picked == NoPick && NextPanel(Cursor, false, Cursor) == Cursor)
				{
					// A member alone, here for its look: nobody to swap with.
					PlaySound(Effect::Buzzer);
					continue;
				}
				if (Picked == NoPick)
				{
					// The pick marked in a faded red, the cursor on to the next member to swap it with.
					Picked = Cursor;
					PointAtPanel(PickPointer, Picked);
					for (FMenuPrimitive& Piece : TargetPointers[PickPointer])
					{
						Piece.Code = static_cast<uint8>(Piece.Code & ~MenuGpu::RawTexture);
						Piece.Corners[0].Color = {FadedPointer, FadedPointer, FadedPointer};
					}
					Cursor = NextPanel(Picked, false, Picked);
				}
				else
				{
					TargetPointerShown.fill(false);
					co_await SwapPanels(Picked, Cursor);
					Picked = NoPick;
				}
				break;
			case EMenuCode::Square:
				// The player's Triangle (the kernel swaps the face buttons): the story's order back, a pick dropped.
				if (!Services.Game->PartyOrder().IsSet())
				{
					PlaySound(Effect::Buzzer);
					continue;
				}
				PlaySound(Effect::Confirm);
				Picked = NoPick;
				TargetPointerShown.fill(false);
				co_await RestoreStoryOrder();
				break;
			case EMenuCode::Triangle:
				// The player's Square: the second Citan with his sword or without, the grown Emeralda or the child.
				if (!SwitchLook(Cursor))
				{
					PlaySound(Effect::Buzzer);
					continue;
				}
				PlaySound(Effect::LookSwitched);
				break;
			case EMenuCode::Cancel:
			case EMenuCode::Left:
				// Cancel drops the pick first; Left goes back to the arc at once.
				if (Picked != NoPick && Code == EMenuCode::Cancel)
				{
					Picked = NoPick;
				}
				else
				{
					bGoesOn = false;
				}
				break;
			default:
				continue;
			}
			TargetPointerShown[PickPointer] = Picked != NoPick;
			PointAtPanel(CursorPointer, Cursor);
			ShadePanels(Lit(Cursor) | Lit(Picked));
		}
		TargetPointerShown.fill(false);
		Flags[MenuFlag::TargetPointers] = 0;
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			if (Member(Slot) != FGameState::EmptySlot)
			{
				DrawPartyPanel(Slot, Member(Slot));
			}
		}
		Arc.bDimmed = false;
	}

	TMenuTask<> FMenuMain::SwapPanels(int32 First, int32 Second)
	{
		std::array<int32, 3> From{0, 1, 2};
		std::swap(From[static_cast<std::size_t>(First)], From[static_cast<std::size_t>(Second)]);
		const std::array<uint8, 3> Shown{Member(From[0]), Member(From[1]), Member(From[2])};
		FPartyOrder Order;
		Order.Arrange(Services.Game->Party(), Shown);
		Services.Game->SetPartyOrder(Order);
		co_await MovePanels(From);
	}

	TMenuTask<> FMenuMain::RestoreStoryOrder()
	{
		Services.Game->SetPartyOrder({});
		// The members by their slots in the game's party; a gap stays last, where the panels keep it.
		const auto StorySlot = [this](int32 Slot) { return GameSlot(Slot) == FPartyOrder::Nobody ? 3 : GameSlot(Slot); };
		std::array<int32, 3> From{0, 1, 2};
		std::sort(From.begin(), From.end(), [&StorySlot](int32 A, int32 B) { return StorySlot(A) < StorySlot(B); });
		co_await MovePanels(From);
	}

	bool FMenuMain::SwitchLook(int32 Slot)
	{
		FCharacterLooks Looks = Services.Game->CharacterLooks();
		if (!FCharacterLooks::CanSwitch(Member(Slot), Services.Game->IsClearGame()) || !Looks.Switch(Member(Slot)))
		{
			return false;
		}
		Services.Game->SetCharacterLooks(Looks);
		UploadPortrait(Slot, Member(Slot));
		UploadFace(Member(Slot));
		RenderPartyNames();
		return true;
	}

	TMenuTask<> FMenuMain::MovePanels(const std::array<int32, 3>& From)
	{
		const std::array<uint8, 3> Shown{Member(0), Member(1), Member(2)};
		const std::array<uint8, 3> HaveGears{Flags[MenuFlag::PartyHasGear], Flags[MenuFlag::PartyHasGear + 1], Flags[MenuFlag::PartyHasGear + 2]};
		const std::array<int8, 3> Slots = GameSlots;
		uint32 Moving = 0;
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			const std::size_t Was = static_cast<std::size_t>(From[static_cast<std::size_t>(Slot)]);
			Flags[static_cast<std::size_t>(MenuFlag::Party + Slot)] = Shown[Was];
			Flags[static_cast<std::size_t>(MenuFlag::PartyHasGear + Slot)] = HaveGears[Was];
			GameSlots[static_cast<std::size_t>(Slot)] = Slots[Was];
			if (static_cast<int32>(Was) != Slot && Member(Slot) != FGameState::EmptySlot)
			{
				Moving |= Lit(Slot);
				UploadPortrait(Slot, Member(Slot));
			}
		}
		if (Moving != 0)
		{
			RenderPartyNames();
		}
		FirstMember = NextPanel(2, false);
		// Each member's panel leaves the place it held for its new one.
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			if ((Moving & Lit(Slot)) != 0)
			{
				const std::array<int32, 2>& Before = PanelPlaces[static_cast<std::size_t>(From[static_cast<std::size_t>(Slot)])];
				const std::array<int32, 2>& After = PanelPlaces[static_cast<std::size_t>(Slot)];
				StartSlide(Before[0], Before[1], After[0], After[1], SlideSteps, Slot);
			}
		}
		const auto Sliding = [this, Moving]
		{
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				if ((Moving & Lit(Slot)) != 0 && !Slides[static_cast<std::size_t>(Slot)].bDone)
				{
					return true;
				}
			}
			return false;
		};
		while (Sliding())
		{
			ShadePanels(Moving);
			co_await Frame();
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				if ((Moving & Lit(Slot)) != 0 && !Slides[static_cast<std::size_t>(Slot)].bDone)
				{
					StepSlide(Slot);
				}
			}
		}
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			if ((Moving & Lit(Slot)) == 0)
			{
				continue;
			}
			FMenuSlide& Slide = Slides[static_cast<std::size_t>(Slot)];
			Slide.FromX = PanelPlaces[static_cast<std::size_t>(Slot)][0];
			Slide.FromY = PanelPlaces[static_cast<std::size_t>(Slot)][1];
			Slide.MovedX = 0;
			Slide.MovedY = 0;
		}
	}
}
