#include "Story/DiscIdentity.h"

#include "Text/MessageTable.h"

#include <algorithm>
#include <array>

namespace Weltall
{
	namespace
	{
		constexpr std::string_view MarkWord = "exported";
		constexpr std::string_view ByteOrderMark = "\xEF\xBB\xBF";
		constexpr std::string_view Blanks = " \t\r";

		void AddWord(FSha1& Hash, uint32 Value)
		{
			const std::array<uint8, 4> Bytes = {static_cast<uint8>(Value), static_cast<uint8>(Value >> 8), static_cast<uint8>(Value >> 16), static_cast<uint8>(Value >> 24)};
			Hash.Update(Bytes);
		}

		/** Each message's codes to its zero byte and its window, after their count; false where the bytes are not a table. */
		bool AddMessages(FSha1& Hash, std::span<const uint8> Messages)
		{
			const TResult<FMessageTable> Table = FMessageTable::Parse(Messages);
			if (!Table)
			{
				return false;
			}
			AddWord(Hash, static_cast<uint32>(Table->Count()));
			for (int32 Message = 0; Message < Table->Count(); ++Message)
			{
				const std::span<const uint8> Rest = Table->String(Message);
				const std::span<const uint8> Codes = Rest.first(std::min(static_cast<std::size_t>(std::find(Rest.begin(), Rest.end(), 0) - Rest.begin()) + 1, Rest.size()));
				const std::array<uint8, 3> Window = {static_cast<uint8>(Table->WidthUnits(Message)), static_cast<uint8>(Table->Lines(Message)),
					static_cast<uint8>(Table->StartsPastBytes(Message) ? 1 : 0)};
				Hash.Update(Window);
				AddWord(Hash, static_cast<uint32>(Codes.size()));
				Hash.Update(Codes);
			}
			return true;
		}

		/** Adds Text to Hash without its carriage returns. */
		void AddText(FSha1& Hash, std::string_view Text)
		{
			for (std::size_t Return = Text.find('\r'); !Text.empty(); Return = Text.find('\r'))
			{
				const std::string_view Run = Text.substr(0, Return);
				Hash.Update(std::span<const uint8>(reinterpret_cast<const uint8*>(Run.data()), Run.size()));
				Text.remove_prefix(std::min(Return == std::string_view::npos ? Text.size() : Return + 1, Text.size()));
			}
		}

		std::string_view Trimmed(std::string_view Line)
		{
			Line.remove_prefix(std::min(Line.find_first_not_of(Blanks), Line.size()));
			Line.remove_suffix(Line.size() - std::min(Line.find_last_not_of(Blanks) + 1, Line.size()));
			return Line;
		}

		/** The digest a mark line names; none for any other line. */
		std::optional<FSha1::FDigest> MarkOf(std::string_view Line)
		{
			Line = Trimmed(Line);
			if (!Line.starts_with(MarkWord) || Line.size() <= MarkWord.size() || (Line[MarkWord.size()] != ' ' && Line[MarkWord.size()] != '\t'))
			{
				return std::nullopt;
			}
			return FSha1::FromHex(Trimmed(Line.substr(MarkWord.size())));
		}
	}

	std::optional<FSha1::FDigest> FieldMapIdentity(std::span<const uint8> EventScripts, std::span<const uint8> Messages)
	{
		FSha1 Hash;
		AddWord(Hash, static_cast<uint32>(EventScripts.size()));
		Hash.Update(EventScripts);
		return AddMessages(Hash, Messages) ? std::optional<FSha1::FDigest>(Hash.Finish()) : std::nullopt;
	}

	std::optional<FSha1::FDigest> MessageTableIdentity(std::span<const uint8> Messages)
	{
		FSha1 Hash;
		return AddMessages(Hash, Messages) ? std::optional<FSha1::FDigest>(Hash.Finish()) : std::nullopt;
	}

	namespace ExportMark
	{
		std::string Marked(std::string_view Head, std::string_view Body)
		{
			FSha1 Hash;
			AddText(Hash, Head);
			AddText(Hash, Body);
			std::string Text(Head);
			Text += MarkWord;
			Text += ' ';
			Text += FSha1::ToHex(Hash.Finish());
			Text += '\n';
			Text += Body;
			return Text;
		}

		bool LeftAsWritten(std::string_view Text)
		{
			if (Text.starts_with(ByteOrderMark))
			{
				Text.remove_prefix(ByteOrderMark.size());
			}
			for (std::size_t Start = 0; Start < Text.size();)
			{
				const std::size_t End = std::min(Text.find('\n', Start), Text.size());
				if (const std::optional<FSha1::FDigest> Mark = MarkOf(Text.substr(Start, End - Start)))
				{
					FSha1 Hash;
					AddText(Hash, Text.substr(0, Start));
					AddText(Hash, Text.substr(std::min(End + 1, Text.size())));
					return Hash.Finish() == *Mark;
				}
				Start = End + 1;
			}
			return false;
		}

		bool IsMark(std::string_view Line)
		{
			return MarkOf(Line).has_value();
		}
	}
}
