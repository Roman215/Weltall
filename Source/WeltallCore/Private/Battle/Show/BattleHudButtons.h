#pragma once

#include "Foundation/Types.h"
#include "Menu/MenuPrimitive.h"

namespace Weltall::BattleHudButtons
{
	/** A sheet frame that pictures a button: the logical button (0 for any other frame) and the picture's shape. */
	struct FHudButton
	{
		uint16 Role = 0;
		EMenuIconSlot Slot = EMenuIconSlot::FaceButton;
	};

	/**
	 * The button a sheet frame pictures, for the player's bindings: the command ring's four face buttons
	 * (Bh-Eh, dimmed 46h-49h), the attack buttons pressed (5Dh-5Fh) and the Deathblow hints' clusters with one button
	 * lit (7-9). The ring's pictures are the physical buttons, their roles the logical ones the kernel reads them as,
	 * the Japanese commands' otherwise. The other frames picture the attacks' own buttons: the Italian translation
	 * redraws them for its Japanese commands, so their roles stay.
	 */
	inline FHudButton HudButton(int32 Frame, bool bJapaneseCommands)
	{
		switch (Frame)
		{
		case 0x0B:
		case 0x46:
			return {static_cast<uint16>(bJapaneseCommands ? 0x20 : 0x40), EMenuIconSlot::FaceButton};
		case 0x0C:
		case 0x47:
			return {static_cast<uint16>(bJapaneseCommands ? 0x40 : 0x20), EMenuIconSlot::FaceButton};
		case 0x0D:
		case 0x48:
			return {static_cast<uint16>(bJapaneseCommands ? 0x80 : 0x10), EMenuIconSlot::FaceButton};
		case 0x0E:
		case 0x49:
			return {static_cast<uint16>(bJapaneseCommands ? 0x10 : 0x80), EMenuIconSlot::FaceButton};
		case 0x5D:
			return {0x20, EMenuIconSlot::Symbol};
		case 0x5E:
			return {0x80, EMenuIconSlot::Symbol};
		case 0x5F:
			return {0x10, EMenuIconSlot::Symbol};
		case 0x07:
			return {0x20, EMenuIconSlot::Cluster};
		case 0x08:
			return {0x80, EMenuIconSlot::Cluster};
		case 0x09:
			return {0x10, EMenuIconSlot::Cluster};
		default:
			return {};
		}
	}
}
