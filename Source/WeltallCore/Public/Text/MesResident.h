#pragma once

#include "Foundation/Result.h"
#include "Text/MesFont.h"
#include "Text/MessageTable.h"

#include <array>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FPsxExecutable;

	/** What the message system keeps for the whole game: the font, the kernel's name tables and its palettes for the font's lines. */
	struct FMesResident
	{
		FMesFont Font;
		FMesNameTables NameTables;
		/** The kernel's two palettes of 16 colours a window's even and odd lines are read with. */
		std::array<uint16, 32> TextClut{};
	};

	/** Loads them as the game does at boot, the box glyph from the executable. */
	WELTALLCORE_API TResult<FMesResident> LoadMesResident(const FCdImage& Image, const FGameFileIndex& Index, const FPsxExecutable& Executable);
}
