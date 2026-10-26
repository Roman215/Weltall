#include "Input/ButtonGlyphs.h"
#include "Input/SwitchGamepadSetup.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <deque>
#include <vector>

using namespace Weltall;

namespace
{
	constexpr uint32 Bit(EGamepadControl Control)
	{
		return GamepadControlBit(Control);
	}

	/** Two 12-bit values in three bytes, as the controller packs its sticks and their calibration. */
	std::array<uint8, 3> Packed(int32 First, int32 Second)
	{
		return {static_cast<uint8>(First), static_cast<uint8>((First >> 8) | ((Second & 0x0F) << 4)), static_cast<uint8>(Second >> 4)};
	}

	void Append(std::vector<uint8>& Bytes, std::array<uint8, 3> Pair)
	{
		Bytes.insert(Bytes.end(), Pair.begin(), Pair.end());
	}

	/** A factory calibration: the left stick's reach above, centre and reach below, then the right's centre, below and above. */
	std::vector<uint8> FactoryCalibration()
	{
		std::vector<uint8> Bytes;
		Append(Bytes, Packed(0x500, 0x480));
		Append(Bytes, Packed(0x7A0, 0x880));
		Append(Bytes, Packed(0x520, 0x4A0));
		Append(Bytes, Packed(0x810, 0x7F0));
		Append(Bytes, Packed(0x600, 0x5A0));
		Append(Bytes, Packed(0x580, 0x620));
		return Bytes;
	}

	/** A full report: its three button bytes and its sticks. */
	std::vector<uint8> FullReport(uint8 Right, uint8 Shared, uint8 Left, std::array<int32, 4> Sticks)
	{
		std::vector<uint8> Report = {SwitchReport::Full, 0x00, 0x8E, Right, Shared, Left};
		Append(Report, Packed(Sticks[0], Sticks[1]));
		Append(Report, Packed(Sticks[2], Sticks[3]));
		Report.resize(49, 0);
		return Report;
	}

	/** A Switch gamepad that answers the setup as a Pro Controller does, or as a gamepad that only passes for one. */
	class FFakeSwitchPad final : public ISwitchLink
	{
	public:
		bool bUsb = true;
		bool bAnswers = true;
		bool bAnswersHighSpeed = true;
		std::vector<uint8> Factory = FactoryCalibration();
		std::vector<uint8> User;
		std::vector<std::vector<uint8>> Sent;
		uint64 Clock = 0;

		virtual bool Send(std::span<const uint8> Report) override
		{
			// Bluetooth's description declares no USB command, so the system refuses them.
			if (Report[0] == SwitchReport::UsbCommand && !bUsb)
			{
				return false;
			}
			Sent.emplace_back(Report.begin(), Report.end());
			// The controls keep coming between the replies.
			Pending.push_back(FullReport(0, 0, 0, {0x7A0, 0x880, 0x810, 0x7F0}));
			if (!bAnswers)
			{
				return true;
			}
			if (Report[0] == SwitchReport::UsbCommand)
			{
				const bool bAnswered = Report[1] != static_cast<uint8>(ESwitchUsbCommand::ForceUsb)
					&& (Report[1] != static_cast<uint8>(ESwitchUsbCommand::HighSpeed) || bAnswersHighSpeed);
				if (bAnswered)
				{
					Pending.push_back({SwitchReport::UsbReply, Report[1], 0x00, 0x03});
				}
			}
			else if (Report[0] == SwitchReport::Subcommand)
			{
				Pending.push_back(Reply(Report));
			}
			return true;
		}

		virtual std::span<const uint8> Receive(uint32 Milliseconds) override
		{
			if (Pending.empty())
			{
				Clock += Milliseconds;
				return {};
			}
			Clock += 8;
			Current = Pending.front();
			Pending.pop_front();
			return Current;
		}

		virtual uint64 Milliseconds() override
		{
			return Clock;
		}

	private:
		std::vector<uint8> Reply(std::span<const uint8> Request) const
		{
			const uint8 Subcommand = Request[10];
			std::vector<uint8> Reply = FullReport(0, 0, 0, {0x7A0, 0x880, 0x810, 0x7F0});
			Reply[0] = SwitchReport::SubcommandReply;
			Reply[13] = 0x80;
			Reply[14] = Subcommand;
			if (Subcommand == static_cast<uint8>(ESwitchSubcommand::ReadSpi))
			{
				const uint32 Address = Request[11] | (Request[12] << 8) | (Request[13] << 16) | (Request[14] << 24);
				const std::vector<uint8>& Flash = Address == SwitchFactorySticksAddress ? Factory : User;
				// A flash it cannot read is refused.
				Reply[13] = Flash.empty() ? 0x00 : 0x90;
				std::copy(Request.begin() + 11, Request.begin() + 16, Reply.begin() + 15);
				std::copy(Flash.begin(), Flash.end(), Reply.begin() + 20);
			}
			return Reply;
		}

		std::deque<std::vector<uint8>> Pending;
		std::vector<uint8> Current;
	};

	std::vector<uint8> Command(ESwitchUsbCommand Command)
	{
		return SwitchUsbCommand(Command);
	}

	/** The subcommand a sent report carries, or 0 for another report. */
	uint8 SubcommandOf(const std::vector<uint8>& Report)
	{
		return Report[0] == SwitchReport::Subcommand ? Report[10] : 0;
	}
}

TEST_CASE("The Switch Pro Controller and the gamepads that pass for it show Nintendo's buttons", "[Input]")
{
	CHECK(HidGamepadModelOf(0x057E, 0x2009) == EHidGamepadModel::SwitchPro);
	CHECK(HidGamepadModelOf(0x0F0D, 0x00F6) == EHidGamepadModel::SwitchPro);
	CHECK(HidGamepadModelOf(0x0E6F, 0x0186) == EHidGamepadModel::SwitchPro);
	// The Joy-Cons and the wired pads that need no setup are not.
	CHECK(HidGamepadModelOf(0x057E, 0x2006) == EHidGamepadModel::Other);
	CHECK(HidGamepadModelOf(0x0F0D, 0x00C1) == EHidGamepadModel::Other);
	CHECK(IconSetOfDevice(true, HidGamepadName(EHidGamepadModel::SwitchPro)) == EButtonIconSet::Nintendo);
}

TEST_CASE("A Switch gamepad's full report gives its buttons by place and its sticks by their calibration", "[Input]")
{
	FSwitchSticks Sticks = SwitchSticksOf(FactoryCalibration(), {});
	CHECK(Sticks.Full[0] == FSwitchAxis{0x7A0, 0x520 * 7 / 10, 0x500 * 7 / 10});
	CHECK(Sticks.Full[3] == FSwitchAxis{0x7F0, 0x5A0 * 7 / 10, 0x620 * 7 / 10});
	// Y, B, ZR; Minus and the left stick's click; the d-pad up and left, ZL. The left stick right as far as its reach, and
	// half way down; the right stick at rest.
	const int32 Right = 0x7A0 + 0x500 * 7 / 10;
	const int32 HalfDown = 0x880 - 0x4A0 * 7 / 10 / 2;
	FGamepadReading Reading;
	REQUIRE(ReadSwitchReport(FullReport(0x01 | 0x04 | 0x80, 0x01 | 0x08, 0x02 | 0x08 | 0x80, {Right, HalfDown, 0x810, 0x7F0}), Sticks, Reading));
	CHECK(Reading.Buttons
		== (Bit(EGamepadControl::FaceLeft) | Bit(EGamepadControl::FaceBottom) | Bit(EGamepadControl::SpecialLeft) | Bit(EGamepadControl::LeftThumb)
			| Bit(EGamepadControl::DPadUp) | Bit(EGamepadControl::DPadLeft)));
	CHECK(Reading.LeftTrigger == 1.0f);
	CHECK(Reading.RightTrigger == 1.0f);
	CHECK(Reading.LeftX == 1.0f);
	CHECK(Reading.LeftY == Catch::Approx(-0.5f).margin(0.002f));
	CHECK(Reading.RightX == 0.0f);
	CHECK(Reading.RightY == 0.0f);
	// X, A, R; Plus and the right stick's click; down, right, L.
	REQUIRE(ReadSwitchReport(FullReport(0x02 | 0x08 | 0x40, 0x02 | 0x04 | 0x10, 0x01 | 0x04 | 0x40, {0x7A0, 0x880, 0x810, 0x7F0}), Sticks, Reading));
	CHECK(Reading.Buttons
		== (Bit(EGamepadControl::FaceTop) | Bit(EGamepadControl::FaceRight) | Bit(EGamepadControl::RightShoulder) | Bit(EGamepadControl::SpecialRight)
			| Bit(EGamepadControl::RightThumb) | Bit(EGamepadControl::DPadDown) | Bit(EGamepadControl::DPadRight) | Bit(EGamepadControl::LeftShoulder)));
	CHECK(Reading.LeftTrigger == 0.0f);
	// A stick seen further out than its reach reads full there from then on.
	REQUIRE(ReadSwitchReport(FullReport(0, 0, 0, {Right + 100, 0x880, 0x810, 0x7F0}), Sticks, Reading));
	CHECK(Reading.LeftX == 1.0f);
	REQUIRE(ReadSwitchReport(FullReport(0, 0, 0, {Right, 0x880, 0x810, 0x7F0}), Sticks, Reading));
	CHECK(Reading.LeftX < 1.0f);
	// Its other reports are not the controls.
	std::vector<uint8> Reply = FullReport(0x04, 0, 0, {Right, 0x880, 0x810, 0x7F0});
	Reply[0] = SwitchReport::SubcommandReply;
	CHECK_FALSE(ReadSwitchReport(Reply, Sticks, Reading));
	CHECK_FALSE(ReadSwitchReport(std::vector<uint8>{SwitchReport::Full, 0x00, 0x8E}, Sticks, Reading));
}

TEST_CASE("A Switch gamepad's simple report counts its sticks down as HID does", "[Input]")
{
	FSwitchSticks Sticks = SwitchSticksOf({}, {});
	// B, A, L, ZR; Plus; the hat down-right; the left stick up as far as the starting reach, the right one left half way.
	const std::vector<uint8> Report = {SwitchReport::Simple, 0x01 | 0x02 | 0x10 | 0x80, 0x02, 0x03, 0x00, 0x80, 0x00, 0x40, 0x00, 0x60, 0x00, 0x80};
	FGamepadReading Reading;
	REQUIRE(ReadSwitchReport(Report, Sticks, Reading));
	CHECK(Reading.Buttons
		== (Bit(EGamepadControl::FaceBottom) | Bit(EGamepadControl::FaceRight) | Bit(EGamepadControl::LeftShoulder) | Bit(EGamepadControl::SpecialRight)
			| Bit(EGamepadControl::DPadDown) | Bit(EGamepadControl::DPadRight)));
	CHECK(Reading.LeftTrigger == 0.0f);
	CHECK(Reading.RightTrigger == 1.0f);
	CHECK(Reading.LeftX == 0.0f);
	CHECK(Reading.LeftY == 1.0f);
	CHECK(Reading.RightX == -0.5f);
	CHECK(Reading.RightY == 0.0f);
}

TEST_CASE("A Switch gamepad's calibration: the player's where marked, the middle where the flash holds none", "[Input]")
{
	// Unwritten flash, and none read: the middle of the range, seven tenths of half of it as the reach, then seven tenths of that.
	const FSwitchAxis Middle{0x800, 0x800 * 7 / 10 * 7 / 10, 0x800 * 7 / 10 * 7 / 10};
	const FSwitchSticks None = SwitchSticksOf({}, {});
	CHECK(None.Full[0] == Middle);
	CHECK(None.Simple[1] == FSwitchAxis{0x8000, 0x4000, 0x4000});
	const FSwitchSticks Unwritten = SwitchSticksOf(std::vector<uint8>(18, 0xFF), std::vector<uint8>(22, 0xFF));
	CHECK(Unwritten == None);
	// A centre far from the middle is not believed.
	std::vector<uint8> OffCentre = FactoryCalibration();
	const std::array<uint8, 3> Low = Packed(0x100, 0x880);
	std::copy(Low.begin(), Low.end(), OffCentre.begin() + 3);
	CHECK(SwitchSticksOf(OffCentre, {}).Full[0].Center == 0x800);
	CHECK(SwitchSticksOf(OffCentre, {}).Full[1].Center == 0x880);
	// The player's own calibration of the right stick, marked; the left one unmarked keeps the factory's.
	std::vector<uint8> User(22, 0xFF);
	User[11] = 0xB2;
	User[12] = 0xA1;
	std::vector<uint8> Right;
	Append(Right, Packed(0x790, 0x830));
	Append(Right, Packed(0x400, 0x420));
	Append(Right, Packed(0x440, 0x460));
	std::copy(Right.begin(), Right.end(), User.begin() + 13);
	const FSwitchSticks Sticks = SwitchSticksOf(FactoryCalibration(), User);
	CHECK(Sticks.Full[0] == SwitchSticksOf(FactoryCalibration(), {}).Full[0]);
	CHECK(Sticks.Full[2] == FSwitchAxis{0x790, 0x400 * 7 / 10, 0x440 * 7 / 10});
	CHECK(Sticks.Full[3] == FSwitchAxis{0x830, 0x420 * 7 / 10, 0x460 * 7 / 10});
}

TEST_CASE("A Switch gamepad's rumble: the motors' speeds as the rumble table's amplitudes", "[Input]")
{
	const std::vector<uint8> AtRest = SwitchRumble(3, 0, 0);
	CHECK(AtRest == std::vector<uint8>{0x10, 0x03, 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40});
	CHECK(SwitchRumble(17, 0, 0)[1] == 0x01);
	// The table's rows (amplitude, its high band byte, its low band bytes): each speed nearest a row gives that row.
	struct FRow
	{
		double Amplitude;
		uint8 High;
		uint8 LowFlag;
		uint8 Low;
	};
	const std::array<FRow, 6> Rows = {{{0.047296, 0x14, 0x00, 0x45}, {0.152341, 0x2C, 0x00, 0x4B}, {0.159085, 0x2E, 0x80, 0x4B}, {0.250715, 0x48, 0x00, 0x52},
		{0.501433, 0x88, 0x00, 0x62}, {1.002867, 0xC8, 0x00, 0x72}}};
	for (const FRow& Row : Rows)
	{
		const uint8 Speed = static_cast<uint8>(std::min(255.0, Row.Amplitude * 255.0 + 0.5));
		const std::vector<uint8> Rumble = SwitchRumble(0, Speed, Speed);
		INFO("amplitude " << Row.Amplitude);
		CHECK(Rumble[3] == (Row.High | 0x01));
		CHECK(Rumble[4] == (0x40 | Row.LowFlag));
		CHECK(Rumble[5] == Row.Low);
		CHECK(std::equal(Rumble.begin() + 2, Rumble.begin() + 6, Rumble.begin() + 6));
	}
	// The faintest speed is the table's faintest step; the small motor alone leaves the low band at rest.
	CHECK(SwitchRumble(0, 1, 0)[3] == 0x03);
	CHECK(SwitchRumble(0, 255, 0)[5] == 0x40);
}

TEST_CASE("A Switch gamepad's motors are written no sooner than 30 ms apart and every 50 ms while they run", "[Input]")
{
	FSwitchRumbleTimer Timer;
	CHECK_FALSE(Timer.IsDue(1000, {0, 0}));
	CHECK(Timer.IsDue(1000, {0, 200}));
	Timer.Written(1000, {0, 200});
	CHECK_FALSE(Timer.IsDue(1029, {0, 100}));
	CHECK(Timer.IsDue(1030, {0, 100}));
	CHECK_FALSE(Timer.IsDue(1049, {0, 200}));
	CHECK(Timer.IsDue(1050, {0, 200}));
	Timer.Written(1050, {0, 0});
	CHECK_FALSE(Timer.IsDue(1200, {0, 0}));
}

TEST_CASE("A Switch gamepad on USB is greeted, kept on USB and set to its full report", "[Input]")
{
	FFakeSwitchPad Pad;
	const FSwitchSetup Setup = SetUpSwitchGamepad(Pad);
	CHECK(Setup.bAnswered);
	CHECK(Setup.bUsb);
	CHECK(Setup.Sticks == SwitchSticksOf(FactoryCalibration(), {}));
	REQUIRE(Pad.Sent.size() >= 12);
	CHECK(Pad.Sent[0] == Command(ESwitchUsbCommand::Status));
	CHECK(Pad.Sent[1] == Command(ESwitchUsbCommand::Handshake));
	CHECK(Pad.Sent[2] == Command(ESwitchUsbCommand::HighSpeed));
	CHECK(Pad.Sent[3] == Command(ESwitchUsbCommand::Handshake));
	CHECK(Pad.Sent[4] == Command(ESwitchUsbCommand::ForceUsb));
	// The player's calibration refused five times, then the factory's.
	for (std::size_t Sent = 5; Sent < 10; ++Sent)
	{
		CHECK(SubcommandOf(Pad.Sent[Sent]) == static_cast<uint8>(ESwitchSubcommand::ReadSpi));
		CHECK(Pad.Sent[Sent][11] == 0x10);
	}
	CHECK(Pad.Sent[10] == SwitchSubcommand(5, ESwitchSubcommand::ReadSpi, std::vector<uint8>{0x3D, 0x60, 0x00, 0x00, 0x12}));
	REQUIRE(Pad.Sent.size() == 15);
	CHECK(Pad.Sent[11] == SwitchSubcommand(6, ESwitchSubcommand::EnableVibration, std::vector<uint8>{0x01}));
	CHECK(Pad.Sent[12] == SwitchSubcommand(7, ESwitchSubcommand::SetInputMode, std::vector<uint8>{0x30}));
	CHECK(Pad.Sent[13] == Command(ESwitchUsbCommand::ForceUsb));
	CHECK(Pad.Sent[14] == SwitchSubcommand(8, ESwitchSubcommand::SetPlayerLights, std::vector<uint8>{0x01}));
	CHECK(Setup.NextPacket == 9);
}

TEST_CASE("A Switch gamepad that ignores the faster link, or is on Bluetooth, or answers nothing", "[Input]")
{
	FFakeSwitchPad Slow;
	Slow.bAnswersHighSpeed = false;
	Slow.User = std::vector<uint8>(22, 0xFF);
	const FSwitchSetup SlowSetup = SetUpSwitchGamepad(Slow);
	CHECK(SlowSetup.bAnswered);
	CHECK(SlowSetup.bUsb);
	CHECK(SlowSetup.Sticks == SwitchSticksOf(FactoryCalibration(), {}));

	// On Bluetooth the report it sends is kept.
	FFakeSwitchPad Wireless;
	Wireless.bUsb = false;
	Wireless.Factory.clear();
	const FSwitchSetup WirelessSetup = SetUpSwitchGamepad(Wireless);
	CHECK(WirelessSetup.bAnswered);
	CHECK_FALSE(WirelessSetup.bUsb);
	CHECK(WirelessSetup.Sticks == SwitchSticksOf({}, {}));
	for (const std::vector<uint8>& Sent : Wireless.Sent)
	{
		CHECK(Sent[0] == SwitchReport::Subcommand);
		CHECK(SubcommandOf(Sent) != static_cast<uint8>(ESwitchSubcommand::SetInputMode));
	}

	// One that only passes for a Pro Controller by its numbers is read through its HID description, after two seconds at most.
	FFakeSwitchPad Silent;
	Silent.bAnswers = false;
	const FSwitchSetup SilentSetup = SetUpSwitchGamepad(Silent);
	CHECK_FALSE(SilentSetup.bAnswered);
	CHECK(Silent.Clock <= 2000);
	FFakeSwitchPad SilentWireless;
	SilentWireless.bAnswers = false;
	SilentWireless.bUsb = false;
	CHECK_FALSE(SetUpSwitchGamepad(SilentWireless).bAnswered);
	CHECK(SilentWireless.Clock <= 2000);
}
