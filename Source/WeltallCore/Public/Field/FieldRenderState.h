#pragma once

#include "Field/FieldCompass.h"
#include "Field/FieldScreenOverlays.h"
#include "Field/FieldScrolls.h"
#include "Field/FieldVramChanges.h"
#include "Foundation/Types.h"
#include "Graphics/MdlModel.h"
#include "Graphics/Vram.h"

#include <array>
#include <vector>

namespace Weltall
{
	/** A full-screen colour layer fading toward a colour over a number of ticks: 8.8 channels, a GPU blend mode. */
	struct FFieldFade
	{
		std::array<int32, 3> Color{};
		std::array<int32, 3> Step{};
		/** The GPU's semi-transparency mode it is drawn with: 50/50, additive, subtractive, a quarter added. */
		uint16 BlendMode = 2;
		bool bActive = false;
		int16 Ticks = 0;

		/**
		 * Starts moving toward Target (0 to 255 a channel) over Ticks from the colour it has, dividing
		 * as the processor does: no ticks gives steps of -1 or 1 and a layer that never moves.
		 */
		WELTALLCORE_API void Start(const std::array<int32, 3>& Target, int32 Ticks, uint16 Mode);
		/** The colour it is drawn with: each channel's whole part. */
		WELTALLCORE_API FMdlColor DrawnColor() const;
		/** As every map's load leaves it: black, off, subtractive; its steps are kept. */
		WELTALLCORE_API void Reset();
	};

	/** Depth-cued fog: models and sprites fade from their colour at Near toward FarColor at Far. */
	struct FFieldFog
	{
		bool bOn = false;
		/** The colour depth-cued polygons start from. */
		std::array<uint8, 3> Color{0x80, 0x80, 0x80};
		std::array<uint8, 3> FarColor{0xFF, 0xFF, 0xFF};
		int16 Near = 0x15E0;
		int16 Far = 0x300C;
	};

	/**
	 * A horizon backdrop drawn behind the scene: a wrapping textured strip with flat and graded colour
	 * bands, made after the map's init when the scripts asked for one. The fields are as the kernel's
	 * panorama takes them.
	 */
	struct FFieldPanorama
	{
		bool bEnabled = false;
		/** After a movie or a menu the scripts asked to leave the screen dark (FE 86), the backdrop is not drawn until the next map. */
		bool bHidden = false;
		int16 VramX = 0;
		int16 VramY = 0;
		/** Never 0: the scripts' 0 becomes 1. */
		int16 Width = 0x140;
		int16 Height = 0;
		int16 ClutX = 0;
		int16 ClutY = 0;
		int16 TexturePage = 0;
		int16 Scroll = 0;
		/**
		 * The horizon's point: x (unused), its height, and its distance ahead of the camera's target along
		 * the view (a negative one reverses the scroll).
		 */
		std::array<int32, 3> Position{0, 0, 0x1000};
		/** The flat band, and the graded band's top and bottom. */
		std::array<std::array<uint8, 3>, 3> Colors{{{}, {}, {0, 0, 0x20}}};
		/** Where the graded band ends: the horizon's point scaled by this (8.8). */
		int16 BandScale = 0;
		/** How the strip fades with the camera's distance: a divisor (0: no fade) and an offset. */
		int16 DepthDivisor = 0;
		int16 DepthOffset = 0;
		/** The colour bands above and below the strip, which a panorama made without colours leaves out (a battle stage's first kind). */
		bool bBands = true;
	};

	/**
	 * The lights of the gears' 3D models: the light matrix (row i is light i's direction) then the
	 * colour matrix (column i is light i's colour), 4.12, each laid out as the GTE's matrices are
	 * kept: nine entries, a pad and a translation the lights leave alone. Scripts name a light
	 * unchecked, and a light past the third lands in the next rows, the pad or the other matrix.
	 */
	struct FFieldGearLights
	{
		static constexpr int32 MatrixWords = 16;
		static constexpr int32 PointLights = 3;

		/**
		 * A point light (FE 88, FE 89): while it follows an actor, every tick the gears are drawn it
		 * sets its directional light's colour to (Near - Far) x (Radius - the actor's distance from
		 * Position) / Radius, per channel: Near - Far at the point, nothing at Radius, and below
		 * nothing past it (the original neither adds Far nor stops at Radius).
		 */
		struct FPointLight
		{
			static constexpr int16 Off = -1;

			/** The actor whose distance fades it; Off: the light stays as it is. */
			int16 Actor = Off;
			std::array<int16, 3> Near{};
			std::array<int16, 3> Far{};
			int32 Radius = 0;
			/** x, y, z, whole units. */
			std::array<int16, 3> Position{};
		};

		/** As every map's load sets them: light 0 white at half strength, from above; the others dark. */
		std::array<int16, 2 * MatrixWords> Words{0x1F8, -0xFC1, -0x1F8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0x800, 0, 0, 0x800, 0, 0, 0x800};
		std::array<uint8, 3> Ambient{0x1E, 0x1E, 0x1E};
		std::array<FPointLight, PointLights> Points;

		/** The word of light Light's direction on Axis, or of its colour's Channel. */
		static constexpr int32 DirectionWord(int32 Light, int32 Axis) { return 3 * Light + Axis; }
		static constexpr int32 ColorWord(int32 Light, int32 Channel) { return MatrixWords + 3 * Channel + Light; }
	};

	namespace FieldSceneryDrift
	{
		/** The scroll mode: whether the scenery (and, in mode 1, free script models) drifts each tick. */
		constexpr uint8 ModeMask = 0x7F;
		/** The drifting models are drawn even where the culling finds them off screen. */
		constexpr uint8 DrawnOffScreen = 0x80;
	}

	/**
	 * The picture the field takes of its screen to change maps or dissolve through, drawn over the
	 * frame in five 64-pixel columns scaled about the screen's centre: the last frame shown before
	 * it was taken, zooming and fading away as the next map comes in, or fading in and held as a
	 * still. Each draw buffer's columns keep their own brightness; the tick draws its buffer's.
	 * The columns are VRAM's picture area, which a picture taken of the screen fills with the
	 * frame shown last and which the scripts can fill with a still of their own.
	 */
	struct FFieldCover
	{
		static constexpr uint8 FullBrightness = 0x80;
		static constexpr int32 UnitScale = 0x1000;
		/** Where the columns lie in VRAM: five 15-bit pages' 64 units each, as wide and high as the screen. */
		static constexpr FVramRect PictureArea{0x2C0, 0x100, 0x140, 0xE0};

		/** Drawn over this frame. */
		bool bShown = false;
		/** Drawn semi-transparent, added to the frame, rather than opaque. */
		bool bAdditive = false;
		/**
		 * The picture area holds the screen as it was last shown when the picture was taken; otherwise
		 * what the scripts put there (and the picture is drawn from VRAM).
		 */
		bool bScreen = true;
		std::array<uint8, 2> Brightness{FullBrightness, FullBrightness};
		/** 4.12; UnitScale covers the screen exactly. */
		int32 Scale = UnitScale;
		/** Counts the pictures taken of the screen: a new count means the renderer takes the frame it showed last as the picture. */
		int32 Taken = 0;

		/**
		 * Sets the picture up anew, both buffers' columns at full brightness and unscaled, from the
		 * screen copied into the picture area first, or from the area as it stands.
		 */
		void Take(bool bInAdditive, bool bCopiesScreen)
		{
			bAdditive = bInAdditive;
			Brightness = {FullBrightness, FullBrightness};
			Scale = UnitScale;
			if (bCopiesScreen)
			{
				bScreen = true;
				++Taken;
			}
		}
	};

	/**
	 * The picture of the map Select shows on a few maps while the party holds the map's item, alone on
	 * a black screen while the field stands still: the picture file (8-bit, 320 pixels wide, its
	 * palette with it, semi-transparent where the palette says) drawn from the screen's top left, 223
	 * lines of it, the parts of it the party has not yet seen drawn over from the parts' file, and the
	 * leader's marker, a 16-pixel sprite; both at Brightness.
	 */
	struct FFieldMapPicture
	{
		static constexpr int32 Directory = 4;
		/** The file whose pixels hide the parts not yet seen, in Directory. */
		static constexpr int32 PartsFile = 0x802;
		static constexpr uint8 FullBrightness = 0x80;

		/** A part the parts' file can hide: a rectangle of the picture, in its pixels. */
		struct FPart
		{
			int16 X, Y, Width, Height;
		};
		static constexpr std::array<FPart, 5> Parts = {{{152, 70, 32, 32}, {168, 140, 44, 44}, {120, 72, 24, 28}, {112, 92, 36, 32}, {188, 50, 44, 44}}};

		/** Drawn this frame. */
		bool bShown = false;
		/** The picture's file, in Directory. */
		int32 File = 0;
		/** Bit k: Parts[k] is drawn over. */
		uint8 HiddenParts = 0;
		/** The marker's top left on the screen. */
		int16 MarkerX = 0;
		int16 MarkerY = 0;
		uint8 Brightness = 0;
	};

	/**
	 * The field's last picture, standing still under a subtractive grey, while the menu the field runs
	 * opens over it and after it closes: the frame shown before the menu, darker by Level x 4 in each
	 * channel (up to 124 before the menu, 248 for a return the scripts keep dark).
	 */
	struct FFieldMenuShade
	{
		/** The blend key of the shading's tile in the field's 2D layer: the picture under the menu is the frame without it. */
		static constexpr uint32 BlendKey = 3;
		/** How much grey each level takes from the picture. */
		static constexpr int32 GreyPerLevel = 4;

		/** Drawn over this frame. */
		bool bShown = false;
		uint8 Level = 0;
	};

	/** The render state a map starts with, which scripts change. */
	struct FFieldRenderState
	{
		/** The color the frame is cleared to. */
		FMdlColor BackgroundColor{};
		/**
		 * The rectangle of the 320 x 224 screen the frame's drawing is clipped to (the GPU's drawing area),
		 * which a map's load resets to the whole screen; the clear colour always fills it all.
		 */
		FVramRect DrawClip{0, 0, 0x140, 0xE0};
		/** The farthest ordering-table entry drawn: depth / 16 beyond it is never drawn. */
		uint16 FarBucket = 0x720;
		/** Margins beyond the screen within which a placed model's bounds still count as on it. */
		int32 CullMarginX = 0;
		int32 CullMarginY = 0;
		FFieldFog Fog;
		FFieldPanorama Panorama;
		/**
		 * The panorama the map made after its init from the parameters its scripts left (bEnabled:
		 * made): what the frames draw. The scripts' later parameters change nothing drawn.
		 */
		FFieldPanorama Backdrop;
		/**
		 * After a lost battle, until the next map or a movie shown over the screen: the defeat's picture is where the
		 * title's backdrop reads its own.
		 */
		bool bDefeatPicture = false;
		/**
		 * How often the map's texture stream has come in again since its load: after every movie shown over the screen,
		 * over whatever the scripts and the movie left in its places.
		 */
		int32 TextureStreams = 0;
		FFieldGearLights GearLights;
		/** FieldSceneryDrift bits. */
		uint8 SceneryDrift = 0;
		/** How far the drifting models move each tick (x, y, z), and how far they have moved since the map's load. */
		std::array<int16, 3> DriftStep{};
		std::array<int32, 3> DriftTotal{};
		/** The screen's colour layers: 0 the map's fades, 1 the scripts'. */
		std::array<FFieldFade, 2> Fades{};
		/** The map's fade-out holds (every map starts so): a finished black layer stays on. */
		bool bScreenFadedOut = true;
		FFieldCover Cover;
		FFieldMapPicture MapPicture;
		FFieldMenuShade MenuShade;
		/** The compass: the Field program keeps its needle from map to map. */
		FFieldCompass Compass;
		/**
		 * FE DF: the display 640 pixels across (every 2D piece's x then counts half a pixel of the 320 x
		 * 224 screen; nothing switches it back but the field's next entry), and the still of 640 x 224
		 * shown (HiResStill*).
		 */
		bool bHiResDisplay = false;
		bool bHiResStill = false;
		/** The still: five 128-pixel columns of an 8-bit page each, the first at this place, and its CLUT. */
		static constexpr int16 HiResStillX = 0x280;
		static constexpr int16 HiResStillClutY = 0xE8;
		/**
		 * Copies of the picture of the screen the field makes after a map's textures have loaded, in
		 * order: transition 5's move of the picture's area, the entry after a lost battle's copy of the
		 * draw buffer that shows it. Cleared as a tick begins.
		 */
		std::vector<FFieldVramOperation> PictureMoves;
		/**
		 * The VRAM changes the scripts asked for since the tick began (the map's load's, before the
		 * first tick), in order: the renderer makes them before the tick's frame uploads.
		 */
		std::vector<FFieldVramOperation> VramOperations;
		/** The units the tick's Upload operations load, each operation's from its UnitsAt. */
		std::vector<uint16> VramUnits;

		// The scripts' scrolling and wave effects (DA, FE 40, FE 1B, FE 26, FE 27).
		static constexpr std::size_t MaxLineScrolls = 32;
		/** The VRAM scrollers the scripts made, in order. */
		std::vector<FFieldLineScroll> LineScrolls;
		/**
		 * The copies the scrollers made this tick, in order: the renderer makes them after the tick's
		 * frame uploads (the sprites') and before the frame is drawn.
		 */
		std::vector<FFieldVramOperation> LineScrollMoves;
		/** The actors whose models' texture coordinates the scripts scrolled. */
		std::vector<FFieldModelUvScroll> ModelUvScrolls;
		FFieldScreenWave ScreenWave;

		// The scripts' images, the VRAM they keep aside and their overlays (FE 77, FE A3, FE DD, FE D4, FE B1).
		/** The effects' textures (the particles' and the screen markers'), which large images overwrite. */
		static constexpr FVramRect EffectTextures{0x3C0, 0x100, 0x40, 0x100};
		/**
		 * The effects' textures are kept aside (SaveArea) until they are put back: the particles stand
		 * still and the field is busy meanwhile (its main loop takes up no request). A map's entry keeps
		 * them aside until its last tick, a map change and a dissolve while they run (the picture of the
		 * screen overwrites them); the scripts keep them aside around large images.
		 */
		bool bEffectTexturesSaved = false;
		FFieldClutFade ClutFade;
		FFieldScreenMarkers Markers;
		FFieldCockpitHud CockpitHud;

		/** A tick begins: the changes and marks left for the last one's frame are gone. */
		WELTALLCORE_API void BeginTick();
		/** Queues an image's upload into VRAM at its rectangle. */
		WELTALLCORE_API void Upload(const FVramImage& Image);
		/** The scripts write Rect of VRAM: the picture area, when it overlaps, holds their texels rather than the screen. */
		WELTALLCORE_API void WroteVram(const FVramRect& Rect);
		/** Keeps the effects' textures aside, unless they are. */
		WELTALLCORE_API void SaveEffectTextures();
		/** Puts the effects' textures back, when they are kept aside. */
		WELTALLCORE_API void RestoreEffectTextures();
		/** The map's texture stream comes in again, the title's own picture with it where the defeat's was. */
		WELTALLCORE_API void StreamTexturesAgain();

		/**
		 * One frame of the fade layers, as the field runs them before drawing: each active layer steps
		 * toward its colour while it has ticks left (a channel clamped to 0 to 255), then the drawing
		 * switches off a layer drawn black and clears its ticks. bRisingFromBlack (the fix) keeps a
		 * layer on while it still moves up toward a colour, which the original switches off on its
		 * first frame when every channel moves less than a unit a frame.
		 */
		WELTALLCORE_API void TickFades(bool bRisingFromBlack);

		/**
		 * A fade-in, when the screen is faded out (a map's entry, B3): the map's layer fades from white,
		 * subtractive (the screen black), to nothing over Ticks ticks. No ticks divides as the
		 * processor does, and the layer then stays up.
		 */
		WELTALLCORE_API void StartFadeIn(int32 Ticks);
		/**
		 * A fade-out (B4), unless the screen is faded out: the map's layer fades from nothing to white,
		 * subtractive, over Ticks ticks. With no ticks the layer is switched off at its first drawing.
		 */
		WELTALLCORE_API void StartFadeOut(int32 Ticks);
		/**
		 * A map change's flash (transition 1): the map's layer adds white over Ticks + 1 ticks, from
		 * nothing (WhiteOut) or from full white (WhiteIn), whether the screen is faded out or not.
		 */
		WELTALLCORE_API void StartWhiteOut(int32 Ticks);
		WELTALLCORE_API void StartWhiteIn(int32 Ticks);

		/**
		 * One tick of the scrollers, as the field runs it before drawing its frame: every band moves,
		 * and LineScrollMoves holds the tick's copies. bWithinWidth (the fix) keeps each band's offset
		 * within its scroller's width; the original's wraps through 16 bits.
		 */
		WELTALLCORE_API void TickLineScrolls(bool bWithinWidth);
		/** Scrolls the actor's model's texture coordinates further. */
		WELTALLCORE_API void ScrollModelUv(uint16 Actor, FMdlUv By);
		/** How far the actor's model's texture coordinates are scrolled: each corner samples (u + U, v + V) modulo 256. */
		WELTALLCORE_API FMdlUv ModelUvScroll(uint16 Actor) const;
	};
}
