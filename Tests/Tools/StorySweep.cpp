// Plays every field map's scenes by machine, to find where the game stops or a scene never gives the party back: each
// map entered at each value of the story's progress its scripts tell apart, from the state the story has by then, and
// played on with the confirm button tapped through whatever its scripts start by themselves: the scenes, their map
// changes, the movies, the world map's cutscenes, the battles (fought with a random pad).
//
//   WeltallStorySweep [FIRST [LAST]] [-cards=FOLDER] [-before] [-triggers | -walked] [-choice=N] [-at=STORY] [-set=CHANGES]
//       [-frames=N] [-battle=N] [-shard=I/N] [-expanded] [-list] [-trace=N] [-watch=A] [-reversed] [-holds=N] [-strong] [-clear]
//       [-keepelly] [-id]
//   WeltallStorySweep -explore=N [FIRST [LAST]] [-cards=FOLDER] [-choice=N] [-frames=N] [-battle=N] [-shard=I/N] [-expanded] [-trace=N]
//       [-watch=A] [-reversed] [-holds=N] [-strong]
//
// Field maps FIRST to LAST of disc 1 (0 to 729). The state the story has at a value is the last save of the memory card
// images in FOLDER it has reached by then, with the progress set to the value (without -cards, or before the first
// save, a new game's state), or with -before the last save from before it reached the value; the map is entered by
// its first entrance. A state made this way is not always one the story has: a scene written for a party the save
// lacks, or one the save already has behind it, stops or waits for ever, and tells nothing of the game. A scene that
// plays through from either save is sound.
//
// With -triggers, once a map's own scenes have given the party to the player, every talk and touch its actors answer is
// begun in turn, each talk twice, as a player begins it: the leader stood near the actor on floor its walk reaches,
// then, once the view shows the actor, beside it and facing it, the talk's button pressed or the touch left to the
// game. Where that starts nothing (an actor across a gap or on another level, one that wants another approach) the
// script is started outright and its line says "forced". Then the leader is stood in each of the map's zones in turn,
// which the scripts test it against for a way out or a scene's place ("area Z"). With -walked instead, nothing is
// started outright: each is begun only as the exploring below begins it, and what no walk of the leader's reaches
// says so, with how far the walk gets and who stands in its way.
//
// With -explore=N the game is played on from states it made itself instead: a new game, then each save of FOLDER, on a
// field map or the world map, each explored for at most N places (Support/StorySweepExplore.h). Its lines' first two
// columns are the place a stretch began at (a field map, or from 1024 up the world map), and the save's column the
// progress of the state the place was entered from; no talk or touch is started outright there, so every line is a
// player's: "unanswered" where the game's own check starts none, "unreached" where no walk of the leader's gets there
// without meeting something else first (a body in the way, another actor's touch not met yet), or at all. A place is
// gone on from by entering its map anew ("scene"); where that does not give the party back ("re-entry": a map whose
// entry reads what the stay changed) the place is come to again by the trigger that first led there ("arrival"). On the
// world map the stretches are "place L.R", the party stood in record R of list L (0 on foot, 1 in gears, 2 aboard)
// where its travel reaches, and the place entered. A floor of the map no walk reaches ("floor L.T", the walking layer
// and the floor's first triangle) is tried from the rim of the ground that is reached, by a walk, a jump and a running
// jump toward it, or by the floor that hands the walker up to it; what stands on it is then walked to by the ways
// found. A talk that said something new is begun again ("talk A again"). A place is a map at a value of the
// progress as the saved variables its scripts test stand, come onto by one of its floors: come back to with one of
// those changed, or by another floor, it is explored again. In a line's path "voyage" is the ship sailing by itself
// on the world map. FIRST and LAST keep the states whose progress lies between them;
// -shard=I/N explores every Nth of those kept.
//
// -choice=N answers every choice with its entry N (0 the first; a question that comes back is given its last entry
// after two of the same, then the one before, so a scene that asks until it hears "no" does and a list is left by
// its way out); -at=STORY plays that value alone;
// -set=CHANGES changes saved variables before each map is entered, to give a state a flag the save lacks: a comma-separated
// list of a variable's byte address and a value, both hexadecimal, joined by "=" (set), "|" (set these bits) or "-" (clear
// them), as 1C2-3; -frames=N
// lets a stretch of scenes go on for N frames (36000, twenty minutes; 360000 when exploring: a long one follows the
// story as far as it plays by itself); -battle=N fights a battle for at most N frames (12000; 24000 when exploring)
// before the stretch ends with it ("battle runs"); -shard=I/N plays every Nth map from the Ith; -expanded tells the second
// disc's story with the port's own documents; -list prints the maps and their values without playing; -trace=N writes what the scripts wait at every N
// frames to the error stream (first whether the drawing takes the tick for a cutscene and the member each party slot
// is drawn as, -1 its own), with -watch=A also where actor A stands and how it moves (-watch=leader: whoever the pad
// walks). -strong makes every character and gear far stronger than the story asks as each map is entered, so that the
// battles the random pad fights are won and the play goes on past them. -clear plays each state as a clear file's
// (ClearFile.h): Elly back in the party in Regurus, in the second slot where she is not in it. -keepelly plays each
// state as a Keep Elly game's (KeepElly.h), the game's own rule bringing her back where it does, in the second slot
// where the rule has her back and she is not in the party. -id draws Fei as Id (CharacterLooks.h) in a New Game+, his
// battles on foot played with Id's sprite and presses, staged ones too.
//
// -reversed keeps the party in the order of its people from the last to the first, so that a party of two or more is
// drawn in another order than the story's wherever the drawing shows the player's; -holds=N writes each hold of N
// frames or more that was drawn so to the error stream as it ends ("HOLD", its map, the progress, its frames, the
// windows opened in it).
//
// Prints a line a stretch of play, tab-separated: the map, the story's value, what began the stretch ("scene", "talk
// A", "talk A again", "touch A", "floor L.T", "area Z": A the actor, Z the zone; "forced" as above, "unanswered" where nothing
// started, the view never showed the actor or the zone has no floor the leader reaches), the save's progress, the
// party, how it ended, the map and the progress it ended at, its frames, the maps and programs it went through, what
// the game said or the scripts wait at (the lock, "free" where the player has the party that frame, the windows open,
// each as "window owner>speaker" with its flags and what it waits for, then each thread as "actor.script @offset
// instruction (x,z)", an actor's own thread without a script or a place), the cases the port thought no script
// reaches that it met, and what the drawing made of the
// staging: "unstaged N on MAP", the longest hold of two seconds or more (the scripts holding the party, or a map
// without its leader, outside a talk) that it drew as play (with -reversed: in which a slot was drawn as another
// member), and "K short", the cutscenes it began and ended within two seconds on one map; last, the story's saved
// variables it changed that the scripts of the map it began or ended on test, as "102h 3>4". The ends: "control" (the player
// has the party: for a moment's quiet, or again and again while the scripts go round where they are, as on a floor
// that hurts whoever stands on it), "held" (the scripts keep it and have gone nowhere for a minute: no thread at an
// instruction new to the stretch, no window opened or closed; the other buttons were tried), "STOPS" (the game
// stopped), "world map", "menu" (one that neither
// cancelling nor confirming closes), "battle runs", "battle lost", "arena", "loops" (the scenes keep coming back to
// one map), "long" (the stretch's frames ran out, or five minutes passed on one map with the story where it was). A
// run plays the same frames every time. The discs are the checkout's, in its Games folder.

#include "Media/GameLibrary.h"
#include "Mods/ExpansionMods.h"
#include "Story/StoryPacks.h"
#include "Support/PlayedGame.h"
#include "Support/StorySweepExplore.h"
#include "Tools/StorySweepList.h"
#include "Support/StorySweepRun.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

using namespace Weltall;
using namespace Weltall::Testing;

namespace
{
	int Fail(const std::string& Why)
	{
		std::fprintf(stderr, "%s\n", Why.c_str());
		return 1;
	}

	/** The frames of a hold drawn as play from which a line tells it. */
	constexpr int32 UnstagedFramesTold = 60;

	/** The frames a stretch of an exploration may take: three hours of the game. */
	constexpr int32 ExploredStretchFrames = 360000;
	/** The frames an exploration's battle is fought for: the random pad takes its time over a story's boss. */
	constexpr int32 ExploredBattleFrames = 24000;

	struct FRow
	{
		int32 Map = 0;
		uint16 Story = 0;
		std::string Start;
	};

	void Print(const FRow& Row, const FStorySweepRun& Run, const FStorySweepStretch& Stretch)
	{
		std::string Unreached;
		for (const std::string& Case : Stretch.Unreached)
		{
			Unreached += Unreached.empty() ? Case : " | " + Case;
		}
		// The staging the drawing missed or flickered over: holds of two seconds or more drawn as play, and cutscenes soon over.
		std::string Staging;
		if (Stretch.UnstagedFrames >= UnstagedFramesTold)
		{
			Staging = "unstaged " + std::to_string(Stretch.UnstagedFrames) + " on " + std::to_string(Stretch.UnstagedMap);
		}
		if (Stretch.ShortCutscenes != 0)
		{
			Staging += (Staging.empty() ? "" : "; ") + std::to_string(Stretch.ShortCutscenes) + " short";
		}
		const std::string Detail = Stretch.Detail + (Stretch.Detail.empty() || Stretch.Barred.empty() ? "" : "; ") + Stretch.Barred;
		std::printf("%d\t%d\t%s\t%d\t%s\t%s\t%d\t%d\t%d\t%s\t%s\t%s\t%s\t%s\n", Row.Map, int32{Row.Story}, Row.Start.c_str(), Run.SaveStory(), Run.Party().c_str(),
			StorySweepEndName(Stretch.End), Stretch.Map, int32{Stretch.Story}, Stretch.Frames, Stretch.Path.c_str(), Detail.c_str(), Unreached.c_str(),
			Staging.c_str(), Stretch.Changed.c_str());
		std::fflush(stdout);
	}

	void PrintUnplayed(const FRow& Row, const FError& Error)
	{
		std::printf("%d\t%d\t%s\t\t\tUNPLAYED\t\t\t0\t\t%s\t\n", Row.Map, int32{Row.Story}, Row.Start.c_str(), Error.Message.c_str());
		std::fflush(stdout);
	}

	/** A map's scenes at a story value, then (with triggers) each talk and touch of the map as the scenes leave it. */
	void PlayMap(FStorySweepRun& Run, int32 Map, uint16 Story, const FStorySweepOptions& Options, bool bTriggers)
	{
		// The map entered and its own scenes played: whether the player has the party there afterwards, the story where it was.
		const auto Arrive = [&](bool bReported)
		{
			if (auto Error = Run.Enter(Map, Story))
			{
				PrintUnplayed({Map, Story, "scene"}, *Error);
				return false;
			}
			const FStorySweepStretch Scene = Run.Play(Options.Scenes);
			if (bReported)
			{
				Print({Map, Story, "scene"}, Run, Scene);
			}
			return Scene.End == EStorySweepEnd::Control && Scene.Map == Map && Scene.Story == Story;
		};
		if (!Arrive(true) || !bTriggers)
		{
			return;
		}
		const bool bRoomy = !Run.IsHemmedIn();
		for (const FStorySweepTrigger& Trigger : Run.Triggers())
		{
			const std::string Name = Trigger.Name();
			for (int32 Pass = 0; Pass < (Trigger.Kind == EStorySweepTrigger::Talk ? 2 : 1); ++Pass)
			{
				if (!Run.Ask(Trigger))
				{
					break;
				}
				const FStorySweepStretch Played = Run.Play(Options.Triggers);
				Print({Map, Story, Name + (Pass == 0 ? "" : " again") + StorySweepOpeningSuffix(Played.Opening)}, Run, Played);
				// Left, or left hemmed in by the map's people where triggers are only walked to: the map anew.
				const bool bHemmedIn = !Options.bForcesTriggers && bRoomy && Run.IsHemmedIn();
				if (Played.End != EStorySweepEnd::Control || Played.Map != Map || Played.Story != Story || bHemmedIn)
				{
					// The next one starts from the map as its scenes leave it, not from wherever this one led.
					if (!Arrive(false))
					{
						return;
					}
					break;
				}
			}
		}
	}
}

int main(int ArgumentCount, char** Arguments)
{
	std::array<int32, 2> Range = {-1, -1};
	int32 Position = 0;
	std::filesystem::path Cards;
	FStorySweepOptions Options;
	bool bTriggers = false;
	bool bList = false;
	int32 ExploredPlaces = 0;
	bool bStretchGiven = false;
	bool bBattleGiven = false;
	int32 OnlyStory = -1;
	int32 Shard = 0;
	int32 Shards = 1;
	for (int32 Argument = 1; Argument < ArgumentCount; ++Argument)
	{
		const std::string Text(Arguments[Argument]);
		if (Text.rfind("-cards=", 0) == 0)
		{
			Cards = Text.substr(7);
		}
		else if (Text == "-before")
		{
			Options.bSaveBefore = true;
		}
		else if (Text.rfind("-explore=", 0) == 0)
		{
			ExploredPlaces = std::atoi(Text.c_str() + 9);
		}
		else if (Text == "-triggers")
		{
			bTriggers = true;
		}
		else if (Text == "-walked")
		{
			bTriggers = true;
			Options.bForcesTriggers = false;
		}
		else if (Text.rfind("-choice=", 0) == 0)
		{
			Options.Choice = std::atoi(Text.c_str() + 8);
		}
		else if (Text.rfind("-at=", 0) == 0)
		{
			OnlyStory = std::atoi(Text.c_str() + 4);
		}
		else if (Text.rfind("-set=", 0) == 0)
		{
			std::size_t At = 5;
			while (At < Text.size())
			{
				const std::size_t End = std::min(Text.find(',', At), Text.size());
				const std::size_t Kind = Text.find_first_of("=|-", At);
				if (Kind == std::string::npos || Kind >= End)
				{
					return Fail("-set=ADDRESS=VALUE, ADDRESS|BITS or ADDRESS-BITS, hexadecimal, separated by commas");
				}
				Options.Variables.push_back({static_cast<uint32>(std::strtoul(Text.substr(At, Kind - At).c_str(), nullptr, 16)), Text[Kind],
					static_cast<uint16>(std::strtoul(Text.substr(Kind + 1, End - Kind - 1).c_str(), nullptr, 16))});
				At = End + 1;
			}
		}
		else if (Text.rfind("-frames=", 0) == 0)
		{
			Options.StretchFrames = std::atoi(Text.c_str() + 8);
			bStretchGiven = true;
		}
		else if (Text.rfind("-battle=", 0) == 0)
		{
			Options.BattleFrames = std::atoi(Text.c_str() + 8);
			bBattleGiven = true;
		}
		else if (Text.rfind("-shard=", 0) == 0)
		{
			const std::size_t Slash = Text.find('/');
			Shard = std::atoi(Text.c_str() + 7);
			Shards = Slash != std::string::npos ? std::atoi(Text.c_str() + Slash + 1) : 0;
			if (Shards < 1 || Shard < 0 || Shard >= Shards)
			{
				return Fail("-shard=I/N: I from 0 below N");
			}
		}
		else if (Text == "-expanded")
		{
			Options.bExpanded = true;
		}
		else if (Text == "-list")
		{
			bList = true;
		}
		else if (Text.rfind("-trace=", 0) == 0)
		{
			Options.TraceFrames = std::atoi(Text.c_str() + 7);
		}
		else if (Text == "-reversed")
		{
			Options.bReversesOrder = true;
		}
		else if (Text == "-strong")
		{
			Options.bStrongParty = true;
		}
		else if (Text == "-clear")
		{
			Options.bClearGame = true;
		}
		else if (Text == "-keepelly")
		{
			Options.bKeepElly = true;
		}
		else if (Text == "-id")
		{
			Options.bFeiAsId = true;
		}
		else if (Text.rfind("-holds=", 0) == 0)
		{
			Options.HoldFramesTold = std::atoi(Text.c_str() + 7);
		}
		else if (Text.rfind("-watch=", 0) == 0)
		{
			Options.WatchedActor = Text.substr(7) == "leader" ? FStorySweepOptions::WatchedLeader : std::atoi(Text.c_str() + 7);
		}
		else if (Text[0] != '-' && Position < 2)
		{
			Range[static_cast<std::size_t>(Position++)] = std::atoi(Text.c_str());
		}
		else
		{
			return Fail("usage: WeltallStorySweep [FIRST [LAST]] [-cards=FOLDER] [-before] [-triggers] [-choice=N] [-at=STORY] [-set=CHANGES] [-frames=N] "
						"[-shard=I/N] [-expanded] [-list] [-trace=N] [-strong] [-clear] [-keepelly] [-id], or WeltallStorySweep -explore=N [-cards=FOLDER] ...");
		}
	}
	const int32 First = Range[0] < 0 ? 0 : Range[0];
	const int32 Last = Range[1] < 0 ? (Range[0] < 0 ? 729 : First) : Range[1];

	const FGameLibrary Library = FGameLibrary::Scan(std::filesystem::path(WELTALL_GAMES_DIR));
	auto Opened = FPlayedGame::Open(Library);
	if (!Opened)
	{
		return Fail(Opened.Error().Message);
	}
	const FPlayedGame& Game = *Opened;
	auto Story = FStoryPacks::Read(std::filesystem::path(WELTALL_STORY_DIR));
	if (!Story)
	{
		return Fail("the port's story cannot be read: " + Story.Error().Message);
	}
	// The example scenes as the expansion, as a player who extracted them plays them.
	if (std::optional<FError> Error = Story->ReadExpansion(std::filesystem::path(WELTALL_EXAMPLES_DIR) / "disc2", ExamplesFolder))
	{
		return Fail("the example scenes cannot be read: " + Error->Message);
	}
	std::vector<FOriginalSave> Saves;
	if (!Cards.empty())
	{
		Saves = ReadCardFolderSaves(Cards);
		if (Saves.empty())
		{
			return Fail("no save on the card images of the folder");
		}
		std::stable_sort(Saves.begin(), Saves.end(), [](const FOriginalSave& A, const FOriginalSave& B) { return A.StoryProgress() < B.StoryProgress(); });
	}

	int32 Number = 0;
	if (ExploredPlaces > 0)
	{
		Options.bForcesTriggers = false;
		if (!bStretchGiven)
		{
			// The story is followed as far as it plays by itself: its stretches end where they stay in one place.
			Options.StretchFrames = ExploredStretchFrames;
		}
		if (!bBattleGiven)
		{
			Options.BattleFrames = ExploredBattleFrames;
		}
		FStorySweepRun Run(Game, Saves, &*Story, Options);
		FStorySweepExplorer Explorer(Run, Options, [](int32 Map, uint16 Value, const std::string& Start, const FStorySweepRun& Played, const FStorySweepStretch& Stretch)
			{
				Print({Map, Value, Start}, Played, Stretch);
			});
		for (const FGameState& Root : StorySweepRoots(Game, Saves))
		{
			const int32 Progress = Root.SavedEventVariable(0);
			if ((Range[0] < 0 || (Progress >= First && Progress <= Last)) && Number++ % Shards == Shard)
			{
				Explorer.Explore(Root, ExploredPlaces);
			}
		}
		return 0;
	}
	for (const FStorySweepMap& Map : ListStorySweepMaps(Game))
	{
		if (Map.Map < First || Map.Map > Last || Number++ % Shards != Shard)
		{
			continue;
		}
		if (bList)
		{
			std::printf("%d\t%s", Map.Map, Map.bBorrowed ? "borrowed" : "own");
			for (const uint16 Value : Map.Stories)
			{
				std::printf("\t%d", int32{Value});
			}
			std::printf("\n");
			continue;
		}
		for (const uint16 Value : OnlyStory >= 0 ? std::vector<uint16>{static_cast<uint16>(OnlyStory)} : Map.Stories)
		{
			FStorySweepRun Run(Game, Saves, &*Story, Options);
			PlayMap(Run, Map.Map, Value, Options, bTriggers);
		}
	}
	return 0;
}
