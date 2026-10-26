#include "Screens/GameScreenSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Input/ButtonGlyphSubsystem.h"
#include "Input/KeyListenerSubsystem.h"
#include "Input/PadPortsSubsystem.h"
#include "Input/TouchControlsSubsystem.h"
#include "Input/WeltallPlayerController.h"
#include "Presentation/ScreenLayout.h"
#include "Rendering/CrtRendering.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Audio/GameSoundSubsystem.h"
#include "Screens/GameScreen.h"
#include "Shell/GameWindowSubsystem.h"

void AddDisplayFramePasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FIntRect& Display, const Weltall::FDisplayOptions& Options,
	const IScreenFrame* Frame)
{
	AddClearRenderTargetPass(GraphBuilder, Output, FLinearColor::Black, Display);
	if (Frame == nullptr)
	{
		return;
	}
	const Weltall::FDisplayRect Placed = Weltall::FitAspect(Display.Width(), Display.Height(), Options.Aspect);
	const FIntPoint Origin = Display.Min + FIntPoint(Placed.X, Placed.Y);
	const FIntRect Area(Origin, Origin + FIntPoint(Placed.Width, Placed.Height));
	if (Options.Crt == Weltall::ECrtFilter::Off)
	{
		Frame->AddPasses(GraphBuilder, Output, Area);
		return;
	}
	FRDGTextureRef Picture = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(Area.Size(), Output->Desc.Format, FClearValueBinding::Black,
		TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("CrtPicture"));
	AddClearRenderTargetPass(GraphBuilder, Picture, FLinearColor::Black);
	Frame->AddPasses(GraphBuilder, Picture, FIntRect(FIntPoint::ZeroValue, Area.Size()));
	AddCrtPasses(GraphBuilder, Picture, Output, Area, Weltall::CrtParameters(Weltall::ShownCrtLook(Options)), Frame->ScreenSize());
}

void UGameScreenSubsystem::Show(TSharedRef<IGameScreen> InScreen)
{
	Screen = MoveTemp(InScreen);
}

void UGameScreenSubsystem::Close()
{
	Screen.Reset();
	if (UPadPortsSubsystem* Ports = GetGameInstance()->GetSubsystem<UPadPortsSubsystem>())
	{
		Ports->SetSecondRead(false);
		Ports->SetMotors(false, false);
	}
}

TSharedPtr<const IScreenFrame> UGameScreenSubsystem::CaptureFrame() const
{
	return Screen ? Screen->CaptureFrame().ToSharedPtr() : nullptr;
}

void UGameScreenSubsystem::Deinitialize()
{
	// The game is shutting down: nothing runs or draws any more, even before this object is collected.
	Screen.Reset();
	Super::Deinitialize();
}

void UGameScreenSubsystem::Tick(float DeltaTime)
{
	// In the background, when the settings say so, the whole game stands still: no frame runs, no time passes, no sound is made.
	const UGameWindowSubsystem* Window = GetGameInstance()->GetSubsystem<UGameWindowSubsystem>();
	const bool bHeld = Window != nullptr && Window->IsHeldInBackground();
	if (UGameSoundSubsystem* Sound = GetGameInstance()->GetSubsystem<UGameSoundSubsystem>())
	{
		Sound->SetHeld(bHeld);
	}
	UPadPortsSubsystem* Ports = GetGameInstance()->GetSubsystem<UPadPortsSubsystem>();
	if (bHeld)
	{
		// A motor running as the game is held would run on.
		if (Ports != nullptr)
		{
			Ports->SetMotors(false, false);
		}
		return;
	}
	AWeltallPlayerController* Controller = Cast<AWeltallPlayerController>(GetGameInstance()->GetFirstLocalPlayerController());
	if (Controller != nullptr)
	{
		Screen->AddViewInput(Controller->ReadView(DeltaTime));
	}
	FGamePads Pads;
	const UButtonGlyphSubsystem* Glyphs = GetGameInstance()->GetSubsystem<UButtonGlyphSubsystem>();
	if (Glyphs != nullptr)
	{
		Pads.bConnected = Glyphs->HasController();
	}
	if (Ports != nullptr)
	{
		Ports->SetSecondRead(Screen->ReadsSecondPad());
		Pads.bMotor = Ports->FirstHasMotor();
		Pads.bSecondConnected = Ports->HasSecond();
		Pads.SecondHeld = Ports->SecondHeld();
	}
	Screen->SetPads(Pads);
	Weltall::FPadState Held = Controller ? Controller->HeldPad() : Weltall::FPadState{};
	if (UTouchControlsSubsystem* Touch = GetGameInstance()->GetSubsystem<UTouchControlsSubsystem>())
	{
		Held.Buttons |= Touch->TakeHeld().Buttons;
	}
	// A gamepad in use is a DualShock in analog mode to the game; the keyboard and the touch screen's pad are digital pads.
	Held.bAnalog = Glyphs != nullptr && Glyphs->GamepadInUse().IsValid();
	if (UKeyListenerSubsystem* Keys = GetGameInstance()->GetSubsystem<UKeyListenerSubsystem>())
	{
		Held = Keys->Filter(Held);
	}
	Screen->Update(DeltaTime, Held);
	if (Ports != nullptr)
	{
		const std::array<bool, 2> Motors = Screen->Motors();
		Ports->SetMotors(Motors[0], Motors[1]);
	}
}

ETickableTickType UGameScreenSubsystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UGameScreenSubsystem::IsTickable() const
{
	return Screen.IsValid();
}

UWorld* UGameScreenSubsystem::GetTickableGameObjectWorld() const
{
	return GetGameInstance()->GetWorld();
}

TStatId UGameScreenSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGameScreenSubsystem, STATGROUP_Tickables);
}
