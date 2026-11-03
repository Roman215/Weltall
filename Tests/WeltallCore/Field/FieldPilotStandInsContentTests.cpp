#include "Field/FieldPilotStandIns.h"
#include "Field/FieldPlay.h"
#include "Field/FieldSprites.h"
#include "Game/CharacterLooks.h"
#include "Game/GameState.h"
#include "Game/PsxRandom.h"
#include "Graphics/Texture.h"
#include "Support/FieldPlaySession.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <optional>
#include <vector>

using namespace Weltall;

// The party's own sprites drawn for their pilots on a map in gears, against the map's own pilots, on the real map.

namespace
{
	/** The Yggdrasil's hangar: its init's GearMode 18 has the party's members on foot drawn with its sprites 18 to 26. */
	constexpr int32 HangarMap = 291;
	constexpr int32 Ticks = 150;
	/** The leader stands, walks down the hangar, then stands again. */
	constexpr int32 WalkFrom = 30;
	constexpr int32 WalkTo = 90;

	/** The quads' extent up the view of a character's pass, by its blend key; none when it drew none. */
	std::optional<float> HeightOf(const FFieldFrame& Frame, uint32 BlendKey)
	{
		float Top = 0.0f;
		float Bottom = 0.0f;
		bool bAny = false;
		for (const FFieldCharacterQuad& Entry : Frame.Quads)
		{
			if (Frame.Characters[Entry.Character].BlendKey != BlendKey)
			{
				continue;
			}
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				const float Y = Entry.Quad.ViewCorner(Corner)[1];
				Top = bAny ? std::min(Top, Y) : Y;
				Bottom = bAny ? std::max(Bottom, Y) : Y;
				bAny = true;
			}
		}
		return bAny ? std::optional<float>(Bottom - Top) : std::nullopt;
	}

	/** The quads that draw from VRAM's port rows. */
	std::vector<const FFieldCharacterQuad*> PortQuads(const FFieldFrame& Frame)
	{
		std::vector<const FFieldCharacterQuad*> Found;
		for (const FFieldCharacterQuad& Entry : Frame.Quads)
		{
			if (FTexturePage::FromPortAttribute(Entry.Quad.TexturePage).Y >= FVram::Height)
			{
				Found.push_back(&Entry);
			}
		}
		return Found;
	}

	/** Whether a quad shows a texel that is not transparent. */
	bool ShowsTexels(const FVram& Vram, const FSpriteQuad& Quad)
	{
		const FTexturePage Page = FTexturePage::FromPortAttribute(Quad.TexturePage);
		const FClutPosition Clut = FClutPosition::FromPortAttribute(Quad.Clut);
		for (int32 V = std::min(Quad.V0, Quad.V1); V < std::max(Quad.V0, Quad.V1); ++V)
		{
			for (int32 U = std::min(Quad.U0, Quad.U1); U < std::max(Quad.U0, Quad.U1); ++U)
			{
				if (FetchTexel(Vram, Page, Clut, static_cast<uint8>(U), static_cast<uint8>(V)) != 0)
				{
					return true;
				}
			}
		}
		return false;
	}
}

TEST_CASE("In gears, a member on foot is drawn with its own sprite at about its pilot's size, the game playing as the original", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Detailed = Testing::EnterFieldMap(*Disc, HangarMap);
	auto Original = Testing::EnterFieldMap(*Disc, HangarMap);
	Detailed->Play->Drawing().SetPilotStandIns(true);
	REQUIRE(Detailed->Play->Field().Context().PilotSpriteBase == 18);

	int32 Drawn = 0;
	int32 Members = 0;
	int32 Walking = 0;
	int32 Differences = 0;
	int32 Unfollowed = 0;
	std::optional<int32> LastAnimation;
	for (int32 Tick = 0; Tick < Ticks; ++Tick)
	{
		std::vector<FPadState> Held(static_cast<std::size_t>(Detailed->Play->VblanksPerTick()));
		if (Tick >= WalkFrom && Tick < WalkTo)
		{
			for (FPadState& Vblank : Held)
			{
				Vblank.Buttons = FPadState::Bit(EPadButton::Down);
			}
		}
		REQUIRE_FALSE(Detailed->Play->Tick(Held, EFieldCulling::InRange, Detailed->Frame));
		REQUIRE_FALSE(Original->Play->Tick(Held, EFieldCulling::InRange, Original->Frame));

		// The game is the same either way: every actor's place and animation, and the random numbers drawn.
		const FEventContext& Context = Detailed->Play->Field().Context();
		const FEventContext& AsOriginal = Original->Play->Field().Context();
		for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
		{
			const FSprite* Sprite = Detailed->Play->Field().Sprites().SpriteOf(static_cast<int32>(Actor));
			const FSprite* OriginalSprite = Original->Play->Field().Sprites().SpriteOf(static_cast<int32>(Actor));
			const bool bSame = Context.States[Actor].Position == AsOriginal.States[Actor].Position
				&& (Sprite == nullptr) == (OriginalSprite == nullptr)
				&& (Sprite == nullptr || (Sprite->Animation() == OriginalSprite->Animation() && Sprite->Frame == OriginalSprite->Frame));
			Differences += bSame ? 0 : 1;
		}
		Differences += Context.Random->Seed() == AsOriginal.Random->Seed() ? 0 : 1;
		CHECK(PortQuads(Original->Frame).empty());

		// Each member's pilot is drawn with its own sprite, from the port's rows, about as tall as its pilot.
		const std::vector<const FFieldCharacterQuad*> Quads = PortQuads(Detailed->Frame);
		std::vector<uint32> Keys;
		for (const FFieldCharacterQuad* Entry : Quads)
		{
			const uint32 Key = Detailed->Frame.Characters[Entry->Character].BlendKey;
			if (std::find(Keys.begin(), Keys.end(), Key) == Keys.end())
			{
				Keys.push_back(Key);
			}
		}
		if (Keys.empty())
		{
			continue;
		}
		++Drawn;
		Members = std::max(Members, static_cast<int32>(Keys.size()));
		CHECK(std::any_of(Quads.begin(), Quads.end(), [&](const FFieldCharacterQuad* Entry) { return ShowsTexels(Detailed->Play->Drawing().Vram(), Entry->Quad); }));
		for (const uint32 Key : Keys)
		{
			const std::optional<float> Height = HeightOf(Detailed->Frame, Key);
			const std::optional<float> PilotHeight = HeightOf(Original->Frame, Key);
			INFO("tick " << Tick << ", key " << Key);
			REQUIRE(Height);
			REQUIRE(PilotHeight);
			INFO("the stand-in " << *Height << " high, its pilot " << *PilotHeight);
			CHECK(*Height > *PilotHeight * 0.75f);
			CHECK(*Height < *PilotHeight * 1.5f);
		}

		// It plays what its pilot plays, from the tick after a start on.
		const FSprite* Pilot = Detailed->Play->Field().Sprites().SpriteOf(Context.LeaderActor);
		const FSprite* StandIn = Detailed->Play->Drawing().PilotStandIns()->Of(0);
		REQUIRE(Pilot != nullptr);
		REQUIRE(StandIn != nullptr);
		CHECK(Detailed->Play->Drawing().PilotStandIns()->CharacterOf(0) == Context.States[Context.LeaderActor].Character);
		if (LastAnimation == Pilot->Animation() && StandIn->Animation() != Pilot->Animation())
		{
			++Unfollowed;
		}
		LastAnimation = Pilot->Animation();
		Walking += Pilot->Animation() == 1 || Pilot->Animation() == 2 ? 1 : 0;
	}
	CHECK(Differences == 0);
	CHECK(Drawn > Ticks * 3 / 4);
	// Fei and the member who follows him.
	CHECK(Members == 2);
	CHECK(Walking > 20);
	CHECK(Unfollowed == 0);
}

TEST_CASE("In gears with Fei drawn as Id, his pilot is drawn with Id's own sprite", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Whether the leader's pilot is drawn with a resident sprite file: Id's is one (his file of map 682), Fei's own is not.
	const auto LeaderFromResidentFile = [&Disc](bool bId)
	{
		auto Detailed = Testing::EnterFieldMap(*Disc, HangarMap, [bId](FGameState& Game)
		{
			FCharacterLooks Looks;
			if (bId)
			{
				REQUIRE(Looks.Switch(FCharacterLooks::Fei));
			}
			Game.SetCharacterLooks(Looks);
		});
		Detailed->Play->Drawing().SetPilotStandIns(true);
		for (int32 Tick = 0; Tick < 30; ++Tick)
		{
			std::vector<FPadState> Held(static_cast<std::size_t>(Detailed->Play->VblanksPerTick()));
			REQUIRE_FALSE(Detailed->Play->Tick(Held, EFieldCulling::InRange, Detailed->Frame));
		}
		const FSprite* StandIn = Detailed->Play->Drawing().PilotStandIns()->Of(0);
		REQUIRE(StandIn != nullptr);
		REQUIRE(StandIn->Resource != nullptr);
		REQUIRE(StandIn->Resource->File != nullptr);
		CHECK(Detailed->Play->Drawing().PilotStandIns()->CharacterOf(0) == FCharacterLooks::Fei);
		return StandIn->Resource->File->IsResident(StandIn->Resource->Tables);
	};
	CHECK_FALSE(LeaderFromResidentFile(false));
	CHECK(LeaderFromResidentFile(true));
}
