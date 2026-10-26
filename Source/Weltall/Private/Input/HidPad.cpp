#include "Input/HidPad.h"

#if PLATFORM_WINDOWS

#include "HAL/PlatformTime.h"
#include "Input/SwitchGamepadSetup.h"
#include "WeltallLog.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <setupapi.h>
#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
	using Weltall::EHidGamepadModel;

	/** Microsoft's gamepads are XInput's, as is any device whose path names an XInput interface. */
	constexpr USHORT MicrosoftVendor = 0x045E;
	const TCHAR* const XInputInterface = TEXT("IG_");

	/** The length of Sony's gamepads' input reports over USB; over Bluetooth they declare longer ones. */
	constexpr USHORT SonyUsbReportLength = 64;

	/** More reports than a gamepad sends between two frames: a read loop stops there. */
	constexpr int32 MostReportsAFrame = 256;

	/** How long a write may take during a Switch gamepad's setup. */
	constexpr uint32 SetupWriteWait = 100;

	/** How long a Switch gamepad on USB may go quiet before it is told again to stay on USB (another program may have stopped it). */
	constexpr double SwitchQuiet = 0.1;

	uint64 ToMilliseconds(double Seconds)
	{
		return static_cast<uint64>(Seconds * 1000.0);
	}
}

/** The setup's exchanges with a Switch gamepad, before its reports are read each frame. */
class FHidPadSwitchLink final : public Weltall::ISwitchLink
{
public:
	explicit FHidPadSwitchLink(FHidPad& InPad) : Pad(InPad) {}

	virtual bool Send(std::span<const uint8> Report) override { return Pad.WriteNow(Report, SetupWriteWait); }
	virtual std::span<const uint8> Receive(uint32 Wait) override { return Pad.ReadNow(Wait); }
	virtual uint64 Milliseconds() override { return ToMilliseconds(FPlatformTime::Seconds()); }

private:
	FHidPad& Pad;
};

FHidPad::~FHidPad()
{
	CancelIoEx(Handle, nullptr);
	DWORD Ignored = 0;
	if (bReading)
	{
		GetOverlappedResult(Handle, &ReadOverlapped, &Ignored, true);
	}
	if (bWriting)
	{
		GetOverlappedResult(Handle, &WriteOverlapped, &Ignored, true);
	}
	CloseHandle(Handle);
	CloseHandle(ReadOverlapped.hEvent);
	CloseHandle(WriteOverlapped.hEvent);
	if (Preparsed != nullptr)
	{
		HidD_FreePreparsedData(Preparsed);
	}
}

FString FHidPad::Description() const
{
	return FString::Printf(TEXT("%s (%04X:%04X)"), *DeviceName, VendorId, ProductId);
}

bool FHidPad::StartReading()
{
	if (bReading)
	{
		return true;
	}
	ResetEvent(ReadOverlapped.hEvent);
	bReading = ReadFile(Handle, ReadBuffer.GetData(), static_cast<DWORD>(ReadBuffer.Num()), nullptr, &ReadOverlapped) || GetLastError() == ERROR_IO_PENDING;
	return bReading;
}

bool FHidPad::Read(Weltall::FGamepadReading& Reading, double Now)
{
	for (int32 Report = 0; Report < MostReportsAFrame; ++Report)
	{
		DWORD Length = 0;
		if (!GetOverlappedResult(Handle, &ReadOverlapped, &Length, false))
		{
			if (GetLastError() != ERROR_IO_INCOMPLETE)
			{
				return false;
			}
			break;
		}
		bReading = false;
		LastReport = Now;
		Interpret(TConstArrayView<uint8>(ReadBuffer.GetData(), static_cast<int32>(Length)), Reading);
		if (!StartReading())
		{
			return false;
		}
	}
	const bool bQuiet = Now - LastReport > SwitchQuiet && Now - LastKeptOnUsb > SwitchQuiet;
	if (Model == EHidGamepadModel::SwitchPro && !bBluetooth && bQuiet && !IsWriting())
	{
		BeginWrite(Weltall::SwitchUsbCommand(Weltall::ESwitchUsbCommand::ForceUsb));
		LastKeptOnUsb = Now;
	}
	return true;
}

bool FHidPad::AreMotorsDue(const std::array<uint8, 2>& Motors, double Now)
{
	if (!HasMotors() || IsWriting())
	{
		return false;
	}
	return Model == EHidGamepadModel::SwitchPro ? RumbleTimer.IsDue(ToMilliseconds(Now), Motors) : Motors != WrittenMotors;
}

void FHidPad::WriteMotors(const std::array<uint8, 2>& Motors, double Now)
{
	switch (Model)
	{
	case EHidGamepadModel::DualShock4:
		BeginWrite(Weltall::DualShock4MotorReport(bBluetooth, Motors[0], Motors[1]));
		break;
	case EHidGamepadModel::DualSense:
		BeginWrite(Weltall::DualSenseMotorReport(bBluetooth, Packet, Motors[0], Motors[1]));
		Packet = static_cast<uint8>((Packet + 1) & 0x0F);
		break;
	case EHidGamepadModel::SwitchPro:
		BeginWrite(Weltall::SwitchRumble(Packet, Motors[0], Motors[1]));
		Packet = static_cast<uint8>((Packet + 1) & 0x0F);
		break;
	case EHidGamepadModel::PlayStationClassic:
	case EHidGamepadModel::Other:
		return;
	}
	// One that failed is not written again: a gamepad that left is found by its reads.
	WrittenMotors = Motors;
	RumbleTimer.Written(ToMilliseconds(Now), Motors);
}

bool FHidPad::HasMotors() const
{
	return OutputLength > 0
		&& (Model == EHidGamepadModel::DualShock4 || Model == EHidGamepadModel::DualSense || Model == EHidGamepadModel::SwitchPro);
}

bool FHidPad::IsWriting()
{
	DWORD Written = 0;
	if (bWriting && !GetOverlappedResult(Handle, &WriteOverlapped, &Written, false) && GetLastError() == ERROR_IO_INCOMPLETE)
	{
		return true;
	}
	bWriting = false;
	return false;
}

bool FHidPad::BeginWrite(std::span<const uint8> Report)
{
	// Windows writes a report padded to the longest the device declares.
	WriteBuffer.SetNumUninitialized(OutputLength);
	FMemory::Memzero(WriteBuffer.GetData(), WriteBuffer.Num());
	FMemory::Memcpy(WriteBuffer.GetData(), Report.data(), FMath::Min<int32>(OutputLength, static_cast<int32>(Report.size())));
	ResetEvent(WriteOverlapped.hEvent);
	bWriting = WriteFile(Handle, WriteBuffer.GetData(), static_cast<DWORD>(WriteBuffer.Num()), nullptr, &WriteOverlapped) || GetLastError() == ERROR_IO_PENDING;
	return bWriting;
}

bool FHidPad::WriteNow(std::span<const uint8> Report, uint32 Milliseconds)
{
	if (!BeginWrite(Report))
	{
		return false;
	}
	if (WaitForSingleObject(WriteOverlapped.hEvent, Milliseconds) != WAIT_OBJECT_0)
	{
		CancelIoEx(Handle, &WriteOverlapped);
	}
	DWORD Written = 0;
	const bool bWritten = GetOverlappedResult(Handle, &WriteOverlapped, &Written, true) != 0;
	bWriting = false;
	return bWritten;
}

std::span<const uint8> FHidPad::ReadNow(uint32 Milliseconds)
{
	// A read the wait gave up on stays under way for the next one.
	DWORD Length = 0;
	if (!StartReading() || WaitForSingleObject(ReadOverlapped.hEvent, Milliseconds) != WAIT_OBJECT_0
		|| !GetOverlappedResult(Handle, &ReadOverlapped, &Length, false))
	{
		return {};
	}
	bReading = false;
	return std::span<const uint8>(ReadBuffer.GetData(), Length);
}

void FHidPad::SetUpSwitch()
{
	FHidPadSwitchLink Link(*this);
	const Weltall::FSwitchSetup Setup = OutputLength > 0 ? Weltall::SetUpSwitchGamepad(Link) : Weltall::FSwitchSetup{};
	if (!Setup.bAnswered)
	{
		Model = EHidGamepadModel::Other;
		UE_LOG(LogWeltall, Log, TEXT("The gamepad %04X:%04X does not answer as a Switch Pro Controller does: read through its HID description"), VendorId,
			ProductId);
		return;
	}
	bBluetooth = !Setup.bUsb;
	Sticks = Setup.Sticks;
	Packet = Setup.NextPacket;
	LastReport = FPlatformTime::Seconds();
}

void FHidPad::Interpret(TConstArrayView<uint8> Report, Weltall::FGamepadReading& Reading)
{
	const std::span<const uint8> Bytes(Report.GetData(), static_cast<std::size_t>(Report.Num()));
	switch (Model)
	{
	case EHidGamepadModel::DualShock4:
		Weltall::ReadDualShock4Report(Bytes, bAdapter, Reading);
		return;
	case EHidGamepadModel::DualSense:
		Weltall::ReadDualSenseReport(Bytes, bBluetooth, Reading);
		return;
	case EHidGamepadModel::SwitchPro:
		Weltall::ReadSwitchReport(Bytes, Sticks, Reading);
		return;
	case EHidGamepadModel::PlayStationClassic:
	case EHidGamepadModel::Other:
		break;
	}
	if (TakeControls(Report))
	{
		Reading = Weltall::ReadHidControls(Model, Controls);
	}
}

bool FHidPad::TakeControls(TConstArrayView<uint8> Report)
{
	// A gamepad may send its buttons and its axes in reports of their own: each report changes only what it holds.
	PCHAR Data = reinterpret_cast<PCHAR>(const_cast<uint8*>(Report.GetData()));
	const ULONG Length = static_cast<ULONG>(Report.Num());
	ULONG Count = static_cast<ULONG>(Usages.Num());
	bool bTaken = HidP_GetUsages(HidP_Input, HID_USAGE_PAGE_BUTTON, 0, Usages.GetData(), &Count, Preparsed, Data, Length) == HIDP_STATUS_SUCCESS;
	if (bTaken)
	{
		Controls.Buttons.assign(Usages.GetData(), Usages.GetData() + Count);
	}
	for (const FHidValue& Value : Values)
	{
		ULONG Raw = 0;
		if (HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, Value.Collection, Value.Usage, &Raw, Preparsed, Data, Length) != HIDP_STATUS_SUCCESS)
		{
			continue;
		}
		bTaken = true;
		// A range from a negative minimum is of signed values; one whose maximum is not above its minimum spans its bits.
		const LONG Signed = Value.Minimum < 0 && Value.BitSize > 0 && Value.BitSize < 32 && (Raw >> (Value.BitSize - 1)) != 0
			? static_cast<LONG>(Raw) - (1L << Value.BitSize)
			: static_cast<LONG>(Raw);
		const bool bSpansBits = Value.Maximum <= Value.Minimum;
		const double Minimum = bSpansBits ? 0.0 : Value.Minimum;
		const double Maximum = bSpansBits ? static_cast<double>((1ull << FMath::Min<USHORT>(Value.BitSize, 31)) - 1) : Value.Maximum;
		if (Value.Usage == HID_USAGE_GENERIC_HATSWITCH)
		{
			// Eight directions, or four (each then every other one of the eight); any other value is at rest.
			const int64 Position = static_cast<int64>(Signed) - static_cast<int64>(Minimum);
			const int64 Positions = static_cast<int64>(Maximum - Minimum) + 1;
			const bool bPushed = Position >= 0 && Position < Positions && (Positions == 8 || Positions == 4);
			Controls.Hat = bPushed ? std::optional<int32>(static_cast<int32>(Positions == 4 ? Position * 2 : Position)) : std::nullopt;
			continue;
		}
		const float Axis = static_cast<float>(FMath::Clamp((Signed - Minimum) / FMath::Max(Maximum - Minimum, 1.0) * 2.0 - 1.0, -1.0, 1.0));
		switch (Value.Usage)
		{
		case HID_USAGE_GENERIC_X: Controls.X = Axis; break;
		case HID_USAGE_GENERIC_Y: Controls.Y = Axis; break;
		case HID_USAGE_GENERIC_Z: Controls.Z = Axis; break;
		case HID_USAGE_GENERIC_RX: Controls.Rx = Axis; break;
		case HID_USAGE_GENERIC_RY: Controls.Ry = Axis; break;
		case HID_USAGE_GENERIC_RZ: Controls.Rz = Axis; break;
		default: break;
		}
	}
	return bTaken;
}

void FHidPad::FindValues()
{
	USHORT Count = 0;
	HidP_GetValueCaps(HidP_Input, nullptr, &Count, Preparsed);
	TArray<HIDP_VALUE_CAPS> Caps;
	Caps.SetNumZeroed(Count);
	if (Count == 0 || HidP_GetValueCaps(HidP_Input, Caps.GetData(), &Count, Preparsed) != HIDP_STATUS_SUCCESS)
	{
		return;
	}
	for (const HIDP_VALUE_CAPS& Cap : Caps)
	{
		if (Cap.UsagePage != HID_USAGE_PAGE_GENERIC)
		{
			continue;
		}
		const USAGE First = Cap.IsRange ? Cap.Range.UsageMin : Cap.NotRange.Usage;
		const USAGE Last = Cap.IsRange ? Cap.Range.UsageMax : Cap.NotRange.Usage;
		for (USAGE Usage = First; Usage >= First && Usage <= Last; ++Usage)
		{
			const bool bRead = (Usage >= HID_USAGE_GENERIC_X && Usage <= HID_USAGE_GENERIC_RZ) || Usage == HID_USAGE_GENERIC_HATSWITCH;
			if (bRead && !Values.ContainsByPredicate([Usage](const FHidValue& Value) { return Value.Usage == Usage; }))
			{
				Values.Add({Usage, Cap.LinkCollection, Cap.LogicalMin, Cap.LogicalMax, Cap.BitSize});
			}
			if (Usage == Last)
			{
				break;
			}
		}
	}
}

TUniquePtr<FHidPad> FHidPad::Open(const FString& Path)
{
	if (Path.Contains(XInputInterface, ESearchCase::IgnoreCase))
	{
		return nullptr;
	}
	// Asking no access of a device opens any HID device, for its description; reading it is asked only of a gamepad.
	HANDLE Query = CreateFileW(*Path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (Query == INVALID_HANDLE_VALUE)
	{
		return nullptr;
	}
	HIDD_ATTRIBUTES Attributes{};
	Attributes.Size = sizeof(Attributes);
	PHIDP_PREPARSED_DATA Preparsed = nullptr;
	HIDP_CAPS Caps{};
	WCHAR Product[128] = {};
	const bool bDescribed = HidD_GetAttributes(Query, &Attributes) && HidD_GetPreparsedData(Query, &Preparsed) && HidP_GetCaps(Preparsed, &Caps) == HIDP_STATUS_SUCCESS;
	if (bDescribed && !HidD_GetProductString(Query, Product, sizeof(Product)))
	{
		Product[0] = 0;
	}
	CloseHandle(Query);
	const bool bGamepad = bDescribed && Caps.UsagePage == HID_USAGE_PAGE_GENERIC
		&& (Caps.Usage == HID_USAGE_GENERIC_JOYSTICK || Caps.Usage == HID_USAGE_GENERIC_GAMEPAD) && Attributes.VendorID != MicrosoftVendor
		&& Caps.InputReportByteLength > 0;
	HANDLE Handle = bGamepad
		? CreateFileW(*Path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr)
		: INVALID_HANDLE_VALUE;
	if (bGamepad && Handle == INVALID_HANDLE_VALUE)
	{
		// A device that refuses writing is still read, without its motors.
		Handle = CreateFileW(*Path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
		Caps.OutputReportByteLength = 0;
	}
	if (Handle == INVALID_HANDLE_VALUE)
	{
		if (Preparsed != nullptr)
		{
			HidD_FreePreparsedData(Preparsed);
		}
		return nullptr;
	}
	TUniquePtr<FHidPad> Pad(new FHidPad());
	Pad->DevicePath = Path;
	Pad->VendorId = Attributes.VendorID;
	Pad->ProductId = Attributes.ProductID;
	Pad->Handle = Handle;
	Pad->Preparsed = Preparsed;
	Pad->Model = Weltall::HidGamepadModelOf(Attributes.VendorID, Attributes.ProductID);
	Pad->bAdapter = Weltall::IsDualShock4Adapter(Attributes.VendorID, Attributes.ProductID);
	Pad->bBluetooth = Caps.InputReportByteLength != SonyUsbReportLength;
	Pad->ReadBuffer.SetNumZeroed(Caps.InputReportByteLength);
	Pad->OutputLength = Caps.OutputReportByteLength;
	Pad->Usages.SetNumZeroed(FMath::Max<ULONG>(HidP_MaxUsageListLength(HidP_Input, HID_USAGE_PAGE_BUTTON, Preparsed), 1));
	Pad->FindValues();
	Pad->ReadOverlapped.hEvent = CreateEventW(nullptr, true, false, nullptr);
	Pad->WriteOverlapped.hEvent = CreateEventW(nullptr, true, false, nullptr);
	if (Pad->Model == EHidGamepadModel::SwitchPro)
	{
		Pad->SetUpSwitch();
	}
	const std::string_view Known = Weltall::HidGamepadName(Pad->Model);
	Pad->DeviceName = !Known.empty() ? FString(static_cast<int32>(Known.size()), Known.data()) : Product[0] != 0 ? FString(Product) : FString(TEXT("Gamepad"));
	return Pad;
}

TArray<TUniquePtr<FHidPad>> FindHidPads(const TSet<FString>& Open)
{
	TArray<TUniquePtr<FHidPad>> Found;
	GUID HidClass;
	HidD_GetHidGuid(&HidClass);
	const HDEVINFO Devices = SetupDiGetClassDevsW(&HidClass, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (Devices == INVALID_HANDLE_VALUE)
	{
		return Found;
	}
	SP_DEVICE_INTERFACE_DATA Interface{};
	Interface.cbSize = sizeof(Interface);
	for (DWORD Index = 0; SetupDiEnumDeviceInterfaces(Devices, nullptr, &HidClass, Index, &Interface); ++Index)
	{
		DWORD Size = 0;
		SetupDiGetDeviceInterfaceDetailW(Devices, &Interface, nullptr, 0, &Size, nullptr);
		if (Size < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
		{
			continue;
		}
		TArray<uint8> Storage;
		Storage.SetNumZeroed(static_cast<int32>(Size));
		SP_DEVICE_INTERFACE_DETAIL_DATA_W* Detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(Storage.GetData());
		Detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
		if (!SetupDiGetDeviceInterfaceDetailW(Devices, &Interface, Detail, Size, nullptr, nullptr))
		{
			continue;
		}
		const FString Path(Detail->DevicePath);
		if (!Open.Contains(Path))
		{
			if (TUniquePtr<FHidPad> Pad = FHidPad::Open(Path))
			{
				Found.Add(MoveTemp(Pad));
			}
		}
	}
	SetupDiDestroyDeviceInfoList(Devices);
	return Found;
}

#endif
