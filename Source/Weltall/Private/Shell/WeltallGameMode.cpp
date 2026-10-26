#include "Shell/WeltallGameMode.h"

#include "Audio/GameSoundSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Foundation/Result.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/ButtonGlyphSubsystem.h"
#include "Input/KeyListenerSubsystem.h"
#include "Input/WeltallPlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Menu/OptionsText.h"
#include "Screens/GameLoopScreen.h"
#include "Screens/GameScreenSubsystem.h"
#include "Shell/DeviceFilesSubsystem.h"
#include "Shell/GameDiscsSubsystem.h"
#include "Shell/GameModsSubsystem.h"
#include "Shell/GameAssetsSubsystem.h"
#include "Shell/GameNotice.h"
#include "Shell/GameWindowSubsystem.h"
#include "Shell/PlayerSettingsSubsystem.h"
#include "Shell/SaveStoreSubsystem.h"
#include "TimerManager.h"
#include "WeltallLog.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

#include <filesystem>
#include <string>

namespace
{
	FText TextOf(const std::string& Message)
	{
		return FText::FromString(FString(UTF8_TO_TCHAR(Message.c_str())));
	}

	std::string FileCount(int32 Count)
	{
		namespace Text = Weltall::OptionsText::Saves;
		return Count == 1 ? std::string(Text::OneFile) : Weltall::OptionsText::Fill(Text::ManyFiles, {std::to_string(Count)});
	}

	/** Why a transfer failed, as the end of a sentence. */
	std::string_view Reason(Weltall::ETransferFailure Failure)
	{
		const std::size_t Number = static_cast<std::size_t>(Failure);
		return Number == 0 || Number > Weltall::OptionsText::Discs::Reasons.size() ? std::string_view() : Weltall::OptionsText::Discs::Reasons[Number - 1];
	}
}

AWeltallGameMode::AWeltallGameMode()
{
	PlayerControllerClass = AWeltallPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}

bool AWeltallGameMode::PlayerCanRestart_Implementation(APlayerController*)
{
	return false;
}

void AWeltallGameMode::BeginPlay()
{
	Super::BeginPlay();
	// Nothing in the world moves by physics: its scene is never stepped.
	GetWorld()->bShouldSimulatePhysics = false;
	if (StartGame())
	{
		return;
	}
	UE_LOG(LogWeltall, Error, TEXT("No disc of the game is in %s"), *UGameDiscsSubsystem::GamesFolder());
	AwaitDiscs();
}

void AWeltallGameMode::AwaitDiscs()
{
	if (UDeviceFilesSubsystem::UsesPicker())
	{
		ShowDiscsPicked(false);
	}
	else
	{
		ShowNotice(DiscsMissing(), false);
	}
	GetWorldTimerManager().SetTimer(LookAgain, this, &AWeltallGameMode::LookForDiscs, 2.0f, true);
}

void AWeltallGameMode::ShowDiscsPicked(bool bContinue)
{
	namespace Text = Weltall::OptionsText::Discs;
	UDeviceFilesSubsystem* Picker = GetGameInstance()->GetSubsystem<UDeviceFilesSubsystem>();
	TArray<FNoticeButton> Buttons;
	Buttons.Add({TextOf(std::string(Text::AddFolderButton)), [Picker]() { Picker->Start(Weltall::EDeviceTransfer::DiscFolder); }});
	Buttons.Add({TextOf(std::string(Text::AddButton)), [Picker]() { Picker->Start(Weltall::EDeviceTransfer::Discs); }});
	if (bContinue)
	{
		// The notice goes with the button: after its click.
		Buttons.Add({TextOf(std::string(Text::ContinueButton)), [this]() { GetWorldTimerManager().SetTimerForNextTick(this, &AWeltallGameMode::ContinueWithOneDisc); }});
	}
	bContinueOffered = bContinue;
	ShowNotice(TextOf(std::string(Text::MissingPicked)), false, MoveTemp(Buttons), [this]() { return DiscsAdded(); });
}

void AWeltallGameMode::LookForDiscs()
{
	UGameDiscsSubsystem* Discs = GetGameInstance()->GetSubsystem<UGameDiscsSubsystem>();
	Discs->Rescan();
	const bool bFirst = Discs->Library().PlayableDisc(1) != nullptr;
	const bool bSecond = Discs->Library().PlayableDisc(2) != nullptr;
	// Discs added through the picker may come one at a time: the player is not hurried past the other.
	const bool bWaitForOther = UDeviceFilesSubsystem::UsesPicker() && bFirst != bSecond;
	if (bWaitForOther && !bContinueOffered)
	{
		ShowDiscsPicked(true);
		return;
	}
	if (bWaitForOther || (!bFirst && !bSecond))
	{
		if (NoticeBelowText)
		{
			NoticeBelow->SetText(NoticeBelowText());
		}
		else
		{
			NoticeText->SetText(DiscsMissing());
		}
		return;
	}
	GetWorldTimerManager().ClearTimer(LookAgain);
	HideNotice();
	StartGame();
}

void AWeltallGameMode::ContinueWithOneDisc()
{
	GetWorldTimerManager().ClearTimer(LookAgain);
	HideNotice();
	StartGame();
}

void AWeltallGameMode::FollowTransfer()
{
	const Weltall::FTransferStatus Status = GetGameInstance()->GetSubsystem<UDeviceFilesSubsystem>()->Status();
	if (Status.Revision == TransferSeen)
	{
		return;
	}
	TransferSeen = Status.Revision;
	if (Weltall::IsDiscTransfer(Status.Kind) && Status.State == Weltall::ETransferState::Done && GetWorldTimerManager().IsTimerActive(LookAgain))
	{
		LookForDiscs();
	}
	if (NoticeBelow && NoticeBelowText)
	{
		NoticeBelow->SetText(NoticeBelowText());
	}
}

void AWeltallGameMode::ShowNotice(const FText& Text, bool bOverPicture, TArray<FNoticeButton> Buttons, TFunction<FText()> Below)
{
	HideNotice();
	const FGameNotice Shown = MakeGameNotice(Text, bOverPicture, MoveTemp(Buttons));
	Notice = Shown.Widget;
	NoticeText = Shown.Text;
	NoticeBelow = Shown.Below;
	NoticeBelowText = MoveTemp(Below);
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->AddViewportWidgetContent(Shown.Widget);
	}
	if (!Shown.Buttons.IsEmpty())
	{
		// A controller or a keyboard presses the buttons too.
		FSlateApplication::Get().SetAllUserFocus(Shown.Buttons[0], EFocusCause::SetDirectly);
	}
	if (NoticeBelow && NoticeBelowText)
	{
		TransferSeen = GetGameInstance()->GetSubsystem<UDeviceFilesSubsystem>()->Status().Revision;
		NoticeBelow->SetText(NoticeBelowText());
		GetWorldTimerManager().SetTimer(FollowCopy, this, &AWeltallGameMode::FollowTransfer, 0.2f, true);
	}
}

void AWeltallGameMode::HideNotice()
{
	if (!Notice)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(FollowCopy);
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->RemoveViewportWidgetContent(Notice.ToSharedRef());
	}
	Notice.Reset();
	NoticeText.Reset();
	NoticeBelow.Reset();
	NoticeBelowText.Reset();
}

FText AWeltallGameMode::DiscsMissing() const
{
	namespace Text = Weltall::OptionsText::Discs;
	const std::u8string FolderName = std::filesystem::path(*UGameDiscsSubsystem::GamesFolder()).make_preferred().u8string();
	std::string Message = std::string(Text::Missing) + "\n\n" + Weltall::OptionsText::Fill(Text::WhereTheyGo, {std::string(FolderName.begin(), FolderName.end())});
	const std::string Found = DiscsFound();
	if (!Found.empty())
	{
		Message += "\n\n" + Found;
	}
	return TextOf(Message);
}

std::string AWeltallGameMode::DiscsFound() const
{
	namespace Text = Weltall::OptionsText::Discs;
	const Weltall::FGameLibrary& Library = GetGameInstance()->GetSubsystem<UGameDiscsSubsystem>()->Library();
	const std::filesystem::path Folder = std::filesystem::path(*UGameDiscsSubsystem::GamesFolder()).make_preferred();
	// Files by their place in the Games folder.
	const auto Shown = [&Folder](const std::filesystem::path& File)
	{
		const std::filesystem::path Relative = File.lexically_relative(Folder);
		const std::u8string Name = (Relative.empty() ? File : Relative).u8string();
		return std::string(Name.begin(), Name.end());
	};
	std::string Message;
	const auto Paragraph = [&Message](const std::string& Lines)
	{
		Message += (Message.empty() ? "" : "\n\n") + Lines;
	};
	for (const Weltall::EDiscRelease Release : Library.Releases())
	{
		const std::string_view Number = Weltall::DiscNumberOfRelease(Release) == 1 ? "1" : "2";
		Paragraph(Weltall::OptionsText::Fill(Text::Found, {Number, Shown(Library.FileOf(Release))}));
	}
	constexpr std::size_t MostListed = 12;
	if (!Library.Notes().empty())
	{
		std::string NotUsed(Text::NotUsed);
		for (std::size_t Index = 0; Index < Library.Notes().size() && Index < MostListed; ++Index)
		{
			const Weltall::FLibraryNote& Note = Library.Notes()[Index];
			NotUsed += "\n" + Weltall::OptionsText::Fill(Text::NotUsedFile, {Shown(Note.File), Note.Text});
		}
		if (Library.Notes().size() > MostListed)
		{
			NotUsed += "\n" + Weltall::OptionsText::Fill(Text::More, {std::to_string(Library.Notes().size() - MostListed)});
		}
		Paragraph(NotUsed);
	}
	return Message;
}

FText AWeltallGameMode::DiscsAdded() const
{
	namespace Text = Weltall::OptionsText::Discs;
	const Weltall::FTransferStatus Status = GetGameInstance()->GetSubsystem<UDeviceFilesSubsystem>()->Status();
	std::string Message;
	switch (Weltall::IsDiscTransfer(Status.Kind) ? Status.State : Weltall::ETransferState::Idle)
	{
	case Weltall::ETransferState::Copying:
		if (Status.Kind == Weltall::EDeviceTransfer::DiscFolder && Status.Files == 0)
		{
			// A folder's disc images are counted before any is copied.
			Message = Weltall::OptionsText::Fill(Text::Scanning, {Status.Name, std::to_string(Status.File)});
		}
		else if (Status.Files > 1)
		{
			Message = Weltall::OptionsText::Fill(Text::CopyingMany, {Status.Name, std::to_string(Status.File), std::to_string(Status.Files), std::to_string(Status.Percent())});
		}
		else
		{
			Message = Weltall::OptionsText::Fill(Text::CopyingOne, {Status.Name, std::to_string(Status.Percent())});
		}
		break;
	case Weltall::ETransferState::Done:
		Message = Weltall::OptionsText::Fill(Text::Copied, {FileCount(Status.Files)});
		break;
	case Weltall::ETransferState::Failed:
		if (Status.Failure == Weltall::ETransferFailure::NoDiscImages)
		{
			Message = Weltall::OptionsText::Fill(Text::NoNewDiscs, {Status.Name});
			break;
		}
		Message = Status.Name.empty() ? Weltall::OptionsText::Fill(Text::NotCopiedNoName, {Reason(Status.Failure)})
									  : Weltall::OptionsText::Fill(Text::NotCopied, {Status.Name, Reason(Status.Failure)});
		break;
	default:
		break;
	}
	const Weltall::FGameLibrary& Library = GetGameInstance()->GetSubsystem<UGameDiscsSubsystem>()->Library();
	const bool bFirst = Library.PlayableDisc(1) != nullptr;
	if (bFirst != (Library.PlayableDisc(2) != nullptr))
	{
		const std::string_view Ready = bFirst ? "1" : "2";
		Message += (Message.empty() ? "" : "\n\n") + Weltall::OptionsText::Fill(Text::OneDiscReady, {Ready, bFirst ? "2" : "1", Ready});
	}
	const std::string Found = DiscsFound();
	if (!Found.empty())
	{
		Message += (Message.empty() ? "" : "\n\n") + Found;
	}
	return TextOf(Message);
}

void AWeltallGameMode::ShowStopped(const Weltall::FError& Error, bool bStarted)
{
	if (!UDeviceFilesSubsystem::UsesPicker())
	{
		ShowNotice(GameStoppedText(Error, bStarted), bStarted);
		return;
	}
	UDeviceFilesSubsystem* Picker = GetGameInstance()->GetSubsystem<UDeviceFilesSubsystem>();
	TArray<FNoticeButton> Buttons;
	Buttons.Add({TextOf(std::string(Weltall::OptionsText::Stop::SaveLogButton)), [Picker]() { Picker->Start(Weltall::EDeviceTransfer::Log); }});
	ShowNotice(GameStoppedText(Error, bStarted, true), bStarted, MoveTemp(Buttons), [this]() { return LogSaved(); });
}

FText AWeltallGameMode::LogSaved() const
{
	namespace Text = Weltall::OptionsText::Stop;
	const Weltall::FTransferStatus Status = GetGameInstance()->GetSubsystem<UDeviceFilesSubsystem>()->Status();
	if (Status.Kind != Weltall::EDeviceTransfer::Log)
	{
		return FText::GetEmpty();
	}
	switch (Status.State)
	{
	case Weltall::ETransferState::Done:
		return TextOf(std::string(Text::LogSaved));
	case Weltall::ETransferState::Failed:
		return TextOf(Weltall::OptionsText::Fill(Text::LogNotSaved, {Reason(Status.Failure)}));
	default:
		return FText::GetEmpty();
	}
}

bool AWeltallGameMode::StartGame()
{
	UGameInstance* Game = GetGameInstance();
	UGameDiscsSubsystem* Discs = Game->GetSubsystem<UGameDiscsSubsystem>();
	const Weltall::FIso9660* FirstDisc = Discs->Library().PlayableDisc(1);
	const Weltall::FIso9660* SecondDisc = Discs->Library().PlayableDisc(2);
	const Weltall::FIso9660* Disc = FirstDisc != nullptr ? FirstDisc : SecondDisc;
	if (Disc == nullptr)
	{
		return false;
	}
	Game->GetSubsystem<UGameWindowSubsystem>()->ShowGameIcon(*Disc);
	// The discs found since the game started (the first, looked for again) take the mods' files before they are read.
	UGameModsSubsystem* Mods = Game->GetSubsystem<UGameModsSubsystem>();
	Mods->Refresh();
	// The game runs from power-on, its sound started with it and started again when the console restarts.
	UGameSoundSubsystem* Sound = Game->GetSubsystem<UGameSoundSubsystem>();
	FGameLoopScreen::FStartSound StartSound = [Sound, Disc]() { return Sound->Start(*Disc) ? Sound->Session() : nullptr; };
	UPlayerSettingsSubsystem* Settings = Game->GetSubsystem<UPlayerSettingsSubsystem>();
	// Disc 2 is read beside disc 1 when the player has it, so the disc change's "INSERT DISC 2" is only confirmed;
	// without it the Games folder is looked in again as the game asks for it, as it is for disc 1 when the game
	// starts from disc 2 (its New Game and disc 1's saves ask for disc 1).
	auto Started = FGameLoopScreen::PowerOn(*Disc, MoveTemp(StartSound), Settings->Settings(), FirstDisc != nullptr ? SecondDisc : nullptr);
	if (!Started)
	{
		UE_LOG(LogWeltall, Error, TEXT("The game cannot start: %hs"), Started.Error().Message.c_str());
		ShowStopped(Started.Error(), false);
		return true;
	}
	const TSharedRef<FGameLoopScreen> Screen = *Started;
	Screen->SetButtonGlyphs(Game->GetSubsystem<UButtonGlyphSubsystem>());
	Screen->SetSaves(&Game->GetSubsystem<USaveStoreSubsystem>()->Store());
	Screen->SetCameraMoved([Settings](const Weltall::FFieldCameraOffsets& Field, const Weltall::FWorldMapCameraOffsets& WorldMap)
	{
		Settings->KeepCameraOffsets(Field, WorldMap);
	});
	Screen->SetBattleSpeedStepped([Settings](Weltall::EBattleSpeed Speed) { Settings->KeepBattleSpeed(Speed); });
	Screen->SetMenuKeys(Game->GetSubsystem<UKeyListenerSubsystem>());
	UGameAssetsSubsystem* Assets = Game->GetSubsystem<UGameAssetsSubsystem>();
	Screen->SetMenuAssets(Assets);
	Screen->SetMenuDeviceFiles(Game->GetSubsystem<UDeviceFilesSubsystem>()->Files());
	Screen->SetMenuMods([Mods]() { return Mods->MenuMods(); });
	Screen->SetPerfectWorksSubtitles(Mods->PerfectWorksSubtitles(), Mods->MenuMods().PerfectWorksInUse.bEnabled);
	Screen->SetDisplayModes([Window = Game->GetSubsystem<UGameWindowSubsystem>()]() { return Window->Modes(); });
	Screen->SetSettingsFollowed([Settings](const Weltall::FPlayerSettings& Changed) { Settings->Follow(Changed); });
	Screen->SetSettingsKept([Settings](const Weltall::FPlayerSettings& Kept) { Settings->Change(Kept); });
	Screen->SetQuit([World = TWeakObjectPtr<UWorld>(GetWorld())]()
	{
		if (UWorld* Playing = World.Get())
		{
			UKismetSystemLibrary::QuitGame(Playing, nullptr, EQuitPreference::Quit, false);
		}
	});
	Screen->SetDiscSought([Discs, Assets, Mods](Weltall::EDiscRelease Release)
	{
		Discs->Rescan();
		Assets->Refresh();
		Mods->Refresh();
		return Discs->Find(Release);
	});
	Screen->SetStopped([Mode = TWeakObjectPtr<AWeltallGameMode>(this)](const Weltall::FError& Error)
	{
		if (AWeltallGameMode* Playing = Mode.Get())
		{
			Playing->ShowStopped(Error, true);
		}
	});
	Game->GetSubsystem<UGameScreenSubsystem>()->Show(Screen);
	SettingsChanged = Settings->OnSettingsChanged.AddLambda([Settings, Weak = Screen.ToWeakPtr()]()
	{
		if (const TSharedPtr<FGameLoopScreen> Shown = Weak.Pin())
		{
			Shown->ApplySettings(Settings->Settings());
		}
	});
	return true;
}

void AWeltallGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(LookAgain);
	HideNotice();
	GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>()->OnSettingsChanged.Remove(SettingsChanged);
	GetGameInstance()->GetSubsystem<UGameScreenSubsystem>()->Close();
	Super::EndPlay(EndPlayReason);
}
