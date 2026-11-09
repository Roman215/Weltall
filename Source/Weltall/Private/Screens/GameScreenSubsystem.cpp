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
	const IScreenFrame* Frame, bool bTouchOverlay)
{
	AddClearRenderTargetPass(GraphBuilder, Output, FLinearColor::Black, Display);
	if (Frame == nullptr)
	{
		return;
	}
	const Weltall::FScreenSize Screen = Frame->ScreenSize();
	const Weltall::FPictureArea Placed = Weltall::PictureArea(Display.Width(), Display.Height(), Options, Screen);
	const FIntPoint Origin = Display.Min + FIntPoint(Placed.Rect.X, Placed.Rect.Y);
	const FScreenArea Area(FIntRect(Origin, Origin + FIntPoint(Placed.Rect.Width, Placed.Rect.Height)), Placed.WholeScale, bTouchOverlay);
	if (Options.Crt == Weltall::ECrtFilter::Off)
	{
		Frame->AddPasses(GraphBuilder, Output, Area);
		return;
	}
	FRDGTextureRef Picture = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(Area.Size(), Output->Desc.Format, FClearValueBinding::Black,
		TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("CrtPicture"));
	AddClearRenderTargetPass(GraphBuilder, Picture, FLinearColor::Black);
	Frame->AddPasses(GraphBuilder, Picture, Area.At(FIntPoint::ZeroValue));
	AddCrtPasses(GraphBuilder, Picture, Output, Area, Weltall::CrtParameters(Weltall::ShownCrtLook(Options)), Screen);
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
		// A screen holding its game still (its motors off) makes no sound either, though it takes the player's buttons.
		Sound->SetHeld(bHeld || Screen->HoldsGame());
	}
	UPadPortsSubsystem* Ports = GetGameInstance()->GetSubsystem<UPadPortsSubsystem>();
	UKeyListenerSubsystem* Listener = GetGameInstance()->GetSubsystem<UKeyListenerSubsystem>();
	auto Chords = Listener ? Listener->TakeChords() : TArray<Weltall::EControlAction, TFixedAllocator<Weltall::PortActionCount>>();
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
		// A key pressed while the Controls page listens is the one it binds.
		const bool bKeyTaken = Listener != nullptr && Listener->IsListening();
		auto Actions = Controller->ReadPortKeys();
		for (const Weltall::EControlAction Action : Chords)
		{
			Actions.AddUnique(Action);
		}
		for (const Weltall::EControlAction Action : Actions)
		{
			if (!bKeyTaken)
			{
				Screen->PressPortKey(Action);
			}
		}
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
		const uint16 Touched = Touch->TakeHeld().Buttons;
		Held.Buttons |= Touched;
		// The touch screen's camera buttons always turn: it has no stick to turn with.
		Held.ZoomButtons &= static_cast<uint16>(~Touched);
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
