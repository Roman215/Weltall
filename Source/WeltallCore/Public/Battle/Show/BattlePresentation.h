#pragma once

#include "Battle/BattleProgramShow.h"
#include "Battle/Show/BattleSceneGraphics.h"
#include "Battle/Show/BattleShow.h"
#include "Battle/Show/BattleStageFiles.h"
#include "Battle/Show/IdsBattleSprite.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>
#include <optional>
#include <vector>

namespace Weltall
{
	class FBattleEventFiles;
	class FCdImage;
	class FGameFileIndex;
	class FPsxExecutable;

	/** Each battle kind's start sounds, as the kernel's table holds them: up to three effects of the start sounds' `seds`. */
	struct FBattleStartSounds
	{
		static constexpr int32 KindCount = 6;
		static constexpr int32 PerKind = 3;
		static constexpr uint8 None = 0xFF;
		/** The kind whose start plays none, and whose file's effects are not stopped either. */
		static constexpr uint8 SilentKind = 4;

		std::array<std::array<uint8, PerKind>, KindCount> Kinds{};

		/** From the boot executable. */
		WELTALLCORE_API static TResult<FBattleStartSounds> Read(const FPsxExecutable& Executable);
	};

	/** What the battle's presentation reads for every battle, loaded once; everything must outlive it. */
	struct FBattlePresentationFiles
	{
		const FCdImage* Image = nullptr;
		const FGameFileIndex* Index = nullptr;
		const FGteArctangents* Arctangents = nullptr;
		const FBattleSpriteFiles* SpriteFiles = nullptr;
		const FBattleHudSheet* Sheet = nullptr;
		const FBattleMarkerModel* MarkerModel = nullptr;
		const FBattleObjectDisc* ObjectFiles = nullptr;
		const FBattleEventFiles* EventFiles = nullptr;
		const FBattleStartSounds* StartSounds = nullptr;
	};

	/**
	 * The Battle program's presentation: the show with its HUD and sound behind what the program and the rules call. It
	 * plays the battle kind's start sounds, sets up the HUD at the setup's end, reads the battle's stage and scene graphics
	 * and begins the show where the battle's opening does, runs the show's frames around the input step, and answers the
	 * rules' calls, a battle event's instructions among them (the HUD's share of those handed to the HUD). What the
	 * show cannot play stops it: HasStopped, and Fault names it.
	 */
	class FBattlePresentation final : public IBattleProgramShow
	{
	public:
		WELTALLCORE_API FBattlePresentation(const FBattlePresentationFiles& Files, IBattleShowSound* Sound, IBattleShowHud* Hud);
		WELTALLCORE_API ~FBattlePresentation() override;
		FBattlePresentation(const FBattlePresentation&) = delete;
		FBattlePresentation& operator=(const FBattlePresentation&) = delete;

		// IBattleShow
		WELTALLCORE_API void FrameCamera(uint16 Slots) override;
		WELTALLCORE_API void SetMessageBox(uint8 Box, uint8 Entry) override;
		WELTALLCORE_API void ShowNameBox(uint8 Entry) override;
		WELTALLCORE_API void BeginPlayback(bool bStays, uint8 Actor, uint8 Target, uint8 NextReady) override;
		WELTALLCORE_API void CancelPlayback() override;
		WELTALLCORE_API void SetTargetMarkers(uint16 Slots) override;
		WELTALLCORE_API void PlaySound(uint8 Effect) override;
		WELTALLCORE_API void BoardGear(uint8 Slot) override;
		WELTALLCORE_API void ShowTurnDamage(uint8 Slot, const std::array<uint32, 3>& Amounts) override;
		WELTALLCORE_API void BeginEvent() override;
		WELTALLCORE_API void LoadBattleBank() override;
		WELTALLCORE_API void PlayEventCue(const FBattleEventCue& Cue) override;
		WELTALLCORE_API bool IsBlocking() const override;
		WELTALLCORE_API void PlayTurnCue(EBattleTurnCue Cue, uint8 Arg) override;
		WELTALLCORE_API void PlayMenuCue(EBattleMenuCue Cue, uint8 Slot, uint8 Arg) override;
		WELTALLCORE_API void PlayEndCue(EBattleEndCue Cue, uint8 Arg) override;

		// IBattleProgramShow
		WELTALLCORE_API void BeginBattle(FBattleContext& Context) override;
		WELTALLCORE_API void PlayOpeningCue(EBattleOpeningCue Cue, uint8 Arg) override;
		WELTALLCORE_API void BeginFrame() override;
		WELTALLCORE_API bool IsHolding() const override;
		WELTALLCORE_API void HoldBlank() override;
		WELTALLCORE_API int32 CatchUpSteps() const override;
		WELTALLCORE_API bool DrewFirstBuffer() const override;
		WELTALLCORE_API bool FinishFrame() override;
		WELTALLCORE_API bool ResumeRules() override;
		WELTALLCORE_API bool HasStarted() const override;
		WELTALLCORE_API void TickHud(uint8 InputMode) override;
		WELTALLCORE_API void ShowPause(bool bPaused) override;
		WELTALLCORE_API bool HasStopped() const override;

		/** Why the show stopped, if it did. */
		WELTALLCORE_API EBattleShowFault Fault() const;
		/** The pause's picture is over the frame (the pause, or the wait for a controller). */
		bool IsPaused() const { return bPaused; }
		/** The show has begun (the opening's presentation): its frames are drawn from then on. */
		bool HasBegun() const { return bBegun; }

		/** The show, whose frames the renderer draws. */
		FBattleShow Show;

	private:
		/** The setup files' sounds: the start sounds' `seds` registered and the kind's effects played. */
		void PlayStartSounds(uint8 Kind);
		/** The start sounds' file released, its effects stopped first unless the kind played none. */
		void ReleaseStartSounds(bool bStopEffects);
		/** The battle's stage and scene graphics read, and the show begun (the setup's end, the presentation's state, the opening's fades and framing). */
		void BeginShow(bool bEvent);

		FBattlePresentationFiles Files;
		IBattleShowSound* Sound = nullptr;
		IBattleShowHud* Hud = nullptr;
		FBattleContext* Context = nullptr;
		uint8 Kind = 0;
		std::vector<uint8> StartSeds;
		std::optional<FBattleStageFiles> Stage;
		std::optional<FBattleSceneGraphics> SceneGraphics;
		std::optional<FIdsBattleSprite> IdsSprite;
		bool bBegun = false;
		bool bPaused = false;
	};
}
