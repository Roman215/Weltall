#include "Menu/MenuMain.h"
#include "Menu/MenuDeviceFiles.h"
#include "Menu/MenuFileBrowser.h"
#include "Menu/OptionsText.h"

#include <array>
#include <optional>
#include <string>

namespace Weltall
{
	namespace
	{
		constexpr int32 MessageWindow = 2;
		constexpr int16 MessageX = 0x7A;
		constexpr int16 MessageY = 0x96;
		constexpr uint16 MessageWidth = 0xBC;
		constexpr uint16 MessageHeight = 0x40;
		constexpr int32 PiecesEntry = 4;
		constexpr int32 LinesX = 0x84;
		constexpr int32 LinesY = 0xA0;
		/** The pointers beside a message's two options: the left one, the right one. */
		constexpr std::size_t LeftPointer = 2;
		constexpr std::size_t RightPointer = 3;
		/** A notice left alone ends after this many checks. */
		constexpr int32 NoticeChecks = 180;
		/** The disc change's prompts: "Save?" (its options "No … Yes"), then "Are you sure you don't need to save?". */
		constexpr uint8 SaveText = 0x7D;
		constexpr uint8 SkipSaveText = 0x80;
		/** The disc the change goes on with: the second. */
		constexpr int32 SecondDisc = 1;

		/** The port's questions: the window across the screen's middle, the line, the answers side by side, the lines on one under them. */
		constexpr int16 OfferX = 0x18;
		constexpr int16 OfferY = 0x34;
		constexpr uint16 OfferWidth = 0x110;
		constexpr uint16 OfferHeight = 0x78;
		constexpr int32 OfferTextX = OfferX + 0x10;
		constexpr int32 OfferQuestionY = OfferY + 0x0C;
		constexpr int32 OfferAnswersY = OfferY + 0x28;
		constexpr std::array<int32, 2> OfferAnswerX = {OfferX + 0x24, OfferX + 0xAC};
		constexpr int32 OfferHelpY = OfferY + 0x44;
		constexpr int32 OfferHelpStep = 0x0E;
		constexpr int32 OfferLineRows = 13;
		/** The pointer beside an answer: a little past its last letter, at the middle of its line, as the messages' pointers stand by theirs. */
		constexpr int32 OfferPointerGap = 3;
		constexpr int32 OfferPointerDrop = 8;
		/** The question's lines among the labels: the line, the two answers, then the three on the answer under the pointer. */
		constexpr std::size_t OfferQuestion = 0;
		constexpr std::size_t OfferLeft = 1;
		constexpr std::size_t OfferRight = 2;
		constexpr std::size_t OfferHelp = 3;
	}

	TMenuTask<> FMenuMain::ChangeDisc()
	{
		bCardWatchInAsk = false;
		OpenLayer();
		while (LayerState != EMenuLayer::Still)
		{
			co_await Frame();
		}
		InitTargetPointers(0);
		// "Save?" left on its right option saves; its left option or Cancel asks whether to go on without.
		bool bSaves = true;
		const uint8 Answer = co_await Ask(SaveText);
		if ((Answer != 0 || bAskCancelled) && co_await Ask(SkipSaveText) != 0)
		{
			bSaves = false;
		}
		Flags[MenuFlag::TargetPointers] = 0;
		if (bSaves)
		{
			// Saved as disc 2's, and the File screen stays after a save for another.
			bSaveAsSecondDisc = true;
			bFileStaysAfterSave = true;
			co_await RunCommand(0);
			bFileStaysAfterSave = false;
			bSaveAsSecondDisc = false;
		}
		Flags[MenuFlag::Pointer] = 0;
		Flags[MenuFlag::Selection] = 0;
		co_await RequireDisc(SecondDisc, true);
	}

	TMenuTask<> FMenuMain::RequireLoadedGameDisc()
	{
		// The disc change's own save is made as disc 2's on the forest where the change opens (map 612), the story still
		// at the change; its first script sends it on to disc 2's opening at once, so its load is the change itself.
		constexpr uint16 DiscChangeMap = 612;
		constexpr uint16 PlaceMapBits = 0x3FFF;
		const FGameState& Game = *Services.Game;
		const bool bResumesInChange = Game.SavedDisc() == SecondDisc && (Game.Place() & PlaceMapBits) == DiscChangeMap
			&& Game.SavedEventVariable(0) == DiscChangeStoryProgress;
		co_await RequireDisc(Game.SavedDisc(), bResumesInChange);
	}

	TMenuTask<> FMenuMain::SaveClearFile()
	{
		bCardWatchInAsk = false;
		OpenLayer();
		while (LayerState != EMenuLayer::Still)
		{
			co_await Frame();
		}
		// The disc change's "Save?" first, its left option or Cancel asking whether to go on without; then the files,
		// which Cancel leaves once a save is made, and before it asks that again ("Are you sure you don't need to save?",
		// No back to the files).
		InitTargetPointers(0);
		const uint8 Answer = co_await Ask(SaveText);
		bool bSaves = (Answer == 0 && !bAskCancelled) || co_await Ask(SkipSaveText) == 0;
		Flags[MenuFlag::TargetPointers] = 0;
		bFileStaysAfterSave = true;
		while (bSaves)
		{
			co_await RunCommand(0);
			if (bFileSaved)
			{
				break;
			}
			InitTargetPointers(0);
			bSaves = co_await Ask(SkipSaveText) == 0;
			Flags[MenuFlag::TargetPointers] = 0;
		}
		bFileStaysAfterSave = false;
		Flags[MenuFlag::Pointer] = 0;
		Flags[MenuFlag::Selection] = 0;
	}

	TMenuTask<uint8> FMenuMain::Ask(uint8 FirstText, uint8 SecondText, bool bWaits)
	{
		co_await ShowMessage(FirstText);
		TargetPointerShown[RightPointer] = true;
		uint8 Answer = co_await Choose(bWaits);
		co_await CloseMessage();
		if (SecondText != NoSecondText)
		{
			if (Answer == 0)
			{
				co_return uint8{0};
			}
			co_await ShowMessage(SecondText);
			TargetPointerShown[RightPointer] = true;
			Answer = co_await Choose(bWaits);
			co_await CloseMessage();
		}
		co_return Answer;
	}

	TMenuTask<uint8> FMenuMain::Choose(bool bWaits)
	{
		// Left picks the left option (1), Right the right one (0), which Cancel also answers. The questions of
		// the arc's commands have the File screen's lists read while they wait.
		if (bCardWatchInAsk)
		{
			Files.Listing = 2;
		}
		bAskCancelled = false;
		uint8 Answer = 0;
		bool bAsks = true;
		int32 Checks = NoticeChecks;
		while (bAsks)
		{
			if (!bWaits)
			{
				TargetPointerShown[LeftPointer] = false;
				TargetPointerShown[RightPointer] = false;
				if (Code != EMenuCode::None || --Checks == 0)
				{
					break;
				}
			}
			co_await Frame();
			switch (Code)
			{
			case EMenuCode::Left:
				TargetPointerShown[LeftPointer] = true;
				TargetPointerShown[RightPointer] = false;
				Answer = 1;
				break;
			case EMenuCode::Right:
				TargetPointerShown[LeftPointer] = false;
				TargetPointerShown[RightPointer] = true;
				Answer = 0;
				break;
			case EMenuCode::Confirm:
				bAsks = false;
				break;
			case EMenuCode::Cancel:
				Answer = 0;
				bAskCancelled = true;
				bAsks = false;
				break;
			default:
				break;
			}
		}
		TargetPointerShown[LeftPointer] = false;
		TargetPointerShown[RightPointer] = false;
		Files.Listing = 0;
		co_return Answer;
	}

	TMenuTask<> FMenuMain::ShowMessage(uint8 FirstText)
	{
		OpenWindow(MessageWindow, MessageX, MessageY, MessageWidth, MessageHeight, true, true, PiecesEntry, false);
		while (!Growths[MessageWindow].bGrown)
		{
			co_await Frame();
		}
		RenderPromptLines(MessageLines, FirstText, LinesX, LinesY);
		bMessageShown = true;
		co_await Frame();
		co_await Frame();
	}

	void FMenuMain::SayDiscNotFound(uint8 FirstText)
	{
		constexpr uint8 BuzzerEffect = 4;
		PlaySound(BuzzerEffect);
		RenderPromptLines(MessageLines, FirstText, LinesX, LinesY, OptionsText::Discs::NotFound);
	}

	TMenuTask<bool> FMenuMain::AddDiscThroughPicker(int32 Disc, uint8 FirstText)
	{
		namespace Text = OptionsText::Discs;
		IMenuDeviceFiles* Picker = Services.Settings != nullptr ? Services.Settings->DeviceFiles() : nullptr;
		if (Picker == nullptr || !Picker->Start(EDeviceTransfer::Discs))
		{
			SayDiscNotFound(FirstText);
			co_return false;
		}
		uint64 Told = 0;
		FTransferStatus Status = Picker->Status();
		for (; Status.Running(); Status = Picker->Status())
		{
			if (Status.State == ETransferState::Copying && Status.Revision != Told)
			{
				Told = Status.Revision;
				RenderPromptLines(MessageLines, FirstText, LinesX, LinesY, OptionsText::Fill(Text::PromptCopying, {std::to_string(Status.Percent())}));
			}
			co_await Frame();
		}
		if (Services.Discs->HasDisc(Disc))
		{
			co_return true;
		}
		// Picked and copied but not the disc asked for, or not copied: the buzzer; closed with nothing picked: Confirm again.
		std::string_view Line = Text::PromptPickAgain;
		if (Status.State == ETransferState::Done || Status.State == ETransferState::Failed)
		{
			constexpr uint8 BuzzerEffect = 4;
			PlaySound(BuzzerEffect);
			Line = Status.State == ETransferState::Done ? Text::PromptNotPicked : Text::PromptNotCopied;
		}
		RenderPromptLines(MessageLines, FirstText, LinesX, LinesY, Line);
		co_return false;
	}

	TMenuTask<> FMenuMain::CloseMessage()
	{
		if (Flags[static_cast<std::size_t>(MenuFlag::Windows + MessageWindow)] != 0)
		{
			CloseWindow(MessageWindow);
			bMessageShown = false;
		}
		co_await Frame();
	}

	bool FMenuMain::OffersStoryMode() const
	{
		const FGameState& Game = *Services.Game;
		const bool bArrives = Request.Mode == EMenuMode::DiscChange || bGameLoaded;
		return Services.bStoryExpansion && bArrives && Services.Disc == SecondDisc + 1 && Game.StoryMode() == EStoryMode::Unchosen
			&& Game.SavedEventVariable(0) == DiscChangeStoryProgress;
	}

	TMenuTask<> FMenuMain::OfferStoryMode()
	{
		namespace Text = OptionsText::SecondDisc;
		constexpr std::size_t NameLetters = 22;
		// A mod's expansion is named under Extended; Menu opens the Modding page to choose another, then asks again. Turned
		// off there, none is offered: nothing is chosen, as where none ever was.
		while (const std::optional<FMenuExpansion> Offered = Services.OfferedExpansion ? Services.OfferedExpansion() : std::nullopt)
		{
			const bool bModding = !Offered->Folder.empty() && Services.Settings != nullptr;
			const std::array<std::string, 3> Help = Offered->Folder.empty() ? OptionsText::Fill(Text::ExtendedHelp)
				: OptionsText::Fill(Text::ExtendedNamed, {FFileBrowser::ShortDisplayName(Offered->Info.Name, NameLetters)});
			OptionsText::FOffer Offer = Text::Offer;
			Offer.RightHelp = {Help[0], Help[1], bModding || Offered->Folder.empty() ? std::string_view(Help[2]) : std::string_view()};
			const EOfferAnswer Answer = co_await AskOffer(Offer, false, bModding);
			if (Answer == EOfferAnswer::Menu)
			{
				co_await OptionsScreen(true);
				continue;
			}
			const bool bExpanded = Answer == EOfferAnswer::Right;
			Services.Game->SetStoryMode(bExpanded ? EStoryMode::Expanded : EStoryMode::Original);
			Services.Game->SetExpansion(bExpanded ? Offered->Folder : std::string());
			co_return;
		}
	}

	TMenuTask<FMenuMain::EOfferAnswer> FMenuMain::AskOffer(const OptionsText::FOffer& Offer, bool bCancels, bool bMenu)
	{
		OpenLayer();
		while (LayerState != EMenuLayer::Still)
		{
			co_await Frame();
		}
		InitTargetPointers(0);
		OpenWindow(MessageWindow, OfferX, OfferY, OfferWidth, OfferHeight, true, true, PiecesEntry, false);
		while (!Growths[MessageWindow].bGrown)
		{
			co_await Frame();
		}
		// The pointer starts on the right answer, as the messages' does.
		bool bRight = true;
		bool bCancelled = false;
		bool bMenuPressed = false;
		DrawOffer(Offer, bRight);
		bOfferShown = true;
		while (true)
		{
			co_await Frame();
			const bool bWas = bRight;
			if (Code == EMenuCode::Left)
			{
				bRight = false;
			}
			else if (Code == EMenuCode::Right)
			{
				bRight = true;
			}
			else if (Code == EMenuCode::Confirm)
			{
				break;
			}
			else if (Code == EMenuCode::Cancel && bCancels)
			{
				bCancelled = true;
				break;
			}
			else if (Code == EMenuCode::Triangle && bMenu)
			{
				bMenuPressed = true;
				break;
			}
			if (bRight != bWas)
			{
				DrawOffer(Offer, bRight);
			}
		}
		bOfferShown = false;
		TargetPointerShown[LeftPointer] = false;
		TargetPointerShown[RightPointer] = false;
		Flags[MenuFlag::TargetPointers] = 0;
		CloseWindow(MessageWindow);
		co_await Frame();
		co_return bCancelled ? EOfferAnswer::Cancelled : bMenuPressed ? EOfferAnswer::Menu : bRight ? EOfferAnswer::Right : EOfferAnswer::Left;
	}

	void FMenuMain::DrawOffer(const OptionsText::FOffer& Offer, bool bRight)
	{
		const std::array<std::string_view, 3>& Help = bRight ? Offer.RightHelp : Offer.LeftHelp;
		std::array<FMenuLabel, 6>& Lines = OfferLines;
		TypeStripPair(0, Offer.Question, Offer.Left, Lines[OfferQuestion], Lines[OfferLeft]);
		TypeStripPair(1, Offer.Right, Help[0], Lines[OfferRight], Lines[OfferHelp]);
		TypeStripPair(2, Help[1], Help[2], Lines[OfferHelp + 1], Lines[OfferHelp + 2]);
		const auto Place = [this, &Lines](std::size_t Line, int32 X, int32 Y)
		{
			SetLayerQuad(Lines[Line].Quad.Points, X, Y, Lines[Line].Width, OfferLineRows);
			RecordLabel(Lines[Line]);
		};
		Place(OfferQuestion, OfferTextX, OfferQuestionY);
		Place(OfferLeft, OfferAnswerX[0], OfferAnswersY);
		Place(OfferRight, OfferAnswerX[1], OfferAnswersY);
		for (std::size_t Line = 0; Line < Help.size(); ++Line)
		{
			Place(OfferHelp + Line, OfferTextX, OfferHelpY + static_cast<int32>(Line) * OfferHelpStep);
		}
		PlaceTargetPointer(LeftPointer, OfferAnswerX[0] + Lines[OfferLeft].Width + OfferPointerGap, OfferAnswersY + OfferPointerDrop);
		PlaceTargetPointer(RightPointer, OfferAnswerX[1] + Lines[OfferRight].Width + OfferPointerGap, OfferAnswersY + OfferPointerDrop);
		TargetPointerShown[LeftPointer] = !bRight;
		TargetPointerShown[RightPointer] = bRight;
	}

	void FMenuMain::DrawDiscChange()
	{
		GrowWindows();
		AddTargetPointers();
		AddLabels();
		AddSelection();
		DrawFileDetails();
		DrawCards();
		AddWindows();
		if (Flags[MenuFlag::SubCommands] != 0)
		{
			AddArc(SubArc, false);
		}
	}
}
