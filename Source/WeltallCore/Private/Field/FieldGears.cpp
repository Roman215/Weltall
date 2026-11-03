#include "Field/FieldGears.h"

#include "Battle/Show/BattleGearDress.h"
#include "Battle/Show/BattleObjectFiles.h"
#include "Battle/Show/BattleRegurus.h"
#include "Field/EventContext.h"
#include "Game/CharacterLooks.h"
#include "Game/GameState.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"

#include <limits>
#include <string>

namespace Weltall
{
	namespace
	{
		constexpr uint32 GearSlotShift = 13;
		constexpr int32 FirstTextureColumn = 0x240;
		constexpr int32 TextureColumnWidth = 0x40;
		constexpr int16 TextureRow = 0x100;
		constexpr int16 FirstClutRow = 0xFC;
		/** The floor words on which a gear casts no shadow. */
		constexpr uint32 ShadowlessFloors = 0x200002;
		/** The model's yaw is three quarters of a turn on from its actor's. */
		constexpr int32 ModelYawOffset = 0xC00;

		/** The Yggdrasil's hangar close up, the bay it shows (var[002]), and Fei's bay. */
		constexpr int32 HangarCloseUp = 293;
		constexpr uint32 BayVariable = 2;
		constexpr uint16 FeisBay = 0;
		/** The field's models of Fei's gears, and the gears they are. */
		constexpr std::array<std::pair<int32, uint8>, 3> FeisGearModels{{{0, FCharacterLooks::Weltall}, {42, FCharacterLooks::Weltall2}, {57, FCharacterLooks::Xenogears}}};
		/** The hangar's other bays close up, Elly's bay there, the field's model of Vierge it shows, and Elly. */
		constexpr int32 OtherBaysCloseUp = 296;
		constexpr uint16 EllysBay = 4;
		constexpr int32 ViergesFieldModel = 1;
		constexpr int32 Elly = 1;

		/** A gear tick for each field tick: the field passes one extra vblank for its two. */
		constexpr uint32 FieldExtraVblanks = 1;

		FError GearError(const std::string& What)
		{
			return MakeError("the field's gears: " + What);
		}

		/**
		 * Each point light that follows an actor sets its directional light's colour from the actor's
		 * distance to its place, in the original's 32-bit arithmetic: the colours' difference over the
		 * radius in 16.16, times what is left of the radius, its top half kept.
		 */
		std::optional<FError> FadePointLights(FEventContext& Context)
		{
			FFieldGearLights& Lights = Context.Render.GearLights;
			for (int32 Light = 0; Light < FFieldGearLights::PointLights; ++Light)
			{
				const FFieldGearLights::FPointLight& Point = Lights.Points[static_cast<std::size_t>(Light)];
				if (Point.Actor == FFieldGearLights::FPointLight::Off)
				{
					continue;
				}
				const FEventActorState* State = Context.StateOf(static_cast<uint32>(Point.Actor));
				if (State == nullptr)
				{
					return GearError("point light " + std::to_string(Light) + " follows actor " + std::to_string(Point.Actor)
						+ ", which has no script state (the original reads its record's memory)");
				}
				uint32 Squares = 0;
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Squares += static_cast<uint32>(GteSquare(Point.Position[Axis] - static_cast<int16>(State->Position[Axis] >> 16)));
				}
				if (static_cast<int32>(Squares) < 0)
				{
					return GearError("point light " + std::to_string(Light) + "'s actor lies so far that libgte's square root reads outside its table");
				}
				const int32 Left = Point.Radius - SquareRoot0(static_cast<int32>(Squares));
				for (int32 Channel = 0; Channel < 3; ++Channel)
				{
					const std::size_t At = static_cast<std::size_t>(Channel);
					const int32 Difference = static_cast<int32>(static_cast<uint32>(Point.Near[At] - Point.Far[At]) << 16);
					// The compiler's checks around the division trap on these.
					if (Point.Radius == 0 || (Point.Radius == -1 && Difference == std::numeric_limits<int32>::min()))
					{
						return GearError("point light " + std::to_string(Light) + "'s radius of " + std::to_string(Point.Radius) + " makes the original's division trap");
					}
					const uint32 Faded = static_cast<uint32>(Difference / Point.Radius) * static_cast<uint32>(Left);
					Lights.Words[static_cast<std::size_t>(FFieldGearLights::ColorWord(Light, Channel))] = static_cast<int16>(Faded >> 16);
				}
			}
			return std::nullopt;
		}
	}

	TResult<FGearFiles> FFieldDiscGearFiles::Read(int32 GearModel)
	{
		return ReadGearFiles(*Image, *Index, GearModel);
	}

	namespace
	{
		/** The battle's dress of Pairing's wearer over its performer, from their battle files (a scene's model for Id's gear). */
		std::shared_ptr<const IGearDress> MakeGearDress(const FCdImage& Image, const FGameFileIndex& Index, const FBattleGearDress::FPairing* Pairing,
			const FGearTexturePlace& Place)
		{
			if (Pairing == nullptr)
			{
				return nullptr;
			}
			const TResult<FBattleObjectTables> Tables = FBattleObjectTables::Load(Image, Index);
			if (!Tables)
			{
				return nullptr;
			}
			TResult<FBattleModelFiles> Performer = BattleObjectFiles::ReadGear(Image, Index, *Tables, Pairing->PerformerGear, 0);
			TResult<FBattleModelFiles> Wearer = Pairing->WearerScene != FBattleGearDress::NoScene
				? BattleObjectFiles::ReadSceneModel(Image, Index, Pairing->WearerScene, Pairing->WearerEntry)
				: BattleObjectFiles::ReadGear(Image, Index, *Tables, Pairing->WearerGear, 0);
			if (!Performer || !Wearer)
			{
				return nullptr;
			}
			TResult<FBattleGearDress> Made = FBattleGearDress::Make(*Pairing, Performer->Model, Performer->Motion, std::move(Wearer->Model), Wearer->Motion, &Place);
			return Made ? std::make_shared<const FBattleGearDress>(std::move(*Made)) : nullptr;
		}
	}

	std::shared_ptr<const IGearDress> FFieldDiscGearFiles::IdsDress(uint8 FeisGear, const FGearTexturePlace& Place)
	{
		return MakeGearDress(*Image, *Index, FBattleGearDress::IdsPairing(FeisGear), Place);
	}

	std::shared_ptr<const IGearDress> FFieldDiscGearFiles::GearDress(uint8 PerformerGear, uint8 WearerGear, const FGearTexturePlace& Place)
	{
		return MakeGearDress(*Image, *Index, FBattleGearDress::FindPairing(PerformerGear, WearerGear), Place);
	}

	uint32 FieldGearLayout(const FFieldGears& Gears)
	{
		constexpr uint32 RevisionBits = 16;
		const uint32 Revision = Gears.Models ? Gears.Models->Revision : 0;
		return static_cast<uint32>(Gears.Loads) << RevisionBits | (Revision & ((1u << RevisionBits) - 1));
	}

	int32 FieldGearSlot(const FEventActorState& State)
	{
		return static_cast<int32>((State.Status >> GearSlotShift) & 7);
	}

	FGearTexturePlace FieldGearTexturePlace(const FFieldGears& Gears, int32 Slot)
	{
		const int32 Shift = Gears.TextureShifts[static_cast<std::size_t>(Slot)];
		return {static_cast<int16>(FirstTextureColumn - (Slot + Shift) * TextureColumnWidth), TextureRow, 0, static_cast<int16>(FirstClutRow + Slot)};
	}

	namespace
	{
		/**
		 * The gear a bay of the Yggdrasil's hangar close up shows, dressed: Fei's (map 293's bay 0) as Id's red gear of its
		 * time while he is drawn as Id, Elly's Vierge (map 296's bay 4) as Regurus while she holds it, as the hangar parks
		 * it; the bay's lowdown and renaming stay the scripts'. Elsewhere the scripts' gears are the story's.
		 */
		std::shared_ptr<const IGearDress> BayDress(FEventContext& Context, int32 Slot)
		{
			FFieldGears& Gears = Context.Gears;
			if (Context.GameState == nullptr)
			{
				return nullptr;
			}
			const int32 Model = Gears.FileNumbers[static_cast<std::size_t>(Slot)] / 2;
			const uint16 Bay = Context.Variables.Raw(BayVariable);
			if (Context.RunningMap == HangarCloseUp && Bay == FeisBay)
			{
				for (const auto& [FieldModel, Gear] : FeisGearModels)
				{
					if (FieldModel == Model && Context.GameState->CharacterLooks().WearsIdsGear(Gear))
					{
						return Gears.Files->IdsDress(Gear, FieldGearTexturePlace(Gears, Slot));
					}
				}
			}
			if (Context.RunningMap == OtherBaysCloseUp && Bay == EllysBay && Model == ViergesFieldModel
				&& Context.GameState->CharacterGear(Elly) == BattleRegurus::Regurus)
			{
				return Gears.Files->GearDress(BattleRegurus::Vierge, BattleRegurus::Regurus, FieldGearTexturePlace(Gears, Slot));
			}
			return nullptr;
		}

		/** The gears' module made afresh and every registered gear loaded, its builds under Lighting (none: unknown). */
		std::optional<FError> MakeFieldGears(FEventContext& Context, const std::optional<FGte>& Lighting)
		{
			FFieldGears& Gears = Context.Gears;
			if (Gears.Count == 0)
			{
				return std::nullopt;
			}
			if (Gears.Files == nullptr)
			{
				return GearError("the map has gears, and no disc to read their files from");
			}
			Gears.Models = std::make_unique<FGearModels>();
			++Gears.Loads;
			FGearModels& Models = *Gears.Models;
			Models.Random = Context.Random;
			Models.Arctangents = Context.Arctangents;
			FGearModelOptions Options;
			Options.bChannelPoolGrows = Context.Options.bGearChannelPoolGrows;
			if (auto Error = Models.Init(Gears.PoolSize, Options))
			{
				return Error;
			}
			Models.BuildLighting = Lighting;
			for (int32 Slot = 0; Slot < Gears.Count; ++Slot)
			{
				auto Files = Gears.Files->Read(Gears.FileNumbers[static_cast<std::size_t>(Slot)] / 2);
				if (!Files)
				{
					return Files.Error();
				}
				if (auto Error = Models.Load(Slot, std::move(Files->Model), std::move(Files->Motion), FieldGearTexturePlace(Gears, Slot), {0, 0, 0}))
				{
					return Error;
				}
				Gears.LoadScales[static_cast<std::size_t>(Slot)] = Models.Model(Slot)->Scale;
				Models.Wear(Slot, BayDress(Context, Slot));
			}
			// A later load (a script's swap) builds under whatever the last frame's drawing left by then.
			Models.BuildLighting.reset();
			return std::nullopt;
		}
	}

	std::optional<FError> LoadFieldGears(FEventContext& Context)
	{
		// The map's load builds under the lights the map's scenery was built under, a light matrix of
		// zeros, and the gears' ambient light the field sets just before.
		FGte Lighting;
		for (std::size_t Channel = 0; Channel < 3; ++Channel)
		{
			Lighting.BackColor[Channel] = Context.Render.GearLights.Ambient[Channel] * 16;
		}
		return MakeFieldGears(Context, Lighting);
	}

	void FreeFieldGearsForMovie(FEventContext& Context)
	{
		if (Context.Gears.Count != 0 && Context.Gears.Models)
		{
			Context.Gears.Models->FreeAll();
			Context.Gears.Models.reset();
		}
	}

	std::optional<FError> ReloadFieldGears(FEventContext& Context)
	{
		return MakeFieldGears(Context, Context.Gears.DrawnLighting);
	}

	std::optional<FError> ReplayFieldGearAnimations(FEventContext& Context)
	{
		FFieldGears& Gears = Context.Gears;
		for (int32 Slot = 0; Slot < Gears.Count; ++Slot)
		{
			if (!Gears.Models)
			{
				return GearError("the gears' last animations are played again with no gears loaded");
			}
			if (auto Error = Gears.Models->PlayAnimation(Slot, 0, Gears.Animations[static_cast<std::size_t>(Slot)]))
			{
				return Error;
			}
		}
		return std::nullopt;
	}

	std::optional<FError> PlaceFieldGear(FEventContext& Context, int32 Actor, int32 Met)
	{
		FGearModel* Model = Met < FFieldGears::Capacity ? FieldGearModel(Context, Met) : nullptr;
		if (Model == nullptr)
		{
			return GearError("gear actor " + std::to_string(Actor) + " is the gear actor " + std::to_string(Met)
				+ " the drawing meets, and there is no model loaded for it (the original writes into the module's memory)");
		}
		FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
		const FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Actor)];
		const bool bShadowless = (State.Flags0 & EventActorFlags0::MovedByScript) != 0 || (State.FloorAttributes & ShadowlessFloors) != 0
			|| (State.Flags1 & EventActorFlags1::NotFloorAligned) != 0;
		Model->Flags = static_cast<uint16>(bShadowless ? Model->Flags | GearModelFlags::NoShadow : Model->Flags & ~GearModelFlags::NoShadow);
		Model->bShown = (Record.Flags & FieldActorFlag::Hidden) == 0 ? 1 : 0;
		FGearBone& Root = Model->Bones[0];
		if ((State.Flags1 & EventActorFlags1::GearMovesIt) == 0)
		{
			Root.Angles[1] = static_cast<int16>(State.Yaw + ModelYawOffset);
		}
		else
		{
			const uint16 Yaw = static_cast<uint16>(Root.Angles[1] - ModelYawOffset);
			State.Yaw = Yaw;
			State.Facing = Yaw;
		}
		Model->Scale = static_cast<int16>((State.Scale[0] * Context.Gears.LoadScales[static_cast<std::size_t>(Met)]) >> 12);
		Model->FloorY = static_cast<int16>(State.Position[1] >> 16);
		Root.Position[0] = static_cast<int16>(State.Position[0] >> 16);
		Root.Position[2] = static_cast<int16>(State.Position[2] >> 16);
		State.Flags1 &= ~EventActorFlags1::OffScreen;
		return std::nullopt;
	}

	std::optional<FError> TickFieldGears(FEventContext& Context)
	{
		FFieldGears& Gears = Context.Gears;
		if (Gears.Count == 0)
		{
			return std::nullopt;
		}
		if (!Gears.Models)
		{
			return GearError("the field counts gears and holds no module: gears registered after the map's load, or a movie in the scene "
				"after the module was freed for it (the original runs the movie player's memory)");
		}
		if (auto Error = FadePointLights(Context))
		{
			return Error;
		}
		return Gears.Models->Tick(FieldExtraVblanks);
	}

	FGearModel* FieldGearModel(FEventContext& Context, int32 Slot)
	{
		return Context.Gears.Models ? Context.Gears.Models->Model(Slot) : nullptr;
	}

	std::optional<FGteMatrix> FieldGearJoint(FEventContext& Context, int32 Slot, int32 Joint)
	{
		return Context.Gears.Models ? Context.Gears.Models->BoneMatrix(Slot, Joint) : std::nullopt;
	}
}
