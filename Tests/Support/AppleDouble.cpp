#include "Support/AppleDouble.h"

#include "Foundation/Types.h"

#include <fstream>
#include <string_view>

namespace Weltall::Testing
{
	namespace
	{
		void Put32(std::string& Bytes, std::size_t Offset, uint32 Value)
		{
			for (int32 Byte = 0; Byte < 4; ++Byte)
			{
				Bytes[Offset + Byte] = static_cast<char>(Value >> (24 - 8 * Byte));
			}
		}

		void Put(std::string& Bytes, std::size_t Offset, std::string_view Text)
		{
			Bytes.replace(Offset, Text.size(), Text);
		}
	}

	std::string AppleDoubleCompanion()
	{
		constexpr uint32 FinderInfo = 0x32;
		constexpr uint32 ResourceFork = 0xEE2;
		std::string Bytes(0x1000, '\0');
		Put32(Bytes, 0x00, 0x00051607);
		Put32(Bytes, 0x04, 0x00020000);
		Put(Bytes, 0x08, "Mac OS X        ");
		Bytes[0x19] = 2;
		Put32(Bytes, 0x1A, 9);
		Put32(Bytes, 0x1E, FinderInfo);
		Put32(Bytes, 0x22, ResourceFork - FinderInfo);
		Put32(Bytes, 0x26, 2);
		Put32(Bytes, 0x2A, ResourceFork);
		Put32(Bytes, 0x2E, 0x1000 - ResourceFork);
		// The extended attributes follow the Finder's 32 bytes and two of padding.
		Put(Bytes, 0x54, "ATTR");
		Put32(Bytes, 0x5C, ResourceFork);
		Put32(Bytes, 0x60, 0x120);
		Put32(Bytes, 0x64, 0x16);
		Bytes[0x73] = 1;
		Put32(Bytes, 0x74, 0x120);
		Put32(Bytes, 0x78, 0x16);
		Bytes[0x7F] = 0x15;
		Put(Bytes, 0x80, "com.apple.quarantine");
		Put(Bytes, 0x120, "0083;6718a2c4;Safari;");
		Put32(Bytes, ResourceFork, 0x100);
		Put32(Bytes, ResourceFork + 4, 0x100);
		Put32(Bytes, ResourceFork + 12, 0x1E);
		Put(Bytes, ResourceFork + 0x10, "This resource fork intentionally left blank   ");
		Put32(Bytes, ResourceFork + 0x100, 0x100);
		Put32(Bytes, ResourceFork + 0x104, 0x100);
		Put32(Bytes, ResourceFork + 0x10C, 0x1E);
		Bytes[ResourceFork + 0x119] = 0x1E;
		Bytes[ResourceFork + 0x11B] = 0x1E;
		Bytes[ResourceFork + 0x11C] = static_cast<char>(0xFF);
		Bytes[ResourceFork + 0x11D] = static_cast<char>(0xFF);
		return Bytes;
	}

	void WriteAppleDoubleCompanion(const std::filesystem::path& File)
	{
		const std::u8string Name = u8"._" + File.filename().u8string();
		std::ofstream(File.parent_path() / Name, std::ios::binary) << AppleDoubleCompanion();
	}
}
