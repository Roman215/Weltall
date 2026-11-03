#include "Foundation/Sha1.h"

#include <algorithm>
#include <bit>

namespace Weltall
{
	FSha1::FSha1()
		: State{0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u}
	{
	}

	void FSha1::ProcessBlock(const uint8* Block)
	{
		std::array<uint32, 80> W{};
		for (std::size_t I = 0; I < 16; ++I)
		{
			W[I] = static_cast<uint32>(Block[I * 4]) << 24 | static_cast<uint32>(Block[I * 4 + 1]) << 16
				| static_cast<uint32>(Block[I * 4 + 2]) << 8 | static_cast<uint32>(Block[I * 4 + 3]);
		}
		for (std::size_t I = 16; I < 80; ++I)
		{
			W[I] = std::rotl(W[I - 3] ^ W[I - 8] ^ W[I - 14] ^ W[I - 16], 1);
		}
		uint32 A = State[0], B = State[1], C = State[2], D = State[3], E = State[4];
		for (std::size_t I = 0; I < 80; ++I)
		{
			uint32 F = 0;
			uint32 K = 0;
			if (I < 20)
			{
				F = (B & C) | (~B & D);
				K = 0x5A827999u;
			}
			else if (I < 40)
			{
				F = B ^ C ^ D;
				K = 0x6ED9EBA1u;
			}
			else if (I < 60)
			{
				F = (B & C) | (B & D) | (C & D);
				K = 0x8F1BBCDCu;
			}
			else
			{
				F = B ^ C ^ D;
				K = 0xCA62C1D6u;
			}
			const uint32 Temp = std::rotl(A, 5) + F + E + K + W[I];
			E = D;
			D = C;
			C = std::rotl(B, 30);
			B = A;
			A = Temp;
		}
		State[0] += A;
		State[1] += B;
		State[2] += C;
		State[3] += D;
		State[4] += E;
	}

	void FSha1::Update(std::span<const uint8> Bytes)
	{
		TotalBytes += Bytes.size();
		while (!Bytes.empty())
		{
			const std::size_t Take = std::min(Buffer.size() - Buffered, Bytes.size());
			std::copy_n(Bytes.begin(), Take, Buffer.begin() + static_cast<std::ptrdiff_t>(Buffered));
			Buffered += Take;
			Bytes = Bytes.subspan(Take);
			if (Buffered == Buffer.size())
			{
				ProcessBlock(Buffer.data());
				Buffered = 0;
			}
		}
	}

	FSha1::FDigest FSha1::Finish()
	{
		const uint64 BitLength = TotalBytes * 8;
		const uint8 Terminator = 0x80;
		Update(std::span<const uint8>(&Terminator, 1));
		const uint8 Zero = 0;
		while (Buffered != 56)
		{
			Update(std::span<const uint8>(&Zero, 1));
		}
		std::array<uint8, 8> Length{};
		for (std::size_t I = 0; I < 8; ++I)
		{
			Length[I] = static_cast<uint8>(BitLength >> (56 - I * 8));
		}
		Update(Length);
		FDigest Digest{};
		for (std::size_t I = 0; I < 5; ++I)
		{
			Digest[I * 4] = static_cast<uint8>(State[I] >> 24);
			Digest[I * 4 + 1] = static_cast<uint8>(State[I] >> 16);
			Digest[I * 4 + 2] = static_cast<uint8>(State[I] >> 8);
			Digest[I * 4 + 3] = static_cast<uint8>(State[I]);
		}
		return Digest;
	}

	FSha1::FDigest FSha1::Hash(std::span<const uint8> Bytes)
	{
		FSha1 Sha1;
		Sha1.Update(Bytes);
		return Sha1.Finish();
	}

	std::string FSha1::ToHex(const FDigest& Digest)
	{
		constexpr char Digits[] = "0123456789abcdef";
		std::string Hex;
		Hex.reserve(Digest.size() * 2);
		for (const uint8 Byte : Digest)
		{
			Hex.push_back(Digits[Byte >> 4]);
			Hex.push_back(Digits[Byte & 0x0F]);
		}
		return Hex;
	}

	std::optional<FSha1::FDigest> FSha1::FromHex(std::string_view Hex)
	{
		const auto Digit = [](char Character) -> int32
		{
			if (Character >= '0' && Character <= '9')
			{
				return Character - '0';
			}
			if (Character >= 'a' && Character <= 'f')
			{
				return Character - 'a' + 10;
			}
			return Character >= 'A' && Character <= 'F' ? Character - 'A' + 10 : -1;
		};
		FDigest Digest{};
		if (Hex.size() != Digest.size() * 2)
		{
			return std::nullopt;
		}
		for (std::size_t Byte = 0; Byte < Digest.size(); ++Byte)
		{
			const int32 High = Digit(Hex[Byte * 2]);
			const int32 Low = Digit(Hex[Byte * 2 + 1]);
			if (High < 0 || Low < 0)
			{
				return std::nullopt;
			}
			Digest[Byte] = static_cast<uint8>(High << 4 | Low);
		}
		return Digest;
	}
}
