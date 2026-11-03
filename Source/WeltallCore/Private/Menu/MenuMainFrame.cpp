#include "Game/GameState.h"
#include "Game/PsxRandom.h"
#include "Game/SaveStore.h"
#include "Menu/MenuMain.h"

namespace Weltall
{
	namespace
	{
		/** L1, R1, Start and Select held together, and nothing else: the console starts again. */
		constexpr uint16 SoftResetButtons = 0x090C;
		/** The ordering table's entry the menu links its pieces into. */
		constexpr int32 PiecesEntry = 4;
		/** The play time's digits, in vertical blanks counted 60 to a second. */
		constexpr std::array<uint32, 6> PlayTimeUnits = {21600000, 2160000, 216000, 36000, 3600, 600};
		constexpr uint32 VblanksPerSecond = 60;

		namespace Effect
		{
			constexpr uint8 Cursor = 1;
			constexpr uint8 Confirm = 2;
			constexpr uint8 Cancel = 3;
		}

		/** The main module's input: the directions and shoulder buttons repeat, each with its sound. */
		constexpr std::array<FMenuInputTest, 11> MainInput = {{
			{MenuButton::Right, true, EMenuCode::Right, Effect::Cursor},
			{MenuButton::Down, true, EMenuCode::Down, Effect::Cursor},
			{MenuButton::Left, true, EMenuCode::Left, Effect::Cursor},
			{MenuButton::Up, true, EMenuCode::Up, Effect::Cursor},
			{MenuButton::Circle, false, EMenuCode::Confirm, Effect::Confirm},
			{MenuButton::Cross, false, EMenuCode::Cancel, Effect::Cancel},
			{MenuButton::Square, false, EMenuCode::Square, 0},
			{MenuButton::Triangle, false, EMenuCode::Triangle, 0},
			{MenuButton::L1, true, EMenuCode::L1, Effect::Cursor},
			{MenuButton::R1, true, EMenuCode::R1, Effect::Cursor},
			{MenuButton::Select, false, EMenuCode::Select, 0},
		}};
	}

	void FMenuMain::DrawFrame()
	{
		ReadInput(MainInput);
		if (bSoftResetAllowed && PadHeld == SoftResetButtons)
		{
			bSoftReset = true;
			return;
		}
		Buffer ^= 1;
		RandomByteBetween(*Services.Random, 0, 0xFF);
		UpdateTransition();
		LayerGte = MenuProjection(Layer);
		++IdleFrames;
		PlayTimeDigits = SplitPlayTime(Services.Game->PlayTime());
		if (Flags[MenuFlag::PlayTime] != 0)
		{
			// The time is drawn again each frame, so it runs while the menu is open.
			DrawPlayTime(PlayTimeX, PlayTimeY);
		}
		DrawScene();
		Drawn.Primitives.clear();
		Table.TakeInto(Drawn.Primitives);
		Drawn.Layer = Layer;
		Drawn.Stores = {Flags[MenuFlag::Cards] != 0 && Files.Drawn != 0, Files.FirstStore, FSaveStore::StoreCount};
		if (Options.bShown)
		{
			Drawn.Keys = Options.Keys;
		}
		else
		{
			Drawn.Keys.clear();
		}
		++Drawn.Number;
		UpdateStoreLists();
	}

	std::array<uint32, 7> FMenuMain::SplitPlayTime(uint32 Time)
	{
		std::array<uint32, 7> Digits{};
		for (std::size_t Digit = 0; Digit < PlayTimeUnits.size(); ++Digit)
		{
			Digits[Digit] = Time / PlayTimeUnits[Digit];
			Time %= PlayTimeUnits[Digit];
		}
		Digits.back() = Time / VblanksPerSecond;
		return Digits;
	}

	void FMenuMain::DrawScene()
	{
		if (bDrawScene)
		{
			if (Request.Mode == EMenuMode::LoadGame)
			{
				DrawTitle();
			}
			else if (Request.Mode == EMenuMode::Normal)
			{
				DrawNormalMenu();
			}
			else if (Request.Mode == EMenuMode::DiscChange || Request.Mode == EMenuMode::ClearFile)
			{
				DrawDiscChange();
			}
		}
		DrawBackground();
	}

	void FMenuMain::DrawTitle()
	{
		GrowWindows();
		AddLabels();
		AddTargetPointers();
		AddSelection();
		DrawFileDetails();
		DrawCards();
		AddCursors();
		AddOptionsScreen();
		AddScrollKnob();
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

	void FMenuMain::AddTargetPointers()
	{
		if (Flags[MenuFlag::TargetPointers] == 0)
		{
			return;
		}
		for (std::size_t Target = 0; Target < TargetPointers.size(); ++Target)
		{
			if (TargetPointerShown[Target] && !TargetPointers[Target].empty())
			{
				Table.Add(PiecesEntry, TargetPointers[Target].front());
			}
		}
	}

	void FMenuMain::DrawBackground()
	{
		AddBackdrop();
	}

	void FMenuMain::AddLabels()
	{
		for (std::size_t Label = 0; Label < HelpLabels.size(); ++Label)
		{
			if (Flags[MenuFlag::HelpLabels + Label] != 0)
			{
				Table.Add(PiecesEntry, HelpLabels[Label].Quad);
			}
		}
		// The status sheet's labels: only its headings are drawn (on the layer), the inner arc's are not.
		for (std::size_t Label = 3; Label < SheetLabels.size(); ++Label)
		{
			if (Flags[MenuFlag::SheetLabels + Label] != 0)
			{
				AddProjected(SheetLabels[Label].Quad, true, PiecesEntry);
			}
		}
		for (std::size_t Label = 0; Label < GearLabels.size(); ++Label)
		{
			if (Flags[MenuFlag::GearLabels + Label] != 0 && GearLabelOnLayer[Label])
			{
				AddProjected(GearLabels[Label].Quad, true, PiecesEntry);
			}
		}
		for (std::size_t Label = 0; Label < SubArcLabels.size(); ++Label)
		{
			if (Flags[MenuFlag::SubArcLabels + Label] != 0)
			{
				Table.Add(PiecesEntry, SubArcLabels[Label].Quad);
			}
		}
		for (std::size_t Label = 0; Label < HelpTags.size(); ++Label)
		{
			if (Flags[MenuFlag::HelpTags + Label] != 0)
			{
				AddProjected(HelpTags[Label].Quad, true, PiecesEntry);
			}
		}
		for (std::size_t Label = 0; Label < EquipSlotLabels.size(); ++Label)
		{
			if (Flags[MenuFlag::EquipSlotLabels + Label] != 0)
			{
				AddProjected(EquipSlotLabels[Label].Quad, true, PiecesEntry);
			}
		}
		if (Flags[MenuFlag::SkillsTitle] != 0)
		{
			Table.Add(PiecesEntry, SkillsLabels[0].Quad);
		}
		if (bMessageShown)
		{
			for (FMenuLabel& Line : MessageLines)
			{
				AddProjected(Line.Quad, true, PiecesEntry);
			}
		}
		if (bOfferShown)
		{
			for (FMenuLabel& Line : OfferLines)
			{
				AddProjected(Line.Quad, true, PiecesEntry);
			}
		}
	}

}
