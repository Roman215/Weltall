#pragma once

#include "Field/FieldSnapshotMembers.h"
#include "Game/GameState.h"
#include "Game/KernelState.h"
#include "Game/PsxRandom.h"
#include "Game/SaveState.h"
#include "Timing/PlayClock.h"

#include <array>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace Weltall
{
	/**
	 * Every member of the types a save state holds, in the order the state keeps them. VisitMembers(Archive, Value) hands
	 * each member to Archive(Member), which writes it, reads it or checks it (SaveStateMembers::Visit goes into each
	 * member's own); the field's types are in Field/FieldSnapshotMembers.h and Field/FieldActorMembers.h. A member added
	 * to one of these types is added to its VisitMembers, and the state's format version raised; the core's tests find a
	 * member left out. Ar.Skip(Member) passes over what a state never needs, saying so.
	 */
	namespace SaveStateMembers
	{
		template <typename T>
		inline constexpr bool IsArray = false;
		template <typename T, std::size_t N>
		inline constexpr bool IsArray<std::array<T, N>> = true;
		template <typename T>
		inline constexpr bool IsList = false;
		template <typename T>
		inline constexpr bool IsList<std::vector<T>> = true;
		template <typename T>
		inline constexpr bool IsList<std::basic_string<T>> = true;
		template <typename T>
		inline constexpr bool IsOptional = false;
		template <typename T>
		inline constexpr bool IsOptional<std::optional<T>> = true;

		/**
		 * Value handed to Archive: a whole number, a bool or an enum as Archive.Number; an array's elements in turn; a list's
		 * (or a string's) length as Archive.Count (which may shorten it), then its elements; whether an optional holds a value, then the
		 * value; anything else its members.
		 */
		template <typename FArchive, typename T>
		void Visit(FArchive& Archive, T& Value)
		{
			if constexpr (std::is_integral_v<T> || std::is_enum_v<T>)
			{
				Archive.Number(Value);
			}
			else if constexpr (IsArray<T>)
			{
				for (auto& Element : Value)
				{
					Visit(Archive, Element);
				}
			}
			else if constexpr (IsList<T>)
			{
				uint32 Count = static_cast<uint32>(Value.size());
				Archive.Count(Count);
				Value.resize(Count);
				for (auto& Element : Value)
				{
					Visit(Archive, Element);
				}
			}
			else if constexpr (IsOptional<T>)
			{
				bool bHeld = Value.has_value();
				Archive.Number(bHeld);
				if (!bHeld)
				{
					Value.reset();
					return;
				}
				if (!Value)
				{
					Value.emplace();
				}
				Visit(Archive, *Value);
			}
			else
			{
				VisitMembers(Archive, Value);
			}
		}
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FPsxRandom& Value)
	{
		uint32 Seed = Value.Seed();
		Ar(Seed);
		Value = FPsxRandom(Seed);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FPlayClock& Value)
	{
		Ar(Value.Frames);
		Ar(Value.Seconds);
		Ar(Value.Minutes);
		Ar(Value.Hours);
		Ar(Value.bStopped);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FPartyOrder& Value)
	{
		std::array<uint8, FPartyOrder::SavedBytes> Saved = Value.ToSaved();
		Ar(Saved);
		Value = FPartyOrder::FromSaved(Saved);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FCharacterLooks& Value)
	{
		std::array<uint8, FCharacterLooks::SavedBytes> Saved = Value.ToSaved();
		Ar(Saved);
		Value = FCharacterLooks::FromSaved(Saved);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FGameState& Value)
	{
		Ar(Value.Bytes);
		Ar(Value.Globals);
		Ar(Value.PlayVblanks);
		Ar(Value.Mode);
		Ar(Value.ExpansionFolder);
		Ar(Value.Order);
		Ar(Value.Looks);
		Ar(Value.Clear);
		Ar(Value.ProgramResult);
		Ar(Value.BattleActingTurns);
		Ar(Value.MenuLastCommand);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FMovieProgramRequest& Value)
	{
		Ar(Value.Type);
		Ar(Value.Number);
		Ar(Value.NextProgram);
		Ar(Value.NoSkip);
		Ar(Value.EndFrame);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FKernelState& Value)
	{
		Ar(Value.NextProgram);
		Ar(Value.Movie);
		Ar(Value.Disc);
		Ar(Value.LightMatrix);
		Ar(Value.BattleKind);
		Ar(Value.bBattleDebug);
		Ar(Value.ChainedBattle);
		Ar(Value.bResultFanfare);
		Ar(Value.Music);
		Ar(Value.Formations);
		Ar(Value.Field);
		Ar(Value.bFieldEntered);
		Ar(Value.FieldTrips);
		Ar(Value.ArenaTrips);
		Ar(Value.bEventFadeIn);
		Ar(Value.MenuMode);
		Ar(Value.bMenuStandalone);
		Ar(Value.BattlingParameters);
		Ar(Value.BattlingTask);
		// Read only as the world map comes back from a battle, which never follows a moment a state is saved at.
		Ar.Skip(Value.WorldMapKeep);
		Ar(Value.WindowColour);
		Ar(Value.BattleWindowBlend);
	}

	template <typename FArchive>
	void VisitMembers(FArchive& Ar, FSaveState& Value)
	{
		Ar(Value.Game);
		Ar(Value.Kernel);
		Ar(Value.Random);
		Ar(Value.Clock);
	}
}
