#include "RenderRoute.h"

#include "Field/EventContext.h"
#include "Game/FieldProgram.h"
#include "Game/GameLoop.h"
#include "Game/WorldMapProgram.h"
#include "Menu/MenuButtonIcons.h"
#include "Screens/GameLoopScreen.h"
#include "WorldMap/WorldMapContext.h"

#include <array>
#include <cmath>
#include <limits>
#include <numbers>

DEFINE_LOG_CATEGORY_STATIC(LogRenderRoute, Log, All);

namespace
{
	using Weltall::EPadButton;
	using Weltall::FPadState;

	constexpr uint16 Up = FPadState::Bit(EPadButton::Up);
	constexpr uint16 Right = FPadState::Bit(EPadButton::Right);
	constexpr uint16 Down = FPadState::Bit(EPadButton::Down);
	constexpr uint16 Left = FPadState::Bit(EPadButton::Left);
	/** The logical button that confirms. */
	constexpr uint16 ConfirmRole = 0x20;
	/** How near a point the leader stands to have reached it: the field's units, the world's. */
	constexpr double FieldNear = 12.0;
	constexpr double WorldNear = 24.0;
	/** The confirm button's rhythm at a confirm point: down, then up, so each press is new. */
	constexpr int32 PressFrames = 4;

	/** An angle of the game's (4096 a turn) of the way along Across and Along, as atan2 gives it. */
	int32 GameAngle(double Across, double Along)
	{
		return static_cast<int32>(std::lround(std::atan2(Across, Along) * 2048.0 / std::numbers::pi));
	}

	/** The eighth of a turn an angle of the game's lies nearest. */
	int32 Eighth(int32 Angle)
	{
		return (((Angle & 0xFFF) + 0x100) >> 9) & 7;
	}
}

bool FRenderRoute::Parse(const FString& Params, FRenderRoute& Route)
{
	FString Text;
	if (!FParse::Value(*Params, TEXT("Route="), Text, false))
	{
		return true;
	}
	Route.bBattles = FParse::Param(*Params, TEXT("RouteBattles"));
	TArray<FString> Parts;
	Text.ParseIntoArray(Parts, TEXT(";"));
	for (FString Part : Parts)
	{
		FPoint& Point = Route.Points.AddDefaulted_GetRef();
		Point.bConfirms = Part.RemoveFromStart(TEXT("!"));
		FString X;
		FString Z;
		if (!Part.Split(TEXT(","), &X, &Z) || !X.IsNumeric() || !Z.IsNumeric())
		{
			UE_LOG(LogRenderRoute, Error, TEXT("-Route takes points x,z, each after a ';', a confirm point with a leading '!': '%s'"), *Part);
			return false;
		}
		Point.X = FCString::Atoi(*X);
		Point.Z = FCString::Atoi(*Z);
	}
	return true;
}

void FRenderRoute::Pass()
{
	PointProgram.Reset();
	++Next;
}

FPadState FRenderRoute::Step(FGameLoopScreen& Game)
{
	++Frame;
	Weltall::FGameLoop* Loop = Game.Loop();
	if (Loop == nullptr || Next >= Points.Num())
	{
		return {};
	}
	const FPoint& Point = Points[Next];
	Weltall::FFieldProgram* Field = Loop->Field();
	Weltall::FWorldMapProgram* WorldMap = Loop->WorldMap();
	const int32 Map = Field != nullptr ? Field->Runtime().Context().RunningMap : -1;
	// A point is passed once the program or the map it was begun on has changed: a way out leads on before its point.
	if (!PointProgram.IsSet())
	{
		PointProgram = Loop->Program();
		PointMap = Map;
	}
	else if (*PointProgram != Loop->Program() || PointMap != Map)
	{
		Pass();
		return {};
	}
	uint16 Toward = 0;
	double Distance = 0.0;
	if (Field != nullptr)
	{
		Weltall::FEventContext& Context = Field->Runtime().Context();
		if (!bBattles)
		{
			Context.Encounters.Period = 0;
			Context.Encounters.Pending.fill(Weltall::FFieldEncounters::NoEncounter);
		}
		// The field walks the cosine and the negated sine of a direction: each button's own (right 0, down a quarter
		// turn on) less the view's yaw.
		static constexpr std::array<uint16, 8> ByEighth = {Right, Right | Down, Down, Down | Left, Left, Left | Up, Up, Up | Right};
		const Weltall::FFieldPoint& At = Context.States[Context.LeaderActor].Position;
		const int32 Dx = Point.X - (At[0] >> 16);
		const int32 Dz = Point.Z - (At[2] >> 16);
		Distance = std::hypot(Dx, Dz);
		Toward = ByEighth[Eighth(GameAngle(-Dz, Dx) + Context.Camera.GoalViewYaw + Context.ViewTurn)];
		if (!Point.bConfirms && Distance <= FieldNear)
		{
			Pass();
			return {};
		}
	}
	else if (WorldMap != nullptr)
	{
		Weltall::FWorldMapContext& Context = WorldMap->Runtime().Context();
		if (!bBattles)
		{
			Context.Encounters.Timers.fill(std::numeric_limits<int16>::max());
		}
		// The world map walks the sine and the negated cosine of a heading: up along the view's yaw, right a quarter
		// turn on.
		static constexpr std::array<uint16, 8> ByEighth = {Up, Up | Right, Right, Right | Down, Down, Down | Left, Left, Left | Up};
		const Weltall::FWorldMapVector& At = Context.Tasks[Weltall::FWorldMapContext::LeaderSlot].Position;
		const int32 Dx = Point.X - (At.X >> 12);
		const int32 Dz = Point.Z - (At.Z >> 12);
		Distance = std::hypot(Dx, Dz);
		const int32 Yaw = (Context.CameraYaw + Context.ViewTurn) & 0xFFF;
		Toward = ByEighth[Eighth(GameAngle(Dx, -Dz) - Yaw)];
		if (!Point.bConfirms && Distance <= WorldNear)
		{
			Pass();
			return {};
		}
	}
	else
	{
		return {};
	}
	if (!Point.bConfirms)
	{
		return FPadState{Toward};
	}
	// At a confirm point: on toward it while still far, the button pressed and let go the while.
	const uint16 Confirm = FPadState::Bit(Weltall::MenuRoleButton(ConfirmRole, Game.HasJapaneseCommands()));
	const uint16 Press = Frame % (2 * PressFrames) < PressFrames ? Confirm : 0;
	return FPadState{static_cast<uint16>((Distance > (Field != nullptr ? FieldNear : WorldNear) ? Toward : 0) | Press)};
}
