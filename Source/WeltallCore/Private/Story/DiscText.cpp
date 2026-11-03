#include "Story/DiscText.h"

#include "Battle/BattleEvent.h"
#include "Field/FieldMap.h"

namespace Weltall
{
	FGameDiscText::FGameDiscText(const FCdImage& InImage, const FGameFileIndex& InIndex) : Image(InImage), Index(InIndex)
	{
	}

	FGameDiscText::~FGameDiscText() = default;

	std::span<const uint8> FGameDiscText::BattleEventMessages(int32 Event) const
	{
		if (!bRead)
		{
			bRead = true;
			if (TResult<FBattleEventArchive> Read = FBattleEventArchive::Load(Image, Index))
			{
				Events = std::make_unique<FBattleEventArchive>(std::move(*Read));
			}
		}
		return Events != nullptr && Event >= 0 && Event < Events->Count() ? Events->Messages(Event) : std::span<const uint8>();
	}

	std::span<const uint8> FGameDiscText::FieldMapMessages(int32 Map) const
	{
		auto [Table, bNew] = Maps.try_emplace(Map);
		if (bNew)
		{
			if (TResult<std::vector<uint8>> Read = LoadFieldMapMessages(Image, Index, Map))
			{
				Table->second = std::move(*Read);
			}
		}
		return Table->second;
	}
}
