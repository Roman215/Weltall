#pragma once

#include "CoreMinimal.h"
#include "Field/FieldRenderOptions.h"
#include "Game/CharacterLooks.h"
#include "Game/PartyOrder.h"
#include "Input/PadLatch.h"
#include "Rendering/WorldMapRendering.h"
#include "Screens/GameScreen.h"
#include "Timing/TickClock.h"
#include "WorldMap/WorldMapCameraOffsets.h"
#include "WorldMap/WorldMapOptions.h"

#include <array>
#include <memory>
#include <optional>
#include <vector>

class FWorldMapView;

namespace Weltall
{
	class FGameFileIndex;
	class FGameSession;
	class FGameSoundSession;
	class FIso9660;
	class FPsxExecutable;
	class FWorldMapProgram;
}

/** Where the world map screen starts: the game state's words the field hands the world map over in. */
struct FWorldMapStart
{
	/** Event variable 0, the story's value, which picks the era. */
	uint16 Story = 60;
	/** The place left (from 400h up) and the entrance id the party arrives by. */
	uint16 Location = 1;
	uint16 Entrance = 1;
	/** The camera's yaw, 4096 a turn. */
	uint16 Yaw = 0xC00;
	/** 1 the map; 2-5 the Yggdrasil's scripted voyages. */
	uint16 Scene = 1;
	/** The party's three characters (FFh none), each with the gear of its own number. */
	std::array<uint8, 3> Party{0, 2, 3};
	/** The order the player keeps the party in: the story's, unless set. */
	Weltall::FPartyOrder Order;
	/** The looks the player has switched (FCharacterLooks): the story's, unless set. */
	Weltall::FCharacterLooks Looks;
	bool bInGears = false;
	/** The Yggdrasil's place and heading (whole units), and the world map's flags (its kind, 2000h start aboard it, 4000h aboard). */
	std::optional<std::array<int16, 4>> Vehicle;
	uint16 VehicleFlags = 0;
	/** The world map's fixes: all on, as the player's settings start. */
	Weltall::FWorldMapOptions Fixes;
	/** The player's framing the map starts under: the game's own unless given. */
	Weltall::FWorldMapCameraOffsets View;
	/** Where the party's gears are drawn with their 3D models (the Display page's Gear sprites). */
	Weltall::EFieldGearModels GearModels = Weltall::EFieldGearModels::Off;
};

/**
 * The world map played from a place the field leaves for, as the kernel runs its program: the scene
 * entered from the game state the start gives, its ticks at the NTSC console's vertical blanks with the
 * controller latched at each. What it asks for next (the field, a battle, the menu) stops it, logged,
 * its last frame staying on screen.
 */
class WELTALL_API FWorldMapScreen final : public IGameScreen
{
public:
	/** Starts the game's sound afresh, as the console's start-up does, and returns it; null when it cannot. It must outlive the next call. */
	using FStartSound = TFunction<Weltall::FGameSoundSession*()>;

	/** The world map entered on a disc, which must outlive the screen; null, with the reason logged, when it cannot start. */
	static TSharedPtr<FWorldMapScreen> Open(const Weltall::FIso9660& Disc, FStartSound StartSound, const FWorldMapStart& Start,
		const FWorldMapRenderOptions& Options);

	FWorldMapScreen(const Weltall::FIso9660& InDisc, const FWorldMapRenderOptions& Options);
	virtual ~FWorldMapScreen() override;

	virtual void Update(double DeltaSeconds, Weltall::FPadState Held) override;
	virtual void AddViewInput(const Weltall::FFieldViewInput& Input) override;
	virtual TSharedRef<const IScreenFrame> CaptureFrame() const override;

	/** Whether the world map has stopped: at what it asked for next, or an error. */
	bool HasStopped() const { return bStopped; }

private:
	bool Start(Weltall::FGameSoundSession& Sound, const FWorldMapStart& Start, const FWorldMapRenderOptions& Options);
	void RunFrame();

	const Weltall::FIso9660& Disc;
	std::unique_ptr<Weltall::FPsxExecutable> Executable;
	std::unique_ptr<Weltall::FGameFileIndex> Index;
	std::unique_ptr<Weltall::FGameSession> Session;
	std::unique_ptr<Weltall::FWorldMapProgram> Program;
	Weltall::FTickClock Vblanks;
	Weltall::FPadLatch Latch;
	std::vector<Weltall::FPadState> HeldPerVblank;
	TUniquePtr<FWorldMapView> View;
	TSharedRef<const IScreenFrame> Blank;
	bool bStopped = false;
};
