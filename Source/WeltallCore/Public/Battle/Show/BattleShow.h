#pragma once

#include "Battle/BattleEndingState.h"
#include "Battle/Show/BattleExtensionDrawing.h"
#include "Battle/Show/BattleModelDrawing.h"
#include "Battle/Show/BattleShowCamera.h"
#include "Battle/Show/BattleShowExtensions.h"
#include "Battle/Show/BattleShowFloor.h"
#include "Battle/Show/BattleShowFrame.h"
#include "Battle/Show/BattleShowMarkers.h"
#include "Battle/Show/BattleShowNumbers.h"
#include "Battle/Show/BattleShowObjects.h"
#include "Battle/Show/BattleShowOptions.h"
#include "Battle/Show/BattleShowPlayback.h"
#include "Battle/Show/BattleShowScreen.h"
#include "Battle/Show/BattleShowScreenWave.h"
#include "Battle/Show/BattleShowSeams.h"
#include "Battle/Show/BattleShowSprites.h"
#include "Battle/Show/BattleShowTasks.h"
#include "Battle/Show/BattleSpriteFiles.h"
#include "Battle/Show/BattleStageBackdrop.h"
#include "Battle/Show/BattleStageTint.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/SpriteSystem.h"
#include "Graphics/Vram.h"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FBattleHudSheet;
	class FBattleSceneGraphics;
	class FIdsBattleSprite;
	class FBattleShowLoad;
	struct FBattleContext;
	struct FBattleStageFiles;
	struct FGteArctangents;

	/** Why the show stopped: something the original does that the port does not model. */
	enum class EBattleShowFault : uint8
	{
		None,
		/** A sprite's script faulted (FSpriteSystem::Fault names it). */
		Sprites,
		/** A file the show reads could not be read or parsed. */
		File,
		/** The scene's graphics name an image set or an entry past the file, or more enemy sprites than the slots. */
		SceneGraphics,
		/**
		 * An animation extension the port does not run: EXT6's screen wave, a command whose module is not the one in memory (its
		 * code is another's), a module read over effects of the one before still running, shards of a model read past its file.
		 */
		Extension,
		/** A script's image list runs past its file. */
		Vram,
		/** The 3D objects stopped (their Fault names what they met). */
		Objects,
		/** A pointer the original keeps to a task that has ended is read (the port keeps only the proxies a gear drop frees). */
		EndedTask,
		/**
		 * The playback met what the original does not survive (a playback begun while one plays, a record's slot or
		 * index past the tables, the routines nested past their room) or a record it does not play yet (FBh).
		 */
		Playback,
		/** An effect started whose file was freed since, or none was loaded: the original parses whatever its pointer reaches. */
		Effect,
		/** A target marker so far ahead that the original divides by zero to size it. */
		MarkerTooFar,
		/**
		 * A sprite's model that runs past its file or holds a kind of primitive no model of the game has; or a lit one drawn
		 * before anything of the battle set the GTE's light, which the original takes from the program before.
		 */
		Model,
		/**
		 * A sound of the battle event's own set (the 3D objects' sound set 3, an event's instruction) without an event's sound
		 * effects in memory: the original reads low memory through the pointer's cleared word.
		 */
		EventSounds,
		/** A battle event's own music (its instructions that start, stop and free it), which no script of the disc runs. */
		EventMusic,
		/** A battle kind past the kernel's table of start sounds: the original plays what the bytes after it hold. */
		StartSounds,
		/** A system sound before the battle's common sounds are read: the original takes the set from the last battle's file. */
		SystemSounds,
		/** The HUD stopped (its own fault names what it met). */
		Hud,
	};

	/** What the show is begun with: the battle the rules set up, the disc's files, and the parts it calls outside itself. */
	struct FBattleShowSetup
	{
		/** The rules' state after the setup; the show reads it and writes what the rules read back. */
		FBattleContext* Context = nullptr;
		const FGteArctangents* Arctangents = nullptr;
		/** The disc's battle sprite files: the party's, the common effects and their images and sounds. */
		const FBattleSpriteFiles* SpriteFiles = nullptr;
		const FBattleStageFiles* Stage = nullptr;
		const FBattleSceneGraphics* SceneGraphics = nullptr;
		/** Id's battle sprite, where the party has Fei drawn as Id; null otherwise. */
		const FIdsBattleSprite* IdsSprite = nullptr;
		const FBattleHudSheet* Sheet = nullptr;
		/** The kernel's pyramid the target markers draw. */
		const FBattleMarkerModel* MarkerModel = nullptr;
		/** The 3D objects' files: the gears' and the special models' archives, the sequence extensions, the tables that name them. */
		const FBattleObjectDisc* ObjectFiles = nullptr;
		/** A battle event's files: its objects, its sound effects, the voices. */
		const FBattleEventFiles* EventFiles = nullptr;
		/** The battle's kind (the kernel's): kind 0 makes the party run in. */
		uint8 Kind = 0;
		/** The battle has an event: the screen stays black for it rather than fading in. */
		bool bEvent = false;
		IBattleShowSound* Sound = nullptr;
		IBattleShowHud* Hud = nullptr;
	};

	/**
	 * The Battle program's show as a whole : the kernel's sprite system in
	 * battle mode with Battle's sprites, the camera, the fades, shake and tints, the numbers, the stage's
	 * floor, the 3D objects and the HUD at their places, run frame by frame in the battle frame's order at
	 * full speed, with the nested frames the show runs inside its own waits. Each frame leaves an
	 * FBattleShowFrame for the renderer.
	 *
	 * A frame is BeginFrame, the rules' input step, then EndFrame, which says whether it returned to the
	 * rules' code (a frame inside one of the show's waits does not). Memory is kept from Begin: a frame
	 * allocates nothing once the battle has started.
	 */
	class FBattleShow final : public IBattleSpriteServices, public IBattleObjectServices, public IBattleStageTinting
	{
	public:
		WELTALLCORE_API FBattleShow();
		WELTALLCORE_API ~FBattleShow() override;
		FBattleShow(const FBattleShow&) = delete;
		FBattleShow& operator=(const FBattleShow&) = delete;

		/**
		 * The show's opening once the rules have set the battle up (the setup's end, the presentation's state,
		 * the opening's fades and framing): the display, the stage's record and floor, the objects made, the
		 * sprite system in battle mode, the setup's loads started as a task (the scene's graphics and enemies,
		 * the party's sprites, the common effects and sounds, the gears, the party's landing), the camera at
		 * the stage's first preset framing the living combatants, the screen black and fading in unless an
		 * event holds it.
		 */
		WELTALLCORE_API void Begin(const FBattleShowSetup& Setup);
		/** The sprite system out of battle mode, every task ended. */
		WELTALLCORE_API void End();

		/**
		 * A frame up to the input step: the camera's step and the view, the stage's frame, the objects, the
		 * sprites' pending frames, the tasks' drawing and their step, the HUD.
		 */
		WELTALLCORE_API void BeginFrame();
		/**
		 * The frame after the input step: its display (DisplayFrame), then its tail (FinishFrame). Whether the
		 * frame returned to the rules' code.
		 */
		WELTALLCORE_API bool EndFrame();
		/** The frame's display: VRAM's queued uploads, the drawing's order, the animation extension asked for. */
		WELTALLCORE_API void DisplayFrame();
		/**
		 * The frame's tail: at depth 1 the playback's step and the objects' idle wait; after a frame of the show's own, its
		 * routine goes on. False while the show still takes frames of its own.
		 */
		WELTALLCORE_API bool FinishFrame();
		/**
		 * A frame's start, before the rules' code: a call the rules made that takes frames (HoldsRules) goes on from its last
		 * frame. True once no such call is under way.
		 */
		WELTALLCORE_API bool ResumeRules();

		/**
		 * The playback's start from the rules' code: its two frames, the controller made, the actor started walking
		 * (unless it stays) toward the first target; the rules' code waits for it (HoldsRules).
		 */
		WELTALLCORE_API void BeginPlayback(bool bStays, uint8 Actor, uint8 Target, uint8 NextReady);
		/** The playback cancelled. */
		WELTALLCORE_API void CancelPlayback();
		/** The markers over the combatants of the mask, the acting member glowing while any shows. */
		WELTALLCORE_API void SetTargetMarkers(uint16 Slots);
		/** From the rules' code at a turn's end: the next ready slot's attack readied; the rules wait (HoldsRules). */
		WELTALLCORE_API void PrepareNext(uint8 Slot);
		/** The actor's flinch and its drains' numbers; the rules wait (HoldsRules). */
		WELTALLCORE_API void ShowTurnDamage(uint8 Slot, const std::array<uint32, 3>& Amounts);
		/** The combatants home in the poses their states give them, with their status marks; the rules wait (HoldsRules). */
		WELTALLCORE_API void RefreshPoses();
		/** A party member's boarding animation, its sprite removed and its gear's model loaded; the rules wait (HoldsRules). */
		WELTALLCORE_API void BoardGear(uint8 Slot);
		/** From the rules' code at the battle's end: the loads finished; the rules wait (HoldsRules). */
		WELTALLCORE_API void FinishLoads();
		/** The battle event's load, once its work is made: its own sound effects registered, the battle's own bank loaded. */
		WELTALLCORE_API void BeginEvent();
		/** A battle event's instruction's show; those the game waits in hold the rules (HoldsRules). */
		WELTALLCORE_API void PlayEventCue(const FBattleEventCue& Cue);
		/** The battle's own bank (44/5) loaded unless it is in memory. */
		WELTALLCORE_API void LoadSystemSoundBank();
		/** The party's end pose, the enemies released, the battle over (HasStarted false); the rules wait (HoldsRules). */
		WELTALLCORE_API void PlayEndPose(EBattleEndPose Pose);
		/** The scene dimmed to a quarter behind the result screen, over 64 steps. */
		WELTALLCORE_API void Dim();
		/** The event's end's fade of the battle's music after a defeat. */
		WELTALLCORE_API void FadeMusic();
		/** A call the rules made still takes frames. */
		bool HoldsRules() const { return Playback.HoldsRules(); }
		/** The camera frames the combatants of the mask. */
		void FrameCamera(uint16 Slots) { FrameSlots(Slots); }
		/** An effect of the battle's common sounds. */
		WELTALLCORE_API void PlayCommonSound(uint8 Effect);
		/** The show meets what it does not play: it stops, the first fault kept. */
		WELTALLCORE_API void Stop(EBattleShowFault Fault);

		/** The setup's loads while they run, a task of the update list; none once the battle has started. */
		WELTALLCORE_API const IKernelTask* LoadTask() const;
		/** The battle has started: the setup's loads are done and the party has landed. */
		bool HasStarted() const { return bStarted; }
		/** 1 for a frame the rules' code runs, 2 for one the show runs inside its own wait. */
		int32 Depth() const { return Playback.Depth(); }
		EBattleShowFault Fault() const { return FirstFault; }
		/** What the show met, in words, where the fault's place gave any. */
		const char* FaultReason() const { return FirstFaultReason; }
		/**
		 * The last frame displayed, whole until the next one is: a frame begun and held before its display (a pause, the
		 * screen wave's blanks) leaves it as it was.
		 */
		const FBattleShowFrame& Frame() const { return Displayed; }
		/** The animation extension module in memory: the one the last resource taken asked for. */
		int32 ExtensionModule() const { return LoadedExtension; }
		/**
		 * EXT6's screen wave ran in the frame begun: its own vertical blanks come before the frame's display, each one
		 * `HoldBlank`, while nothing of the battle steps. The frame then goes on, black but for what it links after the
		 * wave (the HUD).
		 */
		bool IsHolding() const { return WaveHold.IsRunning(); }
		/** One of the screen wave's vertical blanks. */
		WELTALLCORE_API void HoldBlank();
		/** The screen wave, while the frame begun holds for it. */
		const FBattleScreenWave& ScreenWave() const { return WaveHold; }
		/** The steps the frame begun runs beyond its own: the vertical blanks the frame before overran, at most four. */
		int32 FrameCatchUpSteps() const { return CatchUpSteps; }

		// The engines, which the program, the HUD and the tests reach.
		FSpriteSystem Sprites;
		FBattleSprites Battle;
		FBattleCamera Camera;
		FBattleFloor Floor;
		FBattleFadeTask Fade;
		FBattleShakeTask Shake;
		FBattleTintLayers Tints;
		FBattleTintFadeTask TintFade;
		FBattleNumbers Numbers;
		FBattleMarkers Markers;
		FGte Gte;
		/** The fixes of the show's broken effects in use. */
		FBattleShowOptions Options;
		FVram Vram;
		FBattlePlayback Playback;
		FBattleObjects Objects;
		/** The stage's panoramas and floor dome, made with the stage. */
		FBattleStageBackdrop Backdrop;
		/** What the tint layers recolour, kept aside while a tint fade lasts. */
		FBattleStageTint StageTint;
		FBattleExtensions Extensions;

		// IBattleSpriteServices
		bool FilesPending() override { return false; }
		bool ObjectRoot(int32 Slot, std::array<int32, 3>& Translation) override;
		bool BonePlace(int32 Slot, int32 Bone, const FGteVector& Offset, bool bOnFloor, std::array<int32, 3>& Place) override;
		void ReactModel(int32 Slot, uint16 FromSlots, int32 Sequence) override;
		void SetObjectsVisible(bool bVisible) override;
		void LoadSpecialModel(int32 Model) override;
		void RunSpecialModel(int32 Sequence, uint16 Slots) override;
		void FreeSpecialModel() override;
		void RetintModel(const FSprite& Sprite) override;
		void StartExtension(int32 Command, FSprite& Sprite, const FSpriteOperands& Operand, const std::array<int32, 6>& Arguments) override;
		void MoveImage(const FVramRect& Rect, int32 X, int32 Y) override;
		void UploadImages(const FSpriteFile& File, uint32 At) override;
		void RestoreVram() override;
		void PlaySound(uint32 Sound) override;
		void PlaySystemSound(uint32 Sound) override;
		void StopSound(uint32 Sound) override;
		void PlaySoundOn(uint32 Sound, int32 Voice) override;
		void SetMusicVolume(int32 Volume, int32 Steps) override;
		void RegisterSoundEffects(const FBattleSpriteFile& File, const FBattleSpriteSection& Section) override;
		void LoadSoundBank(const FBattleSpriteFile& File, const FBattleSpriteSection& Section) override;

		// IBattleObjectServices
		int32 PlayRecord() override { return Battle.Play.Record; }
		void ShowNumbers() override;
		bool NumbersPending() override { return Numbers.Pending() != 0; }
		void StopCountingTasks() override;
		void SignalEventPerform(int32 Slot) override;
		void CountModelRecord() override { Playback.CountModelRecord(); }
		void BeginEventPerformance(int32 Slot, int32 Target) override;
		void SpawnEffect(const FBattleBoneEffect& Effect) override;
		void ObjectsSwapped(int32 First, int32 Second) override;
		void RequestEffect(int16 Effect) override;
		bool EffectLoaded() override;
		bool CountSpecialModel() override;
		void TakeEffectResource(const FBattleSequenceExtension& Extension) override;
		bool SoundEffectsRegistered(uint16 FileId) override;
		void RegisterSoundEffects(std::span<const uint8> Seds) override;
		void UnregisterSoundEffects(std::span<const uint8> Seds) override;
		uint32 SystemSoundBase() override { return static_cast<uint32>(Battle.SystemSoundSet) << 16; }
		uint32 EventSoundBase() override;
		void SetSoundVolume(uint32 Sound, int32 Volume) override;
		void FadeSound(uint32 Sound, int32 Volume, int32 Steps) override;
		void UploadImage(const FGearTextureBlock& Block, int16 X, int16 Y) override;
		void UploadToPortRows(const FGearTextureBlock& Block, int16 X, int16 Y) override;
		void StoreImage(const FVramRect& Rect, std::span<uint16> Into) override;
		void LoadImage(const FVramRect& Rect, std::span<const uint16> From) override;
		TResult<FBattleModelFiles> ReadGear(int32 Gear, int32 Variant) override;
		TResult<FBattleModelFiles> ReadSpecialModel(int32 Model) override;
		TResult<FBattleSequenceExtension> ReadExtension(int32 File) override;
		TResult<FBattleModelFiles> ReadSceneModel(int32 Scene, int32 Entry) override;
		/** The port's reads end as they are made: none is ever under way to abandon. */
		void AbortReads() override {}
		void CreateProxy(int32 Slot) override;

		// IBattleStageTinting
		void BeginTint() override;
		void EndTint() override;

	private:
		friend class FBattleShowLoad;
		friend class FBattlePlayback;

		/** The catch-up the first frame's objects and stage take from the model enemies, the battle not started. */
		void InitFrameState();
		/** The stage's part of the setup: the objects, the record's clear colour, the floor. */
		void BuildStage();
		/** The clear colour's red, green and blue, which the tint layers recolour. */
		std::span<uint8, 3> ClearColour() { return std::span<uint8, 4>(Battle.Clear).subspan<1, 3>(); }
		/** The tasks' draws: every drawn task in the list's order, filling the frame's 3D list. */
		void RunDraws();
		void DrawSprite(FSpriteTask& Task);
		/** The kernel's sprite drawing: the matrix built as the sprite's own (screen-facing) and its parts linked into entry OtIndex. */
		void DrawSpriteParts(FSpriteTask& Task, int32 OtIndex);
		/** A sprite's entry in the ordering table from its place's depth and bias; 0 when the GTE flags the depth. */
		int32 SpriteEntry(const FSprite& Sprite, const FGteMatrix& View);
		/** The quads the part drawing left in PartScratch, linked for the task's sprite. */
		void AddQuads(const FSpriteTask& Task, EBattleShowDrawer Kind);
		/** The sprite's dot or square at its place, linked into Entry after its drawing mode. */
		void AddTile(const FSpriteTask& Task, bool bDot, int32 Entry);
		/** A streak or a line to the parent: the sprite's line from its place to To through Matrix, linked into Entry before its drawing mode. */
		void AddLine(const FSpriteTask& Task, const FGteMatrix& Matrix, const FGteVector& To, int32 Entry);
		/** A script's trail, a quad for each span of the curve through its sprite's five points. */
		void DrawTrail(const FBattleSprites::FTrailDrawing& Trail);
		/** A number's matrix: turned, at a place on the screen (twice its offset from the centre, H ahead), behind the screen matrix, scaled. */
		FGteMatrix GlyphMatrix(const FGteVector& Rotation, const std::array<int32, 3>& Translation, const std::array<int32, 3>& Scale) const;
		void DrawNumber(const FBattleNumber& Number);
		void DrawBurst(const FBattleTotalBurst& Burst);
		void DrawTotal();
		/** The extensions' drawings of a sprite's parts: EXT2's wave in strips, EXT4's and EXT5's scrolls in copies. */
		void DrawWave(const FBattleWave& Wave);
		void DrawScroll(const FBattleScroll& Scroll);
		/** The kernel's drawings of a model attached to a sprite: the model kind's, or system command 2C's with bOwnRotation. */
		void DrawSpriteModel(FSpriteTask& Task, bool bOwnRotation);
		/** The pyramid through the sprites' view at the marker's place and turn, scaled to one size on the screen. */
		void DrawMarker(const FBattleMarker& Marker);
		/** A number's glyphs (sheet parts) through Matrix, linked into entry 0. */
		void AddGlyphs(uint32 Key, EBattleShowDrawer Kind, const FGteVector& World, const FGteMatrix& Matrix, const std::vector<FBattleSheetPart>& Parts, uint32 Color);
		/** The frame's links sorted into the GPU's drawing order. */
		void BuildOrder();
		void ReportFault(EBattleShowFault Fault, const char* Reason = nullptr);
		/** The objects' fault, once they have stopped, as the show's. */
		void CheckObjects();
		/** The module the last resource asked for is read over the one in memory. */
		void LoadAnimExtension();
		/**
		 * The screen wave (system command 54h): the screen captured over the effects' VRAM and the wave played in frames of
		 * its own, which the show holds for; then the battle's frame after it catches up.
		 */
		void StartScreenWave();
		/** The camera's framing of the slots of the mask, as the camera's own framing does. */
		void FrameSlots(uint32 Slots);
		/** The effects' images uploaded again (44/1); false when the file is missing (a fault). */
		bool UploadCommonImages();
		/** A party character's attack file (directory 45), as the playback reads it. */
		TResult<FBattleSpriteFile> ReadAttacks(uint8 Character) const;
		/** The file's sound effects unregistered. */
		void UnregisterSoundEffects(const FBattleSpriteFile& File);
		/** The battle's own bank unloaded; the enemies' bank (44/7) loaded in its place. */
		void UnloadSystemSoundBank();
		void LoadEnemySoundBank();
		/** The end pose's load of the battle's own bank (44/5) as the kernel's gear battle bank, whatever the driver holds. */
		void ReloadSystemSoundBank();

		FBattleContext* Context = nullptr;
		const FGteArctangents* Arctangents = nullptr;
		const FBattleStageFiles* Stage = nullptr;
		const FBattleSpriteFiles* SpriteFiles = nullptr;
		const FBattleMarkerModel* MarkerModel = nullptr;
		const FBattleObjectDisc* ObjectFiles = nullptr;
		const FBattleEventFiles* EventFiles = nullptr;
		IBattleShowSound* Audio = nullptr;
		IBattleShowHud* Hud = nullptr;
		/** The setup's loads, a task of the update list until the battle has started. */
		std::unique_ptr<FBattleShowLoad> Load;
		/** The objects' and the stage's steps count 1 + this: the model enemies' count for the first frame, 0 after. */
		int32 CatchUp = 0;
		/**
		 * The camera's and the tasks' further steps in a frame: the vertical blanks the frame before overran, at
		 * most four, which only EXT6's screen wave makes at full speed; the catch-up it leaves for the frame after its hold.
		 */
		int32 CatchUpSteps = 0;
		int32 PendingCatchUp = 0;
		/** The screen wave the frame begun holds for, before its display. */
		FBattleScreenWave WaveHold;
		/**
		 * The screen wave ran in the frame begun: what the frame linked before it is gone with the ordering tables the
		 * wave's frames cleared, and a black screen lies behind what it links after.
		 */
		bool bCoveredByWave = false;
		/** The animation extension module in memory. */
		int32 LoadedExtension = 0;
		bool bStarted = false;
		EBattleShowFault FirstFault = EBattleShowFault::None;
		const char* FirstFaultReason = nullptr;
		/** The frame being built, and the last one displayed with its objects' part; a display exchanges them. */
		FBattleShowFrame Shown;
		FBattleShowFrame Displayed;
		FBattleObjectsFrame DisplayedObjects;
		/** The frames begun since Begin. */
		uint32 FrameCount = 0;
		/** Where the stage's own links end among the objects' in the frame begun: the backdrop is linked there. */
		std::size_t StageLinkCount = 0;
		/** The frame's links in the order they were linked, which BuildOrder sorts into Shown.Order. */
		std::vector<FBattleShowLink> Links;
		std::vector<FSpriteQuad> PartScratch;
		std::vector<FBattleSheetPart> SheetScratch;
		/** The extensions' drawings of sprites' parts, into PartScratch. */
		FBattleExtensionDrawing ExtensionDrawing;
		/** The sprites' models' drawing into the frame begun, and the models it has drawn. */
		FBattleModelDrawing ModelDrawing;
		std::vector<FBattleDrawnModel> DrawnModels;
		FVramImage MoveScratch;
		/** The objects' uploads, one at a time. */
		FVramImage UploadScratch;
		/** The sprite resources sequence op 05 took from the extensions, kept for the battle: the effect's resource draws from them. */
		struct FExtensionSprite
		{
			std::unique_ptr<FSpriteFile> Sprite;
			const FBattleSequenceExtension* Source = nullptr;
		};
		std::vector<FExtensionSprite> ExtensionSprites;

	public:
		/** The extension a sprite resource op 05 took lies in, none for another file. */
		WELTALLCORE_API const FBattleSequenceExtension* ExtensionOf(const FSpriteFile* File) const;
	};
}
