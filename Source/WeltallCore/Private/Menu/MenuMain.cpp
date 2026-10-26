#include "Menu/MenuMain.h"

#include "Game/GameState.h"
#include "Media/CdImage.h"
#include "Menu/MenuResources.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The inner arcs' places follow the Normal Menu's seven. */
		constexpr int32 InnerArcPlace = 7;
		constexpr int32 CharacterCount = 16;
		constexpr uint16 CharacterMask = 0x7FF;
		/** The message font's CLUTs (two of 16 colours), and the one-colour CLUT the menu's own text uses. */
		constexpr int16 TextClutX = 0;
		constexpr int16 TextClutY = 0x1D1;
		constexpr int16 WhiteClutY = 0x1C0;
		constexpr uint16 White = 0x7FFF;
		constexpr int32 SelectionPageX = 0x140;
		constexpr int32 SelectionPageY = 0x80;
		/** The menu's frames after its last screen: two, one more without the scene, then until the first draw buffer. */
		constexpr int32 ClosingFrames = 2;
	}

	FMenuMain::FMenuMain(const FMenuServices& InServices, const FMenuRequest& InRequest)
		: FMenuModule(InServices, InRequest), GearParts(InServices.Resources->GearParts())
	{
		SubArcFirstPlace = InnerArcPlace;
		InitArcs();
		HelpImage.Rect = {0, 0, 0x3C, 13};
		HelpImage.Units.assign(0x3C * 13, 0);
	}

	TMenuTask<> FMenuMain::Run()
	{
		Init();
		bDrawScene = true;
		bSoundOn = true;
		FGameState& Game = *Services.Game;
		if (Request.Mode == EMenuMode::LoadGame)
		{
			Game.ProgramResult = 0;
			co_await TitleMenu();
			Flags[MenuFlag::Commands] = 0;
			Flags[MenuFlag::Pointer] = 0;
			Flags[MenuFlag::Selection] = 0;
			if (Game.ProgramResult == 0)
			{
				co_await RequireDisc(0);
			}
			else if (Game.ProgramResult == 2)
			{
				co_await RequireDisc(Game.SavedDisc());
			}
		}
		else if (Request.Mode == EMenuMode::Normal)
		{
			co_await OpenNormalMenu();
			co_await MainMenu();
			if (bGameLoaded && Services.Disc != Game.SavedDisc() + 1)
			{
				co_await RequireDisc(Game.SavedDisc());
			}
		}
		else if (Request.Mode == EMenuMode::DiscChange)
		{
			co_await ChangeDisc();
		}
		else
		{
			co_await Fail("the Menu's mode " + std::to_string(static_cast<int32>(Request.Mode)) + " is not built yet");
		}
		if (OffersStoryMode())
		{
			co_await OfferStoryMode();
		}
		co_await Shutdown();
	}

	void FMenuMain::Init()
	{
		InitParty();
		InitFileStores();
		Buffer = 0;
		// The message font's colours for the labels, the help labels, and a white for the menu's own text (the French
		// translation's program gives that text the message font's colours too, its shadow with them).
		Upload({{TextClutX, TextClutY, 32, 1}, {Resources.TextClut().begin(), Resources.TextClut().end()}});
		RenderLabels(HelpLabels, Resources.Main().HelpLabels, 0);
		FVramImage WhiteClut{{0, WhiteClutY, 16, 1}, std::vector<uint16>(16, 0)};
		WhiteClut.Units[1] = White;
		if (Resources.IsFrench())
		{
			std::copy_n(Resources.TextClut().begin(), WhiteClut.Units.size(), WhiteClut.Units.begin());
		}
		Upload(WhiteClut);
		InitBackground();
	}

	void FMenuMain::InitParty()
	{
		const FGameState& Game = *Services.Game;
		Command = Request.Mode == EMenuMode::Normal && Request.Param == 0 ? Game.MenuLastCommand : 1;
		DrawnCommand = 0xFF;
		Members = 0;
		const uint16 Allowed = static_cast<uint16>(Game.JoinedCharacters() & Game.MenuCharacters() & CharacterMask);
		for (int32 Character = 0; Character < CharacterCount; ++Character)
		{
			Available[static_cast<std::size_t>(Character)] = (Allowed >> Character & 1) != 0;
		}
		// The menu's slots hold the party in the order its player keeps it, each knowing the game's slot it came from.
		const std::array<uint8, 3> InParty = Game.Party();
		const FPartyOrder::FPlaces Places = Services.bArrangesParty ? Game.PartyOrder().PlacesOf(InParty) : FPartyOrder{}.PlacesOf(InParty);
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			uint8& Member = Flags[static_cast<std::size_t>(MenuFlag::Party + Slot)];
			uint8& HasGear = Flags[static_cast<std::size_t>(MenuFlag::PartyHasGear + Slot)];
			HasGear = 0;
			const int8 From = Places[static_cast<std::size_t>(Slot)];
			GameSlots[static_cast<std::size_t>(Slot)] = From;
			const uint8 Character = From == FPartyOrder::Nobody ? FGameState::EmptySlot : InParty[static_cast<std::size_t>(From)];
			if (Character == FGameState::EmptySlot || !Available[Character])
			{
				Member = FGameState::EmptySlot;
				continue;
			}
			Member = Character;
			++Members;
			if (Game.CharacterGear(Character).value_or(FGameState::EmptySlot) != FGameState::EmptySlot)
			{
				HasGear = 1;
				++MembersWithGears;
			}
		}
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			if (Member(Slot) != FGameState::EmptySlot)
			{
				FirstMember = Slot;
				break;
			}
		}
		LoadResources();
	}

	void FMenuMain::LoadResources()
	{
		for (const FVramImage& Image : Resources.ArchiveUploads())
		{
			Upload(Image);
		}
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			const uint8 Character = Member(Slot);
			if (Character == FGameState::EmptySlot)
			{
				continue;
			}
			UploadPortrait(Slot, Character);
		}
	}

	void FMenuMain::InitBackground()
	{
		HideSelection();
		InitBackdrop();
		SelectionMode = FMenuPrimitive::MakeDrawMode(MenuGpu::TexturePage(0, 0, SelectionPageX, SelectionPageY));
	}

	TMenuTask<> FMenuMain::Shutdown()
	{
		if (Request.Mode == EMenuMode::Normal)
		{
			HideSelection();
			// After a game loaded the File screen has slid the party out already.
			if (!bGameLoaded)
			{
				co_await SlideParty(false, false);
			}
			Flags[MenuFlag::PlayTime] = 0;
			Flags[MenuFlag::Gold] = 0;
			// Where saving is allowed the menu opens on File each time: the command used is only kept elsewhere.
			// The port's Options is not kept: the save keeps a command the original's arc has.
			if (Request.Param == 0 && Command < static_cast<int32>(Resources.Main().MainCommands.size()))
			{
				Services.Game->MenuLastCommand = static_cast<uint8>(Command);
			}
		}
		for (int32 Frame = 0; Frame < ClosingFrames; ++Frame)
		{
			co_await this->Frame();
		}
		bDrawScene = false;
		co_await Frame();
		do
		{
			co_await Frame();
		} while (Buffer != 0);
	}

	bool FMenuMain::UsesPerfectWorksRules() const
	{
		return Services.Image != nullptr && Services.Image->ModChanges().bPerfectWorksRebalance;
	}
}
