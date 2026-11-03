#pragma once

#include "Foundation/Types.h"
#include "Graphics/VramRect.h"
#include "Graphics/VramUploads.h"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace Weltall
{
	/**
	 * What an uploaded image is when it is one picture, as a TIM describes it: the depth its texels are read at (an
	 * ETextureDepth) and, for an indexed one, the colours it comes with.
	 */
	struct FVramPictureForm
	{
		uint8 Depth = 0;
		std::shared_ptr<const std::vector<uint16>> Palette{};
	};

	/** A picture VRAM holds: where its uploaded image lies, and its form. */
	struct FVramPicture
	{
		FVramRect Rect;
		FVramPictureForm Form;
	};

	/**
	 * 16-bit units for a rectangle of VRAM, row by row: texture pixels, a CLUT, a font sheet. Units
	 * always holds Width x Height values.
	 */
	struct FVramImage
	{
		FVramRect Rect;
		std::vector<uint16> Units;
		/**
		 * Its texels are a texture apart from whatever lies around them (a sprite's cell, which a frame packs beside
		 * others): VRAM marks its edges, which the texture upscaler reads nothing beyond.
		 */
		bool bSeparateTexture = false;
		/** Set when the image is one picture (a TIM's): VRAM keeps it among its pictures until something is uploaded over it. */
		std::optional<FVramPictureForm> Picture{};
		/** What it holds; Other takes the kind the VRAM uploads under (FVram::FKindScope). */
		EVramUploadKind Kind = EVramUploadKind::Other;
	};

	/** Which edges of a separate texture (FVramImage::bSeparateTexture) a unit of VRAM lies on. */
	namespace VramEdge
	{
		inline constexpr uint8 Left = 1;
		inline constexpr uint8 Right = 2;
		inline constexpr uint8 Top = 4;
		inline constexpr uint8 Bottom = 8;
	}

	/** Tiles of VRAM (FVram::TileWidth x FVram::TileHeight units each): bit x of entry y is tile (x, y). */
	using FVramTiles = std::array<uint64, 64>;

	/** Where a game's uploads into VRAM go: a VRAM state, or whatever passes them on to one. */
	class IVramUploads
	{
	public:
		virtual ~IVramUploads() = default;

		virtual void Upload(const FVramImage& Image) = 0;
	};

	/**
	 * The PlayStation GPU's 1024 x 512 frame buffer of 16-bit units. Textures and their CLUTs are
	 * read from it, so a scene's textures are rebuilt by replaying the game's uploads.
	 *
	 * Below the console's rows lie the port's own (PortRows), where the port keeps textures the game
	 * never had: as a GPU with 2 MB of VRAM has its upper half, reached by the high bit of a page's or a
	 * CLUT's y (FTexturePage::FromPortAttribute). Nothing the game uploads reaches them, and the
	 * console's addresses never wrap into them.
	 */
	class FVram
	{
	public:
		static constexpr int32 Width = 1024;
		/** The console's rows: its addresses wrap within them. */
		static constexpr int32 Height = 512;
		/** The port's own rows, from Height on, and every row the VRAM keeps. */
		static constexpr int32 PortRows = 512;
		static constexpr int32 Rows = Height + PortRows;
		/** The tiles TilesChangedSince tells apart. */
		static constexpr int32 TileWidth = 16;
		static constexpr int32 TileHeight = 8;
		static constexpr int32 TilesAcross = Width / TileWidth;
		static constexpr int32 TilesDown = Height / TileHeight;

		WELTALLCORE_API FVram();

		/** The kind of the uploads that name none of their own, until a scope changes it for a while. */
		void SetUploadKind(EVramUploadKind Kind) { UploadKind = Kind; }

		/** While it lives, the VRAM's uploads that name no kind of their own are of Kind. */
		class FKindScope
		{
		public:
			FKindScope(FVram& InVram, EVramUploadKind Kind) : Vram(InVram), Was(InVram.UploadKind) { Vram.UploadKind = Kind; }
			~FKindScope() { Vram.UploadKind = Was; }
			FKindScope(const FKindScope&) = delete;
			FKindScope& operator=(const FKindScope&) = delete;

		private:
			FVram& Vram;
			EVramUploadKind Was;
		};

		/** Copies an image in; like the GPU's transfers, a rectangle that crosses an edge wraps around. */
		WELTALLCORE_API void Upload(const FVramImage& Image);

		/** As Upload, for the console's MoveImage: Image holds the units read from From, and the uploads under From come with them. */
		WELTALLCORE_API void UploadCopy(const FVramImage& Image, const FVramRect& From);

		/** As Upload, for the console's fills (ClearImage): the units are no upload's. */
		WELTALLCORE_API void UploadFill(const FVramImage& Image);

		/**
		 * Copies an image into the port's rows, its rectangle's y counted from the top of the console's (so from
		 * Height): what lies above or below them is left out, and only its columns wrap.
		 */
		WELTALLCORE_API void UploadToPortRows(const FVramImage& Image);

		uint16 At(int32 X, int32 Y) const
		{
			return Units[static_cast<std::size_t>((Y & (Height - 1)) * Width + (X & (Width - 1)))];
		}

		/** A unit as a texture's or CLUT's place reaches it: in the console's rows as At, in the port's as they are, 0 past them. */
		uint16 TexelAt(int32 X, int32 Y) const
		{
			if (Y < Height)
			{
				return At(X, Y);
			}
			return Y < Rows ? Units[static_cast<std::size_t>(Y * Width + (X & (Width - 1)))] : 0;
		}

		/** The VramEdge flags of the separate texture the unit belongs to, 0 for a unit of none. */
		uint8 EdgesAt(int32 X, int32 Y) const
		{
			return Edges[static_cast<std::size_t>((Y & (Height - 1)) * Width + (X & (Width - 1)))];
		}

		/** How many uploads the VRAM has taken: whatever was decoded from it at another count is out of date. */
		uint32 Revision() const { return Uploads; }

		/**
		 * Which VRAM this is: the same for a copy of it, another for each VRAM made. Revisions count one VRAM's
		 * uploads, so they compare only within one lineage.
		 */
		uint32 Lineage() const { return LineageNumber; }

		/** Every tile. */
		static constexpr FVramTiles EveryTile()
		{
			FVramTiles Tiles{};
			Tiles.fill(~uint64{0});
			return Tiles;
		}

		/** The tiles of the console's rows an upload wrote into after the VRAM's revision was Earlier. */
		WELTALLCORE_API FVramTiles TilesChangedSince(uint32 Earlier) const;
		/** Latest upload touching the tiles of a console rectangle, with texture-fetch wrapping; port rows use the whole revision. */
		WELTALLCORE_API uint32 RevisionOver(const FVramRect& Rect) const;

		/** Every unit, row by row from the top left: Width x Rows of them, the port's rows after the console's. */
		std::span<const uint16> AllUnits() const { return Units; }

		/** The console's rows' units, as its VRAM holds them: Width x Height. */
		std::span<const uint16> ConsoleUnits() const { return std::span<const uint16>(Units).first(static_cast<std::size_t>(Width) * Height); }

		/** Every unit's VramEdge flags, as AllUnits lays them out. */
		std::span<const uint8> AllEdges() const { return Edges; }

		/** From now on, keeps the rectangle of every upload for TakeWrites. */
		void TrackWrites() { bTrackingWrites = true; }

		/** The rectangles uploads wrote since the last call, in order, as they were given. */
		std::vector<FVramRect> TakeWrites() { return std::exchange(Writes, {}); }

		/** The pictures uploaded into the console's rows that nothing has been uploaded over since. */
		std::span<const FVramPicture> Pictures() const { return PictureList; }

		/** Which upload each part of the console's rows holds, as Beetle PSX HW's texture tracker keeps it (FVramUploadTracker). */
		std::span<const FVramUploadPart> UploadParts() const { return Tracker.Parts(); }

		/** StoreImage: the uploads under the rectangle are kept with its units, to come back if the same units are uploaded to it again. */
		WELTALLCORE_API void NoteReadback(const FVramRect& Rect);

	private:
		void WriteUnits(const FVramImage& Image);

		std::vector<uint16> Units;
		std::vector<uint8> Edges;
		uint32 Uploads = 0;
		uint32 LineageNumber = 0;
		/** Per tile, row by row, the revision of the latest upload that wrote into it. */
		std::vector<uint32> TileRevisions;
		bool bTrackingWrites = false;
		std::vector<FVramRect> Writes;
		std::vector<FVramPicture> PictureList;
		FVramUploadTracker Tracker;
		EVramUploadKind UploadKind = EVramUploadKind::Other;
	};

	static_assert(std::tuple_size_v<FVramTiles> == FVram::TilesDown && FVram::TilesAcross == 64, "a tile row's bits fill an entry");
}
