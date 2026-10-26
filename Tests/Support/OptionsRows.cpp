#include "Support/OptionsRows.h"

#include "Text/MesGlyphs.h"

#include <optional>
#include <string_view>

namespace Weltall::Testing
{
	bool OptionsRowsFit(const std::vector<FSettingsRow>& Rows, const FMesFont& Font)
	{
		bool bInFont = true;
		const auto Width = [&Font, &bInFont](std::string_view Words)
		{
			int32 Units = 0;
			for (const char Character : Words)
			{
				const std::optional<FMesCode> Code = MesCodeOfCharacter(Character);
				bInFont = bInFont && Code.has_value();
				Units += Font.WidthUnits(Code.value_or(FMesCode{}));
			}
			return Units * 4;
		};
		for (const FSettingsRow& Row : Rows)
		{
			if (0x28 + Width(Row.Label) + 8 > 0x128 - Width(Row.Value))
			{
				return false;
			}
			for (const std::string& Line : Row.Help)
			{
				if (Width(Line) > 0x3D * 4)
				{
					return false;
				}
			}
		}
		return bInFont;
	}
}
