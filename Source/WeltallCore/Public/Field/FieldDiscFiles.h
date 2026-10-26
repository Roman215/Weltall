#pragma once

#include "Field/EventContext.h"

#include <utility>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	struct FIndexRecord;

	/** The field's files as the disc holds them, read whole and at once. */
	class FFieldDiscFiles final : public IFieldFiles
	{
	public:
		/**
		 * Either may be null: nothing can then be read. Both must outlive the files. Cue, a record of the index or null,
		 * is the file whose read the kernel shows a screen of its own before (FFieldServices::TranslationCreditsCue).
		 */
		FFieldDiscFiles(const FCdImage* InImage, const FGameFileIndex* InIndex, const FIndexRecord* InCue = nullptr)
			: Image(InImage), Index(InIndex), Cue(InCue)
		{
		}

		WELTALLCORE_API bool ReadFile(int32 Directory, int32 File, std::vector<uint8>& Bytes) override;

		/** Whether the cue was read since the last time asked. */
		bool TakeCueRead() { return std::exchange(bCueRead, false); }

	private:
		const FCdImage* Image = nullptr;
		const FGameFileIndex* Index = nullptr;
		const FIndexRecord* Cue = nullptr;
		bool bCueRead = false;
	};
}
