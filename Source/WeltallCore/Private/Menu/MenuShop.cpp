#include "Menu/MenuShop.h"

#include "Game/GameState.h"

namespace Weltall
{
	namespace
	{
		constexpr int32 CharacterCount = 16;
		constexpr uint16 CharacterMask = 0x7FF;
		/** L1, R1, Start and Select held together, and nothing else: the console starts again. */
		constexpr uint16 SoftResetButtons = 0x090C;
		constexpr int32 ClosingFrames = 2;
		constexpr int16 TextClutX = 0;
		constexpr int16 TextClutY = 0x1D1;
		constexpr int16 WhiteClutY = 0x1C0;
		constexpr uint16 White = 0x7FFF;
		constexpr int32 PiecesEntry = 4;
		/** A shop's stock: 30 weapons, 30 accessories, 30 items, 0x5C bytes a shop; at most 48 goods are kept. */
		constexpr std::size_t StockRecord = 0x5C;
		constexpr int32 StockEntries = 90;
		constexpr int32 KindEntries = 30;
		constexpr uint16 ShopDialogWidth = 0xBC;
		/** The shop's arc: Exit, Sell, Buy; the cursor starts on Buy. */
		constexpr int32 ExitCommand = 0;
		constexpr int32 SellCommand = 1;
		constexpr int32 BuyCommand = 2;
		constexpr int32 ArcLabelsBase = 2;
		/** The inner arc's places follow the shop's three. */
		constexpr int32 InnerArcPlace = 3;
		/** How far the fix raises the shop's own three places: to the inner arc's heights, the pointer clear of the words. */
		constexpr int32 ArcRise = 4;
		/** The faces' outlines: a red box around each face's 24 x 22 place. */
		constexpr int16 FaceTop = 0xA6;
		constexpr int16 FaceBottom = 0xBC;
		constexpr int16 FaceWidth = 0x18;
		constexpr FDrawColor OutlineRed{0xFF, 0, 0};
		constexpr FDrawColor LineWhite{0xFF, 0xFF, 0xFF};

		namespace Effect
		{
			constexpr uint8 Cursor = 1;
			constexpr uint8 Confirm = 2;
			constexpr uint8 Cancel = 3;
		}

		/** The shop's input: the directions repeat with their sound, Cancel sounds, Confirm is silent; the shoulder buttons are new presses. */
		constexpr std::array<FMenuInputTest, 13> ShopInput = {{
			{MenuButton::Right, true, EMenuCode::Right, Effect::Cursor},
			{MenuButton::Down, true, EMenuCode::Down, Effect::Cursor},
			{MenuButton::Left, true, EMenuCode::Left, Effect::Cursor},
			{MenuButton::Up, true, EMenuCode::Up, Effect::Cursor},
			{MenuButton::Circle, false, EMenuCode::Confirm, 0},
			{MenuButton::Cross, false, EMenuCode::Cancel, Effect::Cancel},
			{MenuButton::Square, false, EMenuCode::Square, 0},
			{MenuButton::Triangle, false, EMenuCode::Triangle, 0},
			{MenuButton::L1, false, EMenuCode::L1, 0},
			{MenuButton::R1, false, EMenuCode::R1, 0},
			{MenuButton::Start, false, EMenuCode::Start, 0},
			{MenuButton::Select, false, EMenuCode::Select, 0},
			{MenuButton::L2, false, EMenuCode::None, 0},
		}};

		void SetLine(FMenuPrimitive& Line, uint8 Code, uint8 Points, FDrawColor Color)
		{
			Line.Code = Code;
			Line.LinePoints = Points;
			Line.Corners[0].Color = Color;
		}

		void SetPoint(FMenuPrimitive& Line, std::size_t Point, int32 X, int32 Y)
		{
			Line.Corners[Point].X = static_cast<int16>(X);
			Line.Corners[Point].Y = static_cast<int16>(Y);
		}
	}

	FMenuShop::FMenuShop(const FMenuServices& InServices, const FMenuRequest& InRequest) : FMenuShopModule(InServices, InRequest, ShopDialogWidth)
	{
		const FMenuShopTables& Tables = Resources.Shop();
		MainCommands = Tables.MainCommands;
		SubRows = Tables.SubCommands;
		ArcX = Tables.ArcX;
		PlacesY = Tables.ArcY;
		if (InServices.Options.bShopPointerBesideWords)
		{
			for (int32 Place = 0; Place < InnerArcPlace; ++Place)
			{
				PlacesY[static_cast<std::size_t>(Place)] -= ArcRise;
			}
		}
		ArcY = PlacesY;
		SubArcRows = SubRows;
		SubArcFirstPlace = InnerArcPlace;
		ChoiceX = Tables.ChoiceX;
		ChoiceY = Tables.ChoiceY;
	}

	TMenuTask<> FMenuShop::Run()
	{
		Init();
		Buffer = 0;
		Upload({{TextClutX, TextClutY, 32, 1}, {Resources.TextClut().begin(), Resources.TextClut().end()}});
		RenderLabels(HelpLabels, Resources.Shop().HelpLabels, 0);
		FVramImage WhiteClut{{0, WhiteClutY, 16, 1}, std::vector<uint16>(16, 0)};
		WhiteClut.Units[1] = White;
		Upload(WhiteClut);
		HideSelection();
		InitBackdrop();
		SelectionMode = FMenuPrimitive::MakeDrawMode(MenuGpu::TexturePage(0, 0, 0x140, 0x80));
		LoadStock();
		bDrawScene = true;
		bSoundOn = true;
		co_await RunTopMenu();
		co_await Shutdown();
	}

	void FMenuShop::Init()
	{
		const FGameState& Game = *Services.Game;
		const uint16 Allowed = static_cast<uint16>(Game.JoinedCharacters() & Game.MenuCharacters() & CharacterMask);
		for (int32 Character = 0; Character < CharacterCount; ++Character)
		{
			Available[static_cast<std::size_t>(Character)] = (Allowed >> Character & 1) != 0;
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
	}

	void FMenuShop::LoadStock()
	{
		// The shop's goods in its table's order (the shop number is not checked against the table's length).
		const std::span<const uint8> Shops = Resources.ShopStock();
		const std::size_t First = static_cast<std::size_t>(Request.Param) * StockRecord;
		StockCount = 0;
		Stock.fill(0);
		StockKinds.fill(EShopKind::Weapon);
		for (int32 Entry = 0; Entry < StockEntries; ++Entry)
		{
			const std::size_t At = First + static_cast<std::size_t>(Entry);
			const uint8 Id = At < Shops.size() ? Shops[At] : 0;
			if (Id != 0 && StockCount < static_cast<int32>(Stock.size()))
			{
				Stock[static_cast<std::size_t>(StockCount)] = Id;
				StockKinds[static_cast<std::size_t>(StockCount)] = static_cast<EShopKind>(Entry / KindEntries);
				++StockCount;
			}
		}
		const std::array<int32, 11>& FaceX = Resources.Shop().FaceX;
		for (std::size_t Face = 0; Face < Scene.OutlineTops.size(); ++Face)
		{
			// The outline's places as 16 bits.
			const int32 X = static_cast<int16>(FaceX[Face]);
			FMenuPrimitive& Top = Scene.OutlineTops[Face];
			FMenuPrimitive& Bottom = Scene.OutlineBottoms[Face];
			SetLine(Top, MenuGpu::FlatPolyline, 3, OutlineRed);
			SetLine(Bottom, MenuGpu::FlatPolyline, 3, OutlineRed);
			SetPoint(Top, 0, X, FaceTop);
			SetPoint(Top, 1, X + FaceWidth, FaceTop);
			SetPoint(Top, 2, X + FaceWidth, FaceBottom);
			SetPoint(Bottom, 0, X, FaceTop);
			SetPoint(Bottom, 1, X, FaceBottom);
			SetPoint(Bottom, 2, X + FaceWidth, FaceBottom);
			Scene.Outlined[Face] = false;
		}
		// The line under the total, from the gold on entry to the gold left.
		const FMenuShopTables& Tables = Resources.Shop();
		SetLine(Scene.GoldLine, MenuGpu::FlatLine, 2, LineWhite);
		SetPoint(Scene.GoldLine, 0, Tables.GoldX[1] - 8, Tables.GoldY[1] + 9);
		SetPoint(Scene.GoldLine, 1, Tables.GoldX[1] + 0x4E, Tables.GoldY[1] + 9);
	}

	void FMenuShop::DrawFrame()
	{
		ReadInput(ShopInput);
		if (PadHeld == SoftResetButtons)
		{
			bSoftReset = true;
			return;
		}
		Buffer ^= 1;
		UpdateTransition();
		LayerGte = MenuProjection(Layer);
		if (bDrawScene)
		{
			GrowWindows();
			AddChoiceCursors();
			AddLabels();
			AddSelection();
			if (bListCursorShown)
			{
				AddProjected(ListCursor.Quad, true, PiecesEntry);
			}
			AddScene();
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
		}
		AddBackdrop();
		Drawn.Primitives.clear();
		Table.TakeInto(Drawn.Primitives);
		Drawn.Layer = Layer;
		++Drawn.Number;
	}

	void FMenuShop::AddLabels()
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

	TMenuTask<> FMenuShop::RunTopMenu()
	{
		const FMenuShopTables& Tables = Resources.Shop();
		Command = BuyCommand;
		co_await RevealCommands(MainCommands);
		RenderLabels(ArcLabels, Tables.MainLabels, ArcLabelsBase);
		bool bGoesOn = true;
		while (bGoesOn)
		{
			co_await Frame();
			switch (Code)
			{
			case EMenuCode::Up:
				Command = Command + 1 > BuyCommand ? ExitCommand : Command + 1;
				break;
			case EMenuCode::Down:
				Command = Command == ExitCommand ? BuyCommand : Command - 1;
				break;
			case EMenuCode::Confirm:
				PlaySound(Effect::Confirm);
				Arc.bDimmed = true;
				HideSelection();
				ClearFlags(MenuFlag::ArcLabels, static_cast<int32>(Tables.MainLabels.size()));
				bGoesOn = co_await RunChoice();
				break;
			case EMenuCode::Cancel:
				bGoesOn = false;
				break;
			default:
				break;
			}
			if (Command != DrawnCommand)
			{
				DrawCommands(MainCommands, Command);
				PlaceArcLabel(ArcLabels, Tables.MainLabelOffsets, MenuFlag::ArcLabels, static_cast<int32>(Tables.MainLabels.size()), Command, 0);
				DrawnCommand = Command;
			}
		}
	}

	TMenuTask<bool> FMenuShop::RunChoice()
	{
		bool bChose = false;
		bool bGoesOn = true;
		if (Command == SellCommand)
		{
			bChose = co_await RunSellMenu();
		}
		else if (Command == BuyCommand)
		{
			bChose = co_await RunBuy();
		}
		else
		{
			bGoesOn = false;
		}
		if (bChose)
		{
			// The layer leaves without a sound; the arc's labels come back.
			LayerState = EMenuLayer::CloseAsked;
			RenderLabels(ArcLabels, Resources.Shop().MainLabels, ArcLabelsBase);
		}
		if (Command == BuyCommand)
		{
			CloseList(true, true);
		}
		Arc.bDimmed = false;
		Arc.bDimmedApplied = true;
		Flags[MenuFlag::Pointer] = 1;
		Flags[MenuFlag::Selection] = 1;
		DrawnCommand = 0xFF;
		Flags[MenuFlag::SubCommands] = 0;
		co_return bGoesOn;
	}

	TMenuTask<> FMenuShop::Shutdown()
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
