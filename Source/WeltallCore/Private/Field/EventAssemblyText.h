#pragma once

#include "Foundation/LittleEndian.h"
#include "Foundation/NumberText.h"
#include "Foundation/Types.h"

#include <algorithm>
#include <array>
#include <span>
#include <string_view>
#include <vector>

/** What the texts of event scripts share: their words and marks, and how a line is taken apart. */
namespace Weltall::EventAssemblyText
{
	using namespace NumberText;

	inline constexpr uint8 EntranceMark = 0xFF;
	inline constexpr std::size_t EntranceBytes = 7;
	/** An entrance's camera or facing that keeps the previous map's. */
	inline constexpr uint8 KeepDirection = 0xFF;
	inline constexpr std::string_view Keep = "keep";

	/** The actor bytes that are no actor's number: the party slots' actors and the running one. */
	struct FNamedActor
	{
		uint8 Byte = 0;
		std::string_view Name;
	};

	inline constexpr std::array<FNamedActor, 4> NamedActors = {{{0xFF, "party0"}, {0xFE, "party1"}, {0xFD, "party2"}, {0xFB, "self"}}};

	/** The bits of If's flags byte its test takes. */
	inline constexpr uint8 TestBits = 0x0F;
	/** A value's bit that makes it a constant. */
	inline constexpr uint16 ConstantBit = 0x8000;
	/** A script operand's low bits are the script, the rest its priority. */
	inline constexpr int32 ScriptBits = 5;
	inline constexpr std::size_t DataBytesPerLine = 16;

	/** A line's words: what blanks separate, up to a ';', which begins a comment. */
	inline std::vector<std::string_view> WordsOf(std::string_view Line)
	{
		constexpr std::string_view Blanks = " \t\r";
		std::vector<std::string_view> Words;
		Line = Line.substr(0, Line.find(';'));
		for (std::size_t Start = Line.find_first_not_of(Blanks); Start != std::string_view::npos; Start = Line.find_first_not_of(Blanks))
		{
			const std::size_t End = std::min(Line.find_first_of(Blanks, Start), Line.size());
			Words.push_back(Line.substr(Start, End - Start));
			Line.remove_prefix(End);
		}
		return Words;
	}

	/**
	 * Whether an entrance table's record can be one: positions stay within the map and a map has at most four walkmesh
	 * layers. What tells the table, whose length is not recorded, from the code after it.
	 */
	inline bool CanBeEntrance(std::span<const uint8> Record)
	{
		constexpr int32 Reach = 0x3000;
		constexpr uint8 Layers = 4;
		const int32 X = LittleEndian::ReadS16(Record, 0);
		const int32 Z = LittleEndian::ReadS16(Record, 2);
		return X > -Reach && X < Reach && Z > -Reach && Z < Reach && Record[4] < Layers;
	}

	/** The entrance records a bytecode that starts with the table's mark holds before End (its first reached instruction); 0 without the mark. */
	inline std::size_t EntranceRecords(std::span<const uint8> Code, std::size_t End)
	{
		std::size_t Records = 0;
		for (std::size_t At = 1; !Code.empty() && Code[0] == EntranceMark && At + EntranceBytes <= std::min(End, Code.size())
			&& CanBeEntrance(Code.subspan(At, EntranceBytes)); At += EntranceBytes)
		{
			++Records;
		}
		return Records;
	}

	/** A name a text may give a place, an operand or a message: letters, digits, '_' and '.', a letter or '_' first. */
	inline bool IsName(std::string_view Word)
	{
		const auto IsLetter = [](char Character) { return (Character >= 'A' && Character <= 'Z') || (Character >= 'a' && Character <= 'z') || Character == '_'; };
		return !Word.empty() && IsLetter(Word.front())
			&& std::all_of(Word.begin(), Word.end(), [&IsLetter](char Character) { return IsLetter(Character) || (Character >= '0' && Character <= '9') || Character == '.'; });
	}
}
