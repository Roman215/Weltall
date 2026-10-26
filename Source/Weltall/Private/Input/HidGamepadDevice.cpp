#include "Input/HidGamepadDevice.h"

#if PLATFORM_WINDOWS

#include "Features/IModularFeatures.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GenericPlatform/IInputInterface.h"
#include "GenericPlatform/InputDeviceRegistry.h"
#include "HAL/PlatformTime.h"
#include "IInputDevice.h"
#include "IInputDeviceModule.h"
#include "Input/GamepadEvents.h"
#include "Input/HidPad.h"
#include "Tasks/Task.h"
#include "WeltallLog.h"

namespace
{
	const FName DeviceInterfaceName(TEXT("WeltallHidGamepads"));

	/** The HID gamepads as one of the engine's input devices: found again whenever Windows says its devices changed. */
	class FHidGamepadDevice final : public IInputDevice
	{
	public:
		explicit FHidGamepadDevice(const TSharedRef<FGenericApplicationMessageHandler>& InHandler) : Handler(InHandler) {}

		virtual ~FHidGamepadDevice() override
		{
			// The pads a search still opens close with it.
			if (Search.IsSet())
			{
				Search->Wait();
			}
		}

		virtual void Tick(float DeltaTime) override
		{
			if (Search.IsSet() && Search->IsCompleted())
			{
				for (TUniquePtr<FHidPad>& Pad : Search->GetResult())
				{
					Connect(MoveTemp(Pad));
				}
				Search.Reset();
			}
			if (!Search.IsSet() && bSearchAgain)
			{
				bSearchAgain = false;
				TSet<FString> Open;
				for (const FPad& Pad : Pads)
				{
					Open.Add(Pad.Hid->Path());
				}
				Search.Emplace(UE::Tasks::Launch(UE_SOURCE_LOCATION, [Open = MoveTemp(Open)]() { return FindHidPads(Open); }));
			}
		}

		virtual void SendControllerEvents() override
		{
			IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
			const double Now = FPlatformTime::Seconds();
			for (int32 Index = 0; Index < Pads.Num();)
			{
				FPad& Pad = Pads[Index];
				const FPlatformUserId User = Mapper.GetUserForInputDevice(Pad.Device);
				if (!Pad.Hid->Read(Pad.Reading, Now))
				{
					Disconnect(Pad, User);
					Pads.RemoveAt(Index);
					continue;
				}
				Pad.Events.Send(*Handler, User, Pad.Device, Pad.Reading, Now);
				if (Pad.Events.IsHeld())
				{
					Pad.LastUsed = Now;
				}
				if (Pad.Hid->AreMotorsDue(Pad.Motors, Now))
				{
					Pad.Hid->WriteMotors(Pad.Motors, Now);
				}
				++Index;
			}
		}

		virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InHandler) override
		{
			Handler = InHandler;
		}

		virtual bool Exec(UWorld* World, const TCHAR* Command, FOutputDevice& Output) override
		{
			return false;
		}

		virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType Channel, float Value) override
		{
			FForceFeedbackValues Values = Feedback.FindRef(ControllerId);
			switch (Channel)
			{
			case FForceFeedbackChannelType::LEFT_LARGE: Values.LeftLarge = Value; break;
			case FForceFeedbackChannelType::LEFT_SMALL: Values.LeftSmall = Value; break;
			case FForceFeedbackChannelType::RIGHT_LARGE: Values.RightLarge = Value; break;
			case FForceFeedbackChannelType::RIGHT_SMALL: Values.RightSmall = Value; break;
			}
			SetChannelValues(ControllerId, Values);
		}

		/**
		 * The engine addresses force feedback on Windows by the user's index: of that user's gamepads here, the one used last
		 * runs its motors, the others stop.
		 */
		virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues& Values) override
		{
			Feedback.Add(ControllerId, Values);
			IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
			FPad* Used = nullptr;
			for (FPad& Pad : Pads)
			{
				if (Mapper.GetUserIndexForPlatformUser(Mapper.GetUserForInputDevice(Pad.Device)) == ControllerId && Pad.Hid->HasMotors()
					&& (Used == nullptr || Pad.LastUsed > Used->LastUsed))
				{
					Used = &Pad;
				}
			}
			const auto Speed = [](float Left, float Right) { return static_cast<uint8>(FMath::Clamp(FMath::Max(Left, Right), 0.0f, 1.0f) * 255.0f + 0.5f); };
			for (FPad& Pad : Pads)
			{
				if (Mapper.GetUserIndexForPlatformUser(Mapper.GetUserForInputDevice(Pad.Device)) == ControllerId)
				{
					const bool bRuns = &Pad == Used;
					Pad.Motors = {bRuns ? Speed(Values.LeftSmall, Values.RightSmall) : uint8(0), bRuns ? Speed(Values.LeftLarge, Values.RightLarge) : uint8(0)};
				}
			}
		}

		virtual void SetDeviceProperty(int32 ControllerId, const FInputDeviceProperty* Property) override
		{
			// Windows' word that its devices changed, which the engine passes on to every input device.
			static const FName DevicesChanged(TEXT("Request_Device_Update"));
			if (Property != nullptr && Property->Name == DevicesChanged)
			{
				bSearchAgain = true;
			}
		}

		virtual bool IsGamepadAttached() const override
		{
			return !Pads.IsEmpty();
		}

	private:
		struct FPad
		{
			TUniquePtr<FHidPad> Hid;
			FInputDeviceId Device;
			FGamepadEvents Events;
			Weltall::FGamepadReading Reading;
			double LastUsed = 0.0;
			/** The small motor's speed, then the large one's, as the game asks. */
			std::array<uint8, 2> Motors{};
		};

		void Connect(TUniquePtr<FHidPad> Hid)
		{
			if (!Hid->StartReading())
			{
				return;
			}
			IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
			// A gamepad plugged in again is the same device to the engine.
			FInputDeviceId& Device = Devices.FindOrAdd(Hid->Path());
			if (!Device.IsValid())
			{
				Device = Mapper.AllocateNewInputDeviceId();
			}
			FInputDeviceDescriptor Descriptor;
			Descriptor.HardwareDeviceHandle = Device;
			Descriptor.InputDeviceName = DeviceInterfaceName;
			Descriptor.HardwareDeviceIdentifier = FName(*Hid->Name());
			FInputDeviceRegistry::RegisterDevice(Descriptor);
			Mapper.Internal_MapInputDeviceToUser(Device, Mapper.GetPlatformUserForNewlyConnectedDevice(), EInputDeviceConnectionState::Connected);
			UE_LOG(LogWeltall, Log, TEXT("Gamepad connected: %s"), *Hid->Description());
			FPad& Pad = Pads.AddDefaulted_GetRef();
			Pad.Hid = MoveTemp(Hid);
			Pad.Device = Device;
		}

		void Disconnect(FPad& Pad, FPlatformUserId User)
		{
			Pad.Events.Release(*Handler, User, Pad.Device);
			IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
			Mapper.Internal_MapInputDeviceToUser(Pad.Device, Mapper.GetUserForUnpairedInputDevices(), EInputDeviceConnectionState::Disconnected);
			UE_LOG(LogWeltall, Log, TEXT("Gamepad disconnected: %s"), *Pad.Hid->Name());
		}

		TSharedRef<FGenericApplicationMessageHandler> Handler;
		TArray<FPad> Pads;
		TMap<FString, FInputDeviceId> Devices;
		TMap<int32, FForceFeedbackValues> Feedback;
		TOptional<UE::Tasks::TTask<TArray<TUniquePtr<FHidPad>>>> Search;
		bool bSearchAgain = true;
	};

	class FHidGamepadsFeature final : public IInputDeviceModule
	{
	public:
		virtual TSharedPtr<IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& Handler) override
		{
			// The editor's game module may be unloaded before its application lets go of its input devices.
			if (GIsEditor)
			{
				return nullptr;
			}
			return MakeShared<FHidGamepadDevice>(Handler);
		}
	};

	TUniquePtr<FHidGamepadsFeature> Feature;
}

void HidGamepads::Register()
{
	Feature = MakeUnique<FHidGamepadsFeature>();
	IModularFeatures::Get().RegisterModularFeature(IInputDeviceModule::GetModularFeatureName(), Feature.Get());
}

void HidGamepads::Unregister()
{
	if (Feature.IsValid())
	{
		IModularFeatures::Get().UnregisterModularFeature(IInputDeviceModule::GetModularFeatureName(), Feature.Get());
		Feature.Reset();
	}
}

#else

void HidGamepads::Register()
{
}

void HidGamepads::Unregister()
{
}

#endif
