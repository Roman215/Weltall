#include "Story/BattleEventTextPatch.h"

#include "Story/DiscIdentity.h"
#include "Text/MesText.h"
#include "Text/MessageTable.h"

#include <limits>

namespace Weltall
{
	namespace
	{
		constexpr int32 EveryPage = std::numeric_limits<int32>::max();

		/** A message as a battle shows it: its codes to its end and its window. */
		FAddedMessage ShownMessage(const FMessageTable& Table, int32 Index)
		{
			const std::span<const uint8> String = Table.String(Index);
			const std::span<const uint8> Codes = String.first(MesMessageSize(String));
			return {std::vector<uint8>(Codes.begin(), Codes.end()), static_cast<uint8>(Table.WidthUnits(Index)), static_cast<uint8>(Table.Lines(Index))};
		}

		bool operator==(const FAddedMessage& A, const FAddedMessage& B)
		{
			return A.Codes == B.Codes && A.WidthUnits == B.WidthUnits && A.Lines == B.Lines;
		}

		/**
		 * The lines that begin the disc's message K as Mine where it is a later page of it, or pages of the author's told
		 * in its window and then the disc's from a page on; none where it is neither.
		 */
		std::optional<std::string> BeginningOf(const FMessageTable& Disc, int32 K, const FAddedMessage& Mine)
		{
			if (Disc.StartsPastBytes(K) || Mine.WidthUnits != Disc.WidthUnits(K) || Mine.Lines != Disc.Lines(K))
			{
				return std::nullopt;
			}
			const std::string Line = "message " + std::to_string(K) + " from page ";
			for (int32 Page = 1;; ++Page)
			{
				const std::optional<std::vector<uint8>> Rest = MesPagesOfCodes(Disc.String(K), Page, EveryPage);
				if (!Rest)
				{
					return std::nullopt;
				}
				if (Page > 1 && *Rest == Mine.Codes)
				{
					return Line + std::to_string(Page) + "\n";
				}
				for (int32 Told = 1;; ++Told)
				{
					const std::optional<std::vector<uint8>> Before = MesPagesOfCodes(Mine.Codes, 1, Told);
					if (!Before || Before->size() >= Mine.Codes.size())
					{
						break;
					}
					const FMesExtent Extent = MesExtentOfCodes(*Before);
					if (MesPagesJoined(*Before, *Rest) == Mine.Codes && Extent.WidthUnits <= Mine.WidthUnits && Extent.Lines <= Mine.Lines)
					{
						return Line + std::to_string(Page) + " after\n{\n" + MesTextOfCodes(*Before) + "\n}\n";
					}
				}
			}
		}
	}

	TResult<std::string> WriteShareableBattleEventText(const FBattleEventTextPatch& Whole, std::span<const uint8> DiscMessages)
	{
		const std::optional<FSha1::FDigest> Identity = MessageTableIdentity(DiscMessages);
		const TResult<FMessageTable> Disc = FMessageTable::Parse(DiscMessages);
		if (!Whole.IsWhole() || !Identity || !Disc || Identity != Whole.WrittenFor())
		{
			return MakeError(Whole.IsWhole() ? "it was written for another disc's table than this disc's" : "it does not give a battle event's table whole");
		}
		const std::vector<FAddedMessage>& Given = Whole.WholeTable();
		if (static_cast<int32>(Given.size()) != Disc->Count())
		{
			return MakeError("it has " + std::to_string(Given.size()) + " messages where the disc's table has " + std::to_string(Disc->Count())
				+ ": the battle's scripts name the disc's");
		}
		std::string Lines;
		for (int32 K = 0; K < Disc->Count(); ++K)
		{
			const FAddedMessage& Mine = Given[static_cast<std::size_t>(K)];
			if (!Disc->StartsPastBytes(K) && Mine == ShownMessage(*Disc, K))
			{
				continue;
			}
			if (const std::optional<std::string> Beginning = BeginningOf(*Disc, K, Mine))
			{
				Lines += *Beginning;
				continue;
			}
			Lines += "message " + std::to_string(K) + " anew size " + std::to_string(Mine.WidthUnits) + " " + std::to_string(Mine.Lines) + "\n{\n"
				+ MesTextOfCodes(Mine.Codes) + "\n}\n";
		}
		if (Lines.empty())
		{
			return std::string();
		}
		const std::string Text = "battle event " + std::to_string(Whole.Event()) + "\ndisc " + FSha1::ToHex(*Identity) + "\n" + Lines;

		// The text on the disc's table must give the whole table's messages.
		const TResult<FBattleEventTextPatch> Written = FBattleEventTextPatch::Parse(Text);
		const std::optional<std::vector<uint8>> Applied = Written ? Written->Apply(DiscMessages, &*Identity) : std::nullopt;
		const TResult<FMessageTable> Table = Applied ? FMessageTable::Parse(*Applied) : TResult<FMessageTable>(MakeError(""));
		if (!Table || Table->Count() != Disc->Count())
		{
			return MakeError("the text written for it does not go on the disc's table");
		}
		for (int32 K = 0; K < Table->Count(); ++K)
		{
			if (!(ShownMessage(*Table, K) == Given[static_cast<std::size_t>(K)]))
			{
				return MakeError("the text written for it would not give its message " + std::to_string(K) + " as it does");
			}
		}
		return Text;
	}
}
