#include "Field/EventMachine.h"
#include "Field/FieldGearStandIns.h"
#include "Field/FieldPartySprites.h"
#include "Field/FieldPlay.h"
#include "Field/FieldPresenter.h"
#include "Field/FieldRuntime.h"
#include "Field/FieldSprites.h"
#include "Game/FieldProgram.h"
#include "Game/KernelState.h"
#include "Game/PsxRandom.h"
#include "Graphics/GteVectors.h"
#include "Graphics/MdlModel.h"
#include "Graphics/Texture.h"
#include "Support/FieldPlaySession.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

using namespace Weltall;

// The party's gears drawn with their battle models for their sprites on a map in gears, against the sprites, on the real map.

namespace
{
	/** A hangar the party walks on foot or in its gears: L2 and R2 together board them. */
	constexpr int32 HangarMap = 260;
	/** A cave the party walks in gears, a Repair Bot at its save point. */
	constexpr int32 CaveMap = 173;
	/** Golgoda, a cutscene from its first picture (disc 2). */
	constexpr int32 GolgodaMap = 657;
	constexpr int32 Ticks = 300;
	constexpr int32 BoardAt = 120;
	constexpr int32 BoardFor = 4;
	/** The leader's gear stands, walks up the hangar, then stands again. */
	constexpr int32 WalkFrom = 170;
	constexpr int32 WalkTo = 250;
	/** A new game's Fei rides Weltall. */
	constexpr int32 FeisGear = 0;
	constexpr int32 ChuChusGear = 7;
	constexpr uint32 BlendKeyShift = 3;

	struct FExtent
	{
		float Top = 0.0f;
		float Bottom = 0.0f;

		float Height() const { return Bottom - Top; }
	};

	/** The quads' extent up the view of an actor's sprite, its shadow on the floor left out; none when it drew none. */
	std::optional<FExtent> SpriteExtent(const FFieldFrame& Frame, int32 Actor)
	{
		float Top = 0.0f;
		float Bottom = 0.0f;
		bool bAny = false;
		for (const FFieldCharacterQuad& Entry : Frame.Quads)
		{
			const FFieldCharacterView& Character = Frame.Characters[Entry.Character];
			if (Character.bOnFloor || Character.BlendKey >> BlendKeyShift != static_cast<uint32>(Actor))
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
		return bAny ? std::optional<FExtent>(FExtent{Top, Bottom}) : std::nullopt;
	}

	/** What a frame draws of one stand-in's model. */
	struct FStandInDrawn
	{
		int32 Parts = 0;
		int32 Polygons = 0;
		/** Polygons whose first corner's texel, read from VRAM's port rows, is not transparent. */
		int32 Textured = 0;
		float Top = 0.0f;
		float Bottom = 0.0f;
	};

	FStandInDrawn StandInsDrawn(const FFieldFrame& Frame, const FVram& Vram, int32 LayoutSlot)
	{
		FStandInDrawn Drawn;
		const FFieldGearsFrame& Gears = Frame.Gears;
		for (std::size_t Index = 0; Index < Gears.Items.size(); ++Index)
		{
			const FFieldGearLayoutItem& Item = Gears.Items[Index];
			const FFieldGearObject& Object = Gears.Objects[Index];
			if (Item.Slot != LayoutSlot || Item.Part == nullptr || !Object.bDrawn)
			{
				continue;
			}
			CHECK(Item.bPortRows);
			CHECK(Object.bFine);
			for (const FGteVector& Vertex : Item.Part->Vertices)
			{
				const float Y = Object.Fine.Turn({static_cast<float>(Vertex[0]), static_cast<float>(Vertex[1]), static_cast<float>(Vertex[2])})[1] + Object.Fine.T[1];
				Drawn.Top = Drawn.Parts == 0 ? Y : std::min(Drawn.Top, Y);
				Drawn.Bottom = Drawn.Parts == 0 ? Y : std::max(Drawn.Bottom, Y);
				++Drawn.Parts;
			}
			for (uint32 Number = 0; Number < Item.PolygonCount; ++Number)
			{
				if (!Gears.Polygons[Item.FirstPolygon + Number].bDrawn)
				{
					continue;
				}
				++Drawn.Polygons;
				const FMdlPrimitive& Primitive = Item.Part->Primitives[Number];
				if (!Primitive.Traits().bTextured)
				{
					continue;
				}
				const FMdlTextureState& Texture = Item.Packets->Textures[Number];
				const FTexturePage Page = FTexturePage::FromPortAttribute(Texture.TexturePage);
				CHECK(Page.Y >= FVram::Height);
				Drawn.Textured += FetchTexel(Vram, Page, FClutPosition::FromPortAttribute(Texture.Clut), Primitive.Uvs[0].U, Primitive.Uvs[0].V) != 0 ? 1 : 0;
			}
		}
		return Drawn;
	}

	/** How far above the floor of its own space a model's shown parts begin and end, in that space's units. */
	struct FModelReach
	{
		int32 Soles = 0;
		int32 Top = 0;
	};

	FModelReach ReachOf(const FGearModel& Model)
	{
		std::optional<FModelReach> Reach;
		for (const FGearBonePackets& Built : Model.BonePackets)
		{
			const FGearBone& Bone = Model.Bones[Built.Bone];
			if (Bone.bShown == 0)
			{
				continue;
			}
			const FGteMatrix Placed = CompMatrix(Model.Bones[0].Local, Bone.World);
			for (const FGteVector& Vertex : Model.Parts->Parts[Built.Part].Vertices)
			{
				const int32 Up = -RotTrans(Placed, Vertex)[1];
				Reach = Reach ? FModelReach{std::min(Reach->Soles, Up), std::max(Reach->Top, Up)} : FModelReach{Up, Up};
			}
		}
		return Reach.value_or(FModelReach{});
	}
}

TEST_CASE("In gears, a ridden gear is drawn with its battle model at its sprite's size, the game playing as the original", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Models = Testing::EnterFieldMap(*Disc, HangarMap);
	auto Original = Testing::EnterFieldMap(*Disc, HangarMap);
	Models->Play->Drawing().SetGearStandIns(EFieldGearModels::Everywhere);
	const FFieldGearStandIns* StandIns = Models->Play->Drawing().GearStandIns();
	REQUIRE(StandIns != nullptr);

	int32 Shown = 0;
	int32 Standing = 0;
	int32 Moving = 0;
	int32 Differences = 0;
	std::optional<std::array<float, 3>> PartBefore;
	int32 PartsMoved = 0;
	for (int32 Tick = 0; Tick < Ticks; ++Tick)
	{
		std::vector<FPadState> Held(static_cast<std::size_t>(Models->Play->VblanksPerTick()));
		for (FPadState& Vblank : Held)
		{
			if (Tick >= BoardAt && Tick < BoardAt + BoardFor)
			{
				Vblank.Buttons = FPadState::Bit(EPadButton::L2) | FPadState::Bit(EPadButton::R2);
			}
			else if (Tick >= WalkFrom && Tick < WalkTo)
			{
				Vblank.Buttons = FPadState::Bit(EPadButton::Up);
			}
		}
		REQUIRE_FALSE(Models->Play->Tick(Held, EFieldCulling::InRange, Models->Frame));
		REQUIRE_FALSE(Original->Play->Tick(Held, EFieldCulling::InRange, Original->Frame));

		// The game is the same either way: every actor's place and animation, and the random numbers drawn.
		const FEventContext& Context = Models->Play->Field().Context();
		const FEventContext& AsOriginal = Original->Play->Field().Context();
		for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
		{
			const FSprite* Sprite = Models->Play->Field().Sprites().SpriteOf(static_cast<int32>(Actor));
			const FSprite* OriginalSprite = Original->Play->Field().Sprites().SpriteOf(static_cast<int32>(Actor));
			const bool bSame = Context.States[Actor].Position == AsOriginal.States[Actor].Position
				&& (Sprite == nullptr) == (OriginalSprite == nullptr)
				&& (Sprite == nullptr || (Sprite->Animation() == OriginalSprite->Animation() && Sprite->Frame == OriginalSprite->Frame));
			Differences += bSame ? 0 : 1;
		}
		Differences += Context.Random->Seed() == AsOriginal.Random->Seed() ? 0 : 1;
		CHECK(Original->Frame.Gears.Layout == Original->Play->Drawing().GearLayout(AsOriginal));

		const int32 Leader = Context.LeaderActor;
		if (!StandIns->IsShown(Leader))
		{
			PartBefore.reset();
			continue;
		}
		INFO("tick " << Tick);
		++Shown;
		CHECK(StandIns->GearOf(Leader) == FeisGear);
		CHECK(Models->Frame.Gears.Layout == Models->Play->Drawing().GearLayout(Context));
		// The gear's sprite is not drawn where its model is, only its shadow; the original draws it.
		CHECK_FALSE(SpriteExtent(Models->Frame, Leader));
		const std::optional<FExtent> Sprite = SpriteExtent(Original->Frame, Leader);
		REQUIRE(Sprite);
		const std::optional<int32> Slot = StandIns->LayoutSlotOf(Leader);
		REQUIRE(Slot);
		const FStandInDrawn Drawn = StandInsDrawn(Models->Frame, Models->Play->Drawing().Vram(), *Slot);
		REQUIRE(Drawn.Parts > 0);
		CHECK(Drawn.Polygons > 100);
		CHECK(Drawn.Textured > Drawn.Polygons / 2);
		const FSprite* Gear = Original->Play->Field().Sprites().SpriteOf(Leader);
		REQUIRE(Gear != nullptr);
		if (Gear->Animation() == 0)
		{
			// Standing, the model is as tall as its sprite's frame and its soles are where the sprite's are, within what
			// a frame's empty rows and a facing's view take.
			INFO("the model from " << Drawn.Top << " to " << Drawn.Bottom << ", its sprite from " << Sprite->Top << " to " << Sprite->Bottom);
			CHECK(Drawn.Bottom - Drawn.Top > Sprite->Height() * 0.8f);
			CHECK(Drawn.Bottom - Drawn.Top < Sprite->Height() * 1.25f);
			CHECK(std::abs(Drawn.Bottom - Sprite->Bottom) < Sprite->Height() * 0.15f);
			++Standing;
		}
		else
		{
			++Moving;
		}
		// While it walks its parts move from tick to tick.
		const FFieldGearsFrame& Gears = Models->Frame.Gears;
		for (std::size_t Index = 0; Index < Gears.Items.size(); ++Index)
		{
			if (Gears.Items[Index].Slot == *Slot && Gears.Items[Index].Part != nullptr && Gears.Objects[Index].bDrawn)
			{
				const std::array<float, 3> Part = Gears.Objects[Index].Fine.T;
				PartsMoved += PartBefore && *PartBefore != Part ? 1 : 0;
				PartBefore = Part;
				break;
			}
		}
	}
	CHECK(Differences == 0);
	CHECK(Shown > 100);
	CHECK(Standing > 30);
	CHECK(Moving > 30);
	CHECK(PartsMoved > 30);
}

TEST_CASE("In gears, Chu-Chu's model runs in hops that keep her sprite's time", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Models = Testing::EnterFieldMap(*Disc, HangarMap, [](FGameState& Game) { REQUIRE(Game.SetCharacterGear(0, ChuChusGear)); });
	Models->Play->Drawing().SetGearStandIns(EFieldGearModels::Everywhere);
	const FFieldGearStandIns* StandIns = Models->Play->Drawing().GearStandIns();
	REQUIRE(StandIns != nullptr);

	// Her sprite's run goes round eight frames, three ticks each: it is at the top of its bounce in the first and lies
	// flat on the floor in the fifth.
	constexpr int32 RunAnimation = 2;
	constexpr int32 FloorFrame = 4;
	// A run's first ticks blend out of the stand.
	constexpr int32 BlendTicks = 8;
	int32 Height = 0;
	std::optional<int32> FirstFrame;
	int32 Running = 0;
	int32 OnTheFloor = 0;
	int32 AtTheTop = 0;
	for (int32 Tick = 0; Tick < Ticks; ++Tick)
	{
		std::vector<FPadState> Held(static_cast<std::size_t>(Models->Play->VblanksPerTick()));
		for (FPadState& Vblank : Held)
		{
			if (Tick >= BoardAt && Tick < BoardAt + BoardFor)
			{
				Vblank.Buttons = FPadState::Bit(EPadButton::L2) | FPadState::Bit(EPadButton::R2);
			}
			else if (Tick >= WalkFrom && Tick < WalkTo)
			{
				// The release's run button is Circle.
				Vblank.Buttons = FPadState::Bit(EPadButton::Up) | FPadState::Bit(EPadButton::Circle);
			}
		}
		REQUIRE_FALSE(Models->Play->Tick(Held, EFieldCulling::InRange, Models->Frame));
		const int32 Leader = Models->Play->Field().Context().LeaderActor;
		const FSprite* Gear = Models->Play->Field().Sprites().SpriteOf(Leader);
		const FGearModel* Model = StandIns->LoadedModelOf(Leader);
		if (Gear == nullptr || Model == nullptr || !StandIns->IsShown(Leader))
		{
			continue;
		}
		INFO("tick " << Tick << ", the sprite's frame " << Gear->Frame);
		CHECK(StandIns->GearOf(Leader) == ChuChusGear);
		const FModelReach Reach = ReachOf(*Model);
		if (Gear->Animation() != RunAnimation)
		{
			if (Running == 0)
			{
				Height = Reach.Top - Reach.Soles;
			}
			continue;
		}
		FirstFrame = FirstFrame.value_or(Gear->Frame);
		++Running;
		INFO("her soles " << Reach.Soles << " above the floor, standing " << Height << " high");
		if (Gear->Frame == *FirstFrame + FloorFrame)
		{
			CHECK(std::abs(Reach.Soles) * 20 < Height);
			++OnTheFloor;
		}
		else if (Gear->Frame == *FirstFrame && Running > BlendTicks)
		{
			// A quarter of her height off the floor.
			CHECK(Reach.Soles * 4 > Height);
			++AtTheTop;
		}
	}
	CHECK(Height > 0);
	CHECK(Running > 24);
	CHECK(OnTheFloor >= 3);
	CHECK(AtTheTop >= 3);
}

namespace
{
	constexpr int32 PartyGears = 17;
	constexpr uint32 ParticleEffectShift = 20;
	constexpr uint32 ParticleEmitterShift = 17;
	constexpr uint32 ParticleEmitterMask = 7;

	using FPoint = std::array<float, 3>;

	FPoint Minus(const FPoint& Left, const FPoint& Right)
	{
		return {Left[0] - Right[0], Left[1] - Right[1], Left[2] - Right[2]};
	}

	float Dot(const FPoint& Left, const FPoint& Right)
	{
		return Left[0] * Right[0] + Left[1] * Right[1] + Left[2] * Right[2];
	}

	/** How far a point lies from a triangle: from its nearest point, by the part of the triangle's plane it lies over. */
	float DistanceToTriangle(const FPoint& Point, const FPoint& A, const FPoint& B, const FPoint& C)
	{
		const auto From = [&Point](const FPoint& Nearest)
		{
			const FPoint Away = Minus(Point, Nearest);
			return std::sqrt(Dot(Away, Away));
		};
		const auto Along = [](const FPoint& Start, const FPoint& Way, float Part)
		{
			return FPoint{Start[0] + Way[0] * Part, Start[1] + Way[1] * Part, Start[2] + Way[2] * Part};
		};
		const FPoint AB = Minus(B, A);
		const FPoint AC = Minus(C, A);
		const float D1 = Dot(AB, Minus(Point, A));
		const float D2 = Dot(AC, Minus(Point, A));
		if (D1 <= 0.0f && D2 <= 0.0f)
		{
			return From(A);
		}
		const float D3 = Dot(AB, Minus(Point, B));
		const float D4 = Dot(AC, Minus(Point, B));
		if (D3 >= 0.0f && D4 <= D3)
		{
			return From(B);
		}
		const float OverC = D1 * D4 - D3 * D2;
		if (OverC <= 0.0f && D1 >= 0.0f && D3 <= 0.0f)
		{
			return From(Along(A, AB, D1 / (D1 - D3)));
		}
		const float D5 = Dot(AB, Minus(Point, C));
		const float D6 = Dot(AC, Minus(Point, C));
		if (D6 >= 0.0f && D5 <= D6)
		{
			return From(C);
		}
		const float OverB = D5 * D2 - D1 * D6;
		if (OverB <= 0.0f && D2 >= 0.0f && D6 <= 0.0f)
		{
			return From(Along(A, AC, D2 / (D2 - D6)));
		}
		const float OverA = D3 * D6 - D5 * D4;
		if (OverA <= 0.0f && D4 - D3 >= 0.0f && D5 - D6 >= 0.0f)
		{
			return From(Along(B, Minus(C, B), (D4 - D3) / ((D4 - D3) + (D5 - D6))));
		}
		const float Whole = OverA + OverB + OverC;
		return From(Along(Along(A, AB, OverB / Whole), AC, OverC / Whole));
	}

	/** A loaded model's skeleton, as its files give it. */
	std::vector<FGearSkeletonEntry> SkeletonOf(const FGearModel& Model)
	{
		std::vector<FGearSkeletonEntry> Skeleton;
		for (std::size_t Bone = 1; Bone < Model.Bones.size(); ++Bone)
		{
			const int16 Parent = Model.Bones[Bone].Parent;
			Skeleton.push_back({Model.Bones[Bone].Part, Parent == FGearBone::NoParent ? uint16{0xFFFF} : static_cast<uint16>(Parent - 1)});
		}
		return Skeleton;
	}

	/**
	 * How far above the floor each of a posed model's legs reaches, as parts of its height: its legs the bones that reach
	 * the floor in its rest pose (record 0, as its files build it), on either side of its middle.
	 */
	std::array<float, 2> LegsAboveFloor(const FGearModel& Model)
	{
		struct FReach
		{
			int32 Low = 0;
			int32 High = 0;
			bool bAny = false;
		};
		const auto ReachesOf = [&Model](std::span<const FGearBone> Bones)
		{
			std::vector<FReach> Reaches(Bones.size());
			for (const FGearBonePackets& Built : Model.BonePackets)
			{
				const FGearBone& Bone = Bones[Built.Bone];
				if (Bone.bShown == 0)
				{
					continue;
				}
				FReach& Reach = Reaches[Built.Bone];
				for (const FGteVector& Vertex : Model.Parts->Parts[Built.Part].Vertices)
				{
					const int32 Up = -RotTrans(Bone.World, Vertex)[1];
					Reach = Reach.bAny ? FReach{std::min(Reach.Low, Up), std::max(Reach.High, Up), true} : FReach{Up, Up, true};
				}
			}
			return Reaches;
		};
		const auto Floor = [](const std::vector<FReach>& Reaches)
		{
			int32 Low = std::numeric_limits<int32>::max();
			int32 High = std::numeric_limits<int32>::min();
			for (const FReach& Reach : Reaches)
			{
				Low = Reach.bAny ? std::min(Low, Reach.Low) : Low;
				High = Reach.bAny ? std::max(High, Reach.High) : High;
			}
			return std::array<int32, 2>{Low, High};
		};

		const std::vector<FGearSkeletonEntry> Skeleton = SkeletonOf(Model);
		std::vector<FGearBone> Rest = Model.Bones;
		const TResult<FGearPose> RestPose = ReadGearPose(Skeleton, *Model.Motion, 0);
		REQUIRE(RestPose);
		ApplyGearPose(*RestPose, Rest);
		ComputeGearBones(Rest, Model.Scale);
		const std::vector<FReach> AtRest = ReachesOf(Rest);
		const std::array<int32, 2> RestSpan = Floor(AtRest);
		const float Height = static_cast<float>(RestSpan[1] - RestSpan[0]);
		const std::vector<FReach> Posed = ReachesOf(Model.Bones);
		const int32 PosedFloor = Floor(Posed)[0];
		std::array<int32, 2> Legs{std::numeric_limits<int32>::max(), std::numeric_limits<int32>::max()};
		for (std::size_t Bone = 0; Bone < AtRest.size(); ++Bone)
		{
			if (!AtRest[Bone].bAny || !Posed[Bone].bAny || static_cast<float>(AtRest[Bone].Low - RestSpan[0]) > Height * 0.03f)
			{
				continue;
			}
			int32& Leg = Legs[Rest[Bone].World.T[0] < 0 ? 0 : 1];
			Leg = std::min(Leg, Posed[Bone].Low);
		}
		REQUIRE(Legs[0] != std::numeric_limits<int32>::max());
		REQUIRE(Legs[1] != std::numeric_limits<int32>::max());
		return {static_cast<float>(Legs[0] - PosedFloor) / Height, static_cast<float>(Legs[1] - PosedFloor) / Height};
	}
}

TEST_CASE("In gears, every party gear's model stands on both its legs", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Fei, Elly and Citan board their gears in the hangar and stand, three of the seventeen gears at a time.
	constexpr int32 Settle = BoardAt + BoardFor + 60;
	std::vector<bool> Measured(PartyGears, false);
	for (int32 First = 0; First < PartyGears; First += 3)
	{
		auto Models = Testing::EnterFieldMap(*Disc, HangarMap, [First](FGameState& Game)
		{
			Game.SetParty({0, 1, 2});
			for (int32 Character = 0; Character < 3; ++Character)
			{
				REQUIRE(Game.SetCharacterGear(Character, static_cast<uint8>(std::min(First + Character, PartyGears - 1))));
			}
		});
		Models->Play->Drawing().SetGearStandIns(EFieldGearModels::Everywhere);
		const FFieldGearStandIns* StandIns = Models->Play->Drawing().GearStandIns();
		REQUIRE(StandIns != nullptr);
		for (int32 Tick = 0; Tick < Settle; ++Tick)
		{
			std::vector<FPadState> Held(static_cast<std::size_t>(Models->Play->VblanksPerTick()));
			for (FPadState& Vblank : Held)
			{
				Vblank.Buttons = Tick >= BoardAt && Tick < BoardAt + BoardFor ? FPadState::Bit(EPadButton::L2) | FPadState::Bit(EPadButton::R2) : 0;
			}
			REQUIRE_FALSE(Models->Play->Tick(Held, EFieldCulling::InRange, Models->Frame));
		}
		const FEventContext& Context = Models->Play->Field().Context();
		for (int32 Actor = 0; Actor < static_cast<int32>(Context.States.size()); ++Actor)
		{
			const int32 Gear = StandIns->GearOf(Actor);
			const FSprite* Sprite = Models->Play->Field().Sprites().SpriteOf(Actor);
			if (Gear < First || Gear >= First + 3 || !StandIns->IsShown(Actor) || Sprite == nullptr || Sprite->Animation() != 0)
			{
				continue;
			}
			const FGearModel* Model = StandIns->LoadedModelOf(Actor);
			REQUIRE(Model != nullptr);
			const std::array<float, 2> Legs = LegsAboveFloor(*Model);
			INFO("gear " << Gear << ": its legs " << Legs[0] << " and " << Legs[1] << " of its height above the floor");
			// One may stand a little off it, as some sprites hold a foot.
			CHECK(std::max(Legs[0], Legs[1]) < 0.05f);
			Measured[static_cast<std::size_t>(Gear)] = true;
		}
	}
	CHECK(std::count(Measured.begin(), Measured.end(), true) == PartyGears);
}

TEST_CASE("In gears, Weltall's model runs on its legs where its sprite dashes, the dash's thrusters left out", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Models = Testing::EnterFieldMap(*Disc, HangarMap);
	auto Original = Testing::EnterFieldMap(*Disc, HangarMap);
	Models->Play->Drawing().SetGearStandIns(EFieldGearModels::Everywhere);
	const FFieldGearStandIns* StandIns = Models->Play->Drawing().GearStandIns();
	REQUIRE(StandIns != nullptr);

	constexpr int32 RunAnimation = 2;
	constexpr int32 BlendTicks = 8;
	// The particles a frame draws of effects placed on an actor.
	const auto ParticlesOn = [](const FFieldFrame& Frame, const FEventContext& Context, int32 Actor)
	{
		int32 Count = 0;
		for (const FFieldParticleQuad& Particle : Frame.Particles)
		{
			const FFieldParticleEffect& Effect = Context.Particles.Effects[Particle.BlendKey >> ParticleEffectShift];
			Count += Effect.Emitters[Particle.BlendKey >> ParticleEmitterShift & ParticleEmitterMask].Owner == Actor ? 1 : 0;
		}
		return Count;
	};
	int32 Height = 0;
	int32 Running = 0;
	int32 Strides = 0;
	std::optional<int32> SolesBefore;
	int32 Lowest = std::numeric_limits<int32>::max();
	int32 Highest = std::numeric_limits<int32>::min();
	int32 Differences = 0;
	int32 ThrustersDrawn = 0;
	int32 ThrustersOfTheOriginal = 0;
	for (int32 Tick = 0; Tick < Ticks; ++Tick)
	{
		std::vector<FPadState> Held(static_cast<std::size_t>(Models->Play->VblanksPerTick()));
		for (FPadState& Vblank : Held)
		{
			if (Tick >= BoardAt && Tick < BoardAt + BoardFor)
			{
				Vblank.Buttons = FPadState::Bit(EPadButton::L2) | FPadState::Bit(EPadButton::R2);
			}
			else if (Tick >= WalkFrom && Tick < WalkTo)
			{
				Vblank.Buttons = FPadState::Bit(EPadButton::Up) | FPadState::Bit(EPadButton::Circle);
			}
		}
		REQUIRE_FALSE(Models->Play->Tick(Held, EFieldCulling::InRange, Models->Frame));
		REQUIRE_FALSE(Original->Play->Tick(Held, EFieldCulling::InRange, Original->Frame));
		const FEventContext& Context = Models->Play->Field().Context();
		const FEventContext& AsOriginal = Original->Play->Field().Context();
		for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
		{
			Differences += Context.States[Actor].Position == AsOriginal.States[Actor].Position ? 0 : 1;
		}
		Differences += Context.Random->Seed() == AsOriginal.Random->Seed() ? 0 : 1;
		const int32 Leader = Context.LeaderActor;
		if (Tick >= WalkFrom)
		{
			// The game's effects are the original's; only the drawing leaves the thrusters out.
			ThrustersOfTheOriginal += ParticlesOn(Original->Frame, AsOriginal, Leader);
			ThrustersDrawn += ParticlesOn(Models->Frame, Context, Leader);
		}

		const FSprite* Gear = Models->Play->Field().Sprites().SpriteOf(Leader);
		const FGearModel* Model = StandIns->LoadedModelOf(Leader);
		if (Gear == nullptr || Model == nullptr || !StandIns->IsShown(Leader))
		{
			continue;
		}
		const FModelReach Reach = ReachOf(*Model);
		if (Gear->Animation() != RunAnimation)
		{
			Height = Running == 0 ? Reach.Top - Reach.Soles : Height;
			continue;
		}
		CHECK(StandIns->RunsOnFoot(Leader));
		if (++Running <= BlendTicks)
		{
			continue;
		}
		INFO("tick " << Tick << ": its soles " << Reach.Soles << " above the floor, standing " << Height << " high");
		Lowest = std::min(Lowest, Reach.Soles);
		Highest = std::max(Highest, Reach.Soles);
		// Its feet come down and lift again from tick to tick, where a held lunge stands still.
		Strides += SolesBefore && *SolesBefore != Reach.Soles ? 1 : 0;
		SolesBefore = Reach.Soles;
	}
	CHECK(Differences == 0);
	CHECK(Height > 0);
	// The hangar lets it run some 30 ticks.
	CHECK(Running > 20);
	CHECK(Strides > (Running - BlendTicks) * 3 / 4);
	// The lowest of its strides on the floor, the highest little above it, as its battle run is.
	CHECK(std::abs(Lowest) * 50 < Height);
	CHECK(Highest * 10 < Height);
	CHECK(ThrustersOfTheOriginal > 0);
	CHECK(ThrustersDrawn == 0);
}

TEST_CASE("In gears, the party's gears are still drawn with their models after a battle", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FPlayerSettings Settings = Game->Session->Settings();
	Settings.Field.GearModels = EFieldGearModels::OutsideCutscenes;
	Game->Session->SetSettings(Settings);
	Game->EnterField(HangarMap);
	FFieldProgram* Field = Game->Loop->Field();
	REQUIRE(Field != nullptr);
	Game->RunUntil([&] { return Field->Runtime().Context().bFieldRunning; }, 100);
	const auto LeadersModelShown = [&]
	{
		const FFieldGearStandIns* StandIns = Field->Drawing().GearStandIns();
		return StandIns != nullptr && StandIns->IsShown(Field->Runtime().Context().LeaderActor);
	};
	FPadState Board;
	Board.Buttons = FPadState::Bit(EPadButton::L2) | FPadState::Bit(EPadButton::R2);
	for (int32 Frame = 0; Frame < BoardAt + BoardFor; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(Frame < BoardAt ? FPadState{} : Board));
	}
	REQUIRE(Game->RunUntil(LeadersModelShown, 60) < 60);

	// A battle won, as the map's encounters start one: the field comes back to where it left, its party in their gears.
	FEventContext& Context = Field->Runtime().Context();
	Context.bBattleRequested = true;
	Context.Requests.bScriptedBattle = true;
	Context.Requests.bLeavingField = true;
	Game->RunUntil([&] { return Game->Loop->Program() == EGameProgram::Battle; }, 8);
	FKernelState& Kernel = Game->Session->Kernel();
	REQUIRE(Kernel.Field.Saved);
	Game->Loop.reset();
	FBattleProgramEnd End;
	End.Result = FKernelState::BattleWon;
	Kernel.EndBattle(End, Game->Session->Game());
	REQUIRE(Kernel.NextProgram == EGameProgram::Field);
	Game->EnterField(static_cast<uint16>(Game->Session->Game().Place()));
	Field = Game->Loop->Field();
	REQUIRE(Field != nullptr);
	Game->RunUntil([&] { return Field->Runtime().Context().bFieldRunning; }, 100);
	const FEventContext& Back = Field->Runtime().Context();
	REQUIRE(Back.PartySprites != nullptr);
	// The buffers hold the gears' files again, which the field read back without saying what kind they are.
	CHECK(Back.PartySprites->File(0) == FPartySpriteFiles::FirstGearFile + FeisGear);
	CHECK(Game->RunUntil(LeadersModelShown, 30) < 30);
	CHECK(Field->Drawing().GearStandIns()->GearOf(Back.LeaderActor) == FeisGear);
}

namespace
{
	struct FOtherGear
	{
		EFieldGearModelKind Kind = EFieldGearModelKind::BattleGear;
		int16 Number = 0;
		int16 Entry = 0;
		/** How far the model's soles may be from the sprite's feet, of the sprite's height. */
		float Feet = 0.25f;
	};

	/** What a frame draws of one stand-in's model: its extent up the view. */
	std::optional<FExtent> ModelExtent(const FFieldFrame& Frame, int32 LayoutSlot)
	{
		std::optional<FExtent> Extent;
		const FFieldGearsFrame& Gears = Frame.Gears;
		for (std::size_t Index = 0; Index < Gears.Items.size(); ++Index)
		{
			const FFieldGearLayoutItem& Item = Gears.Items[Index];
			const FFieldGearObject& Object = Gears.Objects[Index];
			if (Item.Slot != LayoutSlot || Item.Part == nullptr || !Object.bDrawn)
			{
				continue;
			}
			for (const FGteVector& Vertex : Item.Part->Vertices)
			{
				const float Y = Object.Fine.Turn({static_cast<float>(Vertex[0]), static_cast<float>(Vertex[1]), static_cast<float>(Vertex[2])})[1] + Object.Fine.T[1];
				Extent = Extent ? FExtent{std::min(Extent->Top, Y), std::max(Extent->Bottom, Y)} : FExtent{Y, Y};
			}
		}
		return Extent;
	}

	/**
	 * A map played from a new game twice with the same pad (confirming its messages, walking up for a while), the second
	 * time with the stand-ins where Where has them: the game the same, and each of the map's own gears' sprites drawn as
	 * its model, of the expected ones, standing as tall as the sprite; how many ticks each expected model was shown, and
	 * how many it was measured standing.
	 */
	struct FOtherGearsShown
	{
		std::vector<int32> Ticks;
		int32 Measured = 0;
	};

	FOtherGearsShown PlayWithOtherGears(const FIso9660& Disc, int32 Map, std::span<const FOtherGear> Expected, int32 Played, int32 WalkingFrom,
		EFieldGearModels Where = EFieldGearModels::Everywhere)
	{
		auto Models = Testing::EnterFieldMap(Disc, Map);
		auto Original = Testing::EnterFieldMap(Disc, Map);
		Models->Play->Drawing().SetGearStandIns(Where);
		const FFieldGearStandIns* StandIns = Models->Play->Drawing().GearStandIns();
		REQUIRE(StandIns != nullptr);
		std::vector<int32> Shown(Expected.size(), 0);
		int32 Differences = 0;
		int32 Measured = 0;
		for (int32 Tick = 0; Tick < Played; ++Tick)
		{
			std::vector<FPadState> Held(static_cast<std::size_t>(Models->Play->VblanksPerTick()));
			for (FPadState& Vblank : Held)
			{
				Vblank.Buttons = Tick % 40 == 0 ? FPadState::Bit(EPadButton::Cross) : (Tick >= WalkingFrom ? FPadState::Bit(EPadButton::Up) : 0);
			}
			REQUIRE_FALSE(Models->Play->Tick(Held, EFieldCulling::InRange, Models->Frame));
			REQUIRE_FALSE(Original->Play->Tick(Held, EFieldCulling::InRange, Original->Frame));
			const FEventContext& Context = Models->Play->Field().Context();
			const FEventContext& AsOriginal = Original->Play->Field().Context();
			for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
			{
				const FSprite* Sprite = Models->Play->Field().Sprites().SpriteOf(static_cast<int32>(Actor));
				const FSprite* OriginalSprite = Original->Play->Field().Sprites().SpriteOf(static_cast<int32>(Actor));
				const bool bSame = Context.States[Actor].Position == AsOriginal.States[Actor].Position && (Sprite == nullptr) == (OriginalSprite == nullptr)
					&& (Sprite == nullptr || (Sprite->Animation() == OriginalSprite->Animation() && Sprite->Frame == OriginalSprite->Frame));
				Differences += bSame ? 0 : 1;
				const int32 Number = static_cast<int32>(Actor);
				// The party's own gears (their buffers' sprites) are the other tests'.
				const bool bPartys = Sprite != nullptr && Context.PartySprites != nullptr
					&& (Sprite->PrimaryFile == &Context.PartySprites->Buffer(0) || Sprite->PrimaryFile == &Context.PartySprites->Buffer(1)
						|| Sprite->PrimaryFile == &Context.PartySprites->Buffer(2));
				if (!StandIns->IsShown(Number) || bPartys)
				{
					continue;
				}
				INFO("map " << Map << ", tick " << Tick << ", actor " << Actor);
				const std::optional<std::array<int16, 3>> Model = StandIns->ModelOf(Number);
				REQUIRE(Model);
				const auto Found = std::find_if(Expected.begin(), Expected.end(), [&Model](const FOtherGear& Gear)
					{ return static_cast<int16>(Gear.Kind) == (*Model)[0] && Gear.Number == (*Model)[1] && Gear.Entry == (*Model)[2]; });
				CHECK(Found != Expected.end());
				if (Found == Expected.end())
				{
					continue;
				}
				++Shown[static_cast<std::size_t>(Found - Expected.begin())];
				// Its sprite is left out where the original draws it; standing, the model is about as tall as the sprite.
				CHECK_FALSE(SpriteExtent(Models->Frame, Number));
				const std::optional<FExtent> SpriteDrawn = SpriteExtent(Original->Frame, Number);
				const std::optional<int32> Slot = StandIns->LayoutSlotOf(Number);
				REQUIRE(Slot);
				const std::optional<FExtent> ModelDrawn = ModelExtent(Models->Frame, *Slot);
				if (SpriteDrawn && ModelDrawn && OriginalSprite != nullptr && OriginalSprite->Animation() == 0 && SpriteDrawn->Height() > 8.0f)
				{
					INFO("the model from " << ModelDrawn->Top << " to " << ModelDrawn->Bottom << ", its sprite from " << SpriteDrawn->Top << " to " << SpriteDrawn->Bottom);
					CHECK(ModelDrawn->Height() > SpriteDrawn->Height() * 0.7f);
					CHECK(ModelDrawn->Height() < SpriteDrawn->Height() * 1.4f);
					CHECK(std::abs(ModelDrawn->Bottom - SpriteDrawn->Bottom) < SpriteDrawn->Height() * Found->Feet);
					++Measured;
				}
			}
			Differences += Context.Random->Seed() == AsOriginal.Random->Seed() ? 0 : 1;
		}
		CHECK(Differences == 0);
		return {Shown, Measured};
	}
}

TEST_CASE("The hangar's soldiers' gears, a cave's red gears and its Repair Bot are drawn with their battle models in play, the game playing as the original", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FOtherGear Hangar[] = {{EFieldGearModelKind::SceneEnemy, 16, 0}, {EFieldGearModelKind::SceneEnemy, 9, 1}};
	const FOtherGearsShown InTheHangar = PlayWithOtherGears(*Disc, HangarMap, Hangar, 200, 400, EFieldGearModels::OutsideCutscenes);
	CHECK(InTheHangar.Ticks[0] > 100);
	CHECK(InTheHangar.Measured > 100);
	// The cave's save point keeps a Repair Bot, drawn with the Nomad Fix Bot's model.
	const FOtherGear Cave[] = {{EFieldGearModelKind::BattleGear, 17, 0}, {EFieldGearModelKind::SceneEnemy, 21, 0}};
	const FOtherGearsShown InTheCave = PlayWithOtherGears(*Disc, CaveMap, Cave, 120, 400, EFieldGearModels::OutsideCutscenes);
	CHECK(InTheCave.Ticks[0] > 60);
	CHECK(InTheCave.Ticks[1] > 60);
	CHECK(InTheCave.Measured > 60);
}

TEST_CASE("The desert's Sand Trippers are drawn with their battle model in play as the party walks up to them", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 DesertMap = 180;
	const FOtherGear Desert[] = {{EFieldGearModelKind::SceneEnemy, 43, 0}};
	// They ski about and never stand: shown, but not measured standing.
	CHECK(PlayWithOtherGears(*Disc, DesertMap, Desert, 400, 300, EFieldGearModels::OutsideCutscenes).Ticks[0] > 30);
}

TEST_CASE("At Golgoda Regurus, Amphysvena and Opiomorph are drawn with their battle models, and the crucified gears hang by their hands", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	// The crucified gears hang by their hands from the bars, their feet where the sprites' hang (below where the game
	// stands them), within a few texels.
	constexpr float Hanging = 0.08f;
	const FOtherGear Golgoda[] = {{EFieldGearModelKind::BattleGear, 10, 0}, {EFieldGearModelKind::SceneEnemy, 20, 0}, {EFieldGearModelKind::SceneEnemy, 20, 1},
		{EFieldGearModelKind::BattleGear, 1, 0, Hanging}, {EFieldGearModelKind::BattleGear, 8, 0, Hanging}, {EFieldGearModelKind::BattleGear, 11, 0, Hanging},
		{EFieldGearModelKind::BattleGear, 7, 0}};
	const FOtherGearsShown Shown = PlayWithOtherGears(*Disc, GolgodaMap, Golgoda, 640, 1000);
	CHECK(Shown.Ticks[0] > 100);
	CHECK(Shown.Ticks[1] > 100);
	CHECK(Shown.Ticks[2] > 100);
	CHECK(Shown.Ticks[3] > 30);
	CHECK(Shown.Ticks[5] > 30);
	CHECK(Shown.Measured > 100);
}

TEST_CASE("With the models only in play, Golgoda's cutscene keeps the gears' sprites the models stand in for elsewhere", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	auto InPlay = Testing::EnterFieldMap(*Disc, GolgodaMap);
	auto Always = Testing::EnterFieldMap(*Disc, GolgodaMap);
	InPlay->Play->Drawing().SetGearStandIns(EFieldGearModels::OutsideCutscenes);
	Always->Play->Drawing().SetGearStandIns(EFieldGearModels::Everywhere);
	const FFieldGearStandIns* Models = Always->Play->Drawing().GearStandIns();
	const FFieldGearStandIns* NoModels = InPlay->Play->Drawing().GearStandIns();
	REQUIRE(Models != nullptr);
	REQUIRE(NoModels != nullptr);
	int32 SpritesKept = 0;
	for (int32 Tick = 0; Tick < 400; ++Tick)
	{
		std::vector<FPadState> Held(static_cast<std::size_t>(InPlay->Play->VblanksPerTick()));
		for (FPadState& Vblank : Held)
		{
			Vblank.Buttons = Tick % 40 == 0 ? FPadState::Bit(EPadButton::Cross) : 0;
		}
		REQUIRE_FALSE(InPlay->Play->Tick(Held, EFieldCulling::InRange, InPlay->Frame));
		REQUIRE_FALSE(Always->Play->Tick(Held, EFieldCulling::InRange, Always->Frame));
		INFO("tick " << Tick);
		CHECK(InPlay->Play->Drawing().Cutscenes().InCutscene());
		for (std::size_t Actor = 0; Actor < InPlay->Play->Field().Context().States.size(); ++Actor)
		{
			const int32 Number = static_cast<int32>(Actor);
			CHECK_FALSE(NoModels->IsShown(Number));
			if (Models->IsShown(Number))
			{
				// Where the other draws the model, this draws the sprite.
				CHECK(SpriteExtent(InPlay->Frame, Number));
				++SpritesKept;
			}
		}
	}
	CHECK(SpritesKept > 300);
}

TEST_CASE("With the models only in play, a talk to the Repair Bot is no cutscene: it stays a model while it speaks", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FPlayerSettings Settings = Game->Session->Settings();
	Settings.Field.GearModels = EFieldGearModels::OutsideCutscenes;
	Game->Session->SetSettings(Settings);
	Game->EnterField(CaveMap);
	FFieldProgram* Field = Game->Loop->Field();
	REQUIRE(Field != nullptr);
	Game->RunUntil([&] { return Field->Runtime().Context().bFieldRunning; }, 100);
	// The Repair Bot: the actor drawn with the Nomad Fix Bot's model.
	const auto RepairBot = [&]
	{
		const FFieldGearStandIns* StandIns = Field->Drawing().GearStandIns();
		for (int32 Actor = 0; StandIns != nullptr && Actor < static_cast<int32>(Field->Runtime().Context().States.size()); ++Actor)
		{
			const std::optional<std::array<int16, 3>> Model = StandIns->ModelOf(Actor);
			if (StandIns->IsShown(Actor) && Model && (*Model)[0] == static_cast<int16>(EFieldGearModelKind::SceneEnemy) && (*Model)[1] == 21 && (*Model)[2] == 0)
			{
				return Actor;
			}
		}
		return -1;
	};
	REQUIRE(Game->RunUntil([&] { return RepairBot() >= 0; }, 60) < 60);
	const int32 Bot = RepairBot();

	// Its talk as the player's button starts it: it takes the party and asks what to do, and waits.
	FEventContext& Context = Field->Runtime().Context();
	REQUIRE(FEventMachine::StartScript(Context, Bot, FEventMachine::TalkScript, FEventMachine::TalkPriority));
	int32 Asked = 0;
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(FPadState{}));
		INFO("frame " << Frame);
		CHECK_FALSE(Field->Drawing().Cutscenes().InCutscene());
		CHECK(Field->Drawing().GearStandIns()->IsShown(Bot));
		Asked += Context.PlayerLock != 0 && Context.Messages.OpenMask != 0 ? 1 : 0;
	}
	CHECK(Asked > 60);
}

TEST_CASE("In gears, a model's cloth stays on it as it runs and trails behind it, on a map with no gears of its own", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Renmazuo's cape: its first cloth, on bone 15. The hangar loads no gear models of its own.
	constexpr int32 Renmazuo = 5;
	auto Models = Testing::EnterFieldMap(*Disc, HangarMap, [](FGameState& Game) { REQUIRE(Game.SetCharacterGear(0, Renmazuo)); });
	REQUIRE_FALSE(Models->Play->Field().Context().Gears.Models);
	Models->Play->Drawing().SetGearStandIns(EFieldGearModels::Everywhere);
	const FFieldGearStandIns* StandIns = Models->Play->Drawing().GearStandIns();
	REQUIRE(StandIns != nullptr);

	// The farthest any column's anchor lies from its bone's drawn place, under the view.
	const auto AnchorsFromBone = [&](int32 Leader) -> std::optional<float>
	{
		const FGearModel* Model = StandIns->LoadedModelOf(Leader);
		const std::optional<int32> Slot = StandIns->LayoutSlotOf(Leader);
		if (Model == nullptr || !Slot || Model->Cloths.empty())
		{
			return std::nullopt;
		}
		const FGearCloth& Cloth = Model->Cloths.front();
		const FFieldGearsFrame& Gears = Models->Frame.Gears;
		std::optional<std::array<float, 3>> Bone;
		for (std::size_t Index = 0; Index < Gears.Items.size(); ++Index)
		{
			const FFieldGearLayoutItem& Item = Gears.Items[Index];
			if (Item.Slot == *Slot && Item.Packets != nullptr && Item.Packets->Bone == static_cast<uint16>(Cloth.Bone) && Gears.Objects[Index].bDrawn)
			{
				Bone = Gears.Objects[Index].Fine.T;
			}
		}
		const auto Space = std::find_if(Gears.ClothSpaces.begin(), Gears.ClothSpaces.end(), [&Slot](const FFieldGearClothSpace& Placed) { return Placed.Slot == *Slot; });
		if (!Bone || Space == Gears.ClothSpaces.end())
		{
			return std::nullopt;
		}
		float Farthest = 0.0f;
		for (const uint16 Start : Cloth.ColumnStarts)
		{
			const FGteVector& Anchor = Cloth.Points[Start].Position;
			const std::array<float, 3> Turned = Space->Space.Turn({static_cast<float>(Anchor[0]), static_cast<float>(Anchor[1]), static_cast<float>(Anchor[2])});
			const float X = Turned[0] + Space->Space.T[0] - (*Bone)[0];
			const float Y = Turned[1] + Space->Space.T[1] - (*Bone)[1];
			const float Z = Turned[2] + Space->Space.T[2] - (*Bone)[2];
			Farthest = std::max(Farthest, std::sqrt(X * X + Y * Y + Z * Z));
		}
		return Farthest;
	};
	// How far behind its anchors the cape's columns end, on average: the hangar's run goes along +z.
	const auto Trail = [&](int32 Leader)
	{
		const FGearCloth& Cape = StandIns->LoadedModelOf(Leader)->Cloths.front();
		float Behind = 0.0f;
		for (const uint16 Start : Cape.ColumnStarts)
		{
			std::size_t End = Start;
			while (Cape.Points[End].Length != 0)
			{
				++End;
			}
			Behind += static_cast<float>(Cape.Points[Start].Position[2] - Cape.Points[End].Position[2]);
		}
		return Behind / static_cast<float>(Cape.ColumnStarts.size());
	};
	// The ticks a run's start takes the cape to stream back.
	constexpr int32 Streaming = 8;
	std::optional<float> Standing;
	float StandingTrail = 0.0f;
	int32 Running = 0;
	for (int32 Tick = 0; Tick < Ticks; ++Tick)
	{
		std::vector<FPadState> Held(static_cast<std::size_t>(Models->Play->VblanksPerTick()));
		for (FPadState& Vblank : Held)
		{
			if (Tick >= BoardAt && Tick < BoardAt + BoardFor)
			{
				Vblank.Buttons = FPadState::Bit(EPadButton::L2) | FPadState::Bit(EPadButton::R2);
			}
			else if (Tick >= WalkFrom && Tick < WalkTo)
			{
				Vblank.Buttons = FPadState::Bit(EPadButton::Up) | FPadState::Bit(EPadButton::Circle);
			}
		}
		REQUIRE_FALSE(Models->Play->Tick(Held, EFieldCulling::InRange, Models->Frame));
		const int32 Leader = Models->Play->Field().Context().LeaderActor;
		const FSprite* Gear = Models->Play->Field().Sprites().SpriteOf(Leader);
		const std::optional<float> Distance = StandIns->IsShown(Leader) ? AnchorsFromBone(Leader) : std::nullopt;
		if (Gear == nullptr || !Distance)
		{
			continue;
		}
		INFO("tick " << Tick << ": the anchors " << *Distance << " from their bone, standing " << Standing.value_or(0.0f));
		if (Tick < WalkFrom)
		{
			Standing = std::max(Standing.value_or(0.0f), *Distance);
			StandingTrail = std::max(StandingTrail, Trail(Leader));
			continue;
		}
		REQUIRE(Standing);
		// The run bends the body about the bone, not the anchors away from it.
		CHECK(*Distance < *Standing * 1.25f);
		if (Tick >= WalkTo || Gear->Animation() == 0)
		{
			continue;
		}
		// Running, it streams back as a moving model's cloth does, farther than the breeze ever blows it standing.
		if (++Running > Streaming)
		{
			INFO("the cape " << Trail(Leader) << " behind its anchors, at most " << StandingTrail << " standing");
			CHECK(Trail(Leader) > StandingTrail * 1.5f);
		}
	}
	CHECK(Running > 20);
}

TEST_CASE("In the desert's charge, Weltall's model runs over the sand the scenery drifts past, angled as the player steers", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// The charge on the flagship: the scenery drifts 42 units a tick along +x under Fei, whose sprite runs where he stands
	// facing -x; Right moves him 11 units a tick along +z, facing that way.
	constexpr int32 DesertMap = 180;
	constexpr int32 Confirming = 280;
	constexpr int32 SteerFrom = 380;
	constexpr int32 SteerTo = 470;
	constexpr int32 RunAnimation = 2;
	constexpr int32 Turn = 0x1000;
	// A model's yaw as its root has it: the actor's, three quarters of a turn on.
	constexpr int32 RootOffset = 0xC00;
	constexpr int32 Ahead = 0x800;
	constexpr int32 SpriteRight = 0xC00;
	auto Models = Testing::EnterFieldMap(*Disc, DesertMap);
	Models->Play->Drawing().SetGearStandIns(EFieldGearModels::Everywhere);
	const FFieldGearStandIns* StandIns = Models->Play->Drawing().GearStandIns();
	REQUIRE(StandIns != nullptr);

	const auto AwayFrom = [](int32 Yaw, int32 From)
	{
		const int32 Delta = ((Yaw - From) % Turn + Turn) % Turn;
		return std::min(Delta, Turn - Delta);
	};
	int32 Idle = 0;
	int32 IdleStrides = 0;
	int32 Steered = 0;
	std::optional<int32> SolesBefore;
	for (int32 Tick = 0; Tick < SteerTo; ++Tick)
	{
		std::vector<FPadState> Held(static_cast<std::size_t>(Models->Play->VblanksPerTick()));
		for (FPadState& Vblank : Held)
		{
			Vblank.Buttons = Tick < Confirming ? (Tick % 40 == 0 ? FPadState::Bit(EPadButton::Cross) : 0) : Tick >= SteerFrom ? FPadState::Bit(EPadButton::Right) : 0;
		}
		REQUIRE_FALSE(Models->Play->Tick(Held, EFieldCulling::InRange, Models->Frame));
		const FEventContext& Context = Models->Play->Field().Context();
		const int32 Leader = Context.LeaderActor;
		const FSprite* Gear = Models->Play->Field().Sprites().SpriteOf(Leader);
		const FGearModel* Model = StandIns->LoadedModelOf(Leader);
		if (Tick < Confirming + 20 || Gear == nullptr || Model == nullptr || !StandIns->IsShown(Leader) || Gear->Animation() != RunAnimation)
		{
			continue;
		}
		const int32 Yaw = (Model->Bones[0].Angles[1] - RootOffset + Turn) % Turn;
		const int32 Soles = ReachOf(*Model).Soles;
		INFO("tick " << Tick << ": the model faces " << Yaw << ", the actor " << Context.States[static_cast<std::size_t>(Leader)].Yaw);
		if (Tick < SteerFrom)
		{
			// Where he stands, it runs ahead over the sand as it goes by.
			CHECK(AwayFrom(Yaw, Ahead) < 0x40);
			IdleStrides += SolesBefore && *SolesBefore != Soles ? 1 : 0;
			++Idle;
		}
		else if (Tick > SteerFrom + 20)
		{
			// Steering right, it runs ahead and a little right, as over the sand it goes: not sideways as the sprite faces.
			CHECK(Context.States[static_cast<std::size_t>(Leader)].Yaw == SpriteRight);
			CHECK(AwayFrom(Yaw, Ahead) > 0x80);
			CHECK(AwayFrom(Yaw, Ahead) < 0x200);
			++Steered;
		}
		SolesBefore = Soles;
	}
	CHECK(Idle > 60);
	CHECK(IdleStrides > Idle * 3 / 4);
	CHECK(Steered > 60);
}

TEST_CASE("In gears, Brigandier's and Fenrir's models dash with the map's thrusters on their backs, where their poses carry them", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 Brigandier = 4;
	constexpr int32 Fenrir = 11;
	constexpr std::array<int32, 2> FenrirsArms{6, 16};

	// How far the highest bone of an arm lies above its shoulder, in the model's own units.
	const auto ArmRaised = [](const FGearModel& Model, int32 Arm)
	{
		const std::vector<FGearSkeletonEntry> Skeleton = SkeletonOf(Model);
		const int32 Shoulder = -Model.Bones[static_cast<std::size_t>(Arm)].World.T[1];
		int32 Highest = Shoulder;
		for (std::size_t Bone = 1; Bone < Model.Bones.size(); ++Bone)
		{
			Highest = IsGearBoneBelow(Skeleton, static_cast<int32>(Bone), Arm) ? std::max(Highest, -Model.Bones[Bone].World.T[1]) : Highest;
		}
		return Highest - Shoulder;
	};

	// The nearest of an actor's particles to the model's surface, in view units.
	const auto NearestParticle = [](const FFieldFrame& Frame, const FEventContext& Context, int32 Actor, int32 LayoutSlot) -> std::optional<float>
	{
		std::vector<std::array<FPoint, 3>> Surface;
		const FFieldGearsFrame& Gears = Frame.Gears;
		for (std::size_t Index = 0; Index < Gears.Items.size(); ++Index)
		{
			const FFieldGearLayoutItem& Item = Gears.Items[Index];
			const FFieldGearObject& Object = Gears.Objects[Index];
			if (Item.Slot != LayoutSlot || Item.Part == nullptr || !Object.bDrawn)
			{
				continue;
			}
			std::vector<FPoint> Placed;
			for (const FGteVector& Vertex : Item.Part->Vertices)
			{
				const FPoint Turned = Object.Fine.Turn({static_cast<float>(Vertex[0]), static_cast<float>(Vertex[1]), static_cast<float>(Vertex[2])});
				Placed.push_back({Turned[0] + Object.Fine.T[0], Turned[1] + Object.Fine.T[1], Turned[2] + Object.Fine.T[2]});
			}
			for (const FMdlPrimitive& Primitive : Item.Part->Primitives)
			{
				const std::array<uint16, 4>& Corners = Primitive.Vertices;
				const bool bQuad = Primitive.Traits().Corners == 4;
				if (std::any_of(Corners.begin(), Corners.begin() + (bQuad ? 4 : 3), [&Placed](uint16 Corner) { return Corner >= Placed.size(); }))
				{
					continue;
				}
				Surface.push_back({Placed[Corners[0]], Placed[Corners[1]], Placed[Corners[2]]});
				if (bQuad)
				{
					Surface.push_back({Placed[Corners[1]], Placed[Corners[3]], Placed[Corners[2]]});
				}
			}
		}
		std::optional<float> Least;
		for (const FFieldParticleQuad& Particle : Frame.Particles)
		{
			const FFieldParticleEffect& Effect = Context.Particles.Effects[Particle.BlendKey >> ParticleEffectShift];
			if (Effect.Emitters[Particle.BlendKey >> ParticleEmitterShift & ParticleEmitterMask].Owner != Actor)
			{
				continue;
			}
			const FPoint At{static_cast<float>(Particle.Matrix.T[0]), static_cast<float>(Particle.Matrix.T[1]), static_cast<float>(Particle.Matrix.T[2])};
			for (const std::array<FPoint, 3>& Triangle : Surface)
			{
				const float Distance = DistanceToTriangle(At, Triangle[0], Triangle[1], Triangle[2]);
				Least = Least ? std::min(*Least, Distance) : Distance;
			}
		}
		return Least;
	};
	// Brigandier glides into battle; Fenrir runs there with its sword held ready, and on the field holds the lunge too.
	for (const int32 Gear : {Brigandier, Fenrir})
	{
		INFO("gear " << Gear);
		const auto Ride = [Gear](FGameState& Game) { REQUIRE(Game.SetCharacterGear(0, static_cast<uint8>(Gear))); };
		auto Models = Testing::EnterFieldMap(*Disc, HangarMap, Ride);
		auto Original = Testing::EnterFieldMap(*Disc, HangarMap, Ride);
		Models->Play->Drawing().SetGearStandIns(EFieldGearModels::Everywhere);
		const FFieldGearStandIns* StandIns = Models->Play->Drawing().GearStandIns();
		REQUIRE(StandIns != nullptr);
		int32 Dashing = 0;
		int32 Differences = 0;
		for (int32 Tick = 0; Tick < Ticks; ++Tick)
		{
			std::vector<FPadState> Held(static_cast<std::size_t>(Models->Play->VblanksPerTick()));
			for (FPadState& Vblank : Held)
			{
				if (Tick >= BoardAt && Tick < BoardAt + BoardFor)
				{
					Vblank.Buttons = FPadState::Bit(EPadButton::L2) | FPadState::Bit(EPadButton::R2);
				}
				else if (Tick >= WalkFrom && Tick < WalkTo)
				{
					Vblank.Buttons = FPadState::Bit(EPadButton::Up) | FPadState::Bit(EPadButton::Circle);
				}
			}
			REQUIRE_FALSE(Models->Play->Tick(Held, EFieldCulling::InRange, Models->Frame));
			REQUIRE_FALSE(Original->Play->Tick(Held, EFieldCulling::InRange, Original->Frame));
			const FEventContext& Context = Models->Play->Field().Context();
			const FEventContext& AsOriginal = Original->Play->Field().Context();
			Differences += Context.Random->Seed() == AsOriginal.Random->Seed() ? 0 : 1;
			const int32 Leader = Context.LeaderActor;
			const std::optional<int32> Slot = StandIns->LayoutSlotOf(Leader);
			if (!StandIns->Thrusts(Leader) || !Slot)
			{
				continue;
			}
			CHECK_FALSE(StandIns->RunsOnFoot(Leader));
			const FGearModel* Model = StandIns->LoadedModelOf(Leader);
			REQUIRE(Model != nullptr);
			const FModelReach Reach = ReachOf(*Model);
			const std::optional<FExtent> Drawn = ModelExtent(Models->Frame, *Slot);
			const std::optional<float> Nearest = NearestParticle(Models->Frame, Context, Leader, *Slot);
			if (!Drawn || !Nearest)
			{
				continue;
			}
			INFO("tick " << Tick << ": its nearest thruster particle " << *Nearest << " from the model, drawn " << Drawn->Height() << " high");
			// The flame comes out of it, not out of the air beside it (a third of its height away, drawn where the sprite's
			// back is): the place its newest particles start is moved onto the model, their emitters' spread about it left.
			CHECK(*Nearest < Drawn->Height() * 0.1f);
			CHECK(Reach.Top > Reach.Soles);
			if (Gear == Fenrir)
			{
				// Its arms down at its sides as it stands, where the lunge raises a fist.
				for (const int32 Arm : FenrirsArms)
				{
					CHECK(ArmRaised(*Model, Arm) == 0);
				}
			}
			++Dashing;
		}
		CHECK(Differences == 0);
		CHECK(Dashing > 20);
	}
}

TEST_CASE("In gears with Citan's sword put away, the second Heimdal stands in as the first and Fenrir as itself in the first's stance", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 Heimdal = 3;
	constexpr int32 Fenrir = 11;
	constexpr int32 SwordHeimdal = 16;
	constexpr uint8 SwordCitan = 9;
	/** Fenrir's sword strip, its fist and its open hand on the other side. */
	constexpr std::size_t SwordStrip = 46;
	constexpr std::size_t Fist = 50;
	constexpr std::size_t OpenHand = 51;
	constexpr int32 Settle = BoardAt + BoardFor + 60;

	struct FStood
	{
		int32 ModelNumber = -1;
		std::size_t Bones = 0;
		std::array<float, 2> Legs{};
		std::array<uint8, 3> Hands{};
	};
	// Fei rides the gear in the hangar and stands in it; its stand-in is found by the model it draws.
	const auto Stand = [&Disc](int32 Gear, bool bWithoutSword, int32 Drawn)
	{
		auto Models = Testing::EnterFieldMap(*Disc, HangarMap, [Gear, bWithoutSword](FGameState& Game)
		{
			REQUIRE(Game.SetCharacterGear(0, static_cast<uint8>(Gear)));
			FCharacterLooks Looks;
			if (bWithoutSword)
			{
				REQUIRE(Looks.Switch(SwordCitan));
			}
			Game.SetCharacterLooks(Looks);
		});
		Models->Play->Drawing().SetGearStandIns(EFieldGearModels::Everywhere);
		const FFieldGearStandIns* StandIns = Models->Play->Drawing().GearStandIns();
		REQUIRE(StandIns != nullptr);
		for (int32 Tick = 0; Tick < Settle; ++Tick)
		{
			std::vector<FPadState> Held(static_cast<std::size_t>(Models->Play->VblanksPerTick()));
			for (FPadState& Vblank : Held)
			{
				Vblank.Buttons = Tick >= BoardAt && Tick < BoardAt + BoardFor ? FPadState::Bit(EPadButton::L2) | FPadState::Bit(EPadButton::R2) : 0;
			}
			REQUIRE_FALSE(Models->Play->Tick(Held, EFieldCulling::InRange, Models->Frame));
		}
		FStood Stood;
		const FEventContext& Context = Models->Play->Field().Context();
		for (int32 Actor = 0; Actor < static_cast<int32>(Context.States.size()); ++Actor)
		{
			if (StandIns->GearOf(Actor) != Drawn || !StandIns->IsShown(Actor))
			{
				continue;
			}
			const FGearModel* Model = StandIns->LoadedModelOf(Actor);
			const std::optional<std::array<int16, 3>> Of = StandIns->ModelOf(Actor);
			REQUIRE(Model != nullptr);
			REQUIRE(Of);
			Stood.ModelNumber = (*Of)[1];
			Stood.Bones = Model->Bones.size();
			Stood.Legs = LegsAboveFloor(*Model);
			if (Stood.Bones > OpenHand)
			{
				Stood.Hands = {Model->Bones[SwordStrip].bShown, Model->Bones[Fist].bShown, Model->Bones[OpenHand].bShown};
			}
		}
		return Stood;
	};

	const FStood Second = Stand(SwordHeimdal, true, Heimdal);
	CHECK(Second.ModelNumber == Heimdal);
	CHECK(Second.Bones == 45);
	CHECK(std::max(Second.Legs[0], Second.Legs[1]) < 0.05f);

	// Fenrir as the story has it: its own stance, the open hand its opening shows; without the sword, the first Heimdal's
	// stance worn, its fist shown as Heimdal's is, both feet on the floor.
	const FStood Own = Stand(Fenrir, false, Fenrir);
	const FStood Worn = Stand(Fenrir, true, Fenrir);
	CHECK(Own.ModelNumber == Fenrir);
	CHECK(Own.Hands == std::array<uint8, 3>{0, 0, 1});
	CHECK(Worn.ModelNumber == Fenrir);
	CHECK(Worn.Bones == 56);
	CHECK(Worn.Hands == std::array<uint8, 3>{0, 1, 0});
	INFO("Fenrir's legs " << Worn.Legs[0] << " and " << Worn.Legs[1] << " of its height above the floor");
	CHECK(std::max(Worn.Legs[0], Worn.Legs[1]) < 0.05f);
}

TEST_CASE("In gears with Fei drawn as Id, his gear stands in as Id's red gear of its time, at its own size against his gear's", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 Settle = BoardAt + BoardFor + 60;
	constexpr int16 SceneModel = static_cast<int16>(EFieldGearModelKind::SceneEnemy);

	struct FStood
	{
		std::array<int16, 3> Model{-1, -1, -1};
		std::size_t Bones = 0;
		std::array<float, 2> Legs{};
		float Scale = 0.0f;
	};
	// Fei rides his gear in the hangar and stands in it; his stand-in is his gear's or a red gear's (the hangar's soldiers'
	// gears are scene models too).
	const auto Stand = [&Disc](uint8 Gear, bool bId)
	{
		auto Models = Testing::EnterFieldMap(*Disc, HangarMap, [Gear, bId](FGameState& Game)
		{
			if (Gear != FCharacterLooks::Weltall)
			{
				REQUIRE(Game.CopyGear(FCharacterLooks::Weltall, Gear));
				REQUIRE(Game.SetCharacterGear(FCharacterLooks::Fei, Gear));
			}
			FCharacterLooks Looks;
			if (bId)
			{
				REQUIRE(Looks.Switch(FCharacterLooks::Fei));
			}
			Game.SetCharacterLooks(Looks);
		});
		Models->Play->Drawing().SetGearStandIns(EFieldGearModels::Everywhere);
		const FFieldGearStandIns* StandIns = Models->Play->Drawing().GearStandIns();
		REQUIRE(StandIns != nullptr);
		for (int32 Tick = 0; Tick < Settle; ++Tick)
		{
			std::vector<FPadState> Held(static_cast<std::size_t>(Models->Play->VblanksPerTick()));
			for (FPadState& Vblank : Held)
			{
				Vblank.Buttons = Tick >= BoardAt && Tick < BoardAt + BoardFor ? FPadState::Bit(EPadButton::L2) | FPadState::Bit(EPadButton::R2) : 0;
			}
			REQUIRE_FALSE(Models->Play->Tick(Held, EFieldCulling::InRange, Models->Frame));
		}
		FStood Stood;
		const FEventContext& Context = Models->Play->Field().Context();
		for (int32 Actor = 0; Actor < static_cast<int32>(Context.States.size()); ++Actor)
		{
			const std::optional<std::array<int16, 3>> Of = StandIns->ModelOf(Actor);
			const bool bFeis = Of && (((*Of)[0] == SceneModel && ((*Of)[1] == 74 || (*Of)[1] == 70)) || StandIns->GearOf(Actor) == Gear);
			if (!bFeis || !StandIns->IsShown(Actor))
			{
				continue;
			}
			const FGearModel* Model = StandIns->LoadedModelOf(Actor);
			REQUIRE(Model != nullptr);
			REQUIRE(Model->Placement);
			Stood.Model = *Of;
			Stood.Bones = Model->Bones.size();
			Stood.Legs = LegsAboveFloor(*Model);
			const std::array<float, 3>& Across = {Model->Placement->M[0][0], Model->Placement->M[1][0], Model->Placement->M[2][0]};
			Stood.Scale = std::sqrt(Across[0] * Across[0] + Across[1] * Across[1] + Across[2] * Across[2]);
		}
		return Stood;
	};

	// The red Weltall of his first disc's fights over Weltall and Weltall-2, the red gear of his second disc's over
	// Xenogears: each drawn at the scale his own gear would be, so it keeps its own size against it, on the floor.
	for (const auto [Gear, Scene, Bones] : {std::tuple<uint8, int16, std::size_t>{FCharacterLooks::Weltall, 74, 62}, {FCharacterLooks::Weltall2, 74, 62},
			 {FCharacterLooks::Xenogears, 70, 37}})
	{
		INFO("gear " << int32(Gear));
		const FStood Own = Stand(Gear, false);
		const FStood Red = Stand(Gear, true);
		CHECK(Own.Model == std::array<int16, 3>{static_cast<int16>(EFieldGearModelKind::BattleGear), Gear, 0});
		CHECK(Red.Model == std::array<int16, 3>{SceneModel, Scene, 0});
		CHECK(Red.Bones == Bones);
		REQUIRE(Own.Scale > 0.0f);
		CHECK(std::abs(Red.Scale / Own.Scale - 1.0f) < 0.01f);
		INFO("legs " << Red.Legs[0] << " and " << Red.Legs[1] << " of its height above the floor");
		CHECK(std::min(Red.Legs[0], Red.Legs[1]) < 0.05f);
	}
}

TEST_CASE("In the Yggdrasil's hangar with Fei drawn as Id, his parked gear stands in as Id's red gear, the crucified keep the story's", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	constexpr int32 YggdrasilHangar = 291;
	constexpr std::array<int16, 3> Xenogears{static_cast<int16>(EFieldGearModelKind::BattleGear), FCharacterLooks::Xenogears, 0};
	constexpr std::array<int16, 3> RedGear{static_cast<int16>(EFieldGearModelKind::SceneEnemy), 70, 0};
	// The models the hangar's actors stand in with after some ticks, Fei in Xenogears, drawn as Id or not.
	const auto Parked = [&Disc](bool bId)
	{
		auto Models = Testing::EnterFieldMap(*Disc, YggdrasilHangar, [bId](FGameState& Game)
		{
			REQUIRE(Game.SetCharacterGear(FCharacterLooks::Fei, FCharacterLooks::Xenogears));
			FCharacterLooks Looks;
			if (bId)
			{
				REQUIRE(Looks.Switch(FCharacterLooks::Fei));
			}
			Game.SetCharacterLooks(Looks);
		});
		Models->Play->Drawing().SetGearStandIns(EFieldGearModels::Everywhere);
		const FFieldGearStandIns* StandIns = Models->Play->Drawing().GearStandIns();
		REQUIRE(StandIns != nullptr);
		for (int32 Tick = 0; Tick < 60; ++Tick)
		{
			std::vector<FPadState> Held(static_cast<std::size_t>(Models->Play->VblanksPerTick()));
			REQUIRE_FALSE(Models->Play->Tick(Held, EFieldCulling::InRange, Models->Frame));
		}
		std::vector<std::array<int16, 3>> Shown;
		for (int32 Actor = 0; Actor < static_cast<int32>(Models->Play->Field().Context().States.size()); ++Actor)
		{
			if (const std::optional<std::array<int16, 3>> Of = StandIns->ModelOf(Actor); Of && StandIns->IsShown(Actor))
			{
				Shown.push_back(*Of);
			}
		}
		return Shown;
	};
	const std::vector<std::array<int16, 3>> Own = Parked(false);
	const std::vector<std::array<int16, 3>> AsId = Parked(true);
	CHECK(std::count(Own.begin(), Own.end(), Xenogears) == 1);
	CHECK(std::count(Own.begin(), Own.end(), RedGear) == 0);
	CHECK(std::count(AsId.begin(), AsId.end(), Xenogears) == 0);
	CHECK(std::count(AsId.begin(), AsId.end(), RedGear) == 1);
	CHECK(AsId.size() == Own.size());
}

TEST_CASE("In the Yggdrasil's hangar Elly's Regurus is parked in Vierge's place, drawn with its own model", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	constexpr int32 YggdrasilHangar = 291;
	constexpr uint8 Elly = 1;
	constexpr uint8 Vierge = 2;
	constexpr uint8 Regurus = 10;
	const auto Parked = [&Disc](uint8 EllysGear)
	{
		auto Models = Testing::EnterFieldMap(*Disc, YggdrasilHangar, [EllysGear](FGameState& Game)
		{
			Game.SetJoinedCharacters(static_cast<uint16>(Game.JoinedCharacters() | 1 << Elly));
			Game.SetMenuCharacters(static_cast<uint16>(Game.MenuCharacters() | 1 << Elly));
			REQUIRE(Game.SetCharacterGear(Elly, EllysGear));
		});
		Models->Play->Drawing().SetGearStandIns(EFieldGearModels::Everywhere);
		const FFieldGearStandIns* StandIns = Models->Play->Drawing().GearStandIns();
		REQUIRE(StandIns != nullptr);
		for (int32 Tick = 0; Tick < 60; ++Tick)
		{
			std::vector<FPadState> Held(static_cast<std::size_t>(Models->Play->VblanksPerTick()));
			REQUIRE_FALSE(Models->Play->Tick(Held, EFieldCulling::InRange, Models->Frame));
		}
		std::vector<std::array<int16, 3>> Shown;
		for (int32 Actor = 0; Actor < static_cast<int32>(Models->Play->Field().Context().States.size()); ++Actor)
		{
			if (const std::optional<std::array<int16, 3>> Of = StandIns->ModelOf(Actor); Of && StandIns->IsShown(Actor))
			{
				Shown.push_back(*Of);
			}
		}
		return Shown;
	};
	const auto Count = [](const std::vector<std::array<int16, 3>>& Shown, uint8 Gear)
	{ return std::count(Shown.begin(), Shown.end(), std::array<int16, 3>{static_cast<int16>(EFieldGearModelKind::BattleGear), Gear, 0}); };
	const std::vector<std::array<int16, 3>> InVierge = Parked(Vierge);
	const std::vector<std::array<int16, 3>> InRegurus = Parked(Regurus);
	CHECK(Count(InVierge, Vierge) == 1);
	CHECK(Count(InVierge, Regurus) == 0);
	CHECK(Count(InRegurus, Vierge) == 0);
	CHECK(Count(InRegurus, Regurus) == 1);
	CHECK(InRegurus.size() == InVierge.size());
}
