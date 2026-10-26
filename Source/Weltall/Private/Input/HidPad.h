#pragma once

#if PLATFORM_WINDOWS

#include "CoreMinimal.h"
#include "Input/HidGamepads.h"
#include "Input/SwitchGamepads.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"
#include <hidsdi.h>
#include "Windows/HideWindowsPlatformTypes.h"

#include <array>
#include <span>

/** A HID gamepad open for reading and writing, overlapped. */
class FHidPad
{
public:
	/** Opens the HID device at Path if it is a gamepad Weltall reads (not XInput's), a Switch gamepad set up; null otherwise. Any thread. */
	static TUniquePtr<FHidPad> Open(const FString& Path);

	~FHidPad();

	const FString& Path() const { return DevicePath; }
	const FString& Name() const { return DeviceName; }

	/** Its name, vendor and product numbers, as the log names it. */
	FString Description() const;

	bool HasMotors() const;

	/** Starts reading its reports, unless a read is already under way; false if it cannot. On the thread that reads them. */
	bool StartReading();

	/** Reads the reports that arrived since the last call into Reading; false once the gamepad has gone. Now in seconds. */
	bool Read(Weltall::FGamepadReading& Reading, double Now);

	/** Whether its motors are to be written now to run at Motors, the small one's speed then the large one's. */
	bool AreMotorsDue(const std::array<uint8, 2>& Motors, double Now);

	void WriteMotors(const std::array<uint8, 2>& Motors, double Now);

private:
	friend class FHidPadSwitchLink;

	/** An axis or the hat of a gamepad read through its HID description, and its logical range. */
	struct FHidValue
	{
		USAGE Usage = 0;
		USHORT Collection = 0;
		LONG Minimum = 0;
		LONG Maximum = 0;
		USHORT BitSize = 0;
	};

	FHidPad() = default;

	bool IsWriting();
	bool BeginWrite(std::span<const uint8> Report);
	bool WriteNow(std::span<const uint8> Report, uint32 Milliseconds);
	std::span<const uint8> ReadNow(uint32 Milliseconds);
	void SetUpSwitch();
	void Interpret(TConstArrayView<uint8> Report, Weltall::FGamepadReading& Reading);
	bool TakeControls(TConstArrayView<uint8> Report);
	void FindValues();

	FString DevicePath;
	FString DeviceName;
	USHORT VendorId = 0;
	USHORT ProductId = 0;
	HANDLE Handle = INVALID_HANDLE_VALUE;
	Weltall::EHidGamepadModel Model = Weltall::EHidGamepadModel::Other;
	bool bBluetooth = false;
	bool bAdapter = false;
	PHIDP_PREPARSED_DATA Preparsed = nullptr;
	TArray<FHidValue> Values;
	TArray<USAGE> Usages;
	Weltall::FHidControls Controls;
	Weltall::FSwitchSticks Sticks;
	Weltall::FSwitchRumbleTimer RumbleTimer;
	std::array<uint8, 2> WrittenMotors{};
	double LastReport = 0.0;
	double LastKeptOnUsb = 0.0;
	TArray<uint8> ReadBuffer;
	TArray<uint8> WriteBuffer;
	int32 OutputLength = 0;
	uint8 Packet = 0;
	OVERLAPPED ReadOverlapped{};
	OVERLAPPED WriteOverlapped{};
	bool bReading = false;
	bool bWriting = false;
};

/** The gamepads among the HID devices present, but those already open. Any thread. */
TArray<TUniquePtr<FHidPad>> FindHidPads(const TSet<FString>& Open);

#endif
