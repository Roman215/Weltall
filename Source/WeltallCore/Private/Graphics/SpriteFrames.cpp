#include "Graphics/SpriteFile.h"
#include "Graphics/SpriteSystem.h"
#include "SpriteRules.h"

#include <algorithm>

namespace Weltall
{
	using namespace SpriteRules;

	namespace
	{
		constexpr uint32 FlipVerticallyFlag = 0x20;
		constexpr uint32 FlipHorizontallyFlag = 0x10;
		constexpr uint32 EightBitFlag = 0x08;
		constexpr uint32 GroupMask = 0x07;
		constexpr uint32 SemiTransparentCode = 0x02000000;
		constexpr uint32 DefaultAnchorGroup = 4;
		/** Where streamed children in the kernel's VRAM ring put their frames. */
		constexpr int32 RingX = 0x300;
		constexpr int32 RingY = 0x140;
		constexpr int32 RingWidth = 0x40;
		constexpr int32 RingRows = 3;
		constexpr uint32 RingKind = 0xE;

		uint16 TexturePage(uint32 Depth, uint32 Blend, int32 X, int32 Y)
		{
			return static_cast<uint16>((Depth & 3) << 7 | (Blend & 3) << 5 | (Y & 0x100) >> 4 | (X & 0x3FF) >> 6 | (Y & 0x200) << 2);
		}

		uint16 Clut(int32 X, int32 Y)
		{
			return static_cast<uint16>(Y << 6 | (X >> 4 & 0x3F));
		}

		/** The kernel's own texture pages for kernel cells: four columns from x 768 at y 0, then four at y 256. */
		void KernelPage(uint32 Index, int32& X, int32& Y)
		{
			X = 0x300 + 0x40 * static_cast<int32>(Index & 3);
			Y = Index < 4 ? 0 : 256;
		}

		/**
		 * The empty columns at the right of a streamed cell (Pixels: its rows of texel indices, 0 drawing nothing) that pad
		 * its art to whole VRAM units: fewer than a unit's texels, in a cell of whole units whose art starts at its left
		 * edge. 0 for a cell not cut that way.
		 */
		int32 CellPadding(const FSpriteFile& File, uint32 Pixels, int32 Width, int32 Height, bool bEightBit)
		{
			const int32 PerUnit = bEightBit ? 2 : 4;
			const int32 RowBytes = bEightBit ? Width : Width / 2;
			const auto IsEmpty = [&](int32 Column)
			{
				for (int32 Row = 0; Row < Height; ++Row)
				{
					const uint8 Byte = File.Byte(Pixels + static_cast<uint32>(Row * RowBytes + (bEightBit ? Column : Column / 2)));
					if ((bEightBit ? Byte : Byte >> (Column & 1) * 4 & 0xF) != 0)
					{
						return false;
					}
				}
				return true;
			};
			if (Width == 0 || Width % PerUnit != 0 || IsEmpty(0))
			{
				return 0;
			}
			int32 Empty = 0;
			while (Empty < PerUnit && IsEmpty(Width - 1 - Empty))
			{
				++Empty;
			}
			return Empty < PerUnit ? Empty : 0;
		}
	}

	/** A frame's placement stream as the builders and the anchor reader walk it. */
	class FPlacementWalker
	{
	public:
		FPlacementWalker(FSpriteSystem& InSystem, FSprite& InSprite, const FSpriteFile& InFile, uint32 InAt)
			: System(InSystem), Sprite(InSprite), File(InFile), At(InAt)
		{
		}

		/** Runs a part's commands: anchors into the sprite's slots, sizes and flips into Part when given. */
		bool Commands(FSpritePart* Part)
		{
			if (Part != nullptr)
			{
				Part->GrowWidth = 0;
				Part->GrowHeight = 0;
				Part->Flags &= ~FlipVerticallyFlag;
			}
			for (uint8 Command = File.Byte(At); (Command & 0x80) != 0; Command = File.Byte(At))
			{
				++At;
				if ((Command & 0x40) == 0)
				{
					if (Part != nullptr && (Command & 4) != 0)
					{
						Part->Flags |= FlipVerticallyFlag;
					}
					if ((Command & 1) != 0)
					{
						const uint8 Grow = File.Byte(At++);
						if (Part != nullptr)
						{
							Part->GrowWidth = Grow;
						}
					}
					if ((Command & 2) != 0)
					{
						const uint8 Grow = File.Byte(At++);
						if (Part != nullptr)
						{
							Part->GrowHeight = Grow;
						}
					}
					continue;
				}
				Group = Command & 7u;
				if (!Sprite.bHasAnchors)
				{
					if (Sprite.ModelFile != nullptr)
					{
						System.Report(ESpriteFault::AnchorsHoldAModel, Group);
						return false;
					}
					Sprite.bHasAnchors = true;
					Sprite.Anchors = {};
				}
				FSpriteAnchor& Anchor = Sprite.Anchors[Group];
				if ((Command & 0x20) != 0)
				{
					Anchor.OffsetX = File.Byte(At);
					Anchor.OffsetY = File.Byte(At + 1);
					At += 2;
				}
				Anchor.RotationZ = (Command & 0x10) != 0 ? static_cast<int16>(File.Byte(At++) << 4) : int16{0};
			}
			if (At >= File.Size())
			{
				System.Report(ESpriteFault::PcOutsideFile, At);
				return false;
			}
			return true;
		}

		uint8 Attributes() const { return File.Byte(At); }

		/** Reads the part's position after its attributes and moves to the next part. */
		void Position(bool bWide, FSpritePart* Part)
		{
			if (Part != nullptr)
			{
				Part->X = bWide ? static_cast<int16>(File.Word(At + 1)) : static_cast<int16>(static_cast<int8>(File.Byte(At + 1)));
				Part->Y = bWide ? static_cast<int16>(File.Word(At + 3)) : static_cast<int16>(static_cast<int8>(File.Byte(At + 2)));
			}
			At += bWide ? 5 : 3;
		}

		uint32 Group = DefaultAnchorGroup;

	private:
		FSpriteSystem& System;
		FSprite& Sprite;
		const FSpriteFile& File;
		uint32 At;
	};

	void FSpriteSystem::BuildPendingFrames()
	{
		// The newest request first, as the kernel's queue runs.
		for (auto Sprite = Pending.rbegin(); Sprite != Pending.rend(); ++Sprite)
		{
			if ((*Sprite)->Frame == 0)
			{
				(*Sprite)->BuildFlags &= ~SpriteBuildFlags::PartCountMask;
			}
			else
			{
				BuildFrame(**Sprite, (*Sprite)->Frame);
			}
		}
		Pending.clear();
	}

	void FSpriteSystem::BuildFrame(FSprite& Sprite, int32 Frame)
	{
		Sprite.BuildFlags &= ~(SpriteBuildFlags::Queued | SpriteBuildFlags::KeepQueuedAnchors);
		const FSpriteResource& Resource = *Sprite.Resource;
		const FSpriteFile& File = *Resource.File;
		const FSpriteTables& Tables = Resource.Tables;
		if (Frame >= File.FrameCount(Tables) + 1)
		{
			return;
		}
		if ((Sprite.DrawFlags & SpriteDrawFlags::PaletteUploadPending) != 0)
		{
			Sprite.DrawFlags &= ~SpriteDrawFlags::PaletteUploadPending;
			const int32 Palettes = File.PalettesPerSet(Tables);
			if (Palettes != 0)
			{
				QueueUpload({Resource.ClutX, Resource.ClutY, static_cast<int16>(Palettes * 16), 1}, &File, File.PaletteSet(Tables, Sprite.PaletteSet()));
			}
		}
		if (File.IsResident(Tables))
		{
			BuildResidentFrame(Sprite, Frame);
			return;
		}
		const uint32 At = File.Frame(Tables, Frame);
		int32 AreaX = Resource.TextureX;
		int32 AreaY = Resource.TextureY;
		if (static_cast<uint32>(Sprite.Kind()) == RingKind)
		{
			const int32 Width = File.Byte(At + 4);
			if (RingWidth < RingCursorX + Width)
			{
				RingCursorX = 0;
				RingRow = RingRow + 1 > RingRows - 1 ? 0 : RingRow + 1;
			}
			AreaX = RingX + RingCursorX;
			AreaY = RingY + RingRow * 0x40;
			RingCursorX += Width;
		}
		const uint8 Head = File.Byte(At);
		const int32 Parts = Head & 0x3F;
		const bool bWide = (Head & 0x80) != 0;
		if (Parts > Sprite.PartCapacity)
		{
			Report(ESpriteFault::TooManyParts, static_cast<uint32>(Frame));
			return;
		}
		Sprite.FrameTop = static_cast<int16>(DivideByPower(int64{File.Byte(At + 3)} * Sprite.Scale, 12));
		Sprite.FrameHalfHeight = static_cast<int16>(DivideByPower(int64{File.Byte(At + 1)} * Sprite.Scale, 12));
		const uint32 SpriteBlend = static_cast<uint32>(Sprite.Blend());
		const uint32 Color = static_cast<uint32>(Sprite.Red | Sprite.Green << 8 | Sprite.Blue << 16) | static_cast<uint32>(Sprite.Code) << 24;
		FPlacementWalker Walker(*this, Sprite, File, At + 6 + 4 * static_cast<uint32>(Parts));
		int32 Built = 0;
		for (; Built < Parts; ++Built)
		{
			FSpritePart& Part = Sprite.Parts[static_cast<std::size_t>(Built)];
			if (!Walker.Commands(&Part))
			{
				return;
			}
			const uint32 Entry = At + 6 + 4 * static_cast<uint32>(Built);
			const uint16 Place = File.Word(Entry + 2);
			const int32 PlaceX = Place & 0x1F;
			const int32 PlaceY = Place >> 5 & 0x3F;
			const uint32 Cell = Tables.Frames + 4 * static_cast<uint32>(File.Word(Entry));
			const uint8 Width = File.Byte(Cell);
			const bool bEightBit = (File.Word(Cell + 2) & 1) != 0;
			Part.U = static_cast<uint8>(bEightBit ? ((AreaX & 0x3F) << 1) + PlaceX * 2 : ((AreaX & 0x3F) << 2) + PlaceX * 4);
			Part.Flags = (bEightBit ? Part.Flags | EightBitFlag : Part.Flags & ~EightBitFlag);
			Part.Flags = (Part.Flags & ~GroupMask) | Walker.Group;
			Part.V = static_cast<uint8>(PlaceY + (AreaY & 0xFF));
			Part.Width = Width;
			Part.Height = File.Byte(Cell + 1);
			const uint8 Attributes = Walker.Attributes();
			Part.Flags = (Part.Flags & ~FlipHorizontallyFlag) | (Attributes >> 2 & FlipHorizontallyFlag);
			Part.ColorCode = Color;
			uint32 Blend = Attributes >> 4 & 3;
			if (Blend == 0)
			{
				Blend = SpriteBlend;
			}
			if (Blend != 0)
			{
				--Blend;
				Part.ColorCode |= SemiTransparentCode;
			}
			Part.TexturePage = TexturePage(bEightBit ? 1 : 0, Blend, AreaX, AreaY);
			Part.Clut = Clut(Resource.ClutX + (Attributes & 0xF) * 16, Resource.ClutY);
			QueueUpload({static_cast<int16>(AreaX + PlaceX), static_cast<int16>(AreaY + PlaceY), static_cast<int16>(bEightBit ? Width >> 1 : Width >> 2), Part.Height}, &File, Cell + 4, true);
			Walker.Position(bWide, &Part);
			if (bMirroredPartsPlacedByArt && (Part.Flags & FlipHorizontallyFlag) != 0 && Part.GrowWidth == 0)
			{
				Part.X = static_cast<int16>(Part.X - CellPadding(File, Cell + 4, Width, Part.Height, bEightBit));
			}
		}
		Sprite.BuildFlags = (Sprite.BuildFlags & ~SpriteBuildFlags::PartCountMask) | static_cast<uint32>(Built) << SpriteBuildFlags::PartCountShift;
		QueueUpload({static_cast<int16>(AreaX), static_cast<int16>(AreaY), File.Byte(At + 4), File.Byte(At + 5)}, nullptr, 0);
	}

	void FSpriteSystem::BuildResidentFrame(FSprite& Sprite, int32 Frame)
	{
		const FSpriteResource& Resource = *Sprite.Resource;
		const FSpriteFile& File = *Resource.File;
		const uint32 At = File.Frame(Resource.Tables, Frame);
		const uint8 Head = File.Byte(At);
		const int32 Parts = Head & 0x3F;
		const bool bWide = (Head & 0x80) != 0;
		if (Parts > Sprite.PartCapacity)
		{
			Report(ESpriteFault::TooManyParts, static_cast<uint32>(Frame));
			return;
		}
		Sprite.FrameTop = static_cast<int16>(DivideByPower(int64{File.Byte(At + 3)} * Sprite.Scale, 12));
		Sprite.FrameHalfHeight = static_cast<int16>(DivideByPower(int64{File.Byte(At + 1)} * Sprite.Scale, 12));
		const uint32 SpriteBlend = static_cast<uint32>(Sprite.Blend());
		const uint32 Color = static_cast<uint32>(Sprite.Red | Sprite.Green << 8 | Sprite.Blue << 16) | static_cast<uint32>(Sprite.Code) << 24;
		const bool bSlots = (Sprite.AnimationFlags & SpriteAnimationFlags::HasExtra) != 0 && Sprite.Extra.TextureSlotCount != 0;
		// A slot's row stays the origin of the parts after it, kernel cells included.
		int32 BaseV = Resource.TextureY & 0xFF;
		FPlacementWalker Walker(*this, Sprite, File, At + 4 + 2 * static_cast<uint32>(Parts));
		int32 Built = 0;
		for (; Built < Parts; ++Built)
		{
			FSpritePart& Part = Sprite.Parts[static_cast<std::size_t>(Built)];
			if (!Walker.Commands(&Part))
			{
				return;
			}
			uint32 Cell = Resource.Tables.Frames + File.Word(At + 4 + 2 * static_cast<uint32>(Built));
			const uint8 CellFlags = File.Byte(Cell);
			const bool bEightBit = (CellFlags & 1) != 0;
			int32 BaseU = bEightBit ? (Resource.TextureX & 0x3F) >> 1 : (Resource.TextureX & 0x3F) >> 2;
			Part.Flags = (bEightBit ? Part.Flags | EightBitFlag : Part.Flags & ~EightBitFlag);
			const uint8 Attributes = Walker.Attributes();
			Part.ColorCode = Color;
			uint32 Blend = Attributes >> 4 & 3;
			if (Blend == 0)
			{
				Blend = SpriteBlend;
			}
			if (Blend != 0)
			{
				--Blend;
				Part.ColorCode |= SemiTransparentCode;
			}
			if ((CellFlags & 0x10) == 0)
			{
				if (bSlots)
				{
					const int32 Column = CellFlags >> 1 & 7;
					if (Column >= Sprite.Extra.TextureSlotCount)
					{
						Report(ESpriteFault::TextureSlotOutsideTable, static_cast<uint32>(Column));
						return;
					}
					// The slot's origin is taken in 4-bit units whatever the cell's depth.
					const FSpriteTextureSlot& Slot = Sprite.Extra.TextureSlots[static_cast<std::size_t>(Column)];
					BaseU = (Slot.X & 0x3F) >> 2;
					BaseV = Slot.Y & 0xFF;
					Part.TexturePage = TexturePage(bEightBit ? 1 : 0, Blend, Slot.X, Slot.Y);
				}
				else
				{
					const int32 PageX = Resource.TextureX + static_cast<int32>(CellFlags & 0xE) * 0x20;
					Part.TexturePage = TexturePage(bEightBit ? 1 : 0, Blend, PageX, Resource.TextureY);
				}
				Part.Clut = Clut(Resource.ClutX + (Attributes & 0xF) * 16, Resource.ClutY);
			}
			else
			{
				const uint16 Word = File.Word(Cell);
				int32 PageX = 0;
				int32 PageY = 0;
				KernelPage((Word & 0xE) >> 1, PageX, PageY);
				Part.TexturePage = TexturePage(Word & 1, Blend, PageX, PageY);
				Part.Clut = Clut(Word >> 1 & 0xF0, (Word >> 9 & 0xF) + 0x1CC);
				++Cell;
			}
			Part.Flags = (Part.Flags & ~GroupMask) | Walker.Group;
			Part.U = static_cast<uint8>(BaseU + File.Byte(Cell + 1));
			Part.V = static_cast<uint8>(BaseV + File.Byte(Cell + 2));
			Part.Width = File.Byte(Cell + 3);
			Part.Height = File.Byte(Cell + 4);
			Part.Flags = (Part.Flags & ~FlipHorizontallyFlag) | (Attributes >> 2 & FlipHorizontallyFlag);
			Walker.Position(bWide, &Part);
		}
		Sprite.BuildFlags = (Sprite.BuildFlags & ~SpriteBuildFlags::PartCountMask) | static_cast<uint32>(Built) << SpriteBuildFlags::PartCountShift;
	}

	void FSpriteSystem::ReadAnchors(FSprite& Sprite, int32 Frame)
	{
		const FSpriteFile& File = *Sprite.Resource->File;
		const FSpriteTables& Tables = Sprite.Resource->Tables;
		if (Frame >= File.FrameCount(Tables) + 1)
		{
			return;
		}
		const uint32 At = File.Frame(Tables, Frame);
		const uint8 Head = File.Byte(At);
		const uint32 Parts = Head & 0x3Fu;
		FPlacementWalker Walker(*this, Sprite, File, At + (File.IsResident(Tables) ? 4 + 2 * Parts : 6 + 4 * Parts));
		for (uint32 Part = 0; Part < Parts; ++Part)
		{
			if (!Walker.Commands(nullptr))
			{
				return;
			}
			Walker.Position((Head & 0x80) != 0, nullptr);
		}
	}

	void FSpriteSystem::ApplyColorBlend(FSprite& Sprite)
	{
		if (Sprite.Layout() != SpriteDrawFlags::LayoutParts)
		{
			return;
		}
		uint32 Blend = static_cast<uint32>(Sprite.Blend());
		if (Blend != 0)
		{
			--Blend;
		}
		const uint32 Color = static_cast<uint32>(Sprite.Red | Sprite.Green << 8 | Sprite.Blue << 16) | static_cast<uint32>(Sprite.Code) << 24;
		for (int32 Index = 0; Index < Sprite.PartCount(); ++Index)
		{
			FSpritePart& Part = Sprite.Parts[static_cast<std::size_t>(Index)];
			Part.ColorCode = Color;
			Part.TexturePage = static_cast<uint16>((Part.TexturePage & 0xFF9F) | Blend << 5);
		}
	}

	void FSpriteSystem::CopyParentAnchors(FSprite& Sprite)
	{
		const FSprite* Parent = Sprite.Parent;
		if (Parent == nullptr || Sprite.Layout() == 0)
		{
			return;
		}
		if (!Sprite.Resource->File->IsResident(Sprite.Resource->Tables))
		{
			Sprite.BuildFlags = (Sprite.BuildFlags & 0xFFFE1FFF) | RingKind << SpriteBuildFlags::KindShift;
		}
		if (Sprite.bHasTransform && (Parent->bHasAnchors || Parent->ModelFile != nullptr))
		{
			if (!Parent->bHasAnchors || (!Sprite.bHasAnchors && Sprite.ModelFile != nullptr))
			{
				Report(ESpriteFault::AnchorsHoldAModel, 0);
				return;
			}
			Sprite.bHasAnchors = true;
			Sprite.Anchors = Parent->Anchors;
			Sprite.PivotX = Parent->PivotX;
			Sprite.PivotY = Parent->PivotY;
		}
		SetFrame(Sprite, Sprite.Frame);
	}

	void FSpriteSystem::QueueUpload(const FVramRect& Rect, const FSpriteFile* File, uint32 Offset, bool bCell)
	{
		UploadList.push_back({Rect, File, Offset, false, bCell});
	}

	void FSpriteSystem::UploadNow(const FVramRect& Rect, const FSpriteFile& File, uint32 Offset)
	{
		UploadList.push_back({Rect, &File, Offset, true});
	}

	void FSpriteSystem::FlushUploads(FVram& Vram)
	{
		// A place in the port's rows (only the port's own sprites have one) is drawn from there, as its attribute reads.
		struct FIntoVram final : IVramUploads
		{
			explicit FIntoVram(FVram& InVram) : Vram(InVram) {}
			void Upload(const FVramImage& Image) override
			{
				if (Image.Rect.Y >= FVram::Height)
				{
					Vram.UploadToPortRows(Image);
				}
				else
				{
					Vram.Upload(Image);
				}
			}
			FVram& Vram;
		};
		FIntoVram Into(Vram);
		FlushUploads(Into);
	}

	void FSpriteSystem::FlushUploads(IVramUploads& Into)
	{
		const auto Make = [this, &Into](const FSpriteUpload& Upload) {
			UploadScratch.Rect = Upload.Rect;
			UploadScratch.bSeparateTexture = Upload.bCell;
			UploadScratch.Units.assign(static_cast<std::size_t>(std::max(0, Upload.Rect.Width * Upload.Rect.Height)), 0);
			if (Upload.File != nullptr)
			{
				for (std::size_t Unit = 0; Unit < UploadScratch.Units.size(); ++Unit)
				{
					UploadScratch.Units[Unit] = Upload.File->Word(Upload.Offset + 2 * static_cast<uint32>(Unit));
				}
			}
			Into.Upload(UploadScratch);
		};
		for (const FSpriteUpload& Upload : UploadList)
		{
			if (Upload.bNow)
			{
				Make(Upload);
			}
		}
		for (auto Upload = UploadList.rbegin(); Upload != UploadList.rend(); ++Upload)
		{
			if (!Upload->bNow)
			{
				Make(*Upload);
			}
		}
		UploadList.clear();
	}
}
