#include "Field/FieldDrawingTables.h"
#include "Formats/Tim.h"
#include "Game/FieldProgram.h"
#include "Game/GameState.h"
#include "Media/GameFileIndex.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <algorithm>

using namespace Weltall;

// Map 552, the Soylent System's discovery: Elly speaks, two pictures flash over the faces' places through dissolves
// (which empty the faces' cache), then Citan speaks and Elly's face is read again into the slot it had before.
TEST_CASE("A face read again after a dissolve goes over the picture the scripts loaded where it was", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameState& State = Game->Session->Game();
	State.SetParty({0, 1, 2});
	State.SetJoinedCharacters(0x1FF);
	Game->EnterField(552);
	Game->RunUntil([&] { return Game->Loop->Field()->Runtime().Context().bFieldRunning; }, 100);

	constexpr int32 EllysDenial = 6;
	constexpr int16 Elly = 1;
	const auto DenialShown = [&] {
		const FEventContext& Context = Game->Loop->Field()->Runtime().Context();
		const std::span<const uint8> Denial = Context.Messages.Table.String(EllysDenial);
		return std::any_of(Context.Messages.Windows.begin(), Context.Messages.Windows.end(),
			[&](const FFieldMessageWindow& Window) { return Window.bOpen && Window.Message.data() == Denial.data(); });
	};
	constexpr int32 FrameLimit = 6000;
	int32 Frames = 0;
	for (; Frames < FrameLimit && !DenialShown(); ++Frames)
	{
		FPadState Held;
		Held.Buttons = Frames % 2 == 0 ? FPadState::Bit(EPadButton::Cross) : 0;
		REQUIRE_FALSE(Game->Frame(Held).has_value());
	}
	REQUIRE(Frames < FrameLimit);
	REQUIRE_FALSE(Game->Frame().has_value());

	const FEventContext& Context = Game->Loop->Field()->Runtime().Context();
	const auto& Portraits = Context.Messages.Portraits;
	const auto Slot = std::find_if(Portraits.begin(), Portraits.end(), [](const FFieldPortraitSlot& Cached) { return Cached.Character == Elly; });
	REQUIRE(Slot != Portraits.end());
	const FFieldPresenter& Drawing = Game->Loop->Field()->Drawing();
	const std::size_t SlotIndex = static_cast<std::size_t>(Slot - Portraits.begin());
	const std::optional<std::array<int32, 2>> Files = Drawing.Tables().FaceFiles(Elly);
	REQUIRE(Files.has_value());
	for (std::size_t Face = 0; Face < 2; ++Face)
	{
		const FIndexRecord* Record = Game->Index->Find(4, (*Files)[Face]);
		REQUIRE(Record != nullptr);
		const TResult<std::vector<uint8>> Bytes = ReadFileSectors(Game->Session->Image(), *Record);
		REQUIRE(Bytes);
		const TResult<FTim> Tim = ParseTim(*Bytes);
		REQUIRE(Tim);
		const FFieldImagePlace& Place = Drawing.Tables().FacePlaces[2 * SlotIndex + Face];
		const FVramRect& Own = Tim->Image.Rect;
		int32 Differing = 0;
		for (int32 Y = 0; Y < Own.Height; ++Y)
		{
			for (int32 X = 0; X < Own.Width; ++X)
			{
				Differing += Drawing.Vram().At(Place.X + X, Place.Y + Y) != Tim->Image.Units[static_cast<std::size_t>(Y * Own.Width + X)] ? 1 : 0;
			}
		}
		CHECK(Differing == 0);
	}
}
