#include "Battle/BattleProgram.h"

#include "Formats/Lzss.h"
#include "Foundation/LittleEndian.h"
#include "Foundation/Sha1.h"
#include "Media/GameFileIndex.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

namespace Weltall
{
	namespace
	{
		constexpr int32 ProgramDirectory = 1;
		constexpr int32 ProgramFile = 16;
		/** Where the kernel loads the program. */
		constexpr uint32 Base = 0x8006FAF0;
		constexpr uint32 PatternPointersAddress = 0x800C3160;
		constexpr uint32 DeathblowPointersAddress = 0x800C31AC;
		constexpr uint32 ApBarAddress = 0x800C31D4;
		constexpr uint32 CommandBitsAddress = 0x800C3234;
		constexpr uint32 BasicAttacksAddress = 0x800C34B4;
		constexpr uint32 GearAttacksAddress = 0x800C34CC;
		constexpr uint32 EffectBasesAddress = 0x800C3630;
		constexpr uint32 EffectCastLimitsAddress = 0x800C3648;
		constexpr uint32 VoiceSetsAddress = 0x800C35DC;
		constexpr uint32 TrailCurveAddress = 0x800C37E8;
		/** The French translation's program (unpacked), by its SHA-1. */
		constexpr std::string_view FrenchProgram = "ff6e9fc59534f6867e183db0faf3981cb28695e7";

		/** The program's bytes at a memory address, or none past the program. */
		std::optional<std::span<const uint8>> At(std::span<const uint8> Program, uint32 Address, std::size_t Count)
		{
			if (Address < Base || Address - Base + Count > Program.size())
			{
				return std::nullopt;
			}
			return Program.subspan(Address - Base, Count);
		}

		template <std::size_t Count>
		bool Copy(std::span<const uint8> Program, uint32 Address, std::array<uint8, Count>& Into)
		{
			const std::optional<std::span<const uint8>> Bytes = At(Program, Address, Count);
			if (!Bytes)
			{
				return false;
			}
			std::copy(Bytes->begin(), Bytes->end(), Into.begin());
			return true;
		}

		/** The table a pointer of the program's points at. */
		template <std::size_t Count>
		bool CopyPointed(std::span<const uint8> Program, uint32 PointerAddress, std::array<uint8, Count>& Into)
		{
			const std::optional<std::span<const uint8>> Pointer = At(Program, PointerAddress, 4);
			return Pointer && Copy(Program, LittleEndian::ReadU32(*Pointer, 0), Into);
		}

		/** A table of signed words. */
		template <std::size_t Count>
		bool CopyWords(std::span<const uint8> Program, uint32 Address, std::array<int16, Count>& Into)
		{
			const std::optional<std::span<const uint8>> Bytes = At(Program, Address, 2 * Count);
			if (!Bytes)
			{
				return false;
			}
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				Into[Index] = static_cast<int16>(LittleEndian::ReadU16(*Bytes, 2 * Index));
			}
			return true;
		}
	}

	TResult<FBattleProgramTables> FBattleProgramTables::Parse(std::span<const uint8> Program)
	{
		FBattleProgramTables Tables;
		bool bRead = true;
		for (std::size_t Pattern = 0; Pattern < Tables.Patterns.size(); ++Pattern)
		{
			bRead = bRead && CopyPointed(Program, PatternPointersAddress + static_cast<uint32>(4 * Pattern), Tables.Patterns[Pattern]);
		}
		for (std::size_t Character = 0; Character < Tables.Deathblows.size(); ++Character)
		{
			bRead = bRead && CopyPointed(Program, DeathblowPointersAddress + static_cast<uint32>(4 * Character), Tables.Deathblows[Character]);
		}
		for (std::size_t Row = 0; Row < Tables.ApBar.size(); ++Row)
		{
			bRead = bRead && Copy(Program, ApBarAddress + static_cast<uint32>(ApBarColumns * Row), Tables.ApBar[Row]);
		}
		bRead = bRead && Copy(Program, BasicAttacksAddress, Tables.BasicAttacks) && Copy(Program, GearAttacksAddress, Tables.GearAttacks);
		bRead = bRead && CopyWords(Program, EffectBasesAddress, Tables.EffectBases) && CopyWords(Program, EffectCastLimitsAddress, Tables.EffectCastLimits);
		const std::optional<std::span<const uint8>> Bits = At(Program, CommandBitsAddress, 2 * Tables.CommandBits.size());
		const std::optional<std::span<const uint8>> Voices = At(Program, VoiceSetsAddress, 4 * Tables.VoiceSets.size());
		const std::optional<std::span<const uint8>> Curve = At(Program, TrailCurveAddress, 16 * Tables.TrailCurve.size());
		if (!bRead || !Bits || !Voices || !Curve)
		{
			return MakeError("the Battle program is too short to hold its command tables");
		}
		for (std::size_t Step = 0; Step < Tables.TrailCurve.size(); ++Step)
		{
			for (std::size_t Point = 0; Point < 4; ++Point)
			{
				// A word each, of which the GTE takes the low half.
				Tables.TrailCurve[Step][Point] = static_cast<int16>(LittleEndian::ReadU16(*Curve, 16 * Step + 4 * Point));
			}
		}
		for (std::size_t Set = 0; Set < Tables.VoiceSets.size(); ++Set)
		{
			Tables.VoiceSets[Set] = {static_cast<int16>(LittleEndian::ReadU16(*Voices, 4 * Set)), static_cast<int16>(LittleEndian::ReadU16(*Voices, 4 * Set + 2))};
		}
		for (std::size_t Command = 0; Command < Tables.CommandBits.size(); ++Command)
		{
			Tables.CommandBits[Command] = LittleEndian::ReadU16(*Bits, 2 * Command);
		}
		Tables.bQueueKeepsCamera = FSha1::ToHex(FSha1::Hash(Program)) == FrenchProgram;
		return Tables;
	}

	TResult<std::vector<uint8>> ReadBattleProgram(const FCdImage& Image, const FGameFileIndex& Index)
	{
		const FIndexRecord* Record = Index.Find(ProgramDirectory, ProgramFile);
		if (Record == nullptr)
		{
			return MakeError("the disc lacks the Battle program");
		}
		const TResult<std::vector<uint8>> Sectors = ReadFileSectors(Image, *Record);
		if (!Sectors)
		{
			return Sectors.Error();
		}
		return UnpackLzss(*Sectors);
	}

	TResult<FBattleProgramTables> FBattleProgramTables::Load(const FCdImage& Image, const FGameFileIndex& Index)
	{
		const TResult<std::vector<uint8>> Program = ReadBattleProgram(Image, Index);
		if (!Program)
		{
			return Program.Error();
		}
		return Parse(*Program);
	}
}
