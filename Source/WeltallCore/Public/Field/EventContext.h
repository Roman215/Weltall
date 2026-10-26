#pragma once

#include "Field/EventActorState.h"
#include "Field/EventScripts.h"
#include "Field/EventVariables.h"
#include "Field/FieldActor.h"
#include "Field/FieldActorSounds.h"
#include "Field/FieldCamera.h"
#include "Field/FieldControls.h"
#include "Field/FieldGears.h"
#include "Field/FieldMessages.h"
#include "Field/FieldMovement.h"
#include "Field/FieldParticles.h"
#include "Field/FieldRenderState.h"
#include "Field/FieldRequests.h"
#include "Field/FieldZone.h"
#include "Formats/Tim.h"
#include "Foundation/Types.h"
#include "Foundation/UnreachedCases.h"
#include "Game/GameBalance.h"
#include "Graphics/MdlModel.h"
#include "Timing/PlayClock.h"

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FGameState;
	class FPartySpriteFiles;
	class FPsxRandom;
	class FSprite;
	class FSpriteSystem;
	struct FFieldWalkmesh;
	struct FGteArctangents;

	/** Where an actor's sprite comes from: one of the map's sprite files, or a party member's. */
	enum class EFieldSpriteKind : uint8
	{
		Map = 0,
		/** The member of a party slot, textured in the party's own places in VRAM. */
		Party = 1,
		/** A second copy of a party slot's member, in the places beside the first's. */
		PartyCopy = 2,
	};

	/** An actor's new sprite, as the event scripts ask for one. */
	struct FFieldSpriteRequest
	{
		int32 Actor = 0;
		EFieldSpriteKind Kind = EFieldSpriteKind::Map;
		/** The map's sprite file, or the party slot. */
		int32 Slot = 0;
		int32 PaletteSet = 0;
		/** Made without running its animation's first step. */
		bool bDeferred = false;
		/** The actor's position, 16.16. */
		std::array<int32, 3> Position{};
		/** The height the actor's record held, which the sprite stands on until the record follows the actor. */
		int16 RecordHeight = 0;
		/** The map's fog is on: the sprite is drawn through it. */
		bool bFog = false;
		/**
		 * Made as the field comes back from a battle: the sprite keeps the floor and gravity it is made
		 * with (a fresh kernel sprite's) through its first step, instead of the actor's record's height and
		 * the field's gravity.
		 */
		bool bReturning = false;
	};

	/** What the scripts' bookkeeping takes from a new sprite. */
	struct FFieldSpriteResult
	{
		/** The height of its first frame above its origin, at the sprite's first scale. */
		int32 FrameTop = 0;
		/** Its animation's first step signalled that it has no animations. */
		bool bNoAnimations = false;
	};

	/** The actors' sprites, as the event scripts have them made. */
	class IFieldActorModels
	{
	public:
		virtual ~IFieldActorModels() = default;

		/** Gives the actor a new sprite in place of any it had; nothing when the request names no sprite file. */
		virtual std::optional<FFieldSpriteResult> LoadSprite(const FFieldSpriteRequest& Request) = 0;
		/** The actor's sprite, which scripts move with the actor; null when it has none. */
		virtual FSprite* ActorSprite(int32 Actor) = 0;
		/** The sprite system the actors' sprites run in, for the scripts' changes to them; null without one. */
		virtual FSpriteSystem* SpriteSystem() = 0;
		/** Exchanges two actors' sprites, as a pilot and its gear hand theirs to each other. */
		virtual void SwapSprites(int32 First, int32 Second) = 0;
		/**
		 * Reads an extra animation set (a sprite file of directory 4) for the actor, in place of the one
		 * it read before, which stays as it was for a sprite that still plays it; false when there is none.
		 */
		virtual bool LoadExtraAnimations(int32 Actor, int32 File) = 0;
		/** The actor's sprite plays the extra animation set the actor read last as its negative animations. */
		virtual void UseExtraAnimations(int32 Actor) = 0;
	};

	/** The field's songs, as the scripts ask for them. */
	class IFieldSongs
	{
	public:
		/** The song a map entered after a lost battle plays. */
		static constexpr int32 DefeatSong = 0x0E;

		virtual ~IFieldSongs() = default;

		/**
		 * A request for Song sets how songs start, silent (for the scripts to fade in) or at full volume,
		 * before it waits or loads: one that waits changes how a song still loading starts.
		 */
		virtual void SetSongStart(int32 Song, bool bSilent) = 0;
		/** A map's init chooses the song the map starts with. */
		virtual void ChooseSong(int32 Song) = 0;
		/** Whether a song asked for is not loaded yet: the scripts wait for it. */
		virtual bool IsLoading() const = 0;
		/** A running map's scripts change the song; false when it could not be loaded. */
		virtual bool ChangeSong(int32 Song) = 0;
		/**
		 * A map's entry, after its init: the song the init chose starts loading unless it is the one
		 * playing; after a lost battle the defeat's song, the song a battle kept given up.
		 */
		virtual void StartMapSong(bool bAfterDefeat) = 0;
		/** The field's poll at the end of every tick: a song still loading finishes its load; an error when it cannot. */
		virtual std::optional<FError> Poll() = 0;
		/** The song asked for last (0xFF none), which the game's state keeps for play to resume with. */
		virtual int32 ChosenSong() const = 0;
		/** Whether a song's load has finished and it plays: the scripts' changes to it wait for that. */
		virtual bool IsMusicLoaded() const = 0;
		/** The playing song's volume (0-127), transposition, tempo (0x100 normal) and pan offset, each reached over Interrupts of the sound driver. */
		virtual void SetMusicVolume(int32 Volume, int32 Interrupts) = 0;
		virtual void SetMusicPitch(int32 Semitones, int32 Interrupts) = 0;
		virtual void SetMusicTempo(int32 Scale, int32 Interrupts) = 0;
		virtual void SetMusicPan(int32 Offset, int32 Interrupts) = 0;
		/** The playing song's tracks with their bit set go silent. */
		virtual void MuteMusicTracks(uint32 Mask) = 0;
	};

	/**
	 * The sound effects the scripts and the actors' positional sounds play, on the sound driver's fixed
	 * effect channels (0, 2, ... 14): what the channel plays stops, then the effect starts at a volume
	 * and pan (0-127, the pan's centre 0x40); a playing effect's volume and pan change, or its volume
	 * slides over Ticks of the effect's own.
	 */
	class IFieldSoundEffects
	{
	public:
		virtual ~IFieldSoundEffects() = default;

		virtual void StopEffectOn(int32 Channel) = 0;
		virtual void PlayEffectOn(uint32 Effect, int32 Channel, int32 Volume, int32 Pan) = 0;
		virtual void SetEffectVolumeOn(int32 Channel, int32 Volume) = 0;
		virtual void SetEffectPanOn(int32 Channel, int32 Pan) = 0;
		virtual void FadeEffectVolumeOn(int32 Channel, int32 Volume, int32 Ticks) = 0;
		/** Bit n set: the effect player's track n plays (channel c's effect takes tracks (c ^ 8) and the next). */
		virtual uint32 PlayingEffects() = 0;
	};

	/**
	 * The sound bank the scripts swap into the gear battles' bank's place in sound RAM: the resident
	 * bank given up and another read (as FE B0 starts), then uploaded (as it finishes).
	 */
	class IFieldSoundBanks
	{
	public:
		virtual ~IFieldSoundBanks() = default;

		/** Unloads the gear battles' bank and reads the bank file File of Directory; false when it cannot be read. */
		virtual bool BeginBankLoad(int32 Directory, int32 File) = 0;
		/** Uploads the bank read last in the gear battles' bank's place; false when none was read or it does not fit. */
		virtual bool FinishBankLoad() = 0;
	};

	/** The field's pause of the sound: every voice frozen where it is, until it goes on from there. */
	class IFieldSoundPause
	{
	public:
		virtual ~IFieldSoundPause() = default;

		virtual void PauseSound() = 0;
		virtual void ResumeSound() = 0;
	};

	/** The disc's files the scripts read: the port reads them at once, where the original waits for the drive. */
	class IFieldFiles
	{
	public:
		virtual ~IFieldFiles() = default;

		/** File of Directory, whole; false when it cannot be read. */
		virtual bool ReadFile(int32 Directory, int32 File, std::vector<uint8>& Bytes) = 0;
	};

	/**
	 * The field's random encounters as the scripts set them: within each period of ticks, the ticks
	 * whose step starts a battle.
	 */
	struct FFieldEncounters
	{
		static constexpr int32 MaxPerPeriod = 32;
		static constexpr uint16 NoEncounter = 0xFFFF;
		static constexpr int32 FormationCount = 16;
		/** The song a random encounter's battle plays: the field's table of them only ever holds it. */
		static constexpr uint16 BattleSong = 0x1D;

		/** Ticks in a period; 0 turns encounters off. */
		int32 Period = 0;
		int32 PerPeriod = 0;
		/** Ticks left in the period. */
		int32 Countdown = 0;
		/** Ticks until each of the period's encounters. */
		std::array<uint16, MaxPerPeriod> Pending{};
		/** How likely each of the place's formations is; a map without encounter data keeps the previous place's. */
		std::array<uint8, FormationCount> Weights{};
		/** The formation the last encounter drew, which the battle is set up from. */
		uint8 Formation = 0;
		/** An encounter's battle is on its way: the field starts it once the leader has landed. */
		bool bBattleDrawn = false;

		/**
		 * Draws the period's encounter ticks, all different, with the game's rand(), over the period as Rate makes it
		 * (never too short to hold them, nor past the ticks the schedule can count). False when they cannot all be told
		 * apart, where the original never finishes drawing.
		 */
		WELTALLCORE_API bool Schedule(FPsxRandom& Random, EEncounterRate Rate);
	};

	/** Why the scripts could not go on: an instruction the port does not run, or one that reads outside the map's data. */
	enum class EEventFault : uint8
	{
		None,
		UnimplementedOpcode,
		PcOutsideCode,
		ActorWithoutScripts,
		VariableOutOfRange,
		ZoneOutOfRange,
		ReadOutsideCode,
		NoFloorTriangle,
		SquareRootOutsideTable,
		TableIndexOutOfRange,
		EncountersNeverScheduled,
		NoSongs,
		NoGameState,
		NoActorModels,
		NoSpriteFile,
		/** The gears' module stopped (the field's gears hold why), or a gear actor has no model loaded. */
		GearModule,
		/** A fifth gear registered, or a gear slot past four, which the field's four-gear arrays do not hold. */
		GearArraysFull,
		/** A point on the joint of a gear slot past the gears' module's slots, whose table the original reads past. */
		GearJointUnset,
		/** A platform has no model to stand on, or is attached to an actor the map does not have. */
		PlatformModel,
		/** A message window placed by no band and no script, where the original takes a stale stack word. */
		WindowPlaceUndefined,
		NoPartySprites,
		PartySpriteFileUnreadable,
		UnimplementedAfterInit,
		ActorWithoutSprite,
		/** A VRAM scroller without bands or width, or with more bands than the original can allocate. */
		UnusableLineScroll,
		/** An image to read, and none can be (no files attached, or the file is not a TIM). */
		ImageUnreadable,
		/** An image's upload with none read, or CLUT rows faded with none kept aside, where the original reads memory it gave up or never had. */
		ImageNotLoaded,
		/** The screen markers used before they are made or after they are given up, a marker past the 33, or one placed twice in a tick (the original's ordering table then loops). */
		ScreenMarker,
		/** A texture scroll of an actor whose model is not all flat-textured polygons, which the original's strides assume. */
		ModelUvScroll,
		/** An actor placed on a walkmesh layer the map does not have, whose floor the original takes from stale stack. */
		FloorFromStaleStack,
		/** A sound bank slot other than the gear battles' bank's, where the original unloads no bank or one behind the music's back. */
		SoundBankSlot,
		SoundBankUnreadable,
		/** A bank's upload with no bank read, or one that does not fit in sound RAM. */
		SoundBankNotLoaded,
	};

	/**
	 * The field's clock, variable 0x00A (minutes << 8 | seconds), as scripts set and start it: it
	 * steps once every 31 ticks, up or down, and counts 61 seconds to a minute when it counts up.
	 * It keeps running from map to map.
	 */
	struct FFieldClock
	{
		static constexpr uint32 Address = 0x00A;
		static constexpr uint8 Stopped = 0x80;
		static constexpr uint8 CountsDown = 0x4;

		/** Ticks since the last step. */
		int32 Ticks = 0;
		uint8 Mode = 0xFF;

		/** One field tick, as the field's per-tick update advances the clock. */
		WELTALLCORE_API void Tick(FEventVariables& Variables, const FEventScripts& Scripts);
	};

	/** Fixes of the original scripts' broken effects: each is on unless the original's behavior is asked for. */
	struct FEventOptions
	{
		/** 7C, which restores the party's HP, restores their EP in the original, as 7E does. */
		bool bRecoverPartyHpGivesHp = true;
		/**
		 * F1, B3 and B4 with no frames reach their colour on the next tick; in the original F1's layer
		 * keeps the colour it has, B4 shows nothing and B3 leaves the screen black.
		 */
		bool bZeroFrameFadeSnaps = true;
		/**
		 * A fade up from black runs while its colour still rounds to black; in the original one moving
		 * less than a unit a frame is switched off on its first (FFieldRenderState::TickFades).
		 */
		bool bFadesRiseFromBlack = true;
		/**
		 * The gears' channel pool grows when full, so every bone animates; in the original a map's gears
		 * can ask for more channels than its pool holds, and the bones left without one stand still.
		 */
		bool bGearChannelPoolGrows = true;
		/**
		 * 12 waits whole while a movie is asked for; in the original its map change waits, but it takes
		 * its transition from its own first bytes and stops inside itself.
		 */
		bool bChangeMapWaitsWhole = true;
		/**
		 * A VRAM scroller's bands keep their offsets within the scroller's width, so they turn smoothly
		 * past their start; in the original a width that does not divide 65536 texels jumps there (map
		 * 147's, by 16 texels).
		 */
		bool bScrollsWrapWithinWidth = true;
		/**
		 * B5 in a map's init turns the camera whole, its turning yaw too, so the first turn after starts
		 * from the new direction; in the original it sweeps from the old one for seven ticks (maps 23,
		 * 24 and 154).
		 */
		bool bInitTurnSetsTurningYaw = true;
		/**
		 * F2 ramps the shake's depth from its own amplitude; the original ramps it from the height's, so
		 * the depth ends early or late wherever the two differed (280 of the game's shakes).
		 */
		bool bShakeDepthFromItsOwn = true;
		/**
		 * An actor on a ladder whose sprite has no ladder animation (animation 6) keeps its standing or
		 * walking one; the original plays the words past its animation table as one (map 592's guide).
		 */
		bool bLadderOnlyWithItsAnimation = true;
		/**
		 * The Bledavik hotel (map 104) counts the party by its three slots; its script tests slot 1
		 * twice in the original, so a party of two pays for three (applied as the map loads).
		 */
		bool bHotelCountsWholeParty = true;
		/**
		 * On Ramsus's ship (map 272) the scripts set the volume of the hum and the sound they start beside it, as
		 * they mean to: their FE 62s have the volume and the channel swapped, so the original changes nothing and
		 * both play at full volume (applied as the map loads).
		 */
		bool bShipSoundsAtTheirVolume = true;
		/**
		 * In Lacan's memories (map 670) the room's sound fades out under the monologue, as the scene that leaves for
		 * map 674 has it fade; the original fades a channel that holds nothing (applied as the map loads).
		 */
		bool bMemoryAmbienceFades = true;
		/**
		 * A character counts as on screen anywhere in the widest view the port shows (32:9) through the
		 * view the player is shown, so it walks and animates wherever it can be seen; the original
		 * freezes one off its 4:3 screen, even one a wide screen shows at its sides.
		 */
		bool bActorsRunInWideView = true;
		/**
		 * A sprite made from a party slot takes the file loaded into the slot after it whole (its tables,
		 * palette and animation); the original reads the new file through the old one's tables, so it can
		 * show pieces of another character's or gear's file.
		 */
		bool bPartySpritesFollowTheirFile = true;
		/**
		 * In the house in Lahan (map 7), once three rounds of the man's rock-paper-scissors are lost, his wife, talked
		 * to, offers to have a word with him, after which he loses every round of the visit; the original has no offer
		 * (the story's document under `fix.rps_helper`, StoryPacks.h; added as the map loads).
		 */
		bool bRpsHelper = true;
		/**
		 * A scene staged before a picture (a flat 4:3 model behind its people and gears: FieldScenePictures.h) is drawn
		 * with the picture stretched across a wide view as far as 21:9 and the scene moving sideways with it, cut at 21:9
		 * on a wider one; the original has nothing beyond the picture's edges (marked as the map's scenery is built).
		 */
		bool bScenePictures = true;
		/**
		 * Where a scene leaves a character waiting just off the 4:3 screen, out of the original's shot until the next
		 * shot places it, it waits out of every view of the scene instead, so a wide screen does not show it standing
		 * at its side (map 50's ride into the night: the story's document under `fix.wide_view_staging`, StoryPacks.h).
		 */
		bool bWideViewStaging = true;
		/**
		 * Bart, swimming Aveh's waterway (maps 130 and 147), takes the stroke the run button asks for as he goes back into
		 * the water from a floor where he stands out of it, which gives him the walking speed; the original keeps the
		 * stroke it last chose, so a run held across one leaves him at the slow one until it is pressed again (the story's
		 * documents under `fix.swim_stroke`).
		 */
		bool bSwimStrokeAfterWading = true;
		/**
		 * The moon over the Yggdrasil's deck at night (maps 278 and 283) has its glow fade out within its picture; the
		 * original's glow, painted wider than the picture, ends at its edges in a navy square (FieldMoonGlow.h; done as
		 * the map's textures are uploaded).
		 */
		bool bMoonGlowFades = true;
		/**
		 * The pieces a streamed sprite's frame mirrors left to right are drawn where the frame was made to put them, in the
		 * field, on the world map and in battle (FSpriteSystem::bMirroredPartsPlacedByArt): Fei's arm on his shoulder as he
		 * runs away from the view, Billy's lock of hair on his head as he shakes it. The original draws each up to three
		 * texels to its right.
		 */
		bool bSpriteFramesAligned = true;

		/** Every fix off: the original's behavior throughout. */
		static constexpr FEventOptions Original()
		{
			FEventOptions Options;
			Options.bRecoverPartyHpGivesHp = false;
			Options.bZeroFrameFadeSnaps = false;
			Options.bFadesRiseFromBlack = false;
			Options.bGearChannelPoolGrows = false;
			Options.bChangeMapWaitsWhole = false;
			Options.bScrollsWrapWithinWidth = false;
			Options.bInitTurnSetsTurningYaw = false;
			Options.bShakeDepthFromItsOwn = false;
			Options.bLadderOnlyWithItsAnimation = false;
			Options.bHotelCountsWholeParty = false;
			Options.bShipSoundsAtTheirVolume = false;
			Options.bMemoryAmbienceFades = false;
			Options.bActorsRunInWideView = false;
			Options.bPartySpritesFollowTheirFile = false;
			Options.bRpsHelper = false;
			Options.bScenePictures = false;
			Options.bWideViewStaging = false;
			Options.bSwimStrokeAfterWading = false;
			Options.bMoonGlowFades = false;
			Options.bSpriteFramesAligned = false;
			return Options;
		}
	};

	/**
	 * The GTE as the scripts find it, which their projections of actors go through: the view the
	 * camera's last step built (the scripts run before the tick's camera), and the projection
	 * distance it was last given (the tick's drawing gives it the camera's, CameraSet at once).
	 */
	struct FFieldScriptView
	{
		FGteMatrix View = FGteMatrix::Identity();
		uint16 ProjectionDistance = 0x200;
	};

	/** A new party member's sprite file on its way into its slot's buffer. */
	struct FPartyMemberLoad
	{
		bool bPending = false;
		uint8 Character = 0;
		uint8 Slot = 0;
		/** The file its slot names, as the party's sprite files number them. */
		int32 File = 0;
	};

	/** The machine's registers while one actor's thread runs. */
	struct FEventRun
	{
		int32 Actor = 0;
		FEventActorState* State = nullptr;
		/** The actor's turn ends after this instruction, unless the run is unlimited. */
		bool bYield = false;
		/** Instructions run without a budget until one ends the thread or ends the unlimited run. */
		bool bUnlimited = false;
		/** Instructions the run may still execute; some instructions grant more. */
		int32 Budget = 0;
		EEventFault Fault = EEventFault::None;
		/** What the fault concerns: the opcode, the actor, the address... */
		uint32 FaultValue = 0;
	};

	/**
	 * Everything the event scripts read and change: the map's scripts and data, the variables, the
	 * actors with their script states, the party, the input, the game's random numbers, and what the
	 * scripts ask the field to do. The event machine keeps nothing of its own.
	 */
	struct FEventContext
	{
		const FEventScripts* Scripts = nullptr;
		/** The map's number (bits 14-15 set when it was entered in gears). */
		int32 MapNumber = 0;
		std::span<const FFieldZone> Zones;
		/** The map's walkmesh, whose triangles' attributes the scripts change. */
		FFieldWalkmesh* Walkmesh = nullptr;
		/** The map's models, whose polygons platforms carry actors on. */
		std::span<const FMdlModel> MapModels;
		const FGteArctangents* Arctangents = nullptr;
		FPsxRandom* Random = nullptr;
		IFieldActorModels* Models = nullptr;
		IFieldSongs* Songs = nullptr;
		/** Where the scripts' sound effects go; none, they are not heard and none plays for the scripts that wait on them. */
		IFieldSoundEffects* SoundEffects = nullptr;
		// The sound banks the scripts swap and the sounds actors give off where they stand.
		/** Where the scripts' sound banks load; none, they are read and uploaded nowhere (nothing the field does depends on them). */
		IFieldSoundBanks* SoundBanks = nullptr;
		FFieldActorSounds ActorSounds;
		/** The game's state: gold, items, characters. */
		FGameState* GameState = nullptr;
		FEventOptions Options;
		/** How often the player asked random battles to come: none stops the leader's steps counting toward them. */
		EEncounterRate EncounterRate = EEncounterRate::Original;
		/** Where the cases no script reaches are noted and carried past; none: the scripts stop at them. */
		FUnreachedCases* Unreached = nullptr;
		/** The disc the game plays from (1 or 2). */
		uint8 Disc = 1;

		FEventVariables Variables;
		/** Every actor of the map; the first Scripts->ActorCount() have script states. */
		std::vector<FFieldActor> Actors;
		std::vector<FEventActorState> States;

		static constexpr uint8 NoActor = 0xFF;
		/** The actor that stands for each party slot, NoActor for an empty slot. */
		std::array<uint8, 3> PartyActors{NoActor, NoActor, NoActor};
		/** The character in each party slot, NoActor for none. */
		std::array<uint8, 3> PartyCharacters{NoActor, NoActor, NoActor};
		/** The actor the player controls. */
		uint8 LeaderActor = 0;
		/** The camera as the scripts set it: its direction, the actor it follows. */
		FFieldCamera Camera;
		FFieldEncounters Encounters;
		/** A battle is requested (by an encounter or a script) and has not started: songs and the player's release wait. */
		bool bBattleRequested = false;
		/** 0xFFFF while the scripts keep the party from the player, 0 while the player controls it. */
		uint16 PlayerLock = 0;
		bool bMenuLocked = false;
		/** What the Normal Menu the player opens from the field is given: nonzero, it offers saving (FE 99). */
		uint8 MenuParam = 0;
		static constexpr uint32 NoMenu = 0xFF;
		/** The menu the field opens at its next quiet tick, NoMenu for none: the leader cannot jump meanwhile. */
		uint32 MenuRequest = NoMenu;
		bool bCompassHidden = false;
		/** How the map is drawn: the clear colour, culling, fog, backdrop, lights and fades the scripts set. */
		FFieldRenderState Render;
		/** The particle effects the scripts set up and start. */
		FFieldParticles Particles;
		/** How battles start: a random encounter's and a scripted battle's style. */
		uint8 EncounterBattleKind = 0;
		uint8 ScriptedBattleKind = 5;
		/** Whether the gears can be boarded or left on this map, 0xFF: as the map's gear mode says. */
		uint16 GearBoardingOverride = 0xFF;
		/**
		 * The kernel's word that the gears can be neither boarded nor left, as the field last worked it out (at the
		 * boarding's buttons and as the menu opens) or the program before it left it: the menu and the battle read it.
		 */
		bool bGearsLocked = false;
		/**
		 * The kernel's byte the arena leaves its last match's result in, which the scripts read (FE C0): 0 a match
		 * given up, 1 to 3 a win by its grade, 81h to 88h a loss by its reason, 7Fh the scripted bout's end.
		 */
		uint8 BattlingResult = 0;
		/** How fast the gears' models turn to face their direction, per tick (4096 a turn). */
		uint16 GearTurnSpeed = 0x80;
		/**
		 * The field's buttons this tick, and every button held since a script last cleared them (the
		 * field's per-tick update adds the held ones).
		 */
		uint16 PadHeld = 0;
		uint16 PadLatched = 0;
		/**
		 * How far the view the player sees is turned from the camera's (4096 a turn, the way the view's
		 * yaws count; 0 in the original): the pad's directions and the sprites' facing follow the view shown.
		 */
		int16 ViewTurn = 0;
		/** The leader runs unless the run button is held, which then walks (off in the original: it runs while the button is held). */
		bool bAlwaysRun = false;
		/**
		 * The stick walks the leader in any direction, and a light push walks it slowly; with Always run a firm push runs (off
		 * in the original: the d-pad's eight directions).
		 */
		bool bAnalogMovement = false;
		/** A door's talk script starts when the leader walks into the door (off in the original: only for the button). */
		bool bWalkIntoDoors = false;
		/**
		 * The view the player was last shown (the game's camera moved by the player's offsets), which
		 * the wide view's test of who is on screen projects through; none: the game's own view.
		 */
		std::optional<FGteMatrix> ShownView;

		/** Clear while the map's init scripts run. */
		bool bInitDone = false;
		/** Set once the map's entry is over (its load, init and fade-in): only then do the leader's steps roll encounters. */
		bool bFieldRunning = false;
		/**
		 * The field comes back from a battle, from its load to the entry's first steps: the actors'
		 * sprites are made without setting the actors' heights or starting the sprites' floor and gravity.
		 */
		bool bReturning = false;
		/** How the field shows the movie it plays: over the field, only the first actor's scripts run. */
		EFieldMovieView MovieView = EFieldMovieView::FullScreen;
		/** The party walks as a group: the party's actors run no scripts. */
		bool bPartyWalking = false;
		/** Counts the models and sprites created for actors, which tells the map's init whether an actor got one. */
		int32 ModelsCreated = 0;
		/** The gears the scripts register and swap, and the gears' module that runs them once the map's load has loaded them. */
		FFieldGears Gears;
		/** Set by the player's control step; cleared by every tick's events. */
		bool bPlayerControlled = false;
		/** bPlayerControlled as the last tick's events left it: what the scripts that run before the leader's step go by. */
		bool bPlayerHadControl = false;
		/** Set when the controlled actor's landing empties the pad's queue: input queued before is dropped. */
		bool bPadInputDropped = false;
		/** Message windows opened this tick; only one opens per tick. */
		int32 MessagesOpened = 0;
		/** The pad's words beyond PadHeld, the leader's jumps and the party's trail. */
		FFieldControls Controls;
		/**
		 * The message windows the scripts open, the GTE's view their speakers are placed through, and
		 * the field's slow motion (D6): the vblanks each tick lasts beyond its two.
		 */
		FFieldMessages Messages;
		FFieldScriptView ScriptView;
		int32 FrameDelay = 0;
		FEventRequests Requests;
		FFieldClock Clock;
		/** What the actors' movement keeps for the whole map. */
		FFieldMovement Movement;

		// The scripts' images (FE 77, FE B1): where they are read from, and the one FE 77 read last.
		/** None: an image the scripts read faults. */
		IFieldFiles* Files = nullptr;
		/** Until it is given up, with the places its uploads moved its rectangles to (the original moves them in the file's own header). */
		std::optional<FTim> ScriptImage;

		// What the camera's tick and the actors' turning read of the field's own state.
		/**
		 * The map's first ticks still to run, MapStartSettleTicks at a map's load and one less after
		 * each tick: nothing is drawn, the pad reads nothing, the actors turn to their facing at once
		 * and each frame is drawn with its own tick's view.
		 */
		static constexpr int32 MapStartSettleTicks = 4;
		int32 SettleTicks = 0;
		/** The member change menu reloaded the party's sprites for a map change: the actors' sprites are neither turned nor drawn. */
		bool bActorSpritesReleased = false;

		// The party's sprite files and gears.
		/** The party's sprite files, which the kernel keeps from map to map; the party's sprites read them. */
		FPartySpriteFiles* PartySprites = nullptr;
		/** Set while the party is in gears (FE 1E): added to a character's pilot sprite to pick the map's sprite file. */
		uint8 PilotSpriteBase = 0;
		/** The actor that is each party slot's gear, NoActor for none. */
		std::array<uint8, 3> GearActors{NoActor, NoActor, NoActor};
		/** A member whose sprite file FE 18 has started loading, until FE 1A takes it in. */
		FPartyMemberLoad MemberLoad;
		/** An actor's extra animation set on its way (FE 4A), until FE 4B gives it to the actor's sprite. */
		bool bExtraAnimationsLoading = false;

		// The models' vertex animations, which the scripts weigh (DB) and platforms stand on.
		/**
		 * Each of the map's models' vertices as the last MIMe update left them: every actor placing the
		 * model shares them, as the original's model part does. Empty for a model no MIMe animates.
		 */
		std::vector<std::vector<FGteVector>> MimeVertices;

		// The party's walks as a group (FE 23, FE 24) and the leader's walk to a point (FE 68).
		/** Counted up by FE B5: while it is not 0, a member still on its way is put on its place at once. */
		int16 PartyWalkHurries = 0;
		/** The leader's first flag word as the first tick of FE 68's walk found it, 0 while none runs. */
		uint32 LeaderFlagsBeforeWalk = 0;

		// The field's main loop: what it keeps between its ticks, and the programs and screens the
		// scripts and the player ask for, which it takes up once each tick has run.
		/** The map the field runs (its number alone); MapNumber is the map word, which a map change's request sets to the next map. */
		int32 RunningMap = 0;
		/** The frame buffer the tick draws into (every tick swaps them): some requests wait for one of the two. */
		uint8 DrawBuffer = 0;
		static constexpr int32 NoMapFile = -1;
		/** The map whose file the field has read for a map change, which waits a tick for the read. */
		int32 MapFileRead = NoMapFile;
		/** The parameter the field hands to the menu, and the menus asked for since the last one closed (FE 87 waits for none). */
		uint8 MenuProgramParam = 0;
		int32 MenusPending = 0;
		/** What the movie player tells the scripts of the movie it plays. */
		FFieldMoviePlayback Movie;
		/** How the screen comes back after the menu or a movie (FE 86): 0 fades in, anything else stays dark. */
		uint8 ReturnFade = 0;
		/** Nonzero: Start does not pause the field (FE E0). */
		uint8 PauseDisabled = 0;
		/** After a game's load, or once the scripts leave for the world map: the per-tick update leaves the game state alone. */
		bool bFrameVariablesFrozen = false;
		/** The field runs the map's after-menu script at once (the first actor's script 3): it starts no particle effect. */
		bool bAfterMenuPass = false;
		/** The console's play clock as the tick's vertical blanks left it, which the per-tick update reports to the scripts. */
		FPlayClock PlayClock;

		FEventRun Run;

		FEventActorState* StateOf(int32 Actor)
		{
			return Actor >= 0 && static_cast<std::size_t>(Actor) < States.size() ? &States[static_cast<std::size_t>(Actor)] : nullptr;
		}
		const FEventActorState* StateOf(int32 Actor) const
		{
			return Actor >= 0 && static_cast<std::size_t>(Actor) < States.size() ? &States[static_cast<std::size_t>(Actor)] : nullptr;
		}
	};
}
