#pragma once

#include "CoreMinimal.h"
#include "Foundation/Result.h"
#include "Game/PlayerSettings.h"
#include "Input/PadLatch.h"
#include "Media/DiscIdentification.h"
#include "Menu/MenuProgram.h"
#include "Rendering/FieldSceneRendering.h"
#include "Screens/GameScreen.h"
#include "Screens/MoviePictureView.h"
#include "Timing/TickClock.h"

#include <memory>
#include <array>
#include <optional>
#include <vector>

class FBattleSpeedMarkView;
class FBattleView;
class FBattlingView;
class FBootNoticeView;
class FChapterCardView;
class FTitleMarkView;
class FFieldView;
class FMenuView;
class FWorldMapView;

namespace Weltall
{
	class FGameFileIndex;
	class FGameLoop;
	class FGameSession;
	class FGameState;
	class FGameSoundSession;
	class FIso9660;
	class IGameDiscDrive;
	class FMenuGameMenus;
	class FPsxExecutable;
	class IButtonGlyphs;
	class IMenuKeys;
	class IMenuAssets;
	class IMenuDeviceFiles;
	class FSaveStore;
	class FStoryPacks;
}

/**
 * The game as its kernel runs it, on screen: the publisher's notice, the Movie program's movie, the
 * field (its frames drawn between its ticks, the movies it plays inside itself, the Menu it runs over its
 * last picture) and the programs after
 * them as the loop hands one to the next, run at the NTSC console's vertical blanks with the controller
 * latched at each. What the port does not run yet stops the game, logged and told to the host, the last
 * frame staying on screen. The pad's soft reset starts it again from power-on, its sound too. A battle fought at the player's
 * battle speed has its vertical blanks come that many times as fast (Weltall::GamePace), a mark in the frame's
 * corner saying so; the sound and the play time keep their own pace.
 */
class WELTALL_API FGameLoopScreen final : public IGameScreen
{
public:
	/** Starts the game's sound afresh, as the console's start-up does, and returns it; null when it cannot. It must outlive the next call. */
	using FStartSound = TFunction<Weltall::FGameSoundSession*()>;

	/** Told of the player's framing of the field whenever the player's view moves it. */
	using FCameraMoved = TFunction<void(const Weltall::FFieldCameraOffsets&, const Weltall::FWorldMapCameraOffsets&)>;

	/** Told of the battle speed whenever the player steps it in a battle. */
	using FBattleSpeedStepped = TFunction<void(Weltall::EBattleSpeed)>;

	/** Told of the player's settings as the Menu's options screen closes, to keep them. */
	using FSettingsKept = TFunction<void(const Weltall::FPlayerSettings&)>;
	using FDisplayModesAsked = TFunction<Weltall::FDisplayModes()>;
	using FMenuModsAsked = TFunction<Weltall::FMenuMods()>;

	/** Closes the game. */
	using FQuit = TFunction<void()>;

	/** Told once why the game stopped: at something the port does not run yet, or as a restarted console could not start. */
	using FStopped = TFunction<void(const Weltall::FError&)>;

	/** Looks again where the player keeps the discs for the disc of a release; null while it is still not there. */
	using FDiscSought = TFunction<const Weltall::FIso9660*(Weltall::EDiscRelease)>;

	/**
	 * The game powered on with a disc and the player's settings, the game's other disc beside it when
	 * the player has it (its movies play once the game asks for it); the discs must outlive the screen.
	 * The reason when it cannot start.
	 */
	static Weltall::TResult<TSharedRef<FGameLoopScreen>> PowerOn(const Weltall::FIso9660& Disc, FStartSound StartSound,
		const Weltall::FPlayerSettings& Settings, const Weltall::FIso9660* OtherDisc = nullptr);

	/** For the tools that stage a scene: changes a new game before the field map is entered with it. */
	using FStateStaged = TFunction<void(Weltall::FGameSession&)>;

	/** A new game entering field map Number at once (bits 14 and 15: in gears), the game as Staged leaves it; the reason when it cannot start. */
	static Weltall::TResult<TSharedRef<FGameLoopScreen>> EnterField(const Weltall::FIso9660& Disc, FStartSound StartSound, uint16 Number,
		const Weltall::FPlayerSettings& Settings, FStateStaged Staged = nullptr);

	FGameLoopScreen(const Weltall::FIso9660& InDisc, FStartSound InStartSound, const Weltall::FPlayerSettings& InSettings,
		const Weltall::FIso9660* InOtherDisc = nullptr);
	virtual ~FGameLoopScreen() override;

	virtual void Update(double DeltaSeconds, Weltall::FPadState Held) override;
	virtual void AddViewInput(const Weltall::FFieldViewInput& Input) override;
	virtual void SetPads(const FGamePads& Pads) override;
	virtual bool ReadsSecondPad() const override;
	virtual std::array<bool, 2> Motors() const override;
	virtual TSharedRef<const IScreenFrame> CaptureFrame() const override;

	/** Whether the game has stopped at something the port does not run yet. */
	bool HasStopped() const;

	/** The player changed the settings: the game and its drawing follow them from the next frame, the sound at once. */
	void ApplySettings(const Weltall::FPlayerSettings& InSettings);

	/** The settings as the game has them now: the player's framing of the field moves with the view. */
	const Weltall::FPlayerSettings& Settings() const;

	/** Where the game's programs learn how the PlayStation buttons are shown; it must outlive the screen. */
	void SetButtonGlyphs(const Weltall::IButtonGlyphs* InGlyphs);

	/** The player's saves, which the Menu's File screen shows; it must outlive the screen. */
	void SetSaves(Weltall::FSaveStore* InSaves);

	void SetCameraMoved(FCameraMoved InCameraMoved) { CameraMoved = MoveTemp(InCameraMoved); }
	void SetBattleSpeedStepped(FBattleSpeedStepped InBattleSpeedStepped) { BattleSpeedStepped = MoveTemp(InBattleSpeedStepped); }

	/** Whoever keeps the settings the options screen changes; until one is set, the Menu has the original's Sound in its place. */
	void SetSettingsKept(FSettingsKept InSettingsKept);

	/** Told of each change the options screen makes as it is made, for the host's input to follow its bindings at once. */
	void SetSettingsFollowed(FSettingsKept InSettingsFollowed);

	/** The player's keys, which the options screen's Controls page rebinds; it must outlive the screen. None: no Controls page. */
	void SetMenuKeys(Weltall::IMenuKeys* InKeys);
	void SetMenuAssets(Weltall::IMenuAssets* InAssets);
	/** The device's file picker, which the options screen's Saves page goes through; it must outlive the screen. None: the computer's folders. */
	void SetMenuDeviceFiles(Weltall::IMenuDeviceFiles* InFiles);
	/** The mods the game has found, asked as the options screen's Modding page lists them (one added during play among them). */
	void SetMenuMods(FMenuModsAsked InMods);
	/**
	 * Perfect Works Build's movie subtitles, which every session of the screen is given (null: none); bInUse: the game
	 * plays with the mod. They must outlive the screen.
	 */
	void SetPerfectWorksSubtitles(std::array<const Weltall::FPerfectWorksSubtitles*, 2> Subtitles, bool bInUse);

	/** What the display offers the options screen's Display page, asked as the screen opens. None: the page offers no window sizes. */
	void SetDisplayModes(FDisplayModesAsked InDisplayModes);

	/** Closes the game, which the title's Exit orb asks for. None: the title has the original's three orbs. */
	void SetQuit(FQuit InQuit);

	void SetStopped(FStopped InStopped) { Stopped = MoveTemp(InStopped); }

	/**
	 * Where the game's other disc is looked for when the game asks for it and was not given it at power-on (the
	 * player may put it there while the game asks); the disc found must outlive the screen. None: it is never found.
	 */
	void SetDiscSought(FDiscSought InDiscSought);

	/** For the tools that stage a scene: the state the game plays, changed before the next frame; null before the game has started. */
	Weltall::FGameState* GameState();

	/** For the tools that walk a scene's way: the programs the game runs, changed before the next frame; null before the game has started. */
	Weltall::FGameLoop* Loop();

	/** For the tools that stage a scene: a menu the running field opens at its next quiet tick, as its scripts ask for one; false without a field. */
	bool RequestMenu(uint8 Mode, uint8 Parameter);

	/** For the tools that stage a scene: a battle of the map's formation that the running field starts as a script asks for one; false without a field. */
	bool RequestBattle(uint8 Formation, uint8 Kind);

	/** For the tools that stage a scene: the arena's match of these six bytes, which the running field starts as a script asks for one; false without a field. */
	bool RequestArena(const std::array<uint8, 6>& Match);

	/**
	 * For the tools that stage a scene: a chapter's card begun now, as the player's first control in the chapter begins
	 * it, or the card of the expanded second disc's subtitle of first value Subtitle inside it (0: none); false before
	 * the game has started.
	 */
	bool ShowChapterCard(int32 Chapter, int32 Subtitle = 0);

	/**
	 * For the tools that play a scene as a reader would: how many glyphs the message shows that waits for the
	 * player's confirm button (a field message's page, a battle event's); unset while none waits.
	 */
	TOptional<int32> WaitingMessageGlyphs() const;

	/** Whether the menus draw past the 320 x 224 screen's edges where a wider display shows them; off, as the original clips them. */
	void SetMenuDrawingAreaWide(bool bWide);

private:
	/** The console's start: the session and the loop, from power-on or entering a field map; the reason when it cannot. */
	std::optional<Weltall::FError> Start(std::optional<uint16> FieldMap);
	void RunFrame();
	/** The Menu takes the host's services: the options screen hands what it changed to SettingsFollowed and SettingsKept, when there are, and rebinds MenuKeys'; the title closes the game through Quit. */
	void FollowMenuServices();
	/** The views take what the frame left. */
	void Follow(bool bNewProgram);
	/** What the program running shows now. */
	TSharedRef<const IScreenFrame> CaptureProgramFrame() const;
	/** Whoever keeps the settings is told what the game changed in them by itself: the player's framing, the battle speed. */
	void KeepPlayedSettings();
	/** The other disc's program and file index read; the reason where they cannot be. */
	std::optional<Weltall::FError> ReadOtherDisc();
	/** The game stopped: logged, and the host told. */
	void Stop(const Weltall::FError& Error);
	/** The session asks for disc Number, which it was not given: the other disc looked for, and given it when found. */
	void SeekDisc(int32 Number);
	void RefreshVoiceDiscs();
	/** Discs the player added through the device's picker during play give their voices and subtitles at once. */
	void FollowAddedDiscs();
	friend class FGameLoopDiscDrive;

	const Weltall::FIso9660& Disc;
	/** The game's other disc, none when the player does not have it, and its file index once read. */
	const Weltall::FIso9660* OtherDisc = nullptr;
	std::unique_ptr<Weltall::FGameFileIndex> OtherIndex;
	FDiscSought DiscSought;
	struct FVoiceSource
	{
		Weltall::EDiscRelease Release;
		const Weltall::FIso9660* Disc;
		std::unique_ptr<Weltall::FGameFileIndex> Index;
	};
	std::vector<FVoiceSource> VoiceSources;
	/** The session's drive, which asks DiscSought. */
	std::unique_ptr<Weltall::IGameDiscDrive> Drive;
	FStartSound StartSound;
	FStateStaged StateStaged;
	/** The settings a restarted console starts with. */
	Weltall::FPlayerSettings StartSettings;
	const Weltall::IButtonGlyphs* Glyphs = nullptr;
	Weltall::FSaveStore* Saves = nullptr;
	FCameraMoved CameraMoved;
	FBattleSpeedStepped BattleSpeedStepped;
	FSettingsKept SettingsKept;
	FSettingsKept SettingsFollowed;
	Weltall::IMenuKeys* MenuKeys = nullptr;
	Weltall::IMenuAssets* MenuAssets = nullptr;
	Weltall::IMenuDeviceFiles* MenuDeviceFiles = nullptr;
	/** The picker's status the screen last followed. */
	uint64 DeviceFilesRevision = 0;
	FMenuModsAsked MenuMods;
	std::array<const Weltall::FPerfectWorksSubtitles*, 2> PerfectWorksSubtitles{};
	bool bPerfectWorksInUse = false;
	FDisplayModesAsked DisplayModes;
	FQuit Quit;
	FStopped Stopped;
	Weltall::EDiscRelease Release = Weltall::EDiscRelease::UsaDisc1;
	std::unique_ptr<Weltall::FPsxExecutable> Executable;
	std::unique_ptr<Weltall::FGameFileIndex> Index;
	/** The Menu the field runs, which every session of the screen is given. */
	std::unique_ptr<Weltall::FMenuGameMenus> Menus;
	/** The port's own documents, read from the project's Content/Story as the screen first starts, and given to every session. */
	std::unique_ptr<Weltall::FStoryPacks> Story;
	std::unique_ptr<Weltall::FGameSession> Session;
	std::unique_ptr<Weltall::FGameLoop> GameLoop;
	Weltall::FTickClock Vblanks;
	/** How many times its own pace the game ran at in the last display frame: more than once in a battle fought fast. */
	int32 Pace = 1;
	Weltall::FPadLatch Latch;
	Weltall::FPadLatch SecondLatch;
	/** Each controller at each vertical blank since the last frame. */
	std::vector<Weltall::FPadState> HeldPerVblank;
	std::vector<Weltall::FPadState> SecondHeldPerVblank;
	/** The loop's count of programs run the views last followed. */
	int32 ProgramRuns = -1;
	TUniquePtr<FBootNoticeView> Notice;
	/** The French translation's credits, which the field's file reading shows. */
	TUniquePtr<FBootNoticeView> Credits;
	FMoviePictureView Movie;
	TUniquePtr<FFieldView> Field;
	TUniquePtr<FMenuView> Menu;
	TUniquePtr<FWorldMapView> WorldMap;
	TUniquePtr<FBattleView> Battle;
	TUniquePtr<FBattlingView> Battling;
	TUniquePtr<FChapterCardView> ChapterCard;
	TUniquePtr<FTitleMarkView> TitleMark;
	TUniquePtr<FBattleSpeedMarkView> BattleSpeedMark;
	TSharedRef<const IScreenFrame> Blank;
	/** The last frame the display was given: the picture of the screen a battle's transition breaks. */
	mutable TSharedPtr<const IScreenFrame> Shown;
};
