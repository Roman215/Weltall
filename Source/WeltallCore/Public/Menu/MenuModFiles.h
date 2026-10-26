#pragma once

#include "Foundation/Types.h"
#include "Menu/MenuDeviceFiles.h"
#include "Menu/MenuSettingsPages.h"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Weltall
{
	/**
	 * The Modding page's rows for a device whose game folders only the game reaches (Android): under their heading, a
	 * mod or pack added from a zip or a folder picked through the device's file picker, then each mod and pack on the
	 * device with its size, removed on a second Confirm. A transfer's progress, then what it added, is told on the row
	 * that started it; a removal on the row the cursor is left on.
	 */
	class FMenuModFiles
	{
	public:
		/** Files must outlive the rows. */
		WELTALLCORE_API explicit FMenuModFiles(IMenuDeviceFiles& InFiles);

		/** The rows from their heading on. */
		WELTALLCORE_API std::vector<FSettingsRow> Rows() const;
		/** Confirm on Row, counted from the heading: false where it does nothing. */
		WELTALLCORE_API bool Activate(int32 Row);
		/** The cursor left its row: what the rows told is gone, and a removal asked about is not done. */
		WELTALLCORE_API void CursorMoved();
		/** Follows the device's transfer and its mods: true when the rows changed. */
		WELTALLCORE_API bool Refresh();
		/** What the transfer the rows last saw end added, once: the player added it to use it, so the screen switches it on. */
		std::optional<FAddedMod> TakeAdded() { return std::exchange(JustAdded, std::nullopt); }

	private:
		bool StartTransfer(EDeviceTransfer Kind);
		void TellTransfer(std::vector<FSettingsRow>& Rows) const;

		IMenuDeviceFiles& Files;
		/** The mods as the rows show them, listed again when the device's revision changes. */
		std::vector<FModItem> Items;
		/** The transfer the rows started or saw run, whose end they tell; the revision last followed. */
		std::optional<EDeviceTransfer> Followed;
		uint64 SeenRevision = 0;
		/** The end of the transfer that ran last was seen (a transfer that ended before the rows were made counts as seen). */
		bool bEndSeen = true;
		std::optional<FAddedMod> JustAdded;
		/** The row whose removal waits for a second Confirm. */
		int32 AskedRow = -1;
		int32 ToldRow = -1;
		std::array<std::string, 3> Told;
	};
}
