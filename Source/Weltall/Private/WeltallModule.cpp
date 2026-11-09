#include "Input/HidGamepadDevice.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"
#include "Shell/GameVersion.h"
#include "WeltallLog.h"

DEFINE_LOG_CATEGORY(LogWeltall);

namespace
{
	class FWeltallModule final : public FDefaultGameModuleImpl
	{
	public:
		virtual void StartupModule() override
		{
			// The renderer's shaders live in the project's Shaders folder, mapped before any compile.
			AddShaderSourceDirectoryMapping(TEXT("/Weltall"), FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders")));
			UE_LOG(LogWeltall, Display, TEXT("Weltall %s"), *GameVersion());
#if PLATFORM_ANDROID
			EKeys::AddKey(FKeyDetails(FKey(TEXT("Gamepad_Auxiliary_C")), NSLOCTEXT("WeltallInput", "AuxiliaryC", "Gamepad C"), FKeyDetails::GamepadKey));
			EKeys::AddKey(FKeyDetails(FKey(TEXT("Gamepad_Auxiliary_Z")), NSLOCTEXT("WeltallInput", "AuxiliaryZ", "Gamepad Z"), FKeyDetails::GamepadKey));
#endif
			HidGamepads::Register();
		}

		virtual void ShutdownModule() override
		{
			HidGamepads::Unregister();
		}
	};
}

IMPLEMENT_PRIMARY_GAME_MODULE(FWeltallModule, Weltall, "Weltall");
