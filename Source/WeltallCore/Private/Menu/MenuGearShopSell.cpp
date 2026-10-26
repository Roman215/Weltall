#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Menu/MenuGearShop.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		using namespace GameStateLayout;
		using LittleEndian::ReadU16;
		using LittleEndian::ReadU32;
		using LittleEndian::WriteU16;
		using LittleEndian::WriteU32;

		constexpr int32 PiecesEntry = 4;
		constexpr int32 FullScale = 0x1000;
		constexpr int32 PageRows = 8;
		constexpr int32 LastRow = PageRows - 1;
		constexpr uint32 MostGold = 9999999;
		constexpr int32 ArcLabelsBase = 2;
		constexpr int32 ArcLabelCount = 4;
		/** A row: the count owned, and when some are marked the sale's "-" and quantity. */
		constexpr uint8 NameMode = 0x81;
		constexpr uint8 CountMode = 0x82;
		constexpr int32 SaleMark = 0xE5;
		/** The highlighted goods' name and sale price (half the price) on one line. */
		constexpr int32 DetailX = 0x2C;
		constexpr int32 PriceX = 0x98;
		constexpr int32 DetailY = 0x12;
		constexpr int32 DetailV = 0x4E;
		constexpr int32 RowHeight = 13;
		constexpr int32 OddClutX = 16;
		constexpr int32 TextClutY = 0x1D1;
		constexpr int32 FittedFrame = 0x0E;
		constexpr int32 FittedX = 0x0E;
		constexpr int32 FittedY = 0xB4;
		constexpr int32 TotalX = 0x68;
		constexpr int32 TotalY = 0x54;
		/** Fuel's total and its gold panel: the gold, then "G". */
		constexpr int32 FuelTotalX = 0x50;
		constexpr int32 FuelTotalY = 100;
		constexpr int32 GoldPanelX = 0xAA;
		constexpr int32 GoldPanelY = 0xAE;
		constexpr int32 GoldFrame = 0x10;
		constexpr int32 GoldFrameX = 0xF2;
		constexpr int32 DigitWidth = 8;
		constexpr uint8 Blank = 0xFF;
		constexpr int32 GearWeaponsList = 3;
		constexpr int32 GearPartsList = 4;
		/** Sell's inner arc: parts, gear weapons. */
		constexpr int32 PartsCommand = 0;
		constexpr int32 WeaponsCommand = 1;
		/** Fuel: 10 gold a hundred units (at least 10); HP comes free with it. */
		constexpr int32 FuelPerPrice = 100;
		constexpr int32 PricePerHundred = 10;

		namespace Effect
		{
			constexpr uint8 Confirm = 2;
			constexpr uint8 Buzzer = 4;
			constexpr uint8 Paid = 0xD1;
		}

		/** The dialogs: the sale's total asked about, the marks given up; fuel's and HP's restoring, and the shortfall. */
		constexpr uint8 SaleText = 0x95;
		constexpr uint8 CancelText = 0x92;
		constexpr uint8 RestoreText = 0xA6;
		constexpr uint8 ShortText = 0xA9;
		constexpr uint8 RepairText = 0xB5;
		constexpr uint8 FullText = 0xB8;
		/** What the fuel's dialog does: nothing to restore, fuel (and HP), HP alone. */
		enum class ERestore : uint8
		{
			Nothing,
			Fuel,
			Hp,
		};
	}

	TMenuTask<bool> FMenuGearShop::Ask(uint8 FirstText, std::optional<uint8> SecondText, bool bWaits)
	{
		constexpr std::size_t YesCursor = 2;
		constexpr std::size_t NoCursor = 3;
		co_await OpenDialog(FirstText);
		if (SecondText)
		{
			ChoiceShown[YesCursor] = false;
			ChoiceShown[NoCursor] = false;
		}
		else
		{
			ChoiceShown[NoCursor] = true;
		}
		bool bYes = co_await RunYesNo(bWaits, !SecondText);
		co_await CloseDialog();
		if (SecondText)
		{
			// A notice first, then the question.
			ChoiceShown[NoCursor] = true;
			co_await OpenDialog(*SecondText);
			ChoiceShown[NoCursor] = true;
			bYes = co_await RunYesNo(bWaits, true);
			co_await CloseDialog();
		}
		co_return bYes;
	}

	TMenuTask<> FMenuGearShop::RunSell()
	{
		HideSelection();
		Flags[MenuFlag::SubCommands] = 0;
		ClearFlags(MenuFlag::ArcLabels, ArcLabelCount);
		if (SubCommand == PartsCommand)
		{
			co_await RunSellList(Services.Game->InventoryItems(GearPartsList), Services.Game->InventoryCounts(GearPartsList), EGearGoods::Part);
		}
		else if (SubCommand == WeaponsCommand)
		{
			co_await RunSellList(Services.Game->InventoryItems(GearWeaponsList), Services.Game->InventoryCounts(GearWeaponsList), EGearGoods::Weapon);
		}
		ClearListScreen(true);
		Flags[MenuFlag::SubCommands] = 1;
		Flags[MenuFlag::Pointer] = 1;
		Flags[MenuFlag::Selection] = 1;
		RenderLabels(std::span<FMenuLabel>(ArcLabels).first(ArcLabelCount), Tables.SubLabels, ArcLabelsBase);
	}

	TMenuTask<> FMenuGearShop::RunSellList(std::span<uint8> Ids, std::span<uint8> Counts, EGearGoods Kind)
	{
		// The list compacted: the goods held.
		const std::size_t Listed = Ids.size();
		SaleIds.assign(Listed, 0);
		SaleOwned.assign(Listed, 0);
		SaleSold.assign(Listed, 0);
		SaleListed.assign(Listed, false);
		SaleCount = 0;
		SaleKind = Kind;
		for (std::size_t Entry = 0; Entry < Listed; ++Entry)
		{
			if (Ids[Entry] != 0 && Counts[Entry] != 0)
			{
				const std::size_t At = static_cast<std::size_t>(SaleCount++);
				SaleIds[At] = Ids[Entry];
				SaleOwned[At] = Counts[Entry];
				SaleListed[At] = true;
			}
		}
		ListCursor.Cel = 4;
		ListCursor.Frames = 0;
		const uint32 OnEntry = Services.Game->Gold();
		uint32 Left = OnEntry;
		uint32 Total = 0;
		uint32 Price = 0;
		bool bFirst = true;
		bool bRedraws = true;
		bool bGoesOn = true;
		int32 Row = 0;
		int32 Top = 0;
		int32 DrawnRow = 0xFF;
		int32 DrawnTop = 0xFF;
		const auto Hide = [this](bool bShown)
		{
			if (!bShown)
			{
				Scene.bShown = false;
			}
			Flags[static_cast<std::size_t>(MenuFlag::Windows + 2)] = bShown ? 1 : 0;
			Flags[static_cast<std::size_t>(MenuFlag::Windows + 3)] = bShown ? 1 : 0;
			bScrollThumbShown = bShown;
			bListCursorShown = bShown;
		};
		while (bGoesOn)
		{
			co_await Frame();
			if (Top != DrawnTop || bRedraws)
			{
				RenderSellRows(Top);
				DrawScrollThumb(SaleCount, Top);
			}
			if (Row != DrawnRow || Top != DrawnTop)
			{
				DrawnRow = Row;
				const std::size_t At = static_cast<std::size_t>(Top + Row);
				Price = ShowSellDetail(At < Listed ? SaleIds[At] : uint8{0}, Kind);
				Scene.bShown = true;
				DrawnTop = Top;
			}
			DrawListCursor(Row);
			if (bFirst)
			{
				OpenWindow(2, 0xC, 0x2A, 0xC4, 0x74, false, true, PiecesEntry, true);
				OpenWindow(3, 0x20, 0xE, 0xFC, 0x14, false, true, PiecesEntry, false);
				co_await RevealFaces(Tables.FaceX);
				DrawStatics(Tables.SellStatics.Frames, Tables.SellStatics.X, Tables.SellStatics.Y);
				bFirst = false;
			}
			if (bRedraws)
			{
				DrawMoneyPanel(OnEntry, Total, Left, 0, 0);
				bRedraws = false;
			}
			int32 Scrolled = Top;
			const std::size_t At = static_cast<std::size_t>(Top + Row);
			switch (Code)
			{
			case EMenuCode::Right:
				// One more marked for sale while any are owned (no limit for the gold).
				if (At < Listed && SaleListed[At] && SaleOwned[At] > 0)
				{
					++SaleSold[At];
					--SaleOwned[At];
					Left += Price;
					Total += Price;
					bRedraws = true;
				}
				break;
			case EMenuCode::Down:
				if (++Row > LastRow)
				{
					Row = LastRow;
					Scrolled = Top + 1 > SaleCount - PageRows ? Top : Top + 1;
				}
				break;
			case EMenuCode::Left:
				if (At < Listed && SaleListed[At] && SaleSold[At] > 0)
				{
					++SaleOwned[At];
					--SaleSold[At];
					Left -= Price;
					Total -= Price;
					bRedraws = true;
				}
				break;
			case EMenuCode::Up:
				if (--Row < 0)
				{
					Row = 0;
					Scrolled = std::max(Top - 1, 0);
				}
				break;
			case EMenuCode::Confirm:
				if (Total == 0)
				{
					PlaySound(Effect::Buzzer);
					break;
				}
				PlaySound(Effect::Confirm);
				Hide(false);
				bGoesOn = false;
				OpenChoiceCursors();
				DrawTotal(Total, TotalX, TotalY - (Resources.IsFrench() ? 1 : 0));
				if (co_await Ask(SaleText, std::nullopt, true))
				{
					CommitSale(Left, Ids, Counts);
				}
				else
				{
					Hide(true);
					DrawnRow = 0xFF;
					DrawnTop = 0xFF;
					bGoesOn = true;
				}
				co_await CloseChoiceCursors();
				break;
			case EMenuCode::Cancel:
				bGoesOn = false;
				if (Total != 0)
				{
					Hide(false);
					OpenChoiceCursors();
					if (!co_await Ask(CancelText, std::nullopt, true))
					{
						Hide(true);
						DrawnRow = 0xFF;
						DrawnTop = 0xFF;
						bGoesOn = true;
					}
					co_await CloseChoiceCursors();
				}
				break;
			default:
				break;
			}
			Top = Scrolled;
		}
	}

	void FMenuGearShop::RenderSellRows(int32 Top)
	{
		std::fill(NameImage.Units.begin(), NameImage.Units.end(), uint16{0});
		for (int32 Row = 0; Row < PageRows; ++Row)
		{
			const std::size_t At = static_cast<std::size_t>(Top + Row);
			FMenuShopRow& Shown = Scene.Rows[static_cast<std::size_t>(Row)];
			Shown.Cart.clear();
			const uint8 Id = At < SaleIds.size() ? SaleIds[At] : uint8{0};
			if (Id == 0)
			{
				Shown.bShown = false;
				continue;
			}
			DrawRow(Row, GoodsName(Id, SaleKind), SaleOwned[At], NameMode, CountMode);
			if (SaleSold[At] != 0)
			{
				DrawCartMark(Row, SaleMark, SaleSold[At]);
			}
		}
	}

	uint32 FMenuGearShop::ShowSellDetail(uint8 Id, EGearGoods Kind)
	{
		// The goods' name and their sale price, half the price, in the two planes of one strip.
		std::fill(DetailImage.Units.begin(), DetailImage.Units.end(), uint16{0});
		Scene.Detail.Width = static_cast<uint8>(RenderString(DetailTyping, DetailImage, GoodsName(Id, Kind), 0));
		const uint32 Price = GoodsPrice(Id, Kind) >> 1;
		const uint32 Users = GoodsUsers(Id, Kind);
		const uint32 Fitted = FittedMask(Id, Kind);
		GlyphText(NumberGlyphs(Price), GlyphScratch);
		Scene.SalePrice.Width = static_cast<uint8>(RenderString(DetailTyping, DetailImage, GlyphScratch, 1));
		Upload(DetailImage);
		SetLabelTexture(Scene.Detail, 0, 0);
		SetLabelTexture(Scene.SalePrice, 0, 0);
		RecordLabel(Scene.Detail);
		RecordLabel(Scene.SalePrice);
		Scene.SalePrice.Quad.Clut = MenuGpu::Clut(OddClutX, TextClutY);
		SetTexelRect(Scene.Detail.Quad, 0, DetailV, Scene.Detail.Width, RowHeight);
		SetTexelRect(Scene.SalePrice.Quad, 0, DetailV, Scene.SalePrice.Width, RowHeight);
		SetLayerQuad(Scene.Detail.Quad.Points, DetailX, DetailY, Scene.Detail.Width, RowHeight);
		SetLayerQuad(Scene.SalePrice.Quad.Points, PriceX, DetailY, Scene.SalePrice.Width, RowHeight);
		Scene.bDetailShown = Id != 0;
		Scene.bSalePriceShown = Id != 0;
		Scene.Worn.clear();
		std::size_t Face = 0;
		for (int32 Character = 0; Character < static_cast<int32>(Available.size()) && Face < Scene.Outlined.size(); ++Character)
		{
			if (!Available[static_cast<std::size_t>(Character)])
			{
				continue;
			}
			const uint8 Gear = GearOf(Character);
			const uint32 Bit = Gear < 32 ? 1u << Gear : 0;
			Scene.Outlined[Face] = (Users & Bit) != 0;
			if ((Fitted & Bit) != 0)
			{
				Resources.Sheet().Draw(FittedFrame, Tables.FaceX[Face] + FittedX, FittedY, FullScale, Scene.Worn);
			}
			++Face;
		}
		return Price;
	}

	void FMenuGearShop::CommitSale(uint32 Gold, std::span<uint8> Ids, std::span<uint8> Counts)
	{
		PlaySound(Effect::Paid);
		Services.Game->SetGold(std::min(Gold, MostGold));
		// Every slot holding a sold one loses the quantity; an emptied slot is cleared (the list is not compacted).
		for (std::size_t Entry = 0; Entry < SaleIds.size(); ++Entry)
		{
			if (SaleIds[Entry] == 0)
			{
				continue;
			}
			for (std::size_t Slot = 0; Slot < Ids.size(); ++Slot)
			{
				if (SaleIds[Entry] == Ids[Slot])
				{
					Counts[Slot] = static_cast<uint8>(Counts[Slot] - SaleSold[Entry]);
					if (Counts[Slot] == 0)
					{
						Ids[Slot] = 0;
					}
				}
			}
		}
	}

	void FMenuGearShop::ClearListScreen(bool bCloses)
	{
		ClearScene();
		if (bCloses)
		{
			CloseWindow(2);
			CloseWindow(3);
			bScrollThumbShown = false;
			bListCursorShown = false;
		}
	}

	TMenuTask<> FMenuGearShop::RunRestore()
	{
		const std::span<uint8> Record = Services.Game->Block().subspan(GearOffset(CurrentGear), GearSize);
		const uint16 Fuel = ReadU16(Record, Gear::Fuel);
		const uint16 MaxFuel = ReadU16(Record, Gear::MaxFuel);
		const uint32 Hp = ReadU32(Record, Gear::Hp);
		const uint32 MaxHp = ReadU32(Record, Gear::MaxHp);
		ERestore Restore = ERestore::Fuel;
		uint8 Text = RestoreText;
		if (MaxFuel == Fuel)
		{
			Restore = Hp == MaxHp ? ERestore::Nothing : ERestore::Hp;
			Text = Hp == MaxHp ? FullText : RepairText;
		}
		// Ten gold a hundred units missing, at least ten: also asked with the fuel full, when it charges nothing.
		uint32 Hundreds = static_cast<uint32>((static_cast<int32>(MaxFuel) - static_cast<int32>(Fuel)) / FuelPerPrice) & 0xFFFF;
		Hundreds = Hundreds == 0 ? 1 : Hundreds;
		const uint32 Cost = Hundreds * PricePerHundred;
		OpenWindow(5, 0xA2, 0xA6, 0x60, 0x14, false, true, PiecesEntry, false);
		DrawGoldPanel(Services.Game->Gold());
		if (Restore != ERestore::Nothing)
		{
			DrawTotal(Cost, FuelTotalX, FuelTotalY - (Resources.IsFrench() ? 1 : 0));
		}
		co_await Frame();
		OpenChoiceCursors();
		bool bRepaired = false;
		if (co_await Ask(Text, std::nullopt, Restore != ERestore::Nothing))
		{
			TotalState = 0;
			FGameState& Game = *Services.Game;
			if (Restore == ERestore::Fuel && Game.Gold() < Cost)
			{
				// As much as the gold buys: the HP too, unless there is no gold at all.
				if (co_await Ask(ShortText, std::nullopt, true))
				{
					const uint32 Gold = Game.Gold();
					bRepaired = Gold != 0;
					const uint16 Bought = static_cast<uint16>(ReadU16(Record, Gear::Fuel) + static_cast<uint16>(static_cast<int16>(Gold / PricePerHundred) * FuelPerPrice));
					WriteU16(Record, Gear::Fuel, Bought > MaxFuel ? MaxFuel : Bought);
					Game.SetGold(Gold % PricePerHundred);
				}
			}
			else if (Restore == ERestore::Fuel)
			{
				Game.SetGold(Game.Gold() - Cost);
				WriteU16(Record, Gear::Fuel, MaxFuel);
				bRepaired = true;
			}
			else if (Restore == ERestore::Hp)
			{
				bRepaired = true;
			}
		}
		if (bRepaired)
		{
			WriteU32(Record, Gear::Hp, ReadU32(Record, Gear::MaxHp));
			PlaySound(Effect::Paid);
		}
		co_await CloseChoiceCursors();
		Scene.bGoldPanelShown = false;
		CloseWindow(5);
	}

	void FMenuGearShop::DrawGoldPanel(uint32 Gold)
	{
		SplitNumber(Gold);
		Scene.GoldPanel.clear();
		for (int32 Digit = 0; Digit < 9; ++Digit)
		{
			const uint8 Frame = NumberDigits[static_cast<std::size_t>(Digit)];
			if (Frame != Blank)
			{
				Resources.Sheet().Draw(Frame, GoldPanelX + Digit * DigitWidth, GoldPanelY, FullScale, Scene.GoldPanel);
			}
		}
		Resources.Sheet().Draw(GoldFrame, GoldFrameX, GoldPanelY, FullScale, Scene.GoldPanel);
		Scene.bGoldPanelShown = true;
	}
}
