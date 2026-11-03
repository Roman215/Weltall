#pragma once

#include "Battle/Show/BattleShow.h"
#include "Battle/Show/BattleSpriteFiles.h"
#include "Foundation/Types.h"
#include "Graphics/SpriteSystem.h"

#include <array>
#include <optional>
#include <vector>

namespace Weltall
{
	class FBattleSceneGraphics;
	class FIdsBattleSprite;

	/**
	 * The battle setup's loads as the task the original runs them in, a step a
	 * frame once the disc has delivered what the step before asked for (the port's files are at hand at
	 * once): the scene's graphics and its enemies, the party's sprites (running in for a battle of kind
	 * 0), the common effects' images, sprite and sounds, the gears queued, then a countdown and the wait
	 * for the party to land, which starts the battle. It keeps the files the battle's sprites read.
	 */
	class FBattleShowLoad final : public IKernelTask
	{
	public:
		FBattleShowLoad(FBattleShow& InShow, const FBattleShowSetup& Setup);

		void Update() override;

		/** Each party slot's attack file (the sprite's extra block's first word), 0 for a slot without a sprite. */
		uint32 AttackFile(int32 Slot) const { return AttackFiles[static_cast<std::size_t>(Slot)]; }

	private:
		enum class EStep : uint8
		{
			Scene,
			Party,
			Effects,
			Sounds,
			Gears,
			Landing,
			Done,
		};

		/** A resource block of the setup's: a sprite file and where its textures and palettes go. */
		struct FResource
		{
			const FBattleSpriteFile* File = nullptr;
			int16 TextureX = 0;
			int16 TextureY = 0;
			uint8 PaletteSet = 0;
		};

		static constexpr int32 SlotCount = 11;
		/** A scene's graphics hold an entry for each of its eight enemy types at most. */
		static constexpr int32 SceneEntries = 8;
		static constexpr int32 PartySlots = 3;

		/** The image sets uploaded, the sprite enemies' resources, the model enemies made. */
		void LoadScene();
		/** The sprite file at Data in the scene's graphics, parsed once for every entry that names it. */
		const FBattleSpriteFile* SceneFile(uint32 Data);
		/** The enemies' sprites. */
		void CreateEnemies();
		/** The party's sprites, and the run-in of a battle of kind 0. */
		void CreateParty();
		/** The effects' step: the effects' images, the common effects' resource, the battle's sounds. */
		void LoadEffects();
		void QueueGears();
		void Land();
		/** Slot Slot's combatant from resource Resource, facing its default target. */
		void CreateSprite(int32 Slot, const FResource& Resource);
		bool OnFoot(int32 Slot) const;
		/** Whether the slot's member is Fei drawn as Id, with his own battle sprite. */
		bool DrawnAsId(int32 Slot) const;
		void Fail(EBattleShowFault Fault);

		FBattleShow& Show;
		const FBattleSpriteFiles* SpriteFiles = nullptr;
		const FBattleSceneGraphics* Scene = nullptr;
		const FIdsBattleSprite* IdsSprite = nullptr;
		uint8 Kind = 0;
		EStep Step = EStep::Scene;
		int32 Countdown = 0;
		std::array<FResource, SlotCount> Resources{};
		std::array<uint32, PartySlots> AttackFiles{};
		/**
		 * The scene's sprite enemies' files, one for each place in the scene's data (entries may share one),
		 * the party's and the common effects', kept for the battle: the scripts write into them.
		 */
		std::array<std::optional<FBattleSpriteFile>, SceneEntries> SceneFiles;
		std::array<uint32, SceneEntries> SceneFileData{};
		int32 SceneFileCount = 0;
		std::array<std::optional<FBattleSpriteFile>, PartySlots> PartyFiles;
		std::optional<FBattleSpriteFile> CommonFile;
		std::vector<uint8> CommonImages;
		std::vector<uint8> CommonSounds;
	};
}
