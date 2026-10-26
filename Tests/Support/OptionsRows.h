#pragma once

#include "Menu/MenuSettingsPages.h"
#include "Text/MesFont.h"

#include <vector>

namespace Weltall::Testing
{
	/** Every row's label and value side by side in the options screen's list, every help line in its strip, in the font's widths and letters. */
	bool OptionsRowsFit(const std::vector<FSettingsRow>& Rows, const FMesFont& Font);
}
