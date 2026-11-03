#pragma once

#include "Field/FieldCharacters.h"
#include "Field/FieldCutscenes.h"
#include "Field/FieldDrawingTables.h"
#include "Field/FieldFrame.h"
#include "Field/FieldGearStandIns.h"
#include "Field/FieldMemberStandIns.h"
#include "Field/FieldParticles.h"
#include "Field/FieldPartyWatch.h"
#include "Field/FieldPilotStandIns.h"
#include "Field/FieldPlayerCamera.h"
#include "Field/FieldRenderOptions.h"
#include "Field/FieldRequests.h"
#include "Field/FieldScene.h"
#include "Field/FieldSceneFrame.h"
#include "Field/FieldScreenInVram.h"
#include "Field/FieldSpriteModels.h"
#include "Foundation/Result.h"
#include "Graphics/SpriteDrawing.h"
#include "Graphics/TextureRegion.h"
#include "Graphics/Vram.h"
#include "Input/PadState.h"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FFieldRuntime;
	class FGameFileIndex;
	struct FEventContext;
	struct FFieldMapPicture;
	struct FFieldRenderState;
	struct FFieldVramOperation;
	struct FMdecPicture;
	struct FTim;

	/**
	 * The field's drawing kept beside its runtime: VRAM as the field leaves it (its system images, the
	 * message font's CLUTs, the map's textures, and tick after tick the scripts' VRAM changes, the
	 * faces the messages load, the windows' text and the sprites' frames), the map's scenery, and each
	 * tick's frame for the renderer (FFieldFrame), captured as the tick draws it.
	 */
	class FFieldPresenter final : public IFieldCharacterDrawing
	{
	public:
		/** The field's start: its system images and their CLUTs in VRAM. The disc and its index must outlive the presenter. */
		WELTALLCORE_API static TResult<FFieldPresenter> Start(const FFieldDrawingTables& Tables, const FCdImage& Image, const FGameFileIndex& Index);

		/** After the runtime has entered a map: VRAM as the map's entry leaves it, and the map's scenery. */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> EnterMap(FFieldRuntime& Runtime);

		/**
		 * Runs the runtime's next tick with the buttons held at each of its vertical blanks, and captures
		 * what it draws into Frame, the scenery evaluated under Culling: as the game's camera sees it, or
		 * with Player the view the player's camera presents (the scenery, the characters and their
		 * shadows, the 2D layer staying the game's).
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Tick(FFieldRuntime& Runtime, std::span<const FPadState> HeldPerVblank, EFieldCulling Culling,
			FFieldFrame& Frame, FFieldPlayerCamera* Player = nullptr);

		/**
		 * Whether the party's members on foot are drawn with their own sprites for their pilots while the party is in gears
		 * (FFieldPilotStandIns), from the next tick: off, the pilots are the map's, as the original draws them.
		 */
		WELTALLCORE_API void SetPilotStandIns(bool bOn);
		/** The stand-ins, while they are drawn. */
		const FFieldPilotStandIns* PilotStandIns() const { return StandIns.get(); }

		/**
		 * Where the gears the field draws as sprites are drawn with their battle models instead (FFieldGearStandIns), from
		 * the next tick: off, they are the sprites, as the original draws them; outside cutscenes, the sprites while
		 * FFieldCutscenes says one plays.
		 */
		WELTALLCORE_API void SetGearStandIns(EFieldGearModels Where);
		const FFieldGearStandIns* GearStandIns() const { return GearModels.get(); }
		/** Whether the field plays a cutscene, as the last tick drew its sprites. */
		const FFieldCutscenes& Cutscenes() const { return CutsceneWatch; }

		/** Whether the party walks as one behind its leader, as the last tick drew its sprites. */
		const FFieldPartyWatch& Party() const { return PartyWatch; }

		/**
		 * The party's slots drawn as the members the player's order shows in them (FFieldMemberStandIns): outside
		 * cutscenes, and only while the whole party follows its leader (FFieldPartyWatch).
		 */
		const FFieldMemberStandIns& MemberStandIns() const { return *Members; }

		/**
		 * What the gears a frame draws lay out, in its order: the map's own, then the party's stand-ins; and the
		 * number that changes whenever the layout does (FFieldGearsFrame::Layout).
		 */
		WELTALLCORE_API void LayOutGears(const FEventContext& Context, std::vector<FFieldGearLayoutItem>& Items) const;
		WELTALLCORE_API uint32 GearLayout(const FEventContext& Context) const;

		const FVram& Vram() const { return FieldVram; }
		const FFieldScene& Scene() const { return *MapScene; }
		/** The sheets the map's resident sprites are drawn from (FieldSpriteSheetRegions). */
		std::span<const FTextureRegion> SpriteSheets() const { return MapSpriteSheets; }
		const FFieldDrawingTables& Tables() const { return DrawingTables; }

		/**
		 * Whether the last tick took a picture of the screen (FFieldCover): it is the frame drawn before
		 * that tick, with VRAM as PictureVram holds it and the scenery PictureScene (the map before a map
		 * change), both kept until the next tick.
		 */
		bool TookPicture() const { return bTookPicture; }
		const FVram& PictureVram() const { return VramBeforeTick; }
		const FFieldScene& PictureScene() const { return SceneBeforeTick ? *SceneBeforeTick : *MapScene; }

		/**
		 * A picture of the movie the field plays in the scene, as its player decodes it into VRAM for the
		 * map's models: at the request's place for its Number-th picture (its two buffers in turn), 15-bit.
		 * Returns the rectangle it wrote.
		 */
		WELTALLCORE_API FVramRect UploadMoviePicture(const FMdecPicture& Picture, const FFieldMovieRequest& Movie, uint32 Number);

		WELTALLCORE_API void DrawSprite(int32 Actor, const FSprite& Sprite, const FFieldSpritePass& Pass) override;
		WELTALLCORE_API void DrawSpriteModel(const FFieldSpriteModelDraw& Draw) override;

	private:
		FFieldPresenter(const FFieldDrawingTables& InTables, const FCdImage& InImage, const FGameFileIndex& InIndex);

		std::optional<FError> PlaceTim(const FTim& Tim, const FFieldImagePlace& Place);
		TResult<FTim> ReadTimFile(int32 File) const;
		std::optional<FError> LoadImage(int32 File, const FFieldImagePlace& Place);
		void ApplyVramOperations(const FFieldRenderState& Render, std::span<const FFieldVramOperation> Operations);
		void ApplyVramOperation(const FFieldRenderState& Render, const FFieldVramOperation& Operation);
		/** The rectangles holding the screen's pixels less what VRAM's uploads have written over since. */
		void WriteOverScreen();
		/** Units of Rect as VRAM holds them now, row by row, into Into. */
		void ReadVram(const FVramRect& Rect, FVramImage& Into) const;
		std::optional<FError> LoadFaces(const FEventContext& Context);
		/** The sprites' uploads the frame pool queued, as characters' pictures. */
		void FlushSpriteUploads(const FEventContext& Context);
		void CaptureScenery(const FEventContext& Context, EFieldCulling Culling, const FFieldPresentedView& Shown, FFieldFrame& Frame);
		/** The characters' passes, drawn with the game's view From, placed as the view To shows them. */
		void MoveCharacters(const FGteMatrix& From, const FGteMatrix& To, FFieldFrame& Frame);
		void CaptureShadows(const FFieldRuntime& Runtime, const FGteMatrix& View, FFieldFrame& Frame);
		std::optional<FError> CaptureParticles(const FEventContext& Context, const FFieldView& View, FFieldFrame& Frame);
		void CaptureScreen(const FFieldRuntime& Runtime, const FFieldPresentedView& Shown, FFieldFrame& Frame);
		/** Where the windows opened since the last frame are shown under the view the player sees. */
		void PlaceWindowsForView(const FEventContext& Context, const FFieldPresentedView& Shown);
		std::optional<FError> CaptureGears(const FFieldRuntime& Runtime, EFieldCulling Culling, const FFieldPresentedView& Shown, FFieldFrame& Frame);
		/** The texture blocks the gears' loads placed and the copies their scripts made since the tick began, in order. */
		void ApplyGearVramChanges(const FEventContext& Context);
		/** The running map's texture stream into VRAM, as its load and every movie shown over the screen bring it in. */
		void UploadMapTextureStream(FFieldRuntime& Runtime);
		/** The stream again when the runtime has brought it in again since the last frame (after a movie). */
		void FollowTextureStreams(FFieldRuntime& Runtime);
		void OrderFrame(FFieldFrame& Frame);
		/** Select's picture into VRAM as it comes up (the area it takes kept aside), and VRAM given back once it has gone. */
		std::optional<FError> FollowMapPicture(const FFieldMapPicture& Picture);

		FFieldDrawingTables DrawingTables;
		const FCdImage* Image = nullptr;
		const FGameFileIndex* Index = nullptr;
		FVram FieldVram;
		FVramImage Scratch;
		/** The effects' textures the field keeps aside while its particles stand still. */
		FVramImage SavedArea;
		/** The CLUT rows the scripts fade: as kept aside, and the working copy the fades write. */
		FVramImage ClutRowsKept;
		FVramImage ClutRowsFaded;
		std::optional<FFieldScene> MapScene;
		std::vector<FTextureRegion> MapSpriteSheets;
		/** For the picture of the screen: VRAM and the scenery as the frame before the last tick drew them. */
		bool bTookPicture = false;
		FVram VramBeforeTick;
		std::optional<FFieldScene> SceneBeforeTick;
		/** What Select's picture takes of VRAM while it is up. */
		FVramImage MapPictureArea;
		bool bMapPictureUp = false;
		std::vector<uint32> SortedParticles;
		/** The face read whose faces each face slot's place in VRAM holds (0: none). */
		std::array<uint32, FFieldDrawingTables::FaceSlots> FacesLoaded{};
		/** The frame the running tick draws into, and its drawing's scratch. */
		FFieldFrame* Drawn = nullptr;
		std::vector<FSpriteQuad> Parts;
		std::vector<uint32> Sorted;
		std::vector<uint32> GearsSorted;
		std::vector<uint32> SpriteModelsSorted;
		/**
		 * Each message window's opening count when its place for the player's view was taken, and how
		 * far it is shown from where the game placed it.
		 */
		std::array<uint16, 4> WindowOpenings{};
		std::array<bool, 4> bWindowPlaced{};
		std::array<std::array<int16, 2>, 4> WindowShifts{};
		/** Where VRAM holds the screen's pixels, and scratch for those a copy carries. */
		std::vector<FFieldScreenInVram> ScreenInVram;
		std::vector<FFieldScreenInVram> ScreenCarried;
		/**
		 * The runtime whose tick is running; the children's parts built for its map (the count of map
		 * loads they were built at); the children's models the running tick draws, those the frames
		 * show (the last tick's, still through the frames that run none), and a part that could not be
		 * built.
		 */
		const FFieldRuntime* TickingRuntime = nullptr;
		FFieldSpriteModelParts SpriteModelParts;
		int32 SpriteModelLoads = -1;
		std::vector<FFieldSpriteModelDrawn> SpriteModelCapture;
		std::vector<FFieldSpriteModelDrawn> SpriteModelsShown;
		std::optional<FError> SpriteModelError;
		/** Each character's move into the player's view: x, y, z and the ordering-table entries. */
		std::vector<std::array<int32, 4>> CharacterMoves;
		FFieldPresentedView GameView;
		/** The characters the last tick drew, which the frames that run no tick (the pause, fades, the menu's shading) show still. */
		std::vector<FFieldCharacterView> HeldCharacters;
		std::vector<FFieldCharacterQuad> HeldQuads;
		/** The party's own sprites for their pilots, while the player has them drawn. */
		std::unique_ptr<FFieldPilotStandIns> StandIns;
		/** The gears' models for their sprites, while the player has them drawn; and whether in cutscenes too. */
		std::unique_ptr<FFieldGearStandIns> GearModels;
		bool bGearModelsInCutscenes = false;
		/** The party in its player's order; a pointer, so that the presenter moves. */
		std::unique_ptr<FFieldMemberStandIns> Members;
		/**
		 * Each particle effect as last captured: ended, drawn, or left out for its whole life as the thrusters of a dash
		 * the gear it is placed on runs on foot instead.
		 */
		enum class EEffectLook : uint8
		{
			Ended,
			Drawn,
			LeftOut,
		};
		std::array<EEffectLook, FFieldParticles::EffectCount> EffectLooks{};
		/**
		 * How far each effect is drawn from where the game places it, in view units: a dashing gear's thrusters moved from
		 * its sprite's back onto its model's, the last such move kept for the rest of the effect's life.
		 */
		std::array<std::array<int32, 3>, FFieldParticles::EffectCount> EffectShifts{};
		/** The move a gear's model makes of its thrusters this frame, if the effect is a model's; Frame's gears already captured. */
		std::optional<std::array<int32, 3>> ThrusterShift(const FFieldParticleEffect& Running, const FFieldView& Shown, const FFieldFrame& Frame) const;
		FFieldCutscenes CutsceneWatch;
		FFieldPartyWatch PartyWatch;
		bool bCutsceneWatched = false;
		/** The compass's sixteen colours as the system images leave them, and its quads as the view last drew them. */
		std::array<uint16, 16> CompassColors{};
		FFieldCompassFrame CompassFrame;
		uint32 Ticks = 0;
		/**
		 * The runtime's count of map loads the scenery was built for, of gears' modules whose textures VRAM holds, and of
		 * the map's texture streams since its load that VRAM holds.
		 */
		int32 MapLoadsSeen = 0;
		int32 GearLoadsSeen = 0;
		int32 TextureStreamsSeen = 0;
	};
}
