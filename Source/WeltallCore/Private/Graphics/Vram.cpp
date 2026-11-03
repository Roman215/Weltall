#include "Graphics/Vram.h"

#include "Foundation/Crc32.h"

#include <algorithm>
#include <atomic>

namespace Weltall
{
	namespace
	{
		std::atomic<uint32> LineagesMade{0};

		/** Whether two spans of a wrapping axis share a unit. */
		bool SpansMeet(int32 StartA, int32 LengthA, int32 StartB, int32 LengthB, int32 Limit)
		{
			LengthA = std::min(LengthA, Limit);
			LengthB = std::min(LengthB, Limit);
			if (LengthA <= 0 || LengthB <= 0)
			{
				return false;
			}
			const int32 Offset = ((StartB - StartA) % Limit + Limit) % Limit;
			return Offset < LengthA || Offset + LengthB > Limit;
		}
	}

	FVram::FVram()
		: Units(static_cast<std::size_t>(Width) * Rows, 0), Edges(static_cast<std::size_t>(Width) * Rows, 0),
		  LineageNumber(++LineagesMade), TileRevisions(static_cast<std::size_t>(TilesAcross) * TilesDown, 0)
	{
	}

	void FVram::Upload(const FVramImage& Image)
	{
		WriteUnits(Image);
		Tracker.Upload(Image.Rect, Image.Units, Image.Kind != EVramUploadKind::Other ? Image.Kind : UploadKind);
	}

	void FVram::UploadCopy(const FVramImage& Image, const FVramRect& From)
	{
		WriteUnits(Image);
		Tracker.Copy(From, Image.Rect.X, Image.Rect.Y);
	}

	void FVram::UploadFill(const FVramImage& Image)
	{
		WriteUnits(Image);
		Tracker.Fill(Image.Rect);
	}

	void FVram::WriteUnits(const FVramImage& Image)
	{
		++Uploads;
		if (bTrackingWrites)
		{
			Writes.push_back(Image.Rect);
		}
		std::erase_if(PictureList, [&Image](const FVramPicture& Picture)
		{
			return SpansMeet(Picture.Rect.X, Picture.Rect.Width, Image.Rect.X, Image.Rect.Width, Width) &&
				SpansMeet(Picture.Rect.Y, Picture.Rect.Height, Image.Rect.Y, Image.Rect.Height, Height);
		});
		if (Image.Picture)
		{
			PictureList.push_back({Image.Rect, *Image.Picture});
		}
		const int32 RectWidth = Image.Rect.Width;
		const int32 RectHeight = Image.Rect.Height;
		for (int32 Row = 0; Row < RectHeight; ++Row)
		{
			const int32 Y = (Image.Rect.Y + Row) & (Height - 1);
			uint8 RowEdges = 0;
			if (Image.bSeparateTexture)
			{
				RowEdges = static_cast<uint8>((Row == 0 ? VramEdge::Top : 0) | (Row == RectHeight - 1 ? VramEdge::Bottom : 0));
			}
			for (int32 Column = 0; Column < RectWidth; ++Column)
			{
				const int32 X = (Image.Rect.X + Column) & (Width - 1);
				const std::size_t At = static_cast<std::size_t>(Y * Width + X);
				Units[At] = Image.Units[static_cast<std::size_t>(Row * RectWidth + Column)];
				uint8 UnitEdges = RowEdges;
				if (Image.bSeparateTexture)
				{
					UnitEdges = static_cast<uint8>(UnitEdges | (Column == 0 ? VramEdge::Left : 0) | (Column == RectWidth - 1 ? VramEdge::Right : 0));
				}
				Edges[At] = UnitEdges;
			}
		}
		// Each tile the rectangle touches once, stepping from tile edge to tile edge as the wrapped coordinates cross them.
		for (int32 Row = 0; Row < std::min(RectHeight, Height);)
		{
			const int32 Y = (Image.Rect.Y + Row) & (Height - 1);
			for (int32 Column = 0; Column < std::min(RectWidth, Width);)
			{
				const int32 X = (Image.Rect.X + Column) & (Width - 1);
				TileRevisions[static_cast<std::size_t>(Y / TileHeight * TilesAcross + X / TileWidth)] = Uploads;
				Column += TileWidth - X % TileWidth;
			}
			Row += TileHeight - Y % TileHeight;
		}
	}

	void FVram::UploadToPortRows(const FVramImage& Image)
	{
		++Uploads;
		const int32 RectWidth = Image.Rect.Width;
		const int32 RectHeight = Image.Rect.Height;
		for (int32 Row = 0; Row < RectHeight; ++Row)
		{
			const int32 Y = Image.Rect.Y + Row;
			if (Y < Height || Y >= Rows)
			{
				continue;
			}
			for (int32 Column = 0; Column < RectWidth; ++Column)
			{
				const std::size_t At = static_cast<std::size_t>(Y * Width + ((Image.Rect.X + Column) & (Width - 1)));
				Units[At] = Image.Units[static_cast<std::size_t>(Row * RectWidth + Column)];
				Edges[At] = Image.bSeparateTexture
					? static_cast<uint8>((Row == 0 ? VramEdge::Top : 0) | (Row == RectHeight - 1 ? VramEdge::Bottom : 0) | (Column == 0 ? VramEdge::Left : 0)
						| (Column == RectWidth - 1 ? VramEdge::Right : 0))
					: 0;
			}
		}
	}

	void FVram::NoteReadback(const FVramRect& Rect)
	{
		std::vector<uint16> Read;
		Read.reserve(static_cast<std::size_t>(std::max(0, Rect.Width * Rect.Height)));
		for (int32 Row = 0; Row < Rect.Height; ++Row)
		{
			for (int32 Column = 0; Column < Rect.Width; ++Column)
			{
				Read.push_back(At(Rect.X + Column, Rect.Y + Row));
			}
		}
		Tracker.Readback(Rect, Crc32OfUnits(Read));
	}

	FVramTiles FVram::TilesChangedSince(uint32 Earlier) const
	{
		FVramTiles Tiles{};
		for (int32 TileY = 0; TileY < TilesDown; ++TileY)
		{
			for (int32 TileX = 0; TileX < TilesAcross; ++TileX)
			{
				if (TileRevisions[static_cast<std::size_t>(TileY * TilesAcross + TileX)] > Earlier)
				{
					Tiles[static_cast<std::size_t>(TileY)] |= uint64{1} << TileX;
				}
			}
		}
		return Tiles;
	}

	uint32 FVram::RevisionOver(const FVramRect& Rect) const
	{
		if (Rect.Width <= 0 || Rect.Height <= 0) return 0;
		if (Rect.Y >= Height) return Uploads;
		uint32 Latest = 0;
		for (int32 Row = 0; Row < std::min<int32>(Rect.Height, Height);)
		{
			const int32 Y = (Rect.Y + Row) & (Height - 1);
			for (int32 Column = 0; Column < std::min<int32>(Rect.Width, Width);)
			{
				const int32 X = (Rect.X + Column) & (Width - 1);
				Latest = std::max(Latest, TileRevisions[static_cast<std::size_t>(Y / TileHeight * TilesAcross + X / TileWidth)]);
				Column += TileWidth - X % TileWidth;
			}
			Row += TileHeight - Y % TileHeight;
		}
		return Latest;
	}
}
