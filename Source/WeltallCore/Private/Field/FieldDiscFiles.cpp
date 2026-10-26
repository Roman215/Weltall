#include "Field/FieldDiscFiles.h"

#include "Media/GameFileIndex.h"

#include <utility>

namespace Weltall
{
	bool FFieldDiscFiles::ReadFile(int32 Directory, int32 File, std::vector<uint8>& Bytes)
	{
		const FIndexRecord* Record = Image != nullptr && Index != nullptr ? Index->Find(Directory, File) : nullptr;
		if (Record == nullptr)
		{
			return false;
		}
		bCueRead = bCueRead || Record == Cue;
		TResult<std::vector<uint8>> Sectors = ReadFileSectors(*Image, *Record);
		if (!Sectors)
		{
			return false;
		}
		Bytes = std::move(*Sectors);
		return true;
	}
}
