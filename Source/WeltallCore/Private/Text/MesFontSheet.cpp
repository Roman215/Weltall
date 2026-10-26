#include "Text/MesFontSheet.h"

#include "Foundation/NumberText.h"

#include <algorithm>
#include <optional>

namespace Weltall
{
	namespace
	{
		constexpr uint8 FirstCode = 0x10;
		constexpr uint8 SecondPage = 0xFE;
		constexpr int32 FirstPageGlyphs = SecondPage - FirstCode;
		constexpr int32 SecondPageGlyphs = 0x3D;
		/** Both pages' glyphs, then the box. */
		constexpr int32 GlyphCount = FirstPageGlyphs + SecondPageGlyphs + 1;
		constexpr FMesCode BoxGlyph{0xFF, 0xFF};
		constexpr uint8 FirstWideGlyph = 0x61;
		/** A narrow glyph's pixels stay left of this column: its outline then ends before the next glyph's cell begins. */
		constexpr int32 NarrowColumns = 6;
		constexpr uint16 PastNarrowColumns = 0xFFFF >> NarrowColumns;
		constexpr uint8 Opaque = 255;
		constexpr uint8 HalfOpaque = 128;

		FMesCode CodeOfCell(int32 Cell)
		{
			if (Cell < FirstPageGlyphs)
			{
				return {0, static_cast<uint8>(FirstCode + Cell)};
			}
			if (Cell < FirstPageGlyphs + SecondPageGlyphs)
			{
				return {SecondPage, static_cast<uint8>(Cell - FirstPageGlyphs)};
			}
			return BoxGlyph;
		}

		std::string CodeName(FMesCode Code)
		{
			std::string Name;
			if (Code.Lead != 0)
			{
				NumberText::AppendHex(Name, Code.Lead, 2);
			}
			NumberText::AppendHex(Name, Code.Code, 2);
			return Name;
		}

		std::optional<FMesCode> CodeOfName(std::string_view Name)
		{
			const std::optional<uint32> Value = Name.size() == 2 || Name.size() == 4 ? NumberText::Hex(Name) : std::nullopt;
			if (!Value)
			{
				return std::nullopt;
			}
			if (Name.size() == 2)
			{
				return *Value >= FirstCode && *Value < SecondPage ? std::optional<FMesCode>(FMesCode{0, static_cast<uint8>(*Value)}) : std::nullopt;
			}
			const FMesCode Code{static_cast<uint8>(*Value >> 8), static_cast<uint8>(*Value)};
			const bool bSecondPage = Code.Lead == SecondPage && Code.Code < SecondPageGlyphs;
			return bSecondPage || Code == BoxGlyph ? std::optional<FMesCode>(Code) : std::nullopt;
		}

		bool IsNarrow(FMesCode Code)
		{
			return Code.Lead == 0 && Code.Code < FirstWideGlyph;
		}

		std::optional<int32> CellNumber(std::string_view Digits)
		{
			if (Digits.empty() || Digits.size() > 4 || !std::all_of(Digits.begin(), Digits.end(), [](char Digit) { return Digit >= '0' && Digit <= '9'; }))
			{
				return std::nullopt;
			}
			int32 Value = 0;
			for (const char Digit : Digits)
			{
				Value = Value * 10 + (Digit - '0');
			}
			return Value < MesFontSheet::Columns * MesFontSheet::Rows() ? std::optional<int32>(Value) : std::nullopt;
		}

		std::vector<std::string_view> Fields(std::string_view Line)
		{
			std::vector<std::string_view> Split;
			for (std::size_t Tab = Line.find('\t'); Tab != std::string_view::npos; Tab = Line.find('\t'))
			{
				Split.push_back(Line.substr(0, Tab));
				Line.remove_prefix(Tab + 1);
			}
			Split.push_back(Line);
			return Split;
		}

		/** A cell's glyph as Picture draws it at Scale: each pixel set where its square of the picture is at least half opaque. */
		FMesFont::FGlyphRows RowsOfCell(const FRgbaPicture& Picture, int32 Scale, int32 Cell)
		{
			const int32 Left = Cell % MesFontSheet::Columns * MesFontSheet::CellWidth * Scale;
			const int32 Top = Cell / MesFontSheet::Columns * MesFontSheet::CellHeight * Scale;
			FMesFont::FGlyphRows Rows{};
			for (int32 Y = 0; Y < MesFontSheet::CellHeight; ++Y)
			{
				for (int32 X = 0; X < MesFontSheet::CellWidth; ++X)
				{
					int32 Alpha = 0;
					for (int32 Dy = 0; Dy < Scale; ++Dy)
					{
						for (int32 Dx = 0; Dx < Scale; ++Dx)
						{
							const std::size_t Pixel = static_cast<std::size_t>((Top + Y * Scale + Dy) * Picture.Width + Left + X * Scale + Dx);
							Alpha += Picture.Pixels[Pixel * 4 + 3];
						}
					}
					if (Alpha >= HalfOpaque * Scale * Scale)
					{
						Rows[static_cast<std::size_t>(Y)] = static_cast<uint16>(Rows[static_cast<std::size_t>(Y)] | 0x8000 >> X);
					}
				}
			}
			return Rows;
		}
	}

	int32 MesFontSheet::Rows()
	{
		return (GlyphCount + Columns - 1) / Columns;
	}

	std::vector<FMesSheetGlyph> MesFontSheet::Glyphs(const FMesCharacters& Characters)
	{
		std::vector<FMesSheetGlyph> Glyphs;
		for (int32 Cell = 0; Cell < GlyphCount; ++Cell)
		{
			const FMesCode Code = CodeOfCell(Cell);
			Glyphs.push_back({Code, Cell, std::string(Characters.CharacterOf(Code))});
		}
		return Glyphs;
	}

	FRgbaPicture MesFontSheet::Picture(const FMesFont& Font)
	{
		FRgbaPicture Picture{Columns * CellWidth, Rows() * CellHeight, {}};
		Picture.Pixels.assign(static_cast<std::size_t>(Picture.Width * Picture.Height * 4), 0);
		for (int32 Cell = 0; Cell < GlyphCount; ++Cell)
		{
			const std::optional<FMesFont::FGlyphRows> Rows = Font.Rows(CodeOfCell(Cell));
			for (int32 Y = 0; Rows && Y < CellHeight; ++Y)
			{
				for (int32 X = 0; X < CellWidth; ++X)
				{
					if (((*Rows)[static_cast<std::size_t>(Y)] & 0x8000 >> X) == 0)
					{
						continue;
					}
					const std::size_t Pixel = static_cast<std::size_t>((Cell / Columns * CellHeight + Y) * Picture.Width + Cell % Columns * CellWidth + X);
					std::fill_n(Picture.Pixels.begin() + static_cast<std::ptrdiff_t>(Pixel * 4), 4, Opaque);
				}
			}
		}
		return Picture;
	}

	std::string MesFontSheet::Table(const FMesFont& Font, std::span<const FMesSheetGlyph> Glyphs, const std::unordered_map<uint16, int32>& Uses)
	{
		std::string Text = "code\tcell\twidth\tcharacter\tuses\n";
		for (const FMesSheetGlyph& Glyph : Glyphs)
		{
			const auto Used = Uses.find(static_cast<uint16>(Glyph.Code.Lead << 8 | Glyph.Code.Code));
			Text += CodeName(Glyph.Code) + "\t" + std::to_string(Glyph.Cell) + "\t" + std::to_string(Font.WidthUnits(Glyph.Code) * Font.UnitPixels()) + "\t" + Glyph.Character
				+ "\t" + std::to_string(Used != Uses.end() ? Used->second : 0) + "\n";
		}
		return Text;
	}

	TResult<std::shared_ptr<const FMesFontSheet>> FMesFontSheet::Read(std::string_view Table, const FRgbaPicture* Picture, FReport Report)
	{
		auto Sheet = std::make_shared<FMesFontSheet>();
		const auto Say = [&Report](const std::string& Problem)
		{
			if (Report)
			{
				Report(Problem);
			}
		};
		std::vector<FMesSheetGlyph> Cells;
		if (Table.empty())
		{
			Cells = MesFontSheet::Glyphs(FMesCharacters::Font());
		}
		int32 Number = 0;
		while (!Table.empty())
		{
			const std::size_t Break = Table.find('\n');
			std::string_view Line = Table.substr(0, Break);
			Table.remove_prefix(Break == std::string_view::npos ? Table.size() : Break + 1);
			++Number;
			if (!Line.empty() && Line.back() == '\r')
			{
				Line.remove_suffix(1);
			}
			if (Line.empty() || Line.starts_with("code"))
			{
				continue;
			}
			const std::vector<std::string_view> Words = Fields(Line);
			const std::optional<FMesCode> Code = CodeOfName(Words[0]);
			const std::optional<int32> Cell = Words.size() > 1 ? CellNumber(Words[1]) : std::nullopt;
			const std::string Where = "font.tsv line " + std::to_string(Number) + ": ";
			if (!Code || !Cell)
			{
				Say(Where + "a cell is a glyph's code (10 to FD, FE00 to FE3C, FFFF) and its number in the sheet: the line is left out");
				continue;
			}
			const std::string_view Width = Words.size() > 2 ? Words[2] : std::string_view();
			const uint16 Key = static_cast<uint16>(Code->Lead << 8 | Code->Code);
			if (Width == "8" || Width == "12")
			{
				const int32 Units = Width == "8" ? 2 : 3;
				if (Units != (IsNarrow(*Code) ? 2 : 3))
				{
					Sheet->OwnWidths[Key] = Units;
				}
			}
			else if (!Width.empty())
			{
				Say(Where + "a glyph is 8 or 12 pixels wide: " + CodeName(*Code) + " keeps its own width");
			}
			const std::string_view Character = Words.size() > 3 ? Words[3] : std::string_view();
			const bool bOneCharacter = Utf8SequenceLength(Character) == Character.size() && Character != "{" && Character != "}";
			if (!Character.empty() && Character != " " && !bOneCharacter)
			{
				Say(Where + "a glyph stands for one character: " + CodeName(*Code) + " keeps its own");
			}
			else if (!Character.empty() && Character != " ")
			{
				Sheet->Typed.Set(*Code, Character);
			}
			Cells.push_back({*Code, *Cell, std::string(Character)});
		}
		if (Picture == nullptr)
		{
			return std::shared_ptr<const FMesFontSheet>(std::move(Sheet));
		}
		const int32 Width = MesFontSheet::Columns * MesFontSheet::CellWidth;
		const int32 Height = MesFontSheet::Rows() * MesFontSheet::CellHeight;
		const int32 Scale = Picture->Width / Width;
		if (Scale < 1 || Scale > MesFontSheet::MostScale || Picture->Width != Scale * Width || Picture->Height != Scale * Height
			|| Picture->Pixels.size() != static_cast<std::size_t>(Picture->Width) * static_cast<std::size_t>(Picture->Height) * 4)
		{
			return MakeError("the font sheet's picture is " + std::to_string(Picture->Width) + " x " + std::to_string(Picture->Height) + " pixels: it is the export's grid of "
				+ std::to_string(Width) + " x " + std::to_string(Height) + ", or that grid 2 to " + std::to_string(MesFontSheet::MostScale) + " times as large");
		}
		Sheet->PictureScale = Scale;
		for (const FMesSheetGlyph& Cell : Cells)
		{
			const FMesFont::FGlyphRows Rows = RowsOfCell(*Picture, Scale, Cell.Cell);
			const auto Own = Sheet->OwnWidths.find(static_cast<uint16>(Cell.Code.Lead << 8 | Cell.Code.Code));
			const bool bNarrow = Own != Sheet->OwnWidths.end() ? Own->second == 2 : IsNarrow(Cell.Code);
			if (bNarrow && std::any_of(Rows.begin(), Rows.end(), [](uint16 Row) { return (Row & PastNarrowColumns) != 0; }))
			{
				Say("font.png: glyph " + CodeName(Cell.Code) + " draws past its first " + std::to_string(NarrowColumns)
					+ " columns: the game advances it 8 pixels, and the next glyph cuts its outline's right edge");
			}
			Sheet->Drawn.push_back({Cell.Code, Rows});
		}
		return std::shared_ptr<const FMesFontSheet>(std::move(Sheet));
	}

	void FMesFontSheet::DrawInto(FMesFont& Font) const
	{
		for (const auto& [Code, Rows] : Drawn)
		{
			Font.SetRows(Code, Rows);
		}
		for (const auto& [Key, Units] : OwnWidths)
		{
			Font.SetWidthUnits({static_cast<uint8>(Key >> 8), static_cast<uint8>(Key)}, Units * 4 / Font.UnitPixels());
		}
	}
}
