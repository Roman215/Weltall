#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Menu/MenuShop.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		using namespace GameStateLayout;
		using LittleEndian::ReadU16;

		constexpr int32 PiecesEntry = 4;
		constexpr int32 FullScale = 0x1000;
		constexpr int32 PageRows = 8;
		constexpr int32 LastRow = PageRows - 1;
		constexpr uint8 Affordable = 0x80;
		/** A cart holds at most as many as bring the inventory's count to 99. */
		constexpr int32 MostHeld = 99;
		constexpr uint32 MostGold = 9999999;
		/** The list's heading ("Stored") beside the stored count. */
		constexpr int16 HeadingX = 0xEC;
		constexpr int16 HeadingTop = 0x7E;
		constexpr int16 HeadingBottom = 0x8B;
		constexpr int32 ListLabelsBase = 2;
		/** The highlighted goods' line and the stored count, from their strips. */
		constexpr int32 DetailX = 0x2C;
		constexpr int32 DetailY = 0x12;
		constexpr int32 DetailV = 0x4E;
		constexpr int32 RowHeight = 13;
		constexpr int32 StoredRow = 9;
		constexpr int32 StripBase = 0x80;
		constexpr uint8 StoredMode = 0x82;
		constexpr int16 StoredStripX = 0x198;
		constexpr int16 StoredStripY = 0xB4;
		constexpr int16 StoredStripWidth = 0x28;
		constexpr int32 StoredX = 0xF8;
		constexpr int32 StoredY = 0x8E;
		constexpr uint16 FirstDigitGlyph = 0x10;
		constexpr uint16 BlankGlyph = 0xC3;
		/** "E": the goods are worn; the changes a purchase would make, under each face. */
		constexpr int32 WornFrame = 0x0E;
		constexpr int32 WornX = 0x0E;
		constexpr int32 WornY = 0xB4;
		constexpr int32 ChangesX = 0x49;
		constexpr int32 ChangesStep = 0x1A;
		constexpr int32 AttackY = 0xBE;
		constexpr int32 DefenseY = 0xC6;
		constexpr FDrawColor GainTint{0x80, 0x40, 0x40};
		constexpr FDrawColor LossTint{0x40, 0x40, 0x80};
		constexpr int32 CartMark = 0xF1;
		constexpr int32 TotalX = 0x68;
		constexpr int32 TotalY = 0x54;
		/** Billy's guns: their types; an Ether Pistol's (5) is never replaced in the comparison. */
		constexpr uint8 Billy = 4;
		constexpr uint8 EtherPistol = 5;
		constexpr std::size_t WeaponSlot = 0x6A;
		constexpr std::size_t GunSlots = 0x6F;
		constexpr std::size_t AccessorySlots = 0x74;
		constexpr std::size_t BaseAttack = 0x58;
		constexpr std::size_t BaseDefense = 0x59;

		namespace Effect
		{
			constexpr uint8 Confirm = 2;
			constexpr uint8 Buzzer = 4;
			constexpr uint8 Paid = 0xD1;
		}

		/** The dialogs: the purchase's total asked about, the cart given up. */
		constexpr uint8 PurchaseText = 0x8F;
		constexpr uint8 CancelText = 0x8C;

		int32 InventoryList(EShopKind Kind)
		{
			return Kind == EShopKind::Weapon ? 1 : (Kind == EShopKind::Accessory ? 2 : 0);
		}

		uint8 Field(std::span<const uint8> Record, std::size_t At)
		{
			return At < Record.size() ? Record[At] : uint8{0};
		}

		uint16 Word(std::span<const uint8> Record, std::size_t At)
		{
			return At + 2 <= Record.size() ? ReadU16(Record, At) : uint16{0};
		}
	}

	TMenuTask<bool> FMenuShop::RunBuy()
	{
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
		std::array<uint8, PageRows> Affordables{};
		Cart.fill(0);
		Arc.bDimmed = true;
		ListCursor.Cel = 4;
		ListCursor.Frames = 0;
		// The list hidden under a dialog; after a No it comes back, its rows and line drawn again.
		const auto Hide = [this](bool bShown)
		{
			if (!bShown)
			{
				Scene.bShown = false;
			}
			for (const int32 Window : {2, 3, 5})
			{
				Flags[static_cast<std::size_t>(MenuFlag::Windows + Window)] = bShown ? 1 : 0;
			}
			bScrollThumbShown = bShown;
			bListCursorShown = bShown;
		};
		while (bGoesOn)
		{
			co_await Frame();
			if (Top != DrawnTop || bRedraws)
			{
				RenderBuyRows(Top, Left, Affordables);
				DrawScrollThumb(StockCount, Top);
			}
			if (Row != DrawnRow || Top != DrawnTop)
			{
				Price = ShowBuyDetail(Row, Top);
				Scene.bShown = true;
				DrawnTop = Top;
				DrawnRow = Row;
			}
			DrawListCursor(Row);
			if (bFirst)
			{
				// The heading, the windows, the faces; the list flies in.
				const FMenuShopTables& Tables = Resources.Shop();
				RenderLabels(std::span<FMenuLabel>(ArcLabels).first(Tables.ListLabels.size()), Tables.ListLabels, ListLabelsBase);
				FMenuLabel& Heading = ArcLabels[0];
				RecordLabel(Heading);
				Heading.Quad.Corners[0].X = HeadingX;
				Heading.Quad.Corners[0].Y = HeadingTop;
				Heading.Quad.Corners[1].X = static_cast<int16>(HeadingX + Heading.Width);
				Heading.Quad.Corners[1].Y = HeadingTop;
				Heading.Quad.Corners[2].X = HeadingX;
				Heading.Quad.Corners[2].Y = HeadingBottom;
				Heading.Quad.Corners[3].X = static_cast<int16>(HeadingX + Heading.Width);
				Heading.Quad.Corners[3].Y = HeadingBottom;
				Flags[MenuFlag::ArcLabels] = 0;
				OpenWindow(2, 0xC, 0x2A, 0xC4, 0x74, false, true, PiecesEntry, true);
				OpenWindow(3, 0x20, 0xE, 0xFC, 0x14, false, true, PiecesEntry, false);
				OpenWindow(5, 0xE0, 0x7A, 0x40, 0x24, false, true, PiecesEntry, false);
				OpenLayer();
				co_await RevealFaces(Tables.FaceX);
				DrawStatics(Tables.BuyStatics.Frames, Tables.BuyStatics.X, Tables.BuyStatics.Y);
				bFirst = false;
				while (LayerState != EMenuLayer::Still)
				{
					co_await Frame();
				}
				Flags[MenuFlag::ArcLabels] = 1;
			}
			if (bRedraws)
			{
				DrawGold(OnEntry, Total, Left);
				bRedraws = false;
			}
			int32 Scrolled = Top;
			uint8& InCart = Cart[static_cast<std::size_t>(Top + Row)];
			switch (Code)
			{
			case EMenuCode::Right:
				// One more when affordable and the inventory would hold it (the equipped copies not counted).
				if (Affordables[static_cast<std::size_t>(Row)] != 0 && InCart + StoredCount + 1 < MostHeld + 1)
				{
					Total += Price;
					Left -= Price;
					bRedraws = true;
					++InCart;
				}
				break;
			case EMenuCode::Down:
				if (++Row > LastRow)
				{
					Row = LastRow;
					Scrolled = Top + 1 > StockCount - PageRows ? Top : Top + 1;
				}
				break;
			case EMenuCode::Left:
				if (InCart != 0)
				{
					Total -= Price;
					Left += Price;
					bRedraws = true;
					--InCart;
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
				Flags[MenuFlag::ArcLabels] = 0;
				OpenChoiceCursors();
				DrawTotal(Total, TotalX, TotalY - (Resources.IsFrench() ? 1 : 0));
				bGoesOn = !co_await AskYesNo(PurchaseText);
				if (bGoesOn)
				{
					Hide(true);
					DrawnTop = 0xFF;
					DrawnRow = 0xFF;
					Flags[MenuFlag::ArcLabels] = 1;
				}
				else
				{
					CommitPurchase(Left);
				}
				co_await CloseChoiceCursors();
				break;
			case EMenuCode::Cancel:
				bGoesOn = false;
				Flags[MenuFlag::ArcLabels] = 0;
				if (Total != 0)
				{
					Hide(false);
					OpenChoiceCursors();
					bGoesOn = !co_await AskYesNo(CancelText);
					if (bGoesOn)
					{
						Hide(true);
						DrawnTop = 0xFF;
						DrawnRow = 0xFF;
						Flags[MenuFlag::ArcLabels] = 1;
					}
					co_await CloseChoiceCursors();
				}
				co_await Frame();
				break;
			default:
				break;
			}
			Top = Scrolled;
		}
		Scene.bStoredShown = false;
		co_return true;
	}

	void FMenuShop::RenderBuyRows(int32 Top, uint32 Left, std::array<uint8, 8>& Affordables)
	{
		std::fill(NameImage.Units.begin(), NameImage.Units.end(), uint16{0});
		for (int32 Row = 0; Row < PageRows; ++Row)
		{
			const std::size_t Index = static_cast<std::size_t>(Top + Row);
			FMenuShopRow& Shown = Scene.Rows[static_cast<std::size_t>(Row)];
			Affordables[static_cast<std::size_t>(Row)] = 0;
			Shown.Cart.clear();
			const uint8 Id = Index < Stock.size() ? Stock[Index] : uint8{0};
			if (Id == 0)
			{
				Shown.bShown = false;
				continue;
			}
			const EShopKind Kind = StockKinds[Index];
			const uint32 Price = Word(GoodsRecord(Id, Kind), Kind == EShopKind::Weapon ? 4 : 2);
			const uint8 Mode = static_cast<int64>(Left) < static_cast<int64>(Price) ? uint8{0} : Affordable;
			Affordables[static_cast<std::size_t>(Row)] = Mode;
			DrawRow(Row, GoodsName(Id, Kind), Price, static_cast<uint8>(Mode + 1), static_cast<uint8>(Mode + 2));
			if (Cart[Index] != 0)
			{
				DrawCartMark(Row, CartMark, Cart[Index]);
			}
		}
	}

	uint32 FMenuShop::ShowBuyDetail(int32 Row, int32 Top)
	{
		const std::size_t Index = static_cast<std::size_t>(Top + Row);
		const uint8 Id = Index < Stock.size() ? Stock[Index] : uint8{0};
		const EShopKind Kind = Index < Stock.size() ? StockKinds[Index] : EShopKind::Weapon;
		const std::span<const uint8> Record = GoodsRecord(Id, Kind);
		const FMenuResources::EText Help = Kind == EShopKind::Accessory ? FMenuResources::EText::ShopAccessoryHelp
			: (Kind == EShopKind::Item ? FMenuResources::EText::ShopItemHelp : FMenuResources::EText::ShopWeaponHelp);
		std::fill(DetailImage.Units.begin(), DetailImage.Units.end(), uint16{0});
		Scene.Detail.Width = static_cast<uint8>(RenderString(DetailTyping, DetailImage, Resources.Text(Help).String(Id), 0));
		const uint32 Price = Word(Record, Kind == EShopKind::Weapon ? 4 : 2);
		const uint16 Users = Kind == EShopKind::Item ? uint16{0} : Word(Record, 0);
		const uint16 Wearers = WornMask(Id, Kind);
		Upload(DetailImage);
		SetLabelTexture(Scene.Detail, 0, 0);
		RecordLabel(Scene.Detail);
		SetTexelRect(Scene.Detail.Quad, 0, DetailV, Scene.Detail.Width, RowHeight);
		SetLayerQuad(Scene.Detail.Quad.Points, DetailX, DetailY, Scene.Detail.Width, RowHeight);
		Scene.bDetailShown = Id != 0;

		// For each available character: the outline when the goods fit, "E" when worn, the changes they make.
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
			Scene.AttackChanges[Face].clear();
			Scene.DefenseChanges[Face].clear();
			if (Scene.Outlined[Face])
			{
				std::array<int32, 2> Changes{};
				std::array<bool, 2> Losses{};
				CompareEquipment(Id, Kind, static_cast<uint8>(Character), Changes, Losses);
				const int32 X = ChangesX + ChangesStep * static_cast<int32>(Face);
				const std::array<std::vector<FMenuPrimitive>*, 2> Targets = {&Scene.AttackChanges[Face], &Scene.DefenseChanges[Face]};
				for (std::size_t Stat = 0; Stat < 2; ++Stat)
				{
					if (Changes[Stat] == 0)
					{
						continue;
					}
					DrawDigitsInto(*Targets[Stat], static_cast<uint32>(Changes[Stat]), 6, 3, X, Stat == 0 ? AttackY : DefenseY, false);
					for (FMenuPrimitive& Digit : *Targets[Stat])
					{
						Digit.Code = static_cast<uint8>(Digit.Code & ~MenuGpu::RawTexture);
						Digit.Corners[0].Color = Losses[Stat] ? LossTint : GainTint;
					}
				}
			}
			++Face;
		}
		ShowStoredCount(Kind, Id);
		return Price;
	}

	void FMenuShop::CompareEquipment(uint8 Id, EShopKind Kind, uint8 Character, std::array<int32, 2>& Changes, std::array<bool, 2>& Losses) const
	{
		const std::span<const uint8> Block = Services.Game->Block();
		const std::size_t Record = CharacterOffset(Character);
		const FEquipmentTables& Tables = Resources.Equipment();
		std::array<int16, 2> Now{};
		std::array<int16, 2> After{};
		if (Kind == EShopKind::Weapon)
		{
			const std::span<const uint8> Bought = Tables.Weapon(Id);
			if (Character == Billy)
			{
				// His guns summed; each of the bought weapon's type (but an Ether Pistol) replaced by it.
				for (std::size_t Gun = 0; Gun < 4; ++Gun)
				{
					const std::span<const uint8> Loaded = Tables.Weapon(Block[Record + GunSlots + Gun]);
					Now[0] = static_cast<int16>(Now[0] + Field(Loaded, 0xC));
					const uint8 Type = Field(Loaded, 6);
					After[0] = static_cast<int16>(After[0] + (Type == Field(Bought, 6) && Type != EtherPistol ? Field(Bought, 0xC) : Field(Loaded, 0xC)));
				}
			}
			else
			{
				Now[0] = static_cast<int16>(Field(Tables.Weapon(Block[Record + WeaponSlot]), 0xC) + Block[Record + BaseAttack]);
				After[0] = static_cast<int16>(Field(Bought, 0xC) + Block[Record + BaseAttack]);
			}
		}
		else if (Kind == EShopKind::Accessory)
		{
			// The accessories' armor: the one of the bought one's class replaced, or else the weakest (the last on ties).
			const std::span<const uint8> Bought = Tables.Accessory(Id);
			Now[1] = Block[Record + BaseDefense];
			After[1] = Block[Record + BaseDefense];
			bool bReplaced = false;
			for (std::size_t Slot = 0; Slot < 3; ++Slot)
			{
				const std::span<const uint8> Worn = Tables.Accessory(Block[Record + AccessorySlots + Slot]);
				Now[1] = static_cast<int16>(Now[1] + Field(Worn, 8));
				const uint16 Class = Word(Worn, 0xE);
				if (Class != 0 && Class == Word(Bought, 0xE))
				{
					After[1] = static_cast<int16>(After[1] + Field(Bought, 8));
					bReplaced = true;
				}
				else
				{
					After[1] = static_cast<int16>(After[1] + Field(Worn, 8));
				}
			}
			if (!bReplaced)
			{
				uint8 Weakest = 0xFF;
				std::size_t Replaced = 0;
				for (std::size_t Slot = 0; Slot < 3; ++Slot)
				{
					const uint8 Armor = Field(Tables.Accessory(Block[Record + AccessorySlots + Slot]), 8);
					if (Armor <= Weakest)
					{
						Weakest = Armor;
						Replaced = Slot;
					}
				}
				After[1] = static_cast<int16>(Block[Record + BaseDefense] + Field(Bought, 8));
				for (std::size_t Slot = 0; Slot < 3; ++Slot)
				{
					if (Slot != Replaced)
					{
						After[1] = static_cast<int16>(After[1] + Field(Tables.Accessory(Block[Record + AccessorySlots + Slot]), 8));
					}
				}
			}
		}
		for (std::size_t Stat = 0; Stat < 2; ++Stat)
		{
			Losses[Stat] = After[Stat] < Now[Stat];
			Changes[Stat] = Losses[Stat] ? Now[Stat] - After[Stat] : After[Stat] - Now[Stat];
		}
	}

	void FMenuShop::ShowStoredCount(EShopKind Kind, uint8 Id)
	{
		const int32 List = InventoryList(Kind);
		const std::span<uint8> Items = Services.Game->InventoryItems(List);
		const std::span<uint8> Counts = Services.Game->InventoryCounts(List);
		StoredCount = 0;
		for (std::size_t Slot = 0; Slot < Items.size(); ++Slot)
		{
			if (Items[Slot] == Id)
			{
				StoredCount = Counts[Slot];
				break;
			}
		}
		const std::array<uint16, 2> Glyphs = {static_cast<uint16>(StoredCount / 10 == 0 ? BlankGlyph : StoredCount / 10 + FirstDigitGlyph),
			static_cast<uint16>(StoredCount % 10 + FirstDigitGlyph)};
		GlyphText(Glyphs, GlyphScratch);
		std::fill(NameImage.Units.begin(), NameImage.Units.end(), uint16{0});
		Scene.Stored.Width = static_cast<uint8>(RenderString(NameTyping, NameImage, GlyphScratch, 1));
		NameImage.Rect = {StoredStripX, StoredStripY, StoredStripWidth, RowHeight};
		Upload(NameImage);
		SetStripLabelTexture(Scene.Stored, StoredRow, StripBase, StoredMode);
		SetLayerQuad(Scene.Stored.Quad.Points, StoredX, StoredY, Scene.Stored.Width, RowHeight);
		RecordLabel(Scene.Stored);
		Scene.bStoredShown = true;
	}

	void FMenuShop::CommitPurchase(uint32 Left)
	{
		PlaySound(Effect::Paid);
		Services.Game->SetGold(std::min(Left, MostGold));
		for (std::size_t Index = 0; Index < Stock.size(); ++Index)
		{
			if (Stock[Index] == 0 || Cart[Index] == 0)
			{
				continue;
			}
			const int32 List = InventoryList(StockKinds[Index]);
			const std::span<uint8> Items = Services.Game->InventoryItems(List);
			const std::span<uint8> Counts = Services.Game->InventoryCounts(List);
			// Every slot holding the goods gains the cart's quantity (at most 99); else the first empty slot takes it.
			bool bFound = false;
			for (std::size_t Slot = 0; Slot < Items.size(); ++Slot)
			{
				if (Items[Slot] == Stock[Index])
				{
					const uint8 Count = static_cast<uint8>(Counts[Slot] + Cart[Index]);
					Counts[Slot] = Count > MostHeld ? static_cast<uint8>(MostHeld) : Count;
					bFound = true;
				}
			}
			for (std::size_t Slot = 0; Slot < Items.size() && !bFound; ++Slot)
			{
				if (Items[Slot] == 0)
				{
					Items[Slot] = Stock[Index];
					Counts[Slot] = Cart[Index];
					bFound = true;
				}
			}
		}
	}
}
