#pragma once

#include "Field/FieldFrame.h"
#include "Field/FieldPartySprites.h"
#include "Field/FieldPlayerCamera.h"
#include "Field/FieldPresenter.h"
#include "Field/FieldRuntime.h"
#include "Field/FieldSceneFrame.h"
#include "Game/BootNotice.h"
#include "Game/GameProgram.h"

#include <array>
#include <memory>
#include <optional>

namespace Weltall
{
	class FFieldMovie;
	class FGameSession;

	/**
	 * The Field program as the kernel runs it: its start (the map, camera and entrance the game state
	 * holds, the song, effect file 0 registered with the sound), the field's frames with their drawing
	 * (the last two kept for the display to draw between), the movies it plays and the menu it runs
	 * inside itself, the screen the French translation's kernel shows from its file reading, and its
	 * way out to the program its scripts or the player ask for, with what it hands over in the
	 * kernel's globals and the game state.
	 */
	class FFieldProgram final : public IGameProgram
	{
	public:
		/** The program started as the kernel runs it. The session must outlive it. */
		WELTALLCORE_API static TResult<std::unique_ptr<FFieldProgram>> Start(FGameSession& Session);

		WELTALLCORE_API ~FFieldProgram() override;
		FFieldProgram(const FFieldProgram&) = delete;
		FFieldProgram& operator=(const FFieldProgram&) = delete;

		WELTALLCORE_API int32 VblanksPerFrame() const override;
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Frame(std::span<const FPadState> HeldPerVblank) override;
		EGameProgramState State() const override { return ProgramState; }


		const FFieldRuntime& Runtime() const { return *FieldRuntime; }
		FFieldRuntime& Runtime() { return *FieldRuntime; }
		const FFieldPresenter& Drawing() const { return *Presenter; }

		/** The latest frame the field drew, null before the first. */
		WELTALLCORE_API const FFieldFrame* LatestFrame() const;
		/** The frame before the latest, when the two follow each other on the same map; null otherwise. */
		WELTALLCORE_API const FFieldFrame* PreviousFrame() const;
		/** The frame before the latest, of whatever map; null before the second. */
		WELTALLCORE_API const FFieldFrame* FrameBeforeLatest() const;

		/** The movie the field plays inside itself, while it plays one. */
		const FFieldMovie* Movie() const { return FieldMovie.get(); }

		/**
		 * The French translation's credits while its kernel shows them: as the field reads the picture that ends the
		 * game, its tick's work stands still until a press ends them, then the field goes on.
		 */
		const FBootNotice* Credits() const { return TranslationCredits ? &*TranslationCredits : nullptr; }

		/** What the player does to the field's view, shown from the next frame; nothing while the menu, a movie or the credits run. */
		WELTALLCORE_API void AddViewInput(const FFieldViewInput& Input);

		/** The player's camera the frames are presented with. */
		const FFieldPlayerCamera& PlayerView() const { return PlayerCamera; }

	private:
		explicit FFieldProgram(FGameSession& InSession);

		/** The session's settings the field plays and draws with: its fixes and the scenery's culling. */
		void FollowSettings();
		std::optional<FError> EnterFirstMap();
		std::optional<FError> Tick(std::span<const FPadState> HeldPerVblank);
		/** Takes up what the field stopped at, if anything. */
		std::optional<FError> TakeRequest();
		std::optional<FError> StartMovie();
		/** The credits' screen, their song started anew. */
		std::optional<FError> StartCredits();
		/** A vertical blank of the credits; once they are over, what the field's tick stopped at is taken up. */
		std::optional<FError> AdvanceCredits(FPadState Held);
		/** The movie's share of the field's frame: its pictures and cues through the frame's vertical blanks, the picture in the scene. */
		std::optional<FError> AdvanceMovie(int32 Vblanks);
		std::optional<FError> StartMenu(const FFieldRequest& Request);
		std::optional<FError> Leave(const FFieldRequest& Request);
		/** A game the Normal Menu loaded goes on from its save's map, as the title's map sends a loaded game on. */
		std::optional<FError> GoOnWithLoadedGame();
		void KeepFormations();
		/**
		 * The chapter's title card through a field frame of Vblanks: a new chapter's begins once the player has
		 * control, or where the story passes the chapter inside scenes, as a scene asks or the story enters it.
		 */
		void AdvanceChapterCard(int32 Vblanks);
		void ReleaseSound();

		FGameSession& Session;
		FFieldPlayerCamera PlayerCamera;
		FPartySpriteFiles PartySprites;
		std::unique_ptr<FFieldRuntime> FieldRuntime;
		std::optional<FFieldPresenter> Presenter;
		std::unique_ptr<FFieldMovie> FieldMovie;
		std::optional<FBootNotice> TranslationCredits;
		/** The menu the field runs inside itself, while it runs. */
		std::unique_ptr<IGameProgram> Menu;
		EFieldCulling Culling = EFieldCulling::Original;
		/** The session's count of settings the field last followed. */
		uint32 SettingsSeen = 0;
		std::array<FFieldFrame, 2> Frames;
		int32 Latest = -1;
		bool bPreviousFollows = false;
		bool bFrameBefore = false;
		/** The field's count of map loads the drawing and the kernel's formations were last taken at. */
		int32 MapLoadsSeen = 0;
		bool bMainEffectsRegistered = false;
		EGameProgramState ProgramState = EGameProgramState::Running;
	};
}
