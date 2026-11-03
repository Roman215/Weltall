#pragma once

#include "Field/FieldPlayerCamera.h"
#include "Foundation/Result.h"
#include "Game/GameProgram.h"
#include "WorldMap/WorldMapDrawingTables.h"
#include "WorldMap/WorldMapFrame.h"
#include "WorldMap/WorldMapPresenter.h"
#include "WorldMap/WorldMapRuntime.h"
#include "WorldMap/WorldMapTables.h"

#include <array>
#include <memory>
#include <optional>
#include <span>

namespace Weltall
{
	class FGameSession;

	/**
	 * What a frame of one of the program's own fades (the entry's, the menu's) shows: a picture darkened, the GPU
	 * subtracting Level from each of its channels (0-255), or brightened, adding it (scene 9's entry).
	 */
	struct FWorldMapScreenFade
	{
		enum class EPicture : uint8
		{
			/** Not a frame of a fade: the map's own frames. */
			None,
			/** The screen before the world map started. */
			ScreenBefore,
			/** The map's latest frame. */
			LatestFrame,
		};

		EPicture Picture = EPicture::None;
		/** -1 where the frame drew nothing yet: the display keeps showing what it showed. */
		int32 Level = -1;
		/** The GPU's blend the grey is drawn with: 2 subtracts it (to black), 1 adds it (to white). */
		int32 Blend = 2;
	};

	/**
	 * The world map program as the kernel runs it: its start (the screen before faded to black, then the
	 * scene the game state names entered, or the one a battle left, from what the kernel kept of it), the
	 * loop's ticks and the pause's frames with their drawing (the last two frames kept for the display to
	 * draw between), the menu it runs inside itself between two fades, and its way out to the field or a
	 * battle, with what it hands over in the kernel's globals and the game state.
	 */
	class FWorldMapProgram final : public IGameProgram
	{
	public:
		/** The fade to black a scene's start shows over the screen before it, a frame a vertical blank. */
		static constexpr int32 EntryFadeFrames = FWorldMapRuntime::EntryFadeVblanks;
		/** The menu's fades, a frame a vertical blank: to black before it opens, and back after it closes (with one more). */
		static constexpr int32 MenuFadeOutFrames = 18;
		static constexpr int32 MenuFadeInFrames = 19;

		/** The program started as the kernel runs it. The session must outlive it. */
		WELTALLCORE_API static TResult<std::unique_ptr<FWorldMapProgram>> Start(FGameSession& Session);
		WELTALLCORE_API ~FWorldMapProgram() override;
		FWorldMapProgram(const FWorldMapProgram&) = delete;
		FWorldMapProgram& operator=(const FWorldMapProgram&) = delete;

		WELTALLCORE_API int32 VblanksPerFrame() const override;
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Frame(std::span<const FPadState> HeldPerVblank) override;
		EGameProgramState State() const override { return ProgramState; }

		const FWorldMapRuntime& Runtime() const { return *MapRuntime; }
		FWorldMapRuntime& Runtime() { return *MapRuntime; }
		const FWorldMapPresenter& Drawing() const { return *Presenter; }

		/** How the frames are culled: the original's rules or everything in range. */
		void SetCulling(EWorldMapCulling InCulling) { Culling = InCulling; }

		/**
		 * The player's moves of the view since the last tick: the next tick's frame is drawn under them, while the map
		 * plays (not in a cutscene, the pause or the menu). The game's camera keeps its own place.
		 */
		WELTALLCORE_API void AddViewInput(const FFieldViewInput& Input);
		/** The player's offsets as they stand now, the moves since the last tick among them: what a display between ticks shows. */
		WELTALLCORE_API FWorldMapCameraOffsets LiveViewOffsets() const;

		/** The latest frame the world map drew, null before the first. */
		WELTALLCORE_API const FWorldMapFrame* LatestFrame() const;
		/** The frame before the latest, while the last frame of the program drew the latest and the two are consecutive ticks' of the loop; null otherwise. */
		WELTALLCORE_API const FWorldMapFrame* PreviousFrame() const;
		/** The frames of a fade the program shows rather than the map, and whether it is going to black (the entry's, the menu's first) or back. */
		int32 FadeFramesLeft() const { return FramesLeft; }
		bool IsFadingOut() const { return Phase == EPhase::EntryFade || Phase == EPhase::MenuFadeOut; }
		/** What the last frame showed, when it was a frame of a fade. */
		const FWorldMapScreenFade& ScreenFade() const { return LastFade; }
		/** Whether the map's own ticks run: the program running, past its entry's fade, and not its menu or that menu's fades. */
		bool TicksMap() const { return ProgramState == EGameProgramState::Running && Phase == EPhase::Loop; }

	private:
		enum class EPhase : uint8
		{
			EntryFade,
			Loop,
			MenuFadeOut,
			Menu,
			MenuFadeIn,
		};

		FWorldMapProgram(FGameSession& InSession, FWorldMapTables InTables, FWorldMapDrawingTables InDrawingTables);
		[[nodiscard]] std::optional<FError> RunFrame(std::span<const FPadState> HeldPerVblank);
		/** The chapter's title card through a frame of Vblanks: a new chapter's begins once the player has control of the map. */
		void AdvanceChapterCard(int32 Vblanks);
		void RunVblanks(std::span<const FPadState> HeldPerVblank);
		std::optional<FError> TakeRequest();
		std::optional<FError> OpenMenu();
		std::optional<FError> Leave(const FWorldMapRequest& Request);
		/** A game the menu loaded goes on from its save's place, the world map's song stopped, as the title's map sends a loaded game on. */
		std::optional<FError> GoOnWithLoadedGame();
		/** What the frame about to run shows, as its phase and the frames left say. */
		FWorldMapScreenFade FadeShown() const;

		FGameSession& Session;
		FWorldMapTables Tables;
		FWorldMapDrawingTables DrawingTables;
		std::optional<FWorldMapPresenter> Presenter;
		EWorldMapCulling Culling = EWorldMapCulling::Original;
		std::array<FWorldMapFrame, 2> Frames;
		int32 Latest = -1;
		bool bPreviousFollows = false;
		FFieldViewInput PendingView;
		/** Whether the last tick drew the map under the player's view: its moves are taken. */
		bool bViewMoves = true;
		/** What the last battle kept, which the runtime restores from. */
		std::shared_ptr<const FWorldMapBattleKeep> Keep;
		std::unique_ptr<FWorldMapRuntime> MapRuntime;
		/** The menu the world map runs inside itself, while it runs. */
		std::unique_ptr<IGameProgram> Menu;
		/** The result the program before left, set aside while the menu runs: the menu leaves its own for a game it loaded. */
		uint8 ResultBeforeMenu = 0;
		EPhase Phase = EPhase::EntryFade;
		/** The settings' revision the map's fixes were read at. */
		uint32 SettingsSeen = 0;
		FWorldMapScreenFade LastFade;
		int32 FramesLeft = 0;
		EGameProgramState ProgramState = EGameProgramState::Running;
	};
}
