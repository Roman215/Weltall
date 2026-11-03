#include "Screens/WorldMapScreen.h"

#include "Game/GameSession.h"
#include "Game/GameState.h"
#include "Game/WorldMapProgram.h"
#include "Media/DiscIdentification.h"
#include "Media/GameFileIndex.h"
#include "Media/Iso9660.h"
#include "Media/PsxExecutable.h"
#include "Timing/PsxTiming.h"
#include "WeltallLog.h"
#include "WorldMapView.h"

namespace
{
	/** The most vertical blanks a frame of the world map lasts: a tick's two. */
	constexpr std::size_t MostVblanksPerWorldMapFrame = 2;
	constexpr uint8 NoCharacter = 0xFF;
	/** The world map's places are counted from this place number. */
	constexpr uint16 FirstWorldMapPlace = 0x400;

	class FBlankFrame final : public IScreenFrame
	{
	public:
		virtual void AddPasses(FRDGBuilder&, FRDGTextureRef, const FScreenArea&) const override {}
	};
}

TSharedPtr<FWorldMapScreen> FWorldMapScreen::Open(const Weltall::FIso9660& Disc, FStartSound StartSound, const FWorldMapStart& Start,
	const FWorldMapRenderOptions& Options)
{
	TSharedRef<FWorldMapScreen> Screen = MakeShared<FWorldMapScreen>(Disc, Options);
	Weltall::FGameSoundSession* Sound = StartSound();
	if (Sound == nullptr || !Screen->Start(*Sound, Start, Options))
	{
		return nullptr;
	}
	return Screen;
}

FWorldMapScreen::FWorldMapScreen(const Weltall::FIso9660& InDisc, const FWorldMapRenderOptions& Options)
	: Disc(InDisc), Vblanks(Weltall::PsxTiming::FrameSeconds), View(MakeUnique<FWorldMapView>(Options)), Blank(MakeShared<FBlankFrame>())
{
	HeldPerVblank.reserve(MostVblanksPerWorldMapFrame);
}

FWorldMapScreen::~FWorldMapScreen() = default;

bool FWorldMapScreen::Start(Weltall::FGameSoundSession& Sound, const FWorldMapStart& Start, const FWorldMapRenderOptions& Options)
{
	auto Read = Weltall::ReadBootExecutable(Disc);
	auto Identity = Weltall::IdentifyDisc(Disc);
	if (!Read || !Identity)
	{
		UE_LOG(LogWeltall, Error, TEXT("The disc's program cannot be read"));
		return false;
	}
	Executable = std::make_unique<Weltall::FPsxExecutable>(std::move(*Read));
	auto FileIndex = Weltall::FGameFileIndex::Read(*Executable);
	if (!FileIndex)
	{
		UE_LOG(LogWeltall, Error, TEXT("The disc's file index cannot be read: %hs"), FileIndex.Error().Message.c_str());
		return false;
	}
	Index = std::make_unique<Weltall::FGameFileIndex>(std::move(*FileIndex));
	auto Started = Weltall::FGameSession::PowerOn(Disc.Image(), *Index, *Executable, *Identity, Sound);
	if (!Started)
	{
		UE_LOG(LogWeltall, Error, TEXT("The game cannot start: %hs"), Started.Error().Message.c_str());
		return false;
	}
	Session = std::move(*Started);
	Weltall::FPlayerSettings Settings = Session->Settings();
	Settings.Fixes.WorldMap = Start.Fixes;
	Settings.WorldMapCamera = Start.View;
	Settings.Field.GearModels = Start.GearModels;
	Session->SetSettings(Settings);

	// What the field leaves the world map: the story, the party with their gears and who rides, where and how to arrive.
	Weltall::FGameState& Game = Session->Game();
	Game.SetSavedEventVariable(0, Start.Story);
	Game.SetParty(Start.Party);
	Game.SetPartyOrder(Start.Order);
	Game.SetCharacterLooks(Start.Looks);
	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		const uint8 Character = Start.Party[static_cast<std::size_t>(Slot)];
		if (Character != NoCharacter)
		{
			Game.SetCharacterGear(Character, Character);
		}
		Game.SetPartySlotInGear(Slot, Start.bInGears && Character != NoCharacter ? 1 : 0);
	}
	Game.SetWorldMapPlace(static_cast<uint16>(FirstWorldMapPlace + Start.Location), Start.Entrance, Start.Yaw, Start.Scene);
	if (Start.Vehicle)
	{
		const std::array<int16, 4>& Vehicle = *Start.Vehicle;
		Game.SetWorldMapVehicle(Vehicle[0], Vehicle[1], Vehicle[2], Vehicle[3]);
		Game.SetWorldMapFlags(Start.VehicleFlags);
	}
	auto Entered = Weltall::FWorldMapProgram::Start(*Session);
	if (!Entered)
	{
		UE_LOG(LogWeltall, Error, TEXT("The world map cannot start: %hs"), Entered.Error().Message.c_str());
		return false;
	}
	Program = std::move(*Entered);
	Program->SetCulling(Options.Culling);
	View->Follow(*Program, true, nullptr);
	return true;
}

void FWorldMapScreen::Update(double DeltaSeconds, Weltall::FPadState Held)
{
	if (!Program || bStopped)
	{
		return;
	}
	Latch.Sample(Held);
	const int32 Due = Vblanks.Advance(DeltaSeconds);
	for (int32 Vblank = 0; Vblank < Due && !bStopped; ++Vblank)
	{
		HeldPerVblank.push_back(Latch.TakeForTick());
		if (static_cast<int32>(HeldPerVblank.size()) >= Program->VblanksPerFrame())
		{
			RunFrame();
		}
	}
}

void FWorldMapScreen::RunFrame()
{
	if (auto Error = Program->Frame(HeldPerVblank))
	{
		UE_LOG(LogWeltall, Error, TEXT("The world map stopped: %hs"), Error->Message.c_str());
		bStopped = true;
	}
	HeldPerVblank.clear();
	if (Program->State() != Weltall::EGameProgramState::Running)
	{
		UE_LOG(LogWeltall, Display, TEXT("The world map ended, handing over to program %d"), static_cast<int32>(Session->Kernel().NextProgram));
		bStopped = true;
	}
	View->Follow(*Program, false, nullptr);
}

void FWorldMapScreen::AddViewInput(const Weltall::FFieldViewInput& Input)
{
	if (Program && !bStopped)
	{
		Program->AddViewInput(Input);
	}
}

TSharedRef<const IScreenFrame> FWorldMapScreen::CaptureFrame() const
{
	if (!Program)
	{
		return Blank;
	}
	// The display lies between the last two frames: as far past the latest as the vertical blanks since it say.
	const float Alpha = bStopped ? 1.0f
		: FMath::Clamp((static_cast<float>(HeldPerVblank.size()) + Vblanks.Alpha()) / static_cast<float>(Program->VblanksPerFrame()), 0.0f, 1.0f);
	const TSharedPtr<const IScreenFrame> Frame = View->Capture(*Program, Alpha, nullptr);
	return Frame ? Frame.ToSharedRef() : Blank;
}
