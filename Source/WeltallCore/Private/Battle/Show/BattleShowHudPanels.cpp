#include "Battle/BattleContext.h"
#include "Battle/Show/BattleHudDigits.h"
#include "Battle/Show/BattleHudGpu.h"
#include "Battle/Show/BattleShowHud.h"

namespace Weltall
{
	using namespace BattleHudDigits;
	using namespace BattleHudGpu;

	namespace
	{
		constexpr int16 PanelWidth = 0x60;
		/** The digit places' table: HP from 0, the slash, the maximum from 4, a gear's HP from 7. */
		constexpr std::size_t SlashPlace = 3;
		constexpr std::size_t MaxHpPlace = 4;
		constexpr std::size_t GearHpPlace = 7;
		constexpr int16 HpY = 0x10;
		constexpr int32 HpDigitFrame = 0x67;
		constexpr int32 SlashFrame = 0x71;
		constexpr int32 ApDigitFrame = 0x83;
		constexpr int16 ApDigitX = 0x4A;
		constexpr int16 ApDigitStep = 6;
		constexpr int16 ApY = 0x25;
		constexpr int32 FullScale = 0x1000;
		constexpr int32 HalfScale = 0x800;
		constexpr int32 IconFrame = 0x52;
		constexpr int32 IconShadowFrame = 0x53;
		constexpr int32 IconBackFrame = 0x8F;
		/** The frame whose texture and CLUT the gauges take. */
		constexpr int32 GaugeFrame = 0x5C;
		constexpr int32 ApLabelFrame = 0x90;
		constexpr int32 FuelLabelFrame = 0x91;
		/** The gauge's length for a full gauge: 5600 / 10000 of 100 (56 pixels). */
		constexpr int32 GaugeScale = 0x15E0;
		constexpr int32 GaugeDivisor = 10000;
		constexpr uint8 NoMember = 0x7F;
		constexpr uint8 ChuChu = 7;
		/** The turn-start icon's flight: to the command window's corner, the icon's angle while it rests there. */
		constexpr int32 IconTargetX = 0x10;
		constexpr int32 IconTargetY = 0x98;
		constexpr int32 IconRestAngle = 0xC00;
		constexpr uint16 IconStartScale = 0x800;
		constexpr uint16 IconScaleStep = 0x66;
		constexpr uint16 IconAngleStep = 0x80;
		/** The icon's rotated corners lie 4096 in front of an eye as far behind the screen. */
		constexpr int16 RotatedDepth = 0x1000;
		constexpr uint16 RotatedScreen = 0x1000;
		constexpr uint8 IconArrived = 1;
		constexpr uint8 IconFlying = 2;
		constexpr uint8 IconStarting = 3;
		/** "AP left / AP at the start" beside the AP bar: digit frames from 0Fh, the slash's. */
		constexpr int32 ApNumberFrame = 0xF;
		constexpr int32 ApSlashFrame = 0x19;
		constexpr int16 ApNumbersY = 0xD0;

		/** libgte's RotMatrixZ: Matrix turned about z from the left, each entry's sum shifted down 12 and kept to 16 bits. */
		FGteMatrix RotateZ(const FGteMatrix& Matrix, int16 Angle)
		{
			const int32 Turn = Angle < 0 ? -Angle & 0xFFF : Angle & 0xFFF;
			const int32 Sin = Angle < 0 ? -GteSin(Turn) : GteSin(Turn);
			const int32 Cos = GteCos(Turn);
			FGteMatrix Turned = Matrix;
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				const int32 Top = Matrix.M[0][Column];
				const int32 Middle = Matrix.M[1][Column];
				Turned.M[0][Column] = static_cast<int16>((Cos * Top - Sin * Middle) >> 12);
				Turned.M[1][Column] = static_cast<int16>((Sin * Top + Cos * Middle) >> 12);
			}
			return Turned;
		}

		/** Leading zeros blank, but for the last digit. */
		template <std::size_t Count>
		void BlankLeading(std::array<uint8, Count>& Digits)
		{
			for (std::size_t Index = 0; Index + 1 < Count && Digits[Index] == 0; ++Index)
			{
				Digits[Index] = Blank;
			}
		}
	}

	struct FBattleHud::FPanelDigits
	{
		/** 0 above a quarter of the maximum, 1 down to an eighth, 2 below. */
		uint8 Colour = 0;
		std::array<uint8, 3> Hp{};
		std::array<uint8, 3> Max{};
		std::array<uint8, 5> Gear{};
	};

	int16 FBattleHud::PanelX(int32 Slot) const
	{
		return static_cast<int16>(Slot * PanelWidth + PanelOffset(Slot));
	}

	int16 FBattleHud::PanelOffset(int32 Slot) const
	{
		const std::size_t Index = static_cast<std::size_t>(Layout) * 3 + static_cast<std::size_t>(Slot);
		return Index < Resources.Tables.PanelOffsets.size() ? Resources.Tables.PanelOffsets[Index] : 0;
	}

	bool FBattleHud::InGearPanel(int32 Slot) const
	{
		return Battle->ApRecords[static_cast<std::size_t>(Slot)].InGear != 0 && Battle->PartyCharacters[static_cast<std::size_t>(Slot)] != ChuChu;
	}

	void FBattleHud::SetDimSub(FMenuPrimitive& Part)
	{
		SetSemiTrans(Part, true);
		SetShadeTex(Part, false);
		Part.Corners[0].Color = {0x40, 0x40, 0x40};
		Part.Page = static_cast<uint16>(Part.Page | PageSubtract);
	}

	void FBattleHud::InitPanelPrims()
	{
		for (uint8& Refresh : Battle->PanelRefresh)
		{
			Refresh = 1;
		}
		GaugeTexture = TextureOf(GaugeFrame);
		PlainClut = MenuGpu::Clut(GaugeTexture.ClutX, GaugeTexture.ClutY);
		NormalClut = MenuGpu::Clut(GaugeTexture.ClutX, GaugeTexture.ClutY - 1);
		FastClut = MenuGpu::Clut(GaugeTexture.ClutX, GaugeTexture.ClutY - 2);
		SlowClut = MenuGpu::Clut(GaugeTexture.ClutX, GaugeTexture.ClutY - 3);
		const uint16 Page = MenuGpu::TexturePage(GaugeTexture.Depth, 0, GaugeTexture.VramX, GaugeTexture.VramY);
		const auto MakeGauge = [Page](FMenuPrimitive& Gauge) {
			BattleHudGpu::Set(Gauge, GouraudTexturedQuad);
			SetShadeTex(Gauge, false);
			SetColor(Gauge, 0, 0x80, 0x80, 0x80);
			SetColor(Gauge, 1, 0x80, 0x80, 0x80);
			SetColor(Gauge, 2, 0, 0, 0);
			SetColor(Gauge, 3, 0, 0, 0);
			Gauge.Page = Page;
		};
		for (FBattleHudPanel& Panel : Panels)
		{
			for (FMenuPrimitive& Gauge : Panel.Gauge)
			{
				MakeGauge(Gauge);
			}
			for (FMenuPrimitive& Bar : Panel.Bar)
			{
				BattleHudGpu::Set(Bar, GouraudQuad);
				SetColor(Bar, 2, 0x4F, 0x4F, 0x4F);
				SetColor(Bar, 3, 0x4F, 0x4F, 0x4F);
			}
		}
		for (FMenuPrimitive& Gauge : ApBar)
		{
			MakeGauge(Gauge);
		}
		for (FBattleHudPair& Tick : ApTicks)
		{
			for (FMenuPrimitive& Line : Tick)
			{
				BattleHudGpu::Set(Line, FlatLine);
				SetColor(Line, 0, 0xFF, 0xFF, 0xFF);
			}
		}
	}

	void FBattleHud::InitPanelSprites()
	{
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			if (Battle->Slots[static_cast<std::size_t>(Slot)].Occupant == NoMember)
			{
				continue;
			}
			FBattleHudPanel& Panel = Panels[static_cast<std::size_t>(Slot)];
			const int16 X = PanelX(Slot);
			Panel.RestingIconCount += DrawSheet(IconFrame, Panel.RestingIcon, Panel.RestingIconCount, X + 0x44, 0x24, HalfScale);
			const int32 Shadow = Panel.RestingIconCount;
			Panel.RestingIconCount += DrawSheet(IconShadowFrame, Panel.RestingIcon, Panel.RestingIconCount, X + 0x44, 0x24, HalfScale);
			for (int32 Part = Shadow; Part < Panel.RestingIconCount && Part < FBattleHudPanel::RestingIconParts; ++Part)
			{
				SetDimSub(Panel.RestingIcon[static_cast<std::size_t>(Part)][Buffer]);
			}
			DrawSheet(FBattleHudResources::PortraitFrame + Slot, std::span<FBattleHudPair>(&Panel.Portrait, 1), 0, X + 0x1C, 0x14, FullScale);
			Panel.ApLabelCount = DrawSheet(ApLabelFrame, Panel.ApLabel, 0, X + 0x38, 0x27, FullScale);
			Panel.FuelLabelCount = DrawSheet(FuelLabelFrame, Panel.FuelLabel, 0, X + 0x3C, 0x27, FullScale);
			Panel.LabelBuffer = Buffer;
		}
		RestingIconBuffer = Buffer;
		PortraitBuffer = Buffer;
	}

	void FBattleHud::InitFlash()
	{
		for (int32 Pair = 0; Pair < 2; ++Pair)
		{
			FMenuPrimitive& Quad = Flash[static_cast<std::size_t>(Pair)];
			BattleHudGpu::Set(Quad, FlatQuad);
			SetColor(Quad, 0, 0xFF, 0xFF, 0xFF);
			SetSemiTrans(Quad, true);
			SetDrawMode(FlashMode[static_cast<std::size_t>(Pair)], MenuGpu::TexturePage(0, 2, GaugeTexture.VramX, GaugeTexture.VramY), {0, 0, 0x100, 0x100});
		}
		bFlashOn = false;
		FlashGlow = 0xFF;
		bFlashFalling = false;
	}

	void FBattleHud::UpdatePartyPanels()
	{
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			if (Battle->PanelRefresh[static_cast<std::size_t>(Slot)] == 0)
			{
				continue;
			}
			FBattleHudPanel& Panel = Panels[static_cast<std::size_t>(Slot)];
			Panel.HpCount = 0;
			Panel.ApCount = 0;
			if (Battle->Slots[static_cast<std::size_t>(Slot)].Occupant == NoMember)
			{
				continue;
			}
			DrawApOrFuel(Slot);
			const FPanelDigits Digits = FormatPartyHp(Slot);
			if (Battle->ApRecords[static_cast<std::size_t>(Slot)].InGear == 0)
			{
				DrawPartyHp(Slot, Digits);
			}
			else
			{
				DrawPartyGearHp(Slot, Digits);
			}
			Panel.HpBuffer = Buffer;
			Battle->PanelRefresh[static_cast<std::size_t>(Slot)] = 0;
		}
	}

	void FBattleHud::DrawApOrFuel(int32 Slot)
	{
		FBattleHudPanel& Panel = Panels[static_cast<std::size_t>(Slot)];
		const uint16 Value = Panel.Mode == 1 ? Battle->ApRecords[static_cast<std::size_t>(Slot)].StoredAp
			: Battle->Combatants[static_cast<std::size_t>(Slot)].GearPart.Fuel;
		const std::array<uint8, 9> Digits = ToDigits(static_cast<uint32>(static_cast<int32>(static_cast<int16>(Value))));
		for (int32 Place = 0; Place < 4; ++Place)
		{
			const uint8 Digit = Digits[static_cast<std::size_t>(5 + Place)];
			if (Digit != Blank)
			{
				Panel.ApCount += DrawSheet(ApDigitFrame + Digit, Panel.Ap, Panel.ApCount, PanelX(Slot) + ApDigitX + ApDigitStep * Place, ApY, FullScale);
			}
		}
		Panel.ApBuffer = Buffer;
	}

	FBattleHud::FPanelDigits FBattleHud::FormatPartyHp(int32 Slot) const
	{
		const FBattleCombatant& Member = Battle->Combatants[static_cast<std::size_t>(Slot)];
		FPanelDigits Digits;
		// The colour by the HP on foot, by the gear's in a gear: 16-bit and 32-bit, as signed.
		const bool bInGear = Battle->ApRecords[static_cast<std::size_t>(Slot)].InGear != 0;
		const int32 Max = bInGear ? static_cast<int32>(Member.GearPart.MaxHp) : static_cast<int16>(Member.MaxHp);
		const int32 Current = bInGear ? static_cast<int32>(Member.GearPart.Hp) : static_cast<int16>(Member.Hp);
		Digits.Colour = Max / 8 < Current ? (Max / 4 < Current ? 0 : 1) : 2;
		const int32 Hp = static_cast<int16>(Member.Hp);
		const int32 MaxHp = static_cast<int16>(Member.MaxHp);
		Digits.Hp = {static_cast<uint8>(Hp / 100), static_cast<uint8>(Hp % 100 / 10), static_cast<uint8>(Hp % 10)};
		Digits.Max = {static_cast<uint8>(MaxHp / 100), static_cast<uint8>(MaxHp % 100 / 10), static_cast<uint8>(MaxHp % 10)};
		const int32 Gear = static_cast<int32>(Member.GearPart.Hp);
		Digits.Gear = {static_cast<uint8>(Gear / 10000), static_cast<uint8>(Gear % 10000 / 1000), static_cast<uint8>(Gear % 10000 % 1000 / 100),
			static_cast<uint8>(Gear % 10000 % 1000 % 100 / 10), static_cast<uint8>(Gear % 10000 % 1000 % 100 % 10)};
		BlankLeading(Digits.Hp);
		BlankLeading(Digits.Max);
		BlankLeading(Digits.Gear);
		return Digits;
	}

	void FBattleHud::DrawPartyHp(int32 Slot, const FPanelDigits& Digits)
	{
		FBattleHudPanel& Panel = Panels[static_cast<std::size_t>(Slot)];
		const std::array<int16, 24>& Places = Resources.Tables.DigitPlaces[static_cast<std::size_t>(Slot)];
		const int32 From = Panel.HpCount;
		const int16 Offset = PanelOffset(Slot);
		for (std::size_t Place = 0; Place < Digits.Hp.size(); ++Place)
		{
			if (Digits.Hp[Place] != Blank)
			{
				Panel.HpCount += DrawSheet(HpDigitFrame + Digits.Hp[Place], Panel.Hp, Panel.HpCount, Places[Place] + Offset, HpY, FullScale);
			}
		}
		Panel.HpCount += DrawSheet(SlashFrame, Panel.Hp, Panel.HpCount, Places[SlashPlace] + Offset, HpY, FullScale);
		// The maximum's digits close up to the left: each drawn one takes the next place.
		std::size_t Place = MaxHpPlace;
		for (const uint8 Digit : Digits.Max)
		{
			if (Digit != Blank)
			{
				Panel.HpCount += DrawSheet(HpDigitFrame + Digit, Panel.Hp, Panel.HpCount, Places[Place] + Offset, HpY, FullScale);
				++Place;
			}
		}
		if (Digits.Colour != 0)
		{
			TintDigits(Panel, From, Panel.HpCount, Digits.Colour);
		}
	}

	void FBattleHud::DrawPartyGearHp(int32 Slot, const FPanelDigits& Digits)
	{
		FBattleHudPanel& Panel = Panels[static_cast<std::size_t>(Slot)];
		const std::array<int16, 24>& Places = Resources.Tables.DigitPlaces[static_cast<std::size_t>(Slot)];
		const int32 From = Panel.HpCount;
		for (std::size_t Place = 0; Place < Digits.Gear.size(); ++Place)
		{
			if (Digits.Gear[Place] != Blank)
			{
				Panel.HpCount += DrawSheet(HpDigitFrame + Digits.Gear[Place], Panel.Hp, Panel.HpCount, Places[GearHpPlace + Place] + PanelOffset(Slot), HpY,
					FullScale);
			}
		}
		if (Digits.Colour != 0)
		{
			TintDigits(Panel, From, Panel.HpCount, Digits.Colour);
		}
	}

	void FBattleHud::TintDigits(FBattleHudPanel& Panel, int32 From, int32 To, uint8 Colour)
	{
		for (int32 Part = From; Part < To && Part < FBattleHudPanel::HpParts; ++Part)
		{
			FMenuPrimitive& Digit = Panel.Hp[static_cast<std::size_t>(Part)][Buffer];
			SetShadeTex(Digit, false);
			Digit.Corners[0].Color = {0x80, static_cast<uint8>(Colour == 1 ? 0x80 : 0), 0};
		}
	}

	void FBattleHud::BuildFuelBars()
	{
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			FBattleHudPanel& Panel = Panels[static_cast<std::size_t>(Slot)];
			FMenuPrimitive& Bar = Panel.Bar[Buffer];
			const int16 Left = static_cast<int16>(PanelX(Slot) + 0x28);
			if (Panel.Mode == 1)
			{
				const int16 Right = static_cast<int16>(Left + Battle->ApRecords[static_cast<std::size_t>(Slot)].StoredAp * 2);
				SetCorners(Bar, Left, 0x22, Right, 0x22, Left, 0x26, Right, 0x26);
				SetColor(Bar, 0, 0, 0xFF, 0);
				SetColor(Bar, 1, 0, 0xFF, 0);
			}
			else if (Panel.Mode == 2)
			{
				const FBattleGearPart& Gear = Battle->Combatants[static_cast<std::size_t>(Slot)].GearPart;
				const uint32 Fuel = Gear.Fuel;
				const uint32 Max = Gear.MaxFuel;
				const int16 Length = static_cast<int16>(Fuel * 100 / Max * GaugeScale / GaugeDivisor);
				const int16 Right = static_cast<int16>(Left + Length);
				SetCorners(Bar, Left, 0x22, Right, 0x22, Left, 0x26, Right, 0x26);
				if (Fuel >= Max >> 2)
				{
					SetColor(Bar, 0, 0, 0, 0xFF);
					SetColor(Bar, 1, 0, 0, 0xFF);
				}
				else if (Fuel >= Max >> 3)
				{
					SetColor(Bar, 0, 0xFF, 0xFF, 0);
					SetColor(Bar, 1, 0xFF, 0xFF, 0);
				}
				else
				{
					SetColor(Bar, 0, 0xFF, 0x7F, 0x7F);
					SetColor(Bar, 1, 0xFF, 0x7F, 0x7F);
				}
			}
		}
	}

	void FBattleHud::BuildWaitGauges()
	{
		BuildFuelBars();
		bApBarShown = false;
		const uint8 U = static_cast<uint8>((GaugeTexture.VramX & 0x3F) << 1);
		const uint8 V = static_cast<uint8>(GaugeTexture.VramY);
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			FBattleHudPanel& Panel = Panels[static_cast<std::size_t>(Slot)];
			if (Battle->Slots[static_cast<std::size_t>(Slot)].Occupant == NoMember)
			{
				Panel.bGaugeShown = false;
				continue;
			}
			const int32 Wait = static_cast<int16>(Battle->Waits[static_cast<std::size_t>(Slot)]);
			if (Battle->Party.TurnIcons[static_cast<std::size_t>(Slot)] == 0)
			{
				const int32 Full = static_cast<int16>(Battle->FullWaits[static_cast<std::size_t>(Slot)]);
				const int32 Length = (100 - Wait * 100 / Full) * GaugeScale / GaugeDivisor;
				const int16 Left = static_cast<int16>(PanelX(Slot) + 0x2D);
				const int16 Right = static_cast<int16>(Left + Length);
				FMenuPrimitive& Gauge = Panel.Gauge[Buffer];
				SetCorners(Gauge, Left, 0x1A, Right, 0x1A, Left, 0x1E, Right, 0x1E);
				const uint8 URight = static_cast<uint8>((Length + (GaugeTexture.VramX & 0x3F)) * 2);
				SetTexels(Gauge, U, V, URight, V, U, V + 4, URight, V + 4);
				const FBattleCombatant& Member = Battle->Combatants[static_cast<std::size_t>(Slot)];
				uint16 Speeds = Member.Buffs | Member.PermanentBuffs;
				// Perfect Works' rebalance: a boarded gear's booster too, which its permanent buffs keep.
				if (Battle->bPerfectWorksRebalance && Member.IsInGear())
				{
					Speeds |= Member.GearPart.PermanentBuffs;
				}
				if ((Speeds & 0x8000) != 0)
				{
					Gauge.Clut = FastClut;
				}
				else
				{
					Gauge.Clut = (Member.Status & 0x1000) != 0 ? SlowClut : NormalClut;
				}
				Panel.bGaugeShown = true;
			}
			else
			{
				int32 Length = (100 - Wait) * GaugeScale / GaugeDivisor;
				if (InGearPanel(Slot))
				{
					const FBattleGearPart& Gear = Battle->Combatants[static_cast<std::size_t>(Slot)].GearPart;
					Length = static_cast<int32>(static_cast<uint32>(Gear.Fuel) * 100 / Gear.MaxFuel * GaugeScale) / GaugeDivisor;
				}
				const int16 Top = static_cast<int16>(0xCE - 2 * Length);
				FMenuPrimitive& Gauge = ApBar[Buffer];
				SetCorners(Gauge, 0xC, 0xCE, 0xC, Top, 0x14, 0xCE, 0x14, Top);
				const uint8 URight = static_cast<uint8>((Length + (GaugeTexture.VramX & 0x3F)) * 2);
				SetTexels(Gauge, U, V, URight, V, U, V + 4, URight, V + 4);
				Gauge.Clut = PlainClut;
				if (Battle->Party.bIconsShown)
				{
					bApBarShown = true;
				}
			}
		}
	}

	void FBattleHud::DrawTurnIcons()
	{
		Flight.Count = 0;
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			if (Battle->Slots[static_cast<std::size_t>(Slot)].Occupant == NoMember)
			{
				continue;
			}
			FBattleHudPanel& Panel = Panels[static_cast<std::size_t>(Slot)];
			uint8& Icon = Battle->Party.TurnIcons[static_cast<std::size_t>(Slot)];
			const int16 X = PanelX(Slot);
			if (Icon == IconArrived)
			{
				// At rest over the command window, dimmed.
				Flight.Count = DrawSheetRotated(IconFrame, Flight.Icon, 0, IconTargetX, IconTargetY, FullScale, IconRestAngle);
				const int32 Shadow = Flight.Count;
				Flight.Count += DrawSheetRotated(IconShadowFrame, Flight.Icon, Flight.Count, IconTargetX, IconTargetY, FullScale, IconRestAngle);
				for (int32 Part = Shadow; Part < Flight.Count && Part < FBattleHudFlight::Parts; ++Part)
				{
					SetDimSub(Flight.Icon[static_cast<std::size_t>(Part)][Buffer]);
				}
				Flight.Buffer = Buffer;
			}
			else if (Icon == 0)
			{
				Panel.IconCount = 0;
				Panel.IconCount += DrawSheet(IconBackFrame, Panel.Icon, Panel.IconCount, X + 0x44, 0x1F, FullScale);
				Panel.IconCount += DrawSheet(IconFrame, Panel.Icon, Panel.IconCount, X + 0x48, 0x1C, HalfScale);
				const int32 Shadow = Panel.IconCount;
				Panel.IconCount += DrawSheet(IconShadowFrame, Panel.Icon, Panel.IconCount, X + 0x48, 0x1C, HalfScale);
				for (int32 Part = Shadow; Part < Panel.IconCount && Part < FBattleHudPanel::IconParts; ++Part)
				{
					SetDimSub(Panel.Icon[static_cast<std::size_t>(Part)][Buffer]);
				}
				Panel.IconBuffer = Buffer;
				Panel.bIconShown = true;
			}
			else if (Icon == IconFlying || Icon == IconStarting)
			{
				if (Icon == IconStarting)
				{
					if (!InGearPanel(Slot))
					{
						Panel.IconCount = 0;
					}
					else
					{
						Panel.bIconShown = false;
					}
					StartTurnIcon(Slot);
				}
				MoveTurnIcon(Slot);
				if (!InGearPanel(Slot))
				{
					Panel.bGaugeShown = false;
				}
			}
		}
	}

	void FBattleHud::StartTurnIcon(int32 Slot)
	{
		Flight.StartY = 0x1C;
		Flight.StartX = PanelX(Slot) + 0x48;
		if (InGearPanel(Slot))
		{
			Flight.StartY = 0x24;
			Flight.StartX = PanelX(Slot) + 0x44;
		}
		Flight.TargetX = IconTargetX;
		Flight.TargetY = IconTargetY;
		const uint32 Across = static_cast<uint32>(Flight.StartX - 5 - Flight.TargetX);
		const uint32 Down = static_cast<uint32>(Flight.TargetY - (Flight.StartY + 5));
		Flight.Scale = IconStartScale;
		Flight.Steps = 6;
		Flight.StepY = 0x100;
		Flight.MovedX = 0;
		Flight.MovedY = 0;
		Flight.Angle = 0;
		Flight.StepX = (Across << 8) / Down;
		Flight.Growth = 1;
		--Battle->Party.TurnIcons[static_cast<std::size_t>(Slot)];
	}

	void FBattleHud::MoveTurnIcon(int32 Slot)
	{
		if (Flight.Steps == 0)
		{
			FaultState = EBattleHudFault::IconMovedWithoutSteps;
			return;
		}
		uint16 X = 0;
		uint16 Y = 0;
		for (int32 Step = 0; Step < Flight.Steps; ++Step)
		{
			Flight.MovedX = static_cast<int32>(static_cast<uint32>(Flight.MovedX) - Flight.StepX);
			Flight.MovedY = static_cast<int32>(static_cast<uint32>(Flight.MovedY) + Flight.StepY);
			X = static_cast<uint16>((static_cast<uint32>(Flight.MovedX) >> 8) + static_cast<uint32>(Flight.StartX));
			Y = static_cast<uint16>((static_cast<uint32>(Flight.MovedY) >> 8) + static_cast<uint32>(Flight.StartY));
		}
		Flight.Steps = 0;
		Flight.Count = DrawSheetRotated(IconFrame, Flight.Icon, 0, static_cast<int16>(X), static_cast<int16>(Y), Flight.Scale, Flight.Angle);
		const int32 Shadow = Flight.Count;
		Flight.Count += DrawSheetRotated(IconShadowFrame, Flight.Icon, Flight.Count, static_cast<int16>(X), static_cast<int16>(Y), Flight.Scale, Flight.Angle);
		for (int32 Part = Shadow; Part < Flight.Count && Part < FBattleHudFlight::Parts; ++Part)
		{
			SetSemiTrans(Flight.Icon[static_cast<std::size_t>(Part)][Buffer], true);
		}
		Flight.Buffer = Buffer;
		for (int32 Step = 0; Step < Flight.Growth; ++Step)
		{
			Flight.Scale = static_cast<uint16>(Flight.Scale + IconScaleStep);
			Flight.Angle = static_cast<uint16>(Flight.Angle + IconAngleStep);
		}
		Flight.Growth = 0;
		if (X <= static_cast<uint32>(Flight.TargetX) || static_cast<uint32>(Flight.TargetY) <= Y)
		{
			Battle->Party.TurnIcons[static_cast<std::size_t>(Slot)] = IconArrived;
		}
	}

	int32 FBattleHud::DrawSheetRotated(int32 Frame, std::span<FBattleHudPair> Pairs, int32 First, int32 X, int32 Y, int32 Scale, int32 Angle)
	{
		const std::span<const FMenuSpritePart> Parts = Sheet->Frame(Frame);
		FGte Gte;
		Gte.Transform = RotateZ(ScaleRows(FGteMatrix::Identity(), {static_cast<int16>(Scale), static_cast<int16>(Scale), FGteMatrix::One}),
			static_cast<int16>(Angle));
		Gte.Transform.T = {0, 0, 0};
		Gte.OffsetX = static_cast<int16>(X) * 0x10000;
		Gte.OffsetY = static_cast<int16>(Y) * 0x10000;
		Gte.ProjectionDistance = RotatedScreen;
		int32 Index = First;
		for (const FMenuSpritePart& Part : Parts)
		{
			if (Index >= static_cast<int32>(Pairs.size()))
			{
				FaultState = EBattleHudFault::PartsOutsidePrimitives;
				return static_cast<int32>(Parts.size());
			}
			FMenuPrimitive& Quad = Pairs[static_cast<std::size_t>(Index)][Buffer];
			++Index;
			BattleHudGpu::Set(Quad, TexturedQuad);
			SetSemiTrans(Quad, false);
			SetShadeTex(Quad, true);
			Quad.Page = MenuGpu::TexturePage(Part.Depth, 0, Part.ImageX, Part.ImageY);
			Quad.Clut = MenuGpu::Clut(Part.ClutX, Part.ClutY);
			const int16 Near = Part.X;
			const int16 Far = static_cast<int16>(Part.X + Part.Width);
			const int16 Top = Part.Y;
			const int16 Bottom = static_cast<int16>(Part.Y + Part.Height);
			const int16 Left = Part.bFlipX ? Far : Near;
			const int16 Right = Part.bFlipX ? Near : Far;
			const int16 Upper = Part.bFlipY ? Bottom : Top;
			const int16 Lower = Part.bFlipY ? Top : Bottom;
			const std::array<FGteVector, 4> Corners{FGteVector{Left, Upper, RotatedDepth}, FGteVector{Right, Upper, RotatedDepth},
				FGteVector{Left, Lower, RotatedDepth}, FGteVector{Right, Lower, RotatedDepth}};
			for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
			{
				const FGteProjection Projected = Gte.Rtps(Corners[Corner]);
				Quad.Corners[Corner].X = Projected.ScreenX;
				Quad.Corners[Corner].Y = Projected.ScreenY;
			}
			uint16 U = Part.U;
			uint16 V = Part.V;
			uint8 Width = static_cast<uint8>(Part.Width);
			uint8 Height = static_cast<uint8>(Part.Height);
			if ((Angle & 0xFFF) == 0xC00)
			{
				U = static_cast<uint16>(U - 1);
			}
			if ((Angle & 0xFFF) == 0)
			{
				// Turned half way round, a part drawn mirrored samples its texels from one lower, as the flat drawing does.
				if (Quad.Corners[3].X < Quad.Corners[0].X)
				{
					U = static_cast<uint16>(U - 1);
					if (static_cast<int16>(U) < 0)
					{
						U = 0;
						--Width;
					}
				}
				if (Quad.Corners[3].Y < Quad.Corners[0].Y)
				{
					V = static_cast<uint16>(V - 1);
					if (static_cast<int16>(V) < 0)
					{
						V = 0;
						--Height;
					}
				}
			}
			SetTexels(Quad, U, V, U + Width, V, U, V + Height, U + Width, V + Height);
		}
		return static_cast<int32>(Parts.size());
	}

	void FBattleHud::StartActorFlash(uint8 Slot)
	{
		const int16 X = PanelX(Slot);
		SetCorners(Flash[Buffer], X + 0x10, 8, X + 0x28, 8, X + 0x10, 0x20, X + 0x28, 0x20);
		FlashBuffer = Buffer;
		bFlashOn = true;
	}

	void FBattleHud::BoardGear(uint8 Slot)
	{
		if (Battle->PartyCharacters[Slot] != ChuChu)
		{
			Panels[Slot].Mode = 2;
		}
	}

	void FBattleHud::DrawFlash()
	{
		if (!bFlashOn)
		{
			return;
		}
		const uint8 Level = static_cast<uint8>(FlashGlow);
		Flash[FlashBuffer].Corners[0].Color = {Level, Level, Level};
		Link(Flash[FlashBuffer]);
		Link(FlashMode[FlashBuffer]);
	}

	void FBattleHud::DrawPanels()
	{
		if (ApTickCount != 0)
		{
			LinkPairs(ApTicks, ApTickCount, ApTickBuffer);
		}
		for (const FBattleHudPanel& Panel : Panels)
		{
			LinkPairs(Panel.Icon, Panel.IconCount, Panel.IconBuffer);
		}
		LinkPairs(Flight.Icon, Flight.Count, Flight.Buffer, BattleHudBlendKeys::Flight(0));
		for (const FBattleHudPanel& Panel : Panels)
		{
			if (!Panel.bIconShown)
			{
				continue;
			}
			if (Panel.Mode == 1)
			{
				LinkPairs(Panel.ApLabel, Panel.ApLabelCount, Panel.LabelBuffer);
			}
			else if (Panel.Mode == 2)
			{
				LinkPairs(Panel.FuelLabel, Panel.FuelLabelCount, Panel.LabelBuffer);
			}
			LinkPairs(Panel.Ap, Panel.ApCount, Panel.ApBuffer);
		}
		if (bApNumbersShown)
		{
			LinkPairs(ApNumbers, ApNumberCount, ApNumberBuffer);
		}
		for (const FBattleHudPanel& Panel : Panels)
		{
			LinkPairs(Panel.Hp, Panel.HpCount, Panel.HpBuffer);
		}
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			if (Battle->Slots[static_cast<std::size_t>(Slot)].Occupant == NoMember)
			{
				continue;
			}
			const FBattleHudPanel& Panel = Panels[static_cast<std::size_t>(Slot)];
			Link(Panel.Portrait[PortraitBuffer & 1]);
			if (Panel.bIconShown)
			{
				LinkPairs(Panel.RestingIcon, Panel.RestingIconCount, RestingIconBuffer);
				Link(Panel.Bar[Buffer], BattleHudBlendKeys::StoredBar(static_cast<uint32>(Slot)));
			}
		}
	}

	void FBattleHud::ShowApTicks(uint8 ApAtStart, uint8 ApLeft)
	{
		if (ApAtStart == ApLeft)
		{
			--ApLeft;
		}
		for (int32 Tick = 0; Tick < ApAtStart - 1 && Tick < static_cast<int32>(ApTicks.size()); ++Tick)
		{
			const int32 Y = Resources.Tables.ApTicks[ApAtStart % Resources.Tables.ApTicks.size()][static_cast<std::size_t>(Tick)] + 0x5E;
			FMenuPrimitive& Line = ApTicks[static_cast<std::size_t>(Tick)][Buffer];
			Line.Corners[0].X = 0xC;
			Line.Corners[0].Y = static_cast<int16>(Y);
			Line.Corners[1].X = 0x12;
			Line.Corners[1].Y = static_cast<int16>(Y);
		}
		ApTickCount = ApLeft;
		ApTickBuffer = Buffer;
	}

	void FBattleHud::DrawApNumbers(uint8 ApLeft, uint8 ApAtStart)
	{
		ApNumberCount = DrawSheet(ApNumberFrame + ApLeft, ApNumbers, 0, 0x2A, ApNumbersY, FullScale);
		ApNumberCount += DrawSheet(ApSlashFrame, ApNumbers, ApNumberCount, 0x32, ApNumbersY, FullScale);
		ApNumberCount += DrawSheet(ApNumberFrame + ApAtStart, ApNumbers, ApNumberCount, 0x3A, ApNumbersY, FullScale);
		ApNumberBuffer = Buffer;
	}
}
