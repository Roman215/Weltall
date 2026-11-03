#include "Field/FieldDrawingTables.h"
#include "Game/FieldProgram.h"
#include "Game/GameState.h"
#include "Graphics/BeetleTextureNames.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <algorithm>

using namespace Weltall;

// Both Perfect ART Works and the Xenogears HD Portraits Project, Beetle PSX HW packs, name Fei's face 46c8e27-bc2f6e29:
// directory 4's record 492 uploaded whole, through its own 256 colours.
TEST_CASE("A message's face is named as Beetle PSX HW's packs name it", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameState& State = Game->Session->Game();
	State.SetParty({0, 1, 2});
	State.SetJoinedCharacters(0x1FF);
	// Map 19's dinner at Citan's house, which opens on Fei's face.
	State.SetSavedEventVariable(0, 13);
	Game->EnterField(19);
	Game->RunUntil([&] { return Game->Loop->Field()->Runtime().Context().bFieldRunning; }, 100);
	constexpr int16 Fei = 0;
	std::size_t FeiSlot = FFieldDrawingTables::FaceSlots;
	Game->RunUntil([&] {
		const auto& Portraits = Game->Loop->Field()->Runtime().Context().Messages.Portraits;
		for (std::size_t Slot = 0; Slot < FFieldDrawingTables::FaceSlots; ++Slot)
		{
			if (Portraits[Slot].Character == Fei && Portraits[Slot].Load == FFieldPortraitSlot::Ready)
			{
				FeiSlot = Slot;
			}
		}
		return FeiSlot != FFieldDrawingTables::FaceSlots;
	}, 200);
	REQUIRE(FeiSlot < FFieldDrawingTables::FaceSlots);
	Game->Frame();

	const FFieldPresenter& Drawing = Game->Loop->Field()->Drawing();
	const FFieldImagePlace& Place = Drawing.Tables().FacePlaces[2 * FeiSlot];
	FTexturePage Page;
	Page.X = Place.X / 64 * 64;
	Page.Y = Place.Y / 256 * 256;
	Page.Depth = ETextureDepth::Indexed8;
	const uint32 View = PackTextureView(Page, {Place.ClutX, Place.ClutY});
	const FTextureRegion Face{View, static_cast<uint8>((Place.X - Page.X) * 2), static_cast<uint8>(Place.Y - Page.Y), 64, 64};
	const std::vector<FBeetleTexturePart> Parts = BeetleTextureParts(Drawing.Vram(), Face);
	REQUIRE(Parts.size() == 1);
	CHECK(FBeetleTextureName{Parts[0].Upload, Parts[0].Palette} == FBeetleTextureName{0x46C8E27, 0xBC2F6E29});
	CHECK((Parts[0].UploadUnitsWide == 32 && Parts[0].UploadUnitsHigh == 64));
	CHECK((Parts[0].FromU == 0 && Parts[0].FromV == 0 && Parts[0].U == 0 && Parts[0].V == 0));
	CHECK((Parts[0].Width == 64 && Parts[0].Height == 64));
	CHECK(Parts[0].Kind == EVramUploadKind::Portraits);

	// Beside it, the window's typed lines are text, the map's pages scenery and the party's frames characters.
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		Game->Frame();
	}
	const auto Holds = [&Drawing](EVramUploadKind Kind)
	{
		const auto Held = Drawing.Vram().UploadParts();
		return std::any_of(Held.begin(), Held.end(), [Kind](const FVramUploadPart& Part) { return Part.Upload->Kind == Kind; });
	};
	CHECK(Holds(EVramUploadKind::Text));
	CHECK(Holds(EVramUploadKind::Scenery));
	CHECK(Holds(EVramUploadKind::Characters));
}
