#include "Game/SaveState.h"

#include "Foundation/Crc32.h"
#include "Foundation/LittleEndian.h"
#include "Game/SaveStateMembers.h"

#include <algorithm>
#include <array>
#include <type_traits>

namespace Weltall
{
	namespace
	{
		constexpr std::array<uint8, 8> Mark{'W', 'E', 'L', 'T', 'S', 'T', 'A', 'T'};
		/** The mark, the version and the checksum of what follows. */
		constexpr std::size_t HeaderBytes = Mark.size() + 4 + 4;
		constexpr std::size_t VersionAt = Mark.size();
		constexpr std::size_t ChecksumAt = VersionAt + 4;

		/** The unsigned number of a member's width that a file keeps it as. */
		template <typename T>
		auto StoredNumber()
		{
			if constexpr (std::is_enum_v<T>)
			{
				return std::make_unsigned_t<std::underlying_type_t<T>>{};
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				return uint8{};
			}
			else
			{
				return std::make_unsigned_t<T>{};
			}
		}

		template <typename T>
		using TStored = decltype(StoredNumber<T>());

		/** Each number little-endian in its own width, a bool a byte, a list's length four bytes. */
		class FStateWriter
		{
		public:
			explicit FStateWriter(std::vector<uint8>& InBytes) : Bytes(InBytes) {}

			template <typename T>
			void operator()(T& Member)
			{
				SaveStateMembers::Visit(*this, Member);
			}

			template <typename T>
			void Number(const T& Value)
			{
				const auto Stored = static_cast<TStored<T>>(Value);
				for (std::size_t Byte = 0; Byte < sizeof(Stored); ++Byte)
				{
					Bytes.push_back(static_cast<uint8>(Stored >> (8 * Byte)));
				}
			}

			void Count(uint32& Value) { Number(Value); }

			template <typename T>
			void Skip(T&)
			{
			}

		private:
			std::vector<uint8>& Bytes;
		};

		/** The writer's reading: a list longer than the bytes left, or the bytes running out, marks the state damaged. */
		class FStateReader
		{
		public:
			explicit FStateReader(std::span<const uint8> InBytes) : Bytes(InBytes) {}

			template <typename T>
			void operator()(T& Member)
			{
				SaveStateMembers::Visit(*this, Member);
			}

			template <typename T>
			void Number(T& Value)
			{
				using FStored = TStored<T>;
				if (Bytes.size() - At < sizeof(FStored))
				{
					bDamaged = true;
					At = Bytes.size();
					Value = T{};
					return;
				}
				FStored Stored = 0;
				for (std::size_t Byte = 0; Byte < sizeof(FStored); ++Byte)
				{
					Stored = static_cast<FStored>(Stored | static_cast<FStored>(Bytes[At + Byte]) << (8 * Byte));
				}
				At += sizeof(FStored);
				if constexpr (std::is_same_v<T, bool>)
				{
					Value = Stored != 0;
				}
				else
				{
					Value = static_cast<T>(Stored);
				}
			}

			void Count(uint32& Value)
			{
				Number(Value);
				// Every element takes a byte at least.
				if (Value > Bytes.size() - At)
				{
					bDamaged = true;
					Value = 0;
				}
			}

			template <typename T>
			void Skip(T& Member)
			{
				Member = T{};
			}

			bool IsWhole() const { return !bDamaged && At == Bytes.size(); }

		private:
			std::span<const uint8> Bytes;
			std::size_t At = 0;
			bool bDamaged = false;
		};
	}

	std::vector<uint8> WriteSaveState(const FSaveState& State)
	{
		std::vector<uint8> Bytes(HeaderBytes);
		std::copy(Mark.begin(), Mark.end(), Bytes.begin());
		LittleEndian::WriteU32(Bytes, VersionAt, FSaveState::FormatVersion);
		FSaveState Copy = State;
		FStateWriter Writer(Bytes);
		Writer(Copy);
		LittleEndian::WriteU32(Bytes, ChecksumAt, Crc32(std::span<const uint8>(Bytes).subspan(HeaderBytes)));
		return Bytes;
	}

	std::optional<FSaveState> ReadSaveState(std::span<const uint8> Bytes, ESaveStateFault& Fault)
	{
		if (Bytes.size() < HeaderBytes || !std::equal(Mark.begin(), Mark.end(), Bytes.begin()))
		{
			Fault = ESaveStateFault::NotAState;
			return std::nullopt;
		}
		if (LittleEndian::ReadU32(Bytes, VersionAt) != FSaveState::FormatVersion)
		{
			Fault = ESaveStateFault::OtherVersion;
			return std::nullopt;
		}
		const std::span<const uint8> Members = Bytes.subspan(HeaderBytes);
		if (LittleEndian::ReadU32(Bytes, ChecksumAt) != Crc32(Members))
		{
			Fault = ESaveStateFault::Damaged;
			return std::nullopt;
		}
		FSaveState State;
		FStateReader Reader(Members);
		Reader(State);
		if (!Reader.IsWhole())
		{
			Fault = ESaveStateFault::Damaged;
			return std::nullopt;
		}
		return State;
	}
}
