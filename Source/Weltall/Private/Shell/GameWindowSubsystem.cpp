#include "Shell/GameWindowSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "GenericPlatform/GenericApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Menu/MenuResources.h"
#include "Misc/App.h"
#include "RHI.h"
#include "Shell/PlayerSettingsSubsystem.h"
#include "UnrealClient.h"

#include <algorithm>

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
	/** The editor's windows are the editor's, and a tool that draws nothing has none. */
	bool HasOwnWindow()
	{
		return !GIsEditor && !IsRunningCommandlet() && FApp::CanEverRender();
	}

#if PLATFORM_WINDOWS
	/** The icon's pixels each made Scale x Scale, as an icon Windows takes; null where it cannot make one. */
	HICON MakeWindowsIcon(const Weltall::FGameIcon& Icon, int32 Scale)
	{
		const int32 Size = Weltall::FGameIcon::Size * Scale;
		BITMAPV5HEADER Header{};
		Header.bV5Size = sizeof(Header);
		Header.bV5Width = Size;
		Header.bV5Height = -Size;
		Header.bV5Planes = 1;
		Header.bV5BitCount = 32;
		Header.bV5Compression = BI_BITFIELDS;
		Header.bV5RedMask = 0x00FF0000;
		Header.bV5GreenMask = 0x0000FF00;
		Header.bV5BlueMask = 0x000000FF;
		Header.bV5AlphaMask = 0xFF000000;
		void* Bits = nullptr;
		HDC Screen = GetDC(nullptr);
		HBITMAP Colour = CreateDIBSection(Screen, reinterpret_cast<BITMAPINFO*>(&Header), DIB_RGB_COLORS, &Bits, nullptr, 0);
		ReleaseDC(nullptr, Screen);
		if (Colour == nullptr)
		{
			return nullptr;
		}
		uint32* Out = static_cast<uint32*>(Bits);
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const std::array<uint8, 4>& Pixel = Icon.Pixels[(Y / Scale) * Weltall::FGameIcon::Size + X / Scale];
				Out[Y * Size + X] = static_cast<uint32>(Pixel[3]) << 24 | static_cast<uint32>(Pixel[0]) << 16 | static_cast<uint32>(Pixel[1]) << 8 | Pixel[2];
			}
		}
		HBITMAP Mask = CreateBitmap(Size, Size, 1, 1, nullptr);
		ICONINFO Info{};
		Info.fIcon = 1;
		Info.hbmColor = Colour;
		Info.hbmMask = Mask;
		HICON Result = CreateIconIndirect(&Info);
		DeleteObject(Colour);
		DeleteObject(Mask);
		return Result;
	}
#endif
}

void UGameWindowSubsystem::ShowGameIcon(const Weltall::FIso9660& Disc)
{
#if PLATFORM_WINDOWS
	const UGameViewportClient* Client = GetGameInstance()->GetGameViewportClient();
	const TSharedPtr<SWindow> Window = Client != nullptr ? Client->GetWindow() : nullptr;
	if (!HasOwnWindow() || !Window.IsValid() || !Window->GetNativeWindow().IsValid())
	{
		return;
	}
	const auto Executable = Weltall::ReadBootExecutable(Disc);
	const auto Index = Executable ? Weltall::FGameFileIndex::Read(*Executable) : Weltall::TResult<Weltall::FGameFileIndex>(Executable.Error());
	const auto Icon = Index ? Weltall::ReadGameIcon(Disc.Image(), *Index) : Weltall::TResult<Weltall::FGameIcon>(Index.Error());
	if (!Icon)
	{
		return;
	}
	// The 16 x 16 picture scaled whole: large for the taskbar and the switcher, small for the title bar. The window's
	// class takes them too: the engine gives the window its class's icon again whenever it returns to windowed mode.
	const HWND Handle = static_cast<HWND>(Window->GetNativeWindow()->GetOSWindowHandle());
	constexpr int32 LargeScale = 16;
	constexpr int32 SmallScale = 2;
	if (HICON Large = MakeWindowsIcon(*Icon, LargeScale))
	{
		SetClassLongPtrW(Handle, GCLP_HICON, reinterpret_cast<LONG_PTR>(Large));
		SendMessageW(Handle, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(Large));
	}
	if (HICON Small = MakeWindowsIcon(*Icon, SmallScale))
	{
		SetClassLongPtrW(Handle, GCLP_HICONSM, reinterpret_cast<LONG_PTR>(Small));
		SendMessageW(Handle, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(Small));
	}
#endif
}

void UGameWindowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UPlayerSettingsSubsystem* Settings = Collection.InitializeDependency<UPlayerSettingsSubsystem>();
	SettingsChanged = Settings->OnSettingsChanged.AddUObject(this, &UGameWindowSubsystem::Follow);
	Follow();
#if PLATFORM_ANDROID
	// A phone's screen would go dark in a long scene played without a touch; the engine keeps only a desktop's awake.
	if (HasOwnWindow())
	{
		FPlatformApplicationMisc::ControlScreensaver(FPlatformApplicationMisc::EScreenSaverAction::Disable);
	}
#endif
}

void UGameWindowSubsystem::Deinitialize()
{
	if (UPlayerSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>())
	{
		Settings->OnSettingsChanged.Remove(SettingsChanged);
	}
	Super::Deinitialize();
}

Weltall::FDisplayModes UGameWindowSubsystem::Modes() const
{
	Weltall::FDisplayModes Result;
	if (!HasOwnWindow())
	{
		return Result;
	}
	FDisplayMetrics Metrics;
	FDisplayMetrics::RebuildDisplayMetrics(Metrics);
	FScreenResolutionArray Resolutions;
	if (RHIGetAvailableResolutions(Resolutions, true))
	{
		for (const FScreenResolutionRHI& Resolution : Resolutions)
		{
			const Weltall::FDisplaySize Size{static_cast<int32>(Resolution.Width), static_cast<int32>(Resolution.Height)};
			if (Size.Width <= Metrics.PrimaryDisplayWidth && Size.Height <= Metrics.PrimaryDisplayHeight)
			{
				Result.WindowSizes.push_back(Size);
			}
		}
	}
	std::sort(Result.WindowSizes.begin(), Result.WindowSizes.end());
	Result.WindowSizes.erase(std::unique(Result.WindowSizes.begin(), Result.WindowSizes.end()), Result.WindowSizes.end());
	const UGameViewportClient* Client = GetGameInstance()->GetGameViewportClient();
	if (Client != nullptr && Client->Viewport != nullptr)
	{
		const FIntPoint Size = Client->Viewport->GetSizeXY();
		Result.Window = {Size.X, Size.Y};
	}
	return Result;
}

bool UGameWindowSubsystem::IsHeldInBackground() const
{
	// A run nobody attends has no window to be in use.
	return HasOwnWindow() && !FApp::IsUnattended() && GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>()->Settings().Display.bPauseInBackground
		&& !FApp::HasFocus();
}

void UGameWindowSubsystem::Follow()
{
	const Weltall::FDisplayOptions& Display = GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>()->Settings().Display;
	FApp::SetUnfocusedVolumeMultiplier(Display.bSoundInBackground ? 1.0f : 0.0f);
	const Weltall::FDisplaySize Chosen = Display.WindowSize;
	UGameUserSettings* Window = GEngine != nullptr ? GEngine->GetGameUserSettings() : nullptr;
	if ((Chosen == Followed && Display.bFullScreen == bFollowedFullScreen) || Window == nullptr || !HasOwnWindow())
	{
		return;
	}
	Followed = Chosen;
	bFollowedFullScreen = Display.bFullScreen;
	FIntPoint Size = Window->GetDesktopResolution();
	EWindowMode::Type Mode = EWindowMode::WindowedFullscreen;
	if (!Display.bFullScreen)
	{
		// A window of the chosen size, or of three quarters of the display until one is chosen.
		Size = Chosen.IsSet() ? FIntPoint(Chosen.Width, Chosen.Height) : Size * 3 / 4;
		Mode = EWindowMode::Windowed;
	}
	if (Window->GetScreenResolution() == Size && Window->GetFullscreenMode() == Mode)
	{
		return;
	}
	Window->SetScreenResolution(Size);
	Window->SetFullscreenMode(Mode);
	Window->ApplyResolutionSettings(false);
	// Kept with the engine's own window settings too, so the next start opens the window at this size.
	Window->SaveSettings();
}
