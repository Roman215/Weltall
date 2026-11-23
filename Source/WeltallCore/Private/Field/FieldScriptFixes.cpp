#include "Field/FieldScriptFixes.h"

#include <algorithm>
#include <array>

namespace Weltall
{
	namespace
	{
		struct FScriptFix
		{
			int32 Map = 0;
			uint16 Offset = 0;
			std::array<uint8, 1> Was{};
			std::array<uint8, 1> Now{};
			bool FEventOptions::*Option = nullptr;
		};

		/**
		 * The hotel's head count (actor 14's script 4) asks twice whether party slot 1 is empty, so a party
		 * of two pays for three; its second If names slot 2's variable instead.
		 */
		constexpr FScriptFix Fixes[] = {
			{104, 0x3A0, {0x40}, {0x42}, &FEventOptions::bHotelCountsWholeParty},
			// FE 62 (volume, channel) at 0066, 0080, 00BA and 00C6 names volume 0 or 1 on channel 30 for the
			// sounds just started on channels 0 and 1: volume 30 on them, the two operands' low bytes swapped.
			{272, 0x68, {0x00}, {0x1E}, &FEventOptions::bShipSoundsAtTheirVolume},
			{272, 0x6A, {0x1E}, {0x00}, &FEventOptions::bShipSoundsAtTheirVolume},
			{272, 0x82, {0x00}, {0x1E}, &FEventOptions::bShipSoundsAtTheirVolume},
			{272, 0x84, {0x1E}, {0x00}, &FEventOptions::bShipSoundsAtTheirVolume},
			{272, 0xBC, {0x01}, {0x1E}, &FEventOptions::bShipSoundsAtTheirVolume},
			{272, 0xBE, {0x1E}, {0x01}, &FEventOptions::bShipSoundsAtTheirVolume},
			{272, 0xC8, {0x00}, {0x1E}, &FEventOptions::bShipSoundsAtTheirVolume},
			{272, 0xCA, {0x1E}, {0x00}, &FEventOptions::bShipSoundsAtTheirVolume},
			// Actor 11's script 7 fades channel 12 (FE 8C at 0944) where the room's sound plays on channel 3.
			{670, 0x948, {0x0C}, {0x03}, &FEventOptions::bMemoryAmbienceFades},
		};

		bool Matches(const FEventScripts& Scripts, std::size_t Offset, std::span<const uint8> Bytes)
		{
			return Offset <= Scripts.CodeSize() && Bytes.size() <= Scripts.CodeSize() - Offset
				&& std::equal(Bytes.begin(), Bytes.end(), Scripts.Code() + Offset);
		}

		void FixPerfectWorksGranny(FEventScripts& Scripts)
		{
			// If Bart is away, end the conversation before the next story branch.
			constexpr std::array<uint8, 4> Branch{0x91, 0x03, 0x99, 0x08};
			constexpr std::array<uint8, 4> FixedBranch{0x91, 0x03, 0x95, 0x08};
			constexpr std::array<uint8, 6> Target{0x00, 0x85, 0x3A, 0x80, 0xA4, 0x08};
			if (Matches(Scripts, 0x895, Target))
			{
				Scripts.Replace(0x872, Branch, FixedBranch);
			}
		}

		void FixPerfectWorksHangar(FEventScripts& Scripts)
		{
			// Perfect Works moved the party's platform scripts to make room for Fei's dialogue.
			// The false branch in the shared init and the jumps from the other party actors still use stale addresses.
			if (Scripts.ActorCount() < 2 || Scripts.EntryPoint(1, 4) != 0x199 || Scripts.EntryPoint(1, 5) != 0x1AC
				|| Scripts.EntryPoint(1, 8) != 0x1D0)
			{
				return;
			}
			constexpr std::array<uint8, 8> Init{0x02, 0x02, 0x00, 0x00, 0x00, 0x40, 0x64, 0x01};
			constexpr std::array<uint8, 8> FixedInit{0x02, 0x02, 0x00, 0x00, 0x00, 0x40, 0x66, 0x01};
			Scripts.Replace(0x152, Init, FixedInit);
			constexpr std::array<uint8, 8> Ride{0x01, 0xA1, 0x01, 0x00, 0x01, 0xB4, 0x01, 0x00};
			constexpr std::array<uint8, 8> FixedRide{0x01, 0x99, 0x01, 0x00, 0x01, 0xAC, 0x01, 0x00};
			constexpr std::array<std::size_t, 10> RideOffsets{0x203, 0x289, 0x2EF, 0x304, 0x319, 0x32E, 0x343, 0x358, 0x36D, 0x382};
			for (const std::size_t Offset : RideOffsets)
			{
				Scripts.Replace(Offset, Ride, FixedRide);
			}
		}

		void FixPerfectWorksNarrationFade(FEventScripts& Scripts)
		{
			// Once the fade reaches zero, hide the actor and return to the narration.
			constexpr std::array<uint8, 8> Branch{0x02, 0x02, 0x04, 0x00, 0x00, 0x42, 0x87, 0x02};
			constexpr std::array<uint8, 8> FixedBranch{0x02, 0x02, 0x04, 0x00, 0x00, 0x42, 0x8E, 0x02};
			constexpr std::array<uint8, 5> Target{0x01, 0x74, 0x02, 0x23, 0x00};
			if (Matches(Scripts, 0x28B, Target))
			{
				Scripts.Replace(0x274, Branch, FixedBranch);
			}
		}
	}

	void ApplyFieldScriptFixes(int32 Map, FEventScripts& Scripts, const FEventOptions& Options)
	{
		switch (Map)
		{
		case 75:
			FixPerfectWorksGranny(Scripts);
			break;
		case 84:
			FixPerfectWorksHangar(Scripts);
			break;
		case 618:
			FixPerfectWorksNarrationFade(Scripts);
			break;
		}
		for (const FScriptFix& Fix : Fixes)
		{
			if (Fix.Map == Map && Options.*Fix.Option)
			{
				Scripts.Replace(Fix.Offset, Fix.Was, Fix.Now);
			}
		}
	}
}
