#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Game/OriginalSave.h"

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * The port's saves: 120 slots in eight stores of 15, each store standing for a memory card, so that slot
	 * 15k + n is No. n + 1 on card k + 1. A save is a file of its game data as the game writes it, in its
	 * store's folder under the root. A save is written beside its file first and then put in place, so a
	 * failed write never loses the save it replaces. The slot last saved to is kept for the File screen to
	 * open on. A store written as a memory card image goes in the Cards folder beside the stores', and the
	 * folder the player last imported saves from is kept for the next import to open in.
	 */
	class FSaveStore
	{
	public:
		static constexpr int32 StoreCount = 8;
		static constexpr int32 SlotsPerStore = 15;
		static constexpr int32 SlotCount = StoreCount * SlotsPerStore;

		WELTALLCORE_API explicit FSaveStore(std::filesystem::path InRoot);

		/** The save in Slot: none when the slot is empty, an error when its file cannot be read or is damaged. */
		WELTALLCORE_API TResult<std::optional<FOriginalSave>> Read(int32 Slot) const;

		/** Slot's file as it is, damaged or not: none when the slot is empty, an error when it cannot be read. */
		WELTALLCORE_API TResult<std::optional<std::vector<uint8>>> ReadBytes(int32 Slot) const;

		/** Save put in Slot, which becomes the slot last saved to; otherwise why it could not be. */
		WELTALLCORE_API std::optional<FError> Write(int32 Slot, const FOriginalSave& Save);

		/** Another slot's file put in Slot as it is (a copy), the slot last saved to unchanged; otherwise why it could not be. */
		WELTALLCORE_API std::optional<FError> Copy(int32 Slot, std::span<const uint8> File);

		/** The save in Slot removed; otherwise why it could not be. An empty slot stays empty. */
		WELTALLCORE_API std::optional<FError> Remove(int32 Slot);

		/** The slot last saved to, while its save is there. */
		WELTALLCORE_API std::optional<int32> LastSaved() const;

		/** Store's card image (from ExportStoreCard) written as "Card N.mcr" in the Cards folder: its file, or why it could not be. */
		WELTALLCORE_API TResult<std::filesystem::path> WriteCardImage(int32 Store, std::span<const uint8> Image);

		/** The folder saves were last imported from, while it is noted. */
		WELTALLCORE_API std::optional<std::filesystem::path> ImportFolder() const;
		WELTALLCORE_API void KeepImportFolder(const std::filesystem::path& Folder);

	private:
		std::filesystem::path SlotFile(int32 Slot) const;
		std::filesystem::path LastSavedFile() const;
		std::filesystem::path ImportFolderFile() const;

		std::filesystem::path Root;
	};
}
