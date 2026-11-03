#pragma once

#include "Foundation/Types.h"
#include "Menu/MenuSettingsPages.h"

#include <optional>
#include <string_view>
#include <vector>

namespace Weltall
{
	namespace AssetRow
	{
		inline constexpr int32 External = 0, Capture = 1, Textures = 8, Text = 9, Dump = 10, Music = 11, Status = 12;
		/** The texture packs' kinds of picture, each shown or not: FTexturePackKinds in its order. */
		inline constexpr int32 FirstPackKind = 2, PackKinds = 6;
	}

	enum class EAssetExportState : uint8 { Idle, Running, Complete, Cancelled, Failed, Unavailable };

	/**
	 * What an export makes: PNGs of every texture, a native dump of the discs, WAVs of the songs, the text and its font,
	 * the second disc's maps and battle text as an expansion to edit, or a Disc 2 expansion's copy with only what its
	 * author changed (the last two the Modding page's).
	 */
	enum class EAssetExportKind : uint8 { Textures, Dump, Music, Text, Disc2, Shareable };

	/** What asking for an expansion's shareable copy came to: started (the export's progress tells the rest), or nothing, its copy being there already. */
	enum class EShareableStart : uint8 { Started, CopyThere };

	struct FMenuAssetProgress
	{
		EAssetExportState State = EAssetExportState::Idle;
		EAssetExportKind Kind = EAssetExportKind::Dump;
		/** Bytes for a dump, songs for music, the texture catalogue's items for textures, files for the text and for Disc 2. */
		uint64 Completed = 0;
		uint64 Total = 0;
		/** Changes when the status shown by the menu changes. */
		uint64 Revision = 0;
	};

	/** The host's incremental asset exports, one at a time, which continue while the menu is closed. */
	class IMenuAssets
	{
	public:
		virtual ~IMenuAssets() = default;
		virtual FMenuAssetProgress Progress() const = 0;
		virtual bool StartExport(EAssetExportKind Kind) = 0;
		/**
		 * Starts Make shareable (FShareableExpansion) of the expansion in the Mods folder's folder Folder as the export that
		 * runs, replacing its copy only with bReplace.
		 */
		virtual EShareableStart StartShareable(std::string_view Folder, bool bReplace) = 0;
		virtual void CancelExport() = 0;
	};

	/** The texture packs' kind of picture a row shows or hides, null for another row. */
	WELTALLCORE_API bool* AssetRowPackKind(FTexturePackKinds& Kinds, int32 Row);

	/** The export an action row starts, none for another row. */
	WELTALLCORE_API std::optional<EAssetExportKind> AssetRowExport(int32 Row);

	WELTALLCORE_API std::vector<FSettingsRow> AssetSettingsRows(const FPlayerSettings& Settings, FMenuAssetProgress Progress);
}
