#include "Game/SaveStore.h"

#include "Foundation/WholeFiles.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

namespace Weltall
{
	namespace
	{
		bool IsSlot(int32 Slot)
		{
			return Slot >= 0 && Slot < FSaveStore::SlotCount;
		}

		std::string SlotError(int32 Slot)
		{
			return "there is no save slot " + std::to_string(Slot);
		}
	}

	FSaveStore::FSaveStore(std::filesystem::path InRoot) : Root(std::move(InRoot))
	{
	}

	TResult<std::optional<FOriginalSave>> FSaveStore::Read(int32 Slot) const
	{
		TResult<std::optional<std::vector<uint8>>> Bytes = ReadBytes(Slot);
		if (!Bytes)
		{
			return Bytes.Error();
		}
		if (!*Bytes)
		{
			return std::optional<FOriginalSave>();
		}
		TResult<FOriginalSave> Save = FOriginalSave::FromData(**Bytes);
		if (!Save)
		{
			return MakeError("the save " + SlotFile(Slot).string() + " is damaged: " + Save.Error().Message);
		}
		return std::optional<FOriginalSave>(std::move(*Save));
	}

	TResult<std::optional<std::vector<uint8>>> FSaveStore::ReadBytes(int32 Slot) const
	{
		if (!IsSlot(Slot))
		{
			return MakeError(SlotError(Slot));
		}
		return ReadWholeFile(SlotFile(Slot));
	}

	std::optional<FError> FSaveStore::Write(int32 Slot, const FOriginalSave& Save)
	{
		if (!IsSlot(Slot))
		{
			return MakeError(SlotError(Slot));
		}
		if (std::optional<FError> Failed = ReplaceWholeFile(SlotFile(Slot), Save.Data()))
		{
			return Failed;
		}
		const std::string Number = std::to_string(Slot + 1);
		// The save is in place: failing to note its slot costs only where the File screen opens.
		static_cast<void>(ReplaceWholeFile(LastSavedFile(), std::span(reinterpret_cast<const uint8*>(Number.data()), Number.size())));
		return std::nullopt;
	}

	std::optional<FError> FSaveStore::Copy(int32 Slot, std::span<const uint8> File)
	{
		if (!IsSlot(Slot))
		{
			return MakeError(SlotError(Slot));
		}
		return ReplaceWholeFile(SlotFile(Slot), File);
	}

	std::optional<FError> FSaveStore::Remove(int32 Slot)
	{
		if (!IsSlot(Slot))
		{
			return MakeError(SlotError(Slot));
		}
		std::error_code Error;
		std::filesystem::remove(SlotFile(Slot), Error);
		if (Error)
		{
			return MakeError("the save " + SlotFile(Slot).string() + " cannot be removed: " + Error.message());
		}
		return std::nullopt;
	}

	std::optional<int32> FSaveStore::LastSaved() const
	{
		std::ifstream File(LastSavedFile());
		int32 Number = 0;
		if (!(File >> Number) || !IsSlot(Number - 1))
		{
			return std::nullopt;
		}
		std::error_code Error;
		if (!std::filesystem::exists(SlotFile(Number - 1), Error))
		{
			return std::nullopt;
		}
		return Number - 1;
	}

	TResult<std::filesystem::path> FSaveStore::WriteCardImage(int32 Store, std::span<const uint8> Image)
	{
		if (Store < 0 || Store >= StoreCount)
		{
			return MakeError("there is no save store " + std::to_string(Store));
		}
		const std::filesystem::path File = Root / "Cards" / ("Card " + std::to_string(Store + 1) + ".mcr");
		if (std::optional<FError> Failed = ReplaceWholeFile(File, Image))
		{
			return *Failed;
		}
		return File;
	}

	std::optional<std::filesystem::path> FSaveStore::ImportFolder() const
	{
		std::ifstream File(ImportFolderFile(), std::ios::binary);
		const std::string Text((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
		if (Text.empty())
		{
			return std::nullopt;
		}
		return std::filesystem::path(std::u8string(Text.begin(), Text.end()));
	}

	void FSaveStore::KeepImportFolder(const std::filesystem::path& Folder)
	{
		const std::u8string Text = Folder.u8string();
		// Where the next import opens: failing to note it costs nothing else.
		static_cast<void>(ReplaceWholeFile(ImportFolderFile(), std::span(reinterpret_cast<const uint8*>(Text.data()), Text.size())));
	}

	std::filesystem::path FSaveStore::SlotFile(int32 Slot) const
	{
		char Name[16];
		std::snprintf(Name, sizeof Name, "No%02d.sav", Slot % SlotsPerStore + 1);
		return Root / ("Card" + std::to_string(Slot / SlotsPerStore + 1)) / Name;
	}

	std::filesystem::path FSaveStore::LastSavedFile() const
	{
		return Root / "LastSaved.txt";
	}

	std::filesystem::path FSaveStore::ImportFolderFile() const
	{
		return Root / "ImportFolder.txt";
	}
}
