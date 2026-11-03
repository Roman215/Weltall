#include "Game/OriginalSave.h"

#include "Foundation/LittleEndian.h"
#include "Game/SaveLayout.h"
#include "Media/MemoryCard.h"
#include "Mods/ExpansionMods.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <string_view>

namespace Weltall
{
	namespace
	{
		/** Every save of the game names itself with the first disc's product code, disc 2's too. */
		constexpr std::string_view FileNamePrefix = "BASLUS-00664";
		constexpr std::size_t ChecksumOffset = FOriginalSave::DataSize - 1;

		/** The low byte of the sum of every byte before it. */
		uint8 Checksum(std::span<const uint8> Data)
		{
			return static_cast<uint8>(std::accumulate(Data.begin(), Data.begin() + ChecksumOffset, 0u));
		}
	}

	TResult<FOriginalSave> FOriginalSave::FromCard(const FCardSave& Save)
	{
		if (!Save.FileName.starts_with(FileNamePrefix))
		{
			return MakeError("the save " + Save.FileName + " is not one of this game's");
		}
		if (Save.Blocks.size() < DataOffset + DataSize)
		{
			return MakeError("the save " + Save.FileName + " is too short");
		}
		TResult<FOriginalSave> Result = FromData(std::span<const uint8>(Save.Blocks).subspan(DataOffset, DataSize));
		if (!Result)
		{
			return MakeError("the save " + Save.FileName + " is damaged: " + Result.Error().Message);
		}
		return Result;
	}

	TResult<FOriginalSave> FOriginalSave::FromData(std::span<const uint8> Data)
	{
		if (Data.size() != DataSize)
		{
			return MakeError("a save's data is " + std::to_string(DataSize) + " bytes, not " + std::to_string(Data.size()));
		}
		if (Checksum(Data) != Data[ChecksumOffset])
		{
			return MakeError("its checksum does not hold");
		}
		FOriginalSave Result;
		Result.Bytes.assign(Data.begin(), Data.end());
		return Result;
	}

	FOriginalSave FOriginalSave::WithChecksum(std::vector<uint8> Data)
	{
		FOriginalSave Result;
		Result.Bytes = std::move(Data);
		Result.Bytes[ChecksumOffset] = Checksum(Result.Bytes);
		return Result;
	}

	uint32 FOriginalSave::PlayTime() const
	{
		return LittleEndian::ReadU32(Bytes, SaveLayout::Summary::PlayTime);
	}

	uint16 FOriginalSave::StoryProgress() const
	{
		return LittleEndian::ReadU16(PlayedData(), SaveLayout::StoryProgress);
	}

	std::vector<uint8> FOriginalSave::PlayedData(bool bExpansionPlayed) const
	{
		namespace Port = SaveLayout::Port;
		std::vector<uint8> Played = Bytes;
		if ((StoryMode() != EStoryMode::Expanded || !bExpansionPlayed) && !KeepsEllyApart(ClearGame()))
		{
			return Played;
		}
		std::size_t At = Port::KeptRuns + 1;
		for (int32 Run = 0; Run < Bytes[Port::KeptRuns]; ++Run)
		{
			constexpr std::size_t RunHead = 3;
			if (At + RunHead > Port::RunsEnd)
			{
				return Bytes;
			}
			const std::size_t Offset = LittleEndian::ReadU16(Bytes, At);
			const std::size_t Length = Bytes[At + 2];
			At += RunHead;
			// Runs the port did not write (reaching past the game's own bytes, or the checksum): the save as it is.
			if (Offset + Length > Port::Mark || At + Length > Port::RunsEnd)
			{
				return Bytes;
			}
			std::copy_n(Bytes.begin() + static_cast<std::ptrdiff_t>(At), Length, Played.begin() + static_cast<std::ptrdiff_t>(Offset));
			At += Length;
		}
		return Played;
	}

	std::array<uint8, 3> FOriginalSave::Party() const
	{
		const std::size_t Party = SaveLayout::Summary::Party;
		return {Bytes[Party], Bytes[Party + 1], Bytes[Party + 2]};
	}

	std::array<uint8, 3> FOriginalSave::Levels() const
	{
		const std::size_t Levels = SaveLayout::Summary::Levels;
		return {Bytes[Levels], Bytes[Levels + 1], Bytes[Levels + 2]};
	}

	int32 FOriginalSave::CardSlot() const
	{
		return Bytes[SaveLayout::Summary::CardSlot];
	}

	bool FOriginalSave::HasPortPart() const
	{
		namespace Port = SaveLayout::Port;
		return std::equal(Port::MarkBytes.begin(), Port::MarkBytes.end(), Bytes.begin() + static_cast<std::ptrdiff_t>(Port::Mark));
	}

	EStoryMode FOriginalSave::StoryMode() const
	{
		namespace Port = SaveLayout::Port;
		if (!HasPortPart())
		{
			return EStoryMode::Unchosen;
		}
		switch (Bytes[Port::StoryMode])
		{
		case static_cast<uint8>(EStoryMode::Original):
			return EStoryMode::Original;
		case static_cast<uint8>(EStoryMode::Expanded):
			return EStoryMode::Expanded;
		default:
			return EStoryMode::Unchosen;
		}
	}

	std::string FOriginalSave::Expansion() const
	{
		if (StoryMode() != EStoryMode::Expanded)
		{
			return {};
		}
		const auto Name = Bytes.begin() + static_cast<std::ptrdiff_t>(SaveLayout::Port::Expansion);
		const auto End = std::find(Name, Name + static_cast<std::ptrdiff_t>(ExpansionFolderBytes), uint8{0});
		return Name == End ? std::string(ExamplesFolder) : std::string(Name, End);
	}

	FPartyOrder FOriginalSave::PartyOrder() const
	{
		if (!HasPortPart())
		{
			return {};
		}
		return FPartyOrder::FromSaved(std::span<const uint8, FPartyOrder::SavedBytes>(Bytes.data() + SaveLayout::Port::PartyOrder, FPartyOrder::SavedBytes));
	}

	FCharacterLooks FOriginalSave::CharacterLooks() const
	{
		if (!HasPortPart())
		{
			return {};
		}
		return FCharacterLooks::FromSaved(std::span<const uint8, FCharacterLooks::SavedBytes>(Bytes.data() + SaveLayout::Port::CharacterLooks, FCharacterLooks::SavedBytes));
	}

	EClearGame FOriginalSave::ClearGame() const
	{
		// 2 marks the clear file as the ending saved it, a game of the clear file's like any other.
		constexpr uint8 ClearFileAsSaved = 2;
		const uint8 Kind = HasPortPart() ? Bytes[SaveLayout::Port::ClearGame] : 0;
		if (Kind == static_cast<uint8>(EClearGame::ClearFile) || Kind == ClearFileAsSaved)
		{
			return EClearGame::ClearFile;
		}
		for (const EClearGame Known : {EClearGame::NewGamePlus, EClearGame::KeepElly})
		{
			if (Kind == static_cast<uint8>(Known))
			{
				return Known;
			}
		}
		return EClearGame::None;
	}
}
