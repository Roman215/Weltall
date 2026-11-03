#include "Formats/Tim.h"
#include "Game/CharacterLooks.h"
#include "Game/IdsPictures.h"
#include "Graphics/Vram.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Menu/MenuPlay.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <vector>

using namespace Weltall;

// The party's arranging (Right from the Normal Menu's arc) switches the look of the member under the cursor with Square:
// the second Citan with his sword or without, the grown Emeralda or the child; in a clear game Elly's hair, Fei as Id.
namespace
{
	constexpr uint8 Fei = 0;
	constexpr uint8 Elly = 1;
	constexpr uint8 Citan = 2;
	constexpr uint8 Emeralda = 6;
	constexpr uint8 SwordCitan = 9;
	constexpr uint8 GrownEmeralda = 10;
	constexpr uint8 Buzzer = 4;
	constexpr uint8 Chime = 0x37;

	class FRecordedEffects final : public IMenuSound
	{
	public:
		void PlaySystemEffect(uint8 Effect) override { Effects.push_back(Effect); }
		void Pause() override {}
		void Resume() override {}
		int32 OutputMode() const override { return 1; }
		void SetOutputMode(int32) override {}

		std::vector<uint8> Effects;
	};

	bool Drawn(const TResult<bool>& Frame)
	{
		if (!Frame)
		{
			FAIL(Frame.Error().Message);
		}
		return *Frame;
	}

	void Press(FMenuPlay& Menu, EPadButton Button, int32 Frames = 8)
	{
		REQUIRE(Drawn(Menu.Frame({FPadState::Bit(Button)})));
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			REQUIRE(Drawn(Menu.Frame({})));
		}
	}

	/** A new game's Normal Menu with Fei, Member (made from Record as the story makes the later Citan and Emeralda) and Elly. */
	std::unique_ptr<FMenuPlay> OpenMenu(const FIso9660& Disc, uint8 Record, uint8 Member, IMenuSound* Sound)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		auto Play = FMenuPlay::NewGame(Disc.Image(), *Executable, 1, {EMenuMode::Normal, 0, false}, Sound, nullptr, nullptr, nullptr, true);
		REQUIRE(Play);
		FGameState& Game = (*Play)->Game();
		if (Member != Record)
		{
			REQUIRE(Game.CopyCharacter(Record, Member));
		}
		Game.SetParty({Fei, Member, Elly});
		Game.SetJoinedCharacters(static_cast<uint16>(Game.JoinedCharacters() | 1u << Elly | 1u << Member));
		return std::move(*Play);
	}

	/** Onto the panels and down to the second, the member there. */
	void OntoSecondPanel(FMenuPlay& Menu)
	{
		for (int32 Frame = 0; Frame < 60; ++Frame)
		{
			REQUIRE(Drawn(Menu.Frame({})));
		}
		Press(Menu, EPadButton::Right);
		Press(Menu, EPadButton::Down);
	}

	bool Has(const std::vector<uint8>& Effects, uint8 Effect)
	{
		return std::find(Effects.begin(), Effects.end(), Effect) != Effects.end();
	}
}

TEST_CASE("Square on the second Citan's panel puts his sword away and back, with the menus' chime", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FRecordedEffects Sound;
	std::unique_ptr<FMenuPlay> Play = OpenMenu(*Disc, Citan, SwordCitan, &Sound);
	FMenuPlay& Menu = *Play;
	OntoSecondPanel(Menu);
	Sound.Effects.clear();
	Press(Menu, EPadButton::Square);
	CHECK(Has(Sound.Effects, Chime));
	CHECK(Menu.Game().CharacterLooks().Drawn(SwordCitan) == Citan);
	// Only the drawing: the party and its record are the story's.
	CHECK(Menu.Game().Party() == std::array<uint8, 3>{Fei, SwordCitan, Elly});
	Press(Menu, EPadButton::Square);
	CHECK_FALSE(Menu.Game().CharacterLooks().IsSet());
}

TEST_CASE("Square buzzes on a member with no other look, and changes nothing", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FRecordedEffects Sound;
	// The first Citan, before the story gives him his sword: nothing to switch yet.
	std::unique_ptr<FMenuPlay> Play = OpenMenu(*Disc, Citan, Citan, &Sound);
	FMenuPlay& Menu = *Play;
	OntoSecondPanel(Menu);
	Sound.Effects.clear();
	Press(Menu, EPadButton::Square);
	CHECK(Has(Sound.Effects, Buzzer));
	CHECK_FALSE(Has(Sound.Effects, Chime));
	// Up to Fei: the same.
	Press(Menu, EPadButton::Up);
	Sound.Effects.clear();
	Press(Menu, EPadButton::Square);
	CHECK(Has(Sound.Effects, Buzzer));
	CHECK_FALSE(Menu.Game().CharacterLooks().IsSet());
}

TEST_CASE("The grown Emeralda switched to the child shows the child's portrait on her panel", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::unique_ptr<FMenuPlay> Grown = OpenMenu(*Disc, Emeralda, GrownEmeralda, nullptr);
	std::unique_ptr<FMenuPlay> Child = OpenMenu(*Disc, Emeralda, Emeralda, nullptr);
	OntoSecondPanel(*Grown);
	OntoSecondPanel(*Child);
	const auto Units = [](const FMenuPlay& Menu) { return std::vector<uint16>(Menu.Menu().Vram().ConsoleUnits().begin(), Menu.Menu().Vram().ConsoleUnits().end()); };
	const std::vector<uint16> Before = Units(*Grown);
	Press(*Grown, EPadButton::Square);
	Press(*Child, EPadButton::Square);
	const std::vector<uint16> After = Units(*Grown);
	const std::vector<uint16> AsChild = Units(*Child);
	// What the switch changed is the portrait's place, and there VRAM is the child's menu's to the unit. (The panels' name
	// strips hold names in pairs, so the pair of the grown Emeralda's differs elsewhere unseen.)
	int32 Changed = 0;
	int32 Unlike = 0;
	for (std::size_t At = 0; At < After.size(); ++At)
	{
		if (After[At] != Before[At])
		{
			++Changed;
			Unlike += After[At] == AsChild[At] ? 0 : 1;
		}
	}
	CHECK(Changed > 0);
	CHECK(Unlike == 0);
	CHECK(Grown->Game().CharacterLooks().Drawn(GrownEmeralda) == Emeralda);
}

namespace
{
	/** A colour of Elly's orange (hue 7-43 degrees, strongly coloured unless a Strength is given, not near black). */
	bool Orange(uint16 Colour, double Strength = 0.55)
	{
		const double R = (Colour & 31) / 31.0;
		const double G = (Colour >> 5 & 31) / 31.0;
		const double B = (Colour >> 10 & 31) / 31.0;
		const double Max = std::max({R, G, B});
		const double Min = std::min({R, G, B});
		if (Max <= 1.0 / 31.0 || Max == Min || Max != R)
		{
			return false;
		}
		const double Hue = 60.0 * (G - B) / (Max - Min);
		return Hue >= 7.2 && Hue <= 43.2 && (Max - Min) / Max >= Strength;
	}

	/** The pixels of an 8-bit picture in the menu's VRAM (Width pixels from unit X, Height rows from Y) drawn orange through CLUT row Row. */
	int32 OrangeIn(const FMenuPlay& Menu, int32 X, int32 Y, int32 Width, int32 Height, int32 Row, double Strength = 0.55)
	{
		const std::span<const uint16> Units = Menu.Menu().Vram().ConsoleUnits();
		int32 Count = 0;
		for (int32 Line = Y; Line < Y + Height; ++Line)
		{
			for (int32 Pixel = 0; Pixel < Width; ++Pixel)
			{
				const uint16 Unit = Units[static_cast<std::size_t>(Line * 1024 + X + Pixel / 2)];
				const int32 Index = Pixel % 2 == 0 ? Unit & 0xFF : Unit >> 8;
				Count += Orange(Units[static_cast<std::size_t>(Row * 1024 + Index)], Strength) ? 1 : 0;
			}
		}
		return Count;
	}

	/** Onto the third panel, Elly's in OpenMenu's party. */
	void OntoThirdPanel(FMenuPlay& Menu)
	{
		OntoSecondPanel(Menu);
		Press(Menu, EPadButton::Down);
	}
}

TEST_CASE("Square on Elly's panel gives her purple hair only in a clear game, with the menus' chime", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FRecordedEffects Sound;
	std::unique_ptr<FMenuPlay> Story = OpenMenu(*Disc, Citan, Citan, &Sound);
	OntoThirdPanel(*Story);
	Sound.Effects.clear();
	Press(*Story, EPadButton::Square);
	CHECK(Has(Sound.Effects, Buzzer));
	CHECK_FALSE(Story->Game().CharacterLooks().IsSet());

	std::unique_ptr<FMenuPlay> Clear = OpenMenu(*Disc, Citan, Citan, &Sound);
	Clear->Game().SetClearGame(EClearGame::ClearFile);
	OntoThirdPanel(*Clear);
	Sound.Effects.clear();
	Press(*Clear, EPadButton::Square);
	CHECK(Has(Sound.Effects, Chime));
	CHECK(Clear->Game().CharacterLooks().HasPurpleHair(Elly));
	CHECK(Clear->Game().Party() == std::array<uint8, 3>{Fei, Citan, Elly});
}

TEST_CASE("With purple hair Elly's small face and large portrait hold no orange, and switched back are the disc's again", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::unique_ptr<FMenuPlay> Play = OpenMenu(*Disc, Citan, Citan, nullptr);
	FMenuPlay& Menu = *Play;
	Menu.Game().SetClearGame(EClearGame::ClearFile);
	OntoThirdPanel(Menu);
	// Her small face (sheet frame 14Fh: 24 pixels square at (484, 48), CLUT row 506) and the third panel's large portrait
	// (frame 14Dh: 48 square at (448, 48), CLUT row 508).
	const auto Face = [](const FMenuPlay& Shown) { return OrangeIn(Shown, 484, 48, 24, 24, 506); };
	const auto Portrait = [](const FMenuPlay& Shown) { return OrangeIn(Shown, 448, 48, 48, 48, 508); };
	// The face's left edge below her eyes, where her hair meets the panel behind her: its orange paler there.
	const auto FaceEdge = [](const FMenuPlay& Shown) { return OrangeIn(Shown, 484, 48 + 13, 5, 11, 506, 0.35); };
	const auto Units = [](const FMenuPlay& Shown) { return std::vector<uint16>(Shown.Menu().Vram().ConsoleUnits().begin(), Shown.Menu().Vram().ConsoleUnits().end()); };
	const std::vector<uint16> Before = Units(Menu);
	const int32 FaceBefore = Face(Menu);
	const int32 PortraitBefore = Portrait(Menu);
	CHECK(FaceBefore > 150);
	CHECK(PortraitBefore > 1000);
	CHECK(FaceEdge(Menu) > 8);
	Press(Menu, EPadButton::Square);
	// Only the shading of her skin under the jaw keeps its colour: three pixels of the small face.
	CHECK(Face(Menu) <= 3);
	CHECK(FaceEdge(Menu) == 0);
	CHECK(Portrait(Menu) * 50 < PortraitBefore);
	Press(Menu, EPadButton::Square);
	CHECK(Units(Menu) == Before);
}

namespace
{
	std::vector<uint16> VramUnits(const FMenuPlay& Menu)
	{
		const std::span<const uint16> All = Menu.Menu().Vram().ConsoleUnits();
		return {All.begin(), All.end()};
	}

	/** Width × Height units of the menu's VRAM from (X, Y), row by row. */
	std::vector<uint16> UnitsIn(const FMenuPlay& Menu, int32 X, int32 Y, int32 Width, int32 Height)
	{
		const std::span<const uint16> All = Menu.Menu().Vram().ConsoleUnits();
		std::vector<uint16> Rect;
		for (int32 Row = Y; Row < Y + Height; ++Row)
		{
			const auto Start = All.begin() + Row * 1024 + X;
			Rect.insert(Rect.end(), Start, Start + Width);
		}
		return Rect;
	}

	struct FVramArea
	{
		int32 X = 0;
		int32 Y = 0;
		int32 Width = 0;
		int32 Height = 0;

		bool Holds(std::size_t Unit) const
		{
			const int32 UnitX = static_cast<int32>(Unit % 1024);
			const int32 UnitY = static_cast<int32>(Unit / 1024);
			return UnitX >= X && UnitX < X + Width && UnitY >= Y && UnitY < Y + Height;
		}
	};

	/** Fei's small face (sheet frame 14Eh: 24 pixels square at (472, 48), CLUT row 507) and the first panel's large portrait (frame 14Bh: 48 square at (448, 0), CLUT row 510). */
	constexpr FVramArea SmallFacePixels{472, 48, 12, 24};
	constexpr FVramArea SmallFaceClut{0, 507, 256, 1};
	constexpr FVramArea PortraitPixels{448, 0, 24, 48};
	constexpr FVramArea PortraitClut{0, 510, 256, 1};

	void OntoFirstPanel(FMenuPlay& Menu)
	{
		OntoSecondPanel(Menu);
		Press(Menu, EPadButton::Up);
	}
}

TEST_CASE("Square on Fei's panel draws him as Id only in a clear game, with the menus' chime", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FRecordedEffects Sound;
	std::unique_ptr<FMenuPlay> Story = OpenMenu(*Disc, Citan, Citan, &Sound);
	OntoFirstPanel(*Story);
	Sound.Effects.clear();
	Press(*Story, EPadButton::Square);
	CHECK(Has(Sound.Effects, Buzzer));
	CHECK_FALSE(Story->Game().CharacterLooks().IsSet());

	std::unique_ptr<FMenuPlay> Clear = OpenMenu(*Disc, Citan, Citan, &Sound);
	Clear->Game().SetClearGame(EClearGame::ClearFile);
	OntoFirstPanel(*Clear);
	Sound.Effects.clear();
	Press(*Clear, EPadButton::Square);
	CHECK(Has(Sound.Effects, Chime));
	CHECK(Clear->Game().CharacterLooks().IsId(Fei));
	CHECK(Clear->Game().Party() == std::array<uint8, 3>{Fei, Citan, Elly});
}

TEST_CASE("Fei alone in a New Game+ is reached on his panel to be drawn as Id, Confirm buzzing with nobody to swap", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FRecordedEffects Sound;
	std::unique_ptr<FMenuPlay> Play = OpenMenu(*Disc, Citan, Citan, &Sound);
	FMenuPlay& Menu = *Play;
	Menu.Game().SetClearGame(EClearGame::NewGamePlus);
	Menu.Game().SetParty({Fei, FGameState::EmptySlot, FGameState::EmptySlot});
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		REQUIRE(Drawn(Menu.Frame({})));
	}
	Press(Menu, EPadButton::Right);
	Sound.Effects.clear();
	Press(Menu, EPadButton::Cross);
	CHECK(Has(Sound.Effects, Buzzer));
	CHECK_FALSE(Menu.Game().PartyOrder().IsSet());
	Sound.Effects.clear();
	Press(Menu, EPadButton::Square);
	CHECK(Has(Sound.Effects, Chime));
	CHECK(Menu.Game().CharacterLooks().IsId(Fei));
	CHECK(Menu.Game().Party() == std::array<uint8, 3>{Fei, FGameState::EmptySlot, FGameState::EmptySlot});
}

TEST_CASE("As Id, Fei's portrait and small face are made from Id's face and his panel names him Id; switched back, the disc's again", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const TResult<std::vector<uint8>> Face = IdsPictures::ReadFace(Disc->Image(), *Index);
	REQUIRE(Face);
	const TResult<FTim> SmallFace = IdsPictures::SmallFace(*Face);
	const TResult<FTim> Portrait = IdsPictures::Portrait(*Face);
	REQUIRE(SmallFace);
	REQUIRE(Portrait);

	std::unique_ptr<FMenuPlay> Play = OpenMenu(*Disc, Citan, Citan, nullptr);
	FMenuPlay& Menu = *Play;
	Menu.Game().SetClearGame(EClearGame::ClearFile);
	OntoFirstPanel(Menu);
	const std::vector<uint16> Before = VramUnits(Menu);
	Press(Menu, EPadButton::Square);
	REQUIRE(Menu.Game().CharacterLooks().IsId(Fei));
	CHECK(UnitsIn(Menu, SmallFacePixels.X, SmallFacePixels.Y, SmallFacePixels.Width, SmallFacePixels.Height) == SmallFace->Image.Units);
	CHECK(UnitsIn(Menu, SmallFaceClut.X, SmallFaceClut.Y, SmallFaceClut.Width, SmallFaceClut.Height) == SmallFace->Clut->Units);
	CHECK(UnitsIn(Menu, PortraitPixels.X, PortraitPixels.Y, PortraitPixels.Width, PortraitPixels.Height) == Portrait->Image.Units);
	CHECK(UnitsIn(Menu, PortraitClut.X, PortraitClut.Y, PortraitClut.Width, PortraitClut.Height) == Portrait->Clut->Units);

	// His name: the same menu with Fei named "Id" and drawn as himself differs from his only in the pictures.
	std::unique_ptr<FMenuPlay> Named = OpenMenu(*Disc, Citan, Citan, nullptr);
	const std::span<const uint8> Id = IdsPictures::Name();
	std::copy(Id.begin(), Id.end(), Named->Game().Block().begin());
	OntoFirstPanel(*Named);
	Press(*Named, EPadButton::Square);
	REQUIRE_FALSE(Named->Game().CharacterLooks().IsSet());
	const std::vector<uint16> AsId = VramUnits(Menu);
	const std::vector<uint16> AsNamed = VramUnits(*Named);
	REQUIRE(AsId.size() == AsNamed.size());
	int32 OutsidePictures = 0;
	int32 Renamed = 0;
	for (std::size_t At = 0; At < AsId.size(); ++At)
	{
		const bool bPicture = SmallFacePixels.Holds(At) || SmallFaceClut.Holds(At) || PortraitPixels.Holds(At) || PortraitClut.Holds(At);
		OutsidePictures += AsId[At] != AsNamed[At] && !bPicture ? 1 : 0;
		Renamed += AsId[At] != Before[At] && !bPicture ? 1 : 0;
	}
	CHECK(OutsidePictures == 0);
	CHECK(Renamed > 0);

	Press(Menu, EPadButton::Square);
	CHECK_FALSE(Menu.Game().CharacterLooks().IsSet());
	CHECK(VramUnits(Menu) == Before);
}
