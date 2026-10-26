#include "Battle/BattleContext.h"
#include "Battle/BattleGameState.h"
#include "Battle/Show/BattleHudDigits.h"
#include "Battle/Show/BattleHudGpu.h"
#include "Battle/Show/BattleHudLists.h"
#include "Battle/Show/BattleShowHud.h"
#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Text/MesRender.h"

#include <algorithm>

namespace Weltall
{
	using namespace BattleHudGpu;
	using namespace BattleHudLists;

	namespace
	{
		constexpr uint8 NoMember = 0x7F;
		/** The result sheet's frames: the party's portraits a slot each, the changes' signs, the marks of the level's and the ether level's rows. */
		constexpr int32 PlusFrame = 0xE3;
		constexpr int32 MinusFrame = 0xE5;
		constexpr int32 LevelMark = 0xE8;
		constexpr int32 EtherLevelMark = 0xE9;
		/** A panel lies 20h below the one before; a digit 8 pixels from the one before. */
		constexpr int32 PanelStep = 0x20;
		constexpr int32 DigitStep = 8;
		/** The tints a part is drawn in instead of its own colours. */
		constexpr std::array<uint8, 3> Red{0x80, 0x40, 0x40};
		constexpr std::array<uint8, 3> Green{0x40, 0x80, 0x40};
		constexpr std::array<uint8, 3> Blue{0x40, 0x40, 0x80};
		constexpr std::array<uint8, 3> Dim{0x40, 0x40, 0x40};
		/** A learned Deathblow's name is drawn green, an ability's yellow. */
		constexpr std::array<uint8, 3> DeathblowName{0, 0x80, 0};
		constexpr std::array<uint8, 3> AbilityName{0x80, 0x80, 0};
		/** The stat bars' colours at their top (black at the foot): the value's, a rise's, a fall's. */
		constexpr std::array<uint8, 3> ValueBar{0xFF, 0x80, 0x80};
		constexpr std::array<uint8, 3> RiseBar{0xFF, 0, 0};
		constexpr std::array<uint8, 3> FallBar{0, 0, 0xFF};
		/** The stat rows: 8 apart from y 90h, each bar (from FBattleResultPlaces::BarX) 6 high from 2 below its row's top. */
		constexpr int32 StatRowY = 0x90;
		constexpr int32 StatRowStep = 8;
		constexpr int32 BarTop = 2;
		constexpr int32 BarBottom = 8;
		/** The screen's windows: the items', the level-ups', the gold's. */
		constexpr std::array<std::array<int16, 4>, 3> ResultWindows{{{0x18, 0x18, 0x90, 0xA0}, {0x28, 0x78, 0xE8, 0x58}, {0xB0, 0x38, 0x70, 0x38}}};
		/** The labels typed for the screen (the lists' labels from the tenth on): the two headings and the learned skill's message. */
		constexpr int32 ItemsHeading = 10;
		constexpr int32 GoldHeading = 11;
		constexpr int32 LearnedLabel = 14;
		/** The kernel's name tables: a character's Deathblows lie in the table of its number. */
		constexpr int32 AccessoryNameTable = 17;
		constexpr int32 AbilityNameTable = 20;
		constexpr int32 ItemNameTable = 22;
		constexpr int32 WeaponNameTable = 23;
		constexpr int32 GearPartNameTable = 50;
		constexpr int32 GearWeaponNameTable = 51;
		constexpr int32 AbilitiesPerCharacter = 16;
		/** Where a learned skill's name is uploaded and drawn from, and where the drops' names are, a row each. */
		constexpr int16 LearnedNameX = 0x3C0;
		constexpr int16 LearnedNameY = 0x1A;
		constexpr int16 ItemNameX = 0x380;
		constexpr int16 ItemNameY = 0x100;
		/** The text quads' pages by their mode: 0 at (380h, 0), 1 at (380h, 100h), 2 at (3C0h, 100h), 3 at (3C0h, 0). */
		constexpr std::array<std::array<int16, 2>, 4> TextPages{{{0x380, 0}, {0x380, 0x100}, {0x3C0, 0x100}, {0x3C0, 0}}};
		/** The gold's labels: as many parts drawn as there are labels, however many the labels have. */
		constexpr int32 GoldLabelParts = 7;

		void SetBar(FBattleHudPair& Bar, const std::array<uint8, 3>& Colour)
		{
			for (FMenuPrimitive& Quad : Bar)
			{
				BattleHudGpu::Set(Quad, GouraudQuad);
				SetColor(Quad, 0, Colour[0], Colour[1], Colour[2]);
				SetColor(Quad, 1, Colour[0], Colour[1], Colour[2]);
				SetColor(Quad, 2, 0, 0, 0);
				SetColor(Quad, 3, 0, 0, 0);
			}
		}

		/** A bar's length for a value: 64 pixels for the largest of the member's stats. */
		int32 BarLength(int32 Value, int32 Scale)
		{
			return Value * 100 / Scale * 0x1900 / 10000;
		}
	}

	void FBattleHud::BeginResult()
	{
		for (FBattleHudPanel& Panel : Panels)
		{
			Panel.bGaugeShown = false;
		}
	}

	void FBattleHud::LoadResult()
	{
		for (const FVramImage& Image : Resources.ResultImages)
		{
			Upload(Image);
		}
		Sheet = &Resources.ResultSheet;
		LoadPortraits(Resources.ResultPortraits, FBattleHudResources::ResultPortraitFrame);
		RenderLabels();
	}

	void FBattleHud::MakeResultWork()
	{
		Result.emplace();
	}

	void FBattleHud::InitTextQuads(FBattleHudPair& Quads, uint8 Plane, int32 Mode)
	{
		for (FMenuPrimitive& Quad : Quads)
		{
			BattleHudGpu::Set(Quad, TexturedQuad);
			SetColor(Quad, 0, FullLevel, FullLevel, FullLevel);
			SetSemiTrans(Quad, false);
			SetShadeTex(Quad, true);
			Quad.Page = MenuGpu::TexturePage(0, 0, TextPages[static_cast<std::size_t>(Mode)][0], TextPages[static_cast<std::size_t>(Mode)][1]);
			Quad.Clut = Plane != 0 ? OddTextClut : EvenTextClut;
		}
	}

	void FBattleHud::DrawResultDigits(std::span<FBattleHudPair> Parts, int32& Count, uint32 Value, std::size_t First, int32 X, int32 Y, bool bCloseUp)
	{
		const std::array<uint8, 9> Digits = BattleHudDigits::ToDigits(Value);
		for (std::size_t Place = First; Place < Digits.size(); ++Place)
		{
			const bool bDrawn = Digits[Place] != BattleHudDigits::Blank;
			if (bDrawn)
			{
				// A digit's frame is its number: the sheet's first frames are the digits.
				Count += DrawSheet(Digits[Place], Parts, Count, X, Y, FullScale);
			}
			if (bDrawn || !bCloseUp)
			{
				X += DigitStep;
			}
		}
	}

	void FBattleHud::TintResultParts(std::span<FBattleHudPair> Parts, int32 First, int32 End, const std::array<uint8, 3>& Tint)
	{
		for (int32 Part = First; Part < End; ++Part)
		{
			if (Part < 0 || Part >= static_cast<int32>(Parts.size()))
			{
				FaultState = EBattleHudFault::PartsOutsidePrimitives;
				return;
			}
			FMenuPrimitive& Quad = Parts[static_cast<std::size_t>(Part)][Buffer];
			SetShadeTex(Quad, false);
			SetColor(Quad, 0, Tint[0], Tint[1], Tint[2]);
		}
	}

	std::span<const uint8> FBattleHud::ResultRecord(std::size_t Slot)
	{
		const uint8 Character = Battle->PartyCharacters[Slot];
		if (Character >= GameStateLayout::CharacterCount || !BattleGameState::HasCharacter(Character))
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return {};
		}
		return BattleGameState::CharacterRecord(*Battle->GameState, Character);
	}

	void FBattleHud::BuildResultPortraits()
	{
		if (!Result)
		{
			FaultState = EBattleHudFault::ResultNotMade;
			return;
		}
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			auto& Portrait = Result->Panels[static_cast<std::size_t>(Slot)].Portrait;
			Portrait.Count = 0;
			if (Battle->Slots[static_cast<std::size_t>(Slot)].Occupant != NoMember)
			{
				Portrait.Count += DrawSheet(FBattleHudResources::ResultPortraitFrame + Slot, Portrait.Parts, Portrait.Count, 0x20, 0x24 + PanelStep * Slot, FullScale);
			}
			Portrait.Buffer = Buffer;
		}
	}

	void FBattleHud::BuildResultPanelLabels()
	{
		if (!Result)
		{
			FaultState = EBattleHudFault::ResultNotMade;
			return;
		}
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			auto& Run = Result->Panels[static_cast<std::size_t>(Slot)].Labels;
			const int32 Down = PanelStep * Slot;
			Run.Count = 0;
			if (Battle->Slots[static_cast<std::size_t>(Slot)].Occupant != NoMember)
			{
				for (const FBattleHudResultLabel& Label : Resources.Tables.ResultPanelLabels)
				{
					Run.Count += DrawSheet(Label.Frame, Run.Parts, Run.Count, Label.X, static_cast<int16>(static_cast<uint16>(Label.Y) + Down), FullScale);
				}
				if (Battle->ResultRun.bEtherRows)
				{
					// The marks of the two rows, the last part of each in its row's tint.
					Run.Count += DrawSheet(LevelMark, Run.Parts, Run.Count, 0x88, Down + 0x20, FullScale);
					Run.Count += DrawSheet(EtherLevelMark, Run.Parts, Run.Count, 0x88, Down + 0x28, FullScale);
					TintResultParts(Run.Parts, Run.Count - 2, Run.Count - 1, Red);
					TintResultParts(Run.Parts, Run.Count - 1, Run.Count, Green);
				}
			}
			Run.Buffer = Buffer;
		}
	}

	void FBattleHud::BuildResultHpEp()
	{
		if (!Result)
		{
			FaultState = EBattleHudFault::ResultNotMade;
			return;
		}
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			FBattleHudResultPanel& Panel = Result->Panels[static_cast<std::size_t>(Slot)];
			Panel.Hp.Count = 0;
			Panel.Ep.Count = 0;
			Panel.MaxHp.Count = 0;
			Panel.MaxEp.Count = 0;
			if (Battle->Slots[static_cast<std::size_t>(Slot)].Occupant == NoMember)
			{
				continue;
			}
			const FBattleCombatant& Member = Battle->Combatants[static_cast<std::size_t>(Slot)];
			const FBattleStartFigures& Start = Battle->StartFigures[static_cast<std::size_t>(Slot)];
			const int32 Y = 0x20 + PanelStep * Slot;
			DrawResultDigits(Panel.Hp.Parts, Panel.Hp.Count, Member.Hp, 6, 0x48, Y, false);
			Panel.Hp.Buffer = Buffer;
			DrawResultDigits(Panel.Ep.Parts, Panel.Ep.Count, Member.Ep, 7, 0x50, Y + 8, false);
			Panel.Ep.Buffer = Buffer;
			// The most the member had as the battle began, before what it wore added to them.
			DrawResultDigits(Panel.MaxHp.Parts, Panel.MaxHp.Count, Start.MaxHp, 6, 0x68, Y, false);
			Panel.MaxHp.Buffer = Buffer;
			DrawResultDigits(Panel.MaxEp.Parts, Panel.MaxEp.Count, Start.MaxEp, 7, 0x70, Y + 8, false);
			Panel.MaxEp.Buffer = Buffer;
		}
	}

	void FBattleHud::BuildResultLevels(bool bNew)
	{
		if (!Result)
		{
			FaultState = EBattleHudFault::ResultNotMade;
			return;
		}
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			FBattleHudResultPanel& Panel = Result->Panels[static_cast<std::size_t>(Slot)];
			Panel.Level.Count = 0;
			Panel.EtherLevel.Count = 0;
			if (Battle->Slots[static_cast<std::size_t>(Slot)].Occupant == NoMember)
			{
				continue;
			}
			uint8 Level = Battle->ApRecords[static_cast<std::size_t>(Slot)].Level;
			uint8 EtherLevel = Battle->ApRecords[static_cast<std::size_t>(Slot)].EtherLevel;
			if (bNew)
			{
				const std::span<const uint8> Record = ResultRecord(static_cast<std::size_t>(Slot));
				if (Record.empty())
				{
					return;
				}
				Level = Record[GameStateLayout::Character::Level];
				EtherLevel = Record[GameStateLayout::Character::EtherLevel];
			}
			const int32 Y = 0x20 + PanelStep * Slot;
			DrawResultDigits(Panel.Level.Parts, Panel.Level.Count, Level, 6, 0x90, Y, false);
			Panel.Level.Buffer = Buffer;
			if (Battle->ResultRun.bEtherRows)
			{
				DrawResultDigits(Panel.EtherLevel.Parts, Panel.EtherLevel.Count, EtherLevel, 6, 0x90, Y + 8, false);
				Panel.EtherLevel.Buffer = Buffer;
				TintResultParts(Panel.Level.Parts, 0, Panel.Level.Count, Red);
				TintResultParts(Panel.EtherLevel.Parts, 0, Panel.EtherLevel.Count, Green);
			}
		}
	}

	void FBattleHud::BuildResultExpCounters()
	{
		if (!Result)
		{
			FaultState = EBattleHudFault::ResultNotMade;
			return;
		}
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			FBattleHudResultPanel& Panel = Result->Panels[static_cast<std::size_t>(Slot)];
			Panel.Exp.Count = 0;
			Panel.EtherExp.Count = 0;
			if (Battle->Slots[static_cast<std::size_t>(Slot)].Occupant == NoMember)
			{
				continue;
			}
			const FBattleStartFigures& Counters = Battle->StartFigures[static_cast<std::size_t>(Slot)];
			const int32 Y = 0x20 + PanelStep * Slot;
			DrawResultDigits(Panel.Exp.Parts, Panel.Exp.Count, Counters.Exp, 1, 0xB0, Y, false);
			Panel.Exp.Buffer = Buffer;
			if (Battle->ResultRun.bEtherRows)
			{
				DrawResultDigits(Panel.EtherExp.Parts, Panel.EtherExp.Count, Counters.EtherExp, 1, 0xB0, Y + 8, false);
				Panel.EtherExp.Buffer = Buffer;
			}
		}
	}

	void FBattleHud::BuildResultExpGains()
	{
		if (!Result)
		{
			FaultState = EBattleHudFault::ResultNotMade;
			return;
		}
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			FBattleHudResultPanel& Panel = Result->Panels[static_cast<std::size_t>(Slot)];
			Panel.ExpGain.Count = 0;
			Panel.EtherExpGain.Count = 0;
			if (Battle->Slots[static_cast<std::size_t>(Slot)].Occupant == NoMember)
			{
				continue;
			}
			const int32 Y = 0x20 + PanelStep * Slot;
			DrawResultDigits(Panel.ExpGain.Parts, Panel.ExpGain.Count, Battle->Result.ExpGains[static_cast<std::size_t>(Slot)], 2, 0xF8, Y, false);
			Panel.ExpGain.Buffer = Buffer;
			if (Battle->ResultRun.bEtherRows)
			{
				DrawResultDigits(Panel.EtherExpGain.Parts, Panel.EtherExpGain.Count, Battle->Result.EtherExpGains[static_cast<std::size_t>(Slot)], 2, 0xF8, Y + 8, false);
				Panel.EtherExpGain.Buffer = Buffer;
			}
		}
	}

	void FBattleHud::OpenResultWindow(int32 Window)
	{
		if (Window < 0 || Window >= static_cast<int32>(ResultWindows.size()))
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		const std::array<int16, 4>& Place = ResultWindows[static_cast<std::size_t>(Window)];
		OpenWindow(Window, Place[0], Place[1], static_cast<uint16>(Place[2]), static_cast<uint16>(Place[3]), false);
	}

	void FBattleHud::ShowWindow(int32 Window, bool bShown)
	{
		if (Window < 0 || Window >= WindowCount)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		Windows[static_cast<std::size_t>(Window)].bShown = bShown;
	}

	void FBattleHud::BuildResultLevelUp(uint8 Slot)
	{
		if (!Result)
		{
			FaultState = EBattleHudFault::ResultNotMade;
			return;
		}
		if (Slot >= PartySlots)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		FBattleHudResult& Work = *Result;
		Work.Portrait.Count = DrawSheet(FBattleHudResources::ResultPortraitFrame + Slot, Work.Portrait.Parts, 0, 0x3E, 0xA4, FullScale);
		Work.Portrait.Buffer = Buffer;
		Work.Labels.Count = 0;
		for (const FBattleHudLevelLabel& Label : Resources.Tables.ResultLevelLabels)
		{
			if (Label.Frame == FBattleHudLevelLabel::None)
			{
				continue;
			}
			const int32 First = Work.Labels.Count;
			Work.Labels.Count += DrawSheet(Label.Frame, Work.Labels.Parts, First, Label.X, Label.Y, FullScale);
			if (Label.bTinted)
			{
				// A tint past the two is read from the game's stack.
				if (Label.Tint > 1)
				{
					FaultState = EBattleHudFault::IndexOutsideHud;
					return;
				}
				TintResultParts(Work.Labels.Parts, First, Work.Labels.Count, Label.Tint == 0 ? Red : Dim);
			}
		}
		Work.Labels.Buffer = Buffer;

		// Max HP and EP as the battle began, and what the level-ups changed them by.
		const std::span<const uint8> Record = ResultRecord(Slot);
		if (Record.empty())
		{
			return;
		}
		const FBattleStartFigures& Start = Battle->StartFigures[Slot];
		const FBattleResultPlaces& Places = Resources.ResultPlaces;
		Work.OldMaxHp.Count = 0;
		Work.OldMaxEp.Count = 0;
		Work.MaxHpChange.Count = 0;
		Work.MaxEpChange.Count = 0;
		DrawResultDigits(Work.OldMaxHp.Parts, Work.OldMaxHp.Count, Start.MaxHp, 6, Places.OldMaxHpX, 0x80, false);
		Work.OldMaxHp.Buffer = Buffer;
		DrawResultDigits(Work.OldMaxEp.Parts, Work.OldMaxEp.Count, Start.MaxEp, 7, Places.OldMaxEpX, 0x88, false);
		Work.OldMaxEp.Buffer = Buffer;
		const int32 HpChange = static_cast<int32>(LittleEndian::ReadU16(Record, GameStateLayout::Character::MaxHp)) - Start.MaxHp;
		if (HpChange != 0)
		{
			Work.MaxHpChange.Count = DrawSheet(HpChange < 0 ? MinusFrame : PlusFrame, Work.MaxHpChange.Parts, 0, Places.ChangeSignX, 0x80, FullScale);
			DrawResultDigits(Work.MaxHpChange.Parts, Work.MaxHpChange.Count, static_cast<uint32>(HpChange < 0 ? -HpChange : HpChange), 6, Places.ChangeDigitsX, 0x80, true);
			TintResultParts(Work.MaxHpChange.Parts, 0, Work.MaxHpChange.Count, Red);
			Work.MaxHpChange.Buffer = Buffer;
		}
		const int32 EpChange = static_cast<int32>(LittleEndian::ReadU16(Record, GameStateLayout::Character::MaxEp)) - Start.MaxEp;
		if (EpChange != 0)
		{
			Work.MaxEpChange.Count = DrawSheet(EpChange < 0 ? MinusFrame : PlusFrame, Work.MaxEpChange.Parts, 0, Places.ChangeSignX, 0x88, FullScale);
			DrawResultDigits(Work.MaxEpChange.Parts, Work.MaxEpChange.Count, static_cast<uint32>(EpChange < 0 ? -EpChange : EpChange), 7, Places.ChangeDigitsX, 0x88, true);
			TintResultParts(Work.MaxEpChange.Parts, 0, Work.MaxEpChange.Count, Red);
			Work.MaxEpChange.Buffer = Buffer;
		}

		// The stats: each one's bar as long as it was by the largest of them all, the change's after it or over its end.
		const std::array<uint8, FBattleHudResult::StatCount>& Old = Start.Stats;
		const std::array<uint8, FBattleHudResult::StatCount>& New = Battle->Result.NewStats[Slot];
		const int32 Scale = std::max(*std::max_element(Old.begin(), Old.end()), *std::max_element(New.begin(), New.end()));
		if (Scale == 0)
		{
			FaultState = EBattleHudFault::ResultStatsZero;
			return;
		}
		for (int32 Row = 0; Row < FBattleHudResult::StatCount; ++Row)
		{
			FBattleHudResultStat& Stat = Work.Stats[static_cast<std::size_t>(Row)];
			Stat.Value.Count = 0;
			Stat.Change.Count = 0;
			const int32 Was = Old[static_cast<std::size_t>(Row)];
			int32 Change = New[static_cast<std::size_t>(Row)] - Was;
			const bool bFall = Change < 0;
			if (bFall)
			{
				Change = -Change;
			}
			const int32 Length = BarLength(Was, Scale);
			const int32 ChangeLength = BarLength(Change, Scale);
			SetBar(Stat.Bar, ValueBar);
			SetBar(Stat.ChangeBar, bFall ? FallBar : RiseBar);
			const int32 Y = StatRowY + StatRowStep * Row;
			const int16 Top = static_cast<int16>(Y + BarTop);
			const int16 Bottom = static_cast<int16>(Y + BarBottom);
			const int16 End = static_cast<int16>(Length + Places.BarX);
			SetCorners(Stat.Bar[Buffer], Places.BarX, Top, End, Top, Places.BarX, Bottom, End, Bottom);
			const int16 Left = bFall ? static_cast<int16>(Length - (ChangeLength - Places.FallBarX)) : End;
			const int16 Right = static_cast<int16>(Left + ChangeLength);
			SetCorners(Stat.ChangeBar[Buffer], Left, Top, Right, Top, Left, Bottom, Right, Bottom);
			Stat.BarBuffer = Buffer;
			DrawResultDigits(Stat.Value.Parts, Stat.Value.Count, static_cast<uint32>(Was), 6, Places.StatValueX, Y, false);
			Stat.Value.Buffer = Buffer;
			if (Change != 0)
			{
				Stat.Change.Count = DrawSheet(bFall ? MinusFrame : PlusFrame, Stat.Change.Parts, 0, Places.ChangeSignX, Y, FullScale);
				DrawResultDigits(Stat.Change.Parts, Stat.Change.Count, static_cast<uint32>(Change), 6, Places.ChangeDigitsX, Y, true);
				TintResultParts(Stat.Change.Parts, 0, Stat.Change.Count, bFall ? Blue : Red);
				Stat.Change.Buffer = Buffer;
			}
		}
	}

	void FBattleHud::BuildResultLearnedPortrait(uint8 Slot)
	{
		if (!Result)
		{
			FaultState = EBattleHudFault::ResultNotMade;
			return;
		}
		if (Slot >= PartySlots)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		FBattleHudResult& Work = *Result;
		Work.Portrait.Count = DrawSheet(FBattleHudResources::ResultPortraitFrame + Slot, Work.Portrait.Parts, 0, 0x40, 0xA4, FullScale);
		Work.Portrait.Buffer = Buffer;
		Work.LearnedImage = BlankImage(NarrowUnits);
		InitTextQuads(Work.LearnedLabel, 1, 2);
		InitTextQuads(Work.LearnedName, 0, 3);
	}

	void FBattleHud::BuildResultLearned(uint8 Slot, uint8 Skill, bool bAbility)
	{
		if (!Result)
		{
			FaultState = EBattleHudFault::ResultNotMade;
			return;
		}
		if (Slot >= PartySlots)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		FBattleHudResult& Work = *Result;
		// The names are typed into the image the member's portrait was built with.
		if (Work.LearnedImage.Units.empty())
		{
			FaultState = EBattleHudFault::ResultNotMade;
			return;
		}
		const int32 Character = Battle->PartyCharacters[Slot];
		const std::span<const uint8> Name = bAbility ? TableString(AbilityNameTable, Character * AbilitiesPerCharacter + Skill) : TableString(Character, Skill);
		if (FaultState != EBattleHudFault::None)
		{
			return;
		}
		// The names of a member's skills are typed one over the other: what a longer one left past a shorter stays in the image.
		const uint8 Width = static_cast<uint8>(RenderMesString(Text, ListTyping, Name, Work.LearnedImage, 0));
		Work.LearnedImage.Rect.X = LearnedNameX;
		Work.LearnedImage.Rect.Y = LearnedNameY;
		Upload(Work.LearnedImage);
		FMenuPrimitive& Quad = Work.LearnedName[Buffer];
		SetTextQuad(Quad, 0x52, 0x9C, 0, static_cast<uint8>(LearnedNameY), Width);
		SetShadeTex(Quad, false);
		const std::array<uint8, 3>& Colour = bAbility ? AbilityName : DeathblowName;
		SetColor(Quad, 0, Colour[0], Colour[1], Colour[2]);
		Work.LearnedNameBuffer = Buffer;
		const FBattleHudLabel& Label = Labels[LearnedLabel];
		SetTextQuad(Work.LearnedLabel[Buffer], static_cast<int16>(Width + Resources.ResultPlaces.LearnedLabelX), 0x9C, Label.U, Label.V, Label.Width);
		Work.LearnedLabelBuffer = Buffer;
	}

	void FBattleHud::BuildResultSpoils()
	{
		if (!Result)
		{
			FaultState = EBattleHudFault::ResultNotMade;
			return;
		}
		FBattleHudResult& Work = *Result;
		for (const FBattleHudResultLabel& Label : Resources.Tables.ResultGoldLabels)
		{
			Work.GoldLabels.Count += DrawSheet(Label.Frame, Work.GoldLabels.Parts, Work.GoldLabels.Count, Label.X, Label.Y, FullScale);
		}
		Work.GoldLabels.Buffer = Buffer;
		DrawResultDigits(Work.GoldGained.Parts, Work.GoldGained.Count, Battle->Result.GoldGained, 3, 0xD8, 0x50, false);
		Work.GoldGained.Buffer = Buffer;
		DrawResultDigits(Work.GoldTotal.Parts, Work.GoldTotal.Count, Battle->GameState->Gold(), 0, 0xC0, 0x60, false);
		Work.GoldTotal.Buffer = Buffer;

		InitTextQuads(Work.Headings[0], 0, 2);
		InitTextQuads(Work.Headings[1], 1, 2);
		const FBattleHudLabel& Items = Labels[ItemsHeading];
		const FBattleHudLabel& Gold = Labels[GoldHeading];
		SetTextQuad(Work.Headings[0][Buffer], 0x20, 0x20, Items.U, Items.V, Items.Width);
		SetTextQuad(Work.Headings[1][Buffer], 0xB8, 0x40, Gold.U, Gold.V, Gold.Width);
		Work.HeadingsBuffer = Buffer;

		// The drops as they were grouped: each one's name on a row of its own, and how many beside it.
		int32 Row = 0;
		for (const FBattleDropGroup& Group : Battle->Result.DropGroups)
		{
			if (Group.Id == 0)
			{
				continue;
			}
			int32 Table = 0;
			switch (Group.Kind)
			{
			case BattleDropKinds::Weapon: Table = WeaponNameTable; break;
			case BattleDropKinds::Accessory: Table = AccessoryNameTable; break;
			case BattleDropKinds::Item: Table = ItemNameTable; break;
			case BattleDropKinds::GearWeapon: Table = GearWeaponNameTable; break;
			case BattleDropKinds::GearPart: Table = GearPartNameTable; break;
			default:
				FaultState = EBattleHudFault::ResultDropKind;
				return;
			}
			const std::span<const uint8> Name = TableString(Table, Group.Id);
			if (FaultState != EBattleHudFault::None)
			{
				return;
			}
			InitTextQuads(Work.ItemNames[static_cast<std::size_t>(Row)], 0, 1);
			InitTextQuads(Work.ItemCounts[static_cast<std::size_t>(Row)], 0, 2);
			FVramImage Image = BlankImage(NarrowUnits);
			const uint8 Width = static_cast<uint8>(RenderMesString(Text, ListTyping, Name, Image, 0));
			Image.Rect.X = ItemNameX;
			Image.Rect.Y = static_cast<int16>(ItemNameY + TextRows * Row);
			Upload(Image);
			const int16 Y = static_cast<int16>(Row * 0x10 + 0x30);
			SetTextQuad(Work.ItemNames[static_cast<std::size_t>(Row)][Buffer], 0x2C, Y, 0, static_cast<uint8>(TextRows * Row), Width);
			// The count is the digit's image among the labels', one a drop: more than nine would take the texels after them.
			SetTextQuad(Work.ItemCounts[static_cast<std::size_t>(Row)][Buffer], 0x94, Y, static_cast<uint8>(Group.Count * 8 + LabelsU), 0, 8);
			++Row;
		}
		Work.ItemCount = static_cast<uint8>(Row);
		Work.ItemsBuffer = Buffer;
	}

	void FBattleHud::DrawResult()
	{
		const FBattleResultRun& Run = Battle->ResultRun;
		if (!Run.bPanelsShown && !Run.bLevelUpShown && !Run.bLearnedShown && !Run.bSpoilsShown)
		{
			return;
		}
		if (!Result)
		{
			FaultState = EBattleHudFault::ResultNotMade;
			return;
		}
		const FBattleHudResult& Work = *Result;
		const auto LinkRun = [this](const auto& Sprites) { LinkPairs(Sprites.Parts, Sprites.Count, Sprites.Buffer); };
		if (Run.bPanelsShown)
		{
			for (const FBattleHudResultPanel& Panel : Work.Panels)
			{
				LinkRun(Panel.Portrait);
				LinkRun(Panel.Labels);
				LinkRun(Panel.Hp);
				LinkRun(Panel.Ep);
				LinkRun(Panel.MaxHp);
				LinkRun(Panel.MaxEp);
				LinkRun(Panel.Level);
				LinkRun(Panel.EtherLevel);
				LinkRun(Panel.Exp);
				LinkRun(Panel.EtherExp);
				LinkRun(Panel.ExpGain);
				LinkRun(Panel.EtherExpGain);
			}
		}
		if (Run.bLevelUpShown)
		{
			LinkRun(Work.Portrait);
			LinkRun(Work.Labels);
			LinkRun(Work.OldMaxHp);
			LinkRun(Work.OldMaxEp);
			LinkRun(Work.MaxHpChange);
			LinkRun(Work.MaxEpChange);
			for (const FBattleHudResultStat& Stat : Work.Stats)
			{
				Link(Stat.ChangeBar[Stat.BarBuffer & 1]);
				Link(Stat.Bar[Stat.BarBuffer & 1]);
				LinkRun(Stat.Value);
				LinkRun(Stat.Change);
			}
		}
		if (Run.bLearnedShown)
		{
			LinkRun(Work.Portrait);
			Link(Work.LearnedLabel[Work.LearnedLabelBuffer & 1]);
			Link(Work.LearnedName[Work.LearnedNameBuffer & 1]);
		}
		if (Run.bSpoilsShown)
		{
			LinkPairs(Work.GoldLabels.Parts, GoldLabelParts, Work.GoldLabels.Buffer);
			LinkRun(Work.GoldGained);
			LinkRun(Work.GoldTotal);
			for (std::size_t Row = 0; Row < Work.ItemCount; ++Row)
			{
				Link(Work.ItemNames[Row][Work.ItemsBuffer & 1]);
				Link(Work.ItemCounts[Row][Work.ItemsBuffer & 1]);
			}
			Link(Work.Headings[0][Work.HeadingsBuffer & 1]);
			Link(Work.Headings[1][Work.HeadingsBuffer & 1]);
		}
	}
}
