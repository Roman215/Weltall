#include "Menu/MenuButtonIcons.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Menu/MenuDrawing.h"
#include "Menu/MenuResources.h"
#include "Support/BoundGlyphs.h"
#include "Support/GameDiscs.h"
#include "Text/MesFont.h"
#include "Text/MesGlyphs.h"
#include "Text/MesResident.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>
#include <vector>

using namespace Weltall;

// The player's button icons in the menus: which button a picture stands for, the port's own pictures,
// and a menu's draw list shown with each icon set.
namespace
{
	using FTestGlyphs = Testing::FBoundGlyphs;

	int32 Opaque(const FMenuIconPicture& Picture)
	{
		return static_cast<int32>(std::count_if(Picture.Texels.begin(), Picture.Texels.end(), [](uint16 Texel) { return Texel != 0; }));
	}

	float Across(const FDrawPrimitive& Drawn)
	{
		return Drawn.Corners[1].X - Drawn.Corners[0].X;
	}

	class FBindingGlyphs final : public IButtonGlyphs
	{
	public:
		FControlBindings Bindings = FControlBindings::Defaults();
		uint32 Changes = 0;
		FButtonGlyph Glyph(EPadButton Button) const override { return ButtonGlyph(Button, Bindings, EButtonIconSet::Keyboard); }
		uint32 Revision() const override { return Changes; }
	};
}

TEST_CASE("Switching to original button icons restores native menu pictures and text on the next frame", "[Menu][Content][Input]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Resources = FMenuResources::Load(Disc->Image(), *Index, *Executable);
	REQUIRE(Resources);
	const FMenuSpriteSheet& Sheet = Resources->Sheet();
	FMenuFrame Frame;
	Frame.Layer = MenuFlatTransform;
	Sheet.Draw(0xF9, 100, 50, 0x1000, Frame.Primitives);
	FMenuDrawBuilder Builder;
	FDrawList Original;
	Builder.Build(Frame, nullptr, 1.0f, Original);
	REQUIRE(Builder.Icons.size() == 1);
	FBindingGlyphs Glyphs;
	Glyphs.Bindings.Rebind(EControlAction::Cross, 0, "Gamepad_FaceButton_Right");
	FMenuButtonIcons Pictures;
	FDrawList Remapped = Original;
	Pictures.Apply(Remapped, Builder.Icons, Glyphs, Sheet, MenuSheetButtonPictures);
	CHECK(Remapped.Primitives[0].Texture->Vram == FMenuButtonIcons::PicturesVram);
	Glyphs.Bindings.bOriginalButtonIcons = true;
	++Glyphs.Changes;
	FDrawList Restored = Original;
	Pictures.Apply(Restored, Builder.Icons, Glyphs, Sheet, MenuSheetButtonPictures);
	CHECK(Restored.Primitives[0].Texture->Vram == 0);
	CHECK(Restored.Primitives[0].Corners[0].U == Original.Primitives[0].Corners[0].U);

	const FDrawTexture Page{0, FTexturePage{0x300, 0x100, ETextureDepth::Indexed4, EBlendMode::Average}, {}};
	FDrawList Text;
	Text.Primitives.push_back(MakeSprite(20, 30, 120, 13, Page, 8, 16));
	const std::array<FTextButtonCell, 4> Cells{{{0, 0, 12, EPadButton::Cross}, {0, 20, 12, EPadButton::L1},
		{0, 40, 30, EPadButton::Up, true}, {0, 80, 30, EPadButton::Start}}};
	Pictures.ApplyToText(Text, Cells, Glyphs);
	REQUIRE(Text.Primitives.size() == 1);
	CHECK(Text.Primitives[0].Texture->Vram == 0);
	CHECK(Text.Primitives[0].Corners[0].U == 8);
	CHECK(Text.Primitives[0].Corners[1].X == 140);
}

TEST_CASE("A menu's button picture stands for the controller's button the kernel reads as its role", "[Menu]")
{
	CHECK(MenuRoleButton(0x20) == EPadButton::Cross);
	CHECK(MenuRoleButton(0x40) == EPadButton::Circle);
	CHECK(MenuRoleButton(0x10) == EPadButton::Square);
	CHECK(MenuRoleButton(0x80) == EPadButton::Triangle);
	CHECK(MenuRoleButton(0x04) == EPadButton::L1);
	CHECK(MenuRoleButton(0x08) == EPadButton::R1);
	CHECK(MenuIconSlotOf(0x04) == EMenuIconSlot::Hint);
	CHECK(MenuIconSlotOf(0x20) == EMenuIconSlot::FaceButton);
}

TEST_CASE("A prompt set out as the four face buttons puts each button and its words where the player's button lies", "[Menu][Input]")
{
	// The attack prompt as the USA game draws it: the weak attack's triangle at the top, the strong's square at the left,
	// the fierce's cross at the bottom and cancel's circle at the right, each with its words beside it.
	struct FPlace
	{
		uint16 Role;
		int16 X;
		int16 Y;
	};
	constexpr std::array<FPlace, 4> Usa{{{0x80, 176, 96}, {0x10, 160, 112}, {0x20, 176, 128}, {0x40, 192, 112}}};
	std::vector<FMenuPrimitive> Prompt;
	for (const FPlace& Place : Usa)
	{
		FMenuPrimitive Button;
		Button.Code = MenuGpu::TexturedQuad;
		Button.ButtonRole = Place.Role;
		Button.PromptRole = Place.Role;
		for (std::size_t Corner = 0; Corner < Button.Corners.size(); ++Corner)
		{
			Button.Corners[Corner].X = static_cast<int16>(Place.X + (Corner & 1) * 12);
			Button.Corners[Corner].Y = static_cast<int16>(Place.Y + (Corner >> 1) * 12);
		}
		FMenuPrimitive Words = Button;
		Words.ButtonRole = 0;
		for (FMenuCorner& Corner : Words.Corners)
		{
			Corner.X = static_cast<int16>(Corner.X + 14);
		}
		Prompt.push_back(Button);
		Prompt.push_back(Words);
	}
	FMenuPrimitive Ring;
	Ring.Code = MenuGpu::TexturedQuad;
	Ring.Corners[0].X = 100;
	Prompt.push_back(Ring);
	const auto At = [](const std::vector<FMenuPrimitive>& Primitives, uint16 Role, bool bWords) {
		const auto Found = std::find_if(Primitives.begin(), Primitives.end(),
			[Role, bWords](const FMenuPrimitive& Primitive) { return Primitive.PromptRole == Role && (Primitive.ButtonRole == 0) == bWords; });
		REQUIRE(Found != Primitives.end());
		return std::pair{Found->Corners[0].X, Found->Corners[0].Y};
	};
	const auto Placed = [&](const IButtonGlyphs& Glyphs) {
		FMenuButtonIcons Pictures;
		std::vector<FMenuPrimitive> Shown = Prompt;
		Pictures.PlacePrompt(Shown, Glyphs);
		CHECK(Shown.back().Corners[0].X == 100);
		return Shown;
	};
	// Each role at the USA place given, its words still 14 pixels to its right.
	const auto Check = [&](const std::vector<FMenuPrimitive>& Shown, std::array<int32, 4> Places) {
		for (std::size_t Role = 0; Role < Usa.size(); ++Role)
		{
			const FPlace& Place = Usa[static_cast<std::size_t>(Places[Role])];
			INFO("role " << Usa[Role].Role);
			CHECK(At(Shown, Usa[Role].Role, false) == std::pair{Place.X, Place.Y});
			CHECK(At(Shown, Usa[Role].Role, true) == std::pair{static_cast<int16>(Place.X + 14), Place.Y});
		}
	};
	constexpr std::array<int32, 4> AsDrawn{0, 1, 2, 3};

	FBindingGlyphs Original;
	Original.Bindings.bOriginalButtonIcons = true;
	Check(Placed(Original), AsDrawn);
	// Circle confirms: the fierce attack's circle at the right, cancel's cross at the bottom.
	Original.Bindings.ChangeButtonLayout(EButtonLayout::CircleConfirms);
	++Original.Changes;
	Check(Placed(Original), {0, 1, 3, 2});
	// Japan: also the weak attack's square at the left and the strong's triangle at the top, as the Japanese release.
	Original.Bindings.ChangeButtonLayout(EButtonLayout::Japan);
	++Original.Changes;
	Check(Placed(Original), {1, 0, 3, 2});

	// An Xbox pad bound as the Japanese layout binds it: the same places, by where its letters lie.
	Testing::FBoundGlyphs Xbox(EButtonIconSet::Xbox);
	Xbox.Bind({{EPadButton::Triangle, "X"}, {EPadButton::Square, "Y"}, {EPadButton::Cross, "B"}, {EPadButton::Circle, "A"}});
	Check(Placed(Xbox), {1, 0, 3, 2});
	// Two buttons on one place, or a key: the game's places.
	Xbox.Bind({{EPadButton::Triangle, "Y"}, {EPadButton::Square, "X"}, {EPadButton::Cross, "A"}, {EPadButton::Circle, "A"}});
	Check(Placed(Xbox), AsDrawn);
	FBindingGlyphs Keyboard;
	Keyboard.Bindings.ChangeButtonLayout(EButtonLayout::Japan);
	Check(Placed(Keyboard), AsDrawn);

	// Cancel's words as long as "cancel end": at the bottom they fit, but at the left they would cover the right button.
	for (FMenuPrimitive& Words : Prompt)
	{
		if (Words.PromptRole == 0x40 && Words.ButtonRole == 0)
		{
			Words.Corners[1].X = static_cast<int16>(Words.Corners[1].X + 30);
			Words.Corners[3].X = static_cast<int16>(Words.Corners[3].X + 30);
		}
	}
	Check(Placed(Original), {1, 0, 3, 2});
	Xbox.Bind({{EPadButton::Triangle, "Y"}, {EPadButton::Square, "B"}, {EPadButton::Cross, "A"}, {EPadButton::Circle, "X"}});
	Check(Placed(Xbox), AsDrawn);
}

TEST_CASE("The port's button pictures fill the menus' slots, a key widened to its label", "[Menu]")
{
	const FMenuIconPicture Xbox = DrawMenuButtonIcon(EButtonIconSet::Xbox, "A", EMenuIconSlot::FaceButton);
	CHECK(Xbox.Width == 24);
	CHECK(Xbox.Height == 24);
	// Round: the corners are clear, the centre is drawn, and the letter is green.
	CHECK(Xbox.Texels[0] == 0);
	CHECK(Xbox.Texels[12 * 24 + 12] != 0);
	CHECK(std::any_of(Xbox.Texels.begin(), Xbox.Texels.end(), [](uint16 Texel) { return (Texel >> 5 & 31) > 20 && (Texel & 31) < 12; }));
	const FMenuIconPicture Shoulder = DrawMenuButtonIcon(EButtonIconSet::Xbox, "LB", EMenuIconSlot::Hint);
	CHECK(Shoulder.Width == 32);
	CHECK(Shoulder.Height == 18);
	CHECK(Opaque(Shoulder) == 32 * 18);
	const FMenuIconPicture Letter = DrawMenuButtonIcon(EButtonIconSet::Keyboard, "E", EMenuIconSlot::FaceButton);
	CHECK(Letter.Width == 24);
	const FMenuIconPicture Enter = DrawMenuButtonIcon(EButtonIconSet::Keyboard, "Enter", EMenuIconSlot::FaceButton);
	CHECK(Enter.Width >= 24);
	CHECK(Enter.Width % MenuIconTexelsPerPixel == 0);
	CHECK(Enter.Height == 24);
	const FMenuIconPicture Options = DrawMenuButtonIcon(EButtonIconSet::PlayStation, "Options", EMenuIconSlot::FaceButton);
	CHECK(Options.Width > 24);
	for (const EButtonIconSet Set : {EButtonIconSet::Keyboard, EButtonIconSet::Xbox, EButtonIconSet::PlayStation, EButtonIconSet::Nintendo, EButtonIconSet::SteamDeck})
	{
		for (const char* Icon : {"A", "B", "X", "Y", "Cross", "Circle", "Square", "Triangle", "L1", "RB", "ZL", "View", "Plus", "DPad_Up", "LeftStick_Left",
				 "SpaceBar", "Up", "MouseLeft", "F12", "NumPadOne"})
		{
			for (const EMenuIconSlot Slot : {EMenuIconSlot::FaceButton, EMenuIconSlot::Hint})
			{
				const FMenuIconPicture Picture = DrawMenuButtonIcon(Set, Icon, Slot);
				INFO(static_cast<int32>(Set) << " " << Icon << " " << static_cast<int32>(Slot));
				CHECK(Picture.Height == (Slot == EMenuIconSlot::Hint ? 18 : 24));
				CHECK(Picture.Width <= 64);
				CHECK(Opaque(Picture) > Picture.Width * Picture.Height / 3);
			}
		}
	}
}

TEST_CASE("The battle's bare symbols and lit clusters are a face button's; any other button or key is drawn as in a face slot", "[Menu]")
{
	// The texels of a picture's quarter around a face button's place: brighter where the cluster's dot is lit.
	const auto Brightness = [](const FMenuIconPicture& Picture, EFaceButtonPlace Place) {
		const int32 CentreX = Place == EFaceButtonPlace::Left ? 5 : Place == EFaceButtonPlace::Right ? 18 : 12;
		const int32 CentreY = Place == EFaceButtonPlace::Top ? 5 : Place == EFaceButtonPlace::Bottom ? 18 : 12;
		int32 Sum = 0;
		for (int32 Y = CentreY - 2; Y <= CentreY + 2; ++Y)
		{
			for (int32 X = CentreX - 2; X <= CentreX + 2; ++X)
			{
				const uint16 Texel = Picture.Texels[static_cast<std::size_t>(Y * Picture.Width + X)];
				Sum += (Texel & 31) + (Texel >> 5 & 31) + (Texel >> 10 & 31);
			}
		}
		return Sum;
	};
	const std::array Places{EFaceButtonPlace::Bottom, EFaceButtonPlace::Right, EFaceButtonPlace::Left, EFaceButtonPlace::Top};
	for (const EButtonIconSet Set : {EButtonIconSet::Xbox, EButtonIconSet::PlayStation, EButtonIconSet::Nintendo, EButtonIconSet::SteamDeck})
	{
		const std::array<const char*, 4> Icons = Set == EButtonIconSet::PlayStation ? std::array<const char*, 4>{"Cross", "Circle", "Square", "Triangle"}
			: Set == EButtonIconSet::Nintendo ? std::array<const char*, 4>{"B", "A", "Y", "X"} : std::array<const char*, 4>{"A", "B", "X", "Y"};
		for (std::size_t Lit = 0; Lit < Places.size(); ++Lit)
		{
			INFO(static_cast<int32>(Set) << " " << Icons[Lit]);
			const FMenuIconPicture Cluster = DrawMenuButtonIcon(Set, Icons[Lit], EMenuIconSlot::Cluster);
			REQUIRE(Cluster.Width == 24);
			REQUIRE(Cluster.Height == 24);
			// Four dots around a clear middle, the icon's own the brightest.
			CHECK(Cluster.Texels[12 * 24 + 12] == 0);
			for (std::size_t Other = 0; Other < Places.size(); ++Other)
			{
				CHECK(Brightness(Cluster, Places[Other]) > 0);
				if (Other != Lit)
				{
					CHECK(Brightness(Cluster, Places[Lit]) > 2 * Brightness(Cluster, Places[Other]));
				}
			}
			const FMenuIconPicture Symbol = DrawMenuButtonIcon(Set, Icons[Lit], EMenuIconSlot::Symbol);
			CHECK(Symbol.Width == 24);
			CHECK(Symbol.Height == 24);
			// The symbol alone: no button's body under it.
			CHECK(Opaque(Symbol) > 40);
			CHECK(Opaque(Symbol) < Opaque(DrawMenuButtonIcon(Set, Icons[Lit], EMenuIconSlot::FaceButton)));
		}
	}
	// What is not a face button has neither: a shoulder button and a key are drawn as a face slot shows them.
	for (const EMenuIconSlot Slot : {EMenuIconSlot::Symbol, EMenuIconSlot::Cluster})
	{
		CHECK(DrawMenuButtonIcon(EButtonIconSet::Xbox, "LB", Slot).Texels == DrawMenuButtonIcon(EButtonIconSet::Xbox, "LB", EMenuIconSlot::FaceButton).Texels);
		CHECK(DrawMenuButtonIcon(EButtonIconSet::Keyboard, "J", Slot).Texels == DrawMenuButtonIcon(EButtonIconSet::Keyboard, "J", EMenuIconSlot::FaceButton).Texels);
		CHECK(DrawMenuButtonIcon(EButtonIconSet::PlayStation, "DPad_Up", Slot).Texels
			== DrawMenuButtonIcon(EButtonIconSet::PlayStation, "DPad_Up", EMenuIconSlot::FaceButton).Texels);
	}
}

TEST_CASE("Keys that keyboards mark with a symbol show it, and a mouse's buttons a mouse with the button pressed", "[Menu]")
{
	const auto Key = [](const char* Icon) { return DrawMenuButtonIcon(EButtonIconSet::Keyboard, Icon, EMenuIconSlot::FaceButton); };
	// A symbol fits the key as a letter does, where the names it replaces widened it.
	for (const char* Icon : {"Enter", "Tab", "LeftShift", "Space", "Backspace", "LeftCommand", "MouseLeft", "MouseRight", "MouseMiddle"})
	{
		INFO(Icon);
		CHECK(Key(Icon).Width == 24);
	}
	// Backspace's long arrow is not the Left key's; each mouse button marks its own; both Shifts are one symbol.
	CHECK(Key("Backspace").Texels != Key("Left").Texels);
	CHECK(Key("MouseLeft").Texels != Key("MouseRight").Texels);
	CHECK(Key("MouseMiddle").Texels != Key("MouseLeft").Texels);
	CHECK(Key("LeftShift").Texels == Key("RightShift").Texels);
	// The rest keep their printed names.
	CHECK(Key("Pause").Width > 24);
}

TEST_CASE("A menu's button pictures are shown as the player's controller prints the bound buttons", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Resources = FMenuResources::Load(Disc->Image(), *Index, *Executable);
	REQUIRE(Resources);
	const FMenuSpriteSheet& Sheet = Resources->Sheet();
	FMenuFrame Frame;
	Frame.Layer = MenuFlatTransform;
	Sheet.Draw(0xF9, 100, 50, 0x1000, Frame.Primitives);
	Sheet.Draw(0xFC, 120, 50, 0x1000, Frame.Primitives);
	Sheet.Draw(0x164, 10, 100, 0x1000, Frame.Primitives);
	REQUIRE(Frame.Primitives.size() == 4);
	CHECK(Frame.Primitives[2].ButtonRole == 0);
	CHECK(Frame.Primitives[3].ButtonRole == 0x04);
	FMenuDrawBuilder Builder;
	FDrawList Original;
	Builder.Build(Frame, nullptr, 1.0f, Original);
	REQUIRE(Builder.Icons.size() == 3);
	const std::vector<FMenuDrawnIcon> Icons = Builder.Icons;
	FMenuButtonIcons Pictures;

	// The PlayStation's own buttons, bound as the console's: the game's pictures as they are.
	FTestGlyphs PlayStation(EButtonIconSet::PlayStation);
	PlayStation.Bind({{EPadButton::Cross, "Cross"}, {EPadButton::Circle, "Circle"}, {EPadButton::L1, "L1"}});
	FDrawList List = Original;
	Pictures.Apply(List, Icons, PlayStation, Sheet, MenuSheetButtonPictures);
	for (std::size_t Primitive = 0; Primitive < List.Primitives.size(); ++Primitive)
	{
		CHECK(List.Primitives[Primitive].Corners[0].U == Original.Primitives[Primitive].Corners[0].U);
		CHECK(List.Primitives[Primitive].Texture->Vram == 0);
	}

	// Confirm and cancel swapped: each picture takes the other's frame from the sheet.
	PlayStation.Bind({{EPadButton::Cross, "Circle"}, {EPadButton::Circle, "Cross"}, {EPadButton::L1, "L1"}});
	List = Original;
	Pictures.Apply(List, Icons, PlayStation, Sheet, MenuSheetButtonPictures);
	CHECK(List.Primitives[0].Corners[0].U == Original.Primitives[1].Corners[0].U);
	CHECK(List.Primitives[1].Corners[0].U == Original.Primitives[0].Corners[0].U);
	CHECK(Pictures.Pictures().Revision() == 0);
	// The glyphs are asked again only at their next revision.
	PlayStation.Icons = {{EPadButton::Cross, "Cross"}, {EPadButton::Circle, "Circle"}, {EPadButton::L1, "L1"}};
	List = Original;
	Pictures.Apply(List, Icons, PlayStation, Sheet, MenuSheetButtonPictures);
	CHECK(List.Primitives[0].Corners[0].U == Original.Primitives[1].Corners[0].U);

	// An Xbox pad: the port's pictures, in their own VRAM; the hint's arrow stays the game's.
	FTestGlyphs Xbox(EButtonIconSet::Xbox);
	Xbox.Bind({{EPadButton::Cross, "A"}, {EPadButton::Circle, "B"}, {EPadButton::L1, "LB"}});
	List = Original;
	Pictures.Apply(List, Icons, Xbox, Sheet, MenuSheetButtonPictures);
	CHECK(List.Primitives[0].Texture->Vram == FMenuButtonIcons::PicturesVram);
	CHECK(List.Primitives[0].Texture->Page.Depth == ETextureDepth::Direct15);
	CHECK_FALSE(List.Primitives[0].bModulate);
	CHECK(Across(List.Primitives[0]) == Catch::Approx(Across(Original.Primitives[0])));
	CHECK(List.Primitives[2].Texture->Vram == 0);
	CHECK(List.Primitives[3].Texture->Vram == FMenuButtonIcons::PicturesVram);
	CHECK(Pictures.Pictures().Revision() == 3);
	const FDrawPrimitive& A = List.Primitives[0];
	const uint16 Centre = FetchTexel(Pictures.Pictures(), A.Texture->Page, {}, static_cast<uint8>(A.Corners[0].U + 12), static_cast<uint8>(A.Corners[0].V + 12));
	CHECK(Centre != 0);

	// The keyboard: a key whose label is long widens its picture about the slot's centre; pictures once made are kept.
	FTestGlyphs Keyboard(EButtonIconSet::Keyboard);
	Keyboard.Bind({{EPadButton::Cross, "Pause"}, {EPadButton::Circle, "Backspace"}, {EPadButton::L1, "Q"}});
	List = Original;
	Pictures.Apply(List, Icons, Keyboard, Sheet, MenuSheetButtonPictures);
	const float Width = static_cast<float>(DrawMenuButtonIcon(EButtonIconSet::Keyboard, "Pause", EMenuIconSlot::FaceButton).Width) / MenuIconTexelsPerPixel;
	REQUIRE(Width > 24.0f / MenuIconTexelsPerPixel);
	CHECK(Across(List.Primitives[0]) == Catch::Approx(Width));
	CHECK((List.Primitives[0].Corners[0].X + List.Primitives[0].Corners[1].X) / 2 == Catch::Approx(106.0f));
	CHECK(Pictures.Pictures().Revision() == 6);
	List = Original;
	Pictures.Apply(List, Icons, Xbox, Sheet, MenuSheetButtonPictures);
	CHECK(Pictures.Pictures().Revision() == 6);

	// A role nothing is bound to keeps the game's picture.
	FTestGlyphs Unbound(EButtonIconSet::Xbox);
	List = Original;
	Pictures.Apply(List, Icons, Unbound, Sheet, MenuSheetButtonPictures);
	CHECK(List.Primitives[0].Texture->Vram == 0);

	// A picture drawn at another size names the texels it may show, the sheet's own: the picture put in its place
	// lies elsewhere and must not be held to them (it was, and no controller's picture was seen).
	for (const bool bSwapped : {false, true})
	{
		FMenuFrame Small;
		Small.Layer = MenuFlatTransform;
		Sheet.Draw(0xF9, 100, 50, 0x800, Small.Primitives);
		FMenuDrawBuilder SmallBuilder;
		FDrawList Shrunk;
		SmallBuilder.Build(Small, nullptr, 1.0f, Shrunk);
		REQUIRE(SmallBuilder.Icons.size() == 1);
		REQUIRE(Shrunk.Primitives[0].Texels);
		if (bSwapped)
		{
			PlayStation.Bind({{EPadButton::Cross, "Circle"}, {EPadButton::Circle, "Cross"}, {EPadButton::L1, "L1"}});
			Pictures.Apply(Shrunk, SmallBuilder.Icons, PlayStation, Sheet, MenuSheetButtonPictures);
			CHECK(Shrunk.Primitives[0].Texture->Vram == 0);
		}
		else
		{
			Pictures.Apply(Shrunk, SmallBuilder.Icons, Xbox, Sheet, MenuSheetButtonPictures);
			CHECK(Shrunk.Primitives[0].Texture->Vram == FMenuButtonIcons::PicturesVram);
		}
		CHECK_FALSE(Shrunk.Primitives[0].Texels);
	}
}

TEST_CASE("A frame's own keys are drawn as their pictures, a free place as an empty button", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Resources = FMenuResources::Load(Disc->Image(), *Index, *Executable);
	REQUIRE(Resources);
	const FMenuSpriteSheet& Sheet = Resources->Sheet();
	FMenuFrame Frame;
	Frame.Layer = MenuFlatTransform;
	for (int32 Key = 0; Key < 4; ++Key)
	{
		Sheet.Draw(0xF9, 100 + Key * 20, 50, 0x1000, Frame.Primitives);
		Frame.Primitives.back().ButtonRole = static_cast<uint16>(MenuKeyRole + Key);
	}
	Frame.Keys = {{EButtonIconSet::Keyboard, "K", "K"}, {EButtonIconSet::PlayStation, "Gamepad_FaceButton_Right", "Circle"},
		{EButtonIconSet::Xbox, "Gamepad_LeftShoulder", "LB"}, {EButtonIconSet::Xbox, "", ""}};
	FMenuDrawBuilder Builder;
	FDrawList List;
	Builder.Build(Frame, nullptr, 1.0f, List);
	REQUIRE(Builder.Icons.size() == 4);
	const FDrawList Original = List;
	FMenuButtonIcons Pictures;
	// The glyphs of the player's bindings play no part: each key is the frame's.
	const FTestGlyphs Glyphs(EButtonIconSet::Keyboard);
	Pictures.Apply(List, Builder.Icons, Glyphs, Sheet, MenuSheetButtonPictures, Frame.Keys);
	CHECK(List.Primitives[0].Texture->Vram == FMenuButtonIcons::PicturesVram);
	// The PlayStation's Circle is the game's own picture of it, the sheet's frame FC.
	CHECK(List.Primitives[1].Texture->Vram == 0);
	FMenuFrame Circle;
	Circle.Layer = MenuFlatTransform;
	Sheet.Draw(0xFC, 120, 50, 0x1000, Circle.Primitives);
	FDrawList CircleList;
	FMenuDrawBuilder{}.Build(Circle, nullptr, 1.0f, CircleList);
	CHECK(List.Primitives[1].Corners[0].U == CircleList.Primitives.back().Corners[0].U);
	CHECK(List.Primitives[2].Texture->Vram == FMenuButtonIcons::PicturesVram);
	CHECK(List.Primitives[3].Texture->Vram == FMenuButtonIcons::PicturesVram);
	CHECK(Pictures.Pictures().Revision() == 3);

	// A role past the frame's keys keeps the stand-in.
	FDrawList Short = Original;
	Pictures.Apply(Short, Builder.Icons, Glyphs, Sheet, MenuSheetButtonPictures, std::span<const FButtonGlyph>(Frame.Keys).first(2));
	CHECK(Short.Primitives[3].Texture->Vram == 0);
	CHECK(Short.Primitives[3].Corners[0].U == Original.Primitives[3].Corners[0].U);
}

TEST_CASE("A button glyph in a line of text shows the player's button in its cell, the line cut about it", "[Menu]")
{
	// A line 100 pixels long at (20, 30) with the Cross glyph 40 pixels in and the Square glyph at its start.
	const auto Text = []
	{
		FDrawList List;
		List.Primitives.push_back(MakeGouraudRect({0, 0, 320, 224}, {}));
		FDrawPrimitive Line = MakeSprite(20, 30, 100, 13, FDrawTexture{0, FTexturePage{0x300, 0x100, ETextureDepth::Indexed4, EBlendMode::Average}, {}}, 8, 16);
		List.Primitives.push_back(Line);
		List.Primitives.push_back(MakeGouraudRect({0, 0, 8, 8}, {}));
		return List;
	};
	const std::array<FTextButtonCell, 2> Cells{{{1, 40, 12, EPadButton::Cross}, {1, 0, 12, EPadButton::Square}}};

	FTestGlyphs Xbox(EButtonIconSet::Xbox);
	Xbox.Bind({{EPadButton::Cross, "A"}, {EPadButton::Square, "X"}});
	FDrawList List = Text();
	std::vector<int32> Matches{5, -1, 6};
	uint32 Mark = 2;
	FMenuButtonIcons Pictures;
	Pictures.ApplyToText(List, Cells, Xbox, &Matches, &Mark);
	// The Square picture, the line from 12 to 40, the Cross picture, the rest of the line.
	REQUIRE(List.Primitives.size() == 6);
	CHECK(List.Primitives[1].Texture->Vram == FMenuButtonIcons::PicturesVram);
	CHECK(List.Primitives[1].Corners[0].X == 20.0f);
	CHECK(List.Primitives[2].Corners[0].X == 32.0f);
	CHECK(List.Primitives[2].Corners[1].X == 60.0f);
	CHECK(List.Primitives[2].Corners[0].U == 20.0f);
	CHECK(List.Primitives[3].Texture->Vram == FMenuButtonIcons::PicturesVram);
	CHECK(List.Primitives[3].Corners[0].X == 60.0f);
	CHECK(List.Primitives[4].Corners[0].X == 72.0f);
	CHECK(List.Primitives[4].Corners[1].X == 120.0f);
	CHECK(List.Primitives[4].Corners[0].U == 60.0f);
	CHECK(List.Primitives[4].Texture->Vram == 0);
	CHECK(Matches == std::vector<int32>{5, -1, -1, -1, -1, 6});
	CHECK(Mark == 5);

	// Bound as the font shows them, the PlayStation's glyphs stay the font's; an unbound button too.
	FTestGlyphs PlayStation(EButtonIconSet::PlayStation);
	PlayStation.Bind({{EPadButton::Cross, "Cross"}});
	FDrawList Kept = Text();
	Pictures.ApplyToText(Kept, Cells, PlayStation);
	CHECK(Kept.Primitives.size() == 3);
	// Swapped, the glyph shows the other button's picture.
	PlayStation.Bind({{EPadButton::Cross, "Circle"}, {EPadButton::Square, "Square"}});
	FDrawList Swapped = Text();
	Pictures.ApplyToText(Swapped, Cells, PlayStation);
	CHECK(Swapped.Primitives.size() == 5);
}

TEST_CASE("A button named in words shows the player's button where the word was, the rest of its line closing up", "[Menu]")
{
	// A line of two sprites, 100 pixels from (20, 30) then 50 more; "L1" from 40 pixels in, 30 wide with its space.
	const auto Text = []
	{
		FDrawList List;
		List.Primitives.push_back(MakeGouraudRect({0, 0, 320, 224}, {}));
		const FDrawTexture Page{0, FTexturePage{0x300, 0x100, ETextureDepth::Indexed4, EBlendMode::Average}, {}};
		List.Primitives.push_back(MakeSprite(20, 30, 100, 13, Page, 8, 16));
		List.Primitives.push_back(MakeSprite(120, 30, 50, 13, Page, 0, 16));
		List.Primitives.push_back(MakeGouraudRect({0, 0, 8, 8}, {}));
		return List;
	};
	FTextButtonCell Word;
	Word.Primitive = 1;
	Word.OffsetX = 40;
	Word.Width = 30;
	Word.Button = EPadButton::L1;
	Word.LineEnd = 3;
	const std::array<FTextButtonCell, 1> Cells{Word};

	FTestGlyphs Xbox(EButtonIconSet::Xbox);
	Xbox.Bind({{EPadButton::L1, "LB"}});
	FDrawList List = Text();
	FMenuButtonIcons Pictures;
	Pictures.ApplyToText(List, Cells, Xbox);
	// The line to the word, the LB picture where the word started, the rest of the line and its second sprite closed up
	// by as much as the picture is narrower than the word.
	const float LbWidth = static_cast<float>(DrawMenuButtonIcon(EButtonIconSet::Xbox, "LB", EMenuIconSlot::FaceButton).Width) / 2.0f;
	REQUIRE(LbWidth < 30.0f);
	REQUIRE(List.Primitives.size() == 6);
	CHECK(List.Primitives[1].Corners[1].X == 60.0f);
	CHECK(List.Primitives[2].Texture->Vram == FMenuButtonIcons::PicturesVram);
	CHECK(List.Primitives[2].Corners[0].X == 60.0f);
	CHECK(Across(List.Primitives[2]) == LbWidth);
	CHECK(List.Primitives[3].Corners[0].X == 60.0f + LbWidth);
	CHECK(List.Primitives[3].Corners[0].U == 78.0f);
	CHECK(List.Primitives[4].Corners[0].X == 120.0f - (30.0f - LbWidth));
	CHECK(List.Primitives[5].Corners[0].X == 0.0f);

	// On the PlayStation's controller with L1 bound to L1, the words stay.
	FTestGlyphs PlayStation(EButtonIconSet::PlayStation);
	PlayStation.Bind({{EPadButton::L1, "L1"}});
	FDrawList Kept = Text();
	Pictures.ApplyToText(Kept, Cells, PlayStation);
	CHECK(Kept.Primitives.size() == 4);

	// The directional buttons on the keyboard: the four keys as one key cap.
	FTextButtonCell Directions = Word;
	Directions.Button = EPadButton::Up;
	Directions.bDirections = true;
	const std::array<FTextButtonCell, 1> DirectionCells{Directions};
	FTestGlyphs Keyboard(EButtonIconSet::Keyboard);
	Keyboard.Bind({{EPadButton::Up, "W"}, {EPadButton::Left, "A"}, {EPadButton::Down, "S"}, {EPadButton::Right, "D"}});
	FDrawList Keys = Text();
	Pictures.ApplyToText(Keys, DirectionCells, Keyboard);
	REQUIRE(Keys.Primitives.size() == 6);
	const float KeysWidth = Across(Keys.Primitives[2]);
	CHECK(DrawMenuButtonIcon(EButtonIconSet::Keyboard, "W|A|S|D", EMenuIconSlot::FaceButton).Texels
		!= DrawMenuButtonIcon(EButtonIconSet::Keyboard, "W", EMenuIconSlot::FaceButton).Texels);
	CHECK(Keys.Primitives[3].Corners[0].X == 60.0f + KeysWidth);
	CHECK(DrawMenuButtonIcon(EButtonIconSet::Keyboard, "W|A|S|D", EMenuIconSlot::FaceButton).Width == static_cast<int32>(KeysWidth * 2.0f));
}

TEST_CASE("The port's Options is pieced from the USA release's letters, and typed as a picture of the sheet's own where they are another's",
	"[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Resources = FMenuResources::Load(Disc->Image(), *Index, *Executable);
	REQUIRE(Resources);
	FMenuSpriteSheet Sheet = Resources->Sheet();
	// The title's Options, the Normal Menu's and the raised title's: the O, the p, "tion" and the s, all of VRAM.
	for (const int32 Frame : {0x200, 0x202, 0x209})
	{
		INFO("frame " << Frame);
		REQUIRE(Sheet.Frame(Frame).size() == 4);
		CHECK(std::all_of(Sheet.Frame(Frame).begin(), Sheet.Frame(Frame).end(), [](const FMenuSpritePart& Part) { return Part.Picture < 0; }));
	}

	const auto Resident = LoadMesResident(Disc->Image(), *Index, *Executable);
	REQUIRE(Resident);
	const std::span<const uint16> Colours = std::span<const uint16>(Resources->TextClut()).first(16);
	const FMenuSheetPicture Word = MenuLetteredWord(Resident->Font, Colours, "Options");
	// Set tight as the labels' letters are: narrower than the font's advances (12 or 8 a glyph), inked from edge to edge.
	int32 Advances = 0;
	for (const char Character : std::string_view("Option"))
	{
		Advances += Resident->Font.WidthUnits(*MesCodeOfCharacter(Character)) * 4;
	}
	CHECK(Word.Width < Advances);
	CHECK(Word.Width > 7 * 5);
	CHECK(Word.Height == FMesFont::CellHeight);
	const auto Inked = [&Word](int32 Column)
	{
		for (int32 Row = 0; Row < Word.Height; ++Row)
		{
			if (Word.Texels[static_cast<std::size_t>(Row * Word.Width + Column)] != 0)
			{
				return true;
			}
		}
		return false;
	};
	CHECK(Inked(0));
	CHECK(Inked(Word.Width - 1));
	// A space keeps the font's advance between the words.
	const FMenuSheetPicture Two = MenuLetteredWord(Resident->Font, Colours, "Op tions");
	CHECK(Two.Width == Word.Width + Resident->Font.WidthUnits(*MesCodeOfCharacter(' ')) * 4 + 1);
	CHECK(std::all_of(Word.Texels.begin(), Word.Texels.end(), [&Colours](uint16 Texel) { return Texel == 0 || std::find(Colours.begin() + 1, Colours.begin() + 4, Texel) != Colours.begin() + 4; }));
	CHECK(std::count_if(Word.Texels.begin(), Word.Texels.end(), [](uint16 Texel) { return Texel != 0; }) > Word.Width * 3);

	// Shown by a part of its own, the word is drawn into the pictures' VRAM once, its texels the whole picture.
	FMenuSpritePart Part;
	Part.Width = static_cast<int16>(Word.Width);
	Part.Height = static_cast<int16>(Word.Height);
	Part.Picture = Sheet.AddPicture(Word);
	Sheet.SetFrame(0x200, {Part});
	FMenuFrame Frame;
	Frame.Layer = MenuFlatTransform;
	Sheet.Draw(0x200, 40, 60, 0x1000, Frame.Primitives);
	REQUIRE(Frame.Primitives.size() == 1);
	CHECK(Frame.Primitives[0].ButtonRole == MenuPictureRole + Part.Picture);
	FMenuDrawBuilder Builder;
	FDrawList List;
	Builder.Build(Frame, nullptr, 1.0f, List);
	REQUIRE(Builder.Icons.size() == 1);
	const FDrawList Stand = List;
	FMenuButtonIcons Pictures;
	const FTestGlyphs Glyphs(EButtonIconSet::PlayStation);
	Pictures.Apply(List, Builder.Icons, Glyphs, Sheet, MenuSheetButtonPictures);
	const FDrawPrimitive& Drawn = List.Primitives[Builder.Icons[0].Primitive];
	CHECK(Drawn.Texture->Vram == FMenuButtonIcons::PicturesVram);
	CHECK(Drawn.Corners[3].U - Drawn.Corners[0].U == static_cast<float>(Word.Width));
	CHECK(Drawn.Corners[3].V - Drawn.Corners[0].V == static_cast<float>(Word.Height));
	CHECK(Drawn.Corners[0].X == Stand.Primitives[Builder.Icons[0].Primitive].Corners[0].X);
	CHECK(Pictures.Pictures().Revision() == 1);
	FDrawList Again = Stand;
	Pictures.Apply(Again, Builder.Icons, Glyphs, Sheet, MenuSheetButtonPictures);
	CHECK(Pictures.Pictures().Revision() == 1);
}
