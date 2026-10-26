#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Text/MesCharacters.h"
#include "Text/MesText.h"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	class FCdImage;

	/** One file of the player's messages: its name for the log and what it holds. */
	struct FMessageFile
	{
		std::string Name;
		std::string Contents;
	};

	/** What sizes the windows a table's messages are shown in, which says how a message of the player's is fitted. */
	enum class EMessageWindows : uint8
	{
		/**
		 * The table's own sizes (a field map's, unless a script sizes the window as it opens): sized again for the
		 * player's text, keeping the room the disc's left beyond its text for what its marks put in. A window the disc
		 * made smaller than its text (one that scrolls) keeps its size; one larger than the largest window the field
		 * draws, and than the disc's, is refused.
		 */
		Table,
		/** A script's, as each window opens (a battle event's): the reader checks the text against it then. */
		Script,
		/** The program's, or the image the text is drawn into: text wider or taller than the table's largest is logged. */
		Program,
	};

	class FMessageOverrides;

	/** The messages of a table that the player's replace, which its reader keeps to check them where their windows open. */
	struct FReplacedMessages
	{
		std::shared_ptr<const FMessageOverrides> Overrides;
		std::string Table;
		/** The replaced messages' numbers, in order, and the size of each one's text on the disc. */
		std::vector<int32> Messages;
		std::vector<FMesExtent> DiscExtents;

		/**
		 * Logs once that the replaced message Message (Codes its text) is wider or taller than the window a script
		 * opened for it, where the disc's text fitted it.
		 */
		WELTALLCORE_API void CheckWindow(int32 Message, std::span<const uint8> Codes, int32 WidthUnits, int32 Lines) const;

		/**
		 * Logs once that the replaced message's window, WidthUnits wide with what it shows beside its text (a face), is
		 * wider than the MostUnits the screen holds: it runs off the screen's edge.
		 */
		WELTALLCORE_API void CheckScreenWidth(int32 Message, int32 WidthUnits, int32 MostUnits) const;
	};

	/** A table with the player's messages in place, and which they are. */
	struct FAppliedMessages
	{
		std::vector<uint8> Table;
		FReplacedMessages Replaced;
	};

	/**
	 * The player's messages (Assets/text, Docs/Modding.md), each replacing one message of the game's text by its id
	 * (MessageTableIds.h) where its table is read: after the file that supplied the table, the disc's or a mod's.
	 * Every problem is logged once through the report: a message whose text cannot be used keeps the disc's.
	 */
	class FMessageOverrides
	{
	public:
		using FReport = std::function<void(const std::string&)>;

		/** The folder below Assets/text the export writes, which is never read for overrides. */
		static constexpr std::string_view OriginalsFolder = "original";
		/** The largest window a field map's table can size: the units and lines between the field's screen edges, its frame's 16 pixels left out. */
		static constexpr int32 MostTableWidthUnits = 70;
		static constexpr int32 MostTableLines = 13;

		/**
		 * The messages of Files, read in their order (a later file's message of an id wins), their text in Characters',
		 * measured with the widths a font sheet gives (Widths).
		 */
		WELTALLCORE_API static std::shared_ptr<const FMessageOverrides> Parse(std::span<const FMessageFile> Files, const FMesCharacters& Characters,
			FReport Report, FMesGlyphWidths Widths = {});

		/** Every .txt below Folder but its OriginalsFolder and hidden ones (IsHiddenFile), by their paths' order; none where Folder is missing. */
		WELTALLCORE_API static TResult<std::shared_ptr<const FMessageOverrides>> Read(const std::filesystem::path& Folder, const FMesCharacters& Characters,
			FReport Report, FMesGlyphWidths Widths = {});

		/**
		 * Table's bytes (a message table) with the player's messages of it in place, its windows sized as Windows says;
		 * none where it has none, or none of them can be used.
		 */
		WELTALLCORE_API static std::optional<FAppliedMessages> Apply(const std::shared_ptr<const FMessageOverrides>& Overrides, std::string_view Table,
			std::span<const uint8> Bytes, EMessageWindows Windows);

		/** How many messages the files give. */
		WELTALLCORE_API std::size_t Count() const;

		/** The player's text of a message as its file gives it, none without one. */
		WELTALLCORE_API const std::string* Text(std::string_view Table, int32 Index) const;

		/** The numbers of the messages of Table the files give, in order. */
		WELTALLCORE_API std::vector<int32> Messages(std::string_view Table) const;

		/** Logs Message once, however often it is asked for, prefixed by where the message is given. */
		WELTALLCORE_API void ReportOnce(std::string_view Table, int32 Index, const std::string& Message) const;

		/** The window a message's own glyphs need, measured with the font sheet's widths. */
		FMesExtent Extent(std::span<const uint8> Codes) const { return MesExtentOfCodes(Codes, Widths); }

	private:
		struct FOverride
		{
			std::vector<uint8> Codes;
			std::string Text;
			/** Its file and line, for the log. */
			std::string Where;
		};

		const FOverride* Find(std::string_view Table, int32 Index) const;

		std::map<std::string, std::map<int32, FOverride>, std::less<>> Tables;
		FMesGlyphWidths Widths;
		FReport Report;
		mutable std::mutex Guard;
		mutable std::set<std::string, std::less<>> Reported;
	};

	/** FMessageOverrides::Apply with the player's messages the image's asset store gives. */
	WELTALLCORE_API std::optional<FAppliedMessages> ApplyMessageOverrides(const FCdImage& Image, std::string_view Table, std::span<const uint8> Bytes,
		EMessageWindows Windows);
}
