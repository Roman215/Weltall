#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Field/IdsFieldSprite.h"
#include "Game/CharacterLooks.h"
#include "Input/PadState.h"
#include "Text/MesWindow.h"
#include "WorldMap/WorldMapContext.h"
#include "WorldMap/WorldMapTables.h"

#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	namespace WorldMapOps
	{
		struct FCutscene;
	}
	class FGameFileIndex;
	class FGameSession;
	class FGameState;
	class FKernelPad;
	class FPsxRandom;
	class FStoryPacks;
	struct FGteArctangents;
	struct FPlayClock;

	/**
	 * What the world map keeps through a battle (its block in the kernel's memory):
	 * the task slots (their sprites freed), the controlled task's place and heading, the encounters'
	 * period, the party's trail and the camera. The next Enter after the battle starts from it.
	 */
	struct FWorldMapBattleKeep
	{
		std::array<FWorldMapTask, FWorldMapContext::TaskCount> Tasks{};
		FWorldMapVector Controlled;
		int16 ControlledHeading = 0;
		int32 EncounterPeriod = 0;
		int32 EncounterSlots = 0;
		int32 EncounterLeft = 0;
		std::array<int16, FWorldMapEncounters::SlotCapacity> EncounterTimers{};
		std::array<FWorldMapTrailEntry, FWorldMapContext::TrailLength> Trail{};
		int16 TrailIndex = 0;
		int16 CameraPitch = 0;
		int16 CameraYaw = 0;
		int16 CameraRoll = 0;
		int32 CameraDistance = 0;
		int32 ScreenCentreY = 0;
		FWorldMapVector CameraTarget;
	};

	/**
	 * What the world map takes from the game around it: the disc's files and the program's tables, the
	 * game's state, random numbers, the console's pad and play clock, the text system's font, name tables
	 * and button map, and the sound. Each must outlive the world map.
	 */
	struct FWorldMapServices
	{
		const FCdImage* Image = nullptr;
		const FGameFileIndex* Index = nullptr;
		const FWorldMapTables* Tables = nullptr;
		const FGteArctangents* Arctangents = nullptr;
		FGameState* Game = nullptr;
		FPsxRandom* Random = nullptr;
		FKernelPad* Pad = nullptr;
		FPlayClock* Clock = nullptr;
		FMesContext Text;
		/** What the last battle's request kept, for the return from it (the scene word's bit 8000h); none otherwise. */
		const FWorldMapBattleKeep* BattleKeep = nullptr;
		/** None: the world map is silent. */
		IWorldMapSound* Sound = nullptr;
		/** The fixes of its broken effects in use. */
		FWorldMapOptions Options;
		/** The sprites' mirrored pieces drawn where their frames were made to put them (FSpriteSystem::bMirroredPartsPlacedByArt). */
		bool bMirroredSpritePartsPlacedByArt = false;
		/** How often the player asked random battles to come. */
		EEncounterRate EncounterRate = EEncounterRate::Original;
		/** The kernel's lock on boarding the gears as the world map starts: a cutscene leaves it as it is. */
		uint8 GearLock = 0;
		/** The port's story, whose documents add places to an era's scene as it is read (the game's state says whether the expanded second disc's do); none: the disc's places alone. */
		const FStoryPacks* Story = nullptr;
		/** The shared play clock and pad pacing when running inside the game loop. */
		FGameSession* Session = nullptr;
	};

	/** What the world map stopped at. */
	enum class EWorldMapRequest : uint8
	{
		None,
		/** A place entered (or the Yggdrasil's interior): the field's map and entrance are in the game state. */
		Field,
		/** An encounter: the formation drawn from the region's records; the mount flags are in the game state. */
		Battle,
		/** The menu (mode 0, saving allowed), which the world map runs inside itself: ResumeAfterMenu goes on after it. */
		Menu,
		/** L1, R1, Start and Select held together: the console starts again. */
		SoftReset,
	};

	struct FWorldMapRequest
	{
		EWorldMapRequest Kind = EWorldMapRequest::None;
		/** Field: the place's record left by. */
		FWorldMapTrigger Record;
		/** Battle: the formation (0-15) and the encounter region whose 16 formation records it indexes. */
		uint8 Formation = 0;
		int32 Region = 0;
		/** The next program the kernel starts: the field 1, the battle 2; 0 for the menu and the reset. */
		uint8 NextProgram = 0;
		/** Battle: what the world map keeps for its return, which the caller hands back in the services. */
		std::shared_ptr<const FWorldMapBattleKeep> Keep;
	};

	/** What the world map's last frame was. */
	enum class EWorldMapFrame : uint8
	{
		/** A tick of the loop. */
		Tick,
		/** A frame of the pause Start makes (one vertical blank): nothing moves. */
		Paused,
	};

	/**
	 * The world map while it plays: a playable scene (0-7) of the era the story has reached, entered from
	 * the game state as the program enters it, then its loop's passes, tick for tick, until a place
	 * is entered, a battle falls, the menu is asked for or the console is reset. The scene's start draws
	 * no vertical blanks here: the fade it shows over the screen before (EntryFadeVblanks) is the caller's.
	 */
	class FWorldMapRuntime
	{
	public:
		/** The vertical blanks of the screen's fade to black that starts a scene, before the loop's first tick. */
		static constexpr int32 EntryFadeVblanks = 66;

		WELTALLCORE_API explicit FWorldMapRuntime(const FWorldMapServices& InServices);
		WELTALLCORE_API ~FWorldMapRuntime();
		FWorldMapRuntime(const FWorldMapRuntime&) = delete;
		FWorldMapRuntime& operator=(const FWorldMapRuntime&) = delete;

		/**
		 * Enters the world map from the game state (the place, entrance, camera yaw and scene words the
		 * field or a load left, the party and who rides), or after a battle from what its request kept:
		 * the scene's files read, the party placed, the tasks set up and their inits run. An error names
		 * what the port cannot run: a cutscene scene, a scene the program would read past its tables, a
		 * return from a battle without what it kept, a cutscene the port does not play yet. A cutscene runs to its own end and
		 * leaves for the field.
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Enter();

		/** The vertical blanks the next frame lasts: a tick's two, or the pause's one. */
		WELTALLCORE_API int32 VblanksPerTick() const;

		/**
		 * The world map's next frame, with the buttons held at each of its vertical blanks (VblanksPerTick
		 * of them): a pass of the loop (the pad's words the last frame's vertical blanks queued, the
		 * tasks, the vertical blanks, then the loop's checks: the gears' L2 and R2, the pause, the
		 * encounter, Select's minimap, the menu), or a frame of the pause. What the pass draws goes to
		 * Drawing, when given. An error while a request is pending, or naming what stopped the world map.
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Tick(std::span<const FPadState> HeldPerVblank, IWorldMapDrawing* Drawing = nullptr);

		/** What the world map stopped at, Kind None while it plays on. */
		const FWorldMapRequest& Request() const { return PendingRequest; }

		/** The world map goes on after the menu it stopped at: the pad's queue emptied, the mounts the menu changed applied. */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> ResumeAfterMenu();

		/**
		 * Whether the menu's button would open the menu with the party where it stands: between passes of the loop, the
		 * scene the player's (no cutscene, fade, place's name or notice, nothing asked), the party on foot or in gears.
		 */
		WELTALLCORE_API bool CouldOpenMenu() const;
		/**
		 * What the menu's opening writes into the game state for a save made in it, written into Game: the party's
		 * place kept for the next start (where the party stands and faces, the camera's yaw marked so) and who rides.
		 */
		WELTALLCORE_API void KeepPlaceAsMenuDoes(FGameState& Game) const;

		EWorldMapFrame LastFrame() const { return LastFrameKind; }
		/** Whether the last frame finished a pass of the loop: a tick that did not pause, or the pause's last frame. */
		bool PassEnded() const { return bPassEnded; }
		WELTALLCORE_API FWorldMapContext& Context();
		WELTALLCORE_API const FWorldMapContext& Context() const;
		WELTALLCORE_API const FWorldMapScene& Scene() const;
		/** The blend of the fade the scene's start shows over the screen before: 2 to black, 1 to white. */
		int32 EntryFadeBlend() const { return EntryBlend; }
		/** Whether the scene entered is a cutscene (8-18). */
		bool IsCutscene() const { return Cutscene != nullptr; }
		/** Id's field sprite while a member is drawn as Id: its picture belongs in the port's rows of the drawing's VRAM. */
		const FIdsFieldSprite* IdShown() const { return bIdShown ? &*Id : nullptr; }
		/** Counts the times the party's sprite files were made: the drawing places Id's picture again when it changes. */
		uint32 PartyLoads() const { return PartyLoadCount; }

	private:
		enum class EPhase : uint8
		{
			NotEntered,
			Loop,
			PauseEntry,
			Paused,
			PauseExit,
			Stopped,
		};

		std::optional<FError> LoadScene();
		/** The party's sprite files, each slot's of the member the player's order shows there (FPartyOrder). */
		std::optional<FError> LoadParty();
		/**
		 * The members whose sprites the party's slots show: the party in the order its player keeps it, each member in
		 * a slot that rides or walks as its own does.
		 */
		std::array<uint8, 3> ShownMembers() const;
		/**
		 * Where the slots' members are no longer those shown, or no longer drawn so (the menu arranged the party or
		 * switched a look, or who rides changed): the party's sprites made again as they stood.
		 */
		std::optional<FError> ShowArrangedParty();
		/** The scene's sound as it starts: the scene's bank, effects and song, or after a battle the song kept from before it. */
		void StartSceneSound(int32 GroupHeader, bool bFromBattle);
		void StartScene();
		void RestoreAfterBattle(const FWorldMapBattleKeep& Keep);
		std::shared_ptr<const FWorldMapBattleKeep> SaveForBattle() const;
		void RunVblanks(std::span<const FPadState> HeldPerVblank);
		void TakeInput();
		std::optional<FError> RunPass(std::span<const FPadState> HeldPerVblank);
		void EndPass();
		void CheckGearButtons();
		/** The pause's frames begin: for Start (the pause), or with no controller connected (the wait for one). */
		void BeginPause(bool bForPad);
		std::optional<FError> AfterPause();
		std::optional<FError> AfterPadWait();
		std::optional<FError> Leave();
		void Stop(const FWorldMapRequest& Request);

		FWorldMapServices Services;
		std::unique_ptr<FWorldMapScene> SceneData;
		std::unique_ptr<FWorldMapContext> MapContext;
		/** The cutscene the scene plays, none for the map. */
		const WorldMapOps::FCutscene* Cutscene = nullptr;
		int32 EntryBlend = 2;
		EPhase Phase = EPhase::NotEntered;
		EWorldMapFrame LastFrameKind = EWorldMapFrame::Tick;
		bool bPassEnded = false;
		FWorldMapRequest PendingRequest;
		/** The members the party's sprites were last made of, by slot, and how they were drawn (FCharacterLooks). */
		std::array<uint8, 3> MembersShown{};
		FCharacterLooks LooksShown;
		/** Id's field sprite, read once a member is drawn as Id, and whether one is. */
		std::optional<FIdsFieldSprite> Id;
		bool bIdShown = false;
		uint32 PartyLoadCount = 0;
		/** The play time the pause sets back: its vertical blanks do not count. */
		uint32 PauseStartPlayTime = 0;
		/** The pause's frames wait for a controller rather than for Start. */
		bool bPauseForPad = false;
		/** The gear lock as the menu found it, which its end puts back. */
		uint8 GearLockBeforeMenu = 0;
	};
}
