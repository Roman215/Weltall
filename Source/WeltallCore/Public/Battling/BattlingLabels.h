#pragma once

#include "Battling/BattlingData.h"
#include "Menu/MenuPrimitive.h"
#include "Presentation/DrawList.h"

#include <array>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

namespace Weltall
{
	namespace BattlingLabelTexts
	{
		/** Practice's name of a shot of Ether (its code builds the string without a lui of its own). */
		inline constexpr std::string_view Ether = "ETHER";
		/** Practice's frame rates by the setting. */
		inline constexpr std::array<uint8, 5> PracticeFrameRates{30, 20, 15, 12, 10};
	}

	/** One of the program's printf strings (FBattlingTexts) with its conversions (%d, %02d, %s) replaced by Values in turn, already formatted. */
	WELTALLCORE_API std::string FormatBattlingText(std::string_view Format, std::initializer_list<std::string_view> Values);

	/** A letter a label printed, as its packet holds it: its corners on the screen, each corner's texel (u | v << 8) and its colour. */
	struct FBattlingLabelGlyph
	{
		int16 Left = 0;
		int16 Top = 0;
		int16 Right = 0;
		int16 Bottom = 0;
		/** The corners' texels: top left, top right, bottom left, bottom right. */
		std::array<uint16, 4> Texels{};
		FDrawColor Colour;
	};

	/**
	 * Battling's labels: the program prints text into its draw buffer's list of letters
	 * as it runs, from a place, at a scale and in a colour it sets beforehand, and the frame's drawing takes the list.
	 */
	class FBattlingLabels
	{
	public:
		/** A buffer's list holds a hundred letters; the original writes a hundred and first past it. */
		static constexpr int32 Capacity = 100;
		/** The scale letters are drawn at, 100h their own size. */
		static constexpr int32 OwnSize = 0x100;
		/** How far a line printed moves the place down. */
		static constexpr int16 LineHeight = 0x14;

		/** The font must outlive the labels. */
		explicit FBattlingLabels(const FBattlingProgramTables& InTables) : Tables(InTables) {}

		void SetPosition(int32 InX, int32 InY)
		{
			X = static_cast<int16>(InX);
			Y = static_cast<int16>(InY);
		}

		void SetScale(int32 InScale) { Scale = InScale; }

		/** White, or a yellow whose red pulses with the vertical blanks. */
		WELTALLCORE_API void SetFlash(bool bOn, uint32 VBlank);
		/** White, or a blue whose red and green pulse with the vertical blanks. */
		WELTALLCORE_API void SetFlashBlue(bool bOn, uint32 VBlank);

		/** Prints a line from the place into Buffer's list, then moves the place a line down. */
		WELTALLCORE_API void Print(std::string_view Text, uint32 Buffer);
		/** The same, centred on the place. */
		WELTALLCORE_API void PrintCentered(std::string_view Text, uint32 Buffer);
		/** The same, ending at the place. */
		WELTALLCORE_API void PrintRight(std::string_view Text, uint32 Buffer);
		/** The same, starting Offset left of the place. */
		WELTALLCORE_API void PrintOffset(std::string_view Text, int32 Offset, uint32 Buffer);

		/** How wide a line prints at the scale: each letter's width and 2 between. */
		int32 Width(std::string_view Text) const { return Width(Text, Scale); }

		/** The same at another scale. */
		WELTALLCORE_API int32 Width(std::string_view Text, int32 AtScale) const;

		void Clear() { Count = 0; }

		/** What the program's labels and menus say. */
		const FBattlingTexts& Texts() const;

		/** The drawing's take: the letters of Buffer's list the count reaches, in the order printed (the GPU draws the last first); the count then 0. */
		WELTALLCORE_API std::span<const FBattlingLabelGlyph> Take(uint32 Buffer);

		/** A letter was printed past the list's hundred, where the original writes past its buffer. */
		bool Overflowed() const { return bOverflowed; }

		int16 X = 0;
		int16 Y = 0;
		int32 Scale = OwnSize;
		/** The program's data starts it black; its menus' set-up leaves it white before any bout. */
		FDrawColor Colour{0xFF, 0xFF, 0xFF};

	private:
		const FBattlingProgramTables& Tables;
		std::array<std::array<FBattlingLabelGlyph, Capacity>, 2> Lists{};
		int32 Count = 0;
		bool bOverflowed = false;

		void PrintFrom(int32 Left, std::string_view Text, uint32 Buffer);
	};

	/** A letter's packet: a quad of the font's page, its texels multiplied by its colour. */
	WELTALLCORE_API FMenuPrimitive MakeLabelPrimitive(const FBattlingLabelGlyph& Letter, uint16 FontPage, uint16 FontClut);
}
