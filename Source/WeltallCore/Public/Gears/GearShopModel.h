#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Gears/GearFiles.h"
#include "Gears/GearModels.h"
#include "Gears/GearPackets.h"

#include <array>
#include <memory>
#include <optional>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/** How the gear shop shows one of the characters' gears. */
	struct FGearShopGear
	{
		static constexpr int32 NoModel = -1;

		/** The gear model whose two files the shop reads (FGearFileNumbers), NoModel for none. */
		int32 GearModel = NoModel;
		/** The floor the model's root stands on. */
		int16 Height = 0;
		/** The model's scale (4.12), in place of the one its load gave it. */
		int16 Scale = 0;
		/** The script it plays. */
		uint8 Animation = 0;
	};

	/** The gear shop's table of the characters' gears, by gear number (a character's gear byte). */
	struct FGearShopTable
	{
		static constexpr int32 Gears = 17;
		std::array<FGearShopGear, Gears> Entries{};
	};

	/**
	 * The gear shop's table as the shop's overlay on the disc holds it (directory 16 file 10): each
	 * gear's first file, animation, height and scale. A gear without a file (Chu-Chu's, 7) has no model.
	 */
	WELTALLCORE_API TResult<FGearShopTable> ReadGearShopTable(const FCdImage& Image, const FGameFileIndex& Index);

	struct FGearShopModelOptions
	{
		FGearModelOptions Models;
		/**
		 * The model animates at the field's pace, a tick every two vblanks, instead of the original's
		 * three ticks every other menu frame, which runs its animations three times too fast.
		 */
		bool bFieldPace = false;
	};

	/**
	 * The gear shop's model: the gears' module with one model, the shown pilot's gear, in slot 1. Its
	 * textures go at x 240h, y 0, its CLUTs on row 1C1h; its root stands at the gear's height, turned
	 * -400h about y and -20h about x, playing the gear's animation. Every menu frame the module runs at
	 * the shop's cadence: the original passes the module a code address as its extra vblanks, so the
	 * counter goes negative on one frame and wraps past its cap on the next, three ticks every other frame.
	 *
	 * The shop draws the model with one directional light (LightDirection, grey ColorScale in each
	 * channel), the ambient Ambient, under its camera, into its own ordering table with the kernel's
	 * ordering-table shift 0 (the field's is 2); the menu owns the camera and the rest of the screen.
	 */
	class FGearShopModel
	{
	public:
		static constexpr int32 Slot = 1;
		static constexpr int32 Pool = 0x40;
		static constexpr FGearTexturePlace TexturePlace{0x200 + 0x40 * Slot, 0, 0, 0x1C0 + Slot};
		/** The shop's word in place of a vblank count (a return address): 3 ticks every other frame. */
		static constexpr uint32 OriginalExtraVblanks = 0x801CB480;
		/** The root's turn after the load, about y and about x. */
		static constexpr int16 YawTurn = -0x400;
		static constexpr int16 PitchTurn = -0x20;
		static constexpr std::array<int16, 3> LightDirection{0x546, -0xE39, 0x546};
		static constexpr int16 ColorScale = 0x600;
		static constexpr uint8 Ambient = 0x3C;

		WELTALLCORE_API FGearShopModel();

		/** A fresh module of Pool channels with nothing shown, its loads built under ShopBuildLighting. */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Open(const FGearShopModelOptions& InOptions = {});

		/**
		 * Shows a gear: the model shown before freed, the gear's files loaded into slot 1 (its script 0
		 * run), its height and scale set, its root turned, its animation started. An error for a gear
		 * without a model or where the module stops.
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Show(const FGearShopGear& Gear, FGearFiles Files);

		/** One menu frame of the model: the module's frame at the shop's cadence (or the field's pace). */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Frame();

		/** The shop closes: every model freed. */
		WELTALLCORE_API void Close();

		/**
		 * The lighting a load builds its parts' packets under, which only the polygons the drawing never
		 * relights keep (gear model 44's, of the shop's gears). The original's is whatever the drawing
		 * before left in the GTE: for the first gear the screen before the shop, for a switch the last
		 * frame's last gear part's light matrix; the port's is the shop's ambient alone (a light matrix of
		 * zeros, the back colour the shop sets before its first load, the shop's colour matrix).
		 */
		WELTALLCORE_API static FGte ShopBuildLighting();

		/** Another build lighting for the loads to come; nothing refuses a gear with such polygons. */
		void SetBuildLighting(const std::optional<FGte>& Lighting) { Module->BuildLighting = Lighting; }

		const FGearModels& Models() const { return *Module; }
		FGearModels& Models() { return *Module; }
		const FGearModel* Shown() const { return Module->Model(Slot); }

	private:
		std::unique_ptr<FGearModels> Module;
		FGearShopModelOptions Options;
	};
}
