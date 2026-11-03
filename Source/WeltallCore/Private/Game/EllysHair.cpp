#include "Game/EllysHair.h"

#include "Formats/Tim.h"
#include "Foundation/LittleEndian.h"
#include "Graphics/SpriteFile.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace Weltall::EllysHair
{
	namespace
	{
		constexpr uint16 SemiTransparent = 0x8000;
		/** Her orange: hue 7.2 to 43.2 degrees, strongly coloured; the edge of her hair a little paler; skin paler still and light. */
		constexpr double OrangeFrom = 0.02;
		constexpr double OrangeTo = 0.12;
		constexpr double Strong = 0.55;
		constexpr double EdgeStrength = 0.35;
		constexpr double Darkest = 0.05;
		constexpr double SkinLightness = 0.55;
		/** Skin itself is paler than the pale orange where her hair meets it or the panel behind her. */
		constexpr double SkinStrength = 0.45;
		/** Her trousers' darkest shade in her battle sprite, (104, 48, 48): red-brown, below the orange's hues and duller. */
		constexpr double ShadowTo = 0.02;
		constexpr double ShadowStrength = 0.45;
		constexpr double ShadowStrongest = 0.75;
		constexpr double ShadowDarkness = 0.5;
		constexpr double PurpleHue = 255.0 / 360.0;
		constexpr double PurpleScale = 0.62;
		/** Below the cheeks (from 26 of 48 rows down) skin's shading keeps its colour: a light orange, the hair's strands there darker. */
		constexpr int32 CheeksRow = 26;
		constexpr double ShadingLightness = 0.65;
		constexpr int32 PortraitRows = 48;

		struct FHsv
		{
			double H = 0.0;
			double S = 0.0;
			double V = 0.0;
		};

		FHsv HsvOf(uint16 Colour)
		{
			const double R = (Colour & 31) / 31.0;
			const double G = (Colour >> 5 & 31) / 31.0;
			const double B = (Colour >> 10 & 31) / 31.0;
			const double Max = std::max({R, G, B});
			const double Min = std::min({R, G, B});
			FHsv Hsv{0.0, Max > 0.0 ? (Max - Min) / Max : 0.0, Max};
			const double Range = Max - Min;
			if (Range > 0.0)
			{
				double H = Max == R ? (G - B) / Range : Max == G ? 2.0 + (B - R) / Range : 4.0 + (R - G) / Range;
				H /= 6.0;
				Hsv.H = H < 0.0 ? H + 1.0 : H;
			}
			return Hsv;
		}

		uint16 ColourOf(const FHsv& Hsv)
		{
			const double Sector = Hsv.H * 6.0;
			const int32 Index = static_cast<int32>(std::floor(Sector)) % 6;
			const double F = Sector - std::floor(Sector);
			const double P = Hsv.V * (1.0 - Hsv.S);
			const double Q = Hsv.V * (1.0 - Hsv.S * F);
			const double T = Hsv.V * (1.0 - Hsv.S * (1.0 - F));
			const std::array<std::array<double, 3>, 6> Rgb{{{Hsv.V, T, P}, {Q, Hsv.V, P}, {P, Hsv.V, T}, {P, Q, Hsv.V}, {T, P, Hsv.V}, {Hsv.V, P, Q}}};
			const auto Channel = [](double Value) { return static_cast<uint16>(std::lround(std::clamp(Value, 0.0, 1.0) * 31.0)); };
			const std::array<double, 3>& Picked = Rgb[static_cast<std::size_t>(Index)];
			return static_cast<uint16>(Channel(Picked[0]) | Channel(Picked[1]) << 5 | Channel(Picked[2]) << 10);
		}

		bool IsOrange(uint16 Colour, double Strength)
		{
			const FHsv Hsv = HsvOf(Colour);
			return Hsv.H >= OrangeFrom && Hsv.H <= OrangeTo && Hsv.S >= Strength && Hsv.V > Darkest;
		}

		/** Her orange, or the dull dark red-brown that shades it (her ribbon's reds are vivid). */
		bool IsHairShade(uint16 Colour)
		{
			const FHsv Hsv = HsvOf(Colour);
			return IsOrange(Colour, Strong)
				|| (Hsv.H < ShadowTo && Hsv.S >= ShadowStrength && Hsv.S < ShadowStrongest && Hsv.V > Darkest && Hsv.V < ShadowDarkness);
		}

		bool IsSkin(uint16 Colour)
		{
			const FHsv Hsv = HsvOf(Colour);
			return Hsv.S < SkinStrength && Hsv.V > SkinLightness;
		}

		/** Pale and light: skin, its shading, or the pale edge of her hair. */
		bool IsPale(uint16 Colour)
		{
			const FHsv Hsv = HsvOf(Colour);
			return Hsv.S < Strong && Hsv.V > SkinLightness;
		}

		int32 Distance(uint16 A, uint16 B)
		{
			int32 Sum = 0;
			for (int32 Shift : {0, 5, 10})
			{
				const int32 Difference = (A >> Shift & 31) - (B >> Shift & 31);
				Sum += Difference * Difference;
			}
			return Sum;
		}
	}

	uint16 Purple(uint16 Colour)
	{
		if ((Colour & 0x7FFF) == 0 || !IsHairShade(Colour))
		{
			return Colour;
		}
		const FHsv Hsv = HsvOf(Colour);
		return static_cast<uint16>(ColourOf({PurpleHue, Hsv.S * PurpleScale, Hsv.V * PurpleScale}) | (Colour & SemiTransparent));
	}

	namespace
	{
		// The header: a section count, then the animation table's, the frame table's, the palettes' and the next one's offsets.
		constexpr uint32 PalettesAt = 12;
		constexpr uint32 NextAt = 16;

		/** Every colour of the palette block, from the palettes' offset to the next section's. */
		template <typename FRead, typename FWrite>
		void RecolourPalettes(uint32 Size, FRead Read16, FWrite Write16)
		{
			if (Size < NextAt + 4)
			{
				return;
			}
			const uint32 Palettes = static_cast<uint32>(Read16(PalettesAt) | Read16(PalettesAt + 2) << 16);
			const uint32 End = std::min<uint32>(static_cast<uint32>(Read16(NextAt) | Read16(NextAt + 2) << 16), Size);
			if (Palettes + 4 > End || Read16(Palettes) == 0)
			{
				return;
			}
			for (uint32 At = Palettes + 4; At + 2 <= End; At += 2)
			{
				Write16(At, Purple(static_cast<uint16>(Read16(At))));
			}
		}
	}

	void RecolourSpriteFile(std::span<uint8> File)
	{
		RecolourPalettes(static_cast<uint32>(File.size()), [&](uint32 At) { return static_cast<uint32>(LittleEndian::ReadU16(File, At)); },
			[&](uint32 At, uint16 Value) { LittleEndian::WriteU16(File, At, Value); });
	}

	void RecolourSpriteFile(FSpriteFile& File)
	{
		RecolourPalettes(File.Size(), [&](uint32 At) { return static_cast<uint32>(File.Word(At)); },
			[&](uint32 At, uint16 Value) {
				File.SetByte(At, static_cast<uint8>(Value));
				File.SetByte(At + 1, static_cast<uint8>(Value >> 8));
			});
	}

	void RecolourPicture(std::span<uint8> Pixels, int32 Width, int32 Height, int32 Depth, std::span<uint16> Clut)
	{
		const int32 PerByte = Depth == 4 ? 2 : 1;
		const int32 Stride = Width / PerByte;
		if (Width <= 0 || Height <= 0 || static_cast<int64>(Stride) * Height > static_cast<int64>(Pixels.size()))
		{
			return;
		}
		const auto At = [&](int32 X, int32 Y) {
			const uint8 Byte = Pixels[static_cast<std::size_t>(Y * Stride + X / PerByte)];
			return static_cast<std::size_t>(Depth == 4 ? Byte >> (X & 1) * 4 & 0xF : Byte);
		};
		const auto Colour = [&](int32 X, int32 Y) { return At(X, Y) < Clut.size() ? Clut[At(X, Y)] : uint16{0}; };
		const std::size_t Count = static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height);
		const std::size_t Row = static_cast<std::size_t>(Width);

		// Skin, and the pale shading that hangs from it within two rows; the pale edge of her hair hangs from hair instead.
		std::vector<uint8> SkinLike(Count, 0);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const std::size_t Here = static_cast<std::size_t>(Y * Width + X);
				const uint16 Own = Colour(X, Y);
				SkinLike[Here] = IsSkin(Own) || (IsPale(Own) && ((Y >= 1 && SkinLike[Here - Row]) || (Y >= 2 && SkinLike[Here - 2 * Row]))) ? 1 : 0;
			}
		}
		const auto Kept = [&](int32 X, int32 Y) {
			const std::size_t Here = static_cast<std::size_t>(Y * Width + X);
			return Y * PortraitRows >= CheeksRow * Height && HsvOf(Colour(X, Y)).V >= ShadingLightness
				&& ((Y >= 1 && SkinLike[Here - Row]) || (Y >= 2 && SkinLike[Here - 2 * Row]));
		};

		// What each pixel wants: its hair purple, with the paler orange beside it; the rest as it is.
		std::vector<uint8> Hair(Count, 0);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				Hair[static_cast<std::size_t>(Y * Width + X)] = IsHairShade(Colour(X, Y)) && !Kept(X, Y) ? 1 : 0;
			}
		}
		std::vector<uint16> Wanted(Count, 0);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const std::size_t Here = static_cast<std::size_t>(Y * Width + X);
				const uint16 Own = Colour(X, Y);
				bool bPurple = Hair[Here] != 0;
				if (!bPurple && IsOrange(Own, EdgeStrength) && !Kept(X, Y))
				{
					bPurple = (X > 0 && Hair[Here - 1]) || (X + 1 < Width && Hair[Here + 1]) || (Y > 0 && Hair[Here - static_cast<std::size_t>(Width)])
						|| (Y + 1 < Height && Hair[Here + static_cast<std::size_t>(Width)]);
				}
				if (bPurple)
				{
					const FHsv Hsv = HsvOf(Own);
					Wanted[Here] = static_cast<uint16>(ColourOf({PurpleHue, Hsv.S * PurpleScale, Hsv.V * PurpleScale}) | (Own & SemiTransparent));
				}
				else
				{
					Wanted[Here] = Own;
				}
			}
		}

		// Each entry takes what most of its pixels want; one wanting another takes the nearest entry.
		std::vector<uint16> Original(Clut.begin(), Clut.end());
		std::vector<int32> ForPurple(Clut.size(), 0);
		std::vector<int32> ForOwn(Clut.size(), 0);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const std::size_t Entry = At(X, Y);
				if (Entry < Clut.size())
				{
					++(Wanted[static_cast<std::size_t>(Y * Width + X)] != Original[Entry] ? ForPurple : ForOwn)[Entry];
				}
			}
		}
		for (std::size_t Entry = 0; Entry < Clut.size(); ++Entry)
		{
			if (ForPurple[Entry] > ForOwn[Entry])
			{
				Clut[Entry] = static_cast<uint16>(ColourOf({PurpleHue, HsvOf(Original[Entry]).S * PurpleScale, HsvOf(Original[Entry]).V * PurpleScale})
					| (Original[Entry] & SemiTransparent));
			}
		}
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const std::size_t Entry = At(X, Y);
				const uint16 Want = Wanted[static_cast<std::size_t>(Y * Width + X)];
				if (Entry >= Clut.size() || Clut[Entry] == Want || (Want & 0x7FFF) == 0)
				{
					continue;
				}
				std::size_t Nearest = Entry;
				int32 Best = std::numeric_limits<int32>::max();
				for (std::size_t Other = 1; Other < Clut.size(); ++Other)
				{
					if ((Clut[Other] & 0x7FFF) != 0 && Distance(Clut[Other], Want) < Best)
					{
						Best = Distance(Clut[Other], Want);
						Nearest = Other;
					}
				}
				uint8& Byte = Pixels[static_cast<std::size_t>(Y * Stride + X / PerByte)];
				Byte = Depth == 4 ? static_cast<uint8>((Byte & (0xF0 >> (X & 1) * 4)) | (Nearest & 0xF) << (X & 1) * 4) : static_cast<uint8>(Nearest);
			}
		}
	}

	void RecolourTim(FTim& Tim)
	{
		if ((Tim.BitsPerPixel != 4 && Tim.BitsPerPixel != 8) || !Tim.Clut)
		{
			return;
		}
		std::vector<uint8> Bytes(Tim.Image.Units.size() * 2);
		for (std::size_t Unit = 0; Unit < Tim.Image.Units.size(); ++Unit)
		{
			Bytes[2 * Unit] = static_cast<uint8>(Tim.Image.Units[Unit]);
			Bytes[2 * Unit + 1] = static_cast<uint8>(Tim.Image.Units[Unit] >> 8);
		}
		const std::size_t Colours = std::min<std::size_t>(Tim.Clut->Units.size(), std::size_t{1} << Tim.BitsPerPixel);
		RecolourPicture(Bytes, Tim.PixelWidth(), Tim.Image.Rect.Height, Tim.BitsPerPixel, std::span<uint16>(Tim.Clut->Units).first(Colours));
		for (std::size_t Unit = 0; Unit < Tim.Image.Units.size(); ++Unit)
		{
			Tim.Image.Units[Unit] = static_cast<uint16>(Bytes[2 * Unit] | Bytes[2 * Unit + 1] << 8);
		}
	}
}
