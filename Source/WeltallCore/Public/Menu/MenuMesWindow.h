#pragma once

#include "Graphics/Vram.h"
#include "Menu/MenuPrimitive.h"
#include "Text/MesButtonCells.h"
#include "Text/MesWindow.h"

#include <optional>
#include <vector>

namespace Weltall
{
	/** Where a kernel window's text image lies in VRAM and its text on the screen, and its strips' palettes. */
	struct FMesWindowPlace
	{
		int16 VramX = 0;
		int16 VramY = 0;
		int16 ScreenX = 0;
		int16 ScreenY = 0;
		uint16 EvenClut = 0;
		uint16 OddClut = 0;
	};

	/**
	 * What a window's drawing needs to note the buttons its lines show: the finder, the window's font (whose units its lines
	 * count, the game's 4 pixels without one; for a name read ahead on the line being typed) and where the cells go, each of
	 * a primitive appended, by its place in the list.
	 */
	struct FMesWindowButtons
	{
		FMesButtonCells* Finder = nullptr;
		const FMesFont* Font = nullptr;
		std::vector<FTextButtonCell>* Cells = nullptr;
	};

	/**
	 * The kernel's drawing of a window as its latest tick drew it, appended in the GPU's order (the reverse of the kernel's
	 * links): the first page's drawing mode, the box behind the text (a black half-transparent rectangle) unless the window
	 * has none, each row's first 256 pixels, the second page's drawing mode, each row's rest. The highlighted row is
	 * multiplied by the window's colour, the others drawn raw. Nothing for a window the tick did not draw. The buttons
	 * its rows show go to Buttons when given.
	 */
	WELTALLCORE_API void AppendMesWindowPrimitives(const FMesWindow& Window, const FMesWindowPlace& Place, std::vector<FMenuPrimitive>& Out,
		const FMesWindowButtons& Buttons = {});

	/**
	 * The image a kernel window types into, kept from tick to tick as the kernel keeps it: never cleared, so a line types
	 * over what the lines before it left in its plane.
	 */
	class FMesWindowImage
	{
	public:
		/**
		 * The latest tick's step drawn into the image, and the uploads the kernel makes of it added to Uploads: its line's
		 * strip, after those of the lines a tick of instant text typed before it; none when the tick took no step.
		 */
		WELTALLCORE_API void Type(const FMesFont* Font, const FMesWindow& Window, const FMesWindowPlace& Place, std::vector<FVramImage>& Uploads);

	private:
		std::vector<uint16> Units;
	};
}
