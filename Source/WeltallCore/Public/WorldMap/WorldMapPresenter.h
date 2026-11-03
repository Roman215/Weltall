#pragma once

#include "Field/FieldGearDrawing.h"
#include "Field/FieldRenderOptions.h"
#include "Foundation/Result.h"
#include "Graphics/Vram.h"
#include "Input/PadState.h"
#include "WorldMap/WorldMapContext.h"
#include "WorldMap/WorldMapDrawing.h"
#include "WorldMap/WorldMapDrawingTables.h"
#include "WorldMap/WorldMapFrame.h"
#include "WorldMap/WorldMapGroundDrawing.h"
#include "WorldMap/WorldMapObjectDrawing.h"

#include <memory>
#include <optional>
#include <span>

namespace Weltall
{
	class FCdImage;
	class FFieldGearStandIns;
	class FGameFileIndex;
	class FWorldMapRuntime;

	/**
	 * The world map's drawing kept beside its runtime: VRAM as the scene leaves it (the models' and the
	 * terrain's textures with their fog palettes, and tick after tick the sprites' frames), the place
	 * models' packets as the scene's start builds them, and each tick's frame for the renderer
	 * (FWorldMapFrame), captured as the tick draws it.
	 */
	class FWorldMapPresenter final : public IWorldMapDrawing
	{
	public:
		/** The disc and its index must outlive the presenter; TextClut is the kernel's two palettes of the message font. */
		WELTALLCORE_API FWorldMapPresenter(const FWorldMapDrawingTables& InTables, const FCdImage& InImage, const FGameFileIndex& InIndex,
			std::span<const uint16, 32> InTextClut);
		WELTALLCORE_API ~FWorldMapPresenter() override;

		/** After the runtime has entered a scene: VRAM as the scene's start leaves it. */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> EnterScene(FWorldMapRuntime& Runtime);

		/**
		 * Runs the runtime's next frame with the buttons held at each of its vertical blanks, and captures
		 * what a tick draws into Frame under Culling (a frame of the pause draws nothing new).
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Tick(FWorldMapRuntime& Runtime, std::span<const FPadState> HeldPerVblank,
			EWorldMapCulling Culling, FWorldMapFrame& Frame);

		/** The player's offsets the frames are drawn under, wherever the game's camera stands on its orbit. */
		void SetPlayerView(const FWorldMapCameraOffsets& Offsets) { PlayerOffsets = Offsets; }

		/**
		 * Where the party's gears are drawn with their 3D models in place of their sprites, as the field draws them
		 * (FFieldGearStandIns): nowhere, in the scenes the player walks, or in the cutscenes' scenes too. Only the
		 * drawing changes: the sprites go on as the game has them, and the models follow them.
		 */
		WELTALLCORE_API void SetGearStandIns(EFieldGearModels Where);
		const FFieldGearStandIns* GearStandIns() const { return GearModels.get(); }

		const FVram& Vram() const { return MapVram; }
		/** Whether the last Tick drew a frame: a tick of the loop does, a frame of the pause does not. */
		bool DrewFrame() const { return bDrewFrame; }
		/** What the texture animations loaded into VRAM as the last pass ended, before its frame is shown. */
		const std::vector<FVramImage>& AnimatedUploads() const { return AnimationUploads; }

		WELTALLCORE_API void DrawParticles(const FWorldMapContext& Context) override;
		WELTALLCORE_API void DrawSprite(const FWorldMapContext& Context, int32 Slot, const FSprite& Sprite, int32 OtIndex) override;
		WELTALLCORE_API void DrawWorld(const FWorldMapContext& Context) override;

	private:
		std::optional<FError> LoadTextures(int32 File, int32 PaletteRow, int32 PaletteRows, int32 Levels, int32 FogRow, int32 FogRows,
			const std::array<uint8, 3>& Fog);
		FWorldMapDrawState DrawState(const FWorldMapContext& Context) const;
		/** Whether a gear's model is drawn for the sprite this tick, following it: the sprite is then not drawn. */
		bool FollowGear(const FWorldMapContext& Context, int32 Slot, const FSprite& Sprite, const FWorldMapDrawState& State, int32 OtIndex);
		/** The followed gears' models posed and drawn into the frame, each whole at its sprite's ordering-table entry. */
		std::optional<FError> DrawGears(const FWorldMapDrawState& State);
		void DrawScreen(const FWorldMapContext& Context, FDrawList& Screen) const;
		std::optional<FError> StartAnimations(std::span<const uint8> Section, std::span<const FWorldMapTextureAnimation> Known, bool bPalettes);
		/** Puts Id's picture in the port's rows when the party's files are made with a member drawn as Id, or a scene is entered. */
		void PlaceIdsPicture(const FWorldMapRuntime& Runtime, bool bEntered);
		/** The sprites' uploads the frame pool queued, as characters' pictures. */
		void FlushSpriteUploads(FWorldMapRuntime& Runtime);
		void StepAnimations();

		/** A message window's text image in VRAM, which the kernel never clears: each strip uploaded whole as a line types. */
		struct FWindowText
		{
			int16 VramY = 0;
			std::vector<uint16> Units;
		};
		/** The message windows as their tasks drew them this tick (the notice's, then the place name's), and the lines they typed. */
		void DrawWindows(const FWorldMapContext& Context, FDrawList& Windows);
		void DrawWindow(const FWorldMapContext& Context, const FMesWindow& Window, FWindowText& Text, FDrawList& Windows);

		/** A texture animation as the loop runs it: its images in the scene's section, the frame it shows and the passes left. */
		struct FTextureAnimation
		{
			const FWorldMapTextureAnimation* Animation = nullptr;
			std::span<const uint8> Images;
			std::size_t ImageBytes = 0;
			std::size_t Frame = 0;
			uint16 PassesLeft = 1;
		};

		FWorldMapDrawingTables Tables;
		const FCdImage* Image = nullptr;
		const FGameFileIndex* Index = nullptr;
		FVram MapVram;
		FVramImage Scratch;
		const FWorldMapScene* Scene = nullptr;
		FWorldMapModelPackets ModelPackets;
		/** The blocks the next frame's billboards are drawn from: this frame's, as the original culls them before the next draws its billboards. */
		FWorldMapBillboardArea BillboardArea;
		/** The frame the running tick draws into. */
		FWorldMapFrame* Drawn = nullptr;
		EWorldMapCulling DrawnCulling = EWorldMapCulling::Original;
		FWorldMapCameraOffsets PlayerOffsets;
		FWorldMapFrameBuilder Builder;
		std::vector<FTextureAnimation> Animations;
		std::vector<FVramImage> AnimationUploads;
		std::array<uint16, 32> TextClut{};
		FWindowText PlaceNameText;
		FWindowText NoticeText;
		uint32 Ticks = 0;
		bool bDrewFrame = false;
		/** The party's files Id's picture was last placed for (FWorldMapRuntime::PartyLoads). */
		uint32 PartyLoadsPlaced = 0;

		/** A party slot's gear as its sprite was last followed: where its task stood, and the entry its sprite was linked into. */
		struct FGearPlace
		{
			bool bKnown = false;
			FWorldMapVector At;
			int32 Slot = -1;
			int32 OtIndex = 0;
			bool bFollowed = false;
		};
		std::unique_ptr<FFieldGearStandIns> GearModels;
		bool bGearModelsInCutscenes = false;
		/** Whether this tick's drawing has begun for the gears' models. */
		bool bGearsBegun = false;
		std::array<FGearPlace, 3> GearPlaces;
		FFieldGearsFrame GearsFrame;
		std::vector<FGteProjection> GearProjected;
		std::optional<FError> GearError;
	};
}
