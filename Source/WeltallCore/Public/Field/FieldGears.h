#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Gears/GearFiles.h"
#include "Gears/GearModels.h"

#include <array>
#include <memory>
#include <optional>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	struct FEventActorState;
	struct FEventContext;

	/** Where the field reads a gear's files when a script swaps it (FE 5C). */
	class IFieldGearFiles
	{
	public:
		virtual ~IFieldGearFiles() = default;
		virtual TResult<FGearFiles> Read(int32 GearModel) = 0;
		/**
		 * Id's red gear made to be worn over Fei's gear FeisGear's model (its battle model's, the same), its parts and
		 * textures for Place; none for another gear or files that cannot be read.
		 */
		virtual std::shared_ptr<const IGearDress> IdsDress(uint8 FeisGear, const FGearTexturePlace& Place) = 0;
		/**
		 * Gear WearerGear's model made to be worn over gear PerformerGear's (its battle model's, as the battle pairs them:
		 * Regurus over Vierge), its parts and textures for Place; none without a pairing or files that cannot be read.
		 */
		virtual std::shared_ptr<const IGearDress> GearDress(uint8 PerformerGear, uint8 WearerGear, const FGearTexturePlace& Place) = 0;
	};

	/** The gears' files on the disc, read at once. */
	class FFieldDiscGearFiles final : public IFieldGearFiles
	{
	public:
		FFieldDiscGearFiles(const FCdImage& InImage, const FGameFileIndex& InIndex) : Image(&InImage), Index(&InIndex) {}
		WELTALLCORE_API TResult<FGearFiles> Read(int32 GearModel) override;
		WELTALLCORE_API std::shared_ptr<const IGearDress> IdsDress(uint8 FeisGear, const FGearTexturePlace& Place) override;
		WELTALLCORE_API std::shared_ptr<const IGearDress> GearDress(uint8 PerformerGear, uint8 WearerGear, const FGearTexturePlace& Place) override;

	private:
		const FCdImage* Image;
		const FGameFileIndex* Index;
	};

	/**
	 * The field's gears: what the scripts register (opcode 93) and change, the arrays the field keeps for
	 * them (four entries each, as the original's), and the gears' module once the map's load has loaded
	 * them. A gear actor's model is the slot its script state's bits 13-15 name.
	 */
	struct FFieldGears
	{
		static constexpr int32 Capacity = 4;
		static constexpr int16 DefaultPoolSize = 0x40;
		/** The slots a gear actor's three bits can name. */
		static constexpr int32 ActorSlots = 8;

		int32 Count = 0;
		/** Each gear's model number times two. */
		std::array<uint16, Capacity> FileNumbers{};
		/** The animation each last played, which the field plays again when it comes back from a battle or a menu. */
		std::array<int16, Capacity> Animations{};
		/** Each model's scale as its load left it, which the actor's scale multiplies. */
		std::array<int32, Capacity> LoadScales{};
		/** Columns each gear's textures move left of its slot's place (FE DC). */
		std::array<uint8, Capacity> TextureShifts{};
		/** The channel pool the module is made with (FE CE). */
		int16 PoolSize = DefaultPoolSize;
		/** The gear actors the floor forces have met this tick: the field takes their root motion by this count. */
		int32 MetThisTick = 0;
		std::unique_ptr<FGearModels> Models;
		/** Counts the modules made (a map's load, a movie's end): each loads its gears' textures afresh. */
		int32 Loads = 0;
		/** The files FE 5C read for a slot, until it loads them. */
		std::array<std::optional<FGearFiles>, ActorSlots> Pending;
		/**
		 * The GTE's lighting as the last frame's drawing left it, which a gear loaded after the map's load is
		 * built under; nothing where it is not known, and a gear lit at its build and never again cannot be loaded.
		 */
		std::optional<FGte> DrawnLighting;
		IFieldGearFiles* Files = nullptr;
	};

	/** Which models the field's gears hold: it changes whenever a model is loaded or freed, or the module is made again. */
	WELTALLCORE_API uint32 FieldGearLayout(const FFieldGears& Gears);

	/** The gear model slot of a gear actor's script state. */
	WELTALLCORE_API int32 FieldGearSlot(const FEventActorState& State);

	/** A gear's texture place: its slot's column (shifted by FE DC) at y 100h, its CLUT in row FCh + slot. */
	WELTALLCORE_API FGearTexturePlace FieldGearTexturePlace(const FFieldGears& Gears, int32 Slot);

	/**
	 * The map's load, after its init: the module made with the scripts' pool and every registered gear
	 * loaded (textures at its slot's place, root at the origin), its load scale kept. Nothing without
	 * gears.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> LoadFieldGears(FEventContext& Context);

	/** A movie's start on a map with gears: the models freed with the module, whose place the movie's player takes. */
	WELTALLCORE_API void FreeFieldGearsForMovie(FEventContext& Context);

	/**
	 * A movie's end: the module made again and every registered gear loaded afresh, from its script 0
	 * (the animation it played is not played again), built under the lighting the last frame before the
	 * movie left and the gears' ambient.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> ReloadFieldGears(FEventContext& Context);

	/**
	 * The gears' part of the field's return from a battle, once the map is loaded from the arrays the
	 * field kept before the battle: every registered gear plays again the animation it last played.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> ReplayFieldGearAnimations(FEventContext& Context);

	/**
	 * What the field's drawing writes into each gear actor's model, the k-th gear actor met (in actor
	 * order) taking model k: the floor shadow off while a script moves it, it stands on a certain floor
	 * or flies; shown unless hidden; the yaw either way (FE 46); the scale; the floor's height; the
	 * root's x and z. The actor is never off screen.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> PlaceFieldGear(FEventContext& Context, int32 Actor, int32 Met);

	/** The field's gear tick: one gear tick a field tick, whatever the frame delay. Nothing without gears. */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> TickFieldGears(FEventContext& Context);

	/** The model of a gear actor's slot; null without one, where the original reads the module's memory. */
	WELTALLCORE_API FGearModel* FieldGearModel(FEventContext& Context, int32 Slot);

	/**
	 * The world matrix of a gear model's joint as the field reads it (the root's local matrix for joint
	 * 0): nothing for an empty slot or without the module; a fault of the module for a bone its skeleton
	 * lacks.
	 */
	WELTALLCORE_API std::optional<FGteMatrix> FieldGearJoint(FEventContext& Context, int32 Slot, int32 Joint);
}
