#include "Screens/GameLoopScreen.h"

#include "BattleSpeedMarkView.h"
#include "BattleView.h"
#include "BattlingView.h"
#include "ChapterCardView.h"
#include "TitleMarkView.h"
#include "FieldView.h"
#include "MenuView.h"
#include "WorldMapView.h"
#include "Battle/BattleRuntime.h"
#include "Field/EventContext.h"
#include "Game/BattleGameProgram.h"
#include "Game/BattlingGameProgram.h"
#include "Game/BootNotice.h"
#include "Game/FieldProgram.h"
#include "Game/GameLoop.h"
#include "Game/GamePace.h"
#include "Game/GameSession.h"
#include "Game/MovieProgram.h"
#include "Game/WorldMapProgram.h"
#include "Media/GameFileIndex.h"
#include "Media/Iso9660.h"
#include "Media/PsxExecutable.h"
#include "Menu/MenuAssetPages.h"
#include "Menu/MenuDeviceFiles.h"
#include "Menu/MenuGameMenus.h"
#include "Rendering/DrawListTextures.h"
#include "Rendering/SharedRenderResource.h"
#include "Screens/DrawListScreenFrame.h"
#include "Screens/MovieScreen.h"
#include "Shell/StoryDocuments.h"
#include "Story/StoryPacks.h"
#include "Timing/PsxTiming.h"
#include "WeltallLog.h"

#include <algorithm>
#include <string>

namespace
{
	/** The most vertical blanks a frame of the game lasts: a skipped movie's fade. */
	constexpr std::size_t MostVblanksPerFrame = 8;

	/** The world map's culling: everything in range, so a wide view shows the ground to its sides. */
	constexpr Weltall::EWorldMapCulling GameLoopWorldMapCulling = Weltall::EWorldMapCulling::InRange;

	/** The world map drawn with the field's presentation settings. */
	FWorldMapRenderOptions GameLoopWorldMapOptions(const FFieldRenderOptions& Field)
	{
		FWorldMapRenderOptions Options;
		Options.Sorting = Field.Sorting;
		Options.Filter = Field.Filter;
		Options.SpriteFilter = Field.SpriteFilter;
		Options.AntiAliasing = Field.AntiAliasing;
		Options.Culling = GameLoopWorldMapCulling;
		return Options;
	}

	/** What shows before anything is drawn. */
	class FBlankScreenFrame final : public IScreenFrame
	{
	public:
		virtual void AddPasses(FRDGBuilder&, FRDGTextureRef, const FIntRect&) const override {}
	};
}

/**
 * The kernel's notice as the display shows it: its draw list each vertical blank, drawn between the last
 * two. The lists are filled again once the rendering thread has let go of them.
 */
class FBootNoticeView
{
public:
	explicit FBootNoticeView(const Weltall::FBootNotice& Notice)
	{
		Textures = ShareRenderResource(new FDrawListTextures(std::span<const Weltall::FVram>(&Notice.Vram(), 1), Notice.Textures()));
	}

	void Follow(const Weltall::FBootNotice& Notice)
	{
		std::shared_ptr<Weltall::FDrawList> Next = TakeFree(Lists);
		Notice.DrawList(*Next);
		std::shared_ptr<std::vector<int32>> Pairs = TakeFree(MatchLists);
		Previous = Latest ? Latest : Next;
		Weltall::MatchBlendKeys(*Previous, *Next, *Pairs, Scratch);
		Latest = std::move(Next);
		Matches = std::move(Pairs);
	}

	/** The displayed frame Alpha of the way from the vertical blank before to the latest; nothing before the first. */
	TSharedPtr<const IScreenFrame> Capture(float Alpha) const
	{
		if (!Latest)
		{
			return nullptr;
		}
		TSharedRef<FDrawListScreenFrame>* Free = Frames.FindByPredicate([](const TSharedRef<FDrawListScreenFrame>& Candidate) { return Candidate.IsUnique(); });
		if (Free == nullptr)
		{
			return Frames.Add_GetRef(MakeShared<FDrawListScreenFrame>(Weltall::FBootNotice::ScreenSize, Textures.ToSharedRef(), Latest, Previous, Matches, Alpha));
		}
		(*Free)->Set(Latest, Previous, Matches, Alpha);
		return *Free;
	}

private:
	template <typename TValue>
	static std::shared_ptr<TValue> TakeFree(std::vector<std::shared_ptr<TValue>>& Pool)
	{
		for (const std::shared_ptr<TValue>& Candidate : Pool)
		{
			if (Candidate.use_count() == 1)
			{
				return Candidate;
			}
		}
		return Pool.emplace_back(std::make_shared<TValue>());
	}

	TSharedPtr<FDrawListTextures> Textures;
	std::vector<std::shared_ptr<Weltall::FDrawList>> Lists;
	std::vector<std::shared_ptr<std::vector<int32>>> MatchLists;
	std::vector<std::pair<uint32, int32>> Scratch;
	std::shared_ptr<Weltall::FDrawList> Latest;
	std::shared_ptr<Weltall::FDrawList> Previous;
	std::shared_ptr<std::vector<int32>> Matches;
	mutable TArray<TSharedRef<FDrawListScreenFrame>> Frames;
};

/** The session's drive: a disc it was not given is looked for where the player keeps the discs. */
class FGameLoopDiscDrive final : public Weltall::IGameDiscDrive
{
public:
	explicit FGameLoopDiscDrive(FGameLoopScreen& InScreen) : Screen(InScreen) {}
	virtual void Seek(int32 Number) override { Screen.SeekDisc(Number); }

private:
	FGameLoopScreen& Screen;
};

std::optional<Weltall::FError> FGameLoopScreen::ReadOtherDisc()
{
	auto OtherExecutable = Weltall::ReadBootExecutable(*OtherDisc);
	if (!OtherExecutable)
	{
		return Weltall::MakeError("the other disc's program cannot be read");
	}
	auto OtherFileIndex = Weltall::FGameFileIndex::Read(*OtherExecutable);
	if (!OtherFileIndex)
	{
		return Weltall::MakeError("the other disc's file index cannot be read: " + OtherFileIndex.Error().Message);
	}
	OtherIndex = std::make_unique<Weltall::FGameFileIndex>(std::move(*OtherFileIndex));
	return std::nullopt;
}

void FGameLoopScreen::SeekDisc(int32 Number)
{
	// The other disc of the kind played first (a translation's with Japanese voices after its own), then of the other kind.
	const bool bUndub = Release == Weltall::EDiscRelease::UndubDisc1 || Release == Weltall::EDiscRelease::UndubDisc2;
	const Weltall::EDiscRelease Usa = Number == 0 ? Weltall::EDiscRelease::UsaDisc1 : Weltall::EDiscRelease::UsaDisc2;
	const Weltall::EDiscRelease Undub = Number == 0 ? Weltall::EDiscRelease::UndubDisc1 : Weltall::EDiscRelease::UndubDisc2;
	const Weltall::EDiscRelease Wanted = bUndub ? Undub : Usa;
	if (!DiscSought || OtherIndex || !Session || Wanted == Release)
	{
		return;
	}
	OtherDisc = DiscSought(Wanted);
	if (OtherDisc == nullptr)
	{
		OtherDisc = DiscSought(bUndub ? Usa : Undub);
	}
	if (OtherDisc == nullptr)
	{
		return;
	}
	if (const std::optional<Weltall::FError> Unread = ReadOtherDisc())
	{
		UE_LOG(LogWeltall, Error, TEXT("Disc %d cannot be used: %hs"), Number + 1, Unread->Message.c_str());
		OtherDisc = nullptr;
		return;
	}
	Session->AddDisc(Number, OtherDisc->Image(), *OtherIndex);
	RefreshVoiceDiscs();
	UE_LOG(LogWeltall, Log, TEXT("Disc %d was found where the game asked for it"), Number + 1);
}

void FGameLoopScreen::SetDiscSought(FDiscSought InDiscSought)
{
	DiscSought = MoveTemp(InDiscSought);
	RefreshVoiceDiscs();
}

void FGameLoopScreen::RefreshVoiceDiscs()
{
	if (!Session)
	{
		return;
	}
	constexpr std::array Releases{Weltall::EDiscRelease::UsaDisc1, Weltall::EDiscRelease::UsaDisc2,
		Weltall::EDiscRelease::UndubDisc1, Weltall::EDiscRelease::UndubDisc2};
	for (const Weltall::EDiscRelease Candidate : Releases)
	{
		const bool bKnown = std::any_of(VoiceSources.begin(), VoiceSources.end(), [Candidate](const FVoiceSource& Source) { return Source.Release == Candidate; });
		if (!bKnown && DiscSought)
		{
			const Weltall::FIso9660* Found = DiscSought(Candidate);
			if (Found == nullptr)
			{
				continue;
			}
			auto Program = Weltall::ReadBootExecutable(*Found);
			auto FileIndex = Program ? Weltall::FGameFileIndex::Read(*Program) : Weltall::TResult<Weltall::FGameFileIndex>(Program.Error());
			if (!FileIndex)
			{
				UE_LOG(LogWeltall, Warning, TEXT("Voice disc cannot be read: %hs"), FileIndex.Error().Message.c_str());
				continue;
			}
			VoiceSources.push_back({Candidate, Found, std::make_unique<Weltall::FGameFileIndex>(std::move(*FileIndex))});
		}
	}
	for (const FVoiceSource& Source : VoiceSources)
	{
		const bool bJapanese = Source.Release == Weltall::EDiscRelease::UndubDisc1 || Source.Release == Weltall::EDiscRelease::UndubDisc2;
		Session->AddVoiceDisc(bJapanese ? Weltall::EVoiceLanguage::Japanese : Weltall::EVoiceLanguage::English,
			Weltall::DiscNumberOfRelease(Source.Release) - 1, Source.Disc->Image(), *Source.Index);
	}
}

Weltall::TResult<TSharedRef<FGameLoopScreen>> FGameLoopScreen::PowerOn(const Weltall::FIso9660& Disc, FStartSound StartSound,
	const Weltall::FPlayerSettings& Settings, const Weltall::FIso9660* OtherDisc)
{
	TSharedRef<FGameLoopScreen> Screen = MakeShared<FGameLoopScreen>(Disc, MoveTemp(StartSound), Settings, OtherDisc);
	if (std::optional<Weltall::FError> Error = Screen->Start(std::nullopt))
	{
		return MoveTemp(*Error);
	}
	return Screen;
}

Weltall::TResult<TSharedRef<FGameLoopScreen>> FGameLoopScreen::EnterField(const Weltall::FIso9660& Disc, FStartSound StartSound, uint16 Number,
	const Weltall::FPlayerSettings& Settings, FStateStaged Staged)
{
	TSharedRef<FGameLoopScreen> Screen = MakeShared<FGameLoopScreen>(Disc, MoveTemp(StartSound), Settings);
	Screen->StateStaged = MoveTemp(Staged);
	if (std::optional<Weltall::FError> Error = Screen->Start(Number))
	{
		return MoveTemp(*Error);
	}
	return Screen;
}

FGameLoopScreen::FGameLoopScreen(const Weltall::FIso9660& InDisc, FStartSound InStartSound, const Weltall::FPlayerSettings& InSettings,
	const Weltall::FIso9660* InOtherDisc)
	: Disc(InDisc), OtherDisc(InOtherDisc), StartSound(MoveTemp(InStartSound)), StartSettings(InSettings), Vblanks(Weltall::PsxTiming::FrameSeconds),
	  Field(MakeUnique<FFieldView>(InSettings.Field)), Menu(MakeUnique<FMenuView>(InSettings.Field)),
	  WorldMap(MakeUnique<FWorldMapView>(GameLoopWorldMapOptions(InSettings.Field))), Battle(MakeUnique<FBattleView>(InSettings.Field)),
	  Battling(MakeUnique<FBattlingView>(GameLoopWorldMapOptions(InSettings.Field))),
	  ChapterCard(MakeUnique<FChapterCardView>()),
	  TitleMark(MakeUnique<FTitleMarkView>()),
	  BattleSpeedMark(MakeUnique<FBattleSpeedMarkView>()),
	  Blank(MakeShared<FBlankScreenFrame>())
{
	HeldPerVblank.reserve(MostVblanksPerFrame);
	SecondHeldPerVblank.reserve(MostVblanksPerFrame);
}

FGameLoopScreen::~FGameLoopScreen() = default;

std::optional<Weltall::FError> FGameLoopScreen::Start(std::optional<uint16> FieldMap)
{
	// The old console goes before its sound does; the new one keeps what the player chose on the old.
	if (Session)
	{
		StartSettings = Session->Settings();
	}
	GameLoop.reset();
	Session.reset();
	if (!Index)
	{
		auto Read = Weltall::ReadBootExecutable(Disc);
		auto Identity = Weltall::IdentifyDisc(Disc);
		if (!Read || !Identity)
		{
			return Weltall::MakeError("the disc's program cannot be read");
		}
		auto FileIndex = Weltall::FGameFileIndex::Read(*Read);
		if (!FileIndex)
		{
			return Weltall::MakeError("the disc's file index cannot be read: " + FileIndex.Error().Message);
		}
		if (OtherDisc != nullptr)
		{
			if (std::optional<Weltall::FError> Unread = ReadOtherDisc())
			{
				return Unread;
			}
		}
		auto Documents = ReadStoryDocuments();
		if (!Documents)
		{
			return Weltall::MakeError("the port's story cannot be read: " + Documents.Error().Message);
		}
		UE_LOG(LogWeltall, Log, TEXT("Story documents read: %d"), Documents->DocumentCount());
		Story = std::make_unique<Weltall::FStoryPacks>(std::move(*Documents));
		Executable = std::make_unique<Weltall::FPsxExecutable>(std::move(*Read));
		Index = std::make_unique<Weltall::FGameFileIndex>(std::move(*FileIndex));
		Release = Identity->Release;
		Menus = std::make_unique<Weltall::FMenuGameMenus>(*Executable);
		FollowMenuServices();
	}
	Weltall::FGameSoundSession* Sound = StartSound();
	if (Sound == nullptr)
	{
		return Weltall::MakeError("the game's sound cannot start");
	}
	auto Started = Weltall::FGameSession::PowerOn(Disc.Image(), *Index, *Executable, Release, *Sound);
	if (!Started)
	{
		return Started.Error();
	}
	Session = std::move(*Started);
	if (OtherIndex)
	{
		Session->AddDisc(Weltall::DiscNumberOfRelease(Release) == 2 ? 0 : 1, OtherDisc->Image(), *OtherIndex);
	}
	if (!Drive)
	{
		Drive = std::make_unique<FGameLoopDiscDrive>(*this);
	}
	Session->SetDiscDrive(Drive.get());
	Session->SetSettings(StartSettings);
	Session->SetPerfectWorksSubtitles(PerfectWorksSubtitles, bPerfectWorksInUse);
	RefreshVoiceDiscs();
#if UE_BUILD_SHIPPING
	// The player's builds carry play past the cases no script reaches; the editor's and development builds stop there.
	Session->Unreached().bStops = false;
#endif
	Session->SetButtonGlyphs(Glyphs);
	Session->SetSaves(Saves);
	Session->SetMenus(Menus.get());
	Session->SetStory(Story.get());
	if (FieldMap && StateStaged)
	{
		StateStaged(*Session);
	}
	auto Loop = FieldMap ? Weltall::FGameLoop::EnterField(*Session, *FieldMap) : Weltall::FGameLoop::PowerOn(*Session, *Executable);
	if (!Loop)
	{
		return Loop.Error();
	}
	GameLoop = std::move(*Loop);
	HeldPerVblank.clear();
	SecondHeldPerVblank.clear();
	ProgramRuns = -1;
	Notice.Reset();
	Credits.Reset();
	Follow(true);
	return std::nullopt;
}

void FGameLoopScreen::Stop(const Weltall::FError& Error)
{
	UE_LOG(LogWeltall, Error, TEXT("The game stopped: %hs"), Error.Message.c_str());
	if (Stopped)
	{
		Stopped(Error);
	}
}

void FGameLoopScreen::Update(double DeltaSeconds, Weltall::FPadState Held)
{
	Pace = 1;
	if (!GameLoop || HasStopped())
	{
		return;
	}
	Latch.Sample(Held);
	FollowAddedDiscs();
	Pace = Weltall::GamePace(*GameLoop, Session->Settings());
	Session->SetPace(Pace);
	const int32 Due = Vblanks.Advance(DeltaSeconds, Pace);
	for (int32 Vblank = 0; Vblank < Due && GameLoop && !HasStopped(); ++Vblank)
	{
		HeldPerVblank.push_back(Latch.TakeForTick());
		SecondHeldPerVblank.push_back(SecondLatch.TakeForTick());
		if (static_cast<int32>(HeldPerVblank.size()) >= GameLoop->VblanksPerFrame())
		{
			RunFrame();
			// The fight's end begins what follows at the game's own pace: the display frame's other vertical blanks are dropped.
			if (Pace > 1 && GameLoop && !Weltall::IsBattleFought(*GameLoop))
			{
				break;
			}
		}
	}
	KeepPlayedSettings();
}

void FGameLoopScreen::KeepPlayedSettings()
{
	if (!Session)
	{
		return;
	}
	const Weltall::FPlayerSettings& Played = Session->Settings();
	if (Played.VoiceLanguage != StartSettings.VoiceLanguage)
	{
		StartSettings.VoiceLanguage = Played.VoiceLanguage;
		RefreshVoiceDiscs();
	}
	if (CameraMoved && (Played.Camera != StartSettings.Camera || Played.WorldMapCamera != StartSettings.WorldMapCamera))
	{
		StartSettings.Camera = Played.Camera;
		StartSettings.WorldMapCamera = Played.WorldMapCamera;
		CameraMoved(StartSettings.Camera, StartSettings.WorldMapCamera);
	}
	if (Played.BattleSpeed != StartSettings.BattleSpeed)
	{
		StartSettings.BattleSpeed = Played.BattleSpeed;
		if (BattleSpeedStepped)
		{
			BattleSpeedStepped(StartSettings.BattleSpeed);
		}
	}
}

void FGameLoopScreen::SetPads(const FGamePads& Pads)
{
	SecondLatch.Sample(Pads.bSecondConnected ? Pads.SecondHeld : Weltall::FPadState{});
	if (Session)
	{
		Weltall::FKernelPad& Pad = Session->Pad();
		Pad.SetConnected(Pads.bConnected, 0);
		Pad.SetMotor(0, Pads.bMotor);
		Pad.SetConnected(Pads.bSecondConnected, 1);
		Pad.SetMotor(1, Pads.bSecondConnected);
	}
}

bool FGameLoopScreen::ReadsSecondPad() const
{
	const Weltall::FBattlingGameProgram* Arena = GameLoop && !HasStopped() ? GameLoop->Battling() : nullptr;
	return Arena != nullptr && Arena->ReadsSecondPad();
}

std::array<bool, 2> FGameLoopScreen::Motors() const
{
	if (!Session || !GameLoop || HasStopped())
	{
		return {};
	}
	return {Session->Vibration().IsOn(0), Session->Vibration().IsOn(1)};
}

void FGameLoopScreen::AddViewInput(const Weltall::FFieldViewInput& Input)
{
	if (!GameLoop || HasStopped())
	{
		return;
	}
	if (Weltall::FFieldProgram* FieldProgram = GameLoop->Field())
	{
		FieldProgram->AddViewInput(Input);
	}
	else if (Weltall::FWorldMapProgram* WorldMapProgram = GameLoop->WorldMap())
	{
		WorldMapProgram->AddViewInput(Input);
	}
}

void FGameLoopScreen::RunFrame()
{
	Session->SetSecondPad(SecondHeldPerVblank);
	SecondHeldPerVblank.clear();
	if (auto Error = GameLoop->Frame(HeldPerVblank))
	{
		Stop(*Error);
	}
	for (const std::string& Case : Session->Unreached().TakeNoted())
	{
		UE_LOG(LogWeltall, Warning, TEXT("The game carried on past a case no script reaches: %hs"), Case.c_str());
	}
	HeldPerVblank.clear();
	if (GameLoop->Stage() == Weltall::EGameLoopStage::SoftReset)
	{
		UE_LOG(LogWeltall, Display, TEXT("The console starts again"));
		if (std::optional<Weltall::FError> Error = Start(std::nullopt))
		{
			Stop(*Error);
		}
		return;
	}
	const bool bNewProgram = GameLoop->ProgramRuns() != ProgramRuns;
	ProgramRuns = GameLoop->ProgramRuns();
	if (bNewProgram && GameLoop->Stage() == Weltall::EGameLoopStage::Program)
	{
		UE_LOG(LogWeltall, Log, TEXT("Now running %hs"), Weltall::GameProgramName(GameLoop->Program()));
	}
	Follow(bNewProgram);
}

void FGameLoopScreen::Follow(bool bNewProgram)
{
	if (const Weltall::FBootNotice* BootNotice = GameLoop->Notice())
	{
		if (!Notice)
		{
			Notice = MakeUnique<FBootNoticeView>(*BootNotice);
		}
		Notice->Follow(*BootNotice);
	}
	if (bNewProgram)
	{
		Movie.Reset();
	}
	if (const Weltall::FMovieProgram* MovieProgram = GameLoop->Movie())
	{
		if (MovieProgram->Player() != nullptr)
		{
			Movie.Follow(*MovieProgram->Player(), Settings().Field.MovieFilter);
		}
	}
	if (const Weltall::FFieldProgram* FieldProgram = GameLoop->Field())
	{
		Field->Follow(*FieldProgram, bNewProgram, Shown);
		if (const Weltall::FBootNotice* FieldCredits = FieldProgram->Credits())
		{
			if (!Credits)
			{
				Credits = MakeUnique<FBootNoticeView>(*FieldCredits);
			}
			Credits->Follow(*FieldCredits);
		}
		else
		{
			Credits.Reset();
		}
	}
	if (Weltall::FWorldMapProgram* WorldMapProgram = GameLoop->WorldMap())
	{
		if (bNewProgram)
		{
			WorldMapProgram->SetCulling(GameLoopWorldMapCulling);
		}
		WorldMap->Follow(*WorldMapProgram, bNewProgram, Shown);
	}
	if (const Weltall::FBattleGameProgram* BattleProgram = GameLoop->Battle())
	{
		Battle->Follow(*BattleProgram, bNewProgram, Shown);
	}
	if (const Weltall::FBattlingGameProgram* BattlingProgram = GameLoop->Battling())
	{
		Battling->Follow(*BattlingProgram, bNewProgram);
	}
	if (const Weltall::FMenuProgram* Running = Menus->Running())
	{
		Menu->Follow(*Running);
	}
	else
	{
		Menu->Reset();
	}
}

TSharedRef<const IScreenFrame> FGameLoopScreen::CaptureFrame() const
{
	TSharedRef<const IScreenFrame> Frame = CaptureProgramFrame();
	Shown = Frame;
	return Pace > 1 && Session ? BattleSpeedMark->Capture(*Session, MoveTemp(Frame)) : Frame;
}

TSharedRef<const IScreenFrame> FGameLoopScreen::CaptureProgramFrame() const
{
	if (!GameLoop)
	{
		return Blank;
	}
	// The display lies between the last two frames: as far past the latest as the vertical blanks since it say.
	const float Alpha = HasStopped() ? 1.0f
		: FMath::Clamp((static_cast<float>(HeldPerVblank.size()) + Vblanks.Alpha()) / static_cast<float>(GameLoop->VblanksPerFrame()), 0.0f, 1.0f);
	if (GameLoop->Stage() == Weltall::EGameLoopStage::Boot)
	{
		const TSharedPtr<const IScreenFrame> Frame = Notice ? Notice->Capture(Alpha) : nullptr;
		return Frame ? Frame.ToSharedRef() : Blank;
	}
	if (GameLoop->Movie() != nullptr)
	{
		return Movie.Capture(FMovieScreen::Screen, FMovieScreen::Window);
	}
	if (const Weltall::FFieldProgram* FieldProgram = GameLoop->Field())
	{
		if (Credits && FieldProgram->Credits() != nullptr)
		{
			if (const TSharedPtr<const IScreenFrame> Frame = Credits->Capture(Alpha))
			{
				return Frame.ToSharedRef();
			}
		}
		// A menu the field runs is drawn over the field's last picture, standing still, the title's mark part of it.
		if (Menus->Running() != nullptr)
		{
			TSharedPtr<const IScreenFrame> Still = Field->CaptureStill(*FieldProgram);
			if (Still.IsValid())
			{
				Still = TitleMark->Capture(*Session, *FieldProgram, Still.ToSharedRef(), true);
			}
			if (TSharedPtr<const IScreenFrame> Frame = Menu->Capture(Still, Alpha))
			{
				return Frame.ToSharedRef();
			}
		}
		if (TSharedPtr<const IScreenFrame> Frame = Field->Capture(*FieldProgram, Alpha))
		{
			return TitleMark->Capture(*Session, *FieldProgram, ChapterCard->Capture(*Session, Frame.ToSharedRef()));
		}
	}
	if (const Weltall::FWorldMapProgram* WorldMapProgram = GameLoop->WorldMap())
	{
		// A menu the world map runs is drawn over the map's last picture, standing still.
		if (Menus->Running() != nullptr)
		{
			if (TSharedPtr<const IScreenFrame> Frame = Menu->Capture(WorldMap->CaptureStill(*WorldMapProgram), Alpha))
			{
				return Frame.ToSharedRef();
			}
		}
		if (TSharedPtr<const IScreenFrame> Frame = WorldMap->Capture(*WorldMapProgram, Alpha, Shown))
		{
			return ChapterCard->Capture(*Session, Frame.ToSharedRef());
		}
	}
	if (const Weltall::FBattleGameProgram* BattleProgram = GameLoop->Battle())
	{
		if (TSharedPtr<const IScreenFrame> Frame = Battle->Capture(*BattleProgram, Alpha))
		{
			return Frame.ToSharedRef();
		}
	}
	if (const Weltall::FBattlingGameProgram* BattlingProgram = GameLoop->Battling())
	{
		if (TSharedPtr<const IScreenFrame> Frame = Battling->Capture(*BattlingProgram, Alpha))
		{
			return Frame.ToSharedRef();
		}
	}
	return Blank;
}

void FGameLoopScreen::ApplySettings(const Weltall::FPlayerSettings& InSettings)
{
	StartSettings = InSettings;
	if (Session)
	{
		Session->SetSettings(InSettings);
	}
	Field->SetOptions(InSettings.Field);
	Menu->SetOptions(InSettings.Field);
	WorldMap->SetOptions(GameLoopWorldMapOptions(InSettings.Field));
	Battle->SetOptions(InSettings.Field);
	Battling->SetOptions(GameLoopWorldMapOptions(InSettings.Field));
}

void FGameLoopScreen::SetButtonGlyphs(const Weltall::IButtonGlyphs* InGlyphs)
{
	Glyphs = InGlyphs;
	Menu->SetButtonGlyphs(InGlyphs);
	Battle->SetButtonGlyphs(InGlyphs);
	Battling->SetButtonGlyphs(InGlyphs);
	Field->SetButtonGlyphs(InGlyphs);
	if (Session)
	{
		Session->SetButtonGlyphs(InGlyphs);
	}
}

void FGameLoopScreen::SetSettingsKept(FSettingsKept InSettingsKept)
{
	SettingsKept = MoveTemp(InSettingsKept);
	FollowMenuServices();
}

void FGameLoopScreen::SetSettingsFollowed(FSettingsKept InSettingsFollowed)
{
	SettingsFollowed = MoveTemp(InSettingsFollowed);
	FollowMenuServices();
}

void FGameLoopScreen::SetMenuKeys(Weltall::IMenuKeys* InKeys)
{
	MenuKeys = InKeys;
	FollowMenuServices();
}

void FGameLoopScreen::SetMenuAssets(Weltall::IMenuAssets* InAssets)
{
	MenuAssets = InAssets;
	FollowMenuServices();
}

void FGameLoopScreen::SetMenuDeviceFiles(Weltall::IMenuDeviceFiles* InFiles)
{
	MenuDeviceFiles = InFiles;
	DeviceFilesRevision = InFiles != nullptr ? InFiles->Status().Revision : 0;
	FollowMenuServices();
}

void FGameLoopScreen::FollowAddedDiscs()
{
	if (MenuDeviceFiles == nullptr)
	{
		return;
	}
	const Weltall::FTransferStatus Status = MenuDeviceFiles->Status();
	if (Status.Revision == DeviceFilesRevision)
	{
		return;
	}
	DeviceFilesRevision = Status.Revision;
	if (Weltall::IsDiscTransfer(Status.Kind) && Status.State == Weltall::ETransferState::Done)
	{
		RefreshVoiceDiscs();
	}
}

void FGameLoopScreen::SetMenuMods(FMenuModsAsked InMods)
{
	MenuMods = MoveTemp(InMods);
	FollowMenuServices();
}

void FGameLoopScreen::SetPerfectWorksSubtitles(std::array<const Weltall::FPerfectWorksSubtitles*, 2> Subtitles, bool bInUse)
{
	PerfectWorksSubtitles = Subtitles;
	bPerfectWorksInUse = bInUse;
	if (Session)
	{
		Session->SetPerfectWorksSubtitles(PerfectWorksSubtitles, bPerfectWorksInUse);
	}
}

void FGameLoopScreen::SetDisplayModes(FDisplayModesAsked InDisplayModes)
{
	DisplayModes = MoveTemp(InDisplayModes);
	FollowMenuServices();
}

void FGameLoopScreen::SetQuit(FQuit InQuit)
{
	Quit = MoveTemp(InQuit);
	FollowMenuServices();
}

void FGameLoopScreen::FollowMenuServices()
{
	if (!Menus)
	{
		return;
	}
	Menus->SetAssets(MenuAssets);
	Menus->SetDeviceFiles(MenuDeviceFiles);
	if (MenuMods)
	{
		Menus->SetMods([this]() { return MenuMods(); });
	}
	else
	{
		Menus->SetMods(nullptr);
	}
	Menus->SetKeys(MenuKeys);
	if (Quit)
	{
		Menus->SetQuit([this]() { Quit(); });
	}
	else
	{
		Menus->SetQuit(nullptr);
	}
	if (DisplayModes)
	{
		Menus->SetDisplayModes([this]() { return DisplayModes(); });
	}
	else
	{
		Menus->SetDisplayModes(nullptr);
	}
	if (SettingsFollowed)
	{
		Menus->SetSettingsFollowed([this](const Weltall::FPlayerSettings& Changed) { SettingsFollowed(Changed); });
	}
	else
	{
		Menus->SetSettingsFollowed(nullptr);
	}
	if (!SettingsKept)
	{
		Menus->SetSettingsKept(nullptr);
		return;
	}
	Menus->SetSettingsKept([this](const Weltall::FPlayerSettings& Kept) { SettingsKept(Kept); });
}

void FGameLoopScreen::SetSaves(Weltall::FSaveStore* InSaves)
{
	Saves = InSaves;
	if (Session)
	{
		Session->SetSaves(InSaves);
	}
}

Weltall::FGameState* FGameLoopScreen::GameState()
{
	return Session ? &Session->Game() : nullptr;
}

Weltall::FGameLoop* FGameLoopScreen::Loop()
{
	return GameLoop.get();
}

bool FGameLoopScreen::RequestMenu(uint8 Mode, uint8 Parameter)
{
	Weltall::FFieldProgram* FieldProgram = GameLoop ? GameLoop->Field() : nullptr;
	if (FieldProgram == nullptr)
	{
		return false;
	}
	Weltall::FEventContext& Context = FieldProgram->Runtime().Context();
	Context.MenuRequest = Mode;
	Context.MenuProgramParam = Parameter;
	return true;
}

bool FGameLoopScreen::RequestBattle(uint8 Formation, uint8 Kind)
{
	Weltall::FFieldProgram* FieldProgram = GameLoop ? GameLoop->Field() : nullptr;
	if (FieldProgram == nullptr)
	{
		return false;
	}
	Weltall::FEventContext& Context = FieldProgram->Runtime().Context();
	Context.bBattleRequested = true;
	Context.Requests.bScriptedBattle = true;
	Context.Requests.bLeavingField = true;
	Context.Requests.BattleKind = Kind;
	Context.Encounters.Formation = Formation;
	return true;
}

bool FGameLoopScreen::RequestArena(const std::array<uint8, 6>& Match)
{
	Weltall::FFieldProgram* FieldProgram = GameLoop ? GameLoop->Field() : nullptr;
	if (FieldProgram == nullptr)
	{
		return false;
	}
	Weltall::FEventContext& Context = FieldProgram->Runtime().Context();
	Context.Requests.BattlingParameters = Match;
	Context.Requests.bLeavingField = true;
	Context.Requests.bBattling = true;
	return true;
}

bool FGameLoopScreen::ShowChapterCard(int32 Chapter, int32 Subtitle)
{
	if (!Session)
	{
		return false;
	}
	// A subtitle's card comes once its chapter's has been seen.
	Subtitle != 0 ? Session->ChapterCard().Know(Chapter) : Session->ChapterCard().Reset();
	Session->ChapterCard().Advance(Chapter, Subtitle, true, 0);
	return true;
}

TOptional<int32> FGameLoopScreen::WaitingMessageGlyphs() const
{
	const auto GlyphsShown = [](const Weltall::FMesWindow& Window)
	{
		int32 Glyphs = 0;
		for (int32 Row = 0; Row < Window.Lines(); ++Row)
		{
			Glyphs += static_cast<int32>(Window.Row(Row).Glyphs.size());
		}
		return Glyphs;
	};
	if (!GameLoop)
	{
		return {};
	}
	if (const Weltall::FFieldProgram* FieldProgram = GameLoop->Field())
	{
		for (const Weltall::FFieldMessageWindow& Window : FieldProgram->Runtime().Context().Messages.Windows)
		{
			if (Window.bOpen && Window.bTextWaits && Window.Text)
			{
				return GlyphsShown(*Window.Text);
			}
		}
	}
	if (const Weltall::FBattleGameProgram* BattleProgram = GameLoop->Battle())
	{
		const Weltall::FBattleEventWork& Event = BattleProgram->Runtime().Context().Event;
		if (Event.bKeyWait && Event.Text)
		{
			return GlyphsShown(*Event.Text);
		}
	}
	return {};
}

void FGameLoopScreen::SetMenuDrawingAreaWide(bool bWide)
{
	Menu->SetWideDrawingArea(bWide);
}

const Weltall::FPlayerSettings& FGameLoopScreen::Settings() const
{
	return Session ? Session->Settings() : StartSettings;
}

bool FGameLoopScreen::HasStopped() const
{
	return GameLoop && GameLoop->Stage() == Weltall::EGameLoopStage::Stopped;
}
