#include "Graphics/VramUploads.h"

#include "Foundation/Crc32.h"

#include <algorithm>
#include <array>
#include <bit>
#include <iterator>
#include <optional>

namespace Weltall
{
	namespace
	{
		constexpr int32 VramWidth = 1024;
		constexpr int32 VramHeight = 512;

		/** The rectangle as the GPU takes it: its corner wrapped into VRAM. */
		FVramRect Wrapped(const FVramRect& Rect)
		{
			return {static_cast<int16>(Rect.X & (VramWidth - 1)), static_cast<int16>(Rect.Y & (VramHeight - 1)), Rect.Width, Rect.Height};
		}

		FVramRect Between(int32 Left, int32 Top, int32 Right, int32 Bottom)
		{
			return {static_cast<int16>(Left), static_cast<int16>(Top), static_cast<int16>(Right - Left), static_cast<int16>(Bottom - Top)};
		}

		std::optional<FVramRect> Intersection(const FVramRect& A, const FVramRect& B)
		{
			const int32 Left = std::max<int32>(A.X, B.X);
			const int32 Top = std::max<int32>(A.Y, B.Y);
			const int32 Right = std::min(A.X + A.Width, B.X + B.Width);
			const int32 Bottom = std::min(A.Y + A.Height, B.Y + B.Height);
			if (Right <= Left || Bottom <= Top)
			{
				return std::nullopt;
			}
			return Between(Left, Top, Right, Bottom);
		}

		/** The piece of Part that lies at Piece, keeping its place in its upload. */
		FVramUploadPart PieceOf(const FVramUploadPart& Part, const FVramRect& Piece)
		{
			return {Part.Upload, Piece, static_cast<int16>(Part.FromX + Piece.X - Part.Rect.X), static_cast<int16>(Part.FromY + Piece.Y - Part.Rect.Y)};
		}

		/** What is left of Original around Removed: the bands above and below it across Original's width, then those to its left and right. */
		void AddLeft(const FVramUploadPart& Original, const FVramRect& Removed, std::vector<FVramUploadPart>& Into)
		{
			const FVramRect& Whole = Original.Rect;
			const int32 Right = Whole.X + Whole.Width;
			const int32 Bottom = Whole.Y + Whole.Height;
			const int32 CutRight = Removed.X + Removed.Width;
			const int32 CutBottom = Removed.Y + Removed.Height;
			if (Removed.Y > Whole.Y) Into.push_back(PieceOf(Original, Between(Whole.X, Whole.Y, Right, Removed.Y)));
			if (CutBottom < Bottom) Into.push_back(PieceOf(Original, Between(Whole.X, CutBottom, Right, Bottom)));
			if (Removed.X > Whole.X) Into.push_back(PieceOf(Original, Between(Whole.X, Removed.Y, Removed.X, CutBottom)));
			if (CutRight < Right) Into.push_back(PieceOf(Original, Between(CutRight, Removed.Y, Right, CutBottom)));
		}

		FVramUpload Describe(const FVramRect& Rect, std::span<const uint16> Units)
		{
			FVramUpload Upload;
			Upload.Crc = Crc32OfUnits(Units);
			Upload.Width = Rect.Width;
			Upload.Height = Rect.Height;
			uint32 Nibbles = 0;
			std::array<uint64, 4> Bytes{};
			for (const uint16 Unit : Units)
			{
				Nibbles |= 1u << (Unit & 0xF) | 1u << (Unit >> 4 & 0xF) | 1u << (Unit >> 8 & 0xF) | 1u << (Unit >> 12);
				Bytes[(Unit & 0xFF) >> 6] |= uint64{1} << (Unit & 0x3F);
				Bytes[Unit >> 14] |= uint64{1} << (Unit >> 8 & 0x3F);
			}
			if (Nibbles == 0)
			{
				return Upload;
			}
			Upload.Least4 = static_cast<uint8>(std::countr_zero(Nibbles));
			Upload.Greatest4 = static_cast<uint8>(31 - std::countl_zero(Nibbles));
			const auto Least = std::find_if(Bytes.begin(), Bytes.end(), [](uint64 Word) { return Word != 0; });
			const auto Greatest = std::find_if(Bytes.rbegin(), Bytes.rend(), [](uint64 Word) { return Word != 0; });
			Upload.Least8 = static_cast<uint8>((Least - Bytes.begin()) * 64 + std::countr_zero(*Least));
			Upload.Greatest8 = static_cast<uint8>((Bytes.rend() - Greatest - 1) * 64 + 63 - std::countl_zero(*Greatest));
			return Upload;
		}
	}

	void FVramUploadTracker::Upload(const FVramRect& Given, std::span<const uint16> Units, EVramUploadKind Kind)
	{
		if (Given.Width <= 0 || Given.Height <= 0)
		{
			return;
		}
		if (Given.Width == VramWidth && Given.Height == VramHeight)
		{
			PartList.clear();
			return;
		}
		const FVramRect Rect = Wrapped(Given);
		FVramUpload Described = Describe(Rect, Units);
		Described.Kind = Kind;
		auto Made = std::make_shared<const FVramUpload>(Described);
		const auto Restored = std::find_if(Readbacks.begin(), Readbacks.end(), [&](const FReadback& Readback)
		{
			return Readback.Rect == Rect && Readback.Crc == Made->Crc;
		});
		if (Restored != Readbacks.end())
		{
			for (const FVramUploadPart& Part : Restored->Parts)
			{
				Place(Part);
			}
			return;
		}
		Place({std::move(Made), Rect, 0, 0});
	}

	void FVramUploadTracker::Copy(const FVramRect& Given, int32 X, int32 Y)
	{
		if (Given.Width <= 0 || Given.Height <= 0)
		{
			return;
		}
		const FVramRect From = Wrapped(Given);
		const FVramRect To = Wrapped({static_cast<int16>(X), static_cast<int16>(Y), Given.Width, Given.Height});
		std::vector<FVramUploadPart> Carried;
		for (const FVramUploadPart& Part : PartList)
		{
			if (const std::optional<FVramRect> Shared = Intersection(Part.Rect, From))
			{
				FVramUploadPart Piece = PieceOf(Part, *Shared);
				Piece.Rect.X = static_cast<int16>(Piece.Rect.X + To.X - From.X);
				Piece.Rect.Y = static_cast<int16>(Piece.Rect.Y + To.Y - From.Y);
				Carried.push_back(std::move(Piece));
			}
		}
		Clear(To);
		for (FVramUploadPart& Piece : Carried)
		{
			Place(std::move(Piece));
		}
	}

	void FVramUploadTracker::Fill(const FVramRect& Rect)
	{
		if (Rect.Width > 0 && Rect.Height > 0)
		{
			Clear(Wrapped(Rect));
		}
	}

	void FVramUploadTracker::Readback(const FVramRect& Given, uint32 Crc)
	{
		if (Given.Width <= 0 || Given.Height <= 0)
		{
			return;
		}
		const FVramRect Rect = Wrapped(Given);
		std::erase_if(Readbacks, [&Rect](const FReadback& Earlier) { return Intersection(Earlier.Rect, Rect).has_value(); });
		FReadback Kept{Rect, Crc, {}};
		for (const FVramUploadPart& Part : PartList)
		{
			if (const std::optional<FVramRect> Shared = Intersection(Part.Rect, Rect))
			{
				Kept.Parts.push_back(PieceOf(Part, *Shared));
			}
		}
		if (Readbacks.size() == KeptReadbacks)
		{
			Readbacks.erase(Readbacks.begin());
		}
		Readbacks.push_back(std::move(Kept));
	}

	void FVramUploadTracker::Clear(const FVramRect& Rect)
	{
		std::vector<FVramUploadPart> Left;
		std::erase_if(PartList, [&Rect, &Left](const FVramUploadPart& Part)
		{
			const std::optional<FVramRect> Removed = Intersection(Part.Rect, Rect);
			if (Removed)
			{
				AddLeft(Part, *Removed, Left);
			}
			return Removed.has_value();
		});
		PartList.insert(PartList.end(), std::make_move_iterator(Left.begin()), std::make_move_iterator(Left.end()));
	}

	void FVramUploadTracker::Place(FVramUploadPart Part)
	{
		Clear(Part.Rect);
		PartList.push_back(std::move(Part));
	}
}
