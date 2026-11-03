#include "Battle/BattleContext.h"
#include "Battle/BattleGameState.h"
#include "Battle/BattleProgram.h"
#include "Battle/Show/BattleHudDigits.h"
#include "Battle/Show/BattleHudGpu.h"
#include "Battle/Show/BattleHudLists.h"
#include "Battle/Show/BattleRegurus.h"
#include "Battle/Show/BattleShowHud.h"
#include "Text/MesRender.h"

namespace Weltall
{
	using namespace BattleHudDigits;
	using namespace BattleHudGpu;
	using namespace BattleHudLists;

	namespace
	{
		constexpr uint8 ChuChu = 7;
		constexpr uint8 InfinityLevel = 4;
		/** The buttons' fuel costs in a slot's command pictures: three places a button from 10h, as the costs' digit frames. */
		constexpr std::size_t CostPictures = 0x10;
		constexpr std::size_t CostPicturePlaces = 4;
		constexpr int32 CostButtons = 3;
		constexpr uint8 CostDigitFrame = 0x83;
		constexpr uint8 NoPicture = 0xFF;

		/** The cockpit's own frames. */
		constexpr int32 CockpitFrame = 0xA8;
		constexpr int32 BearingFrames = 0xA9;
		constexpr int32 ReticleFrame = 0xB9;
		constexpr int32 SightFrame = 0x82;
		constexpr int32 ElevationFrames = 0xC9;
		constexpr int32 FlickerFrames = 0xBA;
		constexpr int32 LampFrames = 0xC4;
		constexpr int32 BoosterOffFrame = 0xA0;
		constexpr int32 BoosterOnFrame = 0xA1;
		constexpr int32 InfinityFrame = 0xA2;
		constexpr int32 LevelFrames = 0xA3;
		constexpr int32 LevelShadeFrames = 0xDB;
		constexpr int32 CockpitDigitFrames = 0x92;
		constexpr int32 FuelSlashFrame = 0x9C;
		constexpr int32 PercentFrame = 0x9D;
		/** The cockpit's centre, where its whole-screen frames are drawn from. */
		constexpr int16 CentreX = 0xA0;
		constexpr int16 CentreY = 100;
		/** The figures beside the frame: a column of closed-up digits 6 pixels apart. */
		constexpr int16 FiguresX = 0x11A;
		constexpr int16 FigureStep = 6;
		constexpr int16 AttackY = 0x46;
		constexpr int16 ChanceY = 0x4E;
		constexpr int16 ArmorBreakY = 0x56;
		constexpr int16 AgilityY = 0x5E;
		constexpr int32 Lamps = 5;
		constexpr int16 LampX = 0xE0;
		constexpr int16 LampY = 0x6E;
		constexpr int16 LampStep = 10;
		/** The lamps are drawn while their count is below 15 and it starts again past 20. */
		constexpr int16 LampShownTicks = 15;
		constexpr int16 LampCycleTicks = 20;
		/** The fuel below the frame: four digits at their places, then the most, closed up, after its slash. */
		constexpr int16 FuelX = 0x20;
		constexpr int16 MaxFuelX = 0x48;
		constexpr int16 FuelSlashX = 0x41;
		constexpr int16 FuelY = 0xCC;
		constexpr int16 FuelStep = 8;
		/** The flickering digits: two rows of ten above and below the frame. */
		constexpr int32 FlickerColumns = 10;
		constexpr int16 FlickerX = 0x82;
		constexpr int16 FlickerY = 10;
		constexpr int16 FlickerRowStep = 0xBD;

		/** The gear's attack buttons as the hints draw them (Weak, Strong, Fierce), and the next presses' clusters. */
		constexpr std::array<int32, 3> GearButtonFrames{0x5E, 0x5F, 0x5D};
		constexpr int32 WeakHintFrame = 8;
		constexpr int32 StrongHintFrame = 9;
		constexpr int32 FierceHintFrame = 7;
		constexpr uint8 NoButton = 0xFF;
		constexpr int16 GearHintsX = 0x80;
		constexpr int16 GearHintsY = 0xD0;
		constexpr int16 HintStep = 0x10;
		/** The Infinity attacks' place in the gears' attack commands, one a button. */
		constexpr uint32 InfinityAttacks = 12;
		/** The kernel's tables of the gears' attack names, one a gear. */
		constexpr int32 GearAttackNameTables = 28;
		/** The hints' names (two planes an image at (3C0h + 1Eh per image, 1Ah)) and their costs' digits (at (3DEh + 8 per hint, 0)). */
		constexpr int16 HintTextX = 0x3C0;
		constexpr int16 HintTextY = 0x1A;
		constexpr uint8 HintTextU = 0x78;
		constexpr int16 HintCostX = 0x3DE;
		constexpr int16 HintCostStep = 8;
		constexpr std::size_t WeakButton = 0;
		constexpr std::size_t StrongButton = 1;
		constexpr std::size_t FierceButton = 2;
	}

	void FBattleHud::OpenCockpit(uint8 Slot)
	{
		if (Slot >= PartySlots)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		std::array<uint8, 0x1C>& Pictures = MenuIcons[Slot];
		for (int32 Button = 0; Button < CostButtons; ++Button)
		{
			// The cost's low byte, as up to three digits from the left.
			uint32 Cost = Battle->GearTurn.Costs[static_cast<std::size_t>(Button)] & 0xFF;
			const std::size_t First = CostPictures + CostPicturePlaces * static_cast<std::size_t>(Button);
			Pictures[First] = NoPicture;
			Pictures[First + 1] = NoPicture;
			Pictures[First + 2] = NoPicture;
			std::size_t Place = 0;
			if (Cost / 100 != 0)
			{
				Pictures[First] = static_cast<uint8>(Cost / 100 + CostDigitFrame);
				Cost %= 100;
				Place = 1;
			}
			if (Cost / 10 != 0 || Place != 0)
			{
				Pictures[First + Place] = static_cast<uint8>(Cost / 10 + CostDigitFrame);
				Cost %= 10;
				++Place;
			}
			Pictures[First + Place] = static_cast<uint8>(Cost + CostDigitFrame);
		}
		MakeCommandHud();
		if (Battle->PartyCharacters[Slot] != ChuChu)
		{
			DrawGearHud();
		}
	}

	int32 FBattleHud::DrawCockpitSprite(FBattleHudCockpit::ERun Run, int32 Frame, int32 X, int32 Y, bool bAppend)
	{
		// A run drawn past its room would be linked into the next one's, whose parts the game then links a second
		// time: an ordering table that never ends. The sheet's drawing stops there.
		FBattleHudCockpit::FRun& Parts = Cockpit.Runs[Run];
		const int32 Drawn = DrawSheet(Frame, Cockpit.Room(Run), Parts.Count, X, Y, FullScale);
		Parts.Count = bAppend ? Parts.Count + Drawn : Drawn;
		return Drawn;
	}

	int32 FBattleHud::DrawCockpitDigits(FBattleHudCockpit::ERun Run, uint32 Value, std::size_t First, int16 X, int16 Y, int16 Step, bool bCloseUp)
	{
		const std::array<uint8, 9> Digits = ToDigits(Value);
		int32 Shown = 0;
		for (std::size_t Place = First; Place < Digits.size(); ++Place)
		{
			if (Digits[Place] != Blank)
			{
				DrawCockpitSprite(Run, CockpitDigitFrames + Digits[Place], X, Y, true);
				++Shown;
			}
			if (Digits[Place] != Blank || !bCloseUp)
			{
				X = static_cast<int16>(X + Step);
			}
		}
		return Shown;
	}

	void FBattleHud::BlendCockpitRun(FBattleHudCockpit::ERun Run, bool bSubtract)
	{
		const FBattleHudCockpit::FRun& Parts = Cockpit.Runs[Run];
		const std::span<FBattleHudPair> Room = Cockpit.Room(Run);
		for (int32 Part = 0; Part < Parts.Count && FaultState == EBattleHudFault::None; ++Part)
		{
			FMenuPrimitive& Quad = Room[static_cast<std::size_t>(Part)][Parts.Buffer & 1];
			SetSemiTrans(Quad, true);
			SetShadeTex(Quad, false);
			SetColor(Quad, 0, FullLevel, FullLevel, FullLevel);
			Quad.Page = static_cast<uint16>(Quad.Page | (bSubtract ? PageSubtract : PageAdd));
		}
	}

	void FBattleHud::DrawGearHud()
	{
		using ERun = FBattleHudCockpit::ERun;
		const FBattleGearTurn& Turn = Battle->GearTurn;
		const auto& Glass = Resources.Tables.CockpitGlass;
		auto& Runs = Cockpit.Runs;

		// The frame between the two layers of its glass.
		DrawCockpitSprite(ERun::Glass, Glass[0], CentreX, CentreY, true);
		DrawCockpitSprite(ERun::Glass, Glass[1], CentreX, CentreY, true);
		Runs[ERun::Glass].Buffer = Buffer;
		DrawCockpitSprite(ERun::Frame, CockpitFrame, CentreX, CentreY, false);
		Runs[ERun::Frame].Buffer = Buffer;
		BlendCockpitRun(ERun::Glass, false);
		BlendCockpitRun(ERun::Frame, false);
		DrawCockpitSprite(ERun::Shade, Glass[2], CentreX, CentreY, true);
		DrawCockpitSprite(ERun::Shade, Glass[3], CentreX, CentreY, true);
		Runs[ERun::Shade].Buffer = Buffer;
		BlendCockpitRun(ERun::Shade, true);

		// The warning lamps that are lit, one below the other.
		Runs[ERun::Lamps].Count = 0;
		int16 LampAt = LampY;
		for (int32 Lamp = 0; Lamp < Lamps; ++Lamp)
		{
			if ((Turn.Lamps & (0x8000u >> Lamp)) != 0)
			{
				DrawCockpitSprite(ERun::Lamps, LampFrames + Lamp, LampX, LampAt, true);
				LampAt = static_cast<int16>(LampAt + LampStep);
			}
		}
		Runs[ERun::Lamps].Buffer = Buffer;
		Cockpit.LampTicks = 0;

		Runs[ERun::Booster].Count = 0;
		DrawCockpitSprite(ERun::Booster, Turn.bBooster ? BoosterOnFrame : BoosterOffFrame, CentreX, CentreY, false);
		Runs[ERun::Booster].Buffer = Buffer;
		BlendCockpitRun(ERun::Booster, false);

		Runs[ERun::Level].Count = 0;
		DrawCockpitSprite(ERun::Level, (Turn.Level + LevelFrames) & 0xFF, CentreX, CentreY, false);
		Runs[ERun::Level].Buffer = Buffer;
		Runs[ERun::LevelShade].Count = 0;
		DrawCockpitSprite(ERun::LevelShade, (Turn.Level + LevelShadeFrames) & 0xFF, CentreX, CentreY, false);
		Runs[ERun::LevelShade].Buffer = Buffer;
		BlendCockpitRun(ERun::Level, false);
		BlendCockpitRun(ERun::LevelShade, true);

		// The figures: the attack, Infinity's chance (its sign once in Infinity), the armour broken, the agility.
		DrawCockpitDigits(ERun::Attack, Turn.ShownAttack, 4, FiguresX, AttackY, FigureStep, true);
		Runs[ERun::Attack].Buffer = Buffer;
		BlendCockpitRun(ERun::Attack, false);
		if (Turn.Level < InfinityLevel)
		{
			const int32 Shown = DrawCockpitDigits(ERun::InfinityChance, Turn.InfinityChance, 7, FiguresX, ChanceY, FigureStep, true);
			DrawCockpitSprite(ERun::InfinityChance, PercentFrame, FiguresX + FigureStep * Shown, ChanceY, true);
		}
		else
		{
			Runs[ERun::InfinityChance].Count = 0;
			DrawCockpitSprite(ERun::InfinityChance, InfinityFrame, FiguresX, ChanceY, false);
		}
		Runs[ERun::InfinityChance].Buffer = Buffer;
		BlendCockpitRun(ERun::InfinityChance, false);
		const int32 Shown = DrawCockpitDigits(ERun::ArmorBreak, Turn.ArmorBreak, 6, FiguresX, ArmorBreakY, FigureStep, true);
		DrawCockpitSprite(ERun::ArmorBreak, PercentFrame, FiguresX + FigureStep * Shown, ArmorBreakY, true);
		Runs[ERun::ArmorBreak].Buffer = Buffer;
		BlendCockpitRun(ERun::ArmorBreak, false);
		DrawCockpitDigits(ERun::Agility, Turn.Agility, 7, FiguresX, AgilityY, FigureStep, true);
		Runs[ERun::Agility].Buffer = Buffer;
		BlendCockpitRun(ERun::Agility, false);
	}

	void FBattleHud::DrawCockpitFuel(uint8 Slot)
	{
		using ERun = FBattleHudCockpit::ERun;
		if (!bCommandHudMade)
		{
			FaultState = EBattleHudFault::CommandHudNotMade;
			return;
		}
		if (Slot >= PartySlots)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		const FBattleGearPart& Gear = Battle->Combatants[Slot].GearPart;
		Cockpit.Runs[ERun::Fuel].Count = 0;
		DrawCockpitDigits(ERun::Fuel, Gear.Fuel, 5, FuelX, FuelY, FuelStep, false);
		Cockpit.Runs[ERun::Fuel].Buffer = Buffer;
		Cockpit.Runs[ERun::MaxFuel].Count = 0;
		DrawCockpitDigits(ERun::MaxFuel, Gear.MaxFuel, 5, MaxFuelX, FuelY, FuelStep, true);
		DrawCockpitSprite(ERun::MaxFuel, FuelSlashFrame, FuelSlashX, FuelY, true);
		Cockpit.Runs[ERun::MaxFuel].Buffer = Buffer;
	}

	void FBattleHud::DrawReticle()
	{
		using ERun = FBattleHudCockpit::ERun;
		if (!bCockpitShown)
		{
			return;
		}
		if (Battle->Random == nullptr)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		const bool bFlickers = Reticle.Frame(*Battle->Random);
		auto& Runs = Cockpit.Runs;
		Runs[ERun::Reticle].Count = 0;
		DrawCockpitSprite(ERun::Reticle, ReticleFrame, Reticle.X, Reticle.Y, false);
		Runs[ERun::Reticle].Buffer = Buffer;
		Runs[ERun::Bearing].Count = 0;
		DrawCockpitSprite(ERun::Bearing, (Reticle.X & 0xF) + BearingFrames, CentreX, CentreY, false);
		Runs[ERun::Bearing].Buffer = Buffer;
		Runs[ERun::Sight].Count = 0;
		DrawCockpitSprite(ERun::Sight, SightFrame, Reticle.X, Reticle.Y, false);
		Runs[ERun::Sight].Buffer = Buffer;
		BlendCockpitRun(ERun::Reticle, false);
		BlendCockpitRun(ERun::Sight, false);
		BlendCockpitRun(ERun::Bearing, false);
		Runs[ERun::Elevation].Count = 0;
		DrawCockpitSprite(ERun::Elevation, (Reticle.Y & 0xF) + ElevationFrames, Reticle.X, Reticle.Y, false);
		Runs[ERun::Elevation].Buffer = Buffer;
		BlendCockpitRun(ERun::Elevation, true);
		if (!bFlickers)
		{
			return;
		}
		// Each digit takes its own part of the run.
		const std::span<FBattleHudPair> Flicker = Cockpit.Room(ERun::Digits);
		for (int32 Digit = 0; Digit < FBattleCockpitReticle::DigitCount; ++Digit)
		{
			DrawSheet(FlickerFrames + Reticle.Digits[static_cast<std::size_t>(Digit)], Flicker, Digit,
				FlickerX + FigureStep * (Digit % FlickerColumns), FlickerY + FlickerRowStep * (Digit / FlickerColumns), FullScale);
			FMenuPrimitive& Quad = Flicker[static_cast<std::size_t>(Digit)][Buffer];
			SetSemiTrans(Quad, true);
			SetShadeTex(Quad, false);
			SetColor(Quad, 0, FullLevel, FullLevel, FullLevel);
			Quad.Page = static_cast<uint16>(Quad.Page | PageAdd);
		}
		Runs[ERun::Digits].Buffer = Buffer;
	}

	void FBattleHud::LinkCockpitRun(FBattleHudCockpit::ERun Run, uint32 Key)
	{
		LinkPairs(Cockpit.Room(Run), Cockpit.Runs[Run].Count, Cockpit.Runs[Run].Buffer, Key);
	}

	void FBattleHud::LinkCockpit()
	{
		using ERun = FBattleHudCockpit::ERun;
		if (bCockpitLevelShown)
		{
			LinkCockpitRun(ERun::Level);
			LinkCockpitRun(ERun::Fuel);
			LinkCockpitRun(ERun::MaxFuel);
			LinkCockpitRun(ERun::LevelShade);
		}
		if (!bCockpitShown)
		{
			return;
		}
		LinkPairs(Cockpit.Room(ERun::Digits), FBattleCockpitReticle::DigitCount, Cockpit.Runs[ERun::Digits].Buffer);
		LinkCockpitRun(ERun::Frame);
		LinkCockpitRun(ERun::Glass);
		++Cockpit.LampTicks;
		if (Cockpit.LampTicks < LampShownTicks)
		{
			LinkCockpitRun(ERun::Lamps);
		}
		else if (Cockpit.LampTicks > LampCycleTicks)
		{
			Cockpit.LampTicks = 0;
		}
		for (const ERun Run : {ERun::Booster, ERun::Unused, ERun::Attack, ERun::InfinityChance, ERun::ArmorBreak, ERun::Agility, ERun::Shade, ERun::Elevation,
				 ERun::Bearing})
		{
			LinkCockpitRun(Run);
		}
		LinkCockpitRun(ERun::Reticle, BattleHudBlendKeys::Reticle(0));
		LinkCockpitRun(ERun::Sight, BattleHudBlendKeys::Sight(0));
	}

	bool FBattleHud::GearAttackLearned(uint8 Slot, uint32 Attack)
	{
		const std::size_t Offset = BattleGameState::SkillsOffset(Battle->PartyCharacters[Slot], GameStateLayout::Skills::GearDeathblows);
		if (Battle->GameState == nullptr || Offset + 2 > FGameState::BlockSize || Attack >= FBattleProgramTables::GearAttackCount)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return false;
		}
		const uint8 Bit = Battle->Program->GearAttacks[Attack];
		return Bit < 16 && (BattleGameState::Word(*Battle->GameState, Offset) & (0x8000u >> Bit)) != 0;
	}

	void FBattleHud::DrawGearHints(uint8 Slot, uint8 Button)
	{
		if (!bCommandHudMade)
		{
			FaultState = EBattleHudFault::CommandHudNotMade;
			return;
		}
		const FBattlePartyInput& Party = Battle->Party;
		if (Slot >= PartySlots || Party.ButtonCount > Party.Buttons.size())
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		const uint8 Level = Battle->GearTurn.Level;
		const auto& Disabled = Party.Disabled[Slot];
		Hints.NameCount = 0;
		Hints.SpriteCount = 0;
		Hints.CostCount = 0;
		const int32 Pressed = Party.ButtonCount;
		for (int32 Press = 0; Press < Pressed; ++Press)
		{
			const uint8 Taken = Party.Buttons[static_cast<std::size_t>(Press)];
			if (Taken == NoButton)
			{
				continue;
			}
			// Any other value draws the frame a register last held: the turn never records one.
			if (Taken >= GearButtonFrames.size())
			{
				FaultState = EBattleHudFault::IndexOutsideHud;
				return;
			}
			Hints.SpriteCount += DrawSheet(GearButtonFrames[Taken], Hints.Sprites, Hints.SpriteCount, GearHintsX + HintStep * Press, GearHintsY, FullScale);
		}
		// The attack this press makes: the button's own row, or with a press before it the follow-up of the first.
		uint32 Attack = (Button + 1u) * 3u;
		if (Party.Deathblow != 0)
		{
			Attack = Button + (Party.Buttons[0] + 1u) * 3u;
		}
		if (Level == InfinityLevel)
		{
			Attack = Button == NoButton ? InfinityAttacks : Button + InfinityAttacks;
		}
		Attack &= 0xFF;
		std::array<FVramImage, 2> Images{BlankImage(NarrowUnits), BlankImage(NarrowUnits)};
		int32 Named = 0;
		bool bFollowUp = false;
		if (GearAttackLearned(Slot, Attack))
		{
			bFollowUp = true;
			if (Party.Deathblow != 0 || Disabled[WeakButton] == 0)
			{
				if (Party.Deathblow == 0)
				{
					Hints.SpriteCount += DrawSheet(WeakHintFrame, Hints.Sprites, Hints.SpriteCount, GearHintsX + HintStep * Pressed, GearHintsY, FullScale);
				}
				Named = DrawGearHintName(Slot, 0, Pressed - 1, Attack, Images);
			}
		}
		if (FaultState == EBattleHudFault::None && (Party.Deathblow == 0 || !bFollowUp))
		{
			const uint32 Strong = Level == InfinityLevel ? InfinityAttacks + 1 : ((Button + 1u) * 3u + 1) & 0xFF;
			if (GearAttackLearned(Slot, Strong) && Disabled[StrongButton] == 0)
			{
				Hints.SpriteCount += DrawSheet(StrongHintFrame, Hints.Sprites, Hints.SpriteCount, GearHintsX + 4 + HintStep * Pressed, GearHintsY - HintStep * Named, FullScale);
				Named = DrawGearHintName(Slot, Named, Pressed, Strong, Images);
			}
			const uint32 Fierce = Level == InfinityLevel ? InfinityAttacks + 2 : ((Button + 1u) * 3u + 2) & 0xFF;
			if (FaultState == EBattleHudFault::None && GearAttackLearned(Slot, Fierce) && Disabled[FierceButton] == 0)
			{
				Hints.SpriteCount += DrawSheet(FierceHintFrame, Hints.Sprites, Hints.SpriteCount, GearHintsX + 8 + HintStep * Pressed, GearHintsY - HintStep * Named, FullScale);
				DrawGearHintName(Slot, Named, Pressed, Fierce, Images);
			}
		}
		Hints.NameBuffer = Buffer;
		Hints.SpriteBuffer = Buffer;
		Hints.CostBuffer = Buffer;
		bHintsShown = true;
	}

	int32 FBattleHud::DrawGearHintName(uint8 Slot, int32 Hint, int32 Row, uint32 Attack, std::span<FVramImage> Images)
	{
		if (FaultState != EBattleHudFault::None || Hint >= FBattleHudHints::NameParts || Attack >= FBattleProgramTables::GearAttackCount)
		{
			FaultState = FaultState == EBattleHudFault::None ? EBattleHudFault::PartsOutsidePrimitives : FaultState;
			return Hint + 1;
		}
		const std::size_t At = static_cast<std::size_t>(Hint);
		const uint8 Plane = static_cast<uint8>(Hint % 2);
		const auto InitQuads = [this](FBattleHudPair& Pair, bool bOdd) {
			for (FMenuPrimitive& Quad : Pair)
			{
				BattleHudGpu::Set(Quad, TexturedQuad);
				SetColor(Quad, 0, FullLevel, FullLevel, FullLevel);
				SetSemiTrans(Quad, false);
				SetShadeTex(Quad, true);
				Quad.Page = MenuGpu::TexturePage(0, 0, HintTextX, 0);
				Quad.Clut = bOdd ? OddTextClut : EvenTextClut;
			}
		};
		InitQuads(Hints.Names[At], Plane != 0);
		const uint8 Performing = Battle->PartyLooks().PerformingGear(Battle->Combatants[Slot].Gear);
		const uint8 Naming = Settings.bRegurusNamedAsVierge ? BattleRegurus::NamingGear(Performing) : Performing;
		const std::span<const uint8> Name = TableString(GearAttackNameTables + Naming, Battle->Program->GearAttacks[Attack]);
		if (FaultState != EBattleHudFault::None)
		{
			return Hint + 1;
		}
		FVramImage& Image = Images[static_cast<std::size_t>(Hint / 2)];
		const int32 Width = RenderMesString(Text, HintTyping, Name, Image, Plane);
		Image.Rect.X = static_cast<int16>(HintTextX + Hint / 2 * NarrowUnits);
		Image.Rect.Y = HintTextY;
		Upload(Image);
		const int32 Across = Hint * 4 + (Row + 1) * HintStep;
		const int16 Y = static_cast<int16>((200 - HintStep * Hint) & 0xFFF8);
		SetTextQuad(Hints.Names[At][Buffer], static_cast<int16>((Across + 0x86) & 0xFFFE), Y, static_cast<uint8>(Hint / 2 * HintTextU & 0xF8), HintTextY,
			static_cast<uint8>(Width));

		// The attack's fuel cost: its digits typed beside one another, closed up.
		InitQuads(Hints.Costs[At], false);
		const std::array<uint8, 9> Digits = ToDigits(Battle->GearTurn.Costs[Attack]);
		int32 Typed = 0;
		for (std::size_t Place = 5; Place < Digits.size(); ++Place)
		{
			if (Digits[Place] == Blank)
			{
				continue;
			}
			FVramImage Digit = DigitImages[Digits[Place]];
			Digit.Rect.X = static_cast<int16>(HintCostX + HintCostStep * Hint + 2 * Typed);
			Digit.Rect.Y = 0;
			Upload(Digit);
			++Typed;
		}
		SetTextQuad(Hints.Costs[At][Buffer], static_cast<int16>((Across + 0xEA) & 0xFFFE), Y, static_cast<uint8>((Hint * 0x20 + HintTextU) & 0xF8), 0,
			static_cast<uint8>((Typed & 0x1F) << 3));
		++Hints.NameCount;
		++Hints.CostCount;
		return Hint + 1;
	}
}
