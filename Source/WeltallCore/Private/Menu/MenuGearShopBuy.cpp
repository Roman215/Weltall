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

		constexpr int32 PiecesEntry = 4;
		constexpr int32 FullScale = 0x1000;
		constexpr int32 PageRows = 8;
		constexpr int32 LastRow = PageRows - 1;
		constexpr uint8 Usable = 0x80;
		/** A cart holds at most as many as bring the inventory's count to 99. */
		constexpr int32 MostHeld = 99;
		constexpr uint32 MostGold = 9999999;
		constexpr int32 ListLabelsBase = 2;
		/** The lists' headings ("Stored", "Equip"), at fixed places. */
		constexpr std::array<int16, 2> HeadingX = {0xEC, 0xCE};
		constexpr int16 HeadingTop = 0x7E;
		constexpr int16 HeadingBottom = 0x8B;
		/** The highlighted goods' line, the stored count and the fitted part's name, from their strips. */
		constexpr int32 DetailX = 0x2C;
		constexpr int32 DetailY = 0x12;
		constexpr int32 DetailV = 0x4E;
		constexpr int32 RowHeight = 13;
		constexpr int32 StripRow = 9;
		constexpr int32 StripBase = 0x80;
		constexpr uint8 StoredMode = 0x82;
		constexpr uint8 EquippedMode = 0x81;
		constexpr int16 StripX = 0x198;
		constexpr int16 StripY = 0xB4;
		constexpr int16 StripWidth = 0x28;
		constexpr int32 StoredX = 0xF8;
		constexpr int32 EquippedX = 0xD4;
		constexpr int32 LabelY = 0x8E;
		constexpr uint16 FirstDigitGlyph = 0x10;
		constexpr uint16 BlankGlyph = 0xC3;
		/** "E": the goods are fitted; the changes a purchase would make, under each face. */
		constexpr int32 FittedFrame = 0x0E;
		constexpr int32 FittedX = 0x0E;
		constexpr int32 FittedY = 0xB4;
		constexpr int32 ChangesX = 0x49;
		constexpr int32 ChangesStep = 0x1A;
		constexpr int32 AttackY = 0xBE;
		constexpr int32 DefenseY = 0xC6;
		constexpr FDrawColor GainTint{0x80, 0x40, 0x40};
		constexpr FDrawColor LossTint{0x40, 0x40, 0x80};
		constexpr int32 CartMark = 0xF1;
		constexpr int32 TotalX = 0x68;
		constexpr int32 TotalY = 0x54;
		constexpr FDrawColor LineWhite{0xFF, 0xFF, 0xFF};
		/** The gears' records: fitted frame, engine, armor, parts, weapon slots, Billy's guns. */
		constexpr std::size_t FrameSlot = 0x02;
		constexpr std::size_t EngineSlot = 0x03;
		constexpr std::size_t ArmorSlot = 0x08;
		constexpr std::size_t PartSlots = 0x09;
		constexpr std::size_t WeaponSlot = 0x0C;
		constexpr std::size_t GunSlots = 0x04;
		constexpr uint8 FirstGun = 0x32;
		/** A part's class and defense; a gear weapon's class. */
		constexpr std::size_t PartClass = 0x08;
		constexpr std::size_t PartDefense = 0x0D;
		constexpr std::size_t WeaponClass = 0x0F;
		constexpr int32 GearWeaponsList = 3;
		constexpr int32 GearPartsList = 4;

		namespace Effect
		{
			constexpr uint8 Confirm = 2;
			constexpr uint8 Buzzer = 4;
			constexpr uint8 Paid = 0xD1;
		}

		/** The dialogs: the purchase's total asked about, the cart given up, a tune-up (an upgrade, or not and warned first). */
		constexpr uint8 PurchaseText = 0x8F;
		constexpr uint8 CancelText = 0x8C;
		constexpr uint8 ChangeText = 0xA3;
		constexpr uint8 NotBetterText = 0xAF;
		constexpr uint8 ChangeAnywayText = 0xB2;

		uint8 Field(std::span<const uint8> Record, std::size_t At)
		{
			return At < Record.size() ? Record[At] : uint8{0};
		}

		uint16 Word(std::span<const uint8> Record, std::size_t At)
		{
			return At + 2 <= Record.size() ? ReadU16(Record, At) : uint16{0};
		}
	}

	TMenuTask<> FMenuGearShop::RunBuyList(bool bTuning)
	{
		const uint32 OnEntry = Services.Game->Gold();
		uint32 Left = OnEntry;
		uint32 Total = 0;
		uint32 TradeIn = 0;
		uint32 Price = 0;
		uint32 FittedHalf = 0;
		bool bFirst = true;
		bool bRedraws = true;
		bool bGoesOn = true;
		int32 Row = 0;
		int32 Top = 0;
		int32 DrawnRow = 0xFF;
		int32 DrawnTop = 0xFF;
		int16 FiveX = 0xE0;
		uint16 FiveWidth = 0x40;
		const std::size_t HeadingIndex = bTuning ? 1 : 0;
		std::array<uint8, PageRows> Usables{};

		// Every pilot's gear recomputed and its stats kept for the comparisons, by character.
		for (int32 Character = 0; Character < FGameState::CharacterCount; ++Character)
		{
			const uint8 Gear = GearOf(Character);
			if (Gear == FGameState::EmptySlot)
			{
				continue;
			}
			ApplyGear(Gear);
			const std::array<int16, 2> Stats = GearStats(Gear);
			ListedStats[0][static_cast<std::size_t>(Character)] = Stats[0];
			ListedStats[1][static_cast<std::size_t>(Character)] = Stats[1];
		}
		const std::size_t List = static_cast<std::size_t>((bTuning ? 0 : 3) + SubCommand) % Lists.size();
		Entries.fill(0);
		Cart.fill(0);
		for (std::size_t Entry = 0; Entry < Lists[List].size(); ++Entry)
		{
			Entries[Entry] = Lists[List][Entry];
			EntryKinds[Entry] = static_cast<EGearGoods>(List);
		}
		const int32 EntryCount = ListCounts[List];
		Arc.bDimmed = true;
		ListCursor.Cel = 4;
		ListCursor.Frames = 0;
		const auto Hide = [this, HeadingIndex](bool bShown)
		{
			if (!bShown)
			{
				Scene.bShown = false;
			}
			for (const int32 Window : {2, 3, 5})
			{
				Flags[static_cast<std::size_t>(MenuFlag::Windows + Window)] = bShown ? 1 : 0;
			}
			Flags[MenuFlag::ArcLabels + HeadingIndex] = bShown ? 1 : 0;
			bScrollThumbShown = bShown;
			bListCursorShown = bShown;
		};
		while (bGoesOn)
		{
			co_await Frame();
			if (Top != DrawnTop || bRedraws)
			{
				FittedHalf = RenderBuyRows(Top, bTuning ? OnEntry : Left, Usables);
				DrawScrollThumb(EntryCount, Top);
			}
			if (Row != DrawnRow || Top != DrawnTop)
			{
				Price = ShowBuyDetail(Top + Row);
				Scene.bShown = true;
				DrawnRow = Row;
				DrawnTop = Top;
				if (bTuning)
				{
					// A tune-up pays the price less half the fitted one's, traded in.
					FiveX = 0xC8;
					FiveWidth = 0x70;
					bRedraws = true;
					TradeIn = 0;
					Left = OnEntry;
					Total = 0;
					if (Usables[static_cast<std::size_t>(Row)] != 0)
					{
						Left = OnEntry - Price + FittedHalf;
						TradeIn = FittedHalf;
						Total = Price;
					}
				}
			}
			DrawListCursor(Row);
			if (bFirst)
			{
				OpenWindow(2, 0xC, 0x2A, static_cast<uint16>(0xC4 - (bTuning ? 0x14 : 0)), 0x74, false, true, PiecesEntry, true);
				OpenWindow(3, 0x20, 0xE, 0xFC, 0x14, false, true, PiecesEntry, false);
				OpenWindow(5, FiveX, 0x7A, FiveWidth, 0x24, false, true, PiecesEntry, false);
				RenderLabels(std::span<FMenuLabel>(ArcLabels).first(Tables.ListLabels.size()), Tables.ListLabels, ListLabelsBase);
				// Either heading is as wide as the first ("Stored"): Tune up's "Equip" shows the blank texels after it.
				FMenuLabel& Heading = ArcLabels[HeadingIndex];
				RecordLabel(Heading);
				const int16 X = HeadingX[HeadingIndex];
				// The game's program gives both headings the first's width; the French translation's each its own.
				const int16 Right = static_cast<int16>(X + ArcLabels[Resources.IsFrench() ? HeadingIndex : 0].Width);
				Heading.Quad.Corners[0].X = X;
				Heading.Quad.Corners[0].Y = HeadingTop;
				Heading.Quad.Corners[1].X = Right;
				Heading.Quad.Corners[1].Y = HeadingTop;
				Heading.Quad.Corners[2].X = X;
				Heading.Quad.Corners[2].Y = HeadingBottom;
				Heading.Quad.Corners[3].X = Right;
				Heading.Quad.Corners[3].Y = HeadingBottom;
				Flags[MenuFlag::ArcLabels + HeadingIndex] = 1;
				if (bTuning)
				{
					ShowEquippedName(static_cast<EGearGoods>(List));
				}
				co_await RevealFaces(Tables.FaceX);
				const FMenuPlacedFrames<4>& Captions = bTuning ? Tables.TuneStatics : Tables.BuyStatics;
				DrawStatics(Captions.Frames, Captions.X, Captions.Y);
				bFirst = false;
			}
			if (bRedraws)
			{
				DrawMoneyPanel(OnEntry, Total, Left, TradeIn, bTuning ? 1 : 0);
				bRedraws = false;
			}
			int32 Scrolled = Top;
			const std::size_t Index = static_cast<std::size_t>(Top + Row);
			switch (Code)
			{
			case EMenuCode::Right:
				if (Usables[static_cast<std::size_t>(Row)] != 0 && !bTuning && Cart[Index] + OwnedCount + 1 < MostHeld + 1)
				{
					Total += Price;
					Left -= Price;
					bRedraws = true;
					++Cart[Index];
				}
				break;
			case EMenuCode::Down:
				if (++Row > LastRow)
				{
					Row = LastRow;
					Scrolled = Top + 1 > EntryCount - PageRows ? Top : Top + 1;
				}
				break;
			case EMenuCode::Left:
				if (!bTuning && Cart[Index] != 0)
				{
					Total -= Price;
					Left += Price;
					bRedraws = true;
					--Cart[Index];
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
			{
				if (Total == 0)
				{
					PlaySound(Effect::Buzzer);
					break;
				}
				PlaySound(Effect::Confirm);
				Hide(false);
				bGoesOn = false;
				uint8 FirstText = PurchaseText;
				std::optional<uint8> SecondText;
				if (!bTuning)
				{
					DrawTotal(Total, TotalX, TotalY - (Resources.IsFrench() ? 1 : 0));
				}
				else if (IsUpgrade(static_cast<int32>(Index)))
				{
					FirstText = ChangeText;
				}
				else
				{
					FirstText = NotBetterText;
					SecondText = ChangeAnywayText;
				}
				OpenChoiceCursors();
				if (co_await Ask(FirstText, SecondText, true))
				{
					if (bTuning)
					{
						Cart[Index] = 1;
					}
					CommitPurchase(Left);
				}
				else
				{
					bGoesOn = true;
					Hide(true);
					DrawnTop = 0xFF;
					DrawnRow = 0xFF;
				}
				co_await CloseChoiceCursors();
				break;
			}
			case EMenuCode::Cancel:
				Flags[static_cast<std::size_t>(MenuFlag::Windows + 5)] = 0;
				bGoesOn = false;
				Flags[MenuFlag::ArcLabels + HeadingIndex] = 0;
				if (Total != 0 && !bTuning)
				{
					Hide(false);
					OpenChoiceCursors();
					bGoesOn = !co_await Ask(CancelText, std::nullopt, true);
					if (bGoesOn)
					{
						Hide(true);
						DrawnTop = 0xFF;
						DrawnRow = 0xFF;
					}
					co_await CloseChoiceCursors();
				}
				break;
			default:
				break;
			}
			Top = Scrolled;
		}
		Scene.bStoredShown = false;
		CloseWindow(5);
		ClearFlags(MenuFlag::ArcLabels, static_cast<int32>(Tables.ListLabels.size()));
		ClearListScreen(true);
	}

	uint32 FMenuGearShop::RenderBuyRows(int32 Top, uint32 Budget, std::array<uint8, 8>& Usables)
	{
		// The fitted armor's, frame's or engine's half price, for a tune-up's trade-in: the last row's kind's.
		uint32 FittedHalf = 0;
		const std::span<const uint8> Record = Services.Game->Block().subspan(GearOffset(CurrentGear), GearSize);
		std::fill(NameImage.Units.begin(), NameImage.Units.end(), uint16{0});
		for (int32 Row = 0; Row < PageRows; ++Row)
		{
			const std::size_t Index = static_cast<std::size_t>(Top + Row);
			FMenuShopRow& Shown = Scene.Rows[static_cast<std::size_t>(Row)];
			Usables[static_cast<std::size_t>(Row)] = 0;
			Shown.Cart.clear();
			const uint8 Id = Index < Entries.size() ? Entries[Index] : uint8{0};
			if (Id == 0)
			{
				Shown.bShown = false;
				continue;
			}
			const EGearGoods Kind = EntryKinds[Index];
			uint32 Users = ~0u;
			switch (Kind)
			{
			case EGearGoods::Armor:
				FittedHalf = GoodsPrice(Record[ArmorSlot], Kind) >> 1;
				Users = GoodsUsers(Id, Kind);
				break;
			case EGearGoods::Frame:
				FittedHalf = GoodsPrice(Record[FrameSlot], Kind) >> 1;
				Users = GoodsUsers(Id, Kind);
				break;
			case EGearGoods::Engine:
				FittedHalf = GoodsPrice(Record[EngineSlot], Kind) >> 1;
				Users = GoodsUsers(Id, Kind);
				break;
			default:
				break;
			}
			const uint32 Price = GoodsPrice(Id, Kind);
			const bool bFits = CurrentGear < 32 && (Users >> CurrentGear & 1) != 0;
			const uint8 Mode = static_cast<int64>(Price) <= static_cast<int64>(Budget) && bFits ? Usable : uint8{0};
			Usables[static_cast<std::size_t>(Row)] = Mode;
			DrawRow(Row, GoodsName(Id, Kind), Price, static_cast<uint8>(Mode + 1), static_cast<uint8>(Mode + 2));
			if (Cart[Index] != 0)
			{
				DrawCartMark(Row, CartMark, Cart[Index]);
			}
		}
		return FittedHalf;
	}

	uint32 FMenuGearShop::ShowBuyDetail(int32 Index)
	{
		const std::size_t At = static_cast<std::size_t>(Index);
		const uint8 Id = At < Entries.size() ? Entries[At] : uint8{0};
		const EGearGoods Kind = At < Entries.size() ? EntryKinds[At] : EGearGoods::Part;
		constexpr std::array<FMenuResources::EText, 5> Helps = {FMenuResources::EText::GearArmorHelp, FMenuResources::EText::GearFrameHelp,
			FMenuResources::EText::GearEngineHelp, FMenuResources::EText::GearShopPartHelp, FMenuResources::EText::GearShopWeaponHelp};
		std::fill(DetailImage.Units.begin(), DetailImage.Units.end(), uint16{0});
		Scene.Detail.Width = static_cast<uint8>(RenderString(DetailTyping, DetailImage, Resources.Text(Helps[static_cast<std::size_t>(Kind)]).String(Id), 0));
		const uint32 Price = GoodsPrice(Id, Kind);
		const uint32 Users = GoodsUsers(Id, Kind);
		const uint32 Fitted = FittedMask(Id, Kind);
		Upload(DetailImage);
		SetLabelTexture(Scene.Detail, 0, 0);
		RecordLabel(Scene.Detail);
		SetTexelRect(Scene.Detail.Quad, 0, DetailV, Scene.Detail.Width, RowHeight);
		SetLayerQuad(Scene.Detail.Quad.Points, DetailX, DetailY, Scene.Detail.Width, RowHeight);
		Scene.bDetailShown = Id != 0;

		// For each pilot: the outline when the goods fit the gear, "E" when fitted, a part's or weapon's changes.
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
			Scene.AttackChanges[Face].clear();
			Scene.DefenseChanges[Face].clear();
			if ((Kind == EGearGoods::Part || Kind == EGearGoods::Weapon) && Scene.Outlined[Face])
			{
				std::array<int32, 2> Changes{};
				std::array<bool, 2> Losses{};
				PreviewStats(Id, Kind, static_cast<uint8>(Character), Changes, Losses);
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
		ShowOwnedCount(Kind, Id);
		return Price;
	}

	void FMenuGearShop::ShowOwnedCount(EGearGoods Kind, uint8 Id)
	{
		// Only parts and gear weapons are kept in the inventory; a tune-up's list leaves the count as it was.
		if (Kind != EGearGoods::Part && Kind != EGearGoods::Weapon)
		{
			return;
		}
		const int32 List = Kind == EGearGoods::Part ? GearPartsList : GearWeaponsList;
		const std::span<uint8> Items = Services.Game->InventoryItems(List);
		const std::span<uint8> Counts = Services.Game->InventoryCounts(List);
		OwnedCount = 0;
		for (std::size_t Slot = 0; Slot < Items.size(); ++Slot)
		{
			if (Items[Slot] == Id)
			{
				OwnedCount = Counts[Slot];
				break;
			}
		}
		const std::array<uint16, 2> Glyphs = {static_cast<uint16>(OwnedCount / 10 == 0 ? BlankGlyph : OwnedCount / 10 + FirstDigitGlyph),
			static_cast<uint16>(OwnedCount % 10 + FirstDigitGlyph)};
		GlyphText(Glyphs, GlyphScratch);
		std::fill(NameImage.Units.begin(), NameImage.Units.end(), uint16{0});
		Scene.Stored.Width = static_cast<uint8>(RenderString(NameTyping, NameImage, GlyphScratch, 1));
		NameImage.Rect = {StripX, StripY, StripWidth, RowHeight};
		Upload(NameImage);
		SetStripLabelTexture(Scene.Stored, StripRow, StripBase, StoredMode);
		SetLayerQuad(Scene.Stored.Quad.Points, StoredX, LabelY, Scene.Stored.Width, RowHeight);
		RecordLabel(Scene.Stored);
		Scene.bStoredShown = true;
	}

	void FMenuGearShop::ShowEquippedName(EGearGoods Kind)
	{
		const std::span<const uint8> Record = Services.Game->Block().subspan(GearOffset(CurrentGear), GearSize);
		std::fill(NameImage.Units.begin(), NameImage.Units.end(), uint16{0});
		if (Kind == EGearGoods::Armor || Kind == EGearGoods::Frame || Kind == EGearGoods::Engine)
		{
			const std::size_t Slot = Kind == EGearGoods::Armor ? ArmorSlot : (Kind == EGearGoods::Frame ? FrameSlot : EngineSlot);
			Scene.Equipped.Width = static_cast<uint8>(RenderString(NameTyping, NameImage, GoodsName(Record[Slot], Kind), 0));
		}
		NameImage.Rect = {StripX, StripY, StripWidth, RowHeight};
		Upload(NameImage);
		SetStripLabelTexture(Scene.Equipped, StripRow, StripBase, EquippedMode);
		SetLayerQuad(Scene.Equipped.Quad.Points, EquippedX, LabelY, Scene.Equipped.Width, RowHeight);
		RecordLabel(Scene.Equipped);
		Scene.bEquippedShown = true;
	}

	void FMenuGearShop::PreviewStats(uint8 Id, EGearGoods Kind, uint8 Pilot, std::array<int32, 2>& Changes, std::array<bool, 2>& Losses)
	{
		// The goods fitted to the pilot's gear for a moment: a part in the slot of its class, or else over
		// the weakest (the last on ties); a weapon in its slot, a gun over each of its class.
		const uint8 Gear = GearOf(Pilot);
		const std::span<uint8> Record = Services.Game->Block().subspan(GearOffset(Gear), GearSize);
		std::array<uint8, 8> Kept{};
		std::copy_n(Record.begin() + static_cast<std::ptrdiff_t>(WeaponSlot), 1, Kept.begin());
		std::copy_n(Record.begin() + static_cast<std::ptrdiff_t>(GunSlots), 4, Kept.begin() + 1);
		std::copy_n(Record.begin() + static_cast<std::ptrdiff_t>(PartSlots), 3, Kept.begin() + 5);
		if (Kind == EGearGoods::Part)
		{
			const uint16 Class = Word(GoodsRecord(Id, Kind), PartClass);
			bool bReplaced = false;
			for (std::size_t Slot = 0; Slot < 3; ++Slot)
			{
				const uint16 Fitted = Word(GoodsRecord(Kept[5 + Slot], Kind), PartClass);
				if (Fitted != 0 && Fitted == Class)
				{
					bReplaced = true;
					Record[PartSlots + Slot] = Id;
				}
			}
			if (!bReplaced)
			{
				uint8 Weakest = 0xFF;
				std::size_t Replaced = 0;
				for (std::size_t Slot = 0; Slot < 3; ++Slot)
				{
					const uint8 Defense = Field(GoodsRecord(Kept[5 + Slot], Kind), PartDefense);
					if (Defense <= Weakest)
					{
						Weakest = Defense;
						Replaced = Slot;
					}
				}
				Record[PartSlots + Replaced] = Id;
			}
		}
		else if (Kind == EGearGoods::Weapon)
		{
			if (Id < FirstGun)
			{
				Record[WeaponSlot] = Id;
			}
			else
			{
				const uint8 Class = Field(GoodsRecord(Id, Kind), WeaponClass);
				for (std::size_t Gun = 0; Gun < 4; ++Gun)
				{
					if (Field(GoodsRecord(Record[GunSlots + Gun], Kind), WeaponClass) == Class)
					{
						Record[GunSlots + Gun] = Id;
					}
				}
			}
		}
		ApplyGear(Gear);
		const std::array<int16, 2> After = GearStats(Gear);
		for (std::size_t Stat = 0; Stat < 2; ++Stat)
		{
			const int32 Before = ListedStats[Stat][Pilot];
			Losses[Stat] = After[Stat] < Before;
			Changes[Stat] = Losses[Stat] ? Before - After[Stat] : After[Stat] - Before;
		}
		std::copy_n(Kept.begin(), 1, Record.begin() + static_cast<std::ptrdiff_t>(WeaponSlot));
		std::copy_n(Kept.begin() + 1, 4, Record.begin() + static_cast<std::ptrdiff_t>(GunSlots));
		std::copy_n(Kept.begin() + 5, 3, Record.begin() + static_cast<std::ptrdiff_t>(PartSlots));
		ApplyGear(Gear);
	}

	void FMenuGearShop::CommitPurchase(uint32 Left)
	{
		PlaySound(Effect::Paid);
		Services.Game->SetGold(std::min(Left, MostGold));
		const std::span<uint8> Record = Services.Game->Block().subspan(GearOffset(CurrentGear), GearSize);
		for (std::size_t Index = 0; Index < Entries.size(); ++Index)
		{
			const uint8 Id = Entries[Index];
			if (Id == 0 || Cart[Index] == 0)
			{
				continue;
			}
			switch (EntryKinds[Index])
			{
			case EGearGoods::Armor:
				Record[ArmorSlot] = Id;
				break;
			case EGearGoods::Frame:
				Record[FrameSlot] = Id;
				break;
			case EGearGoods::Engine:
				Record[EngineSlot] = Id;
				break;
			case EGearGoods::Part:
			case EGearGoods::Weapon:
			{
				// Every slot holding the goods gains the cart's quantity (at most 99); else the first empty slot takes it.
				const int32 List = EntryKinds[Index] == EGearGoods::Part ? GearPartsList : GearWeaponsList;
				const std::span<uint8> Items = Services.Game->InventoryItems(List);
				const std::span<uint8> Counts = Services.Game->InventoryCounts(List);
				bool bFound = false;
				for (std::size_t Slot = 0; Slot < Items.size(); ++Slot)
				{
					if (Items[Slot] == Id)
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
						Items[Slot] = Id;
						Counts[Slot] = Cart[Index];
						bFound = true;
					}
				}
				break;
			}
			}
		}
	}

	bool FMenuGearShop::IsUpgrade(int32 Index) const
	{
		const std::span<const uint8> Record = Services.Game->Block().subspan(GearOffset(CurrentGear), GearSize);
		const std::size_t At = static_cast<std::size_t>(Index);
		switch (EntryKinds[At])
		{
		case EGearGoods::Armor:
			return Record[ArmorSlot] < Entries[At];
		case EGearGoods::Frame:
			return Record[FrameSlot] < Entries[At];
		case EGearGoods::Engine:
			return Record[EngineSlot] < Entries[At];
		default:
			return true;
		}
	}

	void FMenuGearShop::DrawMoneyPanel(uint32 OnEntry, uint32 Total, uint32 Left, uint32 TradeIn, int32 Offset)
	{
		// Tune up's panel moves up and left to make room for the trade-in, under the total.
		const int32 LineY = Tables.GoldY[1] - (Offset * 8 - 9);
		Scene.GoldLine.Code = MenuGpu::FlatLine;
		Scene.GoldLine.LinePoints = 2;
		Scene.GoldLine.Corners[0] = {static_cast<int16>(Tables.GoldX[1] - (Offset * 0x10 + 8)), static_cast<int16>(LineY), 0, 0, LineWhite};
		Scene.GoldLine.Corners[1] = {static_cast<int16>(Tables.GoldX[1] - (Offset * 0x10 - 0x4E)), static_cast<int16>(LineY), 0, 0, {}};
		DrawDigitsInto(Scene.Gold[0], OnEntry, 0, 9, Tables.GoldX[0] - Offset * 0x10, Tables.GoldY[0] - Offset * 0x10, false);
		DrawDigitsInto(Scene.Gold[1], Total, 0, 9, Tables.GoldX[1] - Offset * 0x10, Tables.GoldY[1] - Offset * 0x10, false);
		DrawDigitsInto(Scene.Gold[2], Left, 0, 9, Tables.GoldX[2] - Offset * 0x10, Tables.GoldY[2] - Offset * 8, false);
		Scene.bTradeInShown = Offset != 0;
		Scene.TradeIn.clear();
		if (Offset != 0)
		{
			DrawDigitsInto(Scene.TradeIn, TradeIn, 0, 9, Tables.GoldX[1] - Offset * 0x10, Tables.GoldY[1] - Offset * 8, false);
		}
		Scene.bGoldShown = true;
	}
}
