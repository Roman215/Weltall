#include "Game/GameSession.h"

#include "Game/BattleGameFiles.h"
#include "Game/BattlingGameFiles.h"
#include "Game/ChapterCardDrawing.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Story/StoryPacks.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		/** The saved event variable that counts the story's progress. */
		constexpr int32 StoryProgressVariable = 0;

		int32 DiscNumberOf(EDiscRelease Release)
		{
			return DiscNumberOfRelease(Release) - 1;
		}
	}

	FGameSession::FGameSession(const FCdImage& Image, const FGameFileIndex& Index, EDiscRelease Release, FGameSoundSession& Sound, FGteArctangents Arctangents,
		FMesResident Text, FFieldDrawingTables Tables, FFieldMovieCues Cues, std::unique_ptr<FBattleGameFiles> Battle)
		: DiscImage(Image), FileIndex(Index), DiscRelease(Release), GameSound(Sound), GteArctangents(std::move(Arctangents)), TextResources(std::move(Text)),
		  DrawingTables(std::move(Tables)), MovieCues(std::move(Cues)), BattleData(std::move(Battle))
	{
		Discs[static_cast<std::size_t>(DiscNumberOf(Release))] = FGameDisc{&Image, &Index};
		const std::size_t Language = Release == EDiscRelease::UndubDisc1 || Release == EDiscRelease::UndubDisc2 ? 1 : 0;
		VoiceDiscs[Language][static_cast<std::size_t>(DiscNumberOf(Release))] = {&Image, &Index};
		// The game stops at the cases no script reaches until the host lets play carry past them.
		UnreachedCases.bStops = true;
	}

	FGameSession::~FGameSession() = default;

	TResult<std::unique_ptr<FGameSession>> FGameSession::PowerOn(const FCdImage& Image, const FGameFileIndex& Index, const FPsxExecutable& Executable,
		EDiscRelease Release, FGameSoundSession& Sound)
	{
		if (!IsPlayableRelease(Release))
		{
			return MakeError("playing requires a USA or supported undub disc");
		}
		const EDiscRelease Layout = GameplayRelease(Release);
		TResult<FGteArctangents> Arctangents = FGteArctangents::Read(Executable);
		if (!Arctangents)
		{
			return Arctangents.Error();
		}
		TResult<FMesResident> Text = LoadMesResident(Image, Index, Executable);
		if (!Text)
		{
			return Text.Error();
		}
		TResult<FFieldDrawingTables> Tables = FFieldDrawingTables::Read(Image, Index, Executable, Layout);
		if (!Tables)
		{
			return Tables.Error();
		}
		TResult<FFieldMovieCues> Cues = FFieldMovieCues::Read(Image, Index, Layout);
		if (!Cues)
		{
			return Cues.Error();
		}
		TResult<std::unique_ptr<FBattleGameFiles>> Battle = FBattleGameFiles::Load(Image, Index, Executable);
		if (!Battle)
		{
			return Battle.Error();
		}
		TResult<std::unique_ptr<FBattlingGameFiles>> Battling = FBattlingGameFiles::Load(Image, Index, Executable, Layout);
		if (!Battling)
		{
			return Battling.Error();
		}
		TResult<FChapterTitles> Chapters = FChapterTitles::Read(Image, Index);
		if (!Chapters)
		{
			return Chapters.Error();
		}
		std::unique_ptr<FGameSession> Session(new FGameSession(
			Image, Index, Release, Sound, std::move(*Arctangents), std::move(*Text), std::move(*Tables), std::move(*Cues), std::move(*Battle)));
		Session->KernelGlobals = FKernelState::PowerOn(DiscNumberOf(Release));
		Session->BattlingData = std::move(*Battling);
		Session->ChapterList = std::move(*Chapters);
		if (auto Error = Session->InitNewGame())
		{
			return *Error;
		}
		return Session;
	}

	void FGameSession::AddDisc(int32 Number, const FCdImage& Image, const FGameFileIndex& Index)
	{
		if (Number < 0 || Number >= static_cast<int32>(Discs.size()))
		{
			return;
		}
		Discs[static_cast<std::size_t>(Number)] = FGameDisc{&Image, &Index};
	}

	void FGameSession::AddVoiceDisc(EVoiceLanguage Language, int32 Number, const FCdImage& Image, const FGameFileIndex& Index)
	{
		if (Language == EVoiceLanguage::Automatic || Number < 0 || Number >= 2)
		{
			return;
		}
		VoiceDiscs[Language == EVoiceLanguage::Japanese ? 1 : 0][static_cast<std::size_t>(Number)] = {&Image, &Index};
		FollowVoiceLanguage();
	}

	EVoiceLanguage FGameSession::SelectedVoiceLanguage() const
	{
		EVoiceLanguage Language = PlayerSettings.VoiceLanguage;
		if (Language == EVoiceLanguage::Automatic)
		{
			Language = DiscRelease == EDiscRelease::UndubDisc1 || DiscRelease == EDiscRelease::UndubDisc2 ? EVoiceLanguage::Japanese : EVoiceLanguage::English;
		}
		return Language;
	}

	FGameDisc FGameSession::VoiceDisc(int32 Number) const
	{
		if (Number < 0 || Number >= static_cast<int32>(Discs.size())) return {};
		const FGameDisc Selected = VoiceDiscs[SelectedVoiceLanguage() == EVoiceLanguage::Japanese ? 1 : 0][static_cast<std::size_t>(Number)];
		return Selected.Image != nullptr ? Selected : Discs[static_cast<std::size_t>(Number)];
	}

	void FGameSession::FollowVoiceLanguage()
	{
		const auto& Tracks = VoiceDiscs[SelectedVoiceLanguage() == EVoiceLanguage::Japanese ? 1 : 0];
		FGameDisc Selected = Tracks[0].Image != nullptr ? Tracks[0] : Tracks[1];
		if (Selected.Image == nullptr) Selected = Discs[0].Image != nullptr ? Discs[0] : Discs[1];
		if (Selected.Image != nullptr)
		{
			BattleData->SpriteFiles.SetVoiceFiles(*Selected.Image, *Selected.Index);
			BattleData->EventFiles.SetVoiceFiles(*Selected.Image, *Selected.Index);
		}
	}

	TResult<FGameDisc> FGameSession::MovieDisc() const
	{
		const TResult<FGameDisc> Gameplay = DriveDisc();
		if (!Gameplay)
		{
			return Gameplay.Error();
		}
		return VoiceDisc(DiscNumber());
	}

	void FGameSession::SetPerfectWorksSubtitles(std::array<const FPerfectWorksSubtitles*, 2> Subtitles, bool bInUse)
	{
		PerfectWorksSubtitles = Subtitles;
		bPerfectWorksInUse = bInUse;
	}

	TResult<std::optional<FMovieSubtitles>> FGameSession::MovieSubtitles(int32 Number) const
	{
		const int32 Disc = DiscNumber();
		if (PlayerSettings.MovieSubtitles == EMovieSubtitles::Off || Disc < 0 || Disc >= static_cast<int32>(Discs.size()))
		{
			return std::optional<FMovieSubtitles>();
		}
		// The movie's own disc's subtitles where it carries them (the undub's, a translation's for its own sound); with
		// On, the undub's over the English sound too.
		const FGameDisc Movie = VoiceDisc(Disc);
		const bool bOwn = Movie.Image != nullptr && !Movie.Index->MovieSubtitles().Entries().empty();
		if (PlayerSettings.MovieSubtitles == EMovieSubtitles::Automatic && !bOwn)
		{
			return std::optional<FMovieSubtitles>();
		}
		const FGameDisc& Undub = bOwn ? Movie : VoiceDiscs[1][static_cast<std::size_t>(Disc)];
		const FPerfectWorksSubtitles* Mod = PerfectWorksSubtitles[static_cast<std::size_t>(Disc)];
		const EMovieSubtitleSource Source = PlayerSettings.MovieSubtitleSource;
		const bool bModFirst = Source == EMovieSubtitleSource::PerfectWorks || (Source == EMovieSubtitleSource::Automatic && bPerfectWorksInUse);
		const bool bFromMod = Mod != nullptr && (bModFirst || Undub.Image == nullptr);
		const FMovieSubtitleEntry* Entry = bFromMod ? Mod->Table().Find(Number) : Undub.Image != nullptr ? Undub.Index->MovieSubtitles().Find(Number) : nullptr;
		if (Entry == nullptr)
		{
			return std::optional<FMovieSubtitles>();
		}
		TResult<FMovieSubtitles> MovieLines = bFromMod ? Mod->Load(*Entry) : FMovieSubtitles::Load(*Undub.Image, *Undub.Index, *Entry);
		if (!MovieLines)
		{
			return MovieLines.Error();
		}
		return std::optional<FMovieSubtitles>(std::move(*MovieLines));
	}

	FSoundSources FGameSession::SoundSources() const
	{
		const auto Given = [](const std::array<FGameDisc, 2>& Track) { return Track[0].Image != nullptr || Track[1].Image != nullptr; };
		const auto Subtitled = [](const std::array<FGameDisc, 2>& Track) {
			return std::any_of(Track.begin(), Track.end(), [](const FGameDisc& Disc) { return Disc.Image != nullptr && !Disc.Index->MovieSubtitles().Entries().empty(); });
		};
		const bool bUsaPlayed = DiscRelease == EDiscRelease::UsaDisc1 || DiscRelease == EDiscRelease::UsaDisc2;
		return {Given(VoiceDiscs[0]) || bUsaPlayed, Given(VoiceDiscs[1]), PerfectWorksSubtitles[0] != nullptr || PerfectWorksSubtitles[1] != nullptr,
			bPerfectWorksInUse, Subtitled(VoiceDiscs[0])};
	}

	TResult<FGameDisc> FGameSession::DriveDisc() const
	{
		const int32 Number = DiscNumber();
		if (Number < 0 || Number >= static_cast<int32>(Discs.size()) || Discs[static_cast<std::size_t>(Number)].Image == nullptr)
		{
			return MakeError("the game is on disc " + std::to_string(Number + 1) + ", which was not given");
		}
		return Discs[static_cast<std::size_t>(Number)];
	}

	bool FGameSession::HasDisc(int32 Number)
	{
		if (Number < 0 || Number >= static_cast<int32>(Discs.size()))
		{
			return false;
		}
		if (Discs[static_cast<std::size_t>(Number)].Image == nullptr && DiscDrive != nullptr)
		{
			DiscDrive->Seek(Number);
		}
		return Discs[static_cast<std::size_t>(Number)].Image != nullptr;
	}

	void FGameSession::SetSettings(const FPlayerSettings& Settings)
	{
		PlayerSettings = Settings;
		FollowVoiceLanguage();
		++SettingsSet;
		GameSound.Apply(Settings.SoundOutput, Settings.Volumes, Settings.Fixes.SoundDriver, Settings.Fixes.Music);
	}

	void FGameSession::Vblank(FPadState Held)
	{
		const bool bPlayersTime = ++PaceVblanks >= Pace;
		if (bPlayersTime)
		{
			PaceVblanks = 0;
			// The console counts the vertical blank before it reads the pad: the repeat keeps to the count.
			State.SetPlayTime(State.PlayTime() + 1);
		}
		FPadState Second;
		if (SecondPadCount > 0)
		{
			Second = SecondPadHeld[static_cast<std::size_t>(std::min(SecondPadTaken, SecondPadCount - 1))];
			++SecondPadTaken;
		}
		// Run fast, the pad's count goes on by one a vertical blank and meets the play time's at each one it counts.
		KernelPad.Vblank(Held, State.PlayTime() * static_cast<uint32>(Pace) + static_cast<uint32>(PaceVblanks), Second);
		if (bPlayersTime)
		{
			PlayClock.Vblank();
		}
		KernelVibration.Vblank();
	}

	void FGameSession::SetPace(int32 InPace)
	{
		if (InPace != Pace)
		{
			Pace = InPace;
			PaceVblanks = 0;
			KernelPad.SetRepeatPace(Pace);
		}
	}

	void FGameSession::SetSecondPad(std::span<const FPadState> HeldPerVblank)
	{
		SecondPadCount = static_cast<int32>(std::min(HeldPerVblank.size(), SecondPadHeld.size()));
		std::copy_n(HeldPerVblank.begin(), SecondPadCount, SecondPadHeld.begin());
		SecondPadTaken = 0;
	}

	std::optional<FError> FGameSession::InitNewGame()
	{
		TResult<FGameState> NewGame = LoadNewGameState(Image(), Index(), TextResources.NameTables);
		if (!NewGame)
		{
			return NewGame.Error();
		}
		// Only the state block and the battles' globals are loaded: the kernel's counts and the last program's word go on.
		const uint32 PlayTime = State.PlayTime();
		const uint8 ProgramResult = State.ProgramResult;
		const uint16 ActingTurns = State.BattleActingTurns;
		State = std::move(*NewGame);
		State.SetPlayTime(PlayTime);
		State.ProgramResult = ProgramResult;
		State.BattleActingTurns = ActingTurns;
		KernelGlobals.ChainedBattle = 0;
		KernelGlobals.InitNewGame();
		Card.Reset();
		return std::nullopt;
	}

	int32 FGameSession::Chapter() const
	{
		const FChapter* Found = ChapterList.Find(State.SavedEventVariable(StoryProgressVariable));
		return Found != nullptr ? Found->Number : 0;
	}

	int32 FGameSession::SubtitleAt(uint16 Story) const
	{
		const FChapter* Chapter = ChapterList.Find(Story);
		const FStorySubtitle* Under = StoryPacks != nullptr ? StoryPacks->SubtitleAt(Story, State.StoryMode() == EStoryMode::Expanded) : nullptr;
		return Chapter != nullptr && Under != nullptr && Under->First >= Chapter->FirstProgress ? Under->First : 0;
	}

	int32 FGameSession::Subtitle() const
	{
		return SubtitleAt(State.SavedEventVariable(StoryProgressVariable));
	}

	std::string FGameSession::ChapterCardLine() const
	{
		const std::span<const FChapter> List = ChapterList.Chapters();
		if (Card.Shown() <= 0 || Card.Shown() > static_cast<int32>(List.size()))
		{
			return {};
		}
		const FChapter& Shown = List[static_cast<std::size_t>(Card.Shown() - 1)];
		const uint16 First = static_cast<uint16>(Card.ShownSubtitle());
		const FStorySubtitle* Subtitle = First != 0 && StoryPacks != nullptr ? StoryPacks->SubtitleAt(First, true) : nullptr;
		return Subtitle != nullptr ? FChapterCardDrawing::Text(Shown, StoryPacks->SubtitlePart(First, Shown.FirstProgress), Subtitle->Text)
								   : FChapterCardDrawing::Text(Shown);
	}
}
