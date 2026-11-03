#include "Menu/MenuNewGamePlusPage.h"

#include "Menu/OptionsText.h"

#include <array>

namespace Weltall
{
	namespace
	{
		namespace Text = OptionsText::NewGamePlus;

		struct FPart
		{
			const OptionsText::FRow& Row;
			bool FNewGamePlusCarry::* Carried;
		};

		/** The parts kept or reset, in their rows' order; the Limiter's row after them. */
		constexpr std::array<FPart, FMenuNewGamePlusPage::LimiterRow> Parts{{
			{Text::Money, &FNewGamePlusCarry::bMoney},
			{Text::Equipment, &FNewGamePlusCarry::bEquipment},
			{Text::Items, &FNewGamePlusCarry::bItems},
			{Text::Deathblows, &FNewGamePlusCarry::bDeathblows},
			{Text::Levels, &FNewGamePlusCarry::bLevels},
		}};

		std::array<std::string, 3> Help(const std::array<std::string_view, 3>& Lines)
		{
			return {std::string(Lines[0]), std::string(Lines[1]), std::string(Lines[2])};
		}
	}

	std::string_view FMenuNewGamePlusPage::Title()
	{
		return Text::Title;
	}

	std::vector<FSettingsRow> FMenuNewGamePlusPage::Rows() const
	{
		std::vector<FSettingsRow> Rows;
		for (const FPart& Part : Parts)
		{
			Rows.push_back({std::string(Part.Row.Label), std::string(Chosen.*Part.Carried ? Text::Keep : Text::Reset), Help(Part.Row.Help)});
		}
		Rows.push_back({std::string(Text::Limiter), std::string(Chosen.bLimiter ? Text::Removed : Text::Intact), Help(Chosen.bLimiter ? Text::RemovedHelp : Text::IntactHelp)});
		Rows.push_back({std::string(Text::Elly), std::string(Chosen.bKeepElly ? Text::Returns : Text::Leaves), Help(Chosen.bKeepElly ? Text::ReturnsHelp : Text::LeavesHelp)});
		Rows.push_back({std::string(Text::Start.Label), std::string(), Help(Text::Start.Help)});
		return Rows;
	}

	bool FMenuNewGamePlusPage::Switch(int32 Row)
	{
		if (Row < 0 || Row >= StartRow)
		{
			return false;
		}
		bool& Carried = Row == LimiterRow ? Chosen.bLimiter : Row == EllyRow ? Chosen.bKeepElly : Chosen.*Parts[static_cast<std::size_t>(Row)].Carried;
		Carried = !Carried;
		return true;
	}
}
