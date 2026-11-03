#include "Field/FieldGearStandIns.h"

#include "Battle/Show/BattleObjectFiles.h"
#include "Battle/Show/BattleSceneGraphics.h"
#include "Field/EventContext.h"
#include "Field/FieldCamera.h"
#include "Field/FieldPartySprites.h"
#include "Field/FieldRuntime.h"
#include "Game/GameState.h"
#include "Graphics/Sprite.h"
#include "Graphics/SpriteFile.h"
#include "Graphics/Vram.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

namespace Weltall
{
	namespace
	{
		/** The animations every party gear's sprite file numbers alike: stop, walk, run and jump. */
		constexpr int32 StopAnimation = 0;
		constexpr int32 WalkAnimation = 1;
		constexpr int32 RunAnimation = 2;
		constexpr int32 JumpAnimation = 3;
		/** The run's ending, most gears' dash once more. */
		constexpr int32 RunEndingAnimation = 4;
		/** The frame lists an animation's header holds, by its facing mode. */
		constexpr uint32 FacingLists[4] = {1, 3, 5, 1};
		constexpr uint32 FrameNumberMask = 0x1FF;
		/** A frame's height above its origin, in texels. */
		constexpr uint32 FrameTopByte = 3;
		constexpr uint8 SemiTransparentCode = 2;
		constexpr uint8 RawCode = 1;

		/**
		 * The animation records every gear's sequence archive numbers alike, the party's and the enemies': its rest pose
		 * (straight, the arms down: the pose its files build it in, which no battle stands it in), the way it closes in
		 * on a target (a run, a glide or a flight, each gear's own), its leap, and the lunge its approach ends in.
		 */
		constexpr int32 RestRecord = 0;
		constexpr int32 MoveRecord = 2;
		constexpr int32 LeapRecord = 3;
		constexpr int32 DashRecord = 25;

		/**
		 * How a party gear stands: some way from its rest pose toward the first tick of one of its stances, the stance
		 * and the way chosen against its sprite's standing frames and with its feet on the floor: its battle stance,
		 * which many gears take side on, or a stance it takes facing ahead (Regurus' fourth record is another pose: its
		 * sixth is that stance). Weltall's stance ahead is a crouch with a leg stretched behind: part of the way there
		 * only that leg's toes reach the floor.
		 *
		 * And how its model shows its sprite's run, a dash leaning forward on its thrusters, by the way its battle
		 * approach covers ground: a gear that glides or hovers into battle holds the lunge its approach ends in; one
		 * that runs there runs, its battle approach whole as far as the sprite goes; Chu-Chu hops (HopTicks). Fenrir
		 * runs into battle with its sword held ready, a stance that reads as carried on thrusters, so it lunges too, its
		 * arms down at its sides as it stands, where the lunge raises a fist (LungeLimbs: each a bone and those below it,
		 * held as it stands).
		 */
		struct FStanding
		{
			int32 Record;
			float Weight;
			FFieldGearStandIns::ERun Run = FFieldGearStandIns::ERun::Lunge;
			std::array<int16, 2> LungeLimbs{FFieldGearSprite::NoArm, FFieldGearSprite::NoArm};
		};
		using ERun = FFieldGearStandIns::ERun;
		constexpr int32 BattleStance = 1;
		constexpr int32 StanceAhead = 4;
		constexpr int32 RegurusStanceAhead = 6;
		constexpr FStanding Standings[] = {
			{BattleStance, 0.3f, ERun::Strides}, // Weltall
			{BattleStance, 0.5f, ERun::Strides}, // Weltall-2
			{StanceAhead, 0.5f, ERun::Strides}, // Vierge
			{StanceAhead, 0.5f, ERun::Strides}, // Heimdal
			{BattleStance, 1.0f, ERun::Lunge}, // Brigandier
			{StanceAhead, 0.5f, ERun::Strides}, // Renmazuo
			{BattleStance, 1.0f, ERun::Lunge}, // Stier
			{BattleStance, 1.0f, ERun::Hops}, // Chu-Chu
			{BattleStance, 1.0f, ERun::Strides}, // Seibzehn
			{StanceAhead, 1.0f, ERun::Lunge}, // Crescens
			{RegurusStanceAhead, 0.7f, ERun::Strides}, // Regurus
			{StanceAhead, 0.3f, ERun::Lunge, {6, 16}}, // Fenrir, its arms as it stands
			{StanceAhead, 0.7f, ERun::Lunge}, // Andvari
			{BattleStance, 1.0f, ERun::Strides}, // Renmazuo, the Omnigear
			{BattleStance, 1.0f, ERun::Lunge}, // Stier, the Omnigear
			{StanceAhead, 0.7f, ERun::Lunge}, // Xenogears
			{StanceAhead, 0.5f, ERun::Strides}, // Heimdal with its sword
		};

		/**
		 * Chu-Chu's sprite runs bouncing along, where the other gears' dash: her battle model closes in on a target
		 * in hops, and her run is that record whole. Her sprite's walk and run each go round in HopTicks field ticks,
		 * and so does a hop; a run's first frame is the top of a bounce.
		 */
		constexpr int32 HopTicks = 24;

		/** A gear past the table stands in its rest pose. */
		FStanding StandingOf(int32 Gear)
		{
			return Gear >= 0 && static_cast<std::size_t>(Gear) < std::size(Standings) ? Standings[Gear] : FStanding{RestRecord, 0.0f};
		}

		/**
		 * The models' places in VRAM's port rows: a page each, the first eight beside the console's rows' right half,
		 * the rest across the port's second band, and a row each for their CLUTs (the pilots' stand-ins keep the
		 * columns from 0 to 5Fh and the CLUT rows from 512).
		 */
		constexpr int16 FirstBandX = 0x200;
		constexpr int32 FirstBandPages = 8;
		constexpr int16 PageWidth = 0x40;
		constexpr int16 BandHeight = 0x100;
		constexpr int16 ClutX = 0x100;
		constexpr int16 FirstClutRow = FVram::Height + 8;

		FGearTexturePlace PlaceOf(int32 Place)
		{
			const bool bFirstBand = Place < FirstBandPages;
			const int16 X = bFirstBand ? static_cast<int16>(FirstBandX + PageWidth * Place) : static_cast<int16>(PageWidth * (Place - FirstBandPages));
			const int16 Y = bFirstBand ? static_cast<int16>(FVram::Height) : static_cast<int16>(FVram::Height + BandHeight);
			return {X, Y, ClutX, static_cast<int16>(FirstClutRow + Place)};
		}

		/** A model faces along its own -z; an actor's yaw of 0 is three quarters of a turn from it. */
		constexpr int32 ModelYawOffset = 0xC00;
		constexpr int32 Turn = 0x1000;
		/** The most a stand-in turns in a tick toward where its actor faces. */
		constexpr int32 TurnStep = 0x100;
		/** The ticks a change of motion blends over. */
		constexpr int32 BlendTicks = 6;
		/** A walk is the move record at this much of its reach from the standing pose, and of its stride. */
		constexpr float WalkWeight = 0.625f;
		/** A gear that moved farther than this in a tick was put elsewhere: its legs do not follow. */
		constexpr float MostStep = 256.0f;
		/** The scenery drifts under the actors in these modes of FieldSceneryDrift: the ground a gear covers is its move less the drift. */
		constexpr uint8 SceneryDrifts = 0;
		constexpr uint8 EveryModelDrifts = 1;
		/** Over drifting ground a moving gear faces the way it goes over it once it goes this far in a tick. */
		constexpr float LeastHeading = 4.0f;
		/** A walk or run that gains no ground (against a wall) still moves its legs, at this much of a tick a tick. */
		constexpr int32 IdlePhaseStep = FGearPoseTrack::TickPhase / 2;
		constexpr float FixedOne = 65536.0f;
		constexpr int32 Channels = 1;
		/** The stand-ins' light: its direction, its colour (4096 is full) and the ambient grey. */
		constexpr std::array<int16, 3> LightDirection{504, -4083, -504};
		constexpr int16 LightColor = 2048;
		constexpr uint8 Ambient = 60;
		constexpr std::array<uint8, 3> Untinted{0x80, 0x80, 0x80};
		constexpr float RadiansPerDegree = 0.017453293f;
		constexpr float Pi = 3.14159265f;

		uint32 ScriptOf(const FSpriteFile& File, const FSpriteTables& Tables, int32 Animation)
		{
			const uint32 Header = File.AnimationHeader(Tables, Animation);
			return Header + 2 + File.Word(Header + 2);
		}

		/** The frame an animation's facing list starts with. */
		uint32 FirstFrame(const FSpriteFile& File, const FSpriteTables& Tables, int32 Animation, uint32 List = 0)
		{
			const uint32 At = File.AnimationHeader(Tables, Animation) + 4 + 2 * List;
			return File.Word(At + File.Word(At)) & FrameNumberMask;
		}

		/** The height above its origin of the frames an animation starts with, the mean of its facings', in texels. */
		int32 FramesHeight(const FSpriteFile& File, const FSpriteTables& Tables, int32 Animation)
		{
			const uint32 Lists = FacingLists[File.Word(File.AnimationHeader(Tables, Animation)) & 3];
			uint32 Sum = 0;
			for (uint32 List = 0; List < Lists; ++List)
			{
				Sum += File.Byte(File.Frame(Tables, static_cast<int32>(FirstFrame(File, Tables, Animation, List))) + FrameTopByte);
			}
			return static_cast<int32>(Sum / Lists);
		}

		/** The height above its origin of a sprite's standing frames. */
		int32 StandingHeight(const FSpriteFile& File, const FSpriteTables& Tables)
		{
			return FramesHeight(File, Tables, StopAnimation);
		}

		/** The least of them: the facing it stands lowest in. */
		int32 LowestStandingHeight(const FSpriteFile& File, const FSpriteTables& Tables)
		{
			const uint32 Lists = FacingLists[File.Word(File.AnimationHeader(Tables, StopAnimation)) & 3];
			int32 Lowest = 0xFF;
			for (uint32 List = 0; List < Lists; ++List)
			{
				Lowest = std::min<int32>(Lowest, File.Byte(File.Frame(Tables, static_cast<int32>(FirstFrame(File, Tables, StopAnimation, List))) + FrameTopByte));
			}
			return Lowest;
		}

		/** A parked sprite's frame is one it shrinks in where it is this much lower than the sprite stands from any side. */
		constexpr float ShrunkFrame = 0.85f;

		/** How far a posed model's shown parts reach above the floor of its own space and below it, in its units. */
		struct FReach
		{
			int32 Above = 0;
			int32 Below = 0;
		};

		/** Of every shown part, or only of those that hang from one of Under's bones (counting the bone itself). */
		FReach ReachOf(const FGearModel& Model, std::span<const FGearSkeletonEntry> Skeleton = {}, std::span<const int16> Under = {})
		{
			FReach Reach;
			bool bAny = false;
			for (const FGearBonePackets& Built : Model.BonePackets)
			{
				const FGearBone& Bone = Model.Bones[Built.Bone];
				const bool bUnder = Under.empty()
					|| std::any_of(Under.begin(), Under.end(), [&](int16 Top) { return IsGearBoneBelow(Skeleton, static_cast<int32>(Built.Bone), Top); });
				if (Bone.bShown == 0 || !bUnder)
				{
					continue;
				}
				const FGteMatrix Placed = CompMatrix(Model.Bones[0].Local, Bone.World);
				for (const FGteVector& Vertex : Model.Parts->Parts[Built.Part].Vertices)
				{
					const int32 Down = RotTrans(Placed, Vertex)[1];
					Reach.Above = bAny ? std::max(Reach.Above, -Down) : -Down;
					Reach.Below = bAny ? std::max(Reach.Below, Down) : Down;
					bAny = true;
				}
			}
			return Reach;
		}

		/** How tall a model stands in Pose at Scale (its root at rest): its shown parts' highest point to their lowest. */
		int32 HeightStanding(const FGearModelPack& Pack, const FGearPose& Pose, std::span<const uint8> Shown, int16 Scale)
		{
			std::vector<FGearBone> Bones;
			MakeGearSkeleton(Pack.Skeleton, Bones);
			for (std::size_t Bone = 0; Bone < Bones.size() && Bone < Shown.size(); ++Bone)
			{
				Bones[Bone].bShown = Shown[Bone];
			}
			ApplyGearPose(Pose, Bones);
			ComputeGearBones(Bones, Scale);
			int32 Top = 0;
			int32 Bottom = 0;
			bool bAny = false;
			for (std::size_t Bone = 1; Bone < Bones.size(); ++Bone)
			{
				const FGearBone& Placed = Bones[Bone];
				if (Placed.Part == FGearBone::NoPart || Placed.Part >= Pack.Model.Parts.size() || Placed.bShown == 0)
				{
					continue;
				}
				const FGteMatrix World = CompMatrix(Bones[0].Local, Placed.World);
				for (const FGteVector& Vertex : Pack.Model.Parts[Placed.Part].Vertices)
				{
					const int32 Down = RotTrans(World, Vertex)[1];
					Top = bAny ? std::min(Top, Down) : Down;
					Bottom = bAny ? std::max(Bottom, Down) : Down;
					bAny = true;
				}
			}
			return Bottom - Top;
		}

		/** From an angle the shorter way round to another, by at most Step. */
		int32 TurnToward(int32 From, int32 To, int32 Step)
		{
			int32 Delta = (To - From) % Turn;
			if (Delta > Turn / 2)
			{
				Delta -= Turn;
			}
			else if (Delta < -Turn / 2)
			{
				Delta += Turn;
			}
			return From + std::clamp(Delta, -Step, Step);
		}

		/**
		 * An arm turned out from the stand by Angle (radians, a quarter turn level) about the model's forward axis at its
		 * bone, the way that carries the bone below it that hangs farthest from it out to its own side.
		 */
		void RaiseArm(FGearPose& Pose, std::span<const FGearSkeletonEntry> Skeleton, int32 Arm, float Angle)
		{
			if (Arm <= 0 || static_cast<std::size_t>(Arm) > Skeleton.size())
			{
				return;
			}
			const std::array<float, 3> Pivot = GearBoneModelPlace(Pose, Skeleton, Arm);
			std::array<float, 3> Reach{};
			float Farthest = 0.0f;
			for (int32 Bone = Arm + 1; static_cast<std::size_t>(Bone) <= Skeleton.size(); ++Bone)
			{
				if (!IsGearBoneBelow(Skeleton, Bone, Arm))
				{
					continue;
				}
				const std::array<float, 3> Place = GearBoneModelPlace(Pose, Skeleton, Bone);
				const std::array<float, 3> Away{Place[0] - Pivot[0], Place[1] - Pivot[1], Place[2] - Pivot[2]};
				const float Distance = Away[0] * Away[0] + Away[1] * Away[1] + Away[2] * Away[2];
				if (Distance > Farthest)
				{
					Farthest = Distance;
					Reach = Away;
				}
			}
			const float Side = Pivot[0] < 0.0f ? -1.0f : 1.0f;
			const float Half = Angle / 2.0f;
			// About z a quarter turn either way carries the arm's reach to x' = -y or y: out is its own side's sign.
			const float Sign = -Reach[1] * Side >= 0.0f ? 1.0f : -1.0f;
			TurnGearBoneInModel(Pose, Skeleton, Arm, {0.0f, 0.0f, Sign * std::sin(Half), std::cos(Half)});
		}
	}

	FFieldGearStandIns::FFieldGearStandIns(const FCdImage& InImage, const FGameFileIndex& InIndex) : Image(InImage), Index(InIndex)
	{
	}

	FFieldGearStandIns::~FFieldGearStandIns() = default;

	const FFieldGearLights& FFieldGearStandIns::Lights()
	{
		static const FFieldGearLights Shared = []
		{
			FFieldGearLights Made;
			Made.Words.fill(0);
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				Made.Words[static_cast<std::size_t>(FFieldGearLights::DirectionWord(0, Axis))] = LightDirection[static_cast<std::size_t>(Axis)];
				Made.Words[static_cast<std::size_t>(FFieldGearLights::ColorWord(0, Axis))] = LightColor;
			}
			Made.Ambient = {Ambient, Ambient, Ambient};
			return Made;
		}();
		return Shared;
	}

	uint32 FFieldGearStandIns::Revision() const
	{
		uint32 Sum = ModulesMade;
		for (const std::unique_ptr<FModule>& Module : Modules)
		{
			Sum += Module->Models.Revision;
		}
		return Sum;
	}

	void FFieldGearStandIns::BeginTick()
	{
		for (FStandIn& StandIn : StandIns)
		{
			StandIn.bHeld = StandIn.bFollowed;
			StandIn.bFollowed = false;
		}
	}

	void FFieldGearStandIns::Clear()
	{
		for (const std::unique_ptr<FModule>& Module : Modules)
		{
			for (int32 Slot = 0; Slot < FGearModels::Slots; ++Slot)
			{
				Module->Models.Free(Slot);
			}
		}
		StandIns.clear();
		for (const std::unique_ptr<FSource>& Source : Sources)
		{
			Source->Place = -1;
		}
		PlacesUsed = 0;
		MapSprites.clear();
		MapSpritesRead = -1;
	}

	FFieldGearStandIns::FStandIn* FFieldGearStandIns::StandInOf(int32 Actor)
	{
		const auto Found = std::find_if(StandIns.begin(), StandIns.end(), [Actor](const FStandIn& StandIn) { return StandIn.Actor == Actor; });
		return Found != StandIns.end() ? &*Found : nullptr;
	}

	const FFieldGearStandIns::FStandIn* FFieldGearStandIns::StandInOf(int32 Actor) const
	{
		const auto Found = std::find_if(StandIns.begin(), StandIns.end(), [Actor](const FStandIn& StandIn) { return StandIn.Actor == Actor; });
		return Found != StandIns.end() ? &*Found : nullptr;
	}

	bool FFieldGearStandIns::IsShown(int32 Actor) const
	{
		const FStandIn* StandIn = StandInOf(Actor);
		return StandIn != nullptr && StandIn->bFollowed;
	}

	std::optional<std::array<int16, 3>> FFieldGearStandIns::ModelOf(int32 Actor) const
	{
		const FStandIn* StandIn = StandInOf(Actor);
		if (StandIn == nullptr || StandIn->Shape == nullptr)
		{
			return std::nullopt;
		}
		const FSource& Source = *StandIn->Shape->Source;
		return std::array<int16, 3>{static_cast<int16>(Source.Kind), Source.Number, Source.Entry};
	}

	int32 FFieldGearStandIns::GearOf(int32 Actor) const
	{
		const std::optional<std::array<int16, 3>> Model = ModelOf(Actor);
		return Model && (*Model)[0] == static_cast<int16>(EFieldGearModelKind::BattleGear) ? (*Model)[1] : -1;
	}

	const FGearModel* FFieldGearStandIns::LoadedModelOf(int32 Actor) const
	{
		const FStandIn* StandIn = StandInOf(Actor);
		return StandIn != nullptr && StandIn->Module >= 0 ? Modules[static_cast<std::size_t>(StandIn->Module)]->Models.Model(StandIn->Slot) : nullptr;
	}

	bool FFieldGearStandIns::RunsOnFoot(int32 Actor) const
	{
		const FStandIn* StandIn = StandInOf(Actor);
		return StandIn != nullptr && StandIn->bFollowed && StandIn->Shape != nullptr && StandIn->Motion == EMotion::Run
			&& StandIn->Shape->Run != ERun::Lunge;
	}

	bool FFieldGearStandIns::Thrusts(int32 Actor) const
	{
		const FStandIn* StandIn = StandInOf(Actor);
		return StandIn != nullptr && StandIn->bFollowed && StandIn->Shape != nullptr && StandIn->PartyGear >= 0
			&& ((StandIn->Motion == EMotion::Run && StandIn->Shape->Run == ERun::Lunge) || StandIn->Motion == EMotion::Leap);
	}

	std::optional<int32> FFieldGearStandIns::LayoutSlotOf(int32 Actor) const
	{
		const FStandIn* StandIn = StandInOf(Actor);
		return StandIn != nullptr && StandIn->Module >= 0 ? std::optional<int32>(LayoutSlot(StandIn->Module, StandIn->Slot)) : std::nullopt;
	}

	int32 FFieldGearStandIns::ParkedGearOf(const FFieldGearSprite& Parked, const FGameState* Game)
	{
		// The hangar parks Regurus in Vierge's place (the field reads it as Vierge, whose pictures it has).
		constexpr uint8 Elly = 1;
		constexpr int16 Vierge = 2;
		constexpr int16 Regurus = 10;
		const std::optional<uint8> EllysGear = Game != nullptr ? Game->CharacterGear(Elly) : std::nullopt;
		return Parked.Number == Vierge && EllysGear == Regurus ? Regurus : Parked.Number;
	}

	const FFieldGearSprite* FFieldGearStandIns::KnownSprite(const FFieldRuntime& Runtime, const FSprite& Sprite)
	{
		if (MapSpritesRead != Runtime.MapLoads())
		{
			// The map's sprite files, each known by its bytes as they lie in the map's section, before any script writes
			// into them.
			MapSprites.clear();
			const std::span<const uint8> Section = Runtime.Map().SpriteSets;
			const auto Word = [&Section](std::size_t At)
			{ return At + 4 <= Section.size() ? static_cast<uint32>(Section[At] | Section[At + 1] << 8 | Section[At + 2] << 16 | Section[At + 3] << 24) : 0u; };
			const uint32 Count = Word(0);
			for (uint32 File = 0; File < Count && 4 + 4 * std::size_t{File} + 4 <= Section.size(); ++File)
			{
				const uint32 Start = Word(4 + 4 * std::size_t{File});
				MapSprites.push_back(Start < Section.size() ? FindFieldGearSprite(FieldGearSpriteDigest(Section.subspan(Start))) : nullptr);
			}
			MapSpritesRead = Runtime.MapLoads();
		}
		const std::span<const FSpriteFile> Files = Runtime.SpriteFiles();
		const FSpriteFile* File = Sprite.PrimaryFile;
		if (File == nullptr || Files.empty() || File < Files.data() || File >= Files.data() + Files.size())
		{
			return nullptr;
		}
		const std::size_t Number = static_cast<std::size_t>(File - Files.data());
		return Number < MapSprites.size() ? MapSprites[Number] : nullptr;
	}

	FFieldGearStandIns::EMotion FFieldGearStandIns::MotionOf(const FSprite& Sprite, const FFieldGearSprite* Known)
	{
		const int32 Animation = Sprite.Animation();
		if (Animation < 0 || Sprite.Resource == nullptr || Sprite.Resource->File == nullptr)
		{
			return EMotion::None;
		}
		if (Known != nullptr)
		{
			if (Animation >= FFieldGearSprite::Animations)
			{
				return EMotion::None;
			}
			constexpr EMotion ByMotion[] = {EMotion::None, EMotion::Stand, EMotion::Walk, EMotion::Run, EMotion::Leap, EMotion::Idle, EMotion::Gesture};
			return ByMotion[static_cast<std::size_t>(Known->Motions[static_cast<std::size_t>(Animation)])];
		}
		constexpr EMotion Numbered[] = {EMotion::Stand, EMotion::Walk, EMotion::Run, EMotion::Leap};
		if (Animation <= JumpAnimation)
		{
			return Numbered[Animation];
		}
		// The files' further animations are mostly the four again under other numbers: told by the script they
		// share with the stop, or by the frame they start with.
		const FSpriteFile& File = *Sprite.Resource->File;
		const FSpriteTables& Tables = Sprite.Resource->Tables;
		if (Animation >= File.AnimationCount(Tables) || File.AnimationCount(Tables) <= JumpAnimation)
		{
			return EMotion::None;
		}
		if (ScriptOf(File, Tables, Animation) == ScriptOf(File, Tables, StopAnimation))
		{
			return EMotion::Stand;
		}
		const uint32 First = FirstFrame(File, Tables, Animation);
		for (const int32 Like : {RunAnimation, JumpAnimation, WalkAnimation, StopAnimation})
		{
			if (First == FirstFrame(File, Tables, Like))
			{
				return Numbered[Like];
			}
		}
		// A run's ending in frames of its own (Chu-Chu's) is the gear coming to a stand.
		return Animation == RunEndingAnimation ? EMotion::Stand : EMotion::None;
	}

	FFieldGearStandIns::FSource* FFieldGearStandIns::SourceOf(EFieldGearModelKind Kind, int16 Number, int16 Entry)
	{
		for (const std::unique_ptr<FSource>& Source : Sources)
		{
			if (Source->Kind == Kind && Source->Number == Number && Source->Entry == Entry)
			{
				return Source.get();
			}
		}
		FSource& Source = *Sources.emplace_back(std::make_unique<FSource>());
		Source.Kind = Kind;
		Source.Number = Number;
		Source.Entry = Entry;
		Source.bUnreadable = true;
		TResult<FBattleModelFiles> Files = MakeError("no model");
		if (Kind == EFieldGearModelKind::BattleGear)
		{
			if (!Disc && !bDiscUnreadable)
			{
				TResult<FBattleObjectDisc> Read = FBattleObjectDisc::Load(Image, Index);
				if (Read)
				{
					Disc = std::make_unique<FBattleObjectDisc>(std::move(*Read));
				}
				bDiscUnreadable = !Disc;
			}
			if (Disc && Number >= 0 && Number < FBattleObjectTables::GearCount)
			{
				Files = Disc->ReadGear(Number, 0);
			}
		}
		else if (TResult<FBattleSceneGraphics> Graphics = FBattleSceneGraphics::Load(Image, Index, Number))
		{
			Files = BattleObjectFiles::SceneModel(*Graphics, Entry);
		}
		if (Files)
		{
			Source.Opening = FBattleGearOpening::Read(Files->Motion);
			Source.Model = std::move(Files->Model);
			Source.Motion = std::move(Files->Motion);
			Source.bUnreadable = !Source.Opening.bComplete;
		}
		return &Source;
	}

	FFieldGearStandIns::FShape* FFieldGearStandIns::ShapeOf(FSource& Source, const FFieldGearSprite* Known, int32 PartyGear, FSource* Performer)
	{
		// A gear worn over another's skeleton stands and moves as that one does.
		const int32 Moves = Performer != nullptr ? Performer->Number : PartyGear;
		FStanding Standing = StandingOf(Known != nullptr && !Known->bParked ? Known->Number : Moves);
		std::array<int16, 2> Arms{FFieldGearSprite::NoArm, FFieldGearSprite::NoArm};
		int16 ArmTurn = 0;
		int8 Gesture = 0;
		if (Known != nullptr)
		{
			if (Known->StanceRecord != FFieldGearSprite::TheParty)
			{
				Standing = {Known->StanceRecord, Known->StanceWeight};
			}
			Arms = Known->RaisedArms;
			ArmTurn = Known->ArmTurn;
			Gesture = Known->Gesture;
		}
		for (const std::unique_ptr<FShape>& Shape : Shapes)
		{
			if (Shape->Source == &Source && Shape->Performer == Performer && Shape->StanceRecord == Standing.Record && Shape->StanceWeight == Standing.Weight
				&& Shape->RaisedArms == Arms && Shape->ArmTurn == ArmTurn && Shape->GestureRecord == Gesture)
			{
				return Shape.get();
			}
		}
		FShape& Shape = *Shapes.emplace_back(std::make_unique<FShape>());
		Shape.Source = &Source;
		Shape.Performer = Performer;
		Shape.StanceRecord = static_cast<int8>(Standing.Record);
		Shape.StanceWeight = Standing.Weight;
		Shape.RaisedArms = Arms;
		Shape.ArmTurn = ArmTurn;
		Shape.GestureRecord = Gesture;
		// Only a party gear's sprite runs: the others' that dash glide into their battles.
		if (Source.Kind == EFieldGearModelKind::BattleGear || Performer != nullptr)
		{
			const int32 Runner = Performer != nullptr ? Performer->Number : Source.Number;
			Shape.Run = StandingOf(Runner).Run;
			Shape.LungeLimbs = StandingOf(Runner).LungeLimbs;
		}
		return &Shape;
	}

	bool FFieldGearStandIns::Dress(FShape& Shape, int16 Scale)
	{
		FSource& Performer = *Shape.Performer;
		const FSource& Own = *Shape.Source;
		// A gear's own model worn over another's (Fenrir's), or Id's red gear, a battle scene's model, over Fei's own gear.
		const bool bIds = Own.Kind == EFieldGearModelKind::SceneEnemy;
		const FBattleGearDress::FPairing* Pairing = bIds ? FBattleGearDress::IdsPairing(static_cast<uint8>(Performer.Number))
														: FBattleGearDress::FindPairing(static_cast<uint8>(Performer.Number), static_cast<uint8>(Own.Number));
		if (Pairing == nullptr || (bIds && (Pairing->WearerScene != Own.Number || Pairing->WearerEntry != Own.Entry)))
		{
			return false;
		}
		FGearModelPack Wearer = Shape.Source->Model;
		FGearMotionPack WearerMotion = Shape.Source->Motion;
		TResult<FBattleGearDress> Made = FBattleGearDress::Make(*Pairing, Performer.Model, Performer.Motion, std::move(Wearer), WearerMotion, nullptr);
		if (!Made)
		{
			return false;
		}
		Shape.Dress = std::move(*Made);
		std::vector<FGearBone> Opened;
		MakeGearSkeleton(Performer.Model.Skeleton, Opened);
		Performer.Opening.Apply(Opened);
		std::vector<uint8> Shown(Opened.size());
		for (std::size_t Bone = 0; Bone < Opened.size(); ++Bone)
		{
			Shown[Bone] = Opened[Bone].bShown;
		}
		if (bIds)
		{
			Shape.PerformerHeight = HeightStanding(Performer.Model, Shape.Stand, Shown, Scale);
		}
		const FBattleGearDress& Worn = *Shape.Dress;
		Shape.Stand = Worn.PoseOver(Shape.Stand, Shown, &Shape.DressShown);
		Shape.Dash = Worn.PoseOver(Shape.Dash, Shown);
		Shape.Leap = Worn.PoseOver(Shape.Leap, Shown);
		for (FGearPoseTrack* Track : {&Shape.Move, &Shape.Idle, &Shape.Gesture})
		{
			for (FGearPose& Tick : Track->Ticks)
			{
				Tick = Worn.PoseOver(Tick, Shown);
			}
		}
		return true;
	}

	bool FFieldGearStandIns::Measure(FShape& Shape, FGearModel& Model)
	{
		// A worn model takes its performer's records, played on the performer's skeleton, then worn.
		const std::vector<FGearSkeletonEntry>& Skeleton = (Shape.Performer != nullptr ? Shape.Performer : Shape.Source)->Model.Skeleton;
		FGearMotionPack& Motion = Shape.Performer != nullptr ? Shape.Performer->Motion : *Model.Motion;
		const TResult<FGearPose> Rest = ReadGearPose(Skeleton, Motion, RestRecord);
		const TResult<FGearPoseTrack> Stance = BakeGearRecord(Skeleton, Motion, Shape.StanceRecord, RestRecord);
		TResult<FGearPoseTrack> Move = BakeGearRecord(Skeleton, Motion, MoveRecord, RestRecord);
		const TResult<FGearPoseTrack> Dash = BakeGearRecord(Skeleton, Motion, DashRecord, RestRecord);
		const TResult<FGearPoseTrack> Leap = BakeGearRecord(Skeleton, Motion, LeapRecord, RestRecord);
		TResult<FGearPoseTrack> Gesture = BakeGearRecord(Skeleton, Motion, Shape.GestureRecord, RestRecord);
		if (!Rest || !Stance || !Move || !Dash || !Leap || !Gesture || Stance->Ticks.empty() || Move->Ticks.empty() || Dash->Ticks.empty()
			|| Leap->Ticks.empty() || Gesture->Ticks.empty() || Move->Stride == 0)
		{
			return false;
		}
		BlendGearPoses(*Rest, Stance->Ticks.front(), Shape.StanceWeight, Shape.Stand);
		for (const int16 Arm : Shape.RaisedArms)
		{
			RaiseArm(Shape.Stand, Skeleton, Arm, static_cast<float>(Shape.ArmTurn) * RadiansPerDegree);
		}
		Shape.Idle = *Stance;
		Shape.Move = std::move(*Move);
		Shape.Dash = Dash->Ticks.back();
		for (const int16 Limb : Shape.LungeLimbs)
		{
			CopyGearLimb(Shape.Stand, Skeleton, Limb, Shape.Dash);
		}
		Shape.Leap = Leap->Ticks.back();
		Shape.Gesture = std::move(*Gesture);
		Shape.Stride = static_cast<float>(Shape.Move.Stride) * static_cast<float>(Model.Scale) / static_cast<float>(FGteMatrix::One);
		if (Shape.Performer != nullptr && !Dress(Shape, Model.Scale))
		{
			return false;
		}

		// The model's size standing: at the origin of its own space, from its soles, which a stance leaves a little off
		// the space's floor, to its top.
		Model.Bones[0].Angles = {};
		Model.Bones[0].Position = {};
		ApplyGearPose(Shape.Stand, Model.Bones);
		ComputeGearBones(Model.Bones, Model.Scale);
		const FReach Reach = ReachOf(Model);
		Shape.ModelHeight = Shape.PerformerHeight > 0 ? Shape.PerformerHeight : Reach.Above + Reach.Below;
		Shape.Sole = Reach.Below;
		if (Shape.RaisedArms[0] != FFieldGearSprite::NoArm)
		{
			Shape.HandsAbove = ReachOf(Model, Skeleton, Shape.RaisedArms).Above;
		}
		// Walking and running, the lowest its strides reach is the floor: its stance's soles would leave a stride's feet a
		// little in the floor or above it.
		Shape.WalkSole = std::numeric_limits<int32>::min();
		Shape.RunSole = std::numeric_limits<int32>::min();
		for (const FGearPose& Tick : Shape.Move.Ticks)
		{
			ApplyGearPose(Tick, Model.Bones);
			ComputeGearBones(Model.Bones, Model.Scale);
			Shape.RunSole = std::max(Shape.RunSole, ReachOf(Model).Below);
			BlendGearPoses(Shape.Stand, Tick, WalkWeight, Moving);
			ApplyGearPose(Moving, Model.Bones);
			ComputeGearBones(Model.Bones, Model.Scale);
			Shape.WalkSole = std::max(Shape.WalkSole, ReachOf(Model).Below);
		}
		Shape.bMeasured = true;
		return Shape.ModelHeight > 0;
	}

	void FFieldGearStandIns::Make(FStandIn& StandIn, const FSprite& Sprite, FVram& Vram)
	{
		if (StandIn.Module >= 0)
		{
			Modules[static_cast<std::size_t>(StandIn.Module)]->Models.Free(StandIn.Slot);
		}
		StandIn.Module = -1;
		StandIn.Slot = -1;
		StandIn.Shape = nullptr;
		StandIn.bPlaced = false;
		StandIn.bUnusable = true;
		const FFieldGearSprite* Known = StandIn.Known;
		// A party gear, ridden or parked, as the player has its pilot drawn: another gear's model, its own over another's
		// moves, or Id's red gear over Fei's own.
		const bool bLooked = Known == nullptr || Known->bParked;
		const uint8 PartyGear = static_cast<uint8>(Known != nullptr ? StandIn.ParkedGear : StandIn.PartyGear);
		const FBattleGearDress::FPairing* Ids = bLooked && StandIn.Looks.WearsIdsGear(PartyGear) ? FBattleGearDress::IdsPairing(PartyGear) : nullptr;
		const bool bWorn = bLooked && (Ids != nullptr || StandIn.Looks.WearsOwnModel(PartyGear));
		const int16 Shown = static_cast<int16>(!bLooked ? StandIn.PartyGear : bWorn ? PartyGear : StandIn.Looks.PerformingGear(PartyGear));
		FSource* Source = !bLooked		   ? SourceOf(Known->Kind, Known->Number, Known->Entry)
						: Ids != nullptr ? SourceOf(EFieldGearModelKind::SceneEnemy, Ids->WearerScene, Ids->WearerEntry)
										 : SourceOf(EFieldGearModelKind::BattleGear, Shown, 0);
		FSource* Performer = bWorn ? SourceOf(EFieldGearModelKind::BattleGear, static_cast<int16>(StandIn.Looks.PerformingGear(PartyGear)), 0) : nullptr;
		if (Source == nullptr || Source->bUnreadable || (Performer != nullptr && Performer->bUnreadable) || Sprite.Resource == nullptr
			|| Sprite.Resource->File == nullptr)
		{
			return;
		}
		FShape* Shape = ShapeOf(*Source, Known, Shown, Performer);
		if (Shape->bUnusable)
		{
			return;
		}
		const bool bFirstOnMap = Source->Place < 0;
		if (bFirstOnMap && PlacesUsed >= TexturePlaces)
		{
			return;
		}

		// A slot in a module with room for another model's parts, or a new module.
		int32 ModuleIndex = -1;
		int32 Slot = -1;
		for (std::size_t Module = 0; Module < Modules.size() && Slot < 0; ++Module)
		{
			const FGearModels& Models = Modules[Module]->Models;
			if (Models.PartTablesUsed >= FGearModels::PartTables || Models.Faulted())
			{
				continue;
			}
			for (int32 Free = 0; Free < FGearModels::Slots; ++Free)
			{
				if (Models.Model(Free) == nullptr)
				{
					ModuleIndex = static_cast<int32>(Module);
					Slot = Free;
					break;
				}
			}
		}
		if (Slot < 0)
		{
			std::unique_ptr<FModule> Made = std::make_unique<FModule>();
			if (Made->Models.Init(Channels))
			{
				return;
			}
			Made->Tints.fill(Untinted);
			Modules.push_back(std::move(Made));
			++ModulesMade;
			ModuleIndex = static_cast<int32>(Modules.size()) - 1;
			Slot = 0;
		}
		FGearModels& Models = Modules[static_cast<std::size_t>(ModuleIndex)]->Models;

		// A part the drawing never lights again takes the ambient light, as a map's own gears' loads give it.
		FGte Lighting;
		for (std::size_t Channel = 0; Channel < 3; ++Channel)
		{
			Lighting.BackColor[Channel] = Lights().Ambient[Channel] * 16;
		}
		Models.BuildLighting = Lighting;
		const int32 Place = bFirstOnMap ? PlacesUsed : Source->Place;
		FGearModelPack ModelPack = Source->Model;
		FGearMotionPack MotionPack = Source->Motion;
		const std::optional<FError> Failed = Models.LoadStill(Slot, std::move(ModelPack), std::move(MotionPack), PlaceOf(Place));
		FGearModel* Model = Models.Model(Slot);
		if (Failed || Model == nullptr)
		{
			// Only this model cannot be shown: the module goes on for the others.
			Models.Fault.reset();
			Models.Free(Slot);
			Models.TakeVramChanges();
			Source->bUnreadable = true;
			return;
		}
		if (bFirstOnMap)
		{
			FVramImage Upload;
			for (const FGearUpload& Block : Models.Uploads())
			{
				Upload.Rect = {Block.X, Block.Y, Block.Width, Block.Height};
				Upload.Units = Block.Pixels;
				Vram.UploadToPortRows(Upload);
			}
			Source->Place = Place;
			++PlacesUsed;
		}
		Models.TakeVramChanges();

		Source->Opening.Apply(Model->Bones);
		if (!Shape->bMeasured && !Measure(*Shape, *Model))
		{
			Shape->bUnusable = true;
			Models.Free(Slot);
			return;
		}
		for (std::size_t Bone = 1; Bone < Shape->DressShown.size() && Bone < Model->Bones.size(); ++Bone)
		{
			Model->Bones[Bone].bShown = Shape->DressShown[Bone];
		}
		StandIn.SpriteHeight = StandingHeight(*Sprite.Resource->File, Sprite.Resource->Tables);
		StandIn.SpriteLowest = LowestStandingHeight(*Sprite.Resource->File, Sprite.Resource->Tables);
		if (StandIn.SpriteHeight <= 0)
		{
			Models.Free(Slot);
			return;
		}
		Model->bShown = 0;
		Model->FreeHeight = 1;
		Model->Flags |= GearModelFlags::NoShadow;
		StandIn.Shape = Shape;
		StandIn.Module = ModuleIndex;
		StandIn.Slot = Slot;
		StandIn.bUnusable = false;
	}

	bool FFieldGearStandIns::Follow(const FFieldRuntime& Runtime, int32 Actor, const FSprite& Sprite, const FFieldSpritePass& Pass, FVram& Vram,
		std::optional<int32> ShownGear)
	{
		const FEventContext& Context = Runtime.Context();
		if (Sprite.PartCount() == 0 || Actor < 0 || static_cast<std::size_t>(Actor) >= Context.States.size() || Sprite.PrimaryFile == nullptr)
		{
			return false;
		}
		// A party slot's gear by the file its buffer holds, not by the kind last loaded: the field reads the files back
		// after a battle and says no kind until its next map loads them.
		int32 PartyGear = ShownGear.value_or(-1);
		if (const FPartySpriteFiles* Party = ShownGear ? nullptr : Context.PartySprites)
		{
			for (int32 Slot = 0; Slot < FPartySpriteFiles::Slots; ++Slot)
			{
				const int32 File = Party->File(Slot);
				if (Sprite.PrimaryFile == &Party->Buffer(Slot) && File >= FPartySpriteFiles::FirstGearFile && File != FPartySpriteFiles::NoFile)
				{
					PartyGear = File - FPartySpriteFiles::FirstGearFile;
				}
			}
		}
		const FFieldGearSprite* Known = PartyGear < 0 ? KnownSprite(Runtime, Sprite) : nullptr;
		if (PartyGear < 0 && Known == nullptr)
		{
			return false;
		}
		FStandIn* StandIn = StandInOf(Actor);
		if (StandIn != nullptr && StandIn->bFollowed)
		{
			// A further pass of the sprite it follows is the stand-in's too.
			return true;
		}
		const EMotion Motion = MotionOf(Sprite, Known);
		if (Motion == EMotion::None || (Sprite.Code & SemiTransparentCode) != 0)
		{
			return false;
		}
		if (StandIn == nullptr)
		{
			StandIn = &StandIns.emplace_back();
			StandIn->Actor = Actor;
			StandIn->bUnusable = true;
		}
		const FCharacterLooks Looks = (Known == nullptr || Known->bParked) && Context.GameState != nullptr ? Context.GameState->CharacterLooks() : FCharacterLooks{};
		const int32 ParkedGear = Known != nullptr && Known->bParked ? ParkedGearOf(*Known, Context.GameState) : -1;
		if (StandIn->Known != Known || StandIn->PartyGear != PartyGear || StandIn->ParkedGear != ParkedGear || StandIn->Looks != Looks)
		{
			StandIn->Known = Known;
			StandIn->PartyGear = PartyGear;
			StandIn->ParkedGear = ParkedGear;
			StandIn->Looks = Looks;
			Make(*StandIn, Sprite, Vram);
		}
		if (StandIn->bUnusable)
		{
			return false;
		}
		FModule& Module = *Modules[static_cast<std::size_t>(StandIn->Module)];
		Module.Models.Arctangents = Context.Arctangents;
		const FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
		// The sprite's parts are drawn through the pass's matrix, in view units: its second column's length is a part
		// unit's height under the view, which scales the world.
		const std::array<std::array<int16, 3>, 3>& M = Pass.Matrix.M;
		const float Up = std::sqrt(static_cast<float>(M[0][1] * M[0][1] + M[1][1] * M[1][1] + M[2][1] * M[2][1])) / static_cast<float>(FieldWorldScale);
		const float SpriteHeight = static_cast<float>(StandIn->SpriteHeight << Sprite.SizeShift()) * Up;
		StandIn->bFollowed = true;
		StandIn->Motion = Motion;
		StandIn->Position = Sprite.Position;
		StandIn->bCoveredTold = false;
		const uint8 DriftMode = Context.Render.SceneryDrift & FieldSceneryDrift::ModeMask;
		const bool bDrifting = DriftMode == SceneryDrifts || DriftMode == EveryModelDrifts;
		StandIn->Drift = bDrifting ? std::array<int32, 2>{Context.Render.DriftStep[0], Context.Render.DriftStep[2]} : std::array<int32, 2>{};
		StandIn->FloorY = Sprite.FloorY;
		StandIn->Yaw = State.Yaw;
		StandIn->Scale = SpriteHeight / static_cast<float>(StandIn->Shape->ModelHeight);
		StandIn->Lift = 0.0f;
		if (Known != nullptr && Known->Hangs() && Known->HandsAt > Known->FeetAt && StandIn->Shape->HandsAbove + StandIn->Shape->Sole > 0)
		{
			// Hanging by its hands where the sprite's lie, its feet where the sprite's hang.
			const float Texel = static_cast<float>(1 << Sprite.SizeShift()) * Up;
			StandIn->Scale = static_cast<float>(Known->HandsAt - Known->FeetAt) * Texel / static_cast<float>(StandIn->Shape->HandsAbove + StandIn->Shape->Sole);
			StandIn->Lift = static_cast<float>(Known->FeetAt) * Texel;
		}
		Module.Tints[static_cast<std::size_t>(StandIn->Slot)] = {Sprite.Red, Sprite.Green, Sprite.Blue};
		return true;
	}

	bool FFieldGearStandIns::FollowGear(int32 Key, const FSprite& Sprite, const FSight& Sight, const FGteArctangents* Arctangents, FVram& Vram)
	{
		if (Sprite.PartCount() == 0 || Sprite.PrimaryFile == nullptr || Sprite.Resource == nullptr || Sprite.Resource->File == nullptr || Sight.Gear < 0
			|| (Sprite.Code & SemiTransparentCode) != 0)
		{
			return false;
		}
		FStandIn* StandIn = StandInOf(Key);
		if (StandIn != nullptr && StandIn->bFollowed)
		{
			return true;
		}
		if (StandIn == nullptr)
		{
			StandIn = &StandIns.emplace_back();
			StandIn->Actor = Key;
			StandIn->bUnusable = true;
		}
		if (StandIn->Known != nullptr || StandIn->PartyGear != Sight.Gear || StandIn->Looks != Sight.Looks)
		{
			StandIn->Known = nullptr;
			StandIn->PartyGear = Sight.Gear;
			StandIn->Looks = Sight.Looks;
			Make(*StandIn, Sprite, Vram);
		}
		if (StandIn->bUnusable)
		{
			return false;
		}
		// A sprite that neither stands nor goes is parked. Most kneel in frames as tall as they stand; Chu-Chu's shrinks
		// out of her gear's size, in frames lower and lower and then one of no height: her model is as much smaller as
		// the frame shown is lower than she stands, and gone with it.
		float Shown = 1.0f;
		const FSpriteFile& File = *Sprite.Resource->File;
		const FSpriteTables& Tables = Sprite.Resource->Tables;
		if (!Sight.bGoes && Sprite.Animation() != StopAnimation && Sprite.Frame < File.FrameCount(Tables) && StandIn->SpriteLowest > 0)
		{
			const float Top = static_cast<float>(File.Byte(File.Frame(Tables, Sprite.Frame) + FrameTopByte)) / static_cast<float>(StandIn->SpriteLowest);
			Shown = Top < ShrunkFrame ? Top : 1.0f;
		}
		// A sprite drawn at no size has nothing a model could stand in for.
		const float Scale = Shown * static_cast<float>(StandIn->SpriteHeight << Sprite.SizeShift()) * Sight.Up / static_cast<float>(StandIn->Shape->ModelHeight);
		if (!(Scale > 0.0f))
		{
			return false;
		}
		FModule& Module = *Modules[static_cast<std::size_t>(StandIn->Module)];
		Module.Models.Arctangents = Arctangents;
		StandIn->bFollowed = true;
		StandIn->Motion = Sight.bGoes ? EMotion::Run : EMotion::Stand;
		StandIn->Position = Sight.Position;
		StandIn->Drift = {};
		StandIn->bCoveredTold = true;
		StandIn->Covered = Sight.Covered;
		StandIn->FloorY = Sight.FloorY;
		StandIn->Yaw = Sight.Yaw;
		StandIn->Scale = Scale;
		StandIn->Lift = 0.0f;
		// A sprite drawn with its texels as they are has no tint, whatever colour it holds.
		Module.Tints[static_cast<std::size_t>(StandIn->Slot)] =
			(Sprite.Code & RawCode) != 0 ? Untinted : std::array<uint8, 3>{Sprite.Red, Sprite.Green, Sprite.Blue};
		return true;
	}

	int32 FFieldGearStandIns::SoleOf(const FShape& Shape, EMotion Shown)
	{
		// A hop rises from where its stance stands.
		if (Shape.Run == ERun::Hops)
		{
			return Shape.Sole;
		}
		if (Shown == EMotion::Walk)
		{
			return Shape.WalkSole;
		}
		return Shown == EMotion::Run && Shape.Run == ERun::Strides ? Shape.RunSole : Shape.Sole;
	}

	void FFieldGearStandIns::TargetPose(const FStandIn& StandIn, FGearPose& Pose)
	{
		const FShape& Shape = *StandIn.Shape;
		switch (StandIn.Shown)
		{
		case EMotion::Walk:
			Shape.Move.Sample(StandIn.Phase, Moving);
			BlendGearPoses(Shape.Stand, Moving, WalkWeight, Pose);
			break;
		case EMotion::Run:
			if (Shape.Run == ERun::Lunge)
			{
				Pose = Shape.Dash;
			}
			else
			{
				Shape.Move.Sample(StandIn.Phase, Pose);
			}
			break;
		case EMotion::Leap:
			Pose = Shape.Leap;
			break;
		case EMotion::Idle:
			Shape.Idle.Sample(StandIn.Phase, Pose);
			break;
		case EMotion::Gesture:
			Shape.Gesture.Sample(StandIn.Phase, Pose);
			break;
		default:
			Pose = Shape.Stand;
			break;
		}
	}

	void FFieldGearStandIns::Step(FStandIn& StandIn)
	{
		const FShape& Shape = *StandIn.Shape;
		const bool bHops = Shape.Run == ERun::Hops;
		if (StandIn.Motion != StandIn.Shown)
		{
			StandIn.BlendFrom = StandIn.Pose;
			StandIn.SoleFrom = StandIn.Sole;
			StandIn.BlendLeft = BlendTicks;
			StandIn.Shown = StandIn.Motion;
			StandIn.Phase = bHops && StandIn.Shown == EMotion::Run ? Shape.Move.Cycle() / 2 : 0;
		}
		const bool bStrides = StandIn.Shown == EMotion::Walk || (StandIn.Shown == EMotion::Run && Shape.Run == ERun::Strides);
		// The ground it covered this tick: its move less the scenery's drift under it (the desert's charge drifts the sand
		// past a gear that the player only steers).
		const float AcrossX = StandIn.bCoveredTold ? StandIn.Covered[0]
			: static_cast<float>(StandIn.Position[0] - StandIn.PlacedAt[0]) / FixedOne - static_cast<float>(StandIn.Drift[0]);
		const float AcrossZ = StandIn.bCoveredTold ? StandIn.Covered[1]
			: static_cast<float>(StandIn.Position[2] - StandIn.PlacedAt[2]) / FixedOne - static_cast<float>(StandIn.Drift[1]);
		const float Gone = std::sqrt(AcrossX * AcrossX + AcrossZ * AcrossZ);
		int32 Heading = StandIn.Yaw;
		if (StandIn.Drift != std::array<int32, 2>{} && (StandIn.Shown == EMotion::Walk || StandIn.Shown == EMotion::Run) && Gone >= LeastHeading
			&& Gone <= MostStep)
		{
			// An actor's yaw of 0 goes along +x, a quarter turn along -z.
			Heading = static_cast<int32>(std::lround(std::atan2(-AcrossZ, AcrossX) / (2.0f * Pi) * static_cast<float>(Turn))) & (Turn - 1);
		}
		// The cloths trail as a battle's moving model's do: their points stay where they were as the gear goes on, at most at
		// its battle approach's own pace (the field's gears outrun their battles).
		StandIn.ClothTrail = {};
		if ((StandIn.Shown == EMotion::Walk || StandIn.Shown == EMotion::Run) && Gone > 0.0f && Gone <= MostStep && StandIn.Scale > 0.0f)
		{
			const float Pace = Shape.Stride / static_cast<float>(Shape.Move.Ticks.size()) * (StandIn.Shown == EMotion::Walk ? WalkWeight : 1.0f);
			const float Back = std::min(Gone / StandIn.Scale, Pace) / Gone;
			StandIn.ClothTrail = {-AcrossX * Back, -AcrossZ * Back};
		}
		if (StandIn.Shown == EMotion::Idle || StandIn.Shown == EMotion::Gesture)
		{
			// In place, at its records' own pace.
			const FGearPoseTrack& Track = StandIn.Shown == EMotion::Idle ? Shape.Idle : Shape.Gesture;
			StandIn.Phase = (StandIn.Phase + FGearPoseTrack::TickPhase) % Track.Cycle();
		}
		else if (bHops && (StandIn.Shown == EMotion::Walk || StandIn.Shown == EMotion::Run))
		{
			// A hop keeps its sprite's time, however far it carries.
			StandIn.Phase = (StandIn.Phase + Shape.Move.Cycle() / HopTicks) % Shape.Move.Cycle();
		}
		else if (bStrides)
		{
			// A cycle covers the record's stride at the model's size, a walk's shorter steps as much less.
			const float Stride = Shape.Stride * StandIn.Scale * (StandIn.Shown == EMotion::Walk ? WalkWeight : 1.0f);
			const int32 Stepped = Gone > MostStep || Stride <= 0.0f ? 0 : static_cast<int32>(Gone / Stride * static_cast<float>(Shape.Move.Cycle()));
			StandIn.Phase = (StandIn.Phase + std::max(Stepped, IdlePhaseStep)) % Shape.Move.Cycle();
		}
		StandIn.ShownYaw = TurnToward(StandIn.ShownYaw, Heading, TurnStep) & (Turn - 1);
		if (StandIn.BlendLeft > 0)
		{
			--StandIn.BlendLeft;
		}
	}

	std::optional<FError> FFieldGearStandIns::Pose(bool bTickRan)
	{
		for (FStandIn& StandIn : StandIns)
		{
			if (!bTickRan)
			{
				StandIn.bFollowed = StandIn.bHeld;
			}
			if (StandIn.Module < 0 || StandIn.Shape == nullptr)
			{
				continue;
			}
			FGearModel* Model = Modules[static_cast<std::size_t>(StandIn.Module)]->Models.Model(StandIn.Slot);
			if (Model == nullptr)
			{
				continue;
			}
			if (!StandIn.bFollowed)
			{
				Model->bShown = 0;
				StandIn.bPlaced = false;
				continue;
			}
			if (!StandIn.bPlaced)
			{
				// Met anew: it stands as its sprite does, with nothing to blend from.
				StandIn.Shown = StandIn.Motion;
				StandIn.ShownYaw = StandIn.Yaw;
				StandIn.Phase = 0;
				StandIn.BlendLeft = 0;
				StandIn.PlacedAt = StandIn.Position;
				StandIn.bPlaced = true;
			}
			else if (bTickRan)
			{
				Step(StandIn);
				for (FGearCloth& Cloth : Model->Cloths)
				{
					for (FGearClothPoint& Point : Cloth.Points)
					{
						Point.Position[0] = static_cast<int16>(Point.Position[0] + std::lround(StandIn.ClothTrail[0]));
						Point.Position[2] = static_cast<int16>(Point.Position[2] + std::lround(StandIn.ClothTrail[1]));
					}
				}
			}
			StandIn.PlacedAt = StandIn.Position;
			TargetPose(StandIn, Scratch);
			const FShape& Shape = *StandIn.Shape;
			const float Sole = static_cast<float>(SoleOf(Shape, StandIn.Shown));
			if (StandIn.BlendLeft > 0)
			{
				const float Weight = static_cast<float>(BlendTicks - StandIn.BlendLeft) / static_cast<float>(BlendTicks);
				BlendGearPoses(StandIn.BlendFrom, Scratch, Weight, StandIn.Pose);
				StandIn.Sole = StandIn.SoleFrom + (Sole - StandIn.SoleFrom) * Weight;
			}
			else
			{
				StandIn.Pose = Scratch;
				StandIn.Sole = Sole;
			}
			ApplyGearPose(StandIn.Pose, Model->Bones);

			// In its own space the model stands turned at the origin, its soles as far above the floor as the sprite is
			// above its own.
			FGearBone& Root = Model->Bones[0];
			const float Above = static_cast<float>(StandIn.Position[1]) / FixedOne - static_cast<float>(StandIn.FloorY) - StandIn.Lift;
			Root.Angles = {0, static_cast<int16>((StandIn.ShownYaw + ModelYawOffset) & (Turn - 1)), 0};
			Root.Position = {0, static_cast<int32>(std::lround(Above / StandIn.Scale - StandIn.Sole)), 0};
			Model->FloorY = 0;
			Model->bShown = 1;
			// The space goes where the sprite stands on its floor, at the sprite's size.
			FAffine Placement;
			Placement.M = {{{StandIn.Scale, 0.0f, 0.0f}, {0.0f, StandIn.Scale, 0.0f}, {0.0f, 0.0f, StandIn.Scale}}};
			Placement.T = {static_cast<float>(StandIn.Position[0]) / FixedOne, static_cast<float>(StandIn.FloorY), static_cast<float>(StandIn.Position[2]) / FixedOne};
			Model->Placement = Placement;
		}
		for (const std::unique_ptr<FModule>& Module : Modules)
		{
			if (auto Error = Module->Models.DrawPosed())
			{
				return Error;
			}
		}
		return std::nullopt;
	}
}
