#include "Game/SaveStateSlots.h"

#include "Foundation/WholeFiles.h"
#include "Menu/OptionsText.h"

#include <cstdio>

namespace Weltall
{
	namespace
	{
		namespace Text = OptionsText::States;

		constexpr uint32 VblanksPerMinute = 60 * 60;
		constexpr uint32 MinutesPerHour = 60;
	}

	std::string StatePlayTime(uint32 Vblanks)
	{
		const uint32 Minutes = Vblanks / VblanksPerMinute;
		char Text[16];
		std::snprintf(Text, sizeof Text, "%u:%02u", Minutes / MinutesPerHour, Minutes % MinutesPerHour);
		return Text;
	}

	FSaveStateSlots::FSaveStateSlots(std::filesystem::path InFolder) : Folder(std::move(InFolder))
	{
	}

	std::filesystem::path FSaveStateSlots::File(int32 InSlot) const
	{
		return Folder / ("Slot " + std::to_string(InSlot) + ".state");
	}

	FSaveStateTold FSaveStateSlots::Step(int32 By)
	{
		Slot = (Slot - 1 + By % SlotCount + SlotCount) % SlotCount + 1;
		const std::string Number = std::to_string(Slot);
		FSaveStateTold Told;
		if (const std::optional<FSaveState> State = Read(Told))
		{
			return {OptionsText::Fill(Text::Chosen, {Number, StatePlayTime(State->Game.PlayTime())}), {}};
		}
		return {OptionsText::Fill(Text::ChosenEmpty, {Number}), {}};
	}

	FSaveStateTold FSaveStateSlots::Save(FGameLoop& Loop, FGameSession& Session)
	{
		const std::string Number = std::to_string(Slot);
		TResult<std::optional<FSaveState>> Captured = CaptureSaveState(Loop, Session);
		if (!Captured)
		{
			return {OptionsText::Fill(Text::NotWritten, {Number}), Captured.Error().Message};
		}
		if (!*Captured)
		{
			return {std::string(Text::CannotSave), {}};
		}
		if (const std::optional<FError> Failed = ReplaceWholeFile(File(Slot), WriteSaveState(**Captured)))
		{
			return {OptionsText::Fill(Text::NotWritten, {Number}), Failed->Message};
		}
		return {OptionsText::Fill(Text::Saved, {Number}), {}};
	}

	std::optional<FSaveState> FSaveStateSlots::Read(FSaveStateTold& Told) const
	{
		const std::string Number = std::to_string(Slot);
		TResult<std::optional<std::vector<uint8>>> Bytes = ReadWholeFile(File(Slot));
		if (!Bytes)
		{
			Told = {OptionsText::Fill(Text::Unreadable, {Number}), Bytes.Error().Message};
			return std::nullopt;
		}
		if (!*Bytes)
		{
			Told = {OptionsText::Fill(Text::Empty, {Number}), {}};
			return std::nullopt;
		}
		ESaveStateFault Fault = ESaveStateFault::NotAState;
		std::optional<FSaveState> State = ReadSaveState(**Bytes, Fault);
		if (!State)
		{
			const bool bOtherVersion = Fault == ESaveStateFault::OtherVersion;
			Told = {OptionsText::Fill(bOtherVersion ? Text::OtherVersion : Text::Unreadable, {Number}),
				"the state " + File(Slot).string() + (bOtherVersion ? " is of another version" : " is damaged")};
		}
		return State;
	}

	std::string FSaveStateSlots::Loaded() const
	{
		return OptionsText::Fill(Text::Loaded, {std::to_string(Slot)});
	}

	std::string FSaveStateSlots::NeedsDisc(int32 Disc) const
	{
		return OptionsText::Fill(Text::NeedsDisc, {std::to_string(Slot), std::to_string(Disc + 1)});
	}

	std::string FSaveStateSlots::OtherFiles() const
	{
		return OptionsText::Fill(Text::OtherFiles, {std::to_string(Slot)});
	}
}
