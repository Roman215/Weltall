#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Game/SaveLoad.h"
#include "Input/ButtonGlyphs.h"
#include "Input/PadState.h"
#include "Media/VoiceLanguage.h"
#include "Menu/MenuOptions.h"
#include "Menu/MenuPrimitive.h"
#include "Menu/MenuTask.h"
#include "Mods/PerfectWorks.h"
#include "Presentation/DisplayOptions.h"
#include "Text/MesWindow.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FGameState;
	class FGearShopModel;
	class FKernelPad;
	struct FKernelState;
	class FMenuModule;
	class FMenuResources;
	class FPsxRandom;
	struct FPlayerSettings;
	class FSaveStore;
	class FStoryWayBack;
	class IMenuAssets;
	class IMenuDeviceFiles;
	class FVram;
	class IVramUploads;
	struct FPlayClock;

	/** The Menu program's modes, as the field and the world map ask for them. */
	enum class EMenuMode : uint8
	{
		Normal = 0,
		MemberChange = 1,
		/** The title screen: Sound, Continue, New Game. */
		LoadGame = 2,
		EnterName = 3,
		Shop = 4,
		GearShop = 5,
		DiscChange = 6,
	};

	/** What the game asks of the menu. */
	struct FMenuRequest
	{
		EMenuMode Mode = EMenuMode::Normal;
		/** The mode's parameter: in the Normal Menu, whether saving is allowed (a save point's, or the world map's). */
		uint8 Param = 0;
		/** The gears can be neither boarded nor left where the menu was opened. */
		bool bGearBoardingLocked = true;
	};

	/** The sound the menu makes: the system effects of the field's resident effect file, and the sound's output. */
	class IMenuSound
	{
	public:
		virtual ~IMenuSound() = default;

		/** One of the menus' system effects (1 the cursor, 2 a confirm, 3 a cancel, 4 the buzzer...). */
		virtual void PlaySystemEffect(uint8 Effect) = 0;
		/** The sound stands still while no controller is connected, and goes on. */
		virtual void Pause() = 0;
		virtual void Resume() = 0;

		/** The sound driver's output: 0 mono, 1 stereo, 2 stereo with one side inverted ("wide"). */
		virtual int32 OutputMode() const = 0;
		virtual void SetOutputMode(int32 Mode) = 0;
	};

	/** The mods the game found in its Mods folder, as the options screen offers them. */
	struct FMenuMods
	{
		/** Perfect Works Build was found, and its version (empty where its README names none). */
		bool bPerfectWorks = false;
		std::string PerfectWorksVersion;
		/** What the game took from it as it started; the settings' choice applies from the next start. */
		FPerfectWorksOptions PerfectWorksInUse;
	};

	/**
	 * The player's settings, which the options screen shows and changes: a change takes effect at once,
	 * and the settings are kept (written for the next session) as the screen closes.
	 */
	class IMenuSettings
	{
	public:
		virtual ~IMenuSettings() = default;

		virtual const FPlayerSettings& Settings() const = 0;
		virtual void Change(const FPlayerSettings& Settings) = 0;
		virtual void Keep() = 0;
		/** What the display offers the Display page; none where the game has no window. */
		virtual FDisplayModes DisplayModes() const { return {}; }
		virtual IMenuAssets* Assets() const { return nullptr; }
		/** The device's file picker, which the Saves page's transfers go through; none where the game browses the computer's folders itself. */
		virtual IMenuDeviceFiles* DeviceFiles() const { return nullptr; }
		/** The mods found; none where the game looked for none. */
		virtual FMenuMods Mods() const { return {}; }
		/** The voices and movie subtitles the game was given, which the Sound page offers; none where it says nothing of them. */
		virtual FSoundSources SoundSources() const { return {}; }
	};

	/** A key, button or axis the player pressed, by the engine's name for it. */
	struct FPressedKey
	{
		std::string Key;
		/** For an axis moved (a stick, the mouse, its wheel), the way it moved: +1 or -1; 0 for a key or button pressed. */
		int32 Axis = 0;

		bool operator==(const FPressedKey&) const = default;
	};

	/**
	 * The player's keys as the host's input has them, for the options screen's Controls page to bind: while it
	 * listens, every key, button, stick and mouse movement pressed in order, before (and instead of) the game's
	 * buttons; and the devices the player uses.
	 */
	class IMenuKeys
	{
	public:
		virtual ~IMenuKeys() = default;

		/** Starts or stops listening. While it listens the game is given no buttons, and none after it stops until every one is let go. */
		virtual void Listen(bool bListening) = 0;
		/** The next key pressed since listening began, oldest first; none when there are no more. */
		virtual std::optional<FPressedKey> TakePressed() = 0;

		/** The devices the player last pressed something on. */
		virtual EInputFamily LastFamily() const = 0;
		/** The icons of the gamepad the player uses, or last used (Xbox's before any). */
		virtual EButtonIconSet GamepadIcons() const = 0;
	};

	/**
	 * What the menu takes from the game around it: the disc's files (for a new game's state), what the
	 * menu reads of them, the game's state, random numbers, the console's pad and play clock, the sound,
	 * the text system's font, name tables and button map, and the disc played from. Each must outlive
	 * the menu.
	 */
	/** The game's discs as the menu asks for them. */
	class IMenuDiscs
	{
	public:
		virtual ~IMenuDiscs() = default;
		/** Whether the game can read disc Disc (0 the first), looking for it again where it has not been found. */
		virtual bool HasDisc(int32 Disc) = 0;
	};

	struct FMenuServices
	{
		const FCdImage* Image = nullptr;
		const FGameFileIndex* Index = nullptr;
		const FMenuResources* Resources = nullptr;
		FGameState* Game = nullptr;
		FPsxRandom* Random = nullptr;
		FKernelPad* Pad = nullptr;
		FPlayClock* Clock = nullptr;
		/** None: nothing is heard. */
		IMenuSound* Sound = nullptr;
		FMesContext Text;
		/** The disc in the drive, 1 or 2. */
		int32 Disc = 1;
		/** The kernel's globals a new game resets beside the state (the chained battle) and the disc the game asks for; none on the menu's own. */
		FKernelState* Kernel = nullptr;
		/** The player's saves, which the File screen shows two stores at a time; none: the File screen cannot open. */
		FSaveStore* Saves = nullptr;
		FMenuOptions Options;
		/** The fixes of the File screen's load. */
		FSaveLoadOptions LoadOptions;
		/**
		 * The player's settings: the title's Sound becomes the options screen, which the Normal Menu also
		 * opens from an orb of its own. None: the original's menus.
		 */
		IMenuSettings* Settings = nullptr;
		/** The player's keys, which the options screen's Controls page rebinds; none: the screen has no Controls page. */
		IMenuKeys* Keys = nullptr;
		/** The game's discs: the disc prompts ask until the disc is there; none: they take any confirmation. */
		IMenuDiscs* Discs = nullptr;
		/** Closes the game: the title's arc has an Exit orb at its foot, under the other three; none: the original's arc. */
		std::function<void()> Quit;
		/**
		 * The Normal Menu's File screen loads too, and a game loaded there ends the menu with ProgramResult 2 for the
		 * program it runs in to go on from the save's map. Off: the original's File screen, which loads at the title alone.
		 */
		bool bLoadsInPlay = false;
		/**
		 * The party is shown in the order its player keeps it (FPartyOrder), which Right from the Normal Menu's arc
		 * arranges on the party's panels. Off: the original's menus, the party in the game's order, Right doing nothing.
		 */
		bool bArrangesParty = false;
		/**
		 * The port has the expanded second disc: a game that goes on to the second disc with no story mode chosen (at
		 * the disc change, or loaded from a save made before the choice) is asked which it plays, after the disc's
		 * prompt. Off: no game is asked, and each plays the original's.
		 */
		bool bStoryExpansion = false;
		/**
		 * Where a game of the expanded second disc goes on in the original's: a save made where the original stands
		 * elsewhere is the original's at that point, keeping the game's own place for the port. None: every save is
		 * the game's state as it is.
		 */
		const FStoryWayBack* WayBack = nullptr;
	};

	/**
	 * The Menu program, run frame by frame: each frame is one vertical blank of the console, the menu's
	 * code running until its frame is drawn and the blank passing with the controller's buttons. What it
	 * leaves the game (a new game, a changed state, the mode's result) is in the game state once it has ended.
	 */
	class FMenuProgram
	{
	public:
		WELTALLCORE_API FMenuProgram(const FMenuServices& Services, const FMenuRequest& Request);
		WELTALLCORE_API ~FMenuProgram();
		FMenuProgram(const FMenuProgram&) = delete;
		FMenuProgram& operator=(const FMenuProgram&) = delete;

		/**
		 * The menu's next frame, then the vertical blank after it with Held down (the play time, the
		 * pad and the play clock take it). False, with no frame drawn, once the menu has ended; an error
		 * names what the port does not run yet, and the menu stays where it stopped. Uploads, when given,
		 * sees each image the menu loads into its VRAM meanwhile, in order.
		 */
		[[nodiscard]] WELTALLCORE_API TResult<bool> Frame(FPadState Held, IVramUploads* Uploads = nullptr);

		/**
		 * The menu's code up to its next frame drawn, without the vertical blank after it: for a caller
		 * that runs the console's vertical blanks itself (the game's loop, before each frame). False once
		 * the menu has ended; errors as Frame's.
		 */
		[[nodiscard]] WELTALLCORE_API TResult<bool> Step(IVramUploads* Uploads = nullptr);

		bool HasEnded() const { return bEnded; }

		/** L1, R1, Start and Select were held where the menu allows the console's soft reset: it has ended so. */
		WELTALLCORE_API bool EndedBySoftReset() const;

		/** What the last frame drew, and VRAM as it left it. */
		WELTALLCORE_API const FMenuFrame& Snapshot() const;
		WELTALLCORE_API const FVram& Vram() const;

		/** The gear shop's model (its gear drawn under the frame's Gear view); none in the other modes. */
		WELTALLCORE_API const FGearShopModel* GearModel() const;

		/** The menus' files and tables the program draws with (its sprite sheet, for the player's button icons). */
		WELTALLCORE_API const FMenuResources& Resources() const;

	private:
		FMenuServices Services;
		/** The module the mode runs. */
		std::unique_ptr<FMenuModule> Module;
		/** The module's run, from its first frame. */
		TMenuTask<> Root;
		bool bEnded = false;
	};
}
