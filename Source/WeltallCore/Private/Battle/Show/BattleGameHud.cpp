#include "Battle/Show/BattleGameHud.h"

#include "Battle/BattleContext.h"
#include "Battle/BattleEvent.h"
#include "Battle/BattleProgramShow.h"
#include "Battle/Show/BattleEventFiles.h"
#include "Formats/Tim.h"
#include "Foundation/LittleEndian.h"

#include <array>

namespace Weltall
{
	namespace
	{
		/** Where a scene's file keeps the offset of its name table (its enemies' names, then their messages). */
		constexpr std::size_t GameHudNameTableOffset = 0x30;
		/** The message box the action's name is typed into. */
		constexpr int32 GameHudActionNameBox = 7;
		/** The window a battle event's message opens. */
		constexpr int32 GameHudEventWindow = 0;
		/** A message's style: its portrait at the window's right. */
		constexpr uint16 GameHudPortraitAtRight = 0x1;
		/** A learned skill's cue: the skill's number, and whether it is an ability. */
		constexpr uint8 GameHudLearnedSkill = 0x7F;
		constexpr uint8 GameHudLearnedAbility = 0x80;
		/** A message of no text: its zero byte alone. */
		constexpr std::array<uint8, 1> GameHudNoText{0};
	}

	FBattleGameHud::FBattleGameHud(const FBattleHudResources& InResources, FMesContext& InText, const FBattleHudSettings& InSettings,
		const FBattleEventFiles* InEventFiles, FUnreachedCases* InUnreached)
		: Resources(InResources)
		, Text(InText)
		, Settings(InSettings)
		, EventFiles(InEventFiles)
		, Unreached(InUnreached)
	{
	}

	void FBattleGameHud::Begin(FBattleContext& InContext, FVram& Vram)
	{
		Context = &InContext;
		Hud.emplace(Resources, Text, Vram);
		Hud->Begin(InContext, Settings);
		const std::span<const uint8> Scene = InContext.SceneFile;
		if (Scene.size() >= GameHudNameTableOffset + 2)
		{
			const uint16 At = LittleEndian::ReadU16(Scene, GameHudNameTableOffset);
			if (At < Scene.size())
			{
				if (TResult<FMessageTable> Table = FMessageTable::Parse(Scene.subspan(At)))
				{
					Names = *Table;
				}
			}
		}
		if (TResult<FMessageTable> Table = FMessageTable::Parse(Resources.Messages))
		{
			Messages = *Table;
		}
	}

	void FBattleGameHud::BeginMessageBoxes()
	{
		Hud->BeginMessageBoxes();
	}

	void FBattleGameHud::Flip()
	{
		Hud->Flip();
	}

	void FBattleGameHud::Draw(FBattleHudFrame& Frame, bool bHidden)
	{
		Hud->SetHidden(bHidden);
		Hud->Draw(Frame);
	}

	void FBattleGameHud::Tick(uint8 InputMode)
	{
		if (InputMode == BattleInputModes::Commands)
		{
			Hud->StepMenuInput();
		}
		else if (InputMode == BattleInputModes::Messages)
		{
			Hud->AnimateMenuTextures();
		}
	}

	void FBattleGameHud::TypeMessage(uint8 Box, uint8 Number)
	{
		const std::span<const uint8> Line = Entry(Names, Number);
		if (TableFault == EBattleHudFault::None)
		{
			Hud->TypeMessageBox(Box, Line);
		}
	}

	void FBattleGameHud::TypeActionName(uint8 Message)
	{
		const std::span<const uint8> Line = Entry(Messages, Message);
		if (TableFault == EBattleHudFault::None)
		{
			Hud->TypeMessageBox(GameHudActionNameBox, Line);
		}
	}

	void FBattleGameHud::ShowNameBox(uint8 Number)
	{
		const std::span<const uint8> Line = Entry(Names, Number);
		if (TableFault == EBattleHudFault::None)
		{
			Hud->ShowNameBox(Line);
		}
	}

	void FBattleGameHud::ShowMessageBox(int32 Box, bool bShown)
	{
		if (bShown)
		{
			Hud->ShowMessageBox(Box);
		}
		else
		{
			Hud->HideMessageBox(Box);
		}
	}

	void FBattleGameHud::HideMessageBoxes()
	{
		Hud->HideMessageBoxes();
	}

	void FBattleGameHud::PlayMenuCue(EBattleMenuCue Cue, uint8 Slot, uint8 Arg)
	{
		const FBattlePartyInput& Party = Context->Party;
		switch (Cue)
		{
		case EBattleMenuCue::MakeCommandHud:
			Hud->MakeCommandHud();
			break;
		case EBattleMenuCue::CloseCommandHud:
			Hud->CloseCommandHud();
			break;
		case EBattleMenuCue::ApTicks:
			Hud->ShowApTicks(Party.ApAtStart, Party.ApLeft);
			break;
		case EBattleMenuCue::HideApTicks:
			Hud->HideApTicks();
			break;
		case EBattleMenuCue::CommandIcons:
			Hud->DrawCommandIcons(Slot);
			break;
		case EBattleMenuCue::ApNumbers:
			Hud->DrawApNumbers(Party.ApLeft, Party.ApAtStart);
			break;
		case EBattleMenuCue::ShowApNumbers:
			Hud->ShowApNumbers(Arg != 0);
			break;
		case EBattleMenuCue::CancelApNumbers:
			Hud->HideApNumbers();
			break;
		case EBattleMenuCue::InitArrows:
			Hud->InitDirectionArrows();
			break;
		case EBattleMenuCue::HideArrows:
			Hud->HideDirectionArrows();
			break;
		case EBattleMenuCue::ArrowTargets:
			Hud->SetArrowTargets(Arg);
			break;
		case EBattleMenuCue::DeathblowHints:
			Hud->DrawDeathblowHints(Slot);
			break;
		case EBattleMenuCue::HideHints:
			Hud->HideHints();
			break;
		case EBattleMenuCue::OpenListWindow:
			Hud->OpenListWindow(static_cast<EBattleMenuList>(Arg >> 4), Arg & 0xF);
			break;
		case EBattleMenuCue::BuildItemList:
			Hud->BuildItemList(Arg != 0);
			break;
		case EBattleMenuCue::ShowListCells:
			Hud->ShowListCells();
			break;
		case EBattleMenuCue::ItemListFrame:
			Hud->DrawItemListFrame();
			break;
		case EBattleMenuCue::BuildEtherList:
			Hud->BuildEtherList(Slot);
			break;
		case EBattleMenuCue::ShowEtherCells:
			Hud->ShowEtherCells();
			break;
		case EBattleMenuCue::EtherListFrame:
			Hud->DrawEtherListFrame(Slot);
			break;
		case EBattleMenuCue::HideListWindows:
			Hud->HideListWindows(static_cast<EBattleMenuList>(Arg));
			break;
		case EBattleMenuCue::ShowListWindows:
			Hud->ShowListWindows(static_cast<EBattleMenuList>(Arg));
			break;
		case EBattleMenuCue::ShowArrows:
			Hud->ShowDirectionArrows();
			break;
		case EBattleMenuCue::FreeListWork:
			Hud->FreeListWork();
			break;
		case EBattleMenuCue::CloseListWindow:
			Hud->CloseWindow(Arg);
			break;
		case EBattleMenuCue::BuildSpecialList:
			Hud->BuildSpecialList(Slot);
			break;
		case EBattleMenuCue::SpecialListFrame:
			Hud->DrawSpecialListFrame(Slot);
			break;
		case EBattleMenuCue::BuildComboList:
			Hud->BuildComboList(Slot);
			break;
		case EBattleMenuCue::ComboListFrame:
			Hud->DrawComboListFrame();
			break;
		case EBattleMenuCue::HideComboList:
			Hud->HideComboList();
			break;
		case EBattleMenuCue::BuildAmmunition:
			Hud->BuildAmmunition(Slot);
			break;
		case EBattleMenuCue::HideAmmunition:
			Hud->HideAmmunition();
			break;
		case EBattleMenuCue::OpenCockpit:
			Hud->OpenCockpit(Slot);
			break;
		case EBattleMenuCue::CockpitFuel:
			Hud->DrawCockpitFuel(Slot);
			break;
		case EBattleMenuCue::ShowCockpit:
			Hud->ShowCockpit();
			break;
		case EBattleMenuCue::HideCockpit:
			Hud->HideCockpit();
			break;
		case EBattleMenuCue::GearHints:
			Hud->DrawGearHints(Slot, Arg);
			break;
		case EBattleMenuCue::BoardedPanel:
			Hud->BoardGear(Slot);
			break;
		case EBattleMenuCue::EventText:
			Hud->BeginEventText();
			break;
		case EBattleMenuCue::EventArrow:
			if (Arg != 0)
			{
				Hud->DrawEventArrow();
			}
			else
			{
				Hud->HideEventArrow();
			}
			break;
		case EBattleMenuCue::ResultBegin:
			Hud->BeginResult();
			break;
		case EBattleMenuCue::ResultData:
			Hud->LoadResult();
			break;
		case EBattleMenuCue::ResultScreen:
			Hud->MakeResultWork();
			break;
		case EBattleMenuCue::ResultPortraits:
			Hud->BuildResultPortraits();
			break;
		case EBattleMenuCue::ResultPanelLabels:
			Hud->BuildResultPanelLabels();
			break;
		case EBattleMenuCue::ResultHpEp:
			Hud->BuildResultHpEp();
			break;
		case EBattleMenuCue::ResultLevels:
			Hud->BuildResultLevels(Arg != 0);
			break;
		case EBattleMenuCue::ResultExpCounters:
			Hud->BuildResultExpCounters();
			break;
		case EBattleMenuCue::ResultExpGains:
			Hud->BuildResultExpGains();
			break;
		case EBattleMenuCue::ResultOpenWindow:
			Hud->OpenResultWindow(Slot);
			break;
		case EBattleMenuCue::ResultWindow:
			Hud->ShowWindow(Slot, Arg != 0);
			break;
		case EBattleMenuCue::ResultLevelUp:
			Hud->BuildResultLevelUp(Slot);
			break;
		case EBattleMenuCue::ResultLearnedPortrait:
			Hud->BuildResultLearnedPortrait(Slot);
			break;
		case EBattleMenuCue::ResultLearned:
			Hud->BuildResultLearned(Slot, Arg & GameHudLearnedSkill, (Arg & GameHudLearnedAbility) != 0);
			break;
		case EBattleMenuCue::ResultSpoils:
			Hud->BuildResultSpoils();
			break;
		case EBattleMenuCue::ActorFlash:
			if (Arg != 0)
			{
				Hud->StartActorFlash(Slot);
			}
			else
			{
				Hud->StopActorFlash();
			}
			break;
		}
	}

	void FBattleGameHud::PlayEventCue(const FBattleEventCue& Cue)
	{
		const std::array<uint16, 5>& Args = Cue.Args;
		switch (Cue.Cue)
		{
		case EBattleEventCue::ShowPortrait:
		{
			// The portrait, the message's style, then the window's place and width.
			const bool bAtRight = (Args[1] & GameHudPortraitAtRight) != 0;
			TResult<std::vector<uint8>> File = EventFiles != nullptr && Args[0] < FBattleEventFiles::PortraitCount
				? EventFiles->ReadPortrait(Args[0], bAtRight)
				: TResult<std::vector<uint8>>(MakeError("no portrait"));
			if (!File)
			{
				TableFault = EBattleHudFault::EventPortrait;
				break;
			}
			const TResult<FTim> Portrait = ParseTim(*File);
			if (!Portrait || !Portrait->Clut)
			{
				TableFault = EBattleHudFault::EventPortrait;
				break;
			}
			Hud->LoadEventPortrait(*Portrait, bAtRight, static_cast<int16>(Args[2]), static_cast<int16>(Args[3]), static_cast<int16>(Args[4]));
			break;
		}
		case EBattleEventCue::OpenWindow:
			Hud->OpenWindow(GameHudEventWindow, static_cast<int16>(Args[0]), static_cast<int16>(Args[1]), Args[2], Args[3], Args[4] != 0);
			break;
		case EBattleEventCue::CloseWindow:
			Hud->CloseWindow(GameHudEventWindow);
			break;
		case EBattleEventCue::OpenCommandHud:
			Hud->OpenCockpit(static_cast<uint8>(Args[0]));
			break;
		case EBattleEventCue::CloseCommandHud:
			Hud->CloseCommandHud();
			break;
		default:
			break;
		}
	}

	bool FBattleGameHud::HasStopped() const
	{
		return Fault() != EBattleHudFault::None;
	}

	bool FBattleGameHud::IsListCursorShown() const
	{
		return Hud && Hud->IsListCursorShown();
	}

	std::span<const FTextureRegion> FBattleGameHud::TextureRegions() const
	{
		return Hud ? Hud->TextureRegions() : std::span<const FTextureRegion>();
	}

	EBattleHudFault FBattleGameHud::Fault() const
	{
		if (TableFault != EBattleHudFault::None)
		{
			return TableFault;
		}
		return Hud ? Hud->Fault() : EBattleHudFault::None;
	}

	std::span<const uint8> FBattleGameHud::Entry(const std::optional<FMessageTable>& Table, uint8 Number)
	{
		if (Table && Table->StartsPastBytes(Number) && RecoverUnreached(Unreached, "a battle message that starts past its table, typed as none"))
		{
			return GameHudNoText;
		}
		if (!Table || Number >= Table->Count() || Table->StartsPastBytes(Number))
		{
			TableFault = EBattleHudFault::MessageOutsideTable;
			return {};
		}
		return Table->String(Number);
	}
}
