#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Game/PsxRandom.h"
#include "Menu/MenuGearShop.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace GameStateLayout;

		constexpr int32 FullScale = 0x1000;
		/** The camera's resting place: its distance and height, the gear's turn. */
		constexpr std::array<int32, 3> Resting = {0x400, 0, -0x400};
		constexpr std::size_t Distance = 0;
		constexpr std::size_t Height = 1;
		constexpr std::size_t Turn = 2;
		constexpr uint8 MovesDistance = 1;
		constexpr uint8 MovesHeight = 2;
		constexpr uint8 Turns = 4;
		/** The scanner's pieces: frames, and the flicker's first place. */
		constexpr uint16 NoFrame = 0xFFFF;
		constexpr int32 FirstPanelFrame = 0x172;
		constexpr int32 FirstFlickerFrame = 0x167;
		constexpr uint16 FlickerStartX = 0x18;
		constexpr uint16 FlickerStartY = 0x6E;
		constexpr int32 FlickerStepX = 8;
		constexpr int32 FlickerStepY = 10;
		/** A piece's state: closed, opening, running, closing (and the panel's last step). */
		constexpr uint8 Closed = 0;
		constexpr uint8 Opening = 1;
		constexpr uint8 Running = 2;
		constexpr uint8 Closing = 3;
		constexpr uint8 PanelClosing = 4;
		/** The gear's name: typed at (384, 72). */
		constexpr int16 NameStripX = 0x180;
		constexpr int16 NameStripY = 0x48;
		constexpr int16 NameStripWidth = 0x28;
		constexpr int16 NameRows = 0xD;
		constexpr int32 FirstGearName = 11;
		/** The gear's record: HP, max HP, fuel, max fuel, the frame's weight. */
		constexpr std::size_t WeightField = 0x68;
	}

	std::optional<FError> FMenuGearShop::ReadGearFiles(uint8 Gear)
	{
		const FGearShopTable& Gears = Resources.GearShopModels();
		if (Gear >= FGearShopTable::Gears || Gears.Entries[Gear].GearModel == FGearShopGear::NoModel)
		{
			return FError{"the gear shop shows gear " + std::to_string(Gear) + ", which has no model"};
		}
		if (Services.Image == nullptr || Services.Index == nullptr)
		{
			return FError{"the gear shop reads its gears' files from a disc it was not given"};
		}
		TResult<FGearFiles> Files = Weltall::ReadGearFiles(*Services.Image, *Services.Index, Gears.Entries[Gear].GearModel);
		if (!Files)
		{
			return Files.Error();
		}
		PendingFiles = std::move(*Files);
		return std::nullopt;
	}

	std::optional<FError> FMenuGearShop::ShowGear(uint8 Gear)
	{
		if (!PendingFiles)
		{
			return FError{"the gear shop shows a gear whose files it has not read"};
		}
		const std::optional<FError> Error = Model.Show(Resources.GearShopModels().Entries[Gear], std::move(*PendingFiles));
		PendingFiles.reset();
		if (Error)
		{
			return Error;
		}
		TakeModelVram();
		BuildStatusPanel();
		bStatusShown = true;
		return std::nullopt;
	}

	void FMenuGearShop::TakeModelVram()
	{
		// The model's textures and its scripts' copies go into the menu's VRAM, where the shop draws it.
		FGearModels& Models = Model.Models();
		const FVram::FKindScope GearsKind(MenuVram, EVramUploadKind::ModelsAndEffects);
		for (const FGearUpload& Block : Models.Uploads())
		{
			MenuVram.Upload({{Block.X, Block.Y, Block.Width, Block.Height}, Block.Pixels});
		}
		for (const FGearVramMove& Move : Models.VramMoves())
		{
			if (Move.Width <= 0 || Move.Height <= 0)
			{
				continue;
			}
			FVramImage Copy{{Move.ToX, Move.ToY, Move.Width, Move.Height}, {}};
			Copy.Units.reserve(static_cast<std::size_t>(Move.Width) * static_cast<std::size_t>(Move.Height));
			for (int32 Row = 0; Row < Move.Height; ++Row)
			{
				for (int32 Column = 0; Column < Move.Width; ++Column)
				{
					Copy.Units.push_back(MenuVram.At(Move.X + Column, Move.Y + Row));
				}
			}
			MenuVram.UploadCopy(Copy, {Move.X, Move.Y, Move.Width, Move.Height});
		}
		Models.TakeVramChanges();
	}

	TMenuTask<> FMenuGearShop::SwitchGear(bool bPrevious)
	{
		if (Camera.Moving != 0 || PilotCount < 2)
		{
			co_return;
		}
		if (bPrevious)
		{
			PilotIndex = PilotIndex - 1 < 0 ? PilotCount - 1 : PilotIndex - 1;
		}
		else
		{
			PilotIndex = PilotIndex + 1 >= PilotCount ? 0 : PilotIndex + 1;
		}
		int32 Pilot = -1;
		for (int32 Left = PilotIndex + 1; Left != 0;)
		{
			++Pilot;
			Left -= Available[static_cast<std::size_t>(Pilot)] ? 1 : 0;
		}
		co_await Frame();
		Model.Models().Free(FGearShopModel::Slot);
		co_await Frame();
		const uint8 Gear = GearOf(Pilot);
		if (const std::optional<FError> Error = ReadGearFiles(Gear))
		{
			co_await Fail(Error->Message);
		}
		CurrentGear = Gear;
		if (const std::optional<FError> Error = ShowGear(Gear))
		{
			co_await Fail(Error->Message);
		}
		co_await Frame();
	}

	void FMenuGearShop::BuildStatusPanel()
	{
		const FMenuSpriteSheet& Sheet = Resources.Sheet();
		StatusLabels.clear();
		for (std::size_t Label = 0; Label < Tables.StatusFrames.size(); ++Label)
		{
			// Each label's first piece: a label's others would lie in the next one's place.
			Scratch.clear();
			Sheet.Draw(Tables.StatusFrames[Label], Tables.StatusX[Label], Tables.StatusY[Label], FullScale, Scratch);
			if (!Scratch.empty())
			{
				StatusLabels.push_back(Scratch.front());
			}
		}
		const std::span<const uint8> Record = Services.Game->Block().subspan(GearOffset(CurrentGear), GearSize);
		// HP and max HP in five places, fuel and max fuel in four, the weight in five; the maxima packed to the left.
		const std::array<uint32, 5> Values = {LittleEndian::ReadU32(Record, Gear::Hp), LittleEndian::ReadU32(Record, Gear::MaxHp),
			LittleEndian::ReadU16(Record, Gear::Fuel), LittleEndian::ReadU16(Record, Gear::MaxFuel), LittleEndian::ReadU16(Record, WeightField)};
		constexpr std::array<int32, 5> Firsts = {4, 4, 5, 5, 4};
		constexpr std::array<bool, 5> Packed = {false, true, false, true, false};
		for (std::size_t Number = 0; Number < Values.size(); ++Number)
		{
			DrawDigitsInto(StatusNumbers[Number], Values[Number], Firsts[Number], 9 - Firsts[Number], Tables.StatusNumbers[Number][0],
				Tables.StatusNumbers[Number][1], Packed[Number]);
		}
		std::fill(NameImage.Units.begin(), NameImage.Units.end(), uint16{0});
		RenderString(NameTyping, NameImage, Services.Game->Block().subspan(static_cast<std::size_t>(FirstGearName + CurrentGear) * FGameState::NameBytes), 0);
		NameImage.Rect = {NameStripX, NameStripY, NameStripWidth, NameRows};
		Upload(NameImage);
	}

	void FMenuGearShop::PlanCameraMove()
	{
		// The distance and the height move together along the longer: a step of one unit (16.16) on it, the
		// other's in proportion; the turn is spread over as many frames as they take.
		FGearShopCamera& Shop = Camera;
		Shop.Moved = {};
		std::array<int32, 2> Lengths{};
		for (std::size_t Axis = 0; Axis < 2; ++Axis)
		{
			const int32 Length = Shop.To[Axis] - Shop.From[Axis];
			Shop.bDown[Axis] = Shop.To[Axis] < Shop.From[Axis];
			Lengths[Axis] = Length < 0 ? -Length : Length;
		}
		if (Lengths[Distance] < Lengths[Height])
		{
			Shop.Steps[Distance] = (Lengths[Distance] << 16) / Lengths[Height];
			Shop.Steps[Height] = 0x10000;
		}
		else
		{
			Shop.Steps[Height] = Lengths[Distance] == 0 ? 0 : (Lengths[Height] << 16) / Lengths[Distance];
			Shop.Steps[Distance] = 0x10000;
		}
		std::array<uint8, 2> Frames{};
		for (std::size_t Axis = 0; Axis < 2; ++Axis)
		{
			int32 Moved = 0;
			while (true)
			{
				Moved += Shop.Steps[Axis] * Shop.Pace;
				const int32 Now = Shop.bDown[Axis] ? Shop.From[Axis] - (Moved >> 16) : Shop.From[Axis] + (Moved >> 16);
				if (Shop.bDown[Axis] ? Now <= Shop.To[Axis] : Now >= Shop.To[Axis])
				{
					break;
				}
				++Frames[Axis];
			}
		}
		const uint8 Longest = std::max(Frames[Distance], Frames[Height]);
		const int32 Turned = Shop.To[Turn] - Shop.From[Turn];
		Shop.bDown[Turn] = Shop.To[Turn] < Shop.From[Turn];
		Shop.Steps[Turn] = Longest == 0 ? 0 : static_cast<int32>(static_cast<uint32>(Turned < 0 ? -Turned : Turned) << 16) / Longest;
	}

	void FMenuGearShop::StepCamera()
	{
		FGearShopCamera& Shop = Camera;
		for (std::size_t Axis = 0; Axis < 2; ++Axis)
		{
			const uint8 Bit = Axis == Distance ? MovesDistance : MovesHeight;
			if ((Shop.Moving & Bit) == 0)
			{
				continue;
			}
			Shop.Moved[Axis] += Shop.Steps[Axis] * Shop.Pace;
			const int32 Now = Shop.bDown[Axis] ? Shop.From[Axis] - (Shop.Moved[Axis] >> 16) : Shop.From[Axis] + (Shop.Moved[Axis] >> 16);
			const bool bArrived = Shop.bDown[Axis] ? Now <= Shop.To[Axis] : Now >= Shop.To[Axis];
			int32& Place = Axis == Distance ? Shop.Distance : Shop.Height;
			Place = bArrived ? Shop.To[Axis] : Now;
			if (bArrived)
			{
				Shop.Moving = static_cast<uint8>(Shop.Moving & ~Bit);
			}
		}
		if ((Shop.Moving & Turns) == 0)
		{
			return;
		}
		Shop.Moved[Turn] += Shop.Steps[Turn];
		const int32 Turned = Shop.Moved[Turn] >> 16;
		const bool bArrived = Shop.bDown[Turn] ? Shop.From[Turn] - Turned <= Shop.To[Turn] : Turned + Shop.From[Turn] >= Shop.To[Turn];
		// The gear's root turns with the camera's move; the model has one while the shop zooms.
		int16 Yaw = static_cast<int16>(Shop.To[Turn]);
		if (bArrived)
		{
			Shop.Moving = static_cast<uint8>(Shop.Moving & ~Turns);
		}
		else
		{
			Yaw = static_cast<int16>(static_cast<int16>(Shop.bDown[Turn] ? -static_cast<int16>(Turned) : static_cast<int16>(Turned)) + static_cast<int16>(Shop.From[Turn]));
		}
		if (FGearModel* Shown = Model.Models().Model(FGearShopModel::Slot); Shown != nullptr && !Shown->Bones.empty())
		{
			Shown->Bones[0].Angles[1] = Yaw;
		}
	}

	uint8 FMenuGearShop::ScanPlace() const
	{
		return Tables.ScanPlaces[static_cast<std::size_t>(Command * 4 + SubCommand) & 0xF];
	}

	TMenuTask<> FMenuGearShop::ZoomToPart()
	{
		FGearShopCamera& Shop = Camera;
		Shop.From = Shop.To;
		Shop.To[Distance] = Tables.ZoomDistance[CurrentGear % FMenuGearShopTables::Gears];
		const std::size_t HeightAt = static_cast<std::size_t>((Command + CurrentGear * 3) * 4 + SubCommand);
		Shop.To[Height] = HeightAt < Tables.ZoomHeight.size() ? Tables.ZoomHeight[HeightAt] : 0;
		Shop.To[Turn] = Tables.ZoomTurn[static_cast<std::size_t>(Command * 4 + SubCommand) & 0xF];
		PlanCameraMove();
		Shop.Moving = MovesDistance | MovesHeight | Turns;
		Scanner.bFlickerShown = true;
		Scanner.BracketStates = {Opening, Opening};
		Scanner.PanelState = Opening;
		Scanner.FlickerDelay = 0;
		Scanner.BracketDelays = {};
		Scanner.PanelDelay = 0;
		Scanner.FlickerCel = 0;
		Scanner.BracketSteps = {};
		Scanner.PanelStep = 0;
		Scanner.FlickerX = FlickerStartX;
		Scanner.FlickerY = FlickerStartY;
		Scanner.bShown = true;
		while (Scanner.BracketStates[0] != Running)
		{
			co_await Frame();
		}
		while (Scanner.PanelState != Running)
		{
			co_await Frame();
		}
	}

	void FMenuGearShop::ZoomOut()
	{
		Scanner.BracketStates = {Closing, Closing};
		Scanner.PanelState = Closing;
		Scanner.FlickerDelay = 0;
		Scanner.BracketDelays = {};
		Scanner.PanelDelay = 0;
		Scanner.bShown = true;
		FGearShopCamera& Shop = Camera;
		Shop.From = Shop.To;
		Shop.To = Resting;
		PlanCameraMove();
		Shop.Moving = MovesDistance | MovesHeight | Turns;
	}

	void FMenuGearShop::AnimateScanner()
	{
		if (!Scanner.bShown)
		{
			return;
		}
		AnimateBrackets();
		AnimatePanel();
		AnimateFlicker();
	}

	void FMenuGearShop::AnimateBrackets()
	{
		FPsxRandom& Random = *Services.Random;
		for (std::size_t Bracket = 0; Bracket < Scanner.Brackets.size(); ++Bracket)
		{
			uint8& State = Scanner.BracketStates[Bracket];
			if (State == Closed)
			{
				continue;
			}
			// Opening: at the choice's place; running: now and then somewhere else along its side.
			if (State == Opening)
			{
				Scanner.BracketX[Bracket] = static_cast<uint16>(Tables.BracketX[Bracket]);
				Scanner.BracketY[Bracket] = static_cast<uint16>(Tables.BracketY[Bracket][ScanPlace() % 6]);
			}
			else if (State == Running && RandomByteBetween(Random, 0, 0xFF) < 0x10)
			{
				Scanner.BracketX[Bracket] = static_cast<uint16>(Tables.BracketX[Bracket]);
				Scanner.BracketY[Bracket] = static_cast<uint16>(Tables.BracketY[Bracket][RandomByteBetween(Random, 0, 5)]);
			}
			uint8& Delay = Scanner.BracketDelays[Bracket];
			if (Delay != 0)
			{
				--Delay;
				continue;
			}
			std::vector<FMenuPrimitive>& Pieces = Scanner.Brackets[Bracket];
			Pieces.clear();
			int16& Step = Scanner.BracketSteps[Bracket];
			for (std::size_t Part = 0; Part < 4; ++Part)
			{
				const uint16 Frame = Tables.BracketFrames[Bracket][static_cast<std::size_t>(Step) * 4 + Part];
				if (Frame != NoFrame)
				{
					Resources.Sheet().Draw(Frame, Scanner.BracketX[Bracket], Scanner.BracketY[Bracket], FullScale, Pieces);
				}
			}
			if (State == Running)
			{
				Step = static_cast<int16>(Step + 1 > 4 ? 3 : Step + 1);
				Delay = 8;
			}
			else if (State == Opening)
			{
				if (++Step == 4)
				{
					State = Running;
				}
				Delay = 2;
			}
			else if (State == Closing)
			{
				if (--Step < 0)
				{
					State = Closed;
					Scanner.bFlickerShown = false;
				}
				Delay = 2;
			}
		}
	}

	void FMenuGearShop::AnimatePanel()
	{
		FPsxRandom& Random = *Services.Random;
		uint8& State = Scanner.PanelState;
		if (State == Closed)
		{
			return;
		}
		if (Scanner.PanelDelay != 0)
		{
			--Scanner.PanelDelay;
			return;
		}
		if (State == Opening)
		{
			const std::size_t Place = ScanPlace() % 6;
			Scanner.PanelX = static_cast<uint16>(Tables.PanelX[Place]);
			Scanner.PanelY = static_cast<uint16>(Tables.PanelY[Place]);
		}
		else if (State == Running && RandomByteBetween(Random, 0, 0xFF) <= 3)
		{
			Scanner.PanelX = static_cast<uint16>(Tables.PanelX[RandomByteBetween(Random, 0, 5)]);
			Scanner.PanelY = static_cast<uint16>(Tables.PanelY[RandomByteBetween(Random, 0, 5)]);
		}
		Scanner.Panel.clear();
		Resources.Sheet().Draw(FirstPanelFrame + Scanner.PanelStep, Scanner.PanelX, Scanner.PanelY, FullScale, Scanner.Panel);
		Scanner.PanelDelay = 1;
		int16& Step = Scanner.PanelStep;
		switch (State)
		{
		case Running:
			Step = static_cast<int16>(Step + 1 > 10 ? 3 : Step + 1);
			break;
		case Opening:
			if (++Step > 2)
			{
				State = Running;
			}
			break;
		case Closing:
			Step = 2;
			State = PanelClosing;
			break;
		case PanelClosing:
			if (--Step < 0)
			{
				State = Closed;
			}
			break;
		default:
			break;
		}
	}

	void FMenuGearShop::AnimateFlicker()
	{
		FPsxRandom& Random = *Services.Random;
		if (!Scanner.bFlickerShown)
		{
			return;
		}
		if (Scanner.FlickerDelay != 0)
		{
			--Scanner.FlickerDelay;
			return;
		}
		if (RandomByteBetween(Random, 0, 0xFF) < 8)
		{
			Scanner.FlickerX = static_cast<uint16>(Tables.FlickerX[RandomByteBetween(Random, 0, 5)]);
			Scanner.FlickerY = static_cast<uint16>(Tables.FlickerY[RandomByteBetween(Random, 0, 5)]);
		}
		for (std::size_t Mark = 0; Mark < Scanner.Flicker.size(); ++Mark)
		{
			Scanner.Flicker[Mark].clear();
			Resources.Sheet().Draw(FirstFlickerFrame + Scanner.FlickerCel, Scanner.FlickerX + static_cast<int32>(Mark) * FlickerStepX,
				Scanner.FlickerY + static_cast<int32>(Mark) * FlickerStepY, FullScale, Scanner.Flicker[Mark]);
		}
		Scanner.FlickerCel ^= 1;
		Scanner.FlickerDelay = 4;
	}
}
