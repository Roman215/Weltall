#include "Foundation/LittleEndian.h"
#include "Game/CharacterEquipment.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Menu/MenuShop.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		using namespace GameStateLayout;

		constexpr int32 PiecesEntry = 4;
		constexpr int32 FullScale = 0x1000;
		constexpr int32 PageRows = 8;
		constexpr int32 LastRow = PageRows - 1;
		constexpr uint32 MostGold = 9999999;
		constexpr int32 SubLabelsBase = 2;
		/** Sell's inner arc: Equipment, Accessories, Weapons, Items; the cursor starts on Items. */
		constexpr int32 EquipmentCommand = 0;
		constexpr int32 AccessoriesCommand = 1;
		constexpr int32 WeaponsCommand = 2;
		constexpr int32 ItemsCommand = 3;
		constexpr int32 SubArcOffset = 3;
		/** A row: the count owned, and when some are marked the sale's "-" and quantity. */
		constexpr uint8 NameMode = 0x81;
		constexpr uint8 CountMode = 0x82;
		constexpr int32 SaleMark = 0xE5;
		constexpr int32 TotalX = 0x68;
		constexpr int32 TotalY = 0x54;
		/** The highlighted goods' name and sale price (half the price) on one line. */
		constexpr int32 DetailX = 0x2C;
		constexpr int32 PriceX = 0x98;
		constexpr int32 DetailY = 0x12;
		constexpr int32 DetailV = 0x4E;
		constexpr int32 RowHeight = 13;
		constexpr int32 OddClutX = 16;
		constexpr int32 TextClutY = 0x1D1;
		constexpr int32 WornFrame = 0x0E;
		constexpr int32 WornX = 0x0E;
		constexpr int32 WornY = 0xB4;
		/** Items the key flag marks are not sold. */
		constexpr uint8 KeyItem = 0x10;
		constexpr std::size_t ItemUse = 6;
		constexpr uint8 FirstAmmunition = 0x32;
		constexpr std::size_t WeaponSlot = 0x6A;
		constexpr std::size_t GunSlots = 0x6F;
		constexpr std::size_t AccessorySlots = 0x74;
		constexpr std::size_t EquipmentEntries = 8;

		namespace Effect
		{
			constexpr uint8 Confirm = 2;
			constexpr uint8 Buzzer = 4;
			constexpr uint8 Paid = 0xD1;
		}

		/** The dialogs: the sale's total asked about, the marks given up, a member chosen for Equipment. */
		constexpr uint8 SaleText = 0x95;
		constexpr uint8 CancelText = 0x92;
		constexpr uint8 ChooseText = 0x98;

		uint16 Word(std::span<const uint8> Record, std::size_t At)
		{
			return At + 2 <= Record.size() ? LittleEndian::ReadU16(Record, At) : uint16{0};
		}
	}

	TMenuTask<bool> FMenuShop::RunSellMenu()
	{
		const FMenuShopTables& Tables = Resources.Shop();
		SubCommand = ItemsCommand;
		DrawnSubCommand = 0xFF;
		bool bFirst = true;
		bool bGoesOn = true;
		while (bGoesOn)
		{
			co_await Frame();
			if (bFirst)
			{
				bFirst = false;
				RenderLabels(ArcLabels, Tables.SubLabels, SubLabelsBase);
				OpenLayer();
				co_await RevealSubCommands(0);
			}
			if (SubCommand != DrawnSubCommand)
			{
				PlaceArcLabel(ArcLabels, Tables.SubLabelOffsets, MenuFlag::ArcLabels, static_cast<int32>(Tables.SubLabels.size()), SubCommand, SubArcOffset);
				DrawSubCommands(0);
				DrawnSubCommand = SubCommand;
			}
			switch (Code)
			{
			case EMenuCode::Up:
				SubCommand = SubCommand + 1 >= SubCommandCount ? 0 : SubCommand + 1;
				break;
			case EMenuCode::Down:
				SubCommand = SubCommand == 0 ? SubCommandCount - 1 : SubCommand - 1;
				break;
			case EMenuCode::Confirm:
				PlaySound(Effect::Confirm);
				co_await RunSellChoice();
				DrawnSubCommand = 0xFF;
				break;
			case EMenuCode::Cancel:
				bGoesOn = false;
				break;
			default:
				break;
			}
		}
		HideSelection();
		ClearFlags(MenuFlag::ArcLabels, static_cast<int32>(Tables.SubLabels.size()));
		co_return true;
	}

	TMenuTask<> FMenuShop::RunSellChoice()
	{
		const FMenuShopTables& Tables = Resources.Shop();
		HideSelection();
		Flags[MenuFlag::SubCommands] = 0;
		ClearFlags(MenuFlag::ArcLabels, static_cast<int32>(Tables.SubLabels.size()));
		bool bListed = true;
		switch (SubCommand)
		{
		case EquipmentCommand:
			bListed = co_await SellEquipped();
			break;
		case AccessoriesCommand:
			co_await RunSellList(Services.Game->InventoryItems(2), Services.Game->InventoryCounts(2), EShopKind::Accessory, true, {}, 0);
			break;
		case WeaponsCommand:
			co_await RunSellList(Services.Game->InventoryItems(1), Services.Game->InventoryCounts(1), EShopKind::Weapon, true, {}, 0);
			break;
		case ItemsCommand:
			co_await RunSellList(Services.Game->InventoryItems(0), Services.Game->InventoryCounts(0), EShopKind::Item, true, {}, 0);
			break;
		default:
			break;
		}
		CloseList(bListed, false);
		Flags[MenuFlag::SubCommands] = 1;
		Flags[MenuFlag::Pointer] = 1;
		Flags[MenuFlag::Selection] = 1;
		RenderLabels(ArcLabels, Tables.SubLabels, SubLabelsBase);
	}

	TMenuTask<bool> FMenuShop::SellEquipped()
	{
		// A member chosen by the faces (Left and Right, wrapping), then the member's accessories.
		co_await RevealFaces(Resources.Shop().FaceX);
		co_await OpenDialog(ChooseText);
		const int32 Faces = static_cast<int32>(Scene.Faces.size());
		int32 Selected = 0;
		int32 Outlined = 1;
		int32 Result = 2;
		while (Result == 2)
		{
			co_await Frame();
			switch (Code)
			{
			case EMenuCode::Left:
				Selected = Selected - 1 < 0 ? Faces - 1 : Selected - 1;
				break;
			case EMenuCode::Right:
				Selected = Selected + 1 >= Faces ? 0 : Selected + 1;
				break;
			case EMenuCode::Confirm:
				PlaySound(Effect::Confirm);
				Result = 1;
				break;
			case EMenuCode::Cancel:
				Result = 0;
				break;
			default:
				break;
			}
			if (Selected != Outlined)
			{
				Scene.Outlined[static_cast<std::size_t>(Selected)] = true;
				Scene.Outlined[static_cast<std::size_t>(Outlined)] = false;
				Outlined = Selected;
			}
		}
		co_await CloseDialog();
		if (Result != 0)
		{
			// The face's member: Fei's for the first, else the one so many available characters after him.
			uint8 Character = 0;
			for (int32 Left = Selected, Next = 1; Left != 0; ++Next)
			{
				Character = static_cast<uint8>(Next);
				if (Available[static_cast<std::size_t>(Next)])
				{
					--Left;
				}
			}
			std::array<uint8, EquipmentEntries> Ids{};
			std::array<uint8, EquipmentEntries> Counts{};
			std::array<uint8, EquipmentEntries> Kinds{};
			Counts.fill(1);
			Kinds.fill(static_cast<uint8>(EShopKind::Accessory));
			const std::span<const uint8> Record = Services.Game->Block().subspan(CharacterOffset(Character), CharacterSize);
			std::size_t Listed = 0;
			for (std::size_t Slot = 0; Slot < 3; ++Slot)
			{
				if (Record[AccessorySlots + Slot] != 0)
				{
					Ids[Listed++] = Record[AccessorySlots + Slot];
				}
			}
			co_await RunSellList(Ids, Counts, EShopKind::Accessory, false, Kinds, Character);
		}
		co_return Result != 0;
	}

	TMenuTask<> FMenuShop::RunSellList(std::span<uint8> Ids, std::span<uint8> Counts, EShopKind Kind, bool bInventory, std::span<const uint8> Kinds, uint8 Character)
	{
		// The list compacted: goods held, key items left out.
		const std::size_t Entries = Ids.size();
		SaleIds.assign(Entries, 0);
		SaleOwned.assign(Entries, 0);
		SaleSold.assign(Entries, 0);
		SaleListed.assign(Entries, false);
		SaleKinds.assign(Entries, static_cast<uint8>(Kind));
		for (std::size_t Entry = 0; Entry < Entries && !bInventory; ++Entry)
		{
			SaleKinds[Entry] = Entry < Kinds.size() ? Kinds[Entry] : static_cast<uint8>(Kind);
		}
		SaleCount = 0;
		for (std::size_t Entry = 0; Entry < Entries; ++Entry)
		{
			if (Ids[Entry] == 0 || Counts[Entry] == 0)
			{
				continue;
			}
			if (static_cast<EShopKind>(SaleKinds[Entry]) == EShopKind::Item)
			{
				const std::span<const uint8> Record = GoodsRecord(Ids[Entry], EShopKind::Item);
				if (!Record.empty() && (Record[ItemUse] & KeyItem) != 0)
				{
					continue;
				}
			}
			const std::size_t At = static_cast<std::size_t>(SaleCount++);
			SaleIds[At] = Ids[Entry];
			SaleOwned[At] = Counts[Entry];
			SaleListed[At] = true;
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
				Price = ShowSellDetail(At < Entries ? SaleIds[At] : uint8{0}, At < Entries ? static_cast<EShopKind>(SaleKinds[At]) : Kind);
				Scene.bShown = true;
				DrawnTop = Top;
			}
			DrawListCursor(Row);
			if (bFirst)
			{
				OpenWindow(2, 0xC, 0x2A, 0xC4, 0x74, false, true, PiecesEntry, true);
				OpenWindow(3, 0x20, 0xE, 0xFC, 0x14, false, true, PiecesEntry, false);
				OpenLayer();
				if (bInventory)
				{
					co_await RevealFaces(Resources.Shop().FaceX);
				}
				while (LayerState != EMenuLayer::Still)
				{
					co_await Frame();
				}
				const FMenuShopTables& Tables = Resources.Shop();
				DrawStatics(Tables.SellStatics.Frames, Tables.SellStatics.X, Tables.SellStatics.Y);
				bFirst = false;
			}
			if (bRedraws)
			{
				DrawGold(OnEntry, Total, Left);
				bRedraws = false;
			}
			int32 Scrolled = Top;
			const std::size_t At = static_cast<std::size_t>(Top + Row);
			switch (Code)
			{
			case EMenuCode::Right:
				// One more marked for sale while any are owned (no limit for the gold).
				if (At < Entries && SaleListed[At] && SaleOwned[At] > 0)
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
				if (At < Entries && SaleListed[At] && SaleSold[At] > 0)
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
				if (co_await AskYesNo(SaleText))
				{
					CommitSale(Left, Ids, Counts, bInventory, Character);
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
					if (!co_await AskYesNo(CancelText))
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

	void FMenuShop::RenderSellRows(int32 Top)
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
			DrawRow(Row, GoodsName(Id, static_cast<EShopKind>(SaleKinds[At])), SaleOwned[At], NameMode, CountMode);
			if (SaleSold[At] != 0)
			{
				DrawCartMark(Row, SaleMark, SaleSold[At]);
			}
		}
	}

	uint32 FMenuShop::ShowSellDetail(uint8 Id, EShopKind Kind)
	{
		// The goods' name and their sale price, half the price, in the two planes of one strip.
		const std::span<const uint8> Record = GoodsRecord(Id, Kind);
		std::fill(DetailImage.Units.begin(), DetailImage.Units.end(), uint16{0});
		Scene.Detail.Width = static_cast<uint8>(RenderString(DetailTyping, DetailImage, GoodsName(Id, Kind), 0));
		const uint16 Users = Kind == EShopKind::Item ? uint16{0} : Word(Record, 0);
		const uint32 Price = static_cast<uint32>(Word(Record, Kind == EShopKind::Weapon ? 4 : 2) >> 1);
		const uint16 Wearers = WornMask(Id, Kind);
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
		const std::array<int32, 11>& FaceX = Resources.Shop().FaceX;
		Scene.Worn.clear();
		std::size_t Face = 0;
		for (int32 Character = 0; Character < static_cast<int32>(Available.size()) && Face < Scene.Outlined.size(); ++Character)
		{
			if (!Available[static_cast<std::size_t>(Character)])
			{
				continue;
			}
			Scene.Outlined[Face] = (Users >> Character & 1) != 0;
			if ((Wearers >> Character & 1) != 0)
			{
				Resources.Sheet().Draw(WornFrame, FaceX[Face] + WornX, WornY, FullScale, Scene.Worn);
			}
			++Face;
		}
		return Price;
	}

	void FMenuShop::CommitSale(uint32 Gold, std::span<uint8> Ids, std::span<uint8> Counts, bool bInventory, uint8 Character)
	{
		PlaySound(Effect::Paid);
		Services.Game->SetGold(std::min(Gold, MostGold));
		const std::size_t Entries = SaleIds.size();
		if (!bInventory)
		{
			// Equipment: the first slot holding each sold one is emptied; the character is recomputed with the fix alone.
			const std::span<uint8> Record = Services.Game->Block().subspan(CharacterOffset(Character), CharacterSize);
			for (std::size_t Entry = 0; Entry < Entries; ++Entry)
			{
				const uint8 Id = SaleIds[Entry];
				if (Id == 0 || SaleSold[Entry] == 0)
				{
					continue;
				}
				const EShopKind Kind = static_cast<EShopKind>(SaleKinds[Entry]);
				if (Kind == EShopKind::Weapon && Id < FirstAmmunition)
				{
					Record[WeaponSlot] = 0;
				}
				else if (Kind == EShopKind::Weapon || Kind == EShopKind::Accessory)
				{
					const std::size_t First = Kind == EShopKind::Weapon ? GunSlots : AccessorySlots;
					const std::size_t Slots = Kind == EShopKind::Weapon ? 4 : 3;
					for (std::size_t Slot = 0; Slot < Slots; ++Slot)
					{
						if (Record[First + Slot] == Id)
						{
							Record[First + Slot] = 0;
							break;
						}
					}
				}
			}
			if (Services.Options.bSaleRecomputesEquipment)
			{
				RecomputeCharacterEquipment(*Services.Game, Character, Resources.Equipment());
			}
			return;
		}
		// A list: every slot holding a sold one loses the quantity; an emptied slot is cleared (the list is not compacted).
		for (std::size_t Entry = 0; Entry < Entries; ++Entry)
		{
			if (SaleIds[Entry] == 0)
			{
				continue;
			}
			for (std::size_t Slot = 0; Slot < Entries; ++Slot)
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
}
