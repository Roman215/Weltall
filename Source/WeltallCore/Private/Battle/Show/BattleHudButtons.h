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
	 * lit (7-9). The pictures are the US release's physical buttons; the roles are the logical ones behind them.
	 */
	inline FHudButton HudButton(int32 Frame)
	{
		switch (Frame)
		{
		case 0x0B:
		case 0x46:
			return {0x40, EMenuIconSlot::FaceButton};
		case 0x0C:
		case 0x47:
			return {0x20, EMenuIconSlot::FaceButton};
		case 0x0D:
		case 0x48:
			return {0x10, EMenuIconSlot::FaceButton};
		case 0x0E:
		case 0x49:
			return {0x80, EMenuIconSlot::FaceButton};
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
