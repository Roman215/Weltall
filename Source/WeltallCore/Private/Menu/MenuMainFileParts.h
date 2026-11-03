#pragma once

#include "Game/ClearGame.h"
#include "Menu/MenuModuleParts.h"

#include <array>
#include <string>
#include <vector>

namespace Weltall
{
	/**
	 * A save as the File screen holds it, read from its store as a card's file would be: its name on a card,
	 * its first 200h bytes (the header the game writes before the data, then the data's start), the data
	 * whole, and its icon's place in each step of the animation.
	 */
	struct FMenuStoreFile
	{
		std::string Name;
		std::array<uint8, 0x200> Head{};
		std::vector<uint8> Data;
		/** The icon's row in the page at (320, 0) in each of the animation's six steps. */
		std::array<uint8, 6> IconRows{};
		/** Its slot in its store, 0-14. */
		int32 Slot = 0;
		/** Read and its icon loaded: drawn in its cells. */
		bool bShown = false;
		/** What the save's game is after the ending (Weltall's part of it): its details star any but none. */
		EClearGame ClearGame = EClearGame::None;
	};

	/** A cell of a card as drawn: its file's icon, the cursor's grey over it and that grey's drawing mode, its outline in two lines. */
	struct FMenuCardCell
	{
		FMenuPrimitive Icon;
		FMenuPrimitive Highlight;
		FMenuPrimitive HighlightMode;
		/** The top and right edges, then the left and bottom ones. */
		std::array<FMenuPrimitive, 2> Outline{};
		/** The cell's corners on the layer. */
		std::array<FGteVector, 4> Points{};
	};

	/**
	 * The details under the cards of the save the cursor is on: each member's labels, portrait, name, level,
	 * HP and EP; the play time, the slot's number, the place's name; the band behind them.
	 */
	struct FMenuSaveDetails
	{
		std::array<bool, 3> MemberShown{};
		std::array<std::vector<FMenuPrimitive>, 3> Labels{};
		std::array<std::vector<FMenuPrimitive>, 3> Portraits{};
		std::array<FMenuPrimitive, 3> Names{};
		std::array<std::vector<FMenuPrimitive>, 3> Levels{};
		std::array<std::vector<FMenuPrimitive>, 3> Hp{};
		std::array<std::vector<FMenuPrimitive>, 3> MaxHp{};
		std::array<std::vector<FMenuPrimitive>, 3> Ep{};
		std::array<std::vector<FMenuPrimitive>, 3> MaxEp{};
		std::vector<FMenuPrimitive> Time;
		std::vector<FMenuPrimitive> Number;
		std::array<FMenuPrimitive, 16> Place{};
		FMenuPrimitive Band;
	};

	/**
	 * The File screen's two cards, each standing for one of the player's save stores: their files and
	 * cells as the screen reads them, the cursor on the cells, and how they are drawn.
	 */
	struct FMenuFileScreen
	{
		static constexpr int32 Cards = 2;
		/** A card's files and cells: 15 used, the 16th never. */
		static constexpr int32 PerCard = 16;
		static constexpr int32 CellsPerCard = 15;
		static constexpr uint8 Free = 0xFF;
		/** The cell of a cursor off the cards (none found to stand on): the original reads one past its places' table, off both cards. */
		static constexpr int32 NoCell = -1;

		std::array<FMenuStoreFile, Cards * PerCard> Files{};
		/** Each cell's file (the card's files in order, once per block), Free for none. */
		std::array<uint8, Cards * PerCard> Cells{};
		/** Each cell holds a save of this game. */
		std::array<bool, Cards * PerCard> CellIsSave{};
		/** Each card's slots its saves use. */
		std::array<bool, Cards * PerCard> SlotUsed{};
		/** The blocks each card's files take. */
		std::array<int32, Cards> UsedBlocks{};
		/** Each card's files as last listed, and as last read (Free: to be read again). */
		std::array<uint8, Cards> Listed{};
		std::array<uint8, Cards> Scanned{};
		/** Each card's list taken since it was last asked for again. */
		std::array<bool, Cards> ListTaken{};
		/** How the frames list the cards: 0 not at all, 1 or 2 the cards whose lists are asked for again. */
		uint8 Listing = 0;
		/** The cells filled so far in a card's reading. */
		int32 CellsFilled = 0;
		/** The cursor's place (0-29: a card's 15 cells, then the other's), and the place the details were last built for. */
		int32 Cursor = 0;
		int32 DetailsCursor = Free;
		/** The cards drawn: 0 not, 1 their outlines and icons, 2 with the cursor's save picked out. */
		uint8 Drawn = 0;
		/** The icons' animation: its step (0-5) and the frames the step has shown; the cursor's grey, rising or falling. */
		int32 IconStep = 0;
		int32 IconFrames = 0;
		int32 Grey = 0;
		bool bGreyFalling = false;
		/** The cards are to be read again after a copy or a delete. */
		bool bRescan = false;
		/** The pointer follows the cursor onto its cell. */
		bool bPointerOnCursor = false;
		/** The first of the two stores the cards stand for. */
		int32 FirstStore = 0;

		std::array<FMenuCardCell, Cards * PerCard> CellPieces{};
		/** Each card's label ("CARD 1", "CARD 2") and orb, and whether they were drawn this frame. */
		std::array<std::vector<FMenuPrimitive>, Cards> Labels{};
		std::array<std::vector<FMenuPrimitive>, Cards> Orbs{};

		FMenuSaveDetails Details;
	};
}
