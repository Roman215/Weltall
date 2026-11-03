#include "Field/FieldMap.h"

#include "Field/FieldTextures.h"
#include "Foundation/LittleEndian.h"
#include "Graphics/VramStream.h"
#include "Media/GameFileIndex.h"
#include "Text/MessageTableIds.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>

namespace Weltall
{
	namespace
	{
		constexpr int32 MapDirectory = 4;
		constexpr int32 FirstMapFile = 0xB8;
		/** What the second disc holds instead of the maps it never loads. */
		constexpr std::string_view PlaceholderText = "It's CDMAKE Dummy !";
		constexpr std::size_t ZoneBytes = 0x18;
		constexpr std::size_t CornerBytes = 6;

		std::vector<FFieldZone> ReadZones(std::span<const uint8> Section)
		{
			std::vector<FFieldZone> Zones(Section.size() / ZoneBytes);
			for (std::size_t Zone = 0; Zone < Zones.size(); ++Zone)
			{
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					const std::size_t At = Zone * ZoneBytes + Corner * CornerBytes;
					Zones[Zone].Corners[Corner] = {LittleEndian::ReadS16(Section, At), LittleEndian::ReadS16(Section, At + 2), LittleEndian::ReadS16(Section, At + 4)};
				}
			}
			return Zones;
		}

		FFieldModelBounds ComputeBounds(const FMdlPart& Part)
		{
			FFieldModelBounds Bounds;
			int32 Longest = 0;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				const int32 Side = Part.BoundsMax[Axis] - Part.BoundsMin[Axis];
				Longest = Axis == 0 ? Side : std::max(Longest, Side);
				Bounds.Centre[Axis] = static_cast<int16>(static_cast<int16>(Side / 2) + Part.BoundsMin[Axis]);
			}
			Bounds.Radius = static_cast<int16>(static_cast<int16>(Longest) * 2 + 1);
			return Bounds;
		}

		/** The field builds each actor's model from the model's first part, one actor after another. */
		TResult<std::vector<FFieldModelObject>> BuildModelObjects(const std::vector<FFieldActorDefinition>& Actors, const std::vector<FMdlModel>& Models)
		{
			std::vector<FFieldModelObject> Objects;
			FMdlTextureState Builder;
			for (std::size_t Actor = 0; Actor < Actors.size(); ++Actor)
			{
				const FFieldActorDefinition& Definition = Actors[Actor];
				if (!Definition.HasModel())
				{
					continue;
				}
				if (Definition.Model >= Models.size() || Models[Definition.Model].Parts.empty())
				{
					return MakeError("actor " + std::to_string(Actor) + " places model " + std::to_string(Definition.Model) + ", which the map lacks");
				}
				const FMdlPart& Part = Models[Definition.Model].Parts.front();
				Objects.push_back({static_cast<uint16>(Actor), Builder, ComputeBounds(Part)});
				Builder = Part.TextureStateAfter(Builder);
			}
			return Objects;
		}

		TResult<std::vector<uint8>> ReadMapFile(const FCdImage& Image, const FGameFileIndex& Index, int32 Number)
		{
			const FIndexRecord* Record = Index.Find(MapDirectory, Number);
			if (Record == nullptr)
			{
				return MakeError("the disc lacks file " + std::to_string(Number) + " of directory 4");
			}
			TResult<std::vector<uint8>> Sectors = ReadFileSectors(Image, *Record);
			if (Sectors && Sectors->size() >= PlaceholderText.size() && std::equal(PlaceholderText.begin(), PlaceholderText.end(), Sectors->begin()))
			{
				return MakeError("this disc holds a placeholder for file " + std::to_string(Number) + " of directory 4");
			}
			return Sectors;
		}

		void ApplyPlayersMessages(const FCdImage& Image, int32 MapNumber, std::vector<uint8>& Messages, FReplacedMessages& Replaced)
		{
			if (std::optional<FAppliedMessages> Applied = ApplyMessageOverrides(Image, MessageTables::Field(MapNumber), Messages, EMessageWindows::Table))
			{
				Messages = std::move(Applied->Table);
				Replaced = std::move(Applied->Replaced);
			}
		}
	}

	TResult<FFieldMap> ParseFieldMap(std::span<const uint8> ArchiveSectors, std::span<const uint8> TextureStreamSectors)
	{
		TResult<FFieldArchive> Archive = ParseFieldArchive(ArchiveSectors);
		if (!Archive)
		{
			return Archive.Error();
		}
		FFieldMap Map;
		Map.SpriteSlots = Archive->SpriteSlots;
		Map.Lights = Archive->Lights;
		Map.Actors = std::move(Archive->Actors);

		TResult<std::vector<FMdlModel>> Models = ParseMdlModelList(Archive->Section(EFieldSection::Models));
		if (!Models)
		{
			return MakeError("field models: " + Models.Error().Message);
		}
		Map.Models = std::move(*Models);
		TResult<std::vector<FFieldModelObject>> Objects = BuildModelObjects(Map.Actors, Map.Models);
		if (!Objects)
		{
			return Objects.Error();
		}
		Map.ModelObjects = std::move(*Objects);

		TResult<FFieldWalkmesh> Walkmesh = ParseFieldWalkmesh(Archive->Section(EFieldSection::Walkmesh));
		if (!Walkmesh)
		{
			return MakeError("field walkmesh: " + Walkmesh.Error().Message);
		}
		Map.Walkmesh = std::move(*Walkmesh);
		Map.Zones = ReadZones(Archive->Section(EFieldSection::Zones));

		TResult<std::vector<FVramImage>> ArchiveImages = ReadFieldArchiveImages(*Archive);
		if (!ArchiveImages)
		{
			return MakeError("field archive images: " + ArchiveImages.Error().Message);
		}
		TResult<std::vector<FVramImage>> StreamImages = DecodeVramStream(TextureStreamSectors);
		if (!StreamImages)
		{
			return MakeError("field texture stream: " + StreamImages.Error().Message);
		}
		Map.TextureUploads = std::move(*ArchiveImages);
		Map.ArchiveUploads = Map.TextureUploads.size();
		Map.TextureUploads.insert(Map.TextureUploads.end(), std::make_move_iterator(StreamImages->begin()), std::make_move_iterator(StreamImages->end()));

		Map.SpriteSets = std::move(Archive->Section(EFieldSection::SpriteSets));
		Map.EventScripts = std::move(Archive->Section(EFieldSection::EventScripts));
		Map.Encounters = std::move(Archive->Section(EFieldSection::Encounters));
		Map.Messages = std::move(Archive->Section(EFieldSection::Messages));
		return Map;
	}

	bool HoldsFieldMap(const FCdImage& Image, const FGameFileIndex& Index, int32 MapNumber)
	{
		if (MapNumber < 0 || MapNumber >= FieldMapCount)
		{
			return false;
		}
		const FIndexRecord* Record = Index.Find(MapDirectory, FirstMapFile + 2 * MapNumber);
		const TResult<std::vector<uint8>> Sectors = Record != nullptr ? ReadFileSectors(Image, *Record) : TResult<std::vector<uint8>>(MakeError("no record"));
		return !Sectors || Sectors->size() < PlaceholderText.size() || !std::equal(PlaceholderText.begin(), PlaceholderText.end(), Sectors->begin());
	}

	TResult<FFieldArchive> LoadFieldArchive(const FCdImage& Image, const FGameFileIndex& Index, int32 MapNumber)
	{
		if (MapNumber < 0 || MapNumber >= FieldMapCount)
		{
			return MakeError("there is no field map " + std::to_string(MapNumber));
		}
		TResult<std::vector<uint8>> Sectors = ReadMapFile(Image, Index, FirstMapFile + 2 * MapNumber);
		if (!Sectors)
		{
			return MakeError("field map " + std::to_string(MapNumber) + ": " + Sectors.Error().Message);
		}
		TResult<FFieldArchive> Archive = ParseFieldArchive(*Sectors);
		if (!Archive)
		{
			return MakeError("field map " + std::to_string(MapNumber) + ": " + Archive.Error().Message);
		}
		return Archive;
	}

	TResult<FFieldMap> LoadFieldMap(const FCdImage& Image, const FGameFileIndex& Index, int32 MapNumber)
	{
		if (MapNumber < 0 || MapNumber >= FieldMapCount)
		{
			return MakeError("there is no field map " + std::to_string(MapNumber));
		}
		const int32 ArchiveFile = FirstMapFile + 2 * MapNumber;
		TResult<std::vector<uint8>> Archive = ReadMapFile(Image, Index, ArchiveFile);
		if (!Archive)
		{
			return MakeError("field map " + std::to_string(MapNumber) + ": " + Archive.Error().Message);
		}
		TResult<std::vector<uint8>> TextureStream = ReadMapFile(Image, Index, ArchiveFile + 1);
		if (!TextureStream)
		{
			return MakeError("field map " + std::to_string(MapNumber) + ": " + TextureStream.Error().Message);
		}
		TResult<FFieldMap> Map = ParseFieldMap(*Archive, *TextureStream);
		if (!Map)
		{
			return MakeError("field map " + std::to_string(MapNumber) + ": " + Map.Error().Message);
		}
		std::vector<uint8> DiscMessages = Map->Messages;
		ApplyPlayersMessages(Image, MapNumber, Map->Messages, Map->ReplacedMessages);
		if (Map->Messages != DiscMessages)
		{
			Map->DiscMessages = std::move(DiscMessages);
		}
		return Map;
	}

	TResult<std::vector<uint8>> LoadFieldMapMessages(const FCdImage& Image, const FGameFileIndex& Index, int32 MapNumber)
	{
		TResult<FFieldArchive> Archive = LoadFieldArchive(Image, Index, MapNumber);
		if (!Archive)
		{
			return Archive.Error();
		}
		std::vector<uint8> Messages = std::move(Archive->Section(EFieldSection::Messages));
		FReplacedMessages Replaced;
		ApplyPlayersMessages(Image, MapNumber, Messages, Replaced);
		return Messages;
	}
}
