#pragma once

#include "Foundation/Types.h"
#include "Input/ControlBindings.h"
#include "Menu/MenuSettingsPages.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	class IMenuKeys;
	struct FPressedKey;

	/** What a key the player pressed while the Controls page listened did. */
	enum class EControlsHeard : uint8
	{
		/** Nothing: a key where an axis is asked for, or the other way round. Listening goes on. */
		Ignored,
		/** Bound where the page listened, among the keys of its own device. */
		Bound,
		/** Esc: listening stopped, nothing changed. */
		Stopped,
		/** Not bound: it is the only key of its device a PlayStation button has. */
		Refused,
	};

	/**
	 * The options screen's Controls page: each PlayStation button and each of the camera's actions with
	 * two keys of the device the player last touched (the keyboard and mouse, or a controller; each keeps its own),
	 * drawn as their pictures. Confirm on one listens for the key, button, stick or mouse movement to put there, of
	 * either device (Esc, or five seconds without one, stops); the menus' Square (the US discs' Triangle, Jump) clears
	 * it and their Triangle (Square, Menu) swaps the row's two; rows say whether the party runs without the run button held
	 * and whether doors open when walked into, and step the mouse's and a stick's sensitivity; a last row puts the
	 * device's keys back as they start. Its words are OptionsText's. A key bound elsewhere moves here, the action it leaves
	 * taking the key it replaced; no PlayStation button is left without a key of either device. Each row as the
	 * screen shows it; what an action did is told in the help of its row until the cursor moves.
	 */
	class FMenuControlsPage
	{
	public:
		/** How many menu frames listening waits for a key: five seconds. */
		static constexpr int32 ListenFrames = 300;

		/**
		 * Shows the device the player last used; Keys must outlive the page. With the Japanese commands
		 * (FKernelPad::HasJapaneseCommands) the help names the buttons the game reads for each part.
		 */
		WELTALLCORE_API explicit FMenuControlsPage(IMenuKeys& InKeys, bool bInJapaneseCommands = false);
		/** Listening stops with the page. */
		WELTALLCORE_API ~FMenuControlsPage();
		FMenuControlsPage(const FMenuControlsPage&) = delete;
		FMenuControlsPage& operator=(const FMenuControlsPage&) = delete;

		static std::string_view Title() { return "Controls"; }
		WELTALLCORE_API std::vector<FSettingsRow> Rows(const FControlBindings& Bindings) const;

		/** The device whose keys the rows show. */
		EInputFamily Family() const { return Shown; }
		/** The cell the cursor is on in a row of keys, from the left. */
		int32 Cell() const { return Slot; }
		bool Listening() const { return bListening; }

		/** The rows follow the device the player last touched, and its icons: true when either changed. Not while listening. */
		WELTALLCORE_API bool Follow();
		/** Left (-1) or Right (+1): the cursor moved between a row's keys, or the row's choice changed. False where nothing changed. */
		WELTALLCORE_API bool Step(int32 Row, int32 Direction, FControlBindings& Bindings);
		/** Confirm on a row: listening for its cell's key, or its device's keys put back. False where it does nothing. */
		WELTALLCORE_API bool Activate(int32 Row, FControlBindings& Bindings);
		/** The menus' Square on a row: its cell's key cleared. False where there is none, or it is a button's last key of its device (told so). */
		WELTALLCORE_API bool Clear(int32 Row, FControlBindings& Bindings);
		/** The menus' Triangle (the US discs' Square) on a row: its two keys change places, the first being the one the game's prompts show. False with fewer than two. */
		WELTALLCORE_API bool Swap(int32 Row, FControlBindings& Bindings);
		/** A key the player pressed while the page listened. */
		WELTALLCORE_API EControlsHeard Heard(const FPressedKey& Pressed, FControlBindings& Bindings);
		/** A menu frame passed while the page listened: true when listening gave up, nothing pressed in time. */
		WELTALLCORE_API bool Waited();
		/** The cursor left its row: what the last action told is gone. */
		WELTALLCORE_API void CursorMoved();

	private:
		/** The icons a key of a device is drawn with. */
		EButtonIconSet IconSet(EInputFamily Family, const FControlBindings& Bindings) const;
		/** "key" or "button", as the device shown calls them. */
		std::string_view KeyNoun() const;
		std::array<std::string, 3> Help(int32 Row) const;
		void Tell(int32 Row, std::string Line);
		/** The layout's row stepped: the bindings take the next layout, and the row tells when the player's own buttons stay. */
		void StepLayout(int32 Row, int32 Direction, FControlBindings& Bindings);
		void StopListening();

		IMenuKeys& Keys;
		bool bJapaneseCommands = false;
		EInputFamily Shown = EInputFamily::KeyboardAndMouse;
		EButtonIconSet PadIcons = EButtonIconSet::Xbox;
		int32 Slot = 0;
		bool bListening = false;
		int32 ListenRow = -1;
		int32 ListenedFrames = 0;
		int32 ToldRow = -1;
		std::string Told;
	};
}
