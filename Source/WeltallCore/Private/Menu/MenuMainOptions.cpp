#include "Game/GameState.h"
#include "Game/PlayerSettings.h"
#include "Game/WayBackPoint.h"
#include "Menu/MenuMain.h"
#include "Menu/MenuPortFrames.h"
#include "Menu/MenuResources.h"
#include "Menu/MenuSavePages.h"
#include "Menu/OptionsText.h"
#include "Story/StoryWayBack.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The title's Sound orb (the arc's first), whose label the options screen's takes over. */
		constexpr std::size_t TitleSound = 0;
		/** The Normal Menu's Exit, at the foot of its arc, where the title's own stands. */
		constexpr std::size_t NormalExit = 0;
		/** The Normal Menu's File row where saving is allowed (Save, Copy, Delete), which the port's row after the module's copies with Load over Save. */
		constexpr std::size_t NormalSaveRow = 1;
		constexpr std::size_t TopFileCommand = 3;
		/** The Normal Menu's eighth orb: a step on from Status (the arc's place 6). */
		constexpr int32 StatusPlace = 6;
		constexpr int32 OptionsStepX = -10;
		constexpr int32 OptionsStepY = -16;
		/** The Items screen's two windows: the list's, with its scroll bar, and the help's under it. */
		constexpr int32 ListWindow = 3;
		constexpr int32 HelpWindow = 4;
		constexpr int32 PiecesEntry = 4;
		constexpr int32 FullScale = 0x1000;
		/** The rows: the title in the list's first, the page's rows under it, the cursor's place as the items' left column's. */
		constexpr int32 RowTop = 0x0E;
		constexpr int32 RowStep = 0x10;
		constexpr int32 RowHeight = 13;
		constexpr int32 LabelX = 0x28;
		constexpr int32 HeadingX = 0x1C;
		/** The values end at the list's inner edge. */
		constexpr int32 ValueRight = 0x128;
		constexpr int32 ListCentreX = 0x0C + 0x124 / 2;
		/** The L1 and R1 hints beside the title: where the L1 hint's box ends and the R1 hint's begins, from their origins. */
		constexpr int32 PreviousHint = 0x164;
		constexpr int32 NextHint = 0x165;
		constexpr int32 PreviousBoxEnd = 24;
		constexpr int32 NextBoxStart = 16;
		constexpr int32 HintGap = 6;
		constexpr int32 HintY = RowTop;
		/** Each shown row's strip of 13 rows from (384, 128), as the item list's: its label in one plane, its value in the other. */
		constexpr int16 StripX = 0x180;
		constexpr int16 StripY = 0x80;
		constexpr int16 StripWidth = 0x40;
		/** The help's three lines, typed as a prompt's are: two to a strip from (320, 78), one to a plane. */
		constexpr int16 HelpStripX = 0x140;
		constexpr int16 HelpStripY = 0x4E;
		constexpr int32 HelpX = 0x14;
		constexpr int32 HelpY = 0x93;
		constexpr int32 HelpLineStep = 0x0E;
		/** The scroll bar's knob: its place, and its length when every row shows. */
		constexpr int32 KnobX = 0x0C;
		constexpr int32 KnobY = 0x12;
		constexpr int32 KnobFull = 0x74;
		constexpr int32 KnobShortest = 0x10;
		constexpr uint8 Bright = 0x80;
		/** A row's keys: the pictures' left edges (the second's ending at the list's inner edge), the arrow before one, a free one's dimness. */
		constexpr int32 CellWidth = 12;
		constexpr std::array<int32, ControlSlots> CellX = {ValueRight - CellWidth - 44, ValueRight - CellWidth};
		constexpr int32 CursorBeforeCell = 12;
		constexpr int32 CursorY = 0x11;
		/** A face button's picture in the menu's sheet: the keys' stand-in, which the display draws as the key. */
		constexpr int32 CellFrame = 0xF9;
		constexpr uint8 FreeCell = 0x20;

		namespace Effect
		{
			constexpr uint8 Confirm = 2;
			constexpr uint8 Cancel = 3;
			constexpr uint8 Buzzer = 4;
		}

		/** A label's texture: its width of one plane from row V of the 4-bit page at (PageX, 0), bright (to mirror into its other copy). */
		void SetPlainTexture(FMenuLabel& Label, int32 PageX, int32 V, int32 Plane)
		{
			constexpr int32 OddClutX = 16;
			constexpr int32 TextClutY = 0x1D1;
			FMenuPrimitive& Quad = Label.Quad;
			Quad.Code = MenuGpu::TexturedQuad;
			Quad.Corners[0].Color = {Bright, Bright, Bright};
			Quad.Page = MenuGpu::TexturePage(0, 0, PageX, 0);
			Quad.Clut = MenuGpu::Clut(Plane == 0 ? 0 : OddClutX, TextClutY);
			Label.Plane = static_cast<uint8>(Plane);
			SetTexelRect(Quad, 0, V, Label.Width, RowHeight);
		}

		/** The row the cursor moves to from Row, Direction rows on, passing over headings; Row where there is none. */
		int32 NextRow(const std::vector<FSettingsRow>& Rows, int32 Row, int32 Direction)
		{
			for (int32 Next = Row + Direction; Next >= 0 && Next < static_cast<int32>(Rows.size()); Next += Direction)
			{
				if (!Rows[static_cast<std::size_t>(Next)].bHeading && !Rows[static_cast<std::size_t>(Next)].bReadOnly)
				{
					return Next;
				}
			}
			return Row;
		}

		int32 FirstRow(const std::vector<FSettingsRow>& Rows)
		{
			return !Rows.empty() && (Rows.front().bHeading || Rows.front().bReadOnly) ? NextRow(Rows, 0, 1) : 0;
		}
	}

	void FMenuMain::InitArcs()
	{
		const FMenuMainTables& Tables = Resources.Main();
		std::copy(Tables.ArcX.begin(), Tables.ArcX.end(), PlaceX.begin());
		std::copy(Tables.ArcY.begin(), Tables.ArcY.end(), PlaceY.begin());
		PlaceX.back() = Tables.ArcX[StatusPlace] + OptionsStepX;
		PlaceY.back() = Tables.ArcY[StatusPlace] + OptionsStepY;
		ArcX = PlaceX;
		ArcY = PlaceY;
		const bool bExit = static_cast<bool>(Services.Quit);
		TitleArc.clear();
		if (bExit)
		{
			TitleArc.push_back(Tables.MainCommands[NormalExit]);
		}
		for (std::size_t TitleCommand = 0; TitleCommand < Tables.TitleCommands.size(); ++TitleCommand)
		{
			FMenuArcCommand Shown = Tables.TitleCommands[TitleCommand];
			if (bExit)
			{
				Shown.Orb = MenuPortFrame::RaisedTitleOrbs + static_cast<int32>(TitleCommand);
				Shown.Label = MenuPortFrame::RaisedTitleLabels + static_cast<int32>(TitleCommand);
			}
			if (TitleCommand == TitleSound && Services.Settings != nullptr)
			{
				Shown.Label = bExit ? MenuPortFrame::RaisedTitleOptions : MenuPortFrame::TitleOptions;
			}
			TitleArc.push_back(Shown);
		}
		std::copy(Tables.SubCommands.begin(), Tables.SubCommands.end(), SubRows.begin());
		SubRows.back() = Tables.SubCommands[NormalSaveRow];
		SubRows.back()[TopFileCommand] = {MenuPortFrame::TopLoadOrb, MenuPortFrame::TopLoad};
		SubArcRows = SubRows;
		NormalArc.assign(Tables.MainCommands.begin(), Tables.MainCommands.end());
		if (Services.Settings != nullptr)
		{
			NormalArc.push_back({MenuPortFrame::OptionsOrb, MenuPortFrame::Options});
		}
		OptionsImage.Rect = {StripX, StripY, StripWidth, RowHeight};
		OptionsImage.Units.assign(static_cast<std::size_t>(StripWidth * RowHeight), 0);
	}

	ESettingsPage FMenuMain::SettingsPage() const
	{
		return OptionsState.Opened.value_or(OnModdingTurned() ? ESettingsPage::Modding : static_cast<ESettingsPage>(OptionsState.Turned));
	}

	bool FMenuMain::OnControlsPage() const
	{
		return OptionsState.ControlsPage && OptionsState.Turned == TurnedSettingsPages;
	}

	bool FMenuMain::OnSavesPages() const
	{
		return OptionsState.SavePages && OptionsState.Turned == TurnedSettingsPages + (OptionsState.ControlsPage ? 1 : 0);
	}

	int32 FMenuMain::TurnedPages() const
	{
		constexpr int32 ModdingPage = 1;
		return TurnedSettingsPages + (OptionsState.ControlsPage ? 1 : 0) + (OptionsState.SavePages ? 1 : 0) + ModdingPage;
	}

	void FMenuMain::FillOptionsRows()
	{
		FMenuOptionsState& State = OptionsState;
		State.Rows.clear();
		if (OnAssetsPage())
		{
			FillAssetRows();
			return;
		}
		if (OnSavesPages())
		{
			State.Title = State.SavePages->Title();
			State.Rows = State.SavePages->Rows();
			return;
		}
		if (OnControlsPage())
		{
			State.Title = std::string(FMenuControlsPage::Title());
			State.Rows = State.ControlsPage->Rows(State.Settings.Controls);
			return;
		}
		const ESettingsPage Page = SettingsPage();
		State.Title = std::string(FSettingsPages::Title(Page));
		for (int32 Row = 0; Row < FSettingsPages::RowCount(Page, State.Context); ++Row)
		{
			State.Rows.push_back(FSettingsPages::Row(Page, Row, State.Settings, State.bCustomPicked, State.Context));
		}
		if (ShowsSecondDiscRow())
		{
			State.Rows.push_back(SecondDiscRow());
		}
		if (Page == ESettingsPage::Modding)
		{
			AddModdingRows();
		}
	}

	bool FMenuMain::ShowsSecondDiscRow() const
	{
		const bool bExtended = Services.Game->StoryMode() == EStoryMode::Expanded;
		return Services.bStoryExpansion && Request.Mode == EMenuMode::Normal && SettingsPage() == ESettingsPage::Fixes
			&& (bExtended || OptionsState.bSecondDiscSwitched);
	}

	bool FMenuMain::OnSecondDiscRow() const
	{
		return ShowsSecondDiscRow() && OptionsState.Row == FSettingsPages::RowCount(ESettingsPage::Fixes, OptionsState.Context);
	}

	FSettingsRow FMenuMain::SecondDiscRow() const
	{
		namespace Text = OptionsText::SecondDisc;
		const FMenuOptionsState& State = OptionsState;
		if (State.bSecondDiscSwitched)
		{
			FSettingsRow Row{std::string(Text::Row.Label), std::string(Text::Original), OptionsText::Fill(Text::Switched)};
			Row.bReadOnly = true;
			return Row;
		}
		return {std::string(Text::Row.Label), std::string(Text::Extended), OptionsText::Fill(State.bSecondDiscAsked ? Text::SwitchAsked : Text::Row.Help)};
	}

	bool FMenuMain::ActivateSecondDiscRow()
	{
		FMenuOptionsState& State = OptionsState;
		if (State.bSecondDiscSwitched)
		{
			return false;
		}
		if (!State.bSecondDiscAsked)
		{
			State.bSecondDiscAsked = true;
			return true;
		}
		FGameState& Game = *Services.Game;
		const FWayBackPoint* Point = Services.WayBack != nullptr ? Services.WayBack->For(Game) : nullptr;
		Game.SetStoryMode(EStoryMode::Original);
		State.bSecondDiscAsked = false;
		State.bSecondDiscSwitched = true;
		if (Point != nullptr)
		{
			Point->Apply(Game);
			bGameLoaded = true;
		}
		return true;
	}

	void FMenuMain::ShowOptionsPage(bool bFromTop)
	{
		FillOptionsRows();
		if (bFromTop)
		{
			OptionsState.Row = FirstRow(OptionsState.Rows);
			OptionsState.Top = 0;
		}
		OptionsState.Row = std::min(OptionsState.Row, std::max(static_cast<int32>(OptionsState.Rows.size()) - 1, 0));
	}

	TMenuTask<bool> FMenuMain::OptionsScreen()
	{
		OptionsState = {};
		OptionsState.Settings = Services.Settings->Settings();
		OptionsState.Context = {Services.Settings->DisplayModes(), Services.Settings->SoundSources(), Services.Settings->DeviceFiles() != nullptr};
		if (Services.Saves != nullptr)
		{
			OptionsState.SavePages = std::make_unique<FMenuSavePages>(*Services.Saves, Resources, Services.Settings->DeviceFiles());
		}
		if (Services.Keys != nullptr)
		{
			OptionsState.ControlsPage = std::make_unique<FMenuControlsPage>(*Services.Keys);
		}
		if (IMenuDeviceFiles* DeviceFiles = Services.Settings->DeviceFiles())
		{
			OptionsState.ModFiles = std::make_unique<FMenuModFiles>(*DeviceFiles);
		}
		FMenuOptionsState& State = OptionsState;
		ShowOptionsPage(true);
		bool bChanged = true;
		bool bFirst = true;
		AllocCursor(0);
		Options = {};
		while (true)
		{
			co_await Frame();
			if (OnAssetsPage() && RefreshAssetProgress())
			{
				bChanged = true;
			}
			if (OnSavesPages())
			{
				// A transfer through the device's picker moved on; a file to import that came in opens its page.
				const uint32 WasView = State.SavePages->View();
				if (State.SavePages->Refresh())
				{
					ShowOptionsPage(State.SavePages->View() != WasView);
					bChanged = true;
				}
			}
			if (RefreshModFiles())
			{
				bChanged = true;
			}
			if (const FSoundSources Sources = Services.Settings->SoundSources(); Sources != State.Context.Sound)
			{
				// Discs added while the screen is open give the Sound page their voices and subtitles.
				State.Context.Sound = Sources;
				ShowOptionsPage(false);
				bChanged = true;
			}
			if (bChanged)
			{
				DrawOptionsPage();
				DrawOptionsHelp();
				bChanged = false;
			}
			if (State.Row < static_cast<int32>(State.Rows.size()) && !State.Rows[static_cast<std::size_t>(State.Row)].Cells.empty())
			{
				// On a row of keys the arrow points at the key the page is on.
				const std::size_t Cell = static_cast<std::size_t>(State.ControlsPage->Cell());
				DrawCursorAt(CellX[Cell] - CursorBeforeCell, (State.Row - State.Top + 1) * RowStep + CursorY, 0);
			}
			else
			{
				DrawCursor((State.Row - State.Top + 1) * 2, 0, 0, 0);
			}
			if (bFirst)
			{
				OpenWindow(ListWindow, 0x0C, 10, 0x124, 0x84, false, true, PiecesEntry, true);
				OpenWindow(HelpWindow, 8, 0x8F, 0x130, 0x30, false, true, PiecesEntry, false);
				bFirst = false;
				OpenLayer();
				if (Request.Mode == EMenuMode::Normal)
				{
					co_await SlideParty(false, false);
				}
			}
			const int32 Was = State.Row;
			const bool bListening = OnControlsPage() && State.ControlsPage->Listening();
			if (bListening)
			{
				bChanged = HearKeys() || bChanged;
				if (State.ControlsPage->Waited())
				{
					PlaySound(Effect::Cancel);
					FillOptionsRows();
					bChanged = true;
				}
			}
			else if (OnControlsPage() && State.ControlsPage->Follow())
			{
				// The player touched the other device: its keys are shown.
				FillOptionsRows();
				bChanged = true;
			}
			else if (State.bAwaitRelease)
			{
				State.bAwaitRelease = PadHeld != 0;
			}
			switch (bListening || State.bAwaitRelease ? EMenuCode::None : Code)
			{
			case EMenuCode::Up:
				State.Row = NextRow(State.Rows, State.Row, -1);
				break;
			case EMenuCode::Down:
				State.Row = NextRow(State.Rows, State.Row, 1);
				break;
			case EMenuCode::Left:
			case EMenuCode::Right:
				bChanged = StepOption(Code == EMenuCode::Left ? -1 : 1);
				break;
			case EMenuCode::L1:
			case EMenuCode::R1:
				TurnOptionsPage(Code == EMenuCode::L1 ? -1 : 1);
				bChanged = true;
				break;
			case EMenuCode::Confirm:
				if (!ActivateOption())
				{
					PlaySound(Effect::Buzzer);
				}
				bChanged = true;
				if (bGameLoaded)
				{
					// Switched where the original stands elsewhere: the game goes on there, the menu closing.
					Services.Settings->Keep();
					CloseOptions();
					co_return true;
				}
				break;
			case EMenuCode::Square:
			case EMenuCode::Triangle:
				if (OnControlsPage())
				{
					if (!KeyOption(Code == EMenuCode::Triangle))
					{
						PlaySound(Effect::Buzzer);
					}
					bChanged = true;
				}
				break;
			case EMenuCode::Cancel:
				if (BackOption())
				{
					bChanged = true;
					break;
				}
				Services.Settings->Keep();
				CloseOptions();
				co_return true;
			default:
				break;
			}
			if (State.Row != Was)
			{
				if (OnSavesPages())
				{
					State.SavePages->CursorMoved();
					FillOptionsRows();
				}
				else if (OnControlsPage())
				{
					State.ControlsPage->CursorMoved();
					FillOptionsRows();
				}
				else if (State.ModFiles && OnModdingTurned() && !State.Opened && !State.bAssetsOpened)
				{
					State.ModFiles->CursorMoved();
					FillOptionsRows();
				}
				else if (State.bSecondDiscAsked)
				{
					State.bSecondDiscAsked = false;
					FillOptionsRows();
				}
				bChanged = true;
			}
			// The rows in view follow the cursor, with a heading right over it.
			const int32 Count = static_cast<int32>(State.Rows.size());
			const int32 LastTop = std::max(Count - FMenuOptionsScreen::Rows, 0);
			const int32 Shown = State.Row > 0 && State.Rows[static_cast<std::size_t>(State.Row - 1)].bHeading ? State.Row - 1 : State.Row;
			State.Top = std::clamp(std::clamp(State.Top, State.Row - FMenuOptionsScreen::Rows + 1, Shown), 0, LastTop);
		}
	}

	bool FMenuMain::StepOption(int32 Direction)
	{
		FMenuOptionsState& State = OptionsState;
		if (OnAssetsPage())
		{
			return StepAssetOption();
		}
		if (OnSavesPages())
		{
			if (!State.SavePages->Step(State.Row, Direction))
			{
				return false;
			}
			FillOptionsRows();
			return true;
		}
		if (OnControlsPage())
		{
			const FControlBindings Before = State.Settings.Controls;
			if (!State.ControlsPage->Step(State.Row, Direction, State.Settings.Controls))
			{
				return false;
			}
			ControlsChanged(Before);
			FillOptionsRows();
			return true;
		}
		if (SettingsPage() == ESettingsPage::Modding && State.Row >= FSettingsPages::RowCount(ESettingsPage::Modding))
		{
			return StepModdingRow();
		}
		if (OnSecondDiscRow() || !FSettingsPages::Step(SettingsPage(), State.Row, Direction, State.Settings, State.bCustomPicked, State.Context))
		{
			return false;
		}
		Services.Settings->Change(State.Settings);
		FillOptionsRows();
		return true;
	}

	void FMenuMain::TurnOptionsPage(int32 Direction)
	{
		FMenuOptionsState& State = OptionsState;
		if (State.Opened || State.bAssetsOpened || (OnSavesPages() && State.SavePages->Page() != ESavesPage::Saves))
		{
			// A list opened from a page (the fixes', the CRT's, the Assets page, a folder's, a file's saves) turns a page of rows at a time.
			const int32 LastTop = std::max(static_cast<int32>(State.Rows.size()) - FMenuOptionsScreen::Rows, 0);
			State.Top = std::clamp(State.Top + Direction * FMenuOptionsScreen::Rows, 0, LastTop);
			State.Row = State.Rows[static_cast<std::size_t>(State.Top)].bHeading ? NextRow(State.Rows, State.Top, 1) : State.Top;
			return;
		}
		const int32 Pages = TurnedPages();
		State.Turned = (State.Turned + Direction + Pages) % Pages;
		State.bCustomPicked = false;
		if (State.ModFiles)
		{
			State.ModFiles->CursorMoved();
		}
		State.bSecondDiscAsked = false;
		ShowOptionsPage(true);
	}

	bool FMenuMain::ActivateOption()
	{
		FMenuOptionsState& State = OptionsState;
		if (OnAssetsPage())
		{
			return ActivateAssetOption();
		}
		if (OnSavesPages())
		{
			const uint32 Was = State.SavePages->View();
			if (!State.SavePages->Activate(State.Row))
			{
				return false;
			}
			ShowOptionsPage(State.SavePages->View() != Was);
			return true;
		}
		if (OnControlsPage())
		{
			const FControlBindings Before = State.Settings.Controls;
			const bool bDone = State.ControlsPage->Activate(State.Row, State.Settings.Controls);
			ControlsChanged(Before);
			FillOptionsRows();
			return bDone;
		}
		if (OnSecondDiscRow())
		{
			const bool bDone = ActivateSecondDiscRow();
			FillOptionsRows();
			return bDone;
		}
		if (SettingsPage() == ESettingsPage::Modding && State.Row >= FSettingsPages::RowCount(ESettingsPage::Modding))
		{
			return ActivateModdingRow();
		}
		const ESettingsAction Action = FSettingsPages::Activate(SettingsPage(), State.Row, State.Settings, State.bCustomPicked, State.Context);
		switch (Action)
		{
		case ESettingsAction::OpenCustom:
		case ESettingsAction::OpenCrt:
			State.Opened = Action == ESettingsAction::OpenCrt ? ESettingsPage::Crt : ESettingsPage::Custom;
			State.OpenedFrom = State.Row;
			ShowOptionsPage(true);
			return true;
		case ESettingsAction::Reset:
			Services.Settings->Change(State.Settings);
			FillOptionsRows();
			return true;
		case ESettingsAction::None:
			break;
		}
		return false;
	}

	bool FMenuMain::BackOption()
	{
		FMenuOptionsState& State = OptionsState;
		if (OnSavesPages())
		{
			if (!State.SavePages->Back())
			{
				return false;
			}
			ShowOptionsPage(true);
			return true;
		}
		if (State.bAssetsOpened)
		{
			State.bAssetsOpened = false;
			ShowOptionsPage(true);
			State.Row = State.OpenedFrom;
			return true;
		}
		if (!State.Opened)
		{
			return false;
		}
		// Back on the row the list was opened from (the fixes' preset is read from the fixes again).
		State.Opened.reset();
		State.bCustomPicked = false;
		ShowOptionsPage(true);
		State.Row = State.OpenedFrom;
		return true;
	}

	bool FMenuMain::KeyOption(bool bSwap)
	{
		FMenuOptionsState& State = OptionsState;
		const FControlBindings Before = State.Settings.Controls;
		const bool bDone = bSwap ? State.ControlsPage->Swap(State.Row, State.Settings.Controls) : State.ControlsPage->Clear(State.Row, State.Settings.Controls);
		ControlsChanged(Before);
		FillOptionsRows();
		return bDone;
	}

	bool FMenuMain::HearKeys()
	{
		FMenuOptionsState& State = OptionsState;
		while (std::optional<FPressedKey> Pressed = Services.Keys->TakePressed())
		{
			const FControlBindings Before = State.Settings.Controls;
			const EControlsHeard Heard = State.ControlsPage->Heard(*Pressed, State.Settings.Controls);
			if (Heard == EControlsHeard::Ignored)
			{
				continue;
			}
			PlaySound(Heard == EControlsHeard::Bound ? Effect::Confirm : Heard == EControlsHeard::Stopped ? Effect::Cancel : Effect::Buzzer);
			ControlsChanged(Before);
			State.bAwaitRelease = true;
			FillOptionsRows();
			return true;
		}
		return false;
	}

	void FMenuMain::ControlsChanged(const FControlBindings& Before)
	{
		if (OptionsState.Settings.Controls != Before)
		{
			Services.Settings->Change(OptionsState.Settings);
		}
	}

	void FMenuMain::DrawOptionsCells(const FSettingsRow& Shown, int32 Y)
	{
		for (std::size_t Cell = 0; Cell < Shown.Cells.size() && Cell < CellX.size(); ++Cell)
		{
			const std::size_t First = Options.Cells.size();
			Resources.Sheet().Draw(CellFrame, CellX[Cell], Y, FullScale, Options.Cells);
			for (std::size_t Part = First; Part < Options.Cells.size(); ++Part)
			{
				FMenuPrimitive& Picture = Options.Cells[Part];
				if (Picture.ButtonRole != 0)
				{
					Picture.ButtonRole = static_cast<uint16>(MenuKeyRole + Options.Keys.size());
					Picture.ButtonSlot = EMenuIconSlot::FaceButton;
				}
				if (Shown.Cells[Cell].Icon.empty())
				{
					Picture.Code = static_cast<uint8>((Picture.Code | MenuGpu::SemiTransparent) & ~MenuGpu::RawTexture);
					Picture.Page = static_cast<uint16>(Picture.Page | MenuGpu::PageBlendAdd);
					Picture.Corners[0].Color = {FreeCell, FreeCell, FreeCell};
				}
			}
			Options.Keys.push_back(Shown.Cells[Cell]);
		}
	}

	void FMenuMain::TypeStripPair(int32 Strip, std::string_view FirstText, std::string_view SecondText, FMenuLabel& First, FMenuLabel& Second)
	{
		const int32 V = StripY + Strip * RowHeight;
		std::fill(OptionsImage.Units.begin(), OptionsImage.Units.end(), uint16{0});
		First.Width = static_cast<uint8>(RenderPlain(OptionsTyping, OptionsImage, FirstText, 0));
		Second.Width = static_cast<uint8>(RenderPlain(OptionsTyping, OptionsImage, SecondText, 1));
		OptionsImage.Rect.Y = static_cast<int16>(V);
		Upload(OptionsImage);
		SetPlainTexture(First, StripX, V, 0);
		MirrorLabelTexture(First);
		SetPlainTexture(Second, StripX, V, 1);
		MirrorLabelTexture(Second);
	}

	void FMenuMain::DrawOptionsPage()
	{
		const FMenuOptionsState& State = OptionsState;
		// The title, typed into the first strip with the rows' labels after it.
		std::fill(OptionsImage.Units.begin(), OptionsImage.Units.end(), uint16{0});
		Options.Title.Width = static_cast<uint8>(RenderPlain(OptionsTyping, OptionsImage, State.Title, 0));
		OptionsImage.Rect.Y = StripY;
		Upload(OptionsImage);
		SetPlainTexture(Options.Title, StripX, StripY, 0);
		MirrorLabelTexture(Options.Title);
		const int32 TitleX = ListCentreX - Options.Title.Width / 2;
		SetLayerQuad(Options.Title.Quad.Points, TitleX, RowTop, Options.Title.Width, RowHeight);
		RecordLabel(Options.Title);

		// L1 and R1 turn the pages, or a list opened from one by a page of rows.
		Options.Hints.clear();
		Resources.Sheet().Draw(PreviousHint, TitleX - HintGap - PreviousBoxEnd, HintY, FullScale, Options.Hints);
		Resources.Sheet().Draw(NextHint, TitleX + Options.Title.Width + HintGap - NextBoxStart, HintY, FullScale, Options.Hints);
		SetLayerSprites(Options.Hints);

		Options.Cells.clear();
		Options.Keys.clear();
		const int32 Count = static_cast<int32>(State.Rows.size());
		for (int32 Slot = 0; Slot < FMenuOptionsScreen::Rows; ++Slot)
		{
			const std::size_t At = static_cast<std::size_t>(Slot);
			const int32 Row = State.Top + Slot;
			Options.RowShown[At] = Row < Count;
			if (Row >= Count)
			{
				continue;
			}
			const FSettingsRow& Shown = State.Rows[static_cast<std::size_t>(Row)];
			FMenuLabel& Label = Options.Labels[At];
			FMenuLabel& Value = Options.Values[At];
			TypeStripPair(Slot + 1, Shown.Label, Shown.Value, Label, Value);
			const int32 Y = (Slot + 1) * RowStep + RowTop;
			SetLayerQuad(Label.Quad.Points, Shown.bHeading ? HeadingX : LabelX, Y, Label.Width, RowHeight);
			SetLayerQuad(Value.Quad.Points, ValueRight - Value.Width, Y, Value.Width, RowHeight);
			RecordLabel(Label);
			RecordLabel(Value);
			DrawOptionsCells(Shown, Y);
		}
		SetLayerSprites(Options.Cells);

		// The knob: its length the share of the rows shown, along the track by the share scrolled.
		const int32 LastTop = std::max(Count - FMenuOptionsScreen::Rows, 0);
		const int32 Height = LastTop == 0 ? KnobFull : std::max(KnobFull * FMenuOptionsScreen::Rows / Count, KnobShortest);
		DrawScrollKnob(KnobX, KnobY + (LastTop == 0 ? 0 : (KnobFull - Height) * State.Top / LastTop), Height);
		Options.bShown = true;
	}

	void FMenuMain::DrawOptionsHelp()
	{
		const FMenuOptionsState& State = OptionsState;
		static const FSettingsRow NoRow;
		const FSettingsRow& Shown = State.Row < static_cast<int32>(State.Rows.size()) ? State.Rows[static_cast<std::size_t>(State.Row)] : NoRow;
		FVramImage Strip{{HelpStripX, HelpStripY, StripWidth, RowHeight}, std::vector<uint16>(static_cast<std::size_t>(StripWidth * RowHeight), 0)};
		for (std::size_t Line = 0; Line < Options.Help.size(); ++Line)
		{
			FMenuLabel& Help = Options.Help[Line];
			const int32 Index = static_cast<int32>(Line);
			const int32 Plane = Index % 2;
			if (Plane == 0 && Index > 0)
			{
				Upload(Strip);
				std::fill(Strip.Units.begin(), Strip.Units.end(), uint16{0});
				Strip.Rect.Y = static_cast<int16>(Strip.Rect.Y + RowHeight);
			}
			Help.Width = static_cast<uint8>(RenderPlain(OptionsTyping, Strip, Shown.Help[Line], Plane));
			SetPlainTexture(Help, HelpStripX, Strip.Rect.Y, Plane);
			MirrorLabelTexture(Help);
			SetLayerQuad(Help.Quad.Points, HelpX, HelpY + Index * HelpLineStep, Help.Width, RowHeight);
			RecordLabel(Help);
		}
		Upload(Strip);
	}

	void FMenuMain::CloseOptions()
	{
		Options.bShown = false;
		OptionsState = {};
		Flags[MenuFlag::ScrollKnob] = 0;
		Flags[MenuFlag::Cursors] = 0;
		CloseWindow(ListWindow);
		CloseWindow(HelpWindow);
	}

	void FMenuMain::AddOptionsScreen()
	{
		if (!Options.bShown)
		{
			return;
		}
		AddProjected(Options.Title.Quad, true, PiecesEntry);
		AddLayered(Options.Hints);
		AddLayered(Options.Cells);
		for (std::size_t Slot = 0; Slot < Options.RowShown.size(); ++Slot)
		{
			if (Options.RowShown[Slot])
			{
				AddProjected(Options.Labels[Slot].Quad, true, PiecesEntry);
				AddProjected(Options.Values[Slot].Quad, true, PiecesEntry);
			}
		}
		for (FMenuLabel& Help : Options.Help)
		{
			AddProjected(Help.Quad, true, PiecesEntry);
		}
	}
}
