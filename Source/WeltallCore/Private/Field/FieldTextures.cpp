#include "Field/FieldTextures.h"

#include "Field/FieldMap.h"
#include "Formats/Tim.h"
#include "Foundation/LittleEndian.h"

#include <iterator>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		constexpr int32 SpriteImageSpacing = 64;
		constexpr FVramRect PaletteRows{0, 480, 256, 32};
		/** 3 in each 5-bit component. */
		constexpr uint16 PaletteFloor = 0x0C63;

		bool Has(std::span<const uint8> Bytes, std::size_t Offset, std::size_t Count)
		{
			return Offset <= Bytes.size() && Count <= Bytes.size() - Offset;
		}

		/** A u32 count, then that many offsets from the list's start; the field reads the count as signed. */
		TResult<std::vector<std::span<const uint8>>> ReadEntries(std::span<const uint8> List)
		{
			if (!Has(List, 0, 4))
			{
				return MakeError("list has no count");
			}
			const int32 Count = static_cast<int32>(ReadU32(List, 0));
			std::vector<std::span<const uint8>> Entries;
			if (Count <= 0)
			{
				return Entries;
			}
			if (!Has(List, 4, static_cast<std::size_t>(Count) * 4))
			{
				return MakeError("list's offsets run past it");
			}
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const uint32 Offset = ReadU32(List, 4 + static_cast<std::size_t>(Index) * 4);
				if (Offset > List.size())
				{
					return MakeError("list entry " + std::to_string(Index) + " lies past it");
				}
				Entries.push_back(List.subspan(Offset));
			}
			return Entries;
		}

		/** A slot's image set: its images, each u16 width, u16 height and its units. */
		TResult<std::vector<FVramImage>> ReadImageSet(std::span<const uint8> Set, const FFieldSpriteSlot& Slot)
		{
			TResult<std::vector<std::span<const uint8>>> Images = ReadEntries(Set);
			if (!Images)
			{
				return Images.Error();
			}
			std::vector<FVramImage> Uploads;
			for (std::size_t Index = 0; Index < Images->size(); ++Index)
			{
				const std::span<const uint8> Image = (*Images)[Index];
				if (!Has(Image, 0, 4))
				{
					return MakeError("sprite image has no size");
				}
				const int16 Width = ReadS16(Image, 0);
				const int16 Height = ReadS16(Image, 2);
				const std::size_t Units = static_cast<std::size_t>(Width < 0 ? 0 : Width) * static_cast<std::size_t>(Height < 0 ? 0 : Height);
				if (Width < 0 || Height < 0 || !Has(Image, 4, Units * 2))
				{
					return MakeError("sprite image runs past its section");
				}
				const int16 X = static_cast<int16>(Slot.X + SpriteImageSpacing * static_cast<int32>(Index));
				FVramImage Upload{{X, Slot.Y, Width, Height}, std::vector<uint16>(Units)};
				for (std::size_t Unit = 0; Unit < Units; ++Unit)
				{
					Upload.Units[Unit] = ReadU16(Image, 4 + Unit * 2);
				}
				Uploads.push_back(std::move(Upload));
			}
			return Uploads;
		}
	}

	TResult<std::vector<FVramImage>> ReadFieldArchiveImages(const FFieldArchive& Archive)
	{
		std::vector<FVramImage> Uploads;
		TResult<std::vector<std::span<const uint8>>> TimLists = ReadEntries(Archive.Section(EFieldSection::Tims));
		if (!TimLists)
		{
			return MakeError("TIM section: " + TimLists.Error().Message);
		}
		for (const std::span<const uint8> TimList : *TimLists)
		{
			TResult<std::vector<FVramImage>> Tims = ReadTimSequence(TimList);
			if (!Tims)
			{
				return Tims.Error();
			}
			Uploads.insert(Uploads.end(), std::make_move_iterator(Tims->begin()), std::make_move_iterator(Tims->end()));
		}
		TResult<std::vector<std::span<const uint8>>> Sets = ReadEntries(Archive.Section(EFieldSection::SpriteImages));
		if (!Sets)
		{
			return MakeError("sprite image section: " + Sets.Error().Message);
		}
		if (Sets->size() > Archive.SpriteSlots.size())
		{
			return MakeError("sprite image section has more sets than there are sprite slots");
		}
		for (std::size_t Slot = 0; Slot < Sets->size(); ++Slot)
		{
			if (!Archive.SpriteSlots[Slot].bImagesInArchive)
			{
				continue;
			}
			TResult<std::vector<FVramImage>> Images = ReadImageSet((*Sets)[Slot], Archive.SpriteSlots[Slot]);
			if (!Images)
			{
				return MakeError("sprite slot " + std::to_string(Slot) + ": " + Images.Error().Message);
			}
			Uploads.insert(Uploads.end(), std::make_move_iterator(Images->begin()), std::make_move_iterator(Images->end()));
		}
		return Uploads;
	}

	TResult<std::vector<FVramImage>> ReadFieldSpriteSlotImages(const FFieldArchive& Archive, int32 Slot, const FFieldSpriteSlot& At)
	{
		if (Slot < 0 || Slot >= static_cast<int32>(Archive.SpriteSlots.size()) || !Archive.SpriteSlots[static_cast<std::size_t>(Slot)].bImagesInArchive)
		{
			return MakeError("sprite slot " + std::to_string(Slot) + " does not take its images from the archive");
		}
		TResult<std::vector<std::span<const uint8>>> Sets = ReadEntries(Archive.Section(EFieldSection::SpriteImages));
		if (!Sets)
		{
			return MakeError("sprite image section: " + Sets.Error().Message);
		}
		if (static_cast<std::size_t>(Slot) >= Sets->size())
		{
			return MakeError("sprite image section has no set for slot " + std::to_string(Slot));
		}
		const FFieldSpriteSlot Placed{At.X, At.Y, true};
		TResult<std::vector<FVramImage>> Images = ReadImageSet((*Sets)[static_cast<std::size_t>(Slot)], Placed);
		if (!Images)
		{
			return MakeError("sprite slot " + std::to_string(Slot) + ": " + Images.Error().Message);
		}
		return Images;
	}

	void UploadFieldTextures(const FFieldMap& Map, FVram& Vram, bool bBrightenPalettes)
	{
		UploadFieldArchiveImages(Map, Vram);
		UploadFieldTextureStream(Map, Vram, bBrightenPalettes);
	}

	void UploadFieldArchiveImages(const FFieldMap& Map, FVram& Vram)
	{
		for (std::size_t Upload = 0; Upload < Map.ArchiveUploads; ++Upload)
		{
			Vram.Upload(Map.TextureUploads[Upload]);
		}
	}

	void UploadFieldTextureStream(const FFieldMap& Map, FVram& Vram, bool bBrightenPalettes)
	{
		for (std::size_t Upload = Map.ArchiveUploads; Upload < Map.TextureUploads.size(); ++Upload)
		{
			Vram.Upload(Map.TextureUploads[Upload]);
		}
		if (!bBrightenPalettes)
		{
			return;
		}
		FVramImage Palettes{PaletteRows, std::vector<uint16>(static_cast<std::size_t>(PaletteRows.Width) * PaletteRows.Height)};
		for (int32 Row = 0; Row < PaletteRows.Height; ++Row)
		{
			for (int32 Column = 0; Column < PaletteRows.Width; ++Column)
			{
				const uint16 Color = Vram.At(PaletteRows.X + Column, PaletteRows.Y + Row);
				Palettes.Units[static_cast<std::size_t>(Row * PaletteRows.Width + Column)] = Color != 0 ? static_cast<uint16>(Color | PaletteFloor) : Color;
			}
		}
		Vram.Upload(Palettes);
	}
}
