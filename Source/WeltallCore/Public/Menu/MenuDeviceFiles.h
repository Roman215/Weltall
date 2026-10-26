#pragma once

#include "Foundation/Types.h"
#include "Mods/DeviceMods.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Weltall
{
	/**
	 * What a transfer through the device's own file picker does, where the game's folders cannot be reached from
	 * outside it (Android): files the player picks brought in, or a file the player names written out.
	 */
	enum class EDeviceTransfer : uint8
	{
		/** Disc images picked, copied into the Games folder. */
		Discs,
		/** Every save written into one file. */
		BackUp,
		/** A backup's saves put in place of the saves of the same names. */
		Restore,
		/** The game's logs, this session's and the earlier ones the engine keeps, written into one file for a bug report. */
		Log,
		/** A memory card image or save's file picked, copied in for the Saves page to import from. */
		ImportFile,
		/** A card image, written into the Cards folder, copied out. */
		CardImage,
		/** A zip picked, unpacked and added as the mod or pack it holds. */
		ModZip,
		/** A folder picked (a mod or pack the player unpacked), copied and added as the one it holds. */
		ModFolder,
		/**
		 * A folder picked, every disc image in it and the folders in it that the Games folder does not hold yet copied
		 * there as they lie there.
		 */
		DiscFolder,
	};

	/** Whether a transfer brings disc images into the Games folder. */
	inline bool IsDiscTransfer(EDeviceTransfer Kind)
	{
		return Kind == EDeviceTransfer::Discs || Kind == EDeviceTransfer::DiscFolder;
	}

	enum class ETransferState : uint8
	{
		Idle,
		/** The picker is open. */
		Picking,
		Copying,
		Done,
		/** The picker was closed with nothing picked. */
		Cancelled,
		Failed,
	};

	/** Why a transfer failed. */
	enum class ETransferFailure : uint8
	{
		None,
		/** The device has no room left. */
		NoRoom,
		/** The file picked could not be read. */
		Unreadable,
		/** The file could not be written. */
		Unwritable,
		/** The file picked to restore holds none of the game's saves. */
		NotABackup,
		/** The device has no file picker. */
		NoPicker,
		/** What was picked to add holds no mod or pack the game reads. */
		NotAMod,
		/** The file picked to add is not a zip (a 7z or rar, unpacked first, is added as a folder). */
		NotAZip,
		/** The folder picked holds no disc image the Games folder does not hold already. */
		NoDiscImages,
	};

	struct FTransferStatus
	{
		EDeviceTransfer Kind = EDeviceTransfer::Discs;
		ETransferState State = ETransferState::Idle;
		ETransferFailure Failure = ETransferFailure::None;
		/** The file being copied, or the one that failed: its name, its number among the files picked (from 1), and how many. */
		std::string Name;
		int32 File = 0;
		int32 Files = 0;
		/** Bytes copied of every file's; the total 0 while not known. */
		uint64 Copied = 0;
		uint64 Total = 0;
		/** The file an ImportFile transfer brought in. */
		std::filesystem::path Picked;
		/** What a ModZip or ModFolder transfer added. */
		std::optional<FAddedMod> Added;
		/** Changes whenever anything above does. */
		uint64 Revision = 0;

		bool Running() const { return State == ETransferState::Picking || State == ETransferState::Copying; }
		/** How much of every file is copied, 0 to 100. */
		uint64 Percent() const { return Total == 0 ? 0 : (Copied < Total ? Copied : Total) * 100 / Total; }
	};

	/**
	 * The host's transfers through the device's file picker, one at a time, which go on while the menu is closed; and the
	 * mods and packs they added, which the player may remove.
	 */
	class IMenuDeviceFiles
	{
	public:
		virtual ~IMenuDeviceFiles() = default;
		/** The transfer's state; its revision also changes when the mods listed do. */
		virtual FTransferStatus Status() const = 0;
		/** Opens the picker for Kind; File is the file a CardImage transfer copies out. False while another transfer runs. */
		virtual bool Start(EDeviceTransfer Kind, const std::filesystem::path& File = {}) = 0;
		/** The mods and packs on the device, as last listed. */
		virtual std::vector<FModItem> Mods() const { return {}; }
		/** The one listed at Path removed: Failed while a transfer runs. */
		virtual EModRemoval RemoveMod(const std::filesystem::path& /*Path*/) { return EModRemoval::Failed; }
	};
}
