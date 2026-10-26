#include "Input/SwitchGamepadSetup.h"

#include <algorithm>
#include <optional>

namespace Weltall
{
	namespace
	{
		constexpr uint32 ReplyWait = 100;
		constexpr int32 Attempts = 5;

		/** The setup's requests and the replies awaited for them. */
		class FConversation
		{
		public:
			FConversation(ISwitchLink& InLink, uint8& InPacket) : Link(InLink), Packet(InPacket) {}

			/** Sends a USB command; true once it is answered, or once sent when no answer comes to it. */
			bool Command(ESwitchUsbCommand Command, bool bAnswered)
			{
				const std::vector<uint8> Report = SwitchUsbCommand(Command);
				for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
				{
					if (Link.Send(Report) && (!bAnswered || Await([Command](std::span<const uint8> Reply) { return IsSwitchUsbReply(Reply, Command); })))
					{
						return true;
					}
				}
				return false;
			}

			/** Sends a subcommand; its reply's data once acknowledged. */
			std::optional<std::vector<uint8>> Subcommand(ESwitchSubcommand Subcommand, std::span<const uint8> Data)
			{
				for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
				{
					std::vector<uint8> ReplyData;
					const auto Answers = [Subcommand, &ReplyData](std::span<const uint8> Reply)
					{
						const std::span<const uint8> Answer = SwitchSubcommandReply(Reply, Subcommand);
						ReplyData.assign(Answer.begin(), Answer.end());
						return !Answer.empty();
					};
					const std::vector<uint8> Report = SwitchSubcommand(Packet, Subcommand, Data);
					Packet = static_cast<uint8>((Packet + 1) & 0x0F);
					if (Link.Send(Report) && Await(Answers))
					{
						return ReplyData;
					}
				}
				return std::nullopt;
			}

			/** The flash's bytes at Address; empty when they could not be read. */
			std::vector<uint8> ReadFlash(uint32 Address, uint8 Length)
			{
				const std::array<uint8, 5> Request = {static_cast<uint8>(Address), static_cast<uint8>(Address >> 8), static_cast<uint8>(Address >> 16),
					static_cast<uint8>(Address >> 24), Length};
				const std::optional<std::vector<uint8>> Reply = Subcommand(ESwitchSubcommand::ReadSpi, Request);
				// The reply repeats the address and length before the bytes.
				if (!Reply || Reply->size() < Request.size() + Length || !std::equal(Request.begin(), Request.end(), Reply->begin()))
				{
					return {};
				}
				return std::vector<uint8>(Reply->begin() + Request.size(), Reply->begin() + Request.size() + Length);
			}

		private:
			template <typename FAnswers>
			bool Await(const FAnswers& Answers)
			{
				const uint64 Deadline = Link.Milliseconds() + ReplyWait;
				for (uint64 Now = Link.Milliseconds(); Now < Deadline; Now = Link.Milliseconds())
				{
					const std::span<const uint8> Report = Link.Receive(static_cast<uint32>(Deadline - Now));
					if (Report.empty())
					{
						return false;
					}
					if (Answers(Report))
					{
						return true;
					}
				}
				return false;
			}

			ISwitchLink& Link;
			uint8& Packet;
		};
	}

	FSwitchSetup SetUpSwitchGamepad(ISwitchLink& Link)
	{
		FSwitchSetup Setup;
		FConversation Talk(Link, Setup.NextPacket);
		// Only a gamepad on USB answers the USB commands.
		Setup.bUsb = Talk.Command(ESwitchUsbCommand::Status, true);
		if (Setup.bUsb)
		{
			if (!Talk.Command(ESwitchUsbCommand::Handshake, true))
			{
				return Setup;
			}
			// Some gamepads ignore the faster link and the second handshake it asks for, and work all the same.
			Talk.Command(ESwitchUsbCommand::HighSpeed, true);
			Talk.Command(ESwitchUsbCommand::Handshake, true);
			Talk.Command(ESwitchUsbCommand::ForceUsb, false);
		}
		const std::vector<uint8> User = Talk.ReadFlash(SwitchUserSticksAddress, SwitchUserSticksLength);
		const std::vector<uint8> Factory = Talk.ReadFlash(SwitchFactorySticksAddress, SwitchFactorySticksLength);
		Setup.Sticks = SwitchSticksOf(Factory, User);
		constexpr std::array<uint8, 1> On = {0x01};
		const bool bVibration = Talk.Subcommand(ESwitchSubcommand::EnableVibration, On).has_value();
		if (!Setup.bUsb && User.empty() && Factory.empty() && !bVibration)
		{
			return Setup;
		}
		if (Setup.bUsb)
		{
			constexpr std::array<uint8, 1> FullReport = {SwitchReport::Full};
			Talk.Subcommand(ESwitchSubcommand::SetInputMode, FullReport);
			Talk.Command(ESwitchUsbCommand::ForceUsb, false);
		}
		constexpr std::array<uint8, 1> FirstPlayer = {0x01};
		Talk.Subcommand(ESwitchSubcommand::SetPlayerLights, FirstPlayer);
		Setup.bAnswered = true;
		return Setup;
	}
}
