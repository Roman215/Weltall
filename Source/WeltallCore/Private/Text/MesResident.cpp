#include "Text/MesResident.h"

#include "Formats/Lzss.h"
#include "Foundation/LittleEndian.h"
#include "Foundation/Sha1.h"
#include "Media/CdImage.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Text/MesFontSheet.h"
#include "Text/MessageOverrides.h"
#include "Text/MessageTableIds.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace Weltall
{
	namespace
	{
		/** Where the kernel keeps them (both discs of the USA release). */
		constexpr int32 ResidentDirectory = 1;
		constexpr int32 FontFile = 6;
		constexpr int32 NameTablesFile = 7;
		constexpr uint32 BoxGlyphAddress = 0x800501D0;
		/**
		 * The French translation's message code, from its glyph widths' routine to the end of their table (one byte of
		 * pixels per code from the font's first), and its SHA-1 in all four of its executables.
		 */
		constexpr uint32 VariableWidthCodeAddress = 0x80034F98;
		constexpr uint32 VariableWidthCodeEnd = 0x8003569C;
		constexpr std::string_view VariableWidthCode = "dfa6e69621c222f6fb2ff7a6b85496da41e32745";
		constexpr uint32 VariableWidthsAddress = 0x800355CC;

		/** The executable's widths of a variable-width font, as its program reads them for every code; none for the game's own program. */
		/** A variable-width font's advances by code, and the widths its glyphs are drawn with. */
		struct FVariableWidths
		{
			std::array<uint8, 256> Advances{};
			std::array<uint8, 256> Drawn{};
		};

		std::optional<FVariableWidths> VariableWidths(const FPsxExecutable& Executable, uint32 FirstCode)
		{
			const std::span<const uint8> Code = Executable.Read(VariableWidthCodeAddress, VariableWidthCodeEnd - VariableWidthCodeAddress);
			const std::span<const uint8> Table = Executable.Read(VariableWidthsAddress - FirstCode, 256);
			// The drawing takes a code's distance from the first as 16 bits: below it, 64 KB on.
			const std::span<const uint8> Below = Executable.Read(VariableWidthsAddress + 0x10000 - FirstCode, FirstCode);
			if (Code.empty() || Table.empty() || Below.size() != FirstCode || FSha1::ToHex(FSha1::Hash(Code)) != VariableWidthCode)
			{
				return std::nullopt;
			}
			FVariableWidths Widths;
			std::copy(Table.begin(), Table.end(), Widths.Advances.begin());
			Widths.Drawn = Widths.Advances;
			std::copy(Below.begin(), Below.end(), Widths.Drawn.begin());
			return Widths;
		}

		TResult<std::vector<uint8>> ReadPacked(const FCdImage& Image, const FGameFileIndex& Index, int32 Number)
		{
			const FIndexRecord* Record = Index.Find(ResidentDirectory, Number);
			if (Record == nullptr)
			{
				return MakeError("the disc lacks file " + std::to_string(Number) + " of directory 1");
			}
			const TResult<std::vector<uint8>> Sectors = ReadFileSectors(Image, *Record);
			if (!Sectors)
			{
				return Sectors.Error();
			}
			return UnpackLzss(*Sectors);
		}

		/** The name tables' file with the player's names in place: a table they change goes after the file's end, its offset moved there. */
		void ApplyNameOverrides(const FCdImage& Image, std::vector<uint8>& File)
		{
			const uint32 Count = File.size() >= 4 ? LittleEndian::ReadU32(File, 0) : 0;
			for (uint32 Table = 0; Table < Count && 4 + 4 * (static_cast<std::size_t>(Table) + 1) <= File.size(); ++Table)
			{
				const uint32 Offset = LittleEndian::ReadU32(File, 4 + 4 * static_cast<std::size_t>(Table));
				if (static_cast<int32>(Table) == FMesNameTables::GlyphMapTable || Offset >= File.size())
				{
					continue;
				}
				std::optional<FAppliedMessages> Applied = ApplyMessageOverrides(Image, MessageTables::Names(static_cast<int32>(Table)),
					std::span<const uint8>(File).subspan(Offset), EMessageWindows::Program);
				if (Applied)
				{
					const uint32 End = static_cast<uint32>(File.size());
					File.insert(File.end(), Applied->Table.begin(), Applied->Table.end());
					LittleEndian::WriteU32(File, 4 + 4 * static_cast<std::size_t>(Table), End);
				}
			}
		}
	}

	TResult<FMesResident> LoadMesResident(const FCdImage& Image, const FGameFileIndex& Index, const FPsxExecutable& Executable)
	{
		const TResult<std::vector<uint8>> Font = ReadPacked(Image, Index, FontFile);
		if (!Font)
		{
			return Font.Error();
		}
		TResult<std::vector<uint8>> Names = ReadPacked(Image, Index, NameTablesFile);
		if (!Names)
		{
			return Names.Error();
		}
		ApplyNameOverrides(Image, *Names);
		const std::span<const uint8> Box = Executable.Read(BoxGlyphAddress, FMesFont::GlyphBytes);
		TResult<FMesFont> ParsedFont = FMesFont::Parse(*Font, Box);
		if (!ParsedFont)
		{
			return ParsedFont.Error();
		}
		if (const std::optional<FVariableWidths> Widths = VariableWidths(Executable, ParsedFont->FirstGlyphCode()))
		{
			ParsedFont->SetVariableWidths(Widths->Advances, Widths->Drawn);
		}
		if (const std::shared_ptr<const FMesFontSheet> Sheet = Image.FontSheet())
		{
			Sheet->DrawInto(*ParsedFont);
		}
		TResult<FMesNameTables> ParsedNames = FMesNameTables::Parse(std::move(*Names));
		if (!ParsedNames)
		{
			return ParsedNames.Error();
		}
		return FMesResident{std::move(*ParsedFont), std::move(*ParsedNames)};
	}
}
