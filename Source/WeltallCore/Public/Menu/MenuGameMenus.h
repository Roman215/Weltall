#pragma once

#include "Game/GameMenus.h"
#include "Menu/MenuProgram.h"
#include "Presentation/DisplayOptions.h"

#include <functional>

namespace Weltall
{
	class FMenuProgram;
	class IMenuKeys;
	class IMenuAssets;
	class IMenuDeviceFiles;
	struct FPlayerSettings;
	class FPsxExecutable;

	/**
	 * The Menu in the game's loop: each menu the field asks for is the Menu program over the session's
	 * services (its state, random numbers, pad, play clock, text, the kernel's globals and the game's
	 * sound, effect file 0's system effects), its files read from the session's disc as it opens. Each
	 * of its frames is one vertical blank, which the session counts before the menu's code runs. The
	 * menus must outlive the programs they open.
	 */
	class FMenuGameMenus final : public IGameMenus
	{
	public:
		/** The menus of the disc whose kernel is Executable, which must outlive them. */
		explicit FMenuGameMenus(const FPsxExecutable& InExecutable) : Executable(InExecutable) {}

		WELTALLCORE_API TResult<std::unique_ptr<IGameProgram>> Open(FGameSession& Session, const FFieldRequest& Request) override;

		/** The menu running now, whose frames the display draws; null when none is. */
		const FMenuProgram* Running() const { return Current; }

		/**
		 * Told the player's settings as the options screen closes, to keep them for the next session (the session
		 * takes each change at once). None: the menus offer no options screen, the original's Sound in its place.
		 */
		void SetSettingsKept(std::function<void(const FPlayerSettings&)> InKept) { SettingsKept = std::move(InKept); }

		/** Told each change the options screen makes, as it is made, before it is kept: the host's input follows the bindings at once. */
		void SetSettingsFollowed(std::function<void(const FPlayerSettings&)> InFollowed) { SettingsFollowed = std::move(InFollowed); }

		/** The player's keys, which the options screen's Controls page rebinds; none (or no settings kept): no Controls page. It must outlive the menus. */
		void SetKeys(IMenuKeys* InKeys) { Keys = InKeys; }
		void SetAssets(IMenuAssets* InAssets) { Assets = InAssets; }
		/** The device's file picker, for the Saves page; none where the game browses the computer's folders itself. It must outlive the menus. */
		void SetDeviceFiles(IMenuDeviceFiles* InFiles) { DeviceFiles = InFiles; }
		/** The mods the game has found, asked as the options screen's Modding page lists them; none: none found. */
		void SetMods(std::function<FMenuMods()> InMods) { Mods = std::move(InMods); }

		/** What the display offers, asked as the options screen opens; none: the screen offers no window sizes. */
		void SetDisplayModes(std::function<FDisplayModes()> InModes) { DisplayModes = std::move(InModes); }

		/** Closes the game, which the title's Exit orb asks for; none: the title has the original's three orbs. */
		void SetQuit(std::function<void()> InQuit) { Quit = std::move(InQuit); }

	private:
		friend class FMenuGameProgram;
		friend class FMenuGameSettings;

		const FPsxExecutable& Executable;
		const FMenuProgram* Current = nullptr;
		std::function<void(const FPlayerSettings&)> SettingsKept;
		std::function<void(const FPlayerSettings&)> SettingsFollowed;
		IMenuKeys* Keys = nullptr;
		IMenuAssets* Assets = nullptr;
		IMenuDeviceFiles* DeviceFiles = nullptr;
		std::function<FMenuMods()> Mods;
		std::function<FDisplayModes()> DisplayModes;
		std::function<void()> Quit;
	};
}
