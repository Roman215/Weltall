#include "Game/SaveState.h"
#include "Game/SaveStateMembers.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

using namespace Weltall;

namespace
{
	/** Where each member a type's VisitMembers hands over lies in the value, without going into it. */
	struct FMemberPlaces
	{
		struct FPlace
		{
			std::size_t Offset = 0;
			std::size_t Size = 0;
			std::size_t Align = 1;
		};

		const std::byte* Base = nullptr;
		std::size_t Size = 0;
		std::vector<FPlace> Places;

		template <typename T>
		void operator()(T& Member)
		{
			Record(Member);
		}

		template <typename T>
		void Skip(T& Member)
		{
			Record(Member);
		}

		template <typename T>
		void Record(T& Member)
		{
			const std::byte* At = reinterpret_cast<const std::byte*>(&Member);
			REQUIRE(At >= Base);
			REQUIRE(At + sizeof(T) <= Base + Size);
			Places.push_back({static_cast<std::size_t>(At - Base), sizeof(T), alignof(T)});
		}
	};

	constexpr std::size_t AlignUp(std::size_t Offset, std::size_t Align)
	{
		return (Offset + Align - 1) / Align * Align;
	}

	/**
	 * What of a value its VisitMembers leaves out: the bytes between the members it hands over that are more than the
	 * padding the next member's alignment puts before it (a member added and not written), and members handed twice.
	 */
	template <typename T>
	std::string LeftOut()
	{
		T Value = T();
		FMemberPlaces Members;
		Members.Base = reinterpret_cast<const std::byte*>(&Value);
		Members.Size = sizeof(T);
		VisitMembers(Members, Value);
		std::sort(Members.Places.begin(), Members.Places.end(), [](const auto& A, const auto& B) { return A.Offset < B.Offset; });
		std::size_t End = 0;
		for (const FMemberPlaces::FPlace& Place : Members.Places)
		{
			if (Place.Offset < End)
			{
				return "a member at " + std::to_string(Place.Offset) + " is handed over twice";
			}
			if (Place.Offset != AlignUp(End, Place.Align))
			{
				return "bytes " + std::to_string(End) + " to " + std::to_string(Place.Offset) + " are no member handed over";
			}
			End = Place.Offset + Place.Size;
		}
		if (AlignUp(End, alignof(T)) != sizeof(T))
		{
			return "bytes " + std::to_string(End) + " to " + std::to_string(sizeof(T)) + " are no member handed over";
		}
		return {};
	}

	/** Every number a running count, every list two long, every optional held. */
	struct FFiller
	{
		uint32 Next = 0;

		template <typename T>
		void operator()(T& Member)
		{
			SaveStateMembers::Visit(*this, Member);
		}

		template <typename T>
		void Number(T& Value)
		{
			++Next;
			if constexpr (std::is_same_v<T, bool>)
			{
				Value = true;
			}
			else if constexpr (std::is_enum_v<T>)
			{
				Value = static_cast<T>(Next % 3);
			}
			else
			{
				Value = static_cast<T>(Next * 37u);
			}
		}

		void Count(uint32& Value) { Value = 2; }

		template <typename T>
		void Skip(T&)
		{
		}
	};

	FSaveState Filled()
	{
		FSaveState State;
		FFiller Filler;
		Filler(State);
		return State;
	}
}

TEST_CASE("A save state's writer hands over every member of each type it holds", "[Game]")
{
	CHECK(LeftOut<FGteMatrix>() == "");
	CHECK(LeftOut<FMdlColor>() == "");
	CHECK(LeftOut<FPlayClock>() == "");
	CHECK(LeftOut<FGameState>() == "");
	CHECK(LeftOut<FSpriteSavedState>() == "");
	CHECK(LeftOut<FEventThread>() == "");
	CHECK(LeftOut<FEventActorState::FRide>() == "");
	CHECK(LeftOut<FEventActorState>() == "");
	CHECK(LeftOut<FFieldSavedActor>() == "");
	CHECK(LeftOut<FFieldCameraRamp>() == "");
	CHECK(LeftOut<FFieldCameraMove>() == "");
	CHECK(LeftOut<FFieldView>() == "");
	CHECK(LeftOut<FFieldCamera>() == "");
	CHECK(LeftOut<FFieldFog>() == "");
	CHECK(LeftOut<FFieldPanorama>() == "");
	CHECK(LeftOut<FFieldGearLights::FPointLight>() == "");
	CHECK(LeftOut<FFieldGearLights>() == "");
	CHECK(LeftOut<FFieldFade>() == "");
	CHECK(LeftOut<FFieldScreenWave>() == "");
	CHECK(LeftOut<FFieldMovement>() == "");
	CHECK(LeftOut<FFieldSnapshot>() == "");
	CHECK(LeftOut<FFieldClock>() == "");
	CHECK(LeftOut<FSpriteSystem::FRingPlace>() == "");
	CHECK(LeftOut<FFieldKernelState>() == "");
	CHECK(LeftOut<FMovieProgramRequest>() == "");
	CHECK(LeftOut<FKernelState>() == "");
	CHECK(LeftOut<FSaveState>() == "");
}

TEST_CASE("A save state's file reads back as it was written, every member of it", "[Game]")
{
	const FSaveState State = Filled();
	REQUIRE(State.Kernel.Field.Saved.has_value());
	const std::vector<uint8> Written = WriteSaveState(State);
	ESaveStateFault Fault = ESaveStateFault::NotAState;
	const std::optional<FSaveState> Read = ReadSaveState(Written, Fault);
	REQUIRE(Read.has_value());
	CHECK(WriteSaveState(*Read) == Written);
	CHECK(Read->Kernel.Field.Saved->Actors.size() == 2);
	CHECK(Read->Kernel.Field.Saved->Actors[1].State.Threads[7].Word == State.Kernel.Field.Saved->Actors[1].State.Threads[7].Word);
	CHECK(Read->Game.PlayTime() == State.Game.PlayTime());
	CHECK(Read->Random.Seed() == State.Random.Seed());
	CHECK(Read->Clock.Hours == State.Clock.Hours);
	CHECK(Read->Kernel.WorldMapKeep == nullptr);

	// A state without the field's: the world map's.
	FSaveState WorldMap = State;
	WorldMap.Kernel.Field.Saved.reset();
	const std::optional<FSaveState> ReadWorldMap = ReadSaveState(WriteSaveState(WorldMap), Fault);
	REQUIRE(ReadWorldMap.has_value());
	CHECK_FALSE(ReadWorldMap->Kernel.Field.Saved.has_value());
}

TEST_CASE("A save state's file of another version, cut short, changed or of something else is refused", "[Game]")
{
	const std::vector<uint8> Written = WriteSaveState(Filled());
	ESaveStateFault Fault = ESaveStateFault::NotAState;

	std::vector<uint8> OtherVersion = Written;
	OtherVersion[8] = static_cast<uint8>(FSaveState::FormatVersion + 1);
	CHECK_FALSE(ReadSaveState(OtherVersion, Fault).has_value());
	CHECK(Fault == ESaveStateFault::OtherVersion);

	std::vector<uint8> Changed = Written;
	Changed[Changed.size() / 2] ^= 0x10;
	CHECK_FALSE(ReadSaveState(Changed, Fault).has_value());
	CHECK(Fault == ESaveStateFault::Damaged);

	std::vector<uint8> Short(Written.begin(), Written.end() - 1);
	CHECK_FALSE(ReadSaveState(Short, Fault).has_value());
	CHECK(Fault == ESaveStateFault::Damaged);

	std::vector<uint8> Something(Written.size(), 0x41);
	CHECK_FALSE(ReadSaveState(Something, Fault).has_value());
	CHECK(Fault == ESaveStateFault::NotAState);
	CHECK_FALSE(ReadSaveState({}, Fault).has_value());
	CHECK(Fault == ESaveStateFault::NotAState);
}
