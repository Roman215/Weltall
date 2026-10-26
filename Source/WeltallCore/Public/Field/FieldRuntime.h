#pragma once

#include "Field/EventContext.h"
#include "Field/EventScripts.h"
#include "Field/FieldDiscFiles.h"
#include "Field/FieldKernelState.h"
#include "Field/FieldMap.h"
#include "Field/FieldRequests.h"
#include "Foundation/Result.h"
#include "Graphics/GteMatrix.h"
#include "Graphics/SpriteFile.h"
#include "Input/PadState.h"
#include "Text/MesWindow.h"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FFieldSprites;
	class FGameFileIndex;
	class FGameState;
	class FKernelPad;
	class FKernelVibration;
	class FPartySpriteFiles;
	class FPsxRandom;
	class FStoryPacks;
	class IFieldCharacterDrawing;
	struct FFieldTickInput;
	struct FGteArctangents;
	struct FPlayClock;

	/**
	 * What the field takes from the game around it: the disc's files, the game's state, random numbers,
	 * music and sound, the party's sprite files, the console's pad and play clock, and the text system's
	 * font, name tables and button map. Each must outlive the field.
	 */
	struct FFieldServices
	{
		const FCdImage* Image = nullptr;
		const FGameFileIndex* Index = nullptr;
		const FGteArctangents* Arctangents = nullptr;
		FGameState* Game = nullptr;
		FPsxRandom* Random = nullptr;
		IFieldSongs* Songs = nullptr;
		/** None: the scripts' and the actors' sound effects are not heard. */
		IFieldSoundEffects* SoundEffects = nullptr;
		/** None: the banks the scripts swap are not loaded. */
		IFieldSoundBanks* SoundBanks = nullptr;
		/** None: nothing is heard, so nothing pauses with the field. */
		IFieldSoundPause* SoundPause = nullptr;
		FPartySpriteFiles* PartySprites = nullptr;
		FKernelPad* Pad = nullptr;
		FPlayClock* Clock = nullptr;
		/** The controllers' motors, which every vertical blank counts down; none: no motor runs on into the field. */
		FKernelVibration* Vibration = nullptr;
		FMesContext Text;
		FEventOptions Options;
		/** How often the player asked random battles to come. */
		EEncounterRate EncounterRate = EEncounterRate::Original;
		/** The port's own documents, added to the maps they are written for as they load; none: nothing is added. */
		const FStoryPacks* Story = nullptr;
		/** Where the cases no script reaches are noted and carried past; none: the field stops at them. */
		FUnreachedCases* Unreached = nullptr;
		/** The disc the game plays from, numbered as its directory table numbers it (1 or 2): FE CD reads it. */
		uint8 Disc = 1;
		/**
		 * A record of Index whose read the kernel shows a screen of its own before, the field standing still: the French
		 * translation's credits before the picture that ends the game. None on other discs.
		 */
		const FIndexRecord* TranslationCreditsCue = nullptr;
	};

	/**
	 * How the Field program brings its first map in besides the usual entry (the picture of the screen
	 * fading and zooming away over 32 ticks while the map fades in), as the program before it leaves the
	 * kernel's words: after a lost battle, the defeat's song and eight ticks under the picture of the
	 * screen, opaque and unchanged, the map not faded in, and the picture copied where the title's
	 * backdrop reads it; with the battle event's fade, the picture fading on black over frames of its own
	 * before the map's first tick, and the map fading in over the main loop's first ticks. Both may come
	 * together; either sets the picture up opaque.
	 */
	struct FFieldEntry
	{
		bool bAfterDefeat = false;
		bool bBattleEventFade = false;
		/** After a lost battle, the game over takes no buttons at first (DefeatHoldVblanks), where the original was loading. */
		bool bHoldsGameOver = false;
	};

	/** What a frame of the field showed. */
	enum class EFieldFrame : uint8
	{
		/** A tick of the field's logic. */
		Tick,
		/** A frame of a map change's fade before the next map is loaded: the field stands still under its fade layer. */
		MapFade,
		/**
		 * A frame of the pause Start makes, or of the wait for a controller when the first port has none:
		 * nothing moves, the sound pauses, and the pause's picture is shown.
		 */
		Paused,
		/** A frame of the battle event's fade before the map's first tick: the picture of the screen alone, on black. */
		Cover,
		/** A frame of Select's picture of the map (the render state's MapPicture): nothing moves. */
		MapPicture,
		/**
		 * A frame of a movie the field plays inside itself: the wait for its first picture, a pass beside
		 * it (full screen, nothing but the pad's skip; over the field, the first actor's scripts and the
		 * message windows), or its sound's fade after a skip. A pass in the scene is a Tick.
		 */
		Movie,
		/** A frame darkening the field's last picture before the menu, or lightening it after (the render state's MenuShade): nothing moves. */
		MenuShade,
	};

	/**
	 * The field while it plays: the map's data, its actors' sprites and everything its scripts and
	 * systems keep (the context), from a map's load, through the ticks of its entry (the fade-in), in
	 * the field's main loop, and from map to map as the scripts change them. The tick is the
	 * original's, tick for tick, and so are the frames of a map change, a dissolve and the pause; what
	 * the scripts or the player ask of the game around the field (a battle, the world map, the menu, a
	 * movie...) stops the field with a request. Select's picture of the map runs its own frames.
	 */
	class FFieldRuntime
	{
	public:
		/** The ticks a map's entry runs before the field's main loop takes over: its fade-in. Encounters roll only after them. */
		static constexpr int32 EntryTicks = 32;

		WELTALLCORE_API explicit FFieldRuntime(const FFieldServices& InServices);
		WELTALLCORE_API ~FFieldRuntime();
		FFieldRuntime(const FFieldRuntime&) = delete;
		FFieldRuntime& operator=(const FFieldRuntime&) = delete;

		/**
		 * Loads map Number (bits 14 and 15 set: entered in gears) from the game's state, as the field
		 * enters a map: the map's globals reset, its actors placed, its init scripts run, the load's
		 * steps after them (the camera's goal, the models' matrices and draw modes, the party placed
		 * on the leader, the sprites' facing), then the entry's (the song, the frame variables, the
		 * fade-in). An error names what the port cannot load or run yet.
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> EnterMap(int32 Number);

		/**
		 * EnterMap, the field starting from the kernel's globals as another run of the Field program left
		 * them, and entering as Entry says.
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> EnterMap(int32 Number, const FFieldKernelState& Kernel, const FFieldEntry& Entry = {});

		/**
		 * EnterMap as the field starts again after a battle or the arena it saved itself for (the
		 * kernel's trips counted; the game state's place is the map it left): the map's init does not run,
		 * the field comes back to the state it saved in Kernel instead (its actors, sprites, variables,
		 * camera and settings), the party's files read again into fresh buffers, then the first actor's
		 * script 2 runs aside and the gears play their last animations again before the entry (after the
		 * battle event's fade, when Entry has it). An error when Kernel holds no saved state.
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> EnterMapAfterBattle(int32 Number, const FFieldKernelState& Kernel, const FFieldEntry& Entry = {});
		/** The kernel's globals as the field leaves them now, for its next run. */
		WELTALLCORE_API FFieldKernelState KernelState() const;

		/** The fixes the field plays with from now on, the running map included. */
		WELTALLCORE_API void SetOptions(const FEventOptions& Options);
		/** The disc in the drive from now on, 1 or 2, which the scripts read (the running map included). */
		WELTALLCORE_API void SetDisc(uint8 Disc);

		/**
		 * How far the view the player sees is turned from the camera's, from the next frame on (4096 a
		 * turn, as the view's yaws count): the leader's pad directions and the sprites' facing follow it.
		 */
		void SetViewTurn(int16 Turn) { ViewTurn = Turn; }
		/** Whether the leader runs unless the run button is held, from now on (the running map included). */
		void SetAlwaysRun(bool bRuns)
		{
			bAlwaysRun = bRuns;
			FieldContext.bAlwaysRun = bRuns;
		}
		/** Whether the stick walks the leader in any direction and at its pace, from now on (the running map included). */
		void SetAnalogMovement(bool bAnalog)
		{
			bAnalogMovement = bAnalog;
			FieldContext.bAnalogMovement = bAnalog;
		}
		/** Whether doors open when the leader walks into them, from now on (the running map included). */
		void SetWalkIntoDoors(bool bOpens)
		{
			bWalkIntoDoors = bOpens;
			FieldContext.bWalkIntoDoors = bOpens;
		}
		/** How often random battles come, from the next step on (the maps entered later included). */
		void SetEncounterRate(EEncounterRate Rate)
		{
			Services.EncounterRate = Rate;
			FieldContext.EncounterRate = Rate;
		}
		/** How fast messages are typed, from now on (the maps entered later included). */
		void SetTextSpeed(EMesTextSpeed Speed)
		{
			Services.Text.Speed = Speed;
			FieldContext.Messages.Text.Speed = Speed;
		}
		/** The view the player was last shown, which the wide view's test of who is on screen uses from the next frame on. */
		void SetShownView(const FGteMatrix& View) { FieldContext.ShownView = View; }

		/** The vertical blanks the next frame lasts: a tick's two and the scripts' slow motion's, or two for the frames of a fade, the pause or Select's picture. */
		WELTALLCORE_API int32 VblanksPerTick() const;

		/**
		 * The field's next frame after the vertical blanks it lasts (VblanksPerTick of them), with the
		 * buttons held at each: the console's pad and play clock take them, then the field runs its
		 * frame: a tick of its logic, and in the main loop what follows it (a dissolve, the requests,
		 * a map change); or a frame of a map change's fade, the pause or Select's picture. Drawing,
		 * when given, is told of the characters the frame draws. An error while a request is
		 * pending, or naming what stopped the field.
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Tick(std::span<const FPadState> HeldPerVblank, IFieldCharacterDrawing* Drawing = nullptr);

		/**
		 * What the field stopped at, Kind None while it plays on. A Movie request stays while the movie
		 * plays: the field runs its frames beside it (Tick) until it ends.
		 */
		const FFieldRequest& Request() const { return PendingRequest; }

		/**
		 * While the field plays a movie: the frame the movie's player shows once the vertical blanks of the
		 * field's next frame have passed. That frame's scripts still see the frame before; the movie's end
		 * is checked against this one.
		 */
		void SetMovieFrame(int32 Frame) { NextMovieFrame = Frame; }

		/** The player has skipped the movie the field plays: its sound fades through the next frame, which shows no new picture, and it ends. */
		bool IsMovieSkipped() const { return Phase == EPhase::MovieSkipFade; }

		/** Whether the field read the translation's credits' cue (FFieldServices::TranslationCreditsCue) since last asked. */
		bool TakeTranslationCredits() { return DiscFiles.TakeCueRead(); }

		/**
		 * The field goes on after the menu or a movie it stopped at has done its work on the field: its
		 * own steps at the end of it (a movie ends at once, as if it had shown its end frame: the
		 * dissolve back to the field; after the menu those of ResumeAfterMenu, the menu's result 0), then
		 * the rest of the main loop's (the next checks, the music's poll). An error unless one of those
		 * is pending.
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> ResumeAfterRequest();

		/**
		 * The field goes on after the menu it stopped at has closed, with the result the menu leaves
		 * (Load Game's: 0 New Game, 1 the title timed out, 2 a game was loaded into the game state):
		 * what Load Game's end hands the scripts, then the frames lightening the field's picture back
		 * (the next frames: kept dark instead when the scripts asked for it, FE 86); at their end,
		 * after Member Change the party's sprite files read again for the new party and the map
		 * changed; after the others the map's after-menu script, then the gears the menu changed
		 * boarded or left; then the rest of the main loop's steps. An error unless the menu is pending.
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> ResumeAfterMenu(uint8 ModeResult);

		/** What the last frame showed. */
		EFieldFrame LastFrame() const { return LastFrameKind; }

		/** The maps loaded so far (EnterMap and the map changes): each changes the map's data and sprites. */
		int32 MapLoads() const { return Loads; }

		/**
		 * The light matrix the GTE holds: nothing but a lit drawing loads it. A map's load leaves the map's lights
		 * under its reset camera (zeros), each tick the last relit model's, then the last gear part's; the program
		 * after the field is lit by it until it loads its own. LightUnderGears: as the tick's gears were drawn.
		 */
		const FGteMatrix& LightLeft() const { return GteLight; }
		const FGteMatrix& LightUnderGears() const { return GteLightUnderGears; }
		/** Another program's drawing inside the field (the menu's gears) left its own, and with it colours the field does not follow. */
		WELTALLCORE_API void SetLightLeft(const FGteMatrix& Light);
		/**
		 * The GTE's whole lighting beside it: the colour matrix, which a map's load sets to the map's lights' and a
		 * tick's gears to theirs, and the back colour, the map's ambient at each relit model and the gears' at the
		 * gears' load and drawing. Nothing while a drawing the field does not follow left either.
		 */
		WELTALLCORE_API std::optional<FGte> LightingLeft() const;

		FEventContext& Context() { return FieldContext; }
		const FEventContext& Context() const { return FieldContext; }
		const FFieldMap& Map() const { return *MapData; }
		const FFieldSprites& Sprites() const { return *ActorSprites; }
		std::span<const FSpriteFile> SpriteFiles() const { return MapSpriteFiles; }

	private:
		/** Where the field's loop is between frames. */
		enum class EPhase : uint8
		{
			Entry,
			EntryFade,
			MainLoop,
			PadWait,
			Paused,
			MapFadeOut,
			MapEntry,
			DissolveToStill,
			DissolveHold,
			DissolveFromStill,
			MapPictureIn,
			MapPictureHeld,
			MapPictureOut,
			MovieLead,
			MovieLeadAgain,
			MovieFirstPicture,
			MoviePass,
			MovieSkipFade,
			MenuShadeIn,
			MenuShadeOut,
			Stopped,
		};

		/** Where a main loop's iteration goes on once a frame loop inside it is over. */
		enum class EStep : uint8
		{
			Top,
			AfterPadWait,
			AfterPause,
			AfterDissolve,
			AfterMapChange,
			AfterBoarding,
			AfterMapPicture,
			AfterMovie,
			AfterMenu,
		};

		/** Before: the map changes from it; Kernel: the field starts from the kernel's globals; neither: as the console powers on. */
		std::optional<FError> LoadMap(int32 Number, const FEventContext* Before, const FFieldKernelState* Kernel = nullptr, bool bReturning = false);
		/** bReturning: the field comes back to the state it saved (Kernel's) instead of running the map's init. */
		std::optional<FError> Enter(int32 Number, const FFieldKernelState* Kernel, bool bReturning = false, const FFieldEntry& Entry = {});
		/** The entry's steps after the map's load (and after the battle event's fade): the scene's return, the pad, the song, the frame variables, the fade-in. */
		std::optional<FError> BeginEntry();
		std::optional<FError> TickEntry();
		std::optional<FError> TickEntryFade();
		/** The entry's end: the effects' textures back, the gears' poses no longer snapped, the field's main loop from the next frame. */
		void EndEntry();
		std::optional<FError> FinishLoad(int32 Number, bool bReturning = false);
		std::optional<FError> PlaceParty(int32 Number);

		void RunVblanks(std::span<const FPadState> HeldPerVblank);
		FFieldTickInput TakeInput();
		std::optional<FError> RunFrame(int32 StrayWait, bool bPollMusic);
		std::optional<FError> RunIteration(EStep From);
		std::optional<FError> AfterDissolve();
		std::optional<FError> AfterMapChange();
		std::optional<FError> IdleChecks(EStep From);
		std::optional<FError> EndIteration();

		std::optional<FError> TickPause();
		std::optional<FError> TickPadWait();
		/** Every sound frozen where it is, or going on from there: the pause and the wait for a controller. */
		void PauseSound(bool bPaused);
		std::optional<FError> TickMapFade();
		std::optional<FError> TickMapEntry();
		std::optional<FError> TickDissolve();
		void BeginMapPicture(const FFieldMapPicture& Picture);
		std::optional<FError> TickMapPicture();
		std::optional<FError> TickMovieLead();
		std::optional<FError> TickMovie(std::span<const FPadState> HeldPerVblank);
		std::optional<FError> RunMoviePass(std::span<const FPadState> HeldPerVblank);
		/** The movie's pass ends with the pad's skip, the scripts' stop or its end frame: the next frame fades, or the movie ends now. */
		std::optional<FError> CheckMovieEnd();
		std::optional<FError> EndMovieNow();
		bool IsSoftReset();

		void OpenMenu();
		void BeginMenuLightening(uint8 ModeResult);
		std::optional<FError> TickMenuShade();
		void StartMovie();
		std::optional<FError> EndMovie();
		std::optional<FError> CloseMenu();

		bool BeginDissolve();
		bool BeginDissolveFromStill(bool bScreenCopied);
		void EndDissolve();
		std::optional<FError> BeginMapChange(bool& bFramesFollow);
		std::optional<FError> LoadNextMap();
		std::optional<FError> EndMapChange();

		void Stop(const FFieldRequest& Request, EStep ResumeAt);
		std::optional<FError> LeaveField(FFieldRequest Request);

		FFieldServices Services;
		int16 ViewTurn = 0;
		bool bAlwaysRun = false;
		bool bAnalogMovement = false;
		bool bWalkIntoDoors = false;
		FFieldDiscFiles DiscFiles;
		std::unique_ptr<FFieldMap> MapData;
		std::unique_ptr<FEventScripts> Scripts;
		std::vector<FSpriteFile> MapSpriteFiles;
		std::unique_ptr<FFieldSprites> ActorSprites;
		FEventContext FieldContext;
		std::unique_ptr<FFieldDiscGearFiles> DiscGearFiles;
		/** The state the field saved as it last left for a battle or the arena, as the kernel's memory keeps it. */
		std::optional<FFieldSnapshot> SavedForReturn;
		int32 Loads = 0;
		FGteMatrix GteLight;
		FGteMatrix GteLightUnderGears;
		/** The gears' colours loaded, as their drawing's start leaves the GTE. */
		void TakeGearColors();
		/** The gears' ambient loaded while the field counts gears, as their models' load begins: at a map's load and a movie's end. */
		void TakeGearLoadColors();
		FGteMatrix GteColors;
		std::array<int32, 3> GteBackColor{};
		bool bGteColorsKnown = false;
		bool bGteBackColorKnown = false;

		EPhase Phase = EPhase::Entry;
		EFieldFrame LastFrameKind = EFieldFrame::Tick;
		int32 FramesLeft = 0;
		FFieldRequest PendingRequest;
		EStep ResumeStep = EStep::Top;
		/** The play time the pause and the wait for a controller keep: their frames do not count. */
		uint32 PauseStartPlayTime = 0;
		/** How the map being entered comes in, and whether the field came back to the state it saved. */
		FFieldEntry Entry;
		bool bEntryReturning = false;
		/**
		 * After a lost battle the field reads no buttons for these vertical blanks, then none until every button has
		 * been let go: the game over is seen and its song heard before a press leaves it. The original gave as much
		 * by loading the field and its map from the disc, with the battle's last frame on the screen meanwhile; a press
		 * still held from the fight then went to the game over's screen, which leaves on the confirm button held.
		 */
		int32 DefeatHoldVblanks = 0;
		bool bDefeatHeldUntilReleased = false;
		/** The main loop's own: L2 and R2 were held together when last seen, so they board the gears once a press. */
		bool bBoardingButtonsHeld = false;
		/** The transition the map change runs, and the brightness its picture fades with (16.16). */
		int32 Transition = 0;
		int32 CoverBrightness = 0;
		/** Whether each party slot's member rode its gear as the menu opened. */
		std::array<uint8, 3> RidingAtMenu{};
		/** The frame the movie the field plays shows after the next frame's vertical blanks. */
		int32 NextMovieFrame = 0;
		/** The menu the field stops at once its picture has darkened, and the shading's step from 0. */
		FFieldRequest MenuRequested;
		int32 MenuShadeStep = 0;
		/** The observer of the characters' drawing for the frame being run, null outside Tick. */
		IFieldCharacterDrawing* FrameDrawing = nullptr;
	};
}
