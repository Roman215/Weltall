#include "Field/EventAssembly.h"
#include "Field/EventInstructionSet.h"
#include "Field/FieldMap.h"
#include "Field/FieldScriptFixes.h"
#include "Story/DiscText.h"
#include "Story/StoryPacks.h"
#include "Support/FieldOracleMap.h"
#include "Support/GameDiscs.h"
#include "Support/PortStory.h"
#include "Text/MesText.h"
#include "Text/MessageTable.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

using namespace Weltall;

namespace
{
	/**
	 * A window sized to its text keeps to the screen's margins, 12 to 308 of its 320 pixels, at 70 units a line: four
	 * pixels a unit, beside 16 of border. A portrait takes 17 units of it, and the text beside one counts as 24 at
	 * least. A window of the fixed width is 72 units, whatever its text. Either shows four lines a page.
	 */
	constexpr int32 SizedUnits = 70;
	constexpr int32 PortraitUnits = 17;
	constexpr int32 PortraitLeastText = 24;
	constexpr int32 FixedUnits = 72;
	constexpr int32 FourLines = 4;

	constexpr uint16 MessageBox = 0x03;
	constexpr uint16 MessageFixed = 0xD3;
	constexpr uint16 MessageAtActor = 0xD4;
	constexpr uint16 MessageAtActorPortrait = 0xFC;
	constexpr uint16 SetMessageFace = 0xFE0D;
	constexpr std::array<uint16, 6> WindowOpeners = {MessageBox, 0xD2, MessageFixed, MessageAtActor, 0xF5, MessageAtActorPortrait};
	/** The actor operand's running actor, and the first of the party's slots (FD to FF). */
	constexpr int32 RunningActor = 0xFB;
	constexpr int32 FirstPartySlot = 0xFD;

	struct FOpening
	{
		int32 Actor = 0;
		uint16 Opcode = 0;
		int32 Speaker = 0;
		int32 Message = 0;
	};

	/**
	 * The messages from First on that a map's scripts open in a window that cannot hold them, the screen's margins
	 * kept: each as "message N, opened by actor A: W units a line, U fit" or "...: L lines a page, 4 fit". A window shows
	 * a portrait where the actor opening it has a face: one its scripts set (SetMessageFace) or take from the actor
	 * spoken at (MessageAtActorPortrait), a party slot's actor's always. A size a script gives its windows is not
	 * followed: the port's own windows size themselves.
	 */
	std::vector<std::string> MessagesUnheld(const FEventScripts& Scripts, const FMessageTable& Messages, int32 First, const std::set<int32>& OfDisc = {})
	{
		const std::span<const uint8> Code(Scripts.Code(), Scripts.CodeSize());
		std::vector<bool> Faces(static_cast<std::size_t>(Scripts.ActorCount()), false);
		std::vector<FOpening> Openings;
		for (int32 Actor = 0; Actor < Scripts.ActorCount(); ++Actor)
		{
			for (const uint16 Offset : FollowEventThreads(Scripts, Actor).Reached)
			{
				const std::optional<FEventInstruction> Instruction = ReadEventInstruction(Code, Offset);
				REQUIRE(Instruction);
				const uint16 Opcode = Instruction->Shape->Opcode;
				if (Opcode == SetMessageFace || Opcode == MessageAtActorPortrait)
				{
					Faces[static_cast<std::size_t>(Actor)] = true;
				}
				if (std::find(WindowOpeners.begin(), WindowOpeners.end(), Opcode) == WindowOpeners.end())
				{
					continue;
				}
				const bool bAtActor = Opcode == MessageAtActor || Opcode == MessageAtActorPortrait;
				Openings.push_back({Actor, Opcode, bAtActor ? Instruction->Values[0] : RunningActor, Instruction->Values[bAtActor ? 1 : 0]});
			}
		}
		const auto HasFace = [&Faces, &Scripts](int32 Operand, int32 Running) {
			const int32 Actor = Operand == RunningActor ? Running : Operand;
			return Actor >= FirstPartySlot || (Actor < Scripts.ActorCount() && Faces[static_cast<std::size_t>(Actor)]);
		};
		std::vector<std::string> Unheld;
		for (const FOpening& Opening : Openings)
		{
			// The messages before First, and those named of the disc's own, are the game's: shown as the game shows them.
			if (Opening.Message < First || Opening.Message >= Messages.Count() || OfDisc.count(Opening.Message) != 0)
			{
				continue;
			}
			// The window's portrait is the opening actor's own, or the one it takes from the actor spoken at.
			const bool bPortrait = HasFace(Opening.Opcode == MessageAtActorPortrait ? Opening.Speaker : RunningActor, Opening.Actor);
			const int32 Width = Messages.WidthUnits(Opening.Message);
			const bool bFixed = Opening.Opcode == MessageBox || Opening.Opcode == MessageFixed;
			const int32 Fit = (bFixed ? FixedUnits : SizedUnits) - (bPortrait ? PortraitUnits : 0);
			const int32 Taken = bPortrait && !bFixed ? std::max(Width, PortraitLeastText) : Width;
			const std::string Which = "message " + std::to_string(Opening.Message) + ", opened by actor " + std::to_string(Opening.Actor) + ": ";
			if (Taken > Fit)
			{
				Unheld.push_back(Which + std::to_string(Width) + " units a line, " + std::to_string(Fit) + " fit");
			}
			if ((bPortrait || bFixed) && Messages.Lines(Opening.Message) > FourLines)
			{
				Unheld.push_back(Which + std::to_string(Messages.Lines(Opening.Message)) + " lines a page, 4 fit");
			}
		}
		std::sort(Unheld.begin(), Unheld.end());
		Unheld.erase(std::unique(Unheld.begin(), Unheld.end()), Unheld.end());
		return Unheld;
	}

	/** A message table of Texts, for FMessageTable to view. */
	std::vector<uint8> TableBytes(std::initializer_list<std::string_view> Texts)
	{
		std::vector<FAddedMessage> Added;
		for (const std::string_view Text : Texts)
		{
			const std::optional<FAddedMessage> Message = MesMessageOfText(Text);
			REQUIRE(Message);
			Added.push_back(*Message);
		}
		std::optional<std::vector<uint8>> Bytes = BuildMessageTable(Added);
		REQUIRE(Bytes);
		return std::move(*Bytes);
	}
}

TEST_CASE("A message's window holds 35 letters a line, 26 and four lines a page beside a portrait, 36 and 27 at the fixed width", "[Story]")
{
	// Actor 0 has a face, actor 1 none, actor 2 none but takes actor 0's to speak at it.
	const auto Section = AssembleEventScripts(
		"actors 3\n"
		"entrances\n"
		"actor 0 script 0:\n"
		"    SetMessageFace #1\n"
		"    Message msg0 0\n"
		"    Message msg1 0\n"
		"    MessageFixed msg4 0\n"
		"    Message msg6 0\n"
		"    End\n"
		"actor 1 script 0:\n"
		"    Message msg2 0\n"
		"    Message msg6 0\n"
		"    MessageAtActor actor0 msg3 0\n"
		"    MessageBox msg5 0\n"
		"    End\n"
		"actor 2 script 0:\n"
		"    MessageAtActorPortrait actor0 msg1 0\n"
		"    MessageAtActorPortrait party1 msg0 0\n"
		"    End\n");
	REQUIRE(Section);
	const auto Scripts = FEventScripts::Parse(*Section);
	REQUIRE(Scripts);
	const std::vector<uint8> Bytes = TableBytes({
		"Twenty-six letters a line.",
		"Twenty-seven letters a line",
		"Thirty-five letters fill this line.",
		"Thirty-six letters are one too many.",
		"Twenty-eight letters a line.",
		"Thirty-six letters fill a fixed one.",
		"One\nTwo\nThree\nFour\nFive",
	});
	const auto Messages = FMessageTable::Parse(Bytes);
	REQUIRE(Messages);
	CHECK(MessagesUnheld(*Scripts, *Messages, 0) == std::vector<std::string>{
		"message 1, opened by actor 0: 54 units a line, 53 fit",
		"message 1, opened by actor 2: 54 units a line, 53 fit",
		"message 3, opened by actor 1: 72 units a line, 70 fit",
		"message 4, opened by actor 0: 56 units a line, 55 fit",
		"message 6, opened by actor 0: 5 lines a page, 4 fit",
	});
	// The messages before First are the map's own.
	CHECK(MessagesUnheld(*Scripts, *Messages, 5) == std::vector<std::string>{
		"message 6, opened by actor 0: 5 lines a page, 4 fit",
	});
}

TEST_CASE("Every message the port's story adds opens a window that holds it, beside its speaker's portrait too", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FStoryPacks& Story = Testing::PortStory();
	const FGameDiscText DiscText(Disc->Image(), Files->Index);
	std::set<std::pair<int32, bool>> Built;
	for (const FStoryDocument& Document : Story.Documents())
	{
		const int32 Map = Document.Patch.Map();
		if (!Built.insert({Map, Document.bExpansion}).second)
		{
			continue;
		}
		INFO(Document.Name);
		// The map as the field loads it, every document for it added, each under its own setting.
		FEventOptions Options = FEventOptions::Original();
		for (const FStoryDocument& Other : Story.Documents())
		{
			if (Other.Patch.Map() == Map && Other.Option != nullptr)
			{
				Options.*Other.Option = true;
			}
		}
		const FStoryDocument* Own = Story.MapOfItsOwn(Map, Document.bExpansion);
		auto Loaded = LoadFieldMap(Disc->Image(), Files->Index, Own != nullptr ? *Own->Patch.Base() : Map);
		REQUIRE(Loaded);
		const bool bOverBase = Own != nullptr && Own->Patch.KeepsBaseScripts();
		if (Own != nullptr && !bOverBase)
		{
			auto Made = Own->Patch.Build(&DiscText);
			INFO((Made ? std::string() : Made.Error().Message));
			REQUIRE(Made);
			Loaded->EventScripts = std::move(Made->EventScripts);
			Loaded->Messages = std::move(Made->Messages);
		}
		// A disc map's own messages come first, and are the game's.
		const auto DiscMessages = FMessageTable::Parse(Loaded->Messages);
		REQUIRE(DiscMessages);
		const int32 First = Own != nullptr && !bOverBase ? 0 : DiscMessages->Count();
		auto Scripts = FEventScripts::Parse(Loaded->EventScripts);
		REQUIRE(Scripts);
		ApplyFieldScriptFixes(bOverBase ? *Own->Patch.Base() : Map, *Scripts, Options);
		if (bOverBase)
		{
			const TResult<bool> Based = Own->Patch.Apply(*Scripts, Loaded->Messages, &DiscText);
			REQUIRE(Based);
			REQUIRE(*Based);
		}
		REQUIRE_FALSE(Story.ApplyToMap(Map, *Scripts, Loaded->Messages, Options, Document.bExpansion, &DiscText));
		const auto Messages = FMessageTable::Parse(Loaded->Messages);
		REQUIRE(Messages);
		// The disc's own messages a document names are the game's too: those of the map's maker, or of the map's first document.
		std::set<int32> OfDisc;
		const FFieldMapPatch& Naming = Own != nullptr && !bOverBase ? Own->Patch : Document.Patch;
		for (const int32 Named : Naming.DiscMessages())
		{
			OfDisc.insert(First + Named);
		}
		CHECK(MessagesUnheld(*Scripts, *Messages, First, OfDisc) == std::vector<std::string>{});
	}
}
