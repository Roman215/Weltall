#include "EventOps.h"

#include "Game/GameState.h"

namespace Weltall::EventOps
{
	namespace
	{
		using ECharacterPoints = FGameState::ECharacterPoints;

		/** 91: goes on when a party slot holds the character; FF matches an empty slot. */
		void IfCharacterInParty(FEventContext& Context)
		{
			const uint8 Character = CodeByte(Context, 1);
			for (const uint8 Member : Context.PartyCharacters)
			{
				if (Member == Character)
				{
					Advance(Context, 4);
					return;
				}
			}
			Self(Context).Pc = ReadU16(Context, 2);
		}

		/** B9: goes on when the character has joined; characters past 15 never have. */
		void IfCharacterJoined(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			Branch(Context, (State->JoinedCharacters() >> (CodeByte(Context, 1) & 31) & 1) != 0, 4, 2);
		}

		template <bool bJoined>
		void SetCharacterJoined(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			const uint16 Bit = static_cast<uint16>(1u << (CodeByte(Context, 1) & 31));
			State->SetJoinedCharacters(bJoined ? State->JoinedCharacters() | Bit : State->JoinedCharacters() & ~Bit);
			Advance(Context, 2);
		}

		/** A character operand resolved, or nullopt for none. */
		std::optional<int32> ReadCharacter(FEventContext& Context, int32 Offset)
		{
			const int32 Character = ResolveCharacter(Context, ReadValue(Context, Offset));
			return Character == 0xFF ? std::nullopt : std::optional<int32>(Character);
		}

		/** Whether the character has a record, with a fault when it has none. */
		bool HasRecord(FEventContext& Context, int32 Character)
		{
			if (Character < 0 || Character >= FGameState::CharacterCount)
			{
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Character));
				return false;
			}
			return true;
		}

		/** FE 3A and FE 3B: a character's bit in the menus' second character mask. */
		template <bool bSet>
		void SetMenuCharacter(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			if (const std::optional<int32> Character = ReadCharacter(Context, 1))
			{
				const uint16 Bit = static_cast<uint16>(1u << (*Character & 31));
				State->SetMenuCharacters(bSet ? State->MenuCharacters() | Bit : State->MenuCharacters() & ~Bit);
			}
			Advance(Context, 3);
		}

		/** FE 9F: locks (mode 0) or unlocks a character in Member Change. */
		void SetCharacterLocked(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			if (const std::optional<int32> Character = ReadCharacter(Context, 2))
			{
				const uint16 Bit = static_cast<uint16>(1u << (*Character & 31));
				State->SetLockedCharacters(CodeByte(Context, 1) == 0 ? State->LockedCharacters() | Bit : State->LockedCharacters() & ~Bit);
			}
			Advance(Context, 4);
		}

		/** FE 69: a character's weight with what food added, 0 for none. */
		void GetCharacterWeight(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			const std::optional<int32> Character = ReadCharacter(Context, 3);
			if (Character && !HasRecord(Context, *Character))
			{
				return;
			}
			SetVar(Context, ReadU16(Context, 1), Character ? State->CharacterWeight(*Character) + State->CharacterWeightGain(*Character) : 0);
			Advance(Context, 5);
		}

		/** FE 6B: sets a character's weight by what food adds to it, never below its own. */
		void SetCharacterWeight(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			if (const std::optional<int32> Character = ReadCharacter(Context, 3))
			{
				if (!HasRecord(Context, *Character))
				{
					return;
				}
				int32 Gain = ReadValue(Context, 1) - State->CharacterWeight(*Character);
				if (Gain < 0)
				{
					Gain = 0;
				}
				State->SetCharacterWeightGain(*Character, static_cast<uint8>(Gain));
			}
			Advance(Context, 5);
		}

		/** FE 8B: the party slot the running actor stands for, FF for none. */
		void GetOwnPartySlot(FEventContext& Context)
		{
			int32 Slot = 0xFF;
			for (std::size_t Index = 0; Index < Context.PartyActors.size(); ++Index)
			{
				if (Context.PartyActors[Index] == Context.Run.Actor)
				{
					Slot = static_cast<int32>(Index);
					break;
				}
			}
			SetVar(Context, ReadU16(Context, 1), Slot);
			Advance(Context, 3);
		}

		/**
		 * FE 41 and FE 42: whether a party slot's member rides its gear (a slot past 2 is slot 2); the
		 * slot's parked-gear variables then name this map, at 0, 0.
		 */
		template <bool bInGear>
		void SetSlotInGear(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			int32 Slot = ReadValue(Context, 1);
			if (Slot > 2)
			{
				Slot = 2;
			}
			if (Slot < 0)
			{
				// The original writes before the game state's three bytes.
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Slot));
				return;
			}
			State->SetPartySlotInGear(Slot, bInGear ? 1 : 0);
			const uint32 Parked = 0x2A + 6 * static_cast<uint32>(Slot);
			SetVar(Context, Parked, Context.MapNumber & 0xFFF);
			SetVar(Context, Parked + 2, 0);
			SetVar(Context, Parked + 4, 0);
			Advance(Context, 3);
		}

		/** FE A1: sets a character's gear; the character is taken as it is, party slots unresolved. */
		void SetCharacterGear(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			const int32 Character = ReadValue(Context, 1);
			if (!State->SetCharacterGear(Character, static_cast<uint8>(ReadValue(Context, 3))))
			{
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Character));
				return;
			}
			Advance(Context, 5);
		}

		/** Vierge, and Regurus: Elly's Omnigear in the clear game, whose field sprite file is Vierge's. */
		constexpr uint8 Vierge = 2;
		constexpr uint8 Regurus = 10;

		/**
		 * FE C7: a character's gear, FF for none (and for no character). Regurus reads as Vierge, whose pictures the field
		 * has for it: the Yggdrasil's hangar, the one map that reads Elly's gear, parks it in Vierge's place rather than
		 * leaving the place empty as for a gear without a hangar picture.
		 */
		void GetCharacterGear(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			const int32 Character = ResolveCharacter(Context, ReadValue(Context, 1));
			int32 Gear = 0xFF;
			if (Character != 0xFF)
			{
				const std::optional<uint8> Held = State->CharacterGear(Character);
				if (!Held)
				{
					Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Character));
					return;
				}
				Gear = *Held == Regurus ? Vierge : *Held;
			}
			// The one destination past the variables any script names is a slip for FE A1, which would set the gear
			// the character already holds; the original stores it in the field's heap, which the port does not keep.
			const uint32 Destination = ReadU16(Context, 3);
			if (Destination < FEventVariables::AddressEnd)
			{
				SetVar(Context, Destination, Gear);
			}
			Advance(Context, 5);
		}

		/** 79 and 7A: every character's HP or EP back to its highest, in the party or not. */
		template <ECharacterPoints Points, ECharacterPoints Most>
		void RestoreAll(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			for (int32 Character = 0; Character < FGameState::CharacterCount; ++Character)
			{
				State->SetCharacterPoints(Character, Points, State->CharacterPoints(Character, Most));
			}
			Advance(Context, 1);
		}

		/** FE A4: every gear's HP and fuel back to their highest. */
		void RestoreGears(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			State->RestoreGears();
			Advance(Context, 1);
		}

		/** Takes points away, leaving at least 1; a negative amount adds with no ceiling. */
		void TakePoints(FGameState& State, int32 Character, ECharacterPoints Points, int32 Amount)
		{
			int32 Left = State.CharacterPoints(Character, Points) - Amount;
			if (Left < 1)
			{
				Left = 1;
			}
			State.SetCharacterPoints(Character, Points, static_cast<uint16>(Left));
		}

		/** Adds the amount's low 16 bits, then caps at the highest. */
		void GivePoints(FGameState& State, int32 Character, ECharacterPoints Points, ECharacterPoints Most, int32 Amount)
		{
			const uint16 Raised = static_cast<uint16>(State.CharacterPoints(Character, Points) + static_cast<int16>(Amount));
			State.SetCharacterPoints(Character, Points, Raised);
			if (State.CharacterPoints(Character, Most) < Raised)
			{
				State.SetCharacterPoints(Character, Points, State.CharacterPoints(Character, Most));
			}
		}

		enum class EPartyPoints : uint8
		{
			TakeHp,
			GiveHp,
			TakeEp,
			GiveEp,
		};

		/**
		 * 7B-7E: change the HP or EP of the party's filled slots that the flags byte's low bits pick
		 * (0 all three, else slot n - 1). 7C, meant to give HP, gives EP in the original; the options
		 * choose.
		 */
		template <EPartyPoints Change>
		void ChangePartyPoints(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			static constexpr std::array<uint32, 4> Slots = {7, 1, 2, 4};
			const uint8 Flags = CodeByte(Context, 3);
			const int32 Amount = ReadOperand(Context, 1, Flags, 0x80);
			uint32 Picked = Slots[Flags & 3];
			for (std::size_t Slot = 0; Slot < Context.PartyCharacters.size(); ++Slot, Picked >>= 1)
			{
				const uint8 Character = Context.PartyCharacters[Slot];
				if (Character == FEventContext::NoActor || (Picked & 1) == 0)
				{
					continue;
				}
				if (Character >= FGameState::CharacterCount)
				{
					Fail(Context, EEventFault::TableIndexOutOfRange, Character);
					return;
				}
				switch (Change)
				{
				case EPartyPoints::TakeHp:
					TakePoints(*State, Character, ECharacterPoints::Hp, Amount);
					break;
				case EPartyPoints::GiveHp:
					if (Context.Options.bRecoverPartyHpGivesHp)
					{
						GivePoints(*State, Character, ECharacterPoints::Hp, ECharacterPoints::MaxHp, Amount);
					}
					else
					{
						GivePoints(*State, Character, ECharacterPoints::Ep, ECharacterPoints::MaxEp, Amount);
					}
					break;
				case EPartyPoints::TakeEp:
					TakePoints(*State, Character, ECharacterPoints::Ep, Amount);
					break;
				case EPartyPoints::GiveEp:
					GivePoints(*State, Character, ECharacterPoints::Ep, ECharacterPoints::MaxEp, Amount);
					break;
				}
			}
			Advance(Context, 4);
		}
	}

	namespace
	{
		/**
		 * FE AD, FE B4: the HP or EP of the character in a party slot (the byte at operand 3) into the
		 * variable at operand 1; nothing for an empty slot.
		 */
		template <ECharacterPoints Which>
		void GetPartyPoints(FEventContext& Context)
		{
			const uint32 Address = ReadU16(Context, 1);
			const uint8 Slot = CodeByte(Context, 3);
			// The kernel's party array has three words: a slot past them reads memory it does not own, or carried
			// past, reads as an empty slot.
			if (Slot >= Context.PartyCharacters.size())
			{
				if (Recovers(Context, "a party slot past the three read", EEventFault::TableIndexOutOfRange, Slot))
				{
					Advance(Context, 4);
				}
				return;
			}
			const uint8 Character = Context.PartyCharacters[Slot];
			if (Character != FEventContext::NoActor)
			{
				if (Character >= FGameState::CharacterCount)
				{
					Fail(Context, EEventFault::TableIndexOutOfRange, Character);
					return;
				}
				if (const FGameState* State = GameStateOf(Context))
				{
					SetVar(Context, Address, State->CharacterPoints(Character, Which));
				}
			}
			Advance(Context, 4);
		}
	}

	namespace
	{
		/** The story's flags a character made from another sets, by the character it is made as. */
		constexpr int32 SecondStandIn = 9;
		constexpr int32 ThirdStandIn = 10;
		constexpr uint16 SecondStandInMade = 0x2000;
		constexpr uint16 ThirdStandInMade = 0x1000;
		constexpr uint16 SeventhApTier = 0x4000;

		/**
		 * FE AB and FE AC: adds to or takes from the HP of the gears of the party's members: of all three
		 * slots (selector 0) or of slot 0, 1 or 2 (1 to 3); the amount a constant with bit 7 of the selector's
		 * byte. Added HP stops at the gear's most, compared unsigned (an amount below minus the HP wraps and fills the gear); taken
		 * HP leaves at least 1. A member without a gear is passed by.
		 */
		template <bool bAdd>
		void ChangeGearHp(FEventContext& Context)
		{
			constexpr std::array<uint8, 4> SlotMasks = {7, 1, 2, 4};
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			const uint8 Selector = CodeByte(Context, 3);
			const int32 Amount = ReadOperand(Context, 1, Selector, 0x80);
			const uint8 Mask = SlotMasks[Selector & 3];
			for (std::size_t Slot = 0; Slot < Context.PartyCharacters.size(); ++Slot)
			{
				const uint8 Character = Context.PartyCharacters[Slot];
				if (Character == FEventContext::NoActor || (Mask >> Slot & 1) == 0)
				{
					continue;
				}
				const std::optional<uint8> Gear = State->CharacterGear(Character);
				if (!Gear)
				{
					Fail(Context, EEventFault::TableIndexOutOfRange, Character);
					return;
				}
				if (*Gear == FGameState::EmptySlot)
				{
					continue;
				}
				const std::optional<uint32> Hp = State->GearHp(*Gear);
				const std::optional<uint32> Most = State->GearMaxHp(*Gear);
				if (!Hp || !Most)
				{
					// The original changes whatever lies where a twenty-first gear's record would.
					Fail(Context, EEventFault::TableIndexOutOfRange, *Gear);
					return;
				}
				if (bAdd)
				{
					const uint32 Raised = *Hp + static_cast<uint32>(Amount);
					State->SetGearHp(*Gear, *Most < Raised ? *Most : Raised);
				}
				else
				{
					const int32 Left = static_cast<int32>(*Hp) - Amount;
					State->SetGearHp(*Gear, static_cast<uint32>(Left < 1 ? 1 : Left));
				}
			}
			Advance(Context, 4);
		}

		/**
		 * FE D0: a character's record and learned skills copied over another's; made as character 9 or 10 it
		 * sets that stand-in's story flag. The characters are taken as they are, party slots unresolved.
		 */
		void CopyCharacter(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			const int32 From = ReadValue(Context, 1);
			const int32 To = ReadValue(Context, 3);
			if (!State->CopyCharacter(From, To))
			{
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(From < 0 || From >= FGameState::CharacterCount ? From : To));
				return;
			}
			if (To == SecondStandIn)
			{
				State->SetStoryFlags(State->StoryFlags() | SecondStandInMade);
			}
			if (To == ThirdStandIn)
			{
				State->SetStoryFlags(State->StoryFlags() | ThirdStandInMade);
			}
			Advance(Context, 5);
		}

		/** FE D1: the story's flag that opens the seventh AP tier and Infinity. */
		void OpenSeventhApTier(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			State->SetStoryFlags(State->StoryFlags() | SeventhApTier);
			Advance(Context, 1);
		}

		/** FE DB: a character's HP and EP back to their highest; the character is taken as it is. */
		void RestoreCharacter(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			const int32 Character = ReadValue(Context, 1);
			if (!HasRecord(Context, Character))
			{
				return;
			}
			State->SetCharacterPoints(Character, ECharacterPoints::Hp, State->CharacterPoints(Character, ECharacterPoints::MaxHp));
			State->SetCharacterPoints(Character, ECharacterPoints::Ep, State->CharacterPoints(Character, ECharacterPoints::MaxEp));
			Advance(Context, 3);
		}

		/** FE DE: gives a character the special skills of a mask's bits; the character is taken as it is. */
		void GiveSpecialSkills(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			const int32 Character = ReadValue(Context, 1);
			const std::optional<uint16> Skills = State->CharacterSpecialSkills(Character);
			if (!Skills)
			{
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Character));
				return;
			}
			State->SetCharacterSpecialSkills(Character, static_cast<uint16>(*Skills | static_cast<uint16>(ReadValue(Context, 3))));
			Advance(Context, 5);
		}

		/** FE E1: a gear's record copied over another's. */
		void CopyGear(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			const int32 From = ReadValue(Context, 1);
			const int32 To = ReadValue(Context, 3);
			if (!State->CopyGear(From, To))
			{
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(From < 0 || From >= FGameState::GearCount ? From : To));
				return;
			}
			Advance(Context, 5);
		}
	}

	void RegisterCharacters(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0x79] = &RestoreAll<ECharacterPoints::Hp, ECharacterPoints::MaxHp>;
		Base[0x7A] = &RestoreAll<ECharacterPoints::Ep, ECharacterPoints::MaxEp>;
		Base[0x7B] = &ChangePartyPoints<EPartyPoints::TakeHp>;
		Base[0x7C] = &ChangePartyPoints<EPartyPoints::GiveHp>;
		Base[0x7D] = &ChangePartyPoints<EPartyPoints::TakeEp>;
		Base[0x7E] = &ChangePartyPoints<EPartyPoints::GiveEp>;
		Base[0x91] = &IfCharacterInParty;
		Base[0xB9] = &IfCharacterJoined;
		Base[0xBA] = &SetCharacterJoined<true>;
		Base[0xBB] = &SetCharacterJoined<false>;
		Extended[0x3A] = &SetMenuCharacter<true>;
		Extended[0x41] = &SetSlotInGear<true>;
		Extended[0x42] = &SetSlotInGear<false>;
		Extended[0x3B] = &SetMenuCharacter<false>;
		Extended[0x69] = &GetCharacterWeight;
		Extended[0x6B] = &SetCharacterWeight;
		Extended[0x8B] = &GetOwnPartySlot;
		Extended[0x9F] = &SetCharacterLocked;
		Extended[0xA1] = &SetCharacterGear;
		Extended[0xA4] = &RestoreGears;
		Extended[0xAB] = &ChangeGearHp<true>;
		Extended[0xAC] = &ChangeGearHp<false>;
		Extended[0xAD] = &GetPartyPoints<ECharacterPoints::Hp>;
		Extended[0xB4] = &GetPartyPoints<ECharacterPoints::Ep>;
		Extended[0xC7] = &GetCharacterGear;
		Extended[0xD0] = &CopyCharacter;
		Extended[0xD1] = &OpenSeventhApTier;
		Extended[0xDB] = &RestoreCharacter;
		Extended[0xDE] = &GiveSpecialSkills;
		Extended[0xE1] = &CopyGear;
	}
}
