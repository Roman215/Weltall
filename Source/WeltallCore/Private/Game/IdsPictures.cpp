#include "Game/IdsPictures.h"

#include "Formats/Tim.h"
#include "Media/GameFileIndex.h"
#include "Text/MesGlyphs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <string_view>

namespace Weltall::IdsPictures
{
	namespace
	{
		constexpr int32 FaceDirectory = 4;
		constexpr int32 FaceFile = 99;
		constexpr int32 FaceSize = 64;
		constexpr int32 Colours = 256;
		constexpr int32 PortraitSize = 48;
		constexpr int32 SmallFaceSize = 24;
		constexpr uint16 SemiTransparent = 0x8000;
		constexpr uint16 OpaqueBlack = SemiTransparent;
		constexpr double LanczosLobes = 3.0;

		/** The square of the mirrored face a picture shows. */
		struct FCrop
		{
			int32 X = 0;
			int32 Y = 0;
			int32 Side = 0;
		};
		constexpr FCrop WholeFace{0, 0, FaceSize};
		/** His head, from the top of his hair to his chin, which sits right of the picture's middle once mirrored. */
		constexpr FCrop Head{10, 4, 40};

		struct FColour
		{
			double R = 0.0;
			double G = 0.0;
			double B = 0.0;
		};

		FColour ColourOf(uint16 Colour)
		{
			return {static_cast<double>(Colour & 31), static_cast<double>(Colour >> 5 & 31), static_cast<double>(Colour >> 10 & 31)};
		}

		double Lanczos(double X)
		{
			X = std::abs(X);
			if (X < 1e-9)
			{
				return 1.0;
			}
			if (X >= LanczosLobes)
			{
				return 0.0;
			}
			const double Angle = std::numbers::pi * X;
			return LanczosLobes * std::sin(Angle) * std::sin(Angle / LanczosLobes) / (Angle * Angle);
		}

		struct FTap
		{
			int32 Source = 0;
			double Weight = 0.0;
		};

		/** For each of Count samples over Span pixels from Origin, the face's pixels it reads (clamped to the face) and their weights. */
		std::vector<std::vector<FTap>> Taps(int32 Origin, int32 Span, int32 Count)
		{
			const double Scale = static_cast<double>(Span) / Count;
			std::vector<std::vector<FTap>> All(static_cast<std::size_t>(Count));
			for (int32 Sample = 0; Sample < Count; ++Sample)
			{
				const double Centre = Origin + (Sample + 0.5) * Scale;
				std::vector<FTap>& Read = All[static_cast<std::size_t>(Sample)];
				double Total = 0.0;
				for (int32 Source = static_cast<int32>(std::floor(Centre - LanczosLobes * Scale)); Source <= static_cast<int32>(std::ceil(Centre + LanczosLobes * Scale)); ++Source)
				{
					const double Weight = Lanczos((Source + 0.5 - Centre) / Scale);
					if (Weight != 0.0)
					{
						Read.push_back({std::clamp(Source, 0, FaceSize - 1), Weight});
						Total += Weight;
					}
				}
				for (FTap& Tap : Read)
				{
					Tap.Weight /= Total;
				}
			}
			return All;
		}

		/** Crop of the mirrored face resampled to Count × Count. */
		std::vector<FColour> Resample(const std::vector<FColour>& Face, FCrop Crop, int32 Count)
		{
			const std::vector<std::vector<FTap>> Across = Taps(Crop.X, Crop.Side, Count);
			const std::vector<std::vector<FTap>> Down = Taps(Crop.Y, Crop.Side, Count);
			std::vector<FColour> Rows(static_cast<std::size_t>(FaceSize * Count));
			for (int32 Y = 0; Y < FaceSize; ++Y)
			{
				for (int32 X = 0; X < Count; ++X)
				{
					FColour& Sum = Rows[static_cast<std::size_t>(Y * Count + X)];
					for (const FTap& Tap : Across[static_cast<std::size_t>(X)])
					{
						const FColour& From = Face[static_cast<std::size_t>(Y * FaceSize + Tap.Source)];
						Sum = {Sum.R + From.R * Tap.Weight, Sum.G + From.G * Tap.Weight, Sum.B + From.B * Tap.Weight};
					}
				}
			}
			std::vector<FColour> Samples(static_cast<std::size_t>(Count * Count));
			for (int32 Y = 0; Y < Count; ++Y)
			{
				for (int32 X = 0; X < Count; ++X)
				{
					FColour& Sum = Samples[static_cast<std::size_t>(Y * Count + X)];
					for (const FTap& Tap : Down[static_cast<std::size_t>(Y)])
					{
						const FColour& From = Rows[static_cast<std::size_t>(Tap.Source * Count + X)];
						Sum = {Sum.R + From.R * Tap.Weight, Sum.G + From.G * Tap.Weight, Sum.B + From.B * Tap.Weight};
					}
				}
			}
			return Samples;
		}

		TResult<FTim> MakePicture(std::span<const uint8> FaceBytes, int32 Size, FCrop Crop, bool bFramed)
		{
			const TResult<FTim> Face = ParseTim(FaceBytes);
			if (!Face || Face->BitsPerPixel != 8 || !Face->Clut || Face->Clut->Units.size() < Colours || Face->PixelWidth() != FaceSize
				|| Face->Image.Rect.Height != FaceSize)
			{
				return MakeError("Id's dialogue face is not a 64 x 64 picture of 256 colours");
			}
			const std::span<const uint16> Palette = std::span<const uint16>(Face->Clut->Units).first(Colours);
			std::array<bool, Colours> Used{};
			std::vector<FColour> Mirrored(static_cast<std::size_t>(FaceSize * FaceSize));
			for (int32 Y = 0; Y < FaceSize; ++Y)
			{
				for (int32 X = 0; X < FaceSize; ++X)
				{
					const uint8 Index = Face->IndexAt(FaceSize - 1 - X, Y);
					Used[Index] = true;
					Mirrored[static_cast<std::size_t>(Y * FaceSize + X)] = ColourOf(Palette[Index]);
				}
			}

			FVramImage Clut{{0, 0, Colours, 1}, std::vector<uint16>(Colours)};
			for (int32 Entry = 0; Entry < Colours; ++Entry)
			{
				const uint16 Colour = Palette[static_cast<std::size_t>(Entry)];
				Clut.Units[static_cast<std::size_t>(Entry)] = Colour == 0 ? uint16{0} : static_cast<uint16>(Colour | SemiTransparent);
			}
			const auto Unused = std::find(Used.begin(), Used.end(), false);
			if (bFramed && Unused == Used.end())
			{
				return MakeError("Id's dialogue face leaves no colour for the small face's line");
			}
			const uint8 Line = static_cast<uint8>(Unused - Used.begin());
			if (bFramed)
			{
				Clut.Units[Line] = OpaqueBlack;
			}

			const auto Nearest = [&](const FColour& Wanted)
			{
				const FColour Clamped{std::clamp(Wanted.R, 0.0, 31.0), std::clamp(Wanted.G, 0.0, 31.0), std::clamp(Wanted.B, 0.0, 31.0)};
				uint8 Best = 0;
				double BestDistance = std::numeric_limits<double>::max();
				for (int32 Entry = 0; Entry < Colours; ++Entry)
				{
					if (!Used[static_cast<std::size_t>(Entry)] || Palette[static_cast<std::size_t>(Entry)] == 0)
					{
						continue;
					}
					const FColour Colour = ColourOf(Palette[static_cast<std::size_t>(Entry)]);
					const double Distance = (Colour.R - Clamped.R) * (Colour.R - Clamped.R) + (Colour.G - Clamped.G) * (Colour.G - Clamped.G)
						+ (Colour.B - Clamped.B) * (Colour.B - Clamped.B);
					if (Distance < BestDistance)
					{
						BestDistance = Distance;
						Best = static_cast<uint8>(Entry);
					}
				}
				return Best;
			};

			const int32 Border = bFramed ? 1 : 0;
			const int32 Inside = Size - 2 * Border;
			const std::vector<FColour> Samples = Resample(Mirrored, Crop, Inside);
			FTim Picture;
			Picture.BitsPerPixel = 8;
			Picture.Image = {{0, 0, static_cast<int16>(Size / 2), static_cast<int16>(Size)}, std::vector<uint16>(static_cast<std::size_t>(Size / 2 * Size))};
			for (int32 Y = 0; Y < Size; ++Y)
			{
				for (int32 X = 0; X < Size; ++X)
				{
					const bool bInside = X >= Border && Y >= Border && X < Size - Border && Y < Size - Border;
					const uint8 Index = bInside ? Nearest(Samples[static_cast<std::size_t>((Y - Border) * Inside + X - Border)]) : Line;
					uint16& Unit = Picture.Image.Units[static_cast<std::size_t>(Y * Size / 2 + X / 2)];
					Unit = static_cast<uint16>(Unit | Index << (X % 2 * 8));
				}
			}
			Picture.Image.Picture = FVramPictureForm{1, std::make_shared<const std::vector<uint16>>(Clut.Units)};
			Picture.Clut = std::move(Clut);
			return Picture;
		}
	}

	TResult<std::vector<uint8>> ReadFace(const FCdImage& Image, const FGameFileIndex& Index)
	{
		const FIndexRecord* Record = Index.Find(FaceDirectory, FaceFile);
		if (Record == nullptr || !Record->IsFile())
		{
			return MakeError("the disc lacks Id's dialogue face");
		}
		return ReadFileBytes(Image, *Record);
	}

	TResult<FTim> Portrait(std::span<const uint8> Face)
	{
		return MakePicture(Face, PortraitSize, WholeFace, false);
	}

	TResult<FTim> SmallFace(std::span<const uint8> Face)
	{
		return MakePicture(Face, SmallFaceSize, Head, true);
	}

	std::span<const uint8> Name()
	{
		static const std::vector<uint8> Text = []
		{
			std::vector<uint8> Bytes;
			for (const char Letter : std::string_view("Id"))
			{
				const FMesCode Code = MesCodeOfCharacter(Letter).value_or(FMesCode{});
				if (Code.Lead != 0)
				{
					Bytes.push_back(Code.Lead);
				}
				Bytes.push_back(Code.Code);
			}
			Bytes.push_back(0);
			return Bytes;
		}();
		return Text;
	}
}
