#pragma once

#include "Foundation/Types.h"
#include "Text/MesCharacters.h"
#include "Text/MessageTable.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Weltall
{
	/**
	 * A message's codes as text, and back. The text: the font's plain characters as themselves, a line break
	 * for a new line, and marks in braces, their numbers hexadecimal:
	 *
	 *   {page}  a new page after the button        {more}   the button, then on in the same window
	 *   {wait 14} {speed 02} {close 3C} {auto 3C} {delay 01}    the window's timing
	 *   {name 00} {item} {table 01 2A} {weapon 03} {table24 00} {table25 00}    names put in
	 *   {number 00} {digits 00} {signed 00} {mark 00} {button 00}               values put in
	 *   {Circle} {Square} {Triangle} {Cross}       the controller's buttons
	 *   {box}                                      the glyph the kernel carries itself
	 *   {8F}  any other one-byte code              {FE12}  a glyph of two bytes
	 *   {x10}  an extended code without operands that is none of the above
	 */

	/** The codes up to their 00, or all of them without one, as text. */
	WELTALLCORE_API std::string MesTextOfCodes(std::span<const uint8> Codes);

	/** The codes of Text, ended by 00; none where a character has no glyph or a mark is none of the marks. */
	WELTALLCORE_API std::optional<std::vector<uint8>> MesCodesOfText(std::string_view Text);

	/** As MesTextOfCodes, a glyph written as the character Characters gives it, and in braces only without one. */
	WELTALLCORE_API std::string MesTextOfCodes(std::span<const uint8> Codes, const FMesCharacters& Characters);

	/** As MesCodesOfText, its characters those of Characters. */
	WELTALLCORE_API std::optional<std::vector<uint8>> MesCodesOfText(std::string_view Text, const FMesCharacters& Characters);

	/** How many of Codes the message at their start takes as MesTextOfCodes reads it, its 00 included; all of them without one. */
	WELTALLCORE_API std::size_t MesMessageSize(std::span<const uint8> Codes);

	/** The glyphs the message types itself, in order: what its marks put in is not among them. */
	WELTALLCORE_API std::vector<FMesCode> MesGlyphsOfCodes(std::span<const uint8> Codes);

	/** A window's size for a message, as its table gives it. */
	struct FMesExtent
	{
		/** In units of 4 pixels. */
		int32 WidthUnits = 0;
		int32 Lines = 0;
	};

	/**
	 * The window a message's own glyphs need: its widest line (a plain character two units, any other glyph
	 * three) and its tallest page. What a mark puts in at play (a name, a number) is not counted.
	 */
	WELTALLCORE_API FMesExtent MesExtentOfCodes(std::span<const uint8> Codes);

	/** Glyphs a font sheet gives widths of their own, in units of 4 pixels, by their code's lead << 8 | code. */
	using FMesGlyphWidths = std::unordered_map<uint16, int32>;

	/** As MesExtentOfCodes, the glyphs of Widths as wide as it says. */
	WELTALLCORE_API FMesExtent MesExtentOfCodes(std::span<const uint8> Codes, const FMesGlyphWidths& Widths);

	/**
	 * A message's pages From to To, counted from 1, as a message of their own: their codes, ended by 00, the page marks
	 * between them kept and the ones around them left out. To past the last page is the last; none where the message
	 * has fewer pages than From.
	 */
	WELTALLCORE_API std::optional<std::vector<uint8>> MesPagesOfCodes(std::span<const uint8> Codes, int32 From, int32 To);

	/** Two messages as one, Second on a new page after First's last as the game's own pages follow one another; ended by 00. */
	WELTALLCORE_API std::vector<uint8> MesPagesJoined(std::span<const uint8> First, std::span<const uint8> Second);

	/** Text as a message for a table, in a window of its own glyphs' size; none as MesCodesOfText, or past a table's sizes. */
	WELTALLCORE_API std::optional<FAddedMessage> MesMessageOfText(std::string_view Text);

	/** A message table of Messages alone; none where its 16-bit offsets could not reach the last. */
	WELTALLCORE_API std::optional<std::vector<uint8>> BuildMessageTable(std::span<const FAddedMessage> Messages);

	/**
	 * Table with each replacement's message in place of the one of its number and the others as they are, one the table
	 * starts past its bytes still past them; none where the bytes are not a table, a number is past its count, or the
	 * 16-bit offsets could not reach the last message.
	 */
	WELTALLCORE_API std::optional<std::vector<uint8>> ReplaceMessages(std::span<const uint8> Table, std::span<const std::pair<int32, FAddedMessage>> Replacements);
}
