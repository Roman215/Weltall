#pragma once

#include "CoreMinimal.h"
#include "Game/SaveStateSlots.h"
#include "Game/SaveStore.h"
#include "Input/ButtonGlyphs.h"
#include "Menu/MenuAssetPages.h"
#include "Menu/MenuProgram.h"

#include <array>
#include <optional>

class FGameLoopScreen;

namespace Weltall
{
	class FGameSession;
	class FGameState;
}

/**
 * What a render stages in the game before and while it plays, so a screen shows what it is made to
 * show: -State=Full, a state that fills the menus (see MakeFullGameState), -State=Weak, the same with
 * every character a point from death (a battle soon lost), -State=Strong, Full with six AP a turn, or
 * -State=Rising, Strong with everyone a point of EXP from the next level and Elly's abilities to learn again
 * (a result screen with its level-ups); -Party=a,b,c, the staged state's party, set again as -Menu's menu or -Battle's
 * battle is asked for (a map's scripts choose their own), with -Ride=a,b,c whether each slot rides its gear then and
 * -GearOf=character:gear[,character:gear...] the gear a character has; -Order=a,b,... the order the player keeps the
 * staged party in (FPartyOrder: characters, the first shown first); -Looks=a,b,... the characters drawn in their other
 * look (FCharacterLooks: 9 the second Citan without his sword, 10 the grown Emeralda as the child, 1 Elly's purple hair); -Mighty, every
 * character and gear far stronger than the story asks, so that a battle a pad script fights is won (as the story
 * sweep's -strong), set as play begins;
 * -Menu=Mode,Parameter, a
 * menu the field opens as its scripts would ask for it, -MenuAt=S seconds into play (0: at once);
 * -MenuArea=Screen|Wide, how far the menus draw on a display wider than the original's screen;
 * -Battle=Formation[,Kind], a battle of the map's formation the field starts as its scripts would, -BattleAt=S seconds
 * into play (0: at once), and with -BattleEvery=S again each time the field runs S seconds or more after the last;
 * -LeaderAt=X,Z,Layer, the field's leader stood at that ground point of a walkmesh layer, its followers with it and the
 * view snapping to it, -LeaderAtTime=S seconds into play (1 by default: the map's entry over);
 * -Icons=Keyboard|Xbox|PlayStation|Nintendo|SteamDeck, the player's buttons shown with a set's icons
 * under the bindings the game has (without it, the game's own pictures);
 * -Saves=Folder, the folder the game keeps its saves in (without it the game has no save stores);
 * -States=Folder, the folder of the game's save state slots (without it the states' keys do nothing), and
 * -StateKeys=S:Key,S:Key,..., a state's key (Save, Load, Next, Previous) or the message log's (Log) pressed S seconds into play;
 * -Keys=a,b,..., the keys the options screen's Controls page hears, one each time it listens, a second after it
 * begins (an axis moved by its name and + or -: MouseX+); without it the screen has no Controls page;
 * -MenuAssets, an asset store that is ready and exports nothing, so the options screen has its Assets page.
 */
/** The glyphs of one icon set under the game's bindings, as a player of that device sees them. */
class FRenderGlyphs final : public Weltall::IButtonGlyphs
{
public:
	virtual Weltall::FButtonGlyph Glyph(Weltall::EPadButton Button) const override { return Weltall::ButtonGlyph(Button, Bindings, Set); }
	virtual uint32 Revision() const override { return Changes; }

	void Follow(const Weltall::FControlBindings& InBindings)
	{
		Bindings = InBindings;
		++Changes;
	}

	Weltall::EButtonIconSet Set = Weltall::EButtonIconSet::Keyboard;
	Weltall::FControlBindings Bindings = Weltall::FControlBindings::Defaults();
	uint32 Changes = 1;
};

/** The keys a render's player presses as the Controls page listens: the next of a list, a second after listening begins. */
class FRenderKeys final : public Weltall::IMenuKeys
{
public:
	virtual void Listen(bool bOn) override
	{
		bListening = bOn;
		ListenedAt.reset();
	}

	virtual std::optional<Weltall::FPressedKey> TakePressed() override;
	virtual Weltall::EInputFamily LastFamily() const override { return Weltall::FamilyOfIconSet(Set); }
	virtual Weltall::EButtonIconSet GamepadIcons() const override { return Set == Weltall::EButtonIconSet::Keyboard ? Weltall::EButtonIconSet::Xbox : Set; }

	void Advance(double Elapsed);

	Weltall::EButtonIconSet Set = Weltall::EButtonIconSet::Keyboard;
	TArray<Weltall::FPressedKey> Keys;

private:
	bool bListening = false;
	std::optional<double> ListenedAt;
	double Now = 0.0;
	int32 Next = 0;
};

/** An asset store for the options screen that is always ready and starts no export. */
class FRenderAssets final : public Weltall::IMenuAssets
{
public:
	virtual Weltall::FMenuAssetProgress Progress() const override { return {}; }
	virtual bool StartExport(Weltall::EAssetExportKind) override { return false; }
	virtual Weltall::EShareableStart StartShareable(std::string_view, bool) override { return Weltall::EShareableStart::Started; }
	virtual void CancelExport() override {}
};

struct FRenderStaging
{
	bool bFullState = false;
	bool bWeakParty = false;
	bool bStrongParty = false;
	bool bRisingParty = false;
	bool bMightyParty = false;
	/** -PartyAp=N: every character's AP per turn, after the rest of the staged state; 0 leaves it. */
	int32 PartyAp = 0;
	/** -Party: the characters of the staged state's party; empty for Fei, Elly and Citan. */
	TArray<int32> Party;
	/** -Ride: per party slot, whether it rides its gear into the battle; empty for the map's own state. */
	TArray<int32> Ride;
	/** -GearOf=character:gear[,...]: the gear each character named is given with the staged state, as a story script gives one; pairs. */
	TArray<int32> GearOf;
	/** -Looks: the characters switched to their other look with the party; empty for the state's own looks. */
	TArray<int32> Looks;
	/** -Order: the characters the player shows the party in, the first first; empty for the story's order. A screen of its own state (the world map's) reads it too. */
	TArray<int32> Order;
	/** -MenuArea=Wide: the menus drawn past the 320 x 224 screen's edges (Screen, the original's clip, by default). */
	bool bWideMenuArea = false;
	int32 MenuMode = -1;
	int32 MenuParameter = 0;
	double MenuAt = 0.0;
	bool bMenuAsked = false;
	int32 BattleFormation = -1;
	int32 BattleKind = 0;
	double BattleAt = 0.0;
	double BattleEvery = 0.0;
	bool bBattleAsked = false;
	/** -LeaderAt=X,Z,Layer: where the field's leader is stood, -LeaderAtTime seconds into play; empty for where the map puts it. */
	TArray<int32> LeaderAt;
	double LeaderAtTime = 1.0;
	bool bLeaderPlaced = false;
	/** -Arena=a,b,c,d,e,f: the arena's match as a script's six bytes ask for it, -ArenaAt seconds into play. */
	std::optional<std::array<uint8, 6>> Arena;
	double ArenaAt = 0.0;
	bool bArenaAsked = false;
	/** -Card=N[,S]: a chapter's title card begun -CardAt seconds into play, wherever the story is, with the subtitle of first value S under it. */
	int32 CardChapter = 0;
	int32 CardSubtitle = 0;
	double CardAt = 0.0;
	bool bCardShown = false;
	/** -Icons: the glyphs the game is shown with. */
	bool bIcons = false;
	FRenderGlyphs Glyphs;
	/** -Saves: the player's saves for the render. */
	std::optional<Weltall::FSaveStore> Saves;
	/** -States: the save state slots; -StateKeys: their keys pressed in play, each once. */
	std::optional<Weltall::FSaveStateSlots> States;
	struct FStateKey
	{
		double At = 0.0;
		Weltall::EControlAction Action = Weltall::EControlAction::SaveState;
		bool bPressed = false;
	};
	TArray<FStateKey> StateKeys;
	/** -Keys: what the Controls page hears. */
	bool bKeys = false;
	FRenderKeys Keys;
	/** -MenuAssets: the options screen's asset store. */
	bool bMenuAssets = false;
	FRenderAssets Assets;

	/** The staging a command line asks for; false, logged, when it is malformed. */
	static bool Parse(const FString& Params, FRenderStaging& Staging);

	bool IsEmpty() const { return !bFullState && !bMightyParty && PartyAp == 0 && Looks.IsEmpty() && GearOf.IsEmpty() && MenuMode < 0 && BattleFormation < 0 && LeaderAt.IsEmpty() && !Arena && CardChapter <= 0 && !bWideMenuArea && !bIcons && !Saves && !States && StateKeys.IsEmpty() && !bKeys && !bMenuAssets; }

	/** Before the game's first frame. */
	void Begin(FGameLoopScreen& Screen);

	/** Before the frame Elapsed seconds into play. */
	void Advance(FGameLoopScreen& Screen, double Elapsed);

private:
	void ApplyParty(Weltall::FGameState& State) const;
};

/**
 * The game as a player's save leaves it: of the game's saves in the memory card images of Folder, the last one made
 * with the story no further than Story is loaded as the Menu loads it. False, logged, when there is none to load.
 */
bool LoadSaveOfStory(Weltall::FGameSession& Session, const FString& Folder, int32 Story);

/**
 * A state that fills every menu: everyone the gear shop can offer at once (all but the second Citan
 * and the grown Emeralda) joined, Fei, Elly and Citan in the party and hurt, gold, items, weapons,
 * accessories, gear parts and gear weapons in the inventory, Deathblows and abilities learned and
 * under way, Weltall damaged and short of fuel.
 */
void MakeFullGameState(Weltall::FGameState& State);
