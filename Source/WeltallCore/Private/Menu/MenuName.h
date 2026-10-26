#pragma once

#include "Menu/MenuModule.h"

#include <array>
#include <vector>

namespace Weltall
{
	/**
	 * Enter Name (mode 3) as its module runs: a name number's name typed on a grid of characters, glyph
	 * by glyph over its old one (the French translation's: the old one edited at a cursor), and written into its name
	 * slots.
	 */
	class FMenuName final : public FMenuModule
	{
	public:
		FMenuName(const FMenuServices& InServices, const FMenuRequest& InRequest);

		TMenuTask<> Run() override;

	private:
		void DrawFrame() override;
		void Init();
		TMenuTask<> EnterName();
		TMenuTask<> Shutdown();
		/** The grid's byte a cell reads (the cursor can reach past the grid into the module's next bytes). */
		uint8& Cell(int32 Row, int32 Column);
		/** The glyphs typed so far as the name's text. */
		void TypeText();
		/** The glyph map's first glyph of a code (its lead byte not looked at). */
		uint16 GlyphOfCode(uint8 Byte) const;
		void WriteNames();

		// The screen (MenuNameScreen.cpp).
		TMenuTask<> BuildScreen();
		TMenuTask<> ShowPrompt(uint8 FirstText);
		TMenuTask<> HidePrompt();
		void BuildGrid();
		void RenderName();
		void PlaceCursor(int32 Column, int32 Row);
		void AddLabels();
		void AddNameScreen();

		/** The grid as this run changes it: a blank cell typed becomes a space. */
		std::vector<uint8> Grid;
		uint8 OutsideCell = 0;
		/** The name as glyphs: the ones typed over blanks, ended by 1Fh; and as the text it makes. */
		std::array<uint16, 10> Glyphs{};
		std::array<uint8, 24> Text{};
		int32 Position = 0;
		int32 MaxLength = 9;
		uint8 Blink = 0;
		/** The three name slots the name number writes. */
		std::array<uint8, 3> NameSlots{};

		FMenuPrimitive Portrait;
		FMenuPrimitive NameQuad;
		std::vector<FMenuPrimitive> AcceptIcon;
		std::vector<FMenuPrimitive> BackIcon;
		std::vector<FMenuPrimitive> EndMark;
		/** The box around the cell under the cursor (two polylines) and the underline under the insertion place. */
		FMenuPrimitive BoxTop;
		FMenuPrimitive BoxBottom;
		FMenuPrimitive Underline;
		/** The grid's strips: each block's rows of five, typed as one label. */
		std::array<FMenuPrimitive, 36> Strips{};
		std::vector<FMenuPrimitive> GridPointer;
		bool bPointerShown = false;
		bool bScreenShown = false;
		bool bPortraitShown = false;
		bool bControlsShown = false;
		bool bPromptShown = false;
		std::array<FMenuLabel, 3> Prompt{};
		std::array<FMenuLabel, 4> Labels{};
	};
}
