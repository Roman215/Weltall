#pragma once

#include "Foundation/Types.h"
#include "Graphics/Vram.h"
#include "Input/ButtonGlyphs.h"
#include "Input/PadState.h"
#include "Menu/MenuPrimitive.h"
#include "Presentation/DrawList.h"

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Weltall
{
	class FMenuSpriteSheet;
	struct FMenuSheetPicture;

	/** A picture of a button a menu frame drew, as the draw list holds it: its primitive, its role (FMenuPrimitive::ButtonRole) and its shape. */
	struct FMenuDrawnIcon
	{
		uint32 Primitive = 0;
		uint16 Role = 0;
		EMenuIconSlot Slot = EMenuIconSlot::FaceButton;
	};

	/** The shape of a role's picture in the menus: the hints' box for L1 and R1, a face button for the rest. */
	WELTALLCORE_API EMenuIconSlot MenuIconSlotOf(uint16 Role);

	/** The controller's button behind a role: the one the kernel's button map turns into that logical button. */
	WELTALLCORE_API EPadButton MenuRoleButton(uint16 Role);

	/** One of a sprite sheet's own pictures of a PlayStation button: its shape, the symbol and the sheet's frame. */
	struct FSheetButtonPicture
	{
		EMenuIconSlot Slot = EMenuIconSlot::FaceButton;
		std::string_view Icon;
		int32 Frame = 0;
	};

	/** The menu sheet's own pictures: the four face buttons and the L1 and R1 hints' boxes. */
	inline constexpr std::array<FSheetButtonPicture, 6> MenuSheetButtonPictures{{
		{EMenuIconSlot::FaceButton, "Cross", 0xF9},
		{EMenuIconSlot::FaceButton, "Triangle", 0xFA},
		{EMenuIconSlot::FaceButton, "Square", 0xFB},
		{EMenuIconSlot::FaceButton, "Circle", 0xFC},
		{EMenuIconSlot::Hint, "L1", 0x164},
		{EMenuIconSlot::Hint, "R1", 0x165},
	}};

	/**
	 * The battle HUD sheet's own pictures: the command ring's four face buttons, and the three attack buttons
	 * as a bare symbol and as the cluster with that button lit (no Circle among either).
	 */
	inline constexpr std::array<FSheetButtonPicture, 10> BattleHudButtonPictures{{
		{EMenuIconSlot::FaceButton, "Circle", 0x0B},
		{EMenuIconSlot::FaceButton, "Cross", 0x0C},
		{EMenuIconSlot::FaceButton, "Square", 0x0D},
		{EMenuIconSlot::FaceButton, "Triangle", 0x0E},
		{EMenuIconSlot::Symbol, "Cross", 0x5D},
		{EMenuIconSlot::Symbol, "Triangle", 0x5E},
		{EMenuIconSlot::Symbol, "Square", 0x5F},
		{EMenuIconSlot::Cluster, "Cross", 0x07},
		{EMenuIconSlot::Cluster, "Triangle", 0x08},
		{EMenuIconSlot::Cluster, "Square", 0x09},
	}};

	/** Between the keys of an icon that shows several keyboard keys as one key cap (the directional buttons'). */
	inline constexpr char MenuKeysSeparator = '|';

	/** An icon picture's texels a menu pixel, across and down. */
	inline constexpr int32 MenuIconTexelsPerPixel = 2;

	/**
	 * A picture of a button icon in the game's style, the port's own art: a face button's slot a dark
	 * round button (24 x 24 texels) with the icon's symbol or letter in its colours, a hint's a grey box
	 * (32 x 18) with its label in white; a keyboard's key a light key cap in either. A symbol's slot the
	 * face button's symbol or letter alone, large; a cluster's the four face buttons as dots in the
	 * controller's colours, the icon's lit; in both, any other button or key as a face button's slot shows it.
	 * Twice as fine as the menu's pixels (MenuIconTexelsPerPixel), wider than its slot where a key's label
	 * needs it; 15-bit texels as the GPU takes them, 0 transparent.
	 */
	struct FMenuIconPicture
	{
		int32 Width = 0;
		int32 Height = 0;
		std::vector<uint16> Texels;
	};

	/** The picture of Icon (FButtonGlyph::Icon) of Set in a slot of the given shape. */
	WELTALLCORE_API FMenuIconPicture DrawMenuButtonIcon(EButtonIconSet Set, std::string_view Icon, EMenuIconSlot Slot);

	/**
	 * The player's own buttons in a menu's or the battle HUD's drawing: each button picture drawn is shown
	 * as the player's binding of its role. With the PlayStation's symbols a picture whose symbol the
	 * sheet has in that shape is the game's own (its frame's texels, page and CLUT in the drawn one's
	 * place); every other icon is its FMenuIconPicture, drawn once into VRAM of its own as first needed
	 * (Pictures(): 15-bit pages, their primitives' FDrawTexture::Vram PicturesVram) and kept for the
	 * frames to come. A role with no binding keeps the game's picture. The glyphs are asked again only
	 * when their revision changes: a frame allocates nothing once its icons are known.
	 */
	class FMenuButtonIcons
	{
	public:
		/** The VRAM a picture's primitive names (FDrawTexture::Vram): the menu's own is 0. */
		static constexpr uint16 PicturesVram = 1;

		/**
		 * Shows each of Icons (List's button pictures, as FMenuDrawBuilder found them) as Glyphs show its role, or a
		 * role from MenuKeyRole up as the key of Keys it names (a free one, of no icon, as an empty button); Sheet is
		 * the one the pictures were drawn from and Own its pictures of the PlayStation's buttons.
		 */
		WELTALLCORE_API void Apply(FDrawList& List, std::span<const FMenuDrawnIcon> Icons, const IButtonGlyphs& Glyphs, const FMenuSpriteSheet& Sheet,
			std::span<const FSheetButtonPicture> Own, std::span<const FButtonGlyph> Keys = {});

		/**
		 * Shows each button glyph or button named in words of a text's lines (Cells: in List's line sprites) as Glyphs show
		 * that button: the text as it is while the binding is what it shows on the PlayStation's controller, else the port's
		 * face-button picture where the cell starts, the line's sprite cut about it and what follows in the line moved by as
		 * much as the picture is wider than the cell (narrower for a word); the directional buttons as the D-pad, a stick, or
		 * the four keys as one key cap. A line's sprite that lies apart in the list (FTextButtonCell::LineRest) moves with
		 * what follows the cell too. Matches (List's pairs with an earlier list, by index) and Mark (an index into List)
		 * keep in step with the primitives it adds.
		 */
		WELTALLCORE_API void ApplyToText(FDrawList& List, std::span<const FTextButtonCell> Cells, const IButtonGlyphs& Glyphs, std::vector<int32>* Matches = nullptr,
			uint32* Mark = nullptr);

		/**
		 * Sets out a prompt the game draws as the controller's four face buttons (FMenuPrimitive::PromptRole) as the
		 * player's buttons lie: each button, with what is drawn beside it, at the game's place of the face button that lies
		 * where Glyphs show it on the pad (the Japanese layout's symbols where the Japanese release draws them). The game's
		 * places stay unless each button shows as a face button of a place of its own, which a key has not, and unless
		 * the pictures moved would cover pictures of another button they did not (cancel's long words at the left place).
		 */
		WELTALLCORE_API void PlacePrompt(std::span<FMenuPrimitive> Primitives, const IButtonGlyphs& Glyphs);

		const FVram& Pictures() const { return PictureVram; }

	private:
		/** How a role is shown, as the glyphs last said. */
		struct FRoleGlyph
		{
			uint16 Role = 0;
			EButtonIconSet Set = EButtonIconSet::Keyboard;
			std::string Icon;
			/** For the directional buttons' glyph: one past the revision its icon was made for, 0 before. */
			uint32 Directions = 0;
		};

		/** The role's glyph, asked of Glyphs only when they are other glyphs or a newer revision. */
		const FRoleGlyph& GlyphOf(uint16 Role, const IButtonGlyphs& Glyphs);
		/** The same for a button, kept under Key. */
		FRoleGlyph& GlyphOfButton(uint16 Key, EPadButton Button, const IButtonGlyphs& Glyphs);
		/** The directional buttons' glyph: Up's set, its icon the four directions together. */
		const FRoleGlyph& DirectionsGlyph(const IButtonGlyphs& Glyphs);

		struct FPlaced
		{
			EButtonIconSet Set = EButtonIconSet::Keyboard;
			std::string Icon;
			EMenuIconSlot Slot = EMenuIconSlot::FaceButton;
			/** Its texels in VRAM: the page's corner, and its place in the page. */
			int32 PageX = 0;
			int32 PageY = 0;
			int32 U = 0;
			int32 V = 0;
			int32 Width = 0;
			int32 Height = 0;
			/** A sheet's own picture (FMenuSheetPicture) rather than a button's, known by its key. */
			bool bSheetPicture = false;
			uint64 PictureKey = 0;
		};

		/** The picture's place, drawn into the pictures' VRAM the first time; null once the pages are full. */
		const FPlaced* Place(EButtonIconSet Set, std::string_view Icon, EMenuIconSlot Slot);
		/** The same for a sheet's own picture. */
		const FPlaced* PlacePicture(const FMenuSheetPicture& Own);
		/** Picture put on the pages' shelves and uploaded; null once the pages are full. */
		FPlaced* Shelve(FMenuIconPicture Picture);

		FVram PictureVram;
		std::vector<FPlaced> Placed;
		/** The roles' glyphs, of these glyphs at this revision. */
		std::array<FRoleGlyph, 18> Roles{};
		std::vector<FTextButtonCell> TextCells;
		std::vector<FDrawPrimitive> TextPieces;
		/** The sprites a text's cells cut so far: each one's place as the cells name it, and the pieces it grew by. */
		std::vector<std::pair<uint32, uint32>> TextCuts;
		int32 RolesKnown = 0;
		const IButtonGlyphs* RolesOf = nullptr;
		uint32 RolesRevision = 0;
		/** The shelf the next picture goes on: its page, its top, how far along it is filled and its height. */
		int32 Page = 0;
		int32 ShelfY = 0;
		int32 ShelfX = 0;
		int32 ShelfHeight = 0;
	};
}
