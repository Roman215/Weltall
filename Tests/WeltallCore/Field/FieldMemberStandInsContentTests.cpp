#include "Field/FieldMemberStandIns.h"
#include "Field/FieldPilotStandIns.h"
#include "Field/FieldPlay.h"
#include "Field/FieldSprites.h"
#include "Field/IdsFieldSprite.h"
#include "Game/CharacterLooks.h"
#include "Game/PartyOrder.h"
#include "Game/PsxRandom.h"
#include "Graphics/Texture.h"
#include "Support/FieldPlaySession.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <optional>
#include <set>

using namespace Weltall;

// The party drawn in the order its player keeps it, on the real map, against the story's order.

namespace
{
	/** Lahan: a new game's party, Fei and Citan, free to walk from its first tick. */
	constexpr int32 Lahan = 1;
	constexpr int32 Ticks = 150;
	constexpr int32 WalkFrom = 30;
	constexpr int32 WalkTo = 90;
	constexpr uint8 Fei = 0;
	constexpr uint8 Citan = 2;
	constexpr int32 Walk = 1;
	/** A character's blend key is its actor's number, three bits up. */
	constexpr uint32 ActorShift = 3;

	/** The actors whose passes draw from VRAM's port rows: stand-ins. */
	std::set<uint32> StandInActors(const FFieldFrame& Frame)
	{
		std::set<uint32> Actors;
		for (const FFieldCharacterQuad& Entry : Frame.Quads)
		{
			if (FTexturePage::FromPortAttribute(Entry.Quad.TexturePage).Y >= FVram::Height)
			{
				Actors.insert(Frame.Characters[Entry.Character].BlendKey >> ActorShift);
			}
		}
		return Actors;
	}

	void CitanFirst(FGameState& Game)
	{
		FPartyOrder Order;
		const std::array<uint8, 2> Shown{Citan, Fei};
		Order.Arrange(Game.Party(), Shown);
		Game.SetPartyOrder(Order);
	}
}

TEST_CASE("The party walks in its player's order, each slot drawn as the member placed there, the game as the story's", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Arranged = Testing::EnterFieldMap(*Disc, Lahan, CitanFirst);
	auto Story = Testing::EnterFieldMap(*Disc, Lahan);
	REQUIRE(Arranged->Play->Field().Context().PartyCharacters == std::array<uint8, 3>{Fei, Citan, 0xFF});

	int32 Differences = 0;
	int32 BothDrawn = 0;
	int32 Walking = 0;
	int32 Unfollowed = 0;
	std::optional<int32> LastAnimation;
	for (int32 Tick = 0; Tick < Ticks; ++Tick)
	{
		std::vector<FPadState> Held(static_cast<std::size_t>(Arranged->Play->VblanksPerTick()));
		if (Tick >= WalkFrom && Tick < WalkTo)
		{
			for (FPadState& Vblank : Held)
			{
				Vblank.Buttons = FPadState::Bit(EPadButton::Down);
			}
		}
		REQUIRE_FALSE(Arranged->Play->Tick(Held, EFieldCulling::InRange, Arranged->Frame));
		REQUIRE_FALSE(Story->Play->Tick(Held, EFieldCulling::InRange, Story->Frame));

		// The game is the story's either way: every actor's place, animation and frame, and the random numbers drawn.
		const FEventContext& Context = Arranged->Play->Field().Context();
		const FEventContext& AsStory = Story->Play->Field().Context();
		for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
		{
			const FSprite* Sprite = Arranged->Play->Field().Sprites().SpriteOf(static_cast<int32>(Actor));
			const FSprite* StorySprite = Story->Play->Field().Sprites().SpriteOf(static_cast<int32>(Actor));
			const bool bSame = Context.States[Actor].Position == AsStory.States[Actor].Position && (Sprite == nullptr) == (StorySprite == nullptr)
				&& (Sprite == nullptr || (Sprite->Animation() == StorySprite->Animation() && Sprite->Frame == StorySprite->Frame));
			Differences += bSame ? 0 : 1;
		}
		Differences += Context.Random->Seed() == AsStory.Random->Seed() ? 0 : 1;
		CHECK(Context.PartyCharacters == AsStory.PartyCharacters);
		CHECK(StandInActors(Story->Frame).empty());

		// The leader's actor, Fei's, drawn as Citan; Citan's actor, following, as Fei.
		const FFieldMemberStandIns& Members = Arranged->Play->Drawing().MemberStandIns();
		CHECK(Members.ShownIn(0) == std::optional<int32>(Citan));
		CHECK(Members.ShownIn(1) == std::optional<int32>(Fei));
		const std::set<uint32> Drawn = StandInActors(Arranged->Frame);
		if (Drawn.count(Context.PartyActors[0]) != 0 && Drawn.count(Context.PartyActors[1]) != 0)
		{
			++BothDrawn;
		}
		// The leader's stand-in, once the map's first picture is drawn, plays what the leader plays from the tick after a start on.
		const FSprite* Leader = Arranged->Play->Field().Sprites().SpriteOf(Context.LeaderActor);
		const FSprite* StandIn = Members.Of(0);
		REQUIRE(Leader != nullptr);
		if (StandIn != nullptr && LastAnimation == Leader->Animation() && StandIn->Animation() != Leader->Animation())
		{
			++Unfollowed;
		}
		LastAnimation = Leader->Animation();
		Walking += Leader->Animation() == Walk ? 1 : 0;
	}
	CHECK(Differences == 0);
	CHECK(BothDrawn > Ticks * 3 / 4);
	CHECK(Walking > 20);
	CHECK(Unfollowed == 0);
}

TEST_CASE("A memory played on a map without the leader shows its people as the story has them", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Elly's memory of her landing by Lahan: her actor is the party's second member's, and the map has no leader.
	constexpr int32 Landing = 28;
	constexpr uint8 Elly = 1;
	constexpr int32 SceneTicks = 600;
	constexpr uint16 Hidden = 0x20;
	const auto Stage = [](FGameState& Game)
	{
		Game.SetParty({Fei, Elly, 0xFF});
		FPartyOrder Order;
		const std::array<uint8, 2> Shown{Elly, Fei};
		Order.Arrange(Game.Party(), Shown);
		Game.SetPartyOrder(Order);
	};
	auto Arranged = Testing::EnterFieldMap(*Disc, Landing, Stage);
	int32 EllyShown = 0;
	for (int32 Tick = 0; Tick < SceneTicks; ++Tick)
	{
		const std::vector<FPadState> Held(static_cast<std::size_t>(Arranged->Play->VblanksPerTick()));
		REQUIRE_FALSE(Arranged->Play->Tick(Held, EFieldCulling::InRange, Arranged->Frame));
		const FEventContext& Context = Arranged->Play->Field().Context();
		REQUIRE(Context.PartyCharacters == std::array<uint8, 3>{Fei, Elly, 0xFF});
		// With Elly placed first her slot would be drawn as Fei: in a scene nobody is drawn as another.
		CHECK_FALSE(Arranged->Play->Drawing().Party().Together());
		CHECK_FALSE(Arranged->Play->Drawing().MemberStandIns().ShownIn(1));
		CHECK(StandInActors(Arranged->Frame).empty());
		const uint8 Actor = Context.PartyActors[1];
		if (Actor != FEventContext::NoActor && (Context.Actors[Actor].Flags & Hidden) == 0 && !Arranged->Frame.Quads.empty())
		{
			// The watch looks as the tick's first sprite is drawn: a map without the leader is a cutscene throughout.
			CHECK(Arranged->Play->Drawing().Cutscenes().InCutscene());
			++EllyShown;
		}
	}
	// She climbs out of her gear in it: the scene got that far.
	CHECK(EllyShown > 60);
}

TEST_CASE("A member seated apart is himself, and so is the leader the player walks past him", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// The cell Fei wakes in on the way to the desert: Citan sits on its bed while the player walks Fei about.
	constexpr int32 Cell = 56;
	constexpr uint16 WakingInTheCell = 42;
	// The saved variable (by its index) the cell's first scene sets once it has played.
	constexpr int32 WokenInTheCell = 0xAF;
	constexpr int32 CellTicks = 600;
	const auto Stage = [](FGameState& Game)
	{
		Game.SetParty({Fei, Citan, 0xFF});
		Game.SetSavedEventVariable(0, WakingInTheCell);
		Game.SetSavedEventVariable(WokenInTheCell, 1);
		CitanFirst(Game);
	};
	auto Arranged = Testing::EnterFieldMap(*Disc, Cell, Stage);
	int32 Walked = 0;
	for (int32 Tick = 0; Tick < CellTicks; ++Tick)
	{
		// Cross through what is said, then a walk about the cell.
		std::vector<FPadState> Held(static_cast<std::size_t>(Arranged->Play->VblanksPerTick()));
		const FEventContext& Before = Arranged->Play->Field().Context();
		for (FPadState& Vblank : Held)
		{
			Vblank.Buttons = Before.PlayerLock != 0 || Before.Messages.OpenMask != 0 ? (Tick % 12 < 2 ? FPadState::Bit(EPadButton::Cross) : 0)
																					   : FPadState::Bit(Tick / 30 % 2 == 0 ? EPadButton::Down : EPadButton::Up);
		}
		REQUIRE_FALSE(Arranged->Play->Tick(Held, EFieldCulling::InRange, Arranged->Frame));
		const FEventContext& Context = Arranged->Play->Field().Context();
		REQUIRE(Context.PartyCharacters == std::array<uint8, 3>{Fei, Citan, 0xFF});
		// With Citan placed first the leader would be drawn as him, beside the one on the bed.
		CHECK_FALSE(Arranged->Play->Drawing().Party().Together());
		CHECK_FALSE(Arranged->Play->Drawing().MemberStandIns().ShownIn(0));
		CHECK(StandInActors(Arranged->Frame).empty());
		Walked += Context.bPlayerControlled ? 1 : 0;
	}
	// The player did have Fei to walk: it is no scene's hold that keeps the story's look.
	CHECK(Walked > 100);
}

TEST_CASE("In gears the party's pilots and gears are drawn in its player's order too", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// The Yggdrasil's hangar: the members on foot as the map's small pilots, their gears parked.
	constexpr int32 Hangar = 291;
	auto Arranged = Testing::EnterFieldMap(*Disc, Hangar, CitanFirst);
	Arranged->Play->Drawing().SetPilotStandIns(true);
	Arranged->Run(60);
	const FEventContext& Context = Arranged->Play->Field().Context();
	INFO("party " << int(Context.PartyCharacters[0]) << " " << int(Context.PartyCharacters[1]) << " " << int(Context.PartyCharacters[2]));
	REQUIRE(Context.PartyCharacters[0] == Fei);
	REQUIRE(Context.PartyCharacters[1] == Citan);
	const FFieldPilotStandIns* Pilots = Arranged->Play->Drawing().PilotStandIns();
	REQUIRE(Pilots != nullptr);
	CHECK(Pilots->CharacterOf(0) == Citan);
	CHECK(Pilots->CharacterOf(1) == Fei);
}

TEST_CASE("Boarded, the leader rides the gear of the member placed first, as a sprite and as a 3D model", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// A hangar the party walks on foot or in its gears: L2 and R2 together board them.
	constexpr int32 Hangar = 260;
	constexpr int32 BoardAt = 120;
	constexpr int32 BoardFor = 4;
	constexpr int32 Settled = 200;
	constexpr int32 Weltall = 0;
	constexpr int32 Heimdal = 3;
	const auto Stage = [](FGameState& Game)
	{
		Game.SetParty({Fei, Citan, 0xFF});
		CitanFirst(Game);
	};
	for (const bool bModels : {false, true})
	{
		auto Arranged = Testing::EnterFieldMap(*Disc, Hangar, Stage);
		if (bModels)
		{
			Arranged->Play->Drawing().SetGearStandIns(EFieldGearModels::OutsideCutscenes);
		}
		for (int32 Tick = 0; Tick < Settled; ++Tick)
		{
			std::vector<FPadState> Held(static_cast<std::size_t>(Arranged->Play->VblanksPerTick()));
			for (FPadState& Vblank : Held)
			{
				Vblank.Buttons = Tick >= BoardAt && Tick < BoardAt + BoardFor ? FPadState::Bit(EPadButton::L2) | FPadState::Bit(EPadButton::R2) : 0;
			}
			REQUIRE_FALSE(Arranged->Play->Tick(Held, EFieldCulling::InRange, Arranged->Frame));
		}
		const FEventContext& Context = Arranged->Play->Field().Context();
		INFO("models " << bModels);
		REQUIRE(Context.PartyCharacters[1] == Citan);
		REQUIRE(Context.GameState->PartySlotInGear(0) != 0);
		CHECK_FALSE(Arranged->Play->Drawing().Cutscenes().InCutscene());
		if (bModels)
		{
			const FFieldGearStandIns* Models = Arranged->Play->Drawing().GearStandIns();
			REQUIRE(Models != nullptr);
			CHECK(Models->GearOf(Context.LeaderActor) == Heimdal);
			CHECK(Models->GearOf(Context.PartyActors[1]) == Weltall);
		}
		else
		{
			CHECK(Arranged->Play->Drawing().MemberStandIns().Of(0) != nullptr);
			CHECK(StandInActors(Arranged->Frame).count(Context.LeaderActor) != 0);
		}
	}
}

TEST_CASE("The grown Emeralda switched to the child is drawn as the child in her place, the game as the story's", "[Field][Content]")
{
	// The forest of the first battle, whose actors stand for every member, the grown Emeralda among them.
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 EveryMembersMap = 15;
	constexpr uint8 Emeralda = 6;
	constexpr uint8 GrownEmeralda = 10;
	const auto Grown = [](FGameState& Game)
	{
		REQUIRE(Game.CopyCharacter(Emeralda, GrownEmeralda));
		Game.SetParty({Fei, GrownEmeralda, 0xFF});
		Game.SetJoinedCharacters(static_cast<uint16>(Game.JoinedCharacters() | 1u << GrownEmeralda));
	};
	const auto AsTheChild = [&Grown](FGameState& Game)
	{
		Grown(Game);
		FCharacterLooks Looks;
		REQUIRE(Looks.Switch(GrownEmeralda));
		Game.SetCharacterLooks(Looks);
	};
	auto Switched = Testing::EnterFieldMap(*Disc, EveryMembersMap, AsTheChild);
	auto Story = Testing::EnterFieldMap(*Disc, EveryMembersMap, Grown);
	REQUIRE(Switched->Play->Field().Context().PartyCharacters[0] == Fei);
	REQUIRE(Switched->Play->Field().Context().PartyCharacters[1] == GrownEmeralda);

	int32 Differences = 0;
	int32 Drawn = 0;
	for (int32 Tick = 0; Tick < Ticks; ++Tick)
	{
		// Standing, so that no encounter comes.
		const std::vector<FPadState> Held(static_cast<std::size_t>(Switched->Play->VblanksPerTick()));
		REQUIRE_FALSE(Switched->Play->Tick(Held, EFieldCulling::InRange, Switched->Frame));
		REQUIRE_FALSE(Story->Play->Tick(Held, EFieldCulling::InRange, Story->Frame));
		const FEventContext& Context = Switched->Play->Field().Context();
		const FEventContext& AsStory = Story->Play->Field().Context();
		for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
		{
			Differences += Context.States[Actor].Position == AsStory.States[Actor].Position ? 0 : 1;
		}
		Differences += Context.Random->Seed() == AsStory.Random->Seed() ? 0 : 1;
		CHECK(StandInActors(Story->Frame).empty());

		// While the party is together: her own place, the child's pictures; Fei as he is.
		const FFieldMemberStandIns& Members = Switched->Play->Drawing().MemberStandIns();
		CHECK_FALSE(Members.ShownIn(1));
		CHECK_FALSE(Members.PicturedIn(0));
		const std::set<uint32> StandIns = StandInActors(Switched->Frame);
		CHECK(StandIns.count(Context.PartyActors[0]) == 0);
		// Her look holds apart and in cutscenes too: only the party's order waits for it to walk as one.
		CHECK(Members.PicturedIn(1) == std::optional<int32>(Emeralda));
		if (Switched->Play->Drawing().Party().Together())
		{
			Drawn += StandIns.count(Context.PartyActors[1]) != 0 ? 1 : 0;
		}
	}
	CHECK(Differences == 0);
	CHECK(Drawn > Ticks * 3 / 4);
}

TEST_CASE("Elly with purple hair is drawn from her own pictures recoloured in her place, the game as the story's", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 EveryMembersMap = 15;
	constexpr uint8 Elly = FCharacterLooks::Elly;
	const auto WithElly = [](FGameState& Game) { Game.SetParty({Fei, Elly, 0xFF}); };
	const auto PurpleHaired = [&WithElly](FGameState& Game)
	{
		WithElly(Game);
		FCharacterLooks Looks;
		REQUIRE(Looks.Switch(Elly));
		Game.SetCharacterLooks(Looks);
	};
	auto Switched = Testing::EnterFieldMap(*Disc, EveryMembersMap, PurpleHaired);
	auto Story = Testing::EnterFieldMap(*Disc, EveryMembersMap, WithElly);
	REQUIRE(Switched->Play->Field().Context().PartyCharacters[1] == Elly);
	int32 Differences = 0;
	int32 Drawn = 0;
	for (int32 Tick = 0; Tick < Ticks; ++Tick)
	{
		const std::vector<FPadState> Held(static_cast<std::size_t>(Switched->Play->VblanksPerTick()));
		REQUIRE_FALSE(Switched->Play->Tick(Held, EFieldCulling::InRange, Switched->Frame));
		REQUIRE_FALSE(Story->Play->Tick(Held, EFieldCulling::InRange, Story->Frame));
		const FEventContext& Context = Switched->Play->Field().Context();
		const FEventContext& AsStory = Story->Play->Field().Context();
		for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
		{
			Differences += Context.States[Actor].Position == AsStory.States[Actor].Position ? 0 : 1;
		}
		Differences += Context.Random->Seed() == AsStory.Random->Seed() ? 0 : 1;
		CHECK(StandInActors(Story->Frame).empty());
		// Her own record's pictures, recoloured, drawn by a stand-in in her place; Fei as he is.
		const FFieldMemberStandIns& Members = Switched->Play->Drawing().MemberStandIns();
		const std::set<uint32> StandIns = StandInActors(Switched->Frame);
		CHECK(StandIns.count(Context.PartyActors[0]) == 0);
		CHECK(Members.PicturedIn(1) == std::optional<int32>(Elly));
		if (Switched->Play->Drawing().Party().Together())
		{
			Drawn += StandIns.count(Context.PartyActors[1]) != 0 ? 1 : 0;
		}
	}
	CHECK(Differences == 0);
	CHECK(Drawn > Ticks * 3 / 4);
}

TEST_CASE("A stand-in jumps as the leader does from the rise to the landing", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 JumpAt = 40;
	constexpr int32 JumpTicks = 80;
	constexpr int32 Jump = 3;
	auto Arranged = Testing::EnterFieldMap(*Disc, Lahan, CitanFirst);
	int32 Jumping = 0;
	int32 FramesInTheFall = 0;
	bool bFalling = false;
	std::optional<int32> LastFrame;
	for (int32 Tick = 0; Tick < JumpTicks; ++Tick)
	{
		// A walk, and the jump button (the release's Triangle) pressed in it.
		std::vector<FPadState> Held(static_cast<std::size_t>(Arranged->Play->VblanksPerTick()));
		for (FPadState& Vblank : Held)
		{
			Vblank.Buttons = (Tick >= WalkFrom ? FPadState::Bit(EPadButton::Down) : 0) | (Tick == JumpAt ? FPadState::Bit(EPadButton::Triangle) : 0);
		}
		REQUIRE_FALSE(Arranged->Play->Tick(Held, EFieldCulling::InRange, Arranged->Frame));
		const FEventContext& Context = Arranged->Play->Field().Context();
		const FSprite* Leader = Arranged->Play->Field().Sprites().SpriteOf(Context.LeaderActor);
		const FSprite* StandIn = Arranged->Play->Drawing().MemberStandIns().Of(0);
		REQUIRE(Leader != nullptr);
		if (Leader->Animation() != Jump || StandIn == nullptr)
		{
			continue;
		}
		++Jumping;
		// Once the leader falls, its stand-in goes on to its own fall's and landing's frames: the waits on them read
		// the vertical speed and the floor, which only the leader's actor moves.
		bFalling = bFalling || Leader->Velocity[1] > 0;
		if (bFalling && LastFrame != StandIn->Frame)
		{
			++FramesInTheFall;
		}
		LastFrame = StandIn->Frame;
	}
	CHECK(Jumping > 16);
	CHECK(FramesInTheFall >= 2);
}

TEST_CASE("Fei as Id is drawn with Id's own field sprite from the port's rows in his place, the game as the story's", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 EveryMembersMap = 15;
	constexpr uint8 Elly = FCharacterLooks::Elly;
	const auto WithElly = [](FGameState& Game) { Game.SetParty({Fei, Elly, 0xFF}); };
	const auto AsId = [&WithElly](FGameState& Game)
	{
		WithElly(Game);
		FCharacterLooks Looks;
		REQUIRE(Looks.Switch(Fei));
		Game.SetCharacterLooks(Looks);
	};
	auto Switched = Testing::EnterFieldMap(*Disc, EveryMembersMap, AsId);
	auto Story = Testing::EnterFieldMap(*Disc, EveryMembersMap, WithElly);
	REQUIRE(Switched->Play->Field().Context().PartyCharacters[0] == Fei);
	int32 Differences = 0;
	int32 Drawn = 0;
	for (int32 Tick = 0; Tick < Ticks; ++Tick)
	{
		const std::vector<FPadState> Held(static_cast<std::size_t>(Switched->Play->VblanksPerTick()));
		REQUIRE_FALSE(Switched->Play->Tick(Held, EFieldCulling::InRange, Switched->Frame));
		REQUIRE_FALSE(Story->Play->Tick(Held, EFieldCulling::InRange, Story->Frame));
		const FEventContext& Context = Switched->Play->Field().Context();
		const FEventContext& AsStory = Story->Play->Field().Context();
		for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
		{
			Differences += Context.States[Actor].Position == AsStory.States[Actor].Position ? 0 : 1;
		}
		Differences += Context.Random->Seed() == AsStory.Random->Seed() ? 0 : 1;
		const FFieldMemberStandIns& Members = Switched->Play->Drawing().MemberStandIns();
		const std::set<uint32> StandIns = StandInActors(Switched->Frame);
		CHECK(StandIns.count(Context.PartyActors[1]) == 0);
		CHECK(Members.PicturedIn(0) == std::optional<int32>(Fei));
		if (!Switched->Play->Drawing().Party().Together())
		{
			continue;
		}
		// His own record's place, drawn from Id's picture: the first band's page beside the stand-ins' areas (his shadow lies on the floor as ever).
		int32 LeaderQuads = 0;
		for (const FFieldCharacterQuad& Entry : Switched->Frame.Quads)
		{
			const FFieldCharacterView& Character = Switched->Frame.Characters[Entry.Character];
			if (Character.BlendKey >> ActorShift == Context.PartyActors[0] && !Character.bOnFloor)
			{
				const FTexturePage Page = FTexturePage::FromPortAttribute(Entry.Quad.TexturePage);
				CHECK(Page.X == FIdsFieldSprite::TextureX);
				CHECK(Page.Y == FIdsFieldSprite::TextureY);
				++LeaderQuads;
			}
		}
		Drawn += LeaderQuads != 0 && StandIns.count(Context.PartyActors[0]) != 0 ? 1 : 0;
	}
	CHECK(Differences == 0);
	CHECK(Drawn > Ticks * 3 / 4);
}
