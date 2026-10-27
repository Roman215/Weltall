#include "Game/ChapterCard.h"
#include "Game/ChapterCardDrawing.h"
#include "Game/ChapterTitles.h"
#include "Game/FieldProgram.h"
#include "Story/StoryPacks.h"
#include "Story/StorySubtitles.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/GameText.h"
#include "Support/PortStory.h"
#include "Text/MesFont.h"

#include <catch_amalgamated.hpp>

#include <string>
#include <vector>

using namespace Weltall;

namespace
{
	constexpr int32 CardVblanks = FChapterCard::RiseVblanks + FChapterCard::StayVblanks + FChapterCard::FadeVblanks;
	/** "Break the Seal", the original's chapter from Weltall-2's launch, and the first two subtitles the expansion gives it. */
	constexpr int32 BreakTheSeal = 57;
	constexpr int32 SecretBattleship = 254;
	constexpr int32 SoylentSystem = 258;

	std::string ErrorOf(std::string_view Text)
	{
		const TResult<FStorySubtitles> Subtitles = FStorySubtitles::Parse(Text);
		return Subtitles ? std::string() : Subtitles.Error().Message;
	}
}

TEST_CASE("The story's subtitles are read by their first values, each from its value to the next", "[Story]")
{
	const auto Subtitles = FStorySubtitles::Parse("; Two of them.\nsubtitles\nstory 254 Secret Battleship\n\nstory 258 Soylent System ; the second\n");
	INFO((Subtitles ? std::string() : Subtitles.Error().Message));
	REQUIRE(Subtitles);
	REQUIRE(Subtitles->All().size() == 2);
	CHECK(Subtitles->All()[1].Text == "Soylent System");
	CHECK(Subtitles->At(253) == nullptr);
	REQUIRE(Subtitles->At(254) != nullptr);
	CHECK(Subtitles->At(254)->Text == "Secret Battleship");
	CHECK(Subtitles->At(257)->First == 254);
	CHECK(Subtitles->At(400)->First == 258);

	CHECK(ErrorOf("story 254 Secret Battleship\n") == "line 1: the text begins 'subtitles'");
	CHECK(ErrorOf("subtitles\nstory 254\n") == "line 2: a subtitle reads 'story S' and its words");
	CHECK(ErrorOf("subtitles\nstory 258 A\nstory 254 B\n") == "line 3: the story's values rise from subtitle to subtitle");
	CHECK(ErrorOf("subtitles\nstory 254 A subtitle of 25 letters.\n") == "line 2: a subtitle is a line of the message font's plain characters, 24 at most");
	CHECK(ErrorOf("subtitles\nstory 254 Caf\xC3\xA9\n") == "line 2: a subtitle is a line of the message font's plain characters, 24 at most");
	CHECK(ErrorOf("subtitles\nstory 254 A subtitle of 24 letters\n").empty());

	// Numbered inside a chapter: from its first value on, the first is its part 1.
	CHECK(Subtitles->Part(254, 252) == 1);
	CHECK(Subtitles->Part(258, 252) == 2);
	CHECK(Subtitles->Part(258, 256) == 1);
	CHECK(Subtitles->Part(255, 252) == 0);
	CHECK(Subtitles->Part(254, 256) == 0);
}

TEST_CASE("The subtitles are the expanded second disc's, one document of its pack, and a game of it the only one under them", "[Story]")
{
	FStoryPacks Packs;
	CHECK(Packs.Add("additions", "subtitles.txt", "subtitles\nstory 254 Secret Battleship\n")->Message
		== "additions/subtitles.txt: the subtitles are the expanded second disc's, one document of its pack");
	REQUIRE_FALSE(Packs.Add(FStoryPacks::ExpansionPack, "subtitles.txt", "subtitles\nstory 254 Secret Battleship\n"));
	CHECK(Packs.Add(FStoryPacks::ExpansionPack, "again.txt", "subtitles\nstory 258 Soylent System\n").has_value());
	CHECK(Packs.DocumentCount() == 1);
	REQUIRE(Packs.SubtitleAt(255, true) != nullptr);
	CHECK(Packs.SubtitleAt(255, true)->First == SecretBattleship);
	CHECK(Packs.SubtitleAt(255, false) == nullptr);
}

TEST_CASE("A subtitle's card waits for the player's control under it, once each, after its chapter's own", "[Story]")
{
	FChapterCard Card;
	Card.Know(BreakTheSeal);
	// Under the first subtitle, the scripts holding the party: nothing yet; then its card, on the chapter's.
	Card.Advance(BreakTheSeal, SecretBattleship, false, 2);
	CHECK(Card.Shown() == 0);
	Card.Advance(BreakTheSeal, SecretBattleship, true, 2);
	CHECK(Card.Shown() == BreakTheSeal);
	CHECK(Card.ShownSubtitle() == SecretBattleship);
	Card.Advance(BreakTheSeal, SecretBattleship, true, CardVblanks);
	CHECK(Card.Shown() == 0);
	CHECK(Card.ShownSubtitle() == 0);
	// Shown once; the next subtitle's follows.
	Card.Advance(BreakTheSeal, SecretBattleship, true, 2);
	CHECK(Card.Shown() == 0);
	Card.Advance(BreakTheSeal, SoylentSystem, true, 2);
	CHECK(Card.ShownSubtitle() == SoylentSystem);

	// A game loaded under a subtitle shows no card for it.
	Card.Know(BreakTheSeal, SoylentSystem);
	Card.Advance(BreakTheSeal, SoylentSystem, true, 2);
	CHECK(Card.Shown() == 0);
	// A chapter's own card begun under a subtitle is the chapter's alone; the subtitle's follows it, once.
	Card.Know(BreakTheSeal - 1);
	Card.Advance(BreakTheSeal, SecretBattleship, true, 2);
	CHECK(Card.Shown() == BreakTheSeal);
	CHECK(Card.ShownSubtitle() == 0);
	Card.Advance(BreakTheSeal, SecretBattleship, true, CardVblanks);
	CHECK(Card.Shown() == 0);
	Card.Advance(BreakTheSeal, SecretBattleship, true, 2);
	CHECK(Card.ShownSubtitle() == SecretBattleship);
	Card.Advance(BreakTheSeal, SecretBattleship, true, CardVblanks);
	Card.Advance(BreakTheSeal, SecretBattleship, true, 2);
	CHECK(Card.Shown() == 0);
	// A scene's card is its chapter's alone.
	Card.Know(BreakTheSeal - 1);
	Card.Ask(BreakTheSeal);
	Card.Advance(BreakTheSeal - 1, SecretBattleship, false, 2);
	CHECK(Card.Shown() == BreakTheSeal);
	CHECK(Card.ShownSubtitle() == 0);
}

TEST_CASE("A subtitle's card is one line, its chapter's number lettered, as wide as the screen holds", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const FChapter Chapter{BreakTheSeal, 252, "Break the Seal"};
	CHECK(FChapterCardDrawing::Text(Chapter, 1, "Secret Battleship") == "Chapter 57a: Secret Battleship");
	CHECK(FChapterCardDrawing::Text(Chapter, 2, "Soylent System") == "Chapter 57b: Soylent System");

	// A line wider than a texture page: each of its pictures drawn as two sprites, the second from the page beside.
	FChapterCardDrawing Drawing;
	FDrawList Short;
	Drawing.Build(FChapterCardDrawing::Text(Chapter), FChapterCard::FullLevel, Resident.Font, Short);
	FDrawList Long;
	Drawing.Build("Chapter 59a: The Essence of Humans", FChapterCard::FullLevel, Resident.Font, Long);
	REQUIRE(Short.Primitives.size() == 5);
	REQUIRE(Long.Primitives.size() == 7);
	// The band is the chapter's own.
	CHECK(Long.Primitives[0].Corners[0].Y == Short.Primitives[0].Corners[0].Y);
	CHECK(Long.Primitives[2].Corners[2].Y == Short.Primitives[2].Corners[2].Y);
	const FDrawPrimitive& First = Long.Primitives[5];
	const FDrawPrimitive& Second = Long.Primitives[6];
	REQUIRE(First.Texture);
	REQUIRE(Second.Texture);
	CHECK(First.Texture->Page.Blend == EBlendMode::Add);
	CHECK(First.Corners[1].X - First.Corners[0].X == 256.0f);
	CHECK(Second.Corners[0].X == First.Corners[1].X);
	CHECK(Second.Texture->Page.X == 256);
	CHECK(First.Corners[0].X >= 0.0f);
	CHECK(Second.Corners[1].X <= 320.0f);
	CHECK((First.Corners[0].X + Second.Corners[1].X) / 2 == Catch::Approx(166.0f).margin(1.0f));
	// The second page holds the line's last letters.
	int32 Ink = 0;
	for (int32 Row = 0; Row < FMesFont::CellHeight; ++Row)
	{
		for (int32 Column = 256; Column < 256 + static_cast<int32>(Second.Corners[1].X - Second.Corners[0].X); ++Column)
		{
			Ink += Drawing.Pictures().At(Column, static_cast<int32>(Second.Corners[0].V) + Row) != 0 ? 1 : 0;
		}
	}
	CHECK(Ink > 20);
}

TEST_CASE("Every subtitle's card of the expanded second disc fits the screen, numbered inside its chapter", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	const FChapterTitles& Titles = Game->Session->Chapters();
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const FStoryPacks& Story = Testing::PortStory();
	FChapterCardDrawing Drawing;
	std::vector<std::string> Lines;
	for (uint16 Value = 0; Value < 0x400; ++Value)
	{
		const FStorySubtitle* Subtitle = Story.SubtitleAt(Value, true);
		const FChapter* Chapter = Titles.Find(Value);
		if (Subtitle == nullptr || Subtitle->First != Value || Chapter == nullptr)
		{
			continue;
		}
		const std::string Line = FChapterCardDrawing::Text(*Chapter, Story.SubtitlePart(Value, Chapter->FirstProgress), Subtitle->Text);
		INFO(Line);
		Lines.push_back(Line);
		FDrawList List;
		Drawing.Build(Line, FChapterCard::FullLevel, Resident.Font, List);
		for (std::size_t Sprite = 3; Sprite < List.Primitives.size(); ++Sprite)
		{
			CHECK(List.Primitives[Sprite].Corners[0].X >= 0.0f);
			CHECK(List.Primitives[Sprite].Corners[1].X <= 320.0f);
		}
	}
	REQUIRE(Lines.size() == 13);
	CHECK(Lines[0] == "Chapter 57a: Secret Battleship");
	CHECK(Lines[1] == "Chapter 57b: Soylent System");
	CHECK(Lines[2] == "Chapter 57c: The Howling");
	CHECK(Lines[4] == "Chapter 58b: Gold In The Fire");
	CHECK(Lines[7] == "Chapter 60a: The Night Before");
	CHECK(Lines[8] == "Chapter 61a: The Wreckage");
	CHECK(Lines[9] == "Chapter 62a: Razael's Tree");
}

TEST_CASE("Every subtitle of the expanded second disc lies inside one of the original's chapters, past its first value", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	const FChapterTitles& Titles = Game->Session->Chapters();
	const FStoryPacks& Story = Testing::PortStory();
	REQUIRE(Story.SubtitleAt(0x7FFF, true) != nullptr);
	for (uint16 Value = 0; Value < 0x400; ++Value)
	{
		const FStorySubtitle* Subtitle = Story.SubtitleAt(Value, true);
		if (Subtitle == nullptr || Subtitle->First != Value)
		{
			continue;
		}
		INFO(Subtitle->Text);
		const FChapter* Chapter = Titles.Find(Value);
		REQUIRE(Chapter != nullptr);
		CHECK(Chapter->FirstProgress < Value);
	}
}

TEST_CASE("On the field a game of the expanded second disc shows the subtitle it comes under once the player has control, and the original's none", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// The Mass Driver's airlock, entered with the story at the first subtitle: the chapter's card known.
	constexpr uint16 Airlock = 732;
	for (const EStoryMode Mode : {EStoryMode::Expanded, EStoryMode::Original})
	{
		auto Game = Testing::FGameLoopSession::Start(*Disc);
		Game->Session->SetStory(&Testing::PortStory());
		REQUIRE_FALSE(Game->Session->InitNewGame());
		FGameState& State = Game->Session->Game();
		State.SetStoryMode(EStoryMode::Expanded);
		State.SetSavedEventVariable(0, SecretBattleship);
		State.SetParty({1, 6, 0xFF});
		Game->EnterField(Airlock);
		State.SetStoryMode(Mode);
		Game->Session->ChapterCard().Know(BreakTheSeal);
		CHECK(Game->Session->Subtitle() == (Mode == EStoryMode::Expanded ? SecretBattleship : 0));
		constexpr int32 Frames = 300;
		int32 Subtitle = 0;
		for (int32 Frame = 0; Frame < Frames && Subtitle == 0; ++Frame)
		{
			REQUIRE_FALSE(Game->Frame());
			Subtitle = Game->Session->ChapterCard().ShownSubtitle();
		}
		INFO((Mode == EStoryMode::Expanded ? "expanded" : "original"));
		CHECK(Subtitle == (Mode == EStoryMode::Expanded ? SecretBattleship : 0));
		if (Mode == EStoryMode::Expanded)
		{
			CHECK(Game->Session->ChapterCardLine() == "Chapter 57a: Secret Battleship");
		}
	}
}
