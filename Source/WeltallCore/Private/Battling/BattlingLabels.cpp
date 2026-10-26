#include "Battling/BattlingLabels.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The list stops taking letters at a hundred and one, a letter past its buffer's hundred. */
		constexpr int32 PrintLimit = FBattlingLabels::Capacity + 1;
		constexpr uint8 FlashStep = 0x14;

		/** The font's letter for a character, or none. */
		const FBattlingGlyph* FindGlyph(const FBattlingProgramTables& Tables, char Character)
		{
			int32 Index = -1;
			if (Character >= '0' && Character <= '9')
			{
				Index = Character - '0';
			}
			else if (Character >= 'A' && Character <= 'Z')
			{
				Index = Character - 'A' + 10;
			}
			else
			{
				switch (Character)
				{
				case '!': Index = 36; break;
				case ':': Index = 37; break;
				case '-': Index = 38; break;
				case '/': Index = 39; break;
				case '#': Index = 40; break;
				case ' ': Index = 41; break;
				case '\'': Index = 42; break;
				default: return nullptr;
				}
			}
			return &Tables.FontGlyphs[static_cast<std::size_t>(Index)];
		}

		/** The packets' texels are a halfword each, u in the low byte: a sum past FFh carries into v. */
		uint16 Texel(int32 U, int32 V)
		{
			return static_cast<uint16>(U | V << 8);
		}
	}

	std::string FormatBattlingText(std::string_view Format, std::initializer_list<std::string_view> Values)
	{
		std::string Text;
		auto Value = Values.begin();
		for (std::size_t At = 0; At < Format.size(); ++At)
		{
			if (Format[At] != '%' || At + 1 == Format.size())
			{
				Text += Format[At];
				continue;
			}
			// A conversion: its width's digits, then its letter.
			std::size_t Letter = At + 1;
			while (Letter + 1 < Format.size() && Format[Letter] >= '0' && Format[Letter] <= '9')
			{
				++Letter;
			}
			if (Format[Letter] == '%')
			{
				Text += '%';
			}
			else if (Value != Values.end())
			{
				Text += *Value++;
			}
			At = Letter;
		}
		return Text;
	}

	const FBattlingTexts& FBattlingLabels::Texts() const
	{
		return Tables.Texts;
	}

	void FBattlingLabels::SetFlash(bool bOn, uint32 VBlank)
	{
		Colour = bOn ? FDrawColor{static_cast<uint8>(VBlank * FlashStep), 0xFF, 0} : FDrawColor{0xFF, 0xFF, 0xFF};
	}

	void FBattlingLabels::SetFlashBlue(bool bOn, uint32 VBlank)
	{
		const uint8 Pulse = static_cast<uint8>(-static_cast<int32>(static_cast<uint8>(VBlank * FlashStep)) - 1);
		Colour = bOn ? FDrawColor{Pulse, Pulse, 0xFF} : FDrawColor{0xFF, 0xFF, 0xFF};
	}

	int32 FBattlingLabels::Width(std::string_view Text, int32 AtScale) const
	{
		int32 Total = 0;
		for (const char Character : Text)
		{
			// The original reads a missing letter's width from the bottom of memory; no label prints one.
			if (const FBattlingGlyph* Glyph = FindGlyph(Tables, Character))
			{
				Total += 2 + (Glyph->Width * AtScale >> 8);
			}
		}
		return Total;
	}

	void FBattlingLabels::PrintFrom(int32 Left, std::string_view Text, uint32 Buffer)
	{
		const int16 Place = X;
		X = static_cast<int16>(Left);
		for (const char Character : Text)
		{
			if (Count >= PrintLimit)
			{
				continue;
			}
			const FBattlingGlyph* Glyph = FindGlyph(Tables, Character);
			if (Glyph == nullptr)
			{
				continue;
			}
			if (Count == Capacity)
			{
				bOverflowed = true;
			}
			else
			{
				// The quad covers the letter's width rounded up to four texels, and one texel row more than its height.
				const int32 Wide = Glyph->Width | 3;
				FBattlingLabelGlyph& Letter = Lists[Buffer & 1][static_cast<std::size_t>(Count)];
				Letter.Left = X;
				Letter.Top = Y;
				Letter.Right = static_cast<int16>(X + (Wide * Scale >> 8));
				Letter.Bottom = static_cast<int16>(Y + Glyph->Height);
				const int32 LowerV = Glyph->V + Glyph->Height + 1;
				Letter.Texels = {Texel(Glyph->U, Glyph->V), Texel(Glyph->U + Wide, Glyph->V), Texel(Glyph->U, LowerV), Texel(Glyph->U + Wide, LowerV)};
				Letter.Colour = Colour;
			}
			++Count;
			X = static_cast<int16>(X + 2 + static_cast<int16>(static_cast<uint32>(Glyph->Width) * static_cast<uint32>(Scale) >> 8));
		}
		X = Place;
		Y = static_cast<int16>(Y + LineHeight);
	}

	void FBattlingLabels::Print(std::string_view Text, uint32 Buffer)
	{
		PrintFrom(X, Text, Buffer);
	}

	void FBattlingLabels::PrintCentered(std::string_view Text, uint32 Buffer)
	{
		PrintFrom(X - Width(Text) / 2, Text, Buffer);
	}

	void FBattlingLabels::PrintRight(std::string_view Text, uint32 Buffer)
	{
		PrintFrom(X - Width(Text), Text, Buffer);
	}

	void FBattlingLabels::PrintOffset(std::string_view Text, int32 Offset, uint32 Buffer)
	{
		PrintFrom(X - Offset, Text, Buffer);
	}

	FMenuPrimitive MakeLabelPrimitive(const FBattlingLabelGlyph& Letter, uint16 FontPage, uint16 FontClut)
	{
		FMenuPrimitive Quad;
		Quad.Code = MenuGpu::TexturedQuad;
		const std::array<int16, 4> Xs{Letter.Left, Letter.Right, Letter.Left, Letter.Right};
		const std::array<int16, 4> Ys{Letter.Top, Letter.Top, Letter.Bottom, Letter.Bottom};
		for (std::size_t Corner = 0; Corner < 4; ++Corner)
		{
			FMenuCorner& At = Quad.Corners[Corner];
			At.X = Xs[Corner];
			At.Y = Ys[Corner];
			At.U = static_cast<uint8>(Letter.Texels[Corner]);
			At.V = static_cast<uint8>(Letter.Texels[Corner] >> 8);
			At.Color = Letter.Colour;
		}
		Quad.Page = FontPage;
		Quad.Clut = FontClut;
		return Quad;
	}

	std::span<const FBattlingLabelGlyph> FBattlingLabels::Take(uint32 Buffer)
	{
		const std::size_t Taken = static_cast<std::size_t>(std::min(Count, Capacity));
		Count = 0;
		return std::span<const FBattlingLabelGlyph>(Lists[Buffer & 1]).first(Taken);
	}
}
