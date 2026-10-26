#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Game/GearRecompute.h"
#include "Menu/MenuMain.h"
#include "Menu/MenuResources.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		using namespace GameStateLayout;
		using LittleEndian::ReadU16;
		using LittleEndian::ReadU32;

		constexpr uint8 ChuChu = 7;
		/** Chu-Chu's gear: her giant self, made from her own figures. */
		constexpr int32 ChuChuGear = 7;
		constexpr uint8 Buzzer = 4;
		/** The inner arc from the bottom: Equip, Abilities, Gear Option (2), Get ON-OFF. */
		constexpr int32 EquipCommand = 0;
		constexpr int32 AbilitiesCommand = 1;
		constexpr int32 RideCommand = 3;
		constexpr int32 InnerArc = 7;
		/** The labels: the walking set, then the riding set six on ("Get off" for "Get on"). */
		constexpr std::size_t WalkingLabels = 0x20;
		constexpr std::size_t RidingLabels = 0x26;
		constexpr std::size_t FirstHeading = 4;
		constexpr int32 LabelRows = 13;
		constexpr int32 ArcLabelX = 0x16;
		constexpr int32 ArcLabelY = -0x22;
		/** A gear's fields the sheet's stats come from. */
		namespace GearStat
		{
			constexpr std::size_t Output = 0x3C;
			constexpr std::size_t PartsDefense = 0x40;
			constexpr std::size_t PartsEtherDefense = 0x42;
			constexpr std::size_t Weapon = 0x12;
			constexpr std::size_t SecondGun = 0x22;
			constexpr std::size_t PartsHit = 0x4D;
			constexpr std::size_t OverweightPenalty = 0x4A;
			constexpr std::size_t PartsEtherAmplifier = 0x54;
			constexpr std::size_t MultiplierBonus = 0x56;
			constexpr std::size_t Defense = 0x70;
			constexpr std::size_t EtherDefense = 0x72;
			constexpr std::size_t BaseMultiplier = 0x74;
			constexpr std::size_t Agility = 0x98;
			constexpr std::size_t EtherAmplifier = 0x9E;
			constexpr std::size_t Hit = 0x9F;
		}
		/** The twin records the Menu keeps in step: a gear's parts copied to the gear it becomes. */
		struct FTwin
		{
			int32 Gear;
			int32 Twin;
		};
		constexpr std::array<FTwin, 7> Twins = {{{0, 1}, {1, 15}, {2, 10}, {3, 11}, {4, 12}, {5, 13}, {6, 14}}};
		/** Crescens, which the adult Emeralda pilots once the story's flag says so. */
		constexpr int32 Crescens = 9;
		constexpr uint8 AdultEmeralda = 10;
		constexpr uint16 AdultEmeraldaFlag = 0x1000;
		constexpr std::array<int32, 2> GunGears = {5, 13};
	}

	int32 FMenuMain::OpenGear()
	{
		StatusSheet = {};
		StatBars = {};
		EquipNames = {};
		EquipNameShown = {};
		int32 Slot = 0;
		while (Flags[static_cast<std::size_t>(MenuFlag::PartyHasGear + Slot)] == 0)
		{
			++Slot;
		}
		DrawMemberHints(0, true);
		return Slot;
	}

	void FMenuMain::SetGearLabels(bool bRiding)
	{
		RenderLabels(GearLabels, std::span<const uint8>(Resources.Main().LabelSets).subspan(bRiding ? RidingLabels : WalkingLabels, GearLabels.size()), 4);
		GearLabelOnLayer = {};
	}

	TMenuTask<bool> FMenuMain::GearScreen()
	{
		bool bGoesOn = true;
		bool bFirst = true;
		int32 DrawnSlot = 0xF3;
		SubCommand = 0;
		DrawnSubCommand = 0xFF;
		int32 Slot = OpenGear();
		bool bRiding = false;
		while (true)
		{
			co_await Frame();
			if (Slot != DrawnSlot)
			{
				RecomputeGear(Slot);
				co_await DrawStatusSheet(Slot, true);
				bRiding = Services.Game->PartySlotInGear(GameSlot(Slot)) != 0;
				SetGearLabels(bRiding);
				// The headings lie where the status sheet's first two do.
				const FMenuMainTables& Tables = Resources.Main();
				for (std::size_t Heading = 0; Heading < 2; ++Heading)
				{
					FMenuLabel& Placed = GearLabels[FirstHeading + Heading];
					SetLayerQuad(Placed.Quad.Points, Tables.SheetHeadingX[Heading], Tables.SheetHeadingY[Heading], Placed.Width, LabelRows);
					RecordLabel(Placed);
					Flags[MenuFlag::GearLabels + FirstHeading + Heading] = 1;
					GearLabelOnLayer[FirstHeading + Heading] = true;
				}
				DrawnSlot = Slot;
				if (bFirst)
				{
					bFirst = false;
					OpenLayer();
					co_await SlideParty(false, false);
					co_await RevealSubCommands(0);
				}
			}
			if (SubCommand != DrawnSubCommand)
			{
				// The selected inner command's label beside its orb: placed, never drawn.
				ClearFlags(MenuFlag::GearLabels, 4);
				const FMenuMainTables& Tables = Resources.Main();
				FMenuLabel& Placed = GearLabels[static_cast<std::size_t>(SubCommand)];
				const std::size_t Place = static_cast<std::size_t>(InnerArc + SubCommand);
				const int16 Left = static_cast<int16>(Tables.ArcX[Place] + Tables.GearArcLabelOffsets[static_cast<std::size_t>((bRiding ? 6 : 0) + SubCommand)] + ArcLabelX);
				const int16 Top = static_cast<int16>(Tables.ArcY[Place] + ArcLabelY);
				RecordLabel(Placed);
				Placed.Quad.Corners[0].X = Left;
				Placed.Quad.Corners[0].Y = Top;
				Placed.Quad.Corners[1].X = static_cast<int16>(Left + Placed.Width);
				Placed.Quad.Corners[1].Y = Top;
				Placed.Quad.Corners[2].X = Left;
				Placed.Quad.Corners[2].Y = static_cast<int16>(Top + LabelRows);
				Placed.Quad.Corners[3].X = static_cast<int16>(Left + Placed.Width);
				Placed.Quad.Corners[3].Y = static_cast<int16>(Top + LabelRows);
				Flags[static_cast<std::size_t>(MenuFlag::GearLabels + SubCommand)] = 1;
				DrawSubCommands(0);
				DrawnSubCommand = SubCommand;
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
			{
				HideSelection();
				ClearFlags(MenuFlag::GearLabels, 4);
				Flags[MenuFlag::StatusSheet] = 0;
				Flags[MenuFlag::Stats] = 0;
				Flags[MenuFlag::EquipNames] = 0;
				Flags[MenuFlag::GearLabels + 4] = 0;
				Flags[MenuFlag::GearLabels + 5] = 0;
				// Chu-Chu's gear is herself: neither equipped, nor given abilities or options here.
				const bool bChuChu = Member(Slot) == ChuChu;
				if (SubCommand == RideCommand)
				{
					if (Request.bGearBoardingLocked)
					{
						PlaySound(Buzzer);
					}
					else if (Services.Game->PartySlotInGear(GameSlot(Slot)) == 0)
					{
						if (Services.Game->CharacterGear(Member(Slot)).value_or(FGameState::EmptySlot) != FGameState::EmptySlot)
						{
							bRiding = true;
							Services.Game->SetPartySlotInGear(GameSlot(Slot), 1);
						}
						DrawRideBadge(Slot, true);
					}
					else
					{
						Services.Game->SetPartySlotInGear(GameSlot(Slot), 0);
						bRiding = false;
						DrawRideBadge(Slot, true);
					}
				}
				else if (bChuChu)
				{
					PlaySound(Buzzer);
				}
				else if (SubCommand == EquipCommand)
				{
					Flags[MenuFlag::SubCommands] = 0;
					co_await Equip(Slot, false, true);
					co_await CloseEquip();
					DrawStats(false, false, true);
					DrawEquipNames(Slot, 0, false, true);
					Flags[MenuFlag::SubCommands] = 1;
				}
				else
				{
					co_await AbilitiesScreen(Slot, false, SubCommand == AbilitiesCommand ? 1 : 2);
					co_await CloseAbilities();
				}
				DrawnSlot = 0xFF;
				Flags[MenuFlag::GearLabels + 4] = 1;
				Flags[MenuFlag::GearLabels + 5] = 1;
				SetGearLabels(bRiding);
				Flags[MenuFlag::StatusSheet] = 1;
				Flags[MenuFlag::Stats] = 1;
				Flags[MenuFlag::EquipNames] = 1;
				DrawSelection(SubCommand + InnerArc);
				Flags[MenuFlag::Pointer] = 1;
				DrawnSubCommand = 0xFF;
				DrawMemberHints(0, true);
				break;
			}
			case EMenuCode::Cancel:
				bGoesOn = false;
				break;
			case EMenuCode::R1:
			case EMenuCode::L1:
				Slot = NextMember(Slot, Code == EMenuCode::L1, true);
				DrawnSubCommand = 0xFF;
				break;
			default:
				break;
			}
			if (!bGoesOn)
			{
				ClearFlags(MenuFlag::GearLabels, static_cast<int32>(GearLabels.size()));
				Flags[MenuFlag::Pointer] = 0;
				Flags[MenuFlag::Selection] = 0;
				co_return true;
			}
		}
	}

	void FMenuMain::RecomputeGear(int32 Slot)
	{
		FGameState& Game = *Services.Game;
		const std::span<uint8> Block = Game.Block();
		const int32 Gear = Block[CharacterOffset(Member(Slot)) + Character::Gear];
		const std::span<const uint8> Weapons = Resources.GearWeapons();
		static_cast<void>(ApplyGearParts(Game, Gear, GearParts));
		static_cast<void>(ApplyGearWeapon(Game, Gear, Weapons));
		// The gear's twin record takes its parts (and for 12 and 13, its weapons) and is recomputed too.
		for (const FTwin& Pair : Twins)
		{
			if (Pair.Gear != Gear)
			{
				continue;
			}
			for (std::size_t Part = 0; Part < 3; ++Part)
			{
				Block[GearOffset(static_cast<std::size_t>(Pair.Twin)) + 9 + Part] = Block[GearOffset(static_cast<std::size_t>(Gear)) + 9 + Part];
			}
			static_cast<void>(ApplyGearParts(Game, Pair.Twin, GearParts));
			if (Gear == 4 || Gear == 5)
			{
				const std::size_t From = GearOffset(static_cast<std::size_t>(Gear));
				const std::size_t To = GearOffset(static_cast<std::size_t>(Pair.Twin));
				Block[To + 0x0C] = Block[From + 0x0C];
				if (Gear == 5)
				{
					for (const std::size_t Field : {std::size_t{0x0F}, std::size_t{0x04}, std::size_t{0x07}})
					{
						Block[To + Field] = Block[From + Field];
					}
				}
				static_cast<void>(ApplyGearWeapon(Game, Pair.Twin, Weapons));
			}
		}

		// The stats the sheet shows, after Chu-Chu's gear is made from her own figures.
		if ((ReadU16(Block, StoryFlagsOffset) & AdultEmeraldaFlag) != 0)
		{
			GearParts.SetPilot(Crescens, AdultEmeralda);
		}
		const std::size_t Record = GearOffset(static_cast<std::size_t>(Gear));
		if (Gear == ChuChuGear)
		{
			const std::size_t Her = CharacterOffset(ChuChu);
			const auto WriteU32 = [&Block](std::size_t At, uint32 Value)
			{
				for (std::size_t Byte = 0; Byte < 4; ++Byte)
				{
					Block[At + Byte] = static_cast<uint8>(Value >> (8 * Byte));
				}
			};
			const auto WriteU16 = [&Block](std::size_t At, uint16 Value)
			{
				Block[At] = static_cast<uint8>(Value);
				Block[At + 1] = static_cast<uint8>(Value >> 8);
			};
			WriteU32(Record + GameStateLayout::Gear::Hp, static_cast<uint32>(ReadU16(Block, Her + Character::Hp)) * 50);
			WriteU32(Record + GameStateLayout::Gear::MaxHp, static_cast<uint32>(ReadU16(Block, Her + Character::MaxHp)) * 50);
			Block[Record + GearStat::Output] = static_cast<uint8>(Block[Her + 0x58] + Block[Her + 0x28]);
			WriteU16(Record + GearStat::Defense, static_cast<uint16>((Block[Her + 0x59] + Block[Her + 0x29]) * 12));
			WriteU16(Record + GearStat::EtherDefense, static_cast<uint16>((Block[Her + 0x5C] + Block[Her + 0x2C]) * 6));
			Block[Record + GearStat::Agility] = static_cast<uint8>(Block[Her + 0x5A] + Block[Her + 0x2A]);
		}
		const std::size_t Pilot = CharacterOffset(GearParts.Pilot(Gear));
		const bool bGuns = std::find(GunGears.begin(), GunGears.end(), Gear) != GunGears.end();
		const uint16 WeaponAttack =
			bGuns ? static_cast<uint16>((Block[Record + GearStat::Weapon] + Block[Record + GearStat::SecondGun]) * 6 / 10) : Block[Record + GearStat::Weapon];
		const uint16 Attack = static_cast<uint16>(
			WeaponAttack + Block[Record + GearStat::Output] * (Block[Record + GearStat::BaseMultiplier] + Block[Record + GearStat::MultiplierBonus]));
		ShownStats[0] = Attack;
		ShownStats[1] = static_cast<uint16>(ReadU16(Block, Record + GearStat::Defense) + ReadU16(Block, Record + GearStat::PartsDefense));
		ShownStats[2] = static_cast<uint16>(Block[Pilot + 0x5C] + Block[Pilot + 0x2C] + ReadU16(Block, Record + GearStat::PartsEtherDefense)
			+ ReadU16(Block, Record + GearStat::EtherDefense));
		ShownStats[3] = static_cast<uint8>(Block[Record + GearStat::Hit] + Block[Record + GearStat::PartsHit]);
		ShownStats[4] = static_cast<uint8>(Block[Record + GearStat::Agility] - Block[Record + GearStat::OverweightPenalty]);
		ShownStats[5] = static_cast<uint8>(Block[Record + GearStat::EtherAmplifier] + Block[Record + GearStat::PartsEtherAmplifier]);
	}
}
