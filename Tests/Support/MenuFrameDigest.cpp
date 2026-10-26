#include "Support/MenuFrameDigest.h"

#include "Foundation/Sha1.h"
#include "Gears/GearShopModel.h"
#include "Support/GearDigest.h"
#include "Support/OracleDigest.h"

namespace Weltall::Testing
{
	namespace
	{
		void Put16(std::vector<uint8>& Bytes, uint32 Value)
		{
			Bytes.push_back(static_cast<uint8>(Value));
			Bytes.push_back(static_cast<uint8>(Value >> 8));
		}

		void Put32(std::vector<uint8>& Bytes, uint32 Value)
		{
			Put16(Bytes, Value & 0xFFFF);
			Put16(Bytes, Value >> 16);
		}

		void PutPacket(std::vector<uint8>& Bytes, const FMenuPrimitive& Primitive)
		{
			std::vector<uint8> Packet{Primitive.Code};
			if (Primitive.IsPolygon())
			{
				const bool bGouraud = (Primitive.Code & MenuGpu::Gouraud) != 0;
				const bool bTextured = (Primitive.Code & MenuGpu::Textured) != 0;
				const bool bRaw = (Primitive.Code & MenuGpu::RawTexture) != 0;
				for (int32 Corner = 0; Corner < Primitive.PolygonCorners(); ++Corner)
				{
					const FMenuCorner& At = Primitive.Corners[static_cast<std::size_t>(Corner)];
					if ((Corner == 0 || bGouraud) && !(bTextured && bRaw))
					{
						Packet.insert(Packet.end(), {At.Color.R, At.Color.G, At.Color.B});
					}
					Put16(Packet, static_cast<uint16>(At.X));
					Put16(Packet, static_cast<uint16>(At.Y));
					if (bTextured)
					{
						Packet.insert(Packet.end(), {At.U, At.V});
					}
				}
				if (bTextured)
				{
					Put16(Packet, Primitive.Clut);
					Put16(Packet, Primitive.Page);
				}
			}
			else if (Primitive.IsLine())
			{
				const bool bGouraud = (Primitive.Code & MenuGpu::Gouraud) != 0;
				for (int32 Corner = 0; Corner < Primitive.LineCorners(); ++Corner)
				{
					const FMenuCorner& At = Primitive.Corners[static_cast<std::size_t>(Corner)];
					if (Corner == 0 || bGouraud)
					{
						Packet.insert(Packet.end(), {At.Color.R, At.Color.G, At.Color.B});
					}
					Put16(Packet, static_cast<uint16>(At.X));
					Put16(Packet, static_cast<uint16>(At.Y));
				}
			}
			else if (Primitive.IsRectangle())
			{
				const bool bTextured = (Primitive.Code & MenuGpu::Textured) != 0;
				const FMenuCorner& At = Primitive.Corners[0];
				if (!(bTextured && (Primitive.Code & MenuGpu::RawTexture) != 0))
				{
					Packet.insert(Packet.end(), {At.Color.R, At.Color.G, At.Color.B});
				}
				Put16(Packet, static_cast<uint16>(At.X));
				Put16(Packet, static_cast<uint16>(At.Y));
				if (bTextured)
				{
					Packet.insert(Packet.end(), {At.U, At.V});
					Put16(Packet, Primitive.Clut);
				}
				Put16(Packet, static_cast<uint16>(Primitive.Width));
				Put16(Packet, static_cast<uint16>(Primitive.Height));
			}
			else
			{
				for (const uint32 Word : Primitive.Words)
				{
					Put32(Packet, Word);
				}
			}
			Put16(Bytes, static_cast<uint32>(Packet.size()));
			Bytes.insert(Bytes.end(), Packet.begin(), Packet.end());
		}
	}

	std::vector<uint8> CanonicalMenuPackets(const FMenuFrame& Frame)
	{
		std::vector<uint8> Bytes;
		for (const FMenuPrimitive& Primitive : Frame.Primitives)
		{
			PutPacket(Bytes, Primitive);
		}
		return Bytes;
	}

	std::vector<uint8> GearShopFrameBytes(const FMenuFrame& Frame, const FGearShopModel& Model)
	{
		std::vector<uint8> Bytes{'g', 'e', 'a', 'r'};
		for (const std::array<int16, 3>& Row : Frame.Gear.Camera.M)
		{
			for (const int16 Value : Row)
			{
				Put16(Bytes, static_cast<uint16>(Value));
			}
		}
		Put16(Bytes, 0);
		for (const int32 Value : Frame.Gear.Camera.T)
		{
			Put32(Bytes, static_cast<uint32>(Value));
		}
		FOracleDigest Digest;
		DigestGearModels(Digest, Model.Models());
		const FSha1::FDigest Sum = Digest.Finish();
		Bytes.insert(Bytes.end(), Sum.begin(), Sum.end());
		return Bytes;
	}

	std::vector<uint8> MenuFrameBytes(const FMenuFrame& Frame, std::span<const uint8> Sounds, std::span<const FVramImage> Uploads, std::span<const uint8> Gear)
	{
		std::vector<uint8> Bytes = CanonicalMenuPackets(Frame);
		for (const char Letter : std::string("sounds"))
		{
			Bytes.push_back(static_cast<uint8>(Letter));
		}
		for (const uint8 Sound : Sounds)
		{
			Put16(Bytes, Sound);
		}
		for (const FVramImage& Upload : Uploads)
		{
			for (const char Letter : std::string("upload"))
			{
				Bytes.push_back(static_cast<uint8>(Letter));
			}
			for (const int16 Value : {Upload.Rect.X, Upload.Rect.Y, Upload.Rect.Width, Upload.Rect.Height})
			{
				Put16(Bytes, static_cast<uint16>(Value));
			}
			for (const uint16 Unit : Upload.Units)
			{
				Put16(Bytes, Unit);
			}
		}
		Bytes.insert(Bytes.end(), Gear.begin(), Gear.end());
		return Bytes;
	}

	std::string MenuFrameDigest(const FMenuFrame& Frame, std::span<const uint8> Sounds, std::span<const FVramImage> Uploads, std::span<const uint8> Gear)
	{
		FSha1 Hash;
		Hash.Update(MenuFrameBytes(Frame, Sounds, Uploads, Gear));
		return FSha1::ToHex(Hash.Finish()).substr(0, 8);
	}
}
