#include "Battle/BattleContext.h"
#include "Battle/BattleGameState.h"
#include "Battle/BattleProgram.h"
#include "Battle/Show/BattleHudButtons.h"
#include "Battle/Show/BattleHudGpu.h"
#include "Battle/Show/BattleShowHud.h"
#include "Text/MesRender.h"

#include <algorithm>
#include <span>

namespace Weltall
{
	using namespace BattleHudButtons;
	using namespace BattleHudGpu;

	namespace
	{
		constexpr int32 FullScale = 0x1000;
		constexpr uint8 ChuChu = 7;
		/** A ring's element from 4000h names the slot's picture at its low byte; FFh there draws nothing. */
		constexpr uint16 SlotPicture = 0x4000;
		constexpr uint16 CommandFlags = 0x2000;
		constexpr uint8 NoPicture = 0xFF;
		constexpr uint8 DimLevel = 0x10;
		constexpr uint8 FullLevel = 0x80;
		constexpr uint8 FadeStep = 0x10;
		/** The disabled flags of the attack buttons, by the menu's command numbers. */
		constexpr std::size_t WeakButton = 0;
		constexpr std::size_t StrongButton = 1;
		constexpr std::size_t FierceButton = 2;

		/** The arrows' corners (x0, y0, x1, y1, x2, y2), by way: right, down, left, up. */
		constexpr std::array<std::array<int16, 6>, FBattleHudArrows::Ways> ArrowCorners{{
			{0xC0, 0x70, 0xB0, 0x78, 0xB0, 0x68},
			{0xA0, 0x90, 0x98, 0x80, 0xA8, 0x80},
			{0x80, 0x70, 0x90, 0x68, 0x90, 0x78},
			{0xA0, 0x50, 0x98, 0x60, 0xA8, 0x60},
		}};
		constexpr int32 ArrowGlowStep = 4;
		constexpr int32 ArrowGlowTop = 0x100;
		constexpr int32 ArrowGlowBottom = 0x40;

		/** The attack buttons pressed, as the hints draw them: 5Dh Fierce, 5Eh Weak, 5Fh Strong. */
		constexpr int32 FierceFrame = 0x5D;
		constexpr int32 WeakFrame = 0x5E;
		constexpr int32 StrongFrame = 0x5F;
		/** The next presses a hint shows: the four buttons with Fierce, Weak or Strong lit. */
		constexpr int32 FierceHintFrame = 7;
		constexpr int32 WeakHintFrame = 8;
		constexpr int32 StrongHintFrame = 9;
		constexpr int16 HintsX = 0x50;
		constexpr int16 HintsY = 0xD0;
		constexpr int16 HintStep = 0x10;
		constexpr uint8 NoPattern = FBattleProgramTables::PatternCount;
		/** The Deathblow list's entries a Weak press then Fierce complete, by the pattern pressed so far (none for -1). */
		constexpr std::array<int8, FBattleProgramTables::PatternCount> WeakFollowUps{13, 14, 15, 16, 17, 18, -1, -1, 19, -1, -1, -1, -1};
		/** And a Strong press then Fierce. */
		constexpr std::array<int8, FBattleProgramTables::PatternCount> StrongFollowUps{20, 21, -1, 22, -1, -1, -1, -1, -1, -1, -1, -1, -1};

		/** The hints' names: typed 1Bh units wide into text images of two planes at (3C0h + 1Eh per image, 1Ah). */
		constexpr int16 HintTextX = 0x3C0;
		constexpr int16 HintTextY = 0x1A;
		constexpr int16 HintTextUnits = 0x1E;
		constexpr int16 HintTextRows = 13;
		constexpr uint8 HintTextU = 0x78;

		FVramImage HintTextImage()
		{
			FVramImage Image;
			Image.Rect = {0, 0, HintTextUnits, HintTextRows};
			Image.Units.assign(static_cast<std::size_t>(HintTextUnits) * HintTextRows, 0);
			return Image;
		}

		/** The command an element's word names, -1 for none. */
		int32 ElementCommand(const FBattleHudIconElement& Element)
		{
			return Element.Flags >= CommandFlags ? Element.Flags & 0xF : -1;
		}

		/**
		 * The button an element of a prompt goes with: the group's face button that names the same command (the
		 * attack modes' words and costs beside the attack buttons) or none (cancel's); 0 in a group of no face buttons.
		 */
		uint16 PromptRole(std::span<const FBattleHudIconElement> Group, const FBattleHudIconElement& Element)
		{
			for (const FBattleHudIconElement& Other : Group)
			{
				const FHudButton Button = HudButton(Other.Frame);
				if (Button.Role != 0 && Button.Slot == EMenuIconSlot::FaceButton && ElementCommand(Other) == ElementCommand(Element))
				{
					return Button.Role;
				}
			}
			return 0;
		}
	}

	void FBattleHud::InitCommandIcons()
	{
		// The setup gives each slot its character's pictures on foot, a gear's (Chu-Chu's giant form's own) and the outer ring in a gear.
		const FBattleHudTables& Tables = Resources.Tables;
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			std::array<uint8, 0x1C>& Icons = MenuIcons[static_cast<std::size_t>(Slot)];
			Icons.fill(0);
			const uint8 Character = Battle->Combatants[static_cast<std::size_t>(Slot)].Character;
			// An empty slot's pictures are never drawn.
			if (Character < Tables.CharacterIcons.size())
			{
				std::copy_n(Tables.CharacterIcons[Character].begin(), 8, Icons.begin());
			}
			const auto& Gear = Battle->PartyCharacters[static_cast<std::size_t>(Slot)] == ChuChu ? Tables.ChuChuGearIcons : Tables.GearIcons;
			std::copy_n(Gear.begin(), 4, Icons.begin() + 8);
			std::copy_n(Tables.OuterGearIcons.begin(), 4, Icons.begin() + 12);
		}
	}

	void FBattleHud::DrawCommandIcons(uint8 Slot)
	{
		const FBattlePartyInput& Party = Battle->Party;
		if (Party.bMenuFading)
		{
			FadeCommandIcons();
			return;
		}
		const FBattleHudTables& Tables = Resources.Tables;
		Ring.Counts = {0, 0};
		if (Slot >= PartySlots || Party.MenuState >= FBattleHudTables::MenuStateCount)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		for (std::size_t Entry = 0; Entry < 2; ++Entry)
		{
			const uint8 Layer = Tables.StateLayers[Party.MenuState][Entry];
			if (Layer == NoPicture)
			{
				break;
			}
			const uint8 Group = Tables.StateGroups[Party.MenuState][Entry];
			if (Layer >= FBattleHudCommandRing::Layers || Group >= FBattleHudTables::IconGroupCount)
			{
				FaultState = EBattleHudFault::IndexOutsideHud;
				return;
			}
			const std::vector<FBattleHudIconElement>& Elements = Tables.IconGroups[Group];
			for (const FBattleHudIconElement& Element : Elements)
			{
				DrawIconElement(Slot, Layer, Element, PromptRole(Elements, Element));
				if (FaultState != EBattleHudFault::None)
				{
					return;
				}
			}
		}
		Ring.Buffer = Buffer;
	}

	void FBattleHud::DrawIconElement(uint8 Slot, int32 Layer, const FBattleHudIconElement& Element, uint16 PromptRole)
	{
		int32 Frame = Element.Frame;
		if (Frame >= SlotPicture)
		{
			const std::size_t Picture = Frame & 0xFF;
			if (Picture >= MenuIcons[Slot].size())
			{
				FaultState = EBattleHudFault::IndexOutsideHud;
				return;
			}
			Frame = MenuIcons[Slot][Picture];
			if (Frame == NoPicture)
			{
				return;
			}
		}
		const std::size_t Layered = static_cast<std::size_t>(Layer);
		const int32 First = Layer * FBattleHudCommandRing::LayerParts + Ring.Counts[Layered];
		const int32 Drawn = DrawSheet(Frame, Ring.Parts, First, Element.X, Element.Y, FullScale);
		Ring.Counts[Layered] += Drawn;
		uint8 Level = static_cast<uint8>((Element.Flags & 0xFF) >> 1);
		if (Element.Flags >= CommandFlags)
		{
			Level = Battle->Party.Disabled[Slot][Element.Flags & 0xF] == 0 ? static_cast<uint8>((Element.Flags & 0xF0) >> 1) : DimLevel;
		}
		for (int32 Part = First; Part < First + Drawn && Part < static_cast<int32>(Ring.Parts.size()); ++Part)
		{
			FMenuPrimitive& Quad = Ring.Parts[static_cast<std::size_t>(Part)][Buffer];
			Quad.PromptRole = PromptRole;
			if (Level == 0)
			{
				SetColor(Quad, 0, FullLevel, FullLevel, FullLevel);
				continue;
			}
			SetSemiTrans(Quad, true);
			SetShadeTex(Quad, false);
			SetColor(Quad, 0, Level, Level, Level);
			Quad.Page |= PageAdd;
		}
	}

	void FBattleHud::FadeCommandIcons()
	{
		for (int32 Layer = 0; Layer < FBattleHudCommandRing::Layers; ++Layer)
		{
			const int32 First = Layer * FBattleHudCommandRing::LayerParts;
			const int32 Count = Ring.Counts[static_cast<std::size_t>(Layer)];
			for (int32 Part = First; Part < First + Count && Part < static_cast<int32>(Ring.Parts.size()); ++Part)
			{
				FMenuPrimitive& Quad = Ring.Parts[static_cast<std::size_t>(Part)][Ring.Buffer];
				SetSemiTrans(Quad, true);
				SetShadeTex(Quad, false);
				Quad.Page |= PageAdd;
				const uint8 Level = Quad.Corners[0].Color.R;
				if (Level != 0)
				{
					const uint8 Faded = static_cast<uint8>(Level - FadeStep);
					SetColor(Quad, 0, Faded, Faded, Faded);
				}
			}
		}
	}

	void FBattleHud::LinkCommandIcons()
	{
		if (!Battle->Party.bIconsShown)
		{
			return;
		}
		for (int32 Layer = 0; Layer < FBattleHudCommandRing::Layers; ++Layer)
		{
			const std::span<const FBattleHudPair> Parts(Ring.Parts);
			LinkPairs(Parts.subspan(static_cast<std::size_t>(Layer * FBattleHudCommandRing::LayerParts)), Ring.Counts[static_cast<std::size_t>(Layer)], Ring.Buffer);
		}
	}

	void FBattleHud::InitDirectionArrows()
	{
		for (std::size_t Way = 0; Way < ArrowCorners.size(); ++Way)
		{
			const std::array<int16, 6>& At = ArrowCorners[Way];
			for (FMenuPrimitive& Triangle : Arrows.Triangles[Way])
			{
				Triangle = {};
				BattleHudGpu::Set(Triangle, GouraudTriangle);
				for (std::size_t Corner = 0; Corner < 3; ++Corner)
				{
					Triangle.Corners[Corner].X = At[2 * Corner];
					Triangle.Corners[Corner].Y = At[2 * Corner + 1];
				}
				SetColor(Triangle, 0, 0xFF, 0, 0);
				SetColor(Triangle, 1, 0x40, 0, 0);
				SetColor(Triangle, 2, 0x40, 0, 0);
			}
		}
		Arrows.Glow = 0xFF;
		Arrows.bFalling = true;
		Arrows.Buffer = Buffer;
		Arrows.Targets.fill(false);
		Arrows.bShown = true;
	}

	void FBattleHud::SetArrowTargets(uint8 Ways)
	{
		for (std::size_t Way = 0; Way < Arrows.Targets.size(); ++Way)
		{
			Arrows.Targets[Way] = (Ways >> Way & 1) != 0;
		}
	}

	void FBattleHud::DrawDirectionArrows()
	{
		if (!Arrows.bShown)
		{
			return;
		}
		if (!Arrows.bFalling)
		{
			Arrows.Glow += ArrowGlowStep;
			if (Arrows.Glow >= ArrowGlowTop)
			{
				Arrows.bFalling = true;
				Arrows.Glow = ArrowGlowTop - ArrowGlowStep;
			}
		}
		else
		{
			Arrows.Glow -= ArrowGlowStep;
			if (Arrows.Glow < ArrowGlowBottom)
			{
				Arrows.bFalling = false;
				Arrows.Glow = ArrowGlowBottom;
			}
		}
		for (std::size_t Way = 0; Way < Arrows.Targets.size(); ++Way)
		{
			if (Arrows.Targets[Way])
			{
				FMenuPrimitive& Triangle = Arrows.Triangles[Way][Arrows.Buffer];
				SetColor(Triangle, 0, static_cast<uint8>(Arrows.Glow), 0, 0);
				Link(Triangle);
			}
		}
	}

	void FBattleHud::MakeCommandHud()
	{
		Hints = {};
		Cockpit = {};
		Reticle.Open();
		bCommandHudMade = true;
	}

	void FBattleHud::CloseCommandHud()
	{
		bCockpitShown = false;
		bCockpitLevelShown = false;
		bHintsShown = false;
		bCommandHudMade = false;
	}

	void FBattleHud::LinkHints()
	{
		if (bHintsShown)
		{
			LinkPairs(Hints.Names, Hints.NameCount, Hints.NameBuffer);
			LinkPairs(Hints.Sprites, Hints.SpriteCount, Hints.SpriteBuffer);
			LinkPairs(Hints.Costs, Hints.CostCount, Hints.CostBuffer);
		}
	}

	bool FBattleHud::Learned(uint8 Slot, uint8 Deathblow)
	{
		const std::size_t Offset = BattleGameState::SkillsOffset(Battle->PartyCharacters[Slot], GameStateLayout::Skills::Deathblows);
		if (Battle->GameState == nullptr || Offset + 2 > FGameState::BlockSize)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return false;
		}
		return Deathblow < 16 && (BattleGameState::Word(*Battle->GameState, Offset) & (0x8000u >> Deathblow)) != 0;
	}

	void FBattleHud::DrawDeathblowHints(uint8 Slot)
	{
		if (!bCommandHudMade)
		{
			FaultState = EBattleHudFault::CommandHudNotMade;
			return;
		}
		const FBattlePartyInput& Party = Battle->Party;
		const FBattleProgramTables& Program = *Battle->Program;
		if (Slot >= PartySlots || Party.ButtonCount > Party.Buttons.size() || Battle->Combatants[Slot].Character >= FBattleProgramTables::DeathblowListCount)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		Hints.NameCount = 0;
		Hints.SpriteCount = 0;
		const int32 Pressed = Party.ButtonCount;
		for (int32 Press = 0; Press < Pressed; ++Press)
		{
			const uint8 Button = Party.Buttons[static_cast<std::size_t>(Press)];
			const int32 Frame = Button == 0 ? FierceFrame : Button == 2 ? WeakFrame : Button == 3 ? StrongFrame : 0;
			if (Frame == 0)
			{
				FaultState = EBattleHudFault::IndexOutsideHud;
				return;
			}
			Hints.SpriteCount += DrawSheet(Frame, Hints.Sprites, Hints.SpriteCount, HintsX + HintStep * Press, HintsY, FullScale);
		}
		// The pattern the buttons make; a Deathblow's own Fierce press left out.
		std::array<uint8, FBattleProgramTables::PatternLength> Buttons = Party.Buttons;
		if (Party.Deathblow != 0 && Pressed > 0)
		{
			Buttons[static_cast<std::size_t>(Pressed - 1)] = 0xFF;
		}
		uint8 Pattern = NoPattern;
		for (uint8 Candidate = 0; Candidate < Program.Patterns.size(); ++Candidate)
		{
			if (Program.Patterns[Candidate] == Buttons)
			{
				Pattern = Candidate;
				break;
			}
		}
		const auto& Deathblows = Program.Deathblows[Battle->Combatants[Slot].Character];
		const auto& Disabled = Party.Disabled[Slot];
		std::array<FVramImage, 2> Images{HintTextImage(), HintTextImage()};
		int32 Row = Pressed;
		int32 Named = 0;
		bool bFinisher = false;
		if (Pattern != NoPattern && Learned(Slot, Deathblows[Pattern]) && Disabled[FierceButton] == 0)
		{
			if (Party.Deathblow == 0)
			{
				Hints.SpriteCount += DrawSheet(FierceHintFrame, Hints.Sprites, Hints.SpriteCount, HintsX + HintStep * Row, HintsY, FullScale);
			}
			else
			{
				--Row;
			}
			bFinisher = true;
			Named = DrawHintName(Slot, 0, Row, Deathblows[Pattern], 0, Images);
		}
		if (Party.Deathblow == 0 || !bFinisher)
		{
			const int8 Weak = Pattern != NoPattern ? WeakFollowUps[Pattern] : -1;
			if (Weak >= 0 && Learned(Slot, Deathblows[static_cast<std::size_t>(Weak)]) && Disabled[WeakButton] == 0 && Disabled[FierceButton] == 0)
			{
				const int16 Y = static_cast<int16>(HintsY - HintStep * Named);
				Hints.SpriteCount += DrawSheet(WeakHintFrame, Hints.Sprites, Hints.SpriteCount, HintsX + 4 + HintStep * Row, Y, FullScale);
				Hints.SpriteCount += DrawSheet(FierceHintFrame, Hints.Sprites, Hints.SpriteCount, HintsX + 0x14 + HintStep * Row, Y, FullScale);
				Named = DrawHintName(Slot, Named, Row, Deathblows[static_cast<std::size_t>(Weak)], 1, Images);
			}
			const int8 Strong = Pattern != NoPattern ? StrongFollowUps[Pattern] : -1;
			if (Strong >= 0 && Learned(Slot, Deathblows[static_cast<std::size_t>(Strong)]) && Disabled[StrongButton] == 0 && Disabled[FierceButton] == 0)
			{
				const int16 Y = static_cast<int16>(HintsY - HintStep * Named);
				Hints.SpriteCount += DrawSheet(StrongHintFrame, Hints.Sprites, Hints.SpriteCount, HintsX + 8 + HintStep * Row, Y, FullScale);
				Hints.SpriteCount += DrawSheet(FierceHintFrame, Hints.Sprites, Hints.SpriteCount, HintsX + 0x18 + HintStep * Row, Y, FullScale);
				DrawHintName(Slot, Named, Row, Deathblows[static_cast<std::size_t>(Strong)], 1, Images);
			}
		}
		Hints.NameBuffer = Buffer;
		Hints.SpriteBuffer = Buffer;
		bHintsShown = true;
	}

	int32 FBattleHud::DrawHintName(uint8 Slot, int32 Hint, int32 Row, uint8 Deathblow, int32 Beyond, std::span<FVramImage> Images)
	{
		if (FaultState != EBattleHudFault::None || Hint >= FBattleHudHints::NameParts)
		{
			FaultState = FaultState == EBattleHudFault::None ? EBattleHudFault::PartsOutsidePrimitives : FaultState;
			return Hint + 1;
		}
		const uint8 Plane = static_cast<uint8>(Hint % 2);
		for (FMenuPrimitive& Quad : Hints.Names[static_cast<std::size_t>(Hint)])
		{
			BattleHudGpu::Set(Quad, TexturedQuad);
			SetColor(Quad, 0, FullLevel, FullLevel, FullLevel);
			SetSemiTrans(Quad, false);
			SetShadeTex(Quad, true);
			Quad.Page = MenuGpu::TexturePage(0, 0, HintTextX, 0);
			Quad.Clut = Plane != 0 ? OddTextClut : EvenTextClut;
		}
		const std::span<const uint8> Name = Text.Tables != nullptr ? Text.Tables->String(Battle->PartyCharacters[Slot], Deathblow) : std::span<const uint8>{};
		if (Name.empty())
		{
			FaultState = EBattleHudFault::MessageOutsideTable;
			return Hint + 1;
		}
		FVramImage& Image = Images[static_cast<std::size_t>(Hint / 2)];
		const int32 Width = RenderMesString(Text, HintTyping, Name, Image, Plane);
		Image.Rect.X = static_cast<int16>(HintTextX + Hint / 2 * HintTextUnits);
		Image.Rect.Y = HintTextY;
		Upload(Image);
		const int16 X = static_cast<int16>(((Row + Beyond + 1) * HintStep + HintsX + Hint * 4) & 0xFFFC);
		const int16 Y = static_cast<int16>((200 - HintStep * Hint) & 0xFFF8);
		SetTextQuad(Hints.Names[static_cast<std::size_t>(Hint)][Buffer], X, Y, static_cast<uint8>(Hint / 2 * HintTextU & 0xF8), HintTextY,
			static_cast<uint8>(Width));
		++Hints.NameCount;
		return Hint + 1;
	}
}
