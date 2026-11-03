#include "Battle/Show/BattleObjectFiles.h"

#include "Battle/Show/BattleSceneGraphics.h"
#include "Formats/Lzss.h"
#include "Foundation/LittleEndian.h"
#include "Media/GameFileIndex.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		constexpr int32 ProgramDirectory = 1;
		constexpr int32 ProgramFile = 16;
		/** Where the kernel loads the program, and where its tables lie. */
		constexpr uint32 Base = 0x8006FAF0;
		constexpr uint32 GearTableAddress = 0x800C3508;
		constexpr uint32 ExtensionTableAddress = 0x800C3530;
		constexpr uint32 GearPlacesAddress = 0x800C3668;
		/** A variant archive's first section: its part count (a word), then per part the bone and the offset. */
		constexpr std::size_t PartsStart = 4;
		constexpr std::size_t PartBytes = 8;
		/** An extension's sound container: the sprite resource, the sound effects, then two entries, the image list. */
		constexpr std::size_t SpriteEntry = 0;
		constexpr std::size_t SoundEffectsEntry = 1;
		constexpr std::size_t ImagesEntry = 4;
		constexpr std::size_t SoundContainerEntries = ImagesEntry + 2;
		constexpr uint32 MaxEntries = 0x400;

		bool Has(std::span<const uint8> Bytes, std::size_t Offset, std::size_t Count)
		{
			return Offset <= Bytes.size() && Count <= Bytes.size() - Offset;
		}

		/** A section container's entries as offsets from the start of Bytes. */
		std::optional<std::vector<uint32>> Container(std::span<const uint8> Bytes, std::size_t At)
		{
			if (!Has(Bytes, At, 4))
			{
				return std::nullopt;
			}
			const uint32 Count = ReadU32(Bytes, At);
			if (Count == 0 || Count >= MaxEntries || !Has(Bytes, At + 4, 4 * std::size_t{Count}))
			{
				return std::nullopt;
			}
			std::vector<uint32> Entries(Count);
			for (uint32 Entry = 0; Entry < Count; ++Entry)
			{
				Entries[Entry] = static_cast<uint32>(At) + ReadU32(Bytes, At + 4 + 4 * std::size_t{Entry});
				if (Entries[Entry] > Bytes.size())
				{
					return std::nullopt;
				}
			}
			return Entries;
		}

		TResult<std::vector<uint8>> ReadSectors(const FCdImage& Image, const FGameFileIndex& Index, int32 Directory, int32 File)
		{
			const FIndexRecord* Record = Index.Find(Directory, File);
			if (Record == nullptr || !Record->IsFile())
			{
				return MakeError("the disc lacks file " + std::to_string(File) + " of directory " + std::to_string(Directory));
			}
			return ReadFileSectors(Image, *Record);
		}

		TResult<FBattleModelFiles> ParsePair(std::span<const uint8> ModelBytes, std::vector<uint8> MotionBytes)
		{
			TResult<FGearModelPack> Model = ParseGearModelPack(ModelBytes);
			if (!Model)
			{
				return Model.Error();
			}
			TResult<FGearMotionPack> Motion = ParseGearMotionPack(std::move(MotionBytes));
			if (!Motion)
			{
				return Motion.Error();
			}
			return FBattleModelFiles{std::move(*Model), std::move(*Motion), std::nullopt, std::nullopt};
		}

		TResult<FBattleModelFiles> ReadPair(const FCdImage& Image, const FGameFileIndex& Index, int32 Directory, int32 ModelFile)
		{
			TResult<std::vector<uint8>> ModelBytes = ReadSectors(Image, Index, Directory, ModelFile);
			if (!ModelBytes)
			{
				return ModelBytes.Error();
			}
			TResult<std::vector<uint8>> MotionBytes = ReadSectors(Image, Index, Directory, ModelFile + 1);
			if (!MotionBytes)
			{
				return MotionBytes.Error();
			}
			TResult<FBattleModelFiles> Files = ParsePair(*ModelBytes, std::move(*MotionBytes));
			if (!Files)
			{
				return MakeError("directory " + std::to_string(Directory) + " file " + std::to_string(ModelFile) + ": " + Files.Error().Message);
			}
			return Files;
		}
	}

	TResult<FBattleObjectTables> FBattleObjectTables::Parse(std::span<const uint8> Program)
	{
		const auto At = [&](uint32 Address, std::size_t Count) -> std::optional<std::span<const uint8>> {
			if (Address < Base || !Has(Program, Address - Base, Count))
			{
				return std::nullopt;
			}
			return Program.subspan(Address - Base, Count);
		};
		const auto Gears = At(GearTableAddress, 2 * GearCount);
		const auto Extensions = At(ExtensionTableAddress, ExtensionGroups);
		const auto Places = At(GearPlacesAddress, 4 * GearPlaceCount);
		if (!Gears || !Extensions || !Places)
		{
			return MakeError("the Battle program is too short to hold its objects' tables");
		}
		FBattleObjectTables Tables;
		for (std::size_t Gear = 0; Gear < Tables.Gears.size(); ++Gear)
		{
			Tables.Gears[Gear] = {(*Gears)[2 * Gear], (*Gears)[2 * Gear + 1]};
		}
		std::copy(Extensions->begin(), Extensions->end(), Tables.Extensions.begin());
		for (std::size_t Place = 0; Place < Tables.GearPlaces.size(); ++Place)
		{
			Tables.GearPlaces[Place].X = ReadS16(*Places, 4 * Place);
			Tables.GearPlaces[Place].Y = ReadS16(*Places, 4 * Place + 2);
		}
		return Tables;
	}

	TResult<FBattleObjectTables> FBattleObjectTables::Load(const FCdImage& Image, const FGameFileIndex& Index)
	{
		TResult<std::vector<uint8>> Sectors = ReadSectors(Image, Index, ProgramDirectory, ProgramFile);
		if (!Sectors)
		{
			return Sectors.Error();
		}
		const TResult<std::vector<uint8>> Program = UnpackLzss(*Sectors);
		if (!Program)
		{
			return Program.Error();
		}
		return Parse(*Program);
	}

	TResult<FBattleGearVariant> FBattleGearVariant::Parse(std::span<const uint8> File)
	{
		const std::optional<std::vector<uint32>> Sections = Container(File, 0);
		if (!Sections || Sections->size() < 2 || !Has(File, (*Sections)[0], 2))
		{
			return MakeError("a gear variant's archive lacks its parts' list");
		}
		FBattleGearVariant Variant;
		const int16 Count = ReadS16(File, (*Sections)[0]);
		if (Count == 0)
		{
			TResult<std::vector<FGearTextureBlock>> Images = ReadGearImageList(File, (*Sections)[1]);
			if (!Images)
			{
				return Images.Error();
			}
			Variant.Images = std::move(*Images);
			return Variant;
		}
		if (Count < 0 || Sections->size() < 3 || !Has(File, (*Sections)[0] + PartsStart, std::size_t(Count) * PartBytes) || (*Sections)[2] < (*Sections)[1])
		{
			return MakeError("a gear variant's parts lack their list or their archives");
		}
		for (int16 Part = 0; Part < Count; ++Part)
		{
			const std::size_t At = (*Sections)[0] + PartsStart + std::size_t(Part) * PartBytes;
			Variant.Parts.push_back({ReadU16(File, At), {ReadS16(File, At + 2), ReadS16(File, At + 4), ReadS16(File, At + 6)}});
		}
		// The parts' sequence archive is copied out whole to a block of its own; the model archive stays in place.
		TResult<FGearMotionPack> Motion = ParseGearMotionPack(std::vector<uint8>(File.begin() + (*Sections)[1], File.begin() + (*Sections)[2]));
		TResult<FGearModelPack> Model = ParseGearModelPack(File.subspan((*Sections)[2]));
		if (!Motion || !Model)
		{
			return MakeError("a gear variant's parts: " + (!Motion ? Motion.Error().Message : Model.Error().Message));
		}
		Variant.Motion = std::move(*Motion);
		Variant.Model = std::move(*Model);
		return Variant;
	}

	TResult<FBattleSequenceExtension> FBattleSequenceExtension::Parse(std::vector<uint8> File)
	{
		const std::optional<std::vector<uint32>> Sections = Container(File, 0);
		const std::optional<std::vector<uint32>> Sounds = Sections && Sections->size() >= 2 ? Container(File, (*Sections)[1]) : std::nullopt;
		if (!Sounds || Sounds->size() < SoundContainerEntries)
		{
			return MakeError("a sequence extension lacks its sound container's entries");
		}
		FBattleSequenceExtension Extension;
		Extension.Sprite = {(*Sounds)[SpriteEntry], (*Sounds)[SpriteEntry + 1]};
		Extension.SoundEffects = {(*Sounds)[SoundEffectsEntry], (*Sounds)[SoundEffectsEntry + 1]};
		Extension.Images = {(*Sounds)[ImagesEntry], (*Sounds)[ImagesEntry + 1]};
		TResult<FGearMotionPack> Pack = ParseGearMotionPack(std::move(File));
		if (!Pack)
		{
			return MakeError("a sequence extension: " + Pack.Error().Message);
		}
		Extension.Pack = std::move(*Pack);
		return Extension;
	}

	namespace BattleObjectFiles
	{
		TResult<FBattleModelFiles> ReadGear(const FCdImage& Image, const FGameFileIndex& Index, const FBattleObjectTables& Tables, int32 Gear, int32 Variant)
		{
			if (Gear < 0 || Gear >= FBattleObjectTables::GearCount)
			{
				return MakeError("gear " + std::to_string(Gear) + " is past the Battle program's table of gear models");
			}
			const FBattleObjectTables::FGearEntry& Entry = Tables.Gears[static_cast<std::size_t>(Gear)];
			TResult<FBattleModelFiles> Files = ReadPair(Image, Index, GearDirectory, Entry.First + 1);
			if (!Files)
			{
				return Files.Error();
			}
			const int32 Chosen = Variant > Entry.LastVariant ? 0 : Variant;
			if (Chosen != 0)
			{
				TResult<std::vector<uint8>> Bytes = ReadSectors(Image, Index, GearDirectory, Entry.First + 2 + Chosen);
				if (!Bytes)
				{
					return Bytes.Error();
				}
				TResult<FBattleGearVariant> Parsed = FBattleGearVariant::Parse(*Bytes);
				if (!Parsed)
				{
					return MakeError("gear " + std::to_string(Gear) + " variant " + std::to_string(Chosen) + ": " + Parsed.Error().Message);
				}
				Files->Variant = std::move(*Parsed);
			}
			return Files;
		}

		TResult<FBattleModelFiles> ReadSpecialModel(const FCdImage& Image, const FGameFileIndex& Index, int32 Model)
		{
			if (Model < 0 || Model >= SpecialModelCount)
			{
				return MakeError("there is no special model " + std::to_string(Model));
			}
			return ReadPair(Image, Index, SpecialDirectory, 2 * Model + 1);
		}

		TResult<FBattleSequenceExtension> ReadExtension(const FCdImage& Image, const FGameFileIndex& Index, int32 File)
		{
			TResult<std::vector<uint8>> Bytes = ReadSectors(Image, Index, ExtensionDirectory, File);
			if (!Bytes)
			{
				return Bytes.Error();
			}
			TResult<FBattleSequenceExtension> Extension = FBattleSequenceExtension::Parse(std::move(*Bytes));
			if (!Extension)
			{
				return MakeError("directory 42 file " + std::to_string(File) + ": " + Extension.Error().Message);
			}
			return Extension;
		}

		std::optional<int32> ExtensionFile(const FBattleObjectTables& Tables, int32 Operand, int32 Group)
		{
			if (Group < 0 || Group >= FBattleObjectTables::ExtensionGroups)
			{
				return std::nullopt;
			}
			return Operand + Tables.Extensions[static_cast<std::size_t>(Group)];
		}

		TResult<FBattleModelFiles> SceneModel(const FBattleSceneGraphics& Graphics, int32 Entry)
		{
			const std::vector<FBattleSceneEntry>& Entries = Graphics.Entries();
			if (Entry < 0 || static_cast<std::size_t>(Entry) >= Entries.size())
			{
				return MakeError("a scene's graphics have no entry " + std::to_string(Entry));
			}
			const FBattleSceneEntry& Found = Entries[static_cast<std::size_t>(Entry)];
			if (Found.IsSprite() || !Found.HasOwnImages())
			{
				return MakeError("a scene's graphics entry " + std::to_string(Entry) + " is not a model with its own archives");
			}
			const std::span<const uint8> Bytes = Graphics.Bytes();
			// The sequence archive lies in the data the battle keeps, which runs to the end the file's header gives.
			if (Found.Data > Graphics.KeptEnd() || Graphics.KeptEnd() > Bytes.size() || Found.Images > Bytes.size())
			{
				return MakeError("a scene's model entry points past its file");
			}
			TResult<FBattleModelFiles> Files = ParsePair(Bytes.subspan(Found.Images),
				std::vector<uint8>(Bytes.begin() + Found.Data, Bytes.begin() + Graphics.KeptEnd()));
			if (!Files)
			{
				return MakeError("a scene's model entry " + std::to_string(Entry) + ": " + Files.Error().Message);
			}
			return Files;
		}

		TResult<FBattleModelFiles> ReadSceneModel(const FCdImage& Image, const FGameFileIndex& Index, int32 Scene, int32 Entry)
		{
			TResult<FBattleSceneGraphics> Graphics = FBattleSceneGraphics::Load(Image, Index, Scene);
			if (!Graphics)
			{
				return Graphics.Error();
			}
			TResult<FBattleModelFiles> Files = SceneModel(*Graphics, Entry);
			if (!Files)
			{
				return MakeError("scene " + std::to_string(Scene) + ": " + Files.Error().Message);
			}
			return Files;
		}
	}

	TResult<FBattleObjectDisc> FBattleObjectDisc::Load(const FCdImage& Image, const FGameFileIndex& Index)
	{
		TResult<FBattleObjectTables> Tables = FBattleObjectTables::Load(Image, Index);
		if (!Tables)
		{
			return Tables.Error();
		}
		FBattleObjectDisc Disc;
		Disc.Image = &Image;
		Disc.Index = &Index;
		Disc.ObjectTables = *Tables;
		return Disc;
	}

	TResult<FBattleModelFiles> FBattleObjectDisc::ReadGear(int32 Gear, int32 Variant) const
	{
		return BattleObjectFiles::ReadGear(*Image, *Index, ObjectTables, Gear, Variant);
	}

	TResult<FBattleModelFiles> FBattleObjectDisc::ReadSpecialModel(int32 Model) const
	{
		return BattleObjectFiles::ReadSpecialModel(*Image, *Index, Model);
	}

	TResult<FBattleSequenceExtension> FBattleObjectDisc::ReadExtension(int32 File) const
	{
		return BattleObjectFiles::ReadExtension(*Image, *Index, File);
	}

	TResult<FBattleModelFiles> FBattleObjectDisc::ReadSceneModel(int32 Scene, int32 Entry) const
	{
		return BattleObjectFiles::ReadSceneModel(*Image, *Index, Scene, Entry);
	}
}
