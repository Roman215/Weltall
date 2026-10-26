#pragma once

#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Input/ButtonGlyphs.h"
#include "Presentation/DrawList.h"

#include <array>
#include <vector>

namespace Weltall
{
	/** The GPU commands the menus draw with, and their flags. */
	namespace MenuGpu
	{
		constexpr uint8 FlatQuad = 0x28;
		constexpr uint8 TexturedQuad = 0x2C;
		constexpr uint8 GouraudQuad = 0x38;
		/** A line of one colour between two points, and one through several (its corners, in LinePoints). */
		constexpr uint8 FlatLine = 0x40;
		constexpr uint8 FlatPolyline = 0x48;
		/** A textured rectangle of any size (libgpu's SPRT): its texels are of the page the drawing-mode command before it names. */
		constexpr uint8 Sprite = 0x64;
		constexpr uint8 DrawModeCommand = 0xE1;
		/** In a polygon's code: its pixels blend with the ones behind. */
		constexpr uint8 SemiTransparent = 0x02;
		/** In a textured polygon's code: its texels are drawn as they are, not multiplied by its colour. */
		constexpr uint8 RawTexture = 0x01;
		/** In a polygon's code. */
		constexpr uint8 Textured = 0x04;
		constexpr uint8 Gouraud = 0x10;
		/** A texture page attribute's blend mode bits: 1 is B + F. */
		constexpr uint16 PageBlendAdd = 0x20;

		constexpr uint16 TexturePage(int32 Depth, int32 Blend, int32 X, int32 Y)
		{
			return static_cast<uint16>((Depth & 3) << 7 | (Blend & 3) << 5 | (Y & 0x100) >> 4 | (X & 0x3FF) >> 6 | (Y & 0x200) << 2);
		}

		constexpr uint16 Clut(int32 X, int32 Y) { return static_cast<uint16>(Y << 6 | (X >> 4 & 0x3F)); }

		/** The drawing-mode command libgpu's SetDrawMode makes without dithering or drawing into the display. */
		constexpr uint32 DrawMode(uint16 Page) { return 0xE1000000u | (Page & 0x9FFu); }
	}

	/** Where a primitive's corners are: placed on the screen, or projected each frame from 3D points. */
	enum class EMenuSpace : uint8
	{
		Screen,
		/** The menu's layer: projected through the transform its opening and closing turn and move. */
		Layer,
		/** Projected through the view that shows the points at the screen's own scale. */
		Flat,
	};

	/** A corner of a menu primitive: its place on the screen, its texel and its colour. */
	struct FMenuCorner
	{
		int16 X = 0;
		int16 Y = 0;
		uint8 U = 0;
		uint8 V = 0;
		FDrawColor Color;
	};

	/**
	 * The shapes of button picture the game draws, each 12 x 12 but the hints' box (16 x 9): the menus' and the
	 * battle ring's round face button, the menus' L1/R1 hints' box, the battle's bare symbol of an attack
	 * button pressed, and the battle hints' cluster of the four face buttons with one lit.
	 */
	enum class EMenuIconSlot : uint8
	{
		FaceButton,
		Hint,
		Symbol,
		Cluster,
	};

	/**
	 * One GPU command a menu links into its ordering table, as the GPU takes it: a three- or four-cornered
	 * polygon (flat, Gouraud or textured, raw or modulated, opaque or semi-transparent: its code says),
	 * a line through its first corners, a rectangle of any size from its first corner, or a drawing-mode
	 * command, whose page the untextured semi-transparent polygons after it blend by and the rectangles
	 * after it take their texels from.
	 */
	struct FMenuPrimitive
	{
		uint8 Code = 0;
		std::array<FMenuCorner, 4> Corners{};
		uint16 Clut = 0;
		uint16 Page = 0;
		/** A drawing-mode command's two words (its mode and texture window). */
		std::array<uint32, 2> Words{};
		EMenuSpace Space = EMenuSpace::Screen;
		/** A projected polygon's corners in 3D, which each frame's projection puts on the screen. */
		std::array<FGteVector, 4> Points{};
		/** Non-zero for a primitive the display draws between frames from its partner in the frame before. */
		uint32 BlendKey = 0;
		/** Covers the whole screen, however wide the display shows it (the background's grey). */
		bool bWholeScreen = false;
		/**
		 * A button icon's role: the game's logical button the picture stands for, as the kernel's
		 * logical word has it (0x20 Circle, 0x40 Cross, 0x10 Triangle, 0x80 Square, 4 L1, 8 R1), for the
		 * display to draw the player's binding of it; from MenuKeyRole up, the picture of a key the frame names
		 * (FMenuFrame::Keys, by the role's offset from it); from MenuPictureRole up, a picture of the sheet's own
		 * (FMenuSpriteSheet::Picture, by the role's offset); 0 for any other picture.
		 */
		uint16 ButtonRole = 0;
		/** A button icon's shape. */
		EMenuIconSlot ButtonSlot = EMenuIconSlot::FaceButton;
		/**
		 * In a prompt the game sets out as the controller's four face buttons (the battle's attack modes), the role of the
		 * button a picture belongs to: the button's own picture and what is drawn beside it, which the display moves
		 * together to the place the player's button has among the four (FMenuButtonIcons::PlacePrompt); 0 for any other.
		 */
		uint16 PromptRole = 0;
		/** A polyline's points (2 to 4); a single line has 2. */
		uint8 LinePoints = 0;
		/** A rectangle's size; its place, texel and colour are its first corner's. */
		int16 Width = 0;
		int16 Height = 0;

		bool IsPolygon() const { return Code >= 0x20 && Code < 0x40; }
		/** A polygon's corners: four for a quad, three for a triangle. */
		int32 PolygonCorners() const { return (Code & 0x08) != 0 ? 4 : 3; }
		bool IsLine() const { return Code >= 0x40 && Code < 0x60; }
		/** How many of a line's corners it goes through. */
		int32 LineCorners() const { return (Code & 0x08) != 0 ? LinePoints : 2; }
		/** A rectangle of any size, textured or not. */
		bool IsRectangle() const { return Code >= 0x60 && Code < 0x68; }
		bool IsDrawMode() const { return Code == MenuGpu::DrawModeCommand; }

		WELTALLCORE_API static FMenuPrimitive MakeDrawMode(uint16 Page);
	};

	/** The first button role that names a key of the frame's rather than a logical button. */
	inline constexpr uint16 MenuKeyRole = 0x100;
	/** The first button role that names a picture of the sheet's own, past every key's. */
	inline constexpr uint16 MenuPictureRole = 0x200;

	/** The 3D layer's transform in one frame: its rotation (4096 a turn) and its translation. */
	struct FMenuLayerTransform
	{
		FGteVector Angles{};
		std::array<int32, 3> Translation{};

		bool operator==(const FMenuLayerTransform&) const = default;
	};

	/** The GTE's state the menus project with: the screen's centre and a projection distance of 512. */
	WELTALLCORE_API FGte MenuProjection(const FMenuLayerTransform& Transform);

	/** The layer's transform that shows its points at the screen's own scale, as the menus' fixed windows do. */
	inline constexpr FMenuLayerTransform MenuFlatTransform{{}, {0, 0, 0x200}};

	/**
	 * Primitives as the menus' 16-entry reversed ordering table sorts them: the table is drawn from its
	 * last entry down, and within an entry the primitive linked last is drawn first.
	 */
	class FMenuOrderingTable
	{
	public:
		static constexpr int32 Entries = 16;

		/**
		 * Links a primitive into an entry. One never built (its code still zero, as a window's before it is
		 * first opened) is an empty packet the GPU passes over: it is left out.
		 */
		void Add(int32 Entry, const FMenuPrimitive& Primitive)
		{
			if (Primitive.Code != 0)
			{
				Linked[static_cast<std::size_t>(Entry)].push_back(Primitive);
			}
		}

		/** Appends everything linked, in drawing order, and empties the table (keeping its memory). */
		WELTALLCORE_API void TakeInto(std::vector<FMenuPrimitive>& Drawn);

	private:
		std::array<std::vector<FMenuPrimitive>, Entries> Linked;
	};

	/** The gear shop's gear as a frame shows it: drawn or not, and the shop's camera, the view its model is drawn through. */
	struct FMenuGearView
	{
		bool bShown = false;
		FGteMatrix Camera;
	};

	/**
	 * The File screen's two cards, which show two of the player's save stores at a time: whether
	 * they are shown, and which two, for the display's hint of the other pairs (L1 and R1 turn to them).
	 */
	struct FMenuStorePair
	{
		bool bShown = false;
		/** The first of the two stores shown, from 0. */
		int32 FirstStore = 0;
		int32 StoreCount = 0;
	};

	/**
	 * What one frame of a menu draws, in the core's integers, for the renderer: its primitives in the
	 * GPU's drawing order (3D ones with their points and their projection this frame) over the picture
	 * the menu was opened over (the gear shop's: black, its gear drawn first), and the layer's transform
	 * the frame projected with.
	 */
	struct FMenuFrame
	{
		std::vector<FMenuPrimitive> Primitives;
		FMenuLayerTransform Layer;
		/** The gear shop's gear, under everything else the frame draws. */
		FMenuGearView Gear;
		FMenuStorePair Stores;
		/** The keys the frame's pictures from MenuKeyRole up show (the options screen's Controls page), each in its icon set. */
		std::vector<FButtonGlyph> Keys;
		/**
		 * The buttons the frame's lines of text show as glyphs or name in words, as cells of its primitives (by their
		 * places among them): the display draws the player's bindings in their place (FMenuButtonIcons::ApplyToText).
		 */
		std::vector<FTextButtonCell> TextButtons;
		/** The frame's number since the menu began, from 1. */
		uint32 Number = 0;
	};
}
