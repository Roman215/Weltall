#include "Menu/MenuGearShop.h"

#include "Field/FieldLight.h"
#include "Game/GameState.h"
#include "Game/KernelState.h"
#include "Menu/MenuGearView.h"
#include "Game/GameStateLayout.h"
#include "Graphics/GteVectors.h"

namespace Weltall
{
	namespace
	{
		constexpr int32 CharacterCount = 16;
		/** The characters a gear shop offers: Chu-Chu, who has no gear to fit, left out. */
		constexpr uint16 PilotMask = 0x77F;
		/**
		 * The character whose gear is shown when nobody holds one: the original's search for the first pilot has no end
		 * and runs on through the menu's bytes after the sixteen, all zero as the shop opens but the 26th, its input
		 * code, which the menu program starts at 8. That character's gear byte lies in the gears' records, a byte
		 * nothing writes: Weltall.
		 */
		constexpr int32 NoPilotShown = 25;
		/** L1, R1, Start and Select held together, and nothing else: the console starts again. */
		constexpr uint16 SoftResetButtons = 0x090C;
		constexpr int32 ClosingFrames = 2;
		constexpr uint16 GearDialogWidth = 0xC0;
		constexpr int16 TextClutX = 0;
		constexpr int16 TextClutY = 0x1D1;
		constexpr int16 WhiteClutY = 0x1C0;
		constexpr uint16 White = 0x7FFF;
		constexpr int32 PiecesEntry = 4;
		constexpr int32 FullScale = 0x1000;
		/** A gear shop's stock: 20 frames, 20 armors, 20 engines, 20 gear weapons, 20 parts. */
		constexpr std::size_t StockRecord = 0x64;
		/** Where each list lies in a record, by EGearGoods. */
		constexpr std::array<std::size_t, 5> ListPlaces = {0x14, 0x00, 0x28, 0x50, 0x3C};
		/** The shop's arc: Exit, Sell, Buy, Tune up; the cursor starts on Buy. */
		constexpr int32 ExitCommand = 0;
		constexpr int32 SellCommand = 1;
		constexpr int32 BuyCommand = 2;
		constexpr int32 TuneCommand = 3;
		constexpr int32 FuelCommand = 3;
		constexpr int32 ArcLabelsBase = 2;
		constexpr int32 ArcLabelCount = 4;
		/** The inner arc's places follow the shop's four. */
		constexpr int32 InnerArcPlace = 4;
		/** The faces' outlines: a red box around each face's 24 x 22 place. */
		constexpr int16 FaceTop = 0xA6;
		constexpr int16 FaceBottom = 0xBC;
		constexpr int16 FaceWidth = 0x18;
		constexpr FDrawColor OutlineRed{0xFF, 0, 0};
		/** The shop's title and the L1 and R1 hints. */
		constexpr int32 TitleFrame = 0x166;
		constexpr int32 TitleX = 0x108;
		constexpr int32 TitleY = 0x18;
		constexpr int32 PreviousHint = 0x164;
		constexpr int32 NextHint = 0x165;
		constexpr int32 HintY = 100;
		constexpr std::size_t HintParts = 2;
		/** The gear's name: typed at (384, 72), shown at (16, 32). */
		constexpr int16 NameStripX = 0x180;
		constexpr int16 NameStripY = 0x48;
		constexpr int16 NameX = 0x10;
		constexpr int16 NameY = 0x20;
		constexpr int16 NameWidth = 0x60;
		constexpr int16 NameRows = 0xD;
		constexpr uint8 Grey = 0x80;

		namespace Effect
		{
			constexpr uint8 Cursor = 1;
			constexpr uint8 Confirm = 2;
			constexpr uint8 Cancel = 3;
		}

		/** The gear shop's input: the directions repeat with their sound, Cancel sounds, Confirm is silent; L1 and R1 repeat. */
		constexpr std::array<FMenuInputTest, 10> GearShopInput = {{
			{MenuButton::Right, true, EMenuCode::Right, Effect::Cursor},
			{MenuButton::Down, true, EMenuCode::Down, Effect::Cursor},
			{MenuButton::Left, true, EMenuCode::Left, Effect::Cursor},
			{MenuButton::Up, true, EMenuCode::Up, Effect::Cursor},
			{MenuButton::Circle, false, EMenuCode::Confirm, 0},
			{MenuButton::Cross, false, EMenuCode::Cancel, Effect::Cancel},
			{MenuButton::Square, false, EMenuCode::Square, 0},
			{MenuButton::Triangle, false, EMenuCode::Triangle, 0},
			{MenuButton::L1, true, EMenuCode::L1, 0},
			{MenuButton::R1, true, EMenuCode::R1, 0},
		}};

		/** A red line through three points, (x, y) each. */
		void SetLine(FMenuPrimitive& Line, const std::array<std::array<int32, 2>, 3>& Points)
		{
			Line.Code = MenuGpu::FlatPolyline;
			Line.LinePoints = 3;
			Line.Corners[0].Color = OutlineRed;
			for (std::size_t Point = 0; Point < Points.size(); ++Point)
			{
				Line.Corners[Point].X = static_cast<int16>(Points[Point][0]);
				Line.Corners[Point].Y = static_cast<int16>(Points[Point][1]);
			}
		}
	}

	FMenuGearShop::FMenuGearShop(const FMenuServices& InServices, const FMenuRequest& InRequest)
		: FMenuShopModule(InServices, InRequest, GearDialogWidth), Tables(Resources.GearShop())
	{
		ArcX = Tables.ArcX;
		ArcY = Tables.ArcY;
		SubArcRows = Tables.SubCommands;
		SubArcFirstPlace = InnerArcPlace;
		ChoiceX = Tables.ChoiceX;
		ChoiceY = Tables.ChoiceY;
		Pilots = Tables.Pilots;
		// The shop has no layer that opens: its pieces are drawn at the screen's own scale.
		Layer = MenuFlatTransform;
		LayerGte = FlatGte;
	}

	TMenuTask<> FMenuGearShop::Run()
	{
		if (!Init())
		{
			co_return;
		}
		bDrawScene = true;
		bSoundOn = true;
		co_await RunTopMenu();
		co_await Shutdown();
	}

	uint8 FMenuGearShop::GearOf(int32 Character) const
	{
		return Services.Game->Block()[GameStateLayout::CharacterOffset(static_cast<std::size_t>(Character)) + GameStateLayout::Character::Gear];
	}

	bool FMenuGearShop::Init()
	{
		const FGameState& Game = *Services.Game;
		Command = 4;
		DrawnCommand = 0xFF;
		const uint16 Offered = static_cast<uint16>(Game.JoinedCharacters() & Game.MenuCharacters() & PilotMask);
		for (int32 Character = 0; Character < CharacterCount; ++Character)
		{
			const bool bOffered = (Offered >> Character & 1) != 0;
			Available[static_cast<std::size_t>(Character)] = bOffered && GearOf(Character) != FGameState::EmptySlot;
		}
		const std::array<uint8, 3> InParty = Game.Party();
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			const uint8 Character = InParty[static_cast<std::size_t>(Slot)];
			const bool bKept = Character != FGameState::EmptySlot && Character < CharacterCount && Available[Character];
			Flags[static_cast<std::size_t>(MenuFlag::Party + Slot)] = bKept ? Character : FGameState::EmptySlot;
		}
		UploadArchive();
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			if (Member(Slot) != FGameState::EmptySlot)
			{
				UploadPortrait(Slot, Member(Slot));
			}
		}
		Buffer = 0;
		Upload({{TextClutX, TextClutY, 32, 1}, {Resources.TextClut().begin(), Resources.TextClut().end()}});
		RenderLabels(HelpLabels, Tables.HelpLabels, 0);
		FVramImage WhiteClut{{0, WhiteClutY, 16, 1}, std::vector<uint16>(16, 0)};
		WhiteClut.Units[1] = White;
		Upload(WhiteClut);
		HideSelection();
		SelectionMode = FMenuPrimitive::MakeDrawMode(MenuGpu::TexturePage(0, 0, 0x140, 0x80));
		LoadLists();

		// The model's view: a fresh module, the first pilot's gear's files read, the pilots counted.
		FGearShopModelOptions Options;
		Options.bFieldPace = Services.Options.bGearModelFieldPace;
		if (const std::optional<FError> Error = Model.Open(Options))
		{
			Fault = *Error;
			return false;
		}
		Model.Models().Random = Services.Random;
		Model.Models().Arctangents = &Resources.Arctangents();
		int32 First = 0;
		while (First < CharacterCount && !Available[static_cast<std::size_t>(First)])
		{
			++First;
		}
		CurrentGear = GearOf(First == CharacterCount ? NoPilotShown : First);
		if (const std::optional<FError> Error = ReadGearFiles(CurrentGear))
		{
			Fault = *Error;
			return false;
		}
		for (int32 Character = 0; Character < FGameState::CharacterCount; ++Character)
		{
			PilotCount += Available[static_cast<std::size_t>(Character)] ? 1 : 0;
		}

		MemberHints.clear();
		Resources.Sheet().Draw(PreviousHint, Tables.HintX[0], HintY, FullScale, MemberHints);
		MemberHints.resize(HintParts);
		Resources.Sheet().Draw(NextHint, Tables.HintX[1], HintY, FullScale, MemberHints);
		MemberHints.resize(2 * HintParts);
		SetLayerSprites(MemberHints);
		return true;
	}

	void FMenuGearShop::LoadLists()
	{
		// The shop's lists in its record's order (the shop number is not checked against the table's length).
		const std::span<const uint8> Shops = Resources.GearShopStock();
		const std::size_t First = static_cast<std::size_t>(Request.Param) * StockRecord;
		for (std::size_t Kind = 0; Kind < Lists.size(); ++Kind)
		{
			ListCounts[Kind] = 0;
			for (std::size_t Entry = 0; Entry < Lists[Kind].size(); ++Entry)
			{
				const std::size_t At = First + ListPlaces[Kind] + Entry;
				Lists[Kind][Entry] = At < Shops.size() ? Shops[At] : uint8{0};
				ListCounts[Kind] += Lists[Kind][Entry] != 0 ? 1 : 0;
			}
		}
		for (std::size_t Face = 0; Face < Scene.OutlineTops.size(); ++Face)
		{
			// The outline's places as 16 bits.
			const int32 X = static_cast<int16>(Tables.FaceX[Face]);
			SetLine(Scene.OutlineTops[Face], {{{X, FaceTop}, {X + FaceWidth, FaceTop}, {X + FaceWidth, FaceBottom}}});
			SetLine(Scene.OutlineBottoms[Face], {{{X, FaceTop}, {X, FaceBottom}, {X + FaceWidth, FaceBottom}}});
			Scene.Outlined[Face] = false;
		}
		Title.clear();
		Resources.Sheet().Draw(TitleFrame, TitleX, TitleY, FullScale, Title);
		NameQuad.Code = MenuGpu::TexturedQuad;
		NameQuad.Page = MenuGpu::TexturePage(0, 0, NameStripX, 0);
		NameQuad.Clut = MenuGpu::Clut(TextClutX, TextClutY);
		NameQuad.Corners[0] = {NameX, NameY, 0, static_cast<uint8>(NameStripY), {Grey, Grey, Grey}};
		NameQuad.Corners[1] = {NameX + NameWidth, NameY, static_cast<uint8>(NameWidth), static_cast<uint8>(NameStripY), {}};
		NameQuad.Corners[2] = {NameX, NameY + NameRows, 0, static_cast<uint8>(NameStripY + NameRows), {}};
		NameQuad.Corners[3] = {NameX + NameWidth, NameY + NameRows, static_cast<uint8>(NameWidth), static_cast<uint8>(NameStripY + NameRows), {}};
	}

	void FMenuGearShop::DrawFrame()
	{
		ReadInput(GearShopInput);
		if (PadHeld == SoftResetButtons)
		{
			bSoftReset = true;
			return;
		}
		Buffer ^= 1;
		StepCamera();
		if (bDrawScene)
		{
			GrowWindows();
			AnimateScanner();
			AddChoiceCursors();
			AddLabels();
			AddSelection();
			if (bListCursorShown)
			{
				AddProjected(ListCursor.Quad, true, PiecesEntry);
			}
			AddScene();
			if (bMemberHintsShown && PilotCount > 1)
			{
				AddLayered(MemberHints);
			}
			if (bScrollThumbShown)
			{
				AddProjected(ScrollThumb.Quad, true, PiecesEntry);
			}
			if (Flags[MenuFlag::Commands] != 0)
			{
				AddArc(Arc, true);
			}
			if (Flags[MenuFlag::SubCommands] != 0)
			{
				AddArc(SubArc, false);
			}
			AddWindows();
			AddStatusPanel();
			if (!Title.empty())
			{
				Table.Add(PiecesEntry, Title.front());
			}
			if (bModelShown)
			{
				if (const std::optional<FError> Error = Model.Frame())
				{
					Fault = *Error;
				}
				TakeModelVram();
				// The model's last part leaves the GTE its light, for whatever the program after the menu lights first.
				if (const std::optional<FGteMatrix> Light = FieldGearsLight(Model.Models(), GearShopLights()); Light && Services.Kernel != nullptr)
				{
					Services.Kernel->LightMatrix = *Light;
				}
			}
		}
		Drawn.Primitives.clear();
		Table.TakeInto(Drawn.Primitives);
		Drawn.Layer = Layer;
		Drawn.Gear.bShown = bModelShown;
		Drawn.Gear.Camera = RotationXyz({0, 0x400, 0});
		Drawn.Gear.Camera.T = {0, Camera.Height, Camera.Distance};
		++Drawn.Number;
	}

	void FMenuGearShop::AddLabels()
	{
		for (std::size_t Label = 0; Label < HelpLabels.size(); ++Label)
		{
			if (Flags[static_cast<std::size_t>(MenuFlag::HelpLabels) + Label] != 0)
			{
				Table.Add(PiecesEntry, HelpLabels[Label].Quad);
			}
		}
		for (std::size_t Label = 0; Label < ArcLabels.size(); ++Label)
		{
			if (Flags[static_cast<std::size_t>(MenuFlag::ArcLabels) + Label] != 0)
			{
				Table.Add(PiecesEntry, ArcLabels[Label].Quad);
			}
		}
		if (bDialogShown)
		{
			for (FMenuLabel& Line : DialogLines)
			{
				AddProjected(Line.Quad, true, PiecesEntry);
			}
		}
	}

	void FMenuGearShop::AddStatusPanel()
	{
		if (Scanner.bShown)
		{
			if (Scanner.bFlickerShown)
			{
				for (const std::vector<FMenuPrimitive>& Mark : Scanner.Flicker)
				{
					AddScreen(Mark);
				}
			}
			for (std::size_t Bracket = 0; Bracket < Scanner.Brackets.size(); ++Bracket)
			{
				if (Scanner.BracketStates[Bracket] != 0)
				{
					AddScreen(Scanner.Brackets[Bracket]);
				}
			}
			if (Scanner.PanelState != 0)
			{
				AddScreen(Scanner.Panel);
			}
		}
		if (bStatusShown)
		{
			Table.Add(PiecesEntry, NameQuad);
			AddScreen(StatusLabels);
			for (const std::vector<FMenuPrimitive>& Number : StatusNumbers)
			{
				AddScreen(Number);
			}
		}
	}

	TMenuTask<> FMenuGearShop::RunTopMenu()
	{
		if (const std::optional<FError> Error = ShowGear(CurrentGear))
		{
			co_await Fail(Error->Message);
		}
		bModelShown = true;
		Command = BuyCommand;
		co_await RevealCommands(Tables.MainCommands);
		RenderLabels(std::span<FMenuLabel>(ArcLabels).first(ArcLabelCount), Tables.MainLabels, ArcLabelsBase);
		if (PilotCount > 1)
		{
			bMemberHintsShown = true;
		}
		bool bGoesOn = true;
		while (bGoesOn)
		{
			co_await Frame();
			switch (Code)
			{
			case EMenuCode::Down:
				Command = Command == ExitCommand ? TuneCommand : Command - 1;
				break;
			case EMenuCode::Up:
				Command = Command + 1 > TuneCommand ? ExitCommand : Command + 1;
				break;
			case EMenuCode::Confirm:
				PlaySound(Effect::Confirm);
				Arc.bDimmed = true;
				HideSelection();
				ClearFlags(MenuFlag::ArcLabels, ArcLabelCount);
				bGoesOn = co_await EnterSubmenu();
				break;
			case EMenuCode::Cancel:
				bGoesOn = false;
				break;
			case EMenuCode::R1:
				co_await SwitchGear(false);
				break;
			case EMenuCode::L1:
				co_await SwitchGear(true);
				break;
			default:
				break;
			}
			if (Command != DrawnCommand)
			{
				DrawCommands(Tables.MainCommands, Command);
				PlaceArcLabel(ArcLabels, Tables.MainLabelOffsets, MenuFlag::ArcLabels, ArcLabelCount, Command, 0);
				DrawnCommand = Command;
			}
		}
		// The model's view closes once the camera rests; the hints go.
		while (Camera.Moving != 0)
		{
			co_await Frame();
		}
		bModelShown = false;
		co_await Frame();
		Model.Close();
		bMemberHintsShown = false;
		co_await Frame();
	}

	TMenuTask<bool> FMenuGearShop::EnterSubmenu()
	{
		const bool bChose = Command != ExitCommand;
		if (bChose)
		{
			co_await RunSubmenu();
		}
		// Exit renders the arc's labels again too: the original tests a register left holding a heap
		// address there, whose low byte is not zero.
		RenderLabels(std::span<FMenuLabel>(ArcLabels).first(ArcLabelCount), Tables.MainLabels, ArcLabelsBase);
		Arc.bDimmed = false;
		Arc.bDimmedApplied = true;
		Flags[MenuFlag::Pointer] = 1;
		Flags[MenuFlag::Selection] = 1;
		DrawnCommand = 0xFF;
		Flags[MenuFlag::SubCommands] = 0;
		co_return bChose;
	}

	TMenuTask<> FMenuGearShop::RunSubmenu()
	{
		const bool bTuning = Command == TuneCommand;
		if (bTuning)
		{
			HideSelection();
			ClearFlags(MenuFlag::ArcLabels, ArcLabelCount);
			ClearListScreen(false);
		}
		SubCommand = bTuning ? FuelCommand : 0;
		DrawnSubCommand = 0xFF;
		const std::span<const uint8> Labels = bTuning ? std::span<const uint8>(Tables.TuneLabels) : std::span<const uint8>(Tables.SubLabels);
		const std::span<const int32> Offsets = bTuning ? std::span<const int32>(Tables.TuneLabelOffsets) : std::span<const int32>(Tables.SubLabelOffsets);
		bool bFirst = true;
		bool bGoesOn = true;
		while (bGoesOn)
		{
			co_await Frame();
			if (Camera.Moving == 0 && PilotCount > 1)
			{
				bMemberHintsShown = true;
			}
			if (bFirst)
			{
				bFirst = false;
				RenderLabels(std::span<FMenuLabel>(ArcLabels).first(ArcLabelCount), Labels, ArcLabelsBase);
				co_await RevealSubCommands(0);
			}
			if (SubCommand != DrawnSubCommand)
			{
				PlaceArcLabel(ArcLabels, Offsets, MenuFlag::ArcLabels, ArcLabelCount, SubCommand, InnerArcPlace);
				DrawSubCommands(0);
				DrawnSubCommand = SubCommand;
			}
			if (Scanner.BracketStates[0] == 0)
			{
				bStatusShown = true;
				Scanner.bShown = false;
			}
			switch (Code)
			{
			case EMenuCode::Down:
				SubCommand = SubCommand == 0 ? SubCommandCount - 1 : SubCommand - 1;
				break;
			case EMenuCode::Up:
				SubCommand = SubCommand + 1 >= SubCommandCount ? 0 : SubCommand + 1;
				break;
			case EMenuCode::Confirm:
				co_await RunChoice();
				RenderLabels(std::span<FMenuLabel>(ArcLabels).first(ArcLabelCount), Labels, ArcLabelsBase);
				DrawnSubCommand = 0xFF;
				break;
			case EMenuCode::Cancel:
				bGoesOn = false;
				break;
			case EMenuCode::R1:
				co_await SwitchGear(false);
				break;
			case EMenuCode::L1:
				co_await SwitchGear(true);
				break;
			default:
				break;
			}
		}
		HideSelection();
		ClearFlags(MenuFlag::ArcLabels, ArcLabelCount);
	}

	TMenuTask<> FMenuGearShop::RunChoice()
	{
		HideSelection();
		Flags[MenuFlag::SubCommands] = 0;
		bStatusShown = false;
		ClearFlags(MenuFlag::ArcLabels, ArcLabelCount);
		PlaySound(Effect::Confirm);
		co_await ZoomToPart();
		bMemberHintsShown = false;
		if (Command == BuyCommand)
		{
			co_await RunBuyList(false);
		}
		else if (Command == SellCommand)
		{
			co_await RunSell();
		}
		else if (Command == TuneCommand)
		{
			if (SubCommand < FuelCommand)
			{
				co_await RunBuyList(true);
				ApplyGear(CurrentGear);
			}
			else
			{
				co_await RunRestore();
			}
		}
		ZoomOut();
		co_await Frame();
		BuildStatusPanel();
		co_await Frame();
		Flags[MenuFlag::Pointer] = 1;
		Flags[MenuFlag::Selection] = 1;
		Flags[MenuFlag::SubCommands] = 1;
	}

	TMenuTask<> FMenuGearShop::Shutdown()
	{
		for (int32 Closing = 0; Closing < ClosingFrames; ++Closing)
		{
			co_await Frame();
		}
		bDrawScene = false;
		co_await Frame();
		do
		{
			co_await Frame();
		} while (Buffer != 0);
	}
}
