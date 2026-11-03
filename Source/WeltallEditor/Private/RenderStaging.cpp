#include "RenderStaging.h"

#include "Foundation/LittleEndian.h"
#include "Game/GameSession.h"
#include "Game/GameState.h"
#include "Game/GearPartTables.h"
#include "Game/OriginalSave.h"
#include "Game/SaveLoad.h"
#include "Field/EventPlacement.h"
#include "Field/FieldFollowers.h"
#include "Game/FieldProgram.h"
#include "Game/GameLoop.h"
#include "HAL/FileManager.h"
#include "Menu/MenuSaveCards.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Screens/GameLoopScreen.h"

#include <filesystem>
#include <vector>
#include <initializer_list>
#include <span>
#include <utility>

DEFINE_LOG_CATEGORY_STATIC(LogRenderStaging, Log, All);

namespace
{
	/**
	 * Every character and gear far stronger than the story's battles ask for, as the story sweep makes them: full points
	 * at their caps, attack, defense and ether well above any enemy's, a gear's armor, output and fuel likewise.
	 */
	void MakeMightyParty(Weltall::FGameState& State)
	{
		constexpr std::size_t Characters = 0x26C;
		constexpr std::size_t Gears = 0x978;
		constexpr std::size_t RecordSize = 0xA4;
		constexpr std::size_t CharacterCount = 11;
		constexpr std::size_t GearCount = 20;
		const std::span<uint8> Block = State.Block();
		const auto Put16 = [](std::span<uint8> Record, std::size_t At, uint16 Value) { Weltall::LittleEndian::WriteU16(Record, At, Value); };
		const auto Put32 = [](std::span<uint8> Record, std::size_t At, uint32 Value) { Weltall::LittleEndian::WriteU32(Record, At, Value); };
		for (std::size_t Character = 0; Character < CharacterCount; ++Character)
		{
			const std::span<uint8> Record = Block.subspan(Characters + Character * RecordSize, RecordSize);
			Put16(Record, 0x4C, 999);
			Put16(Record, 0x4E, 999);
			Put16(Record, 0x50, 99);
			Put16(Record, 0x52, 99);
			// Attack, defense, agility as far as a battle counts it, ether, ether defense, hit, evade.
			for (const auto& [At, Value] : {std::pair<std::size_t, uint8>{0x58, 150}, {0x59, 150}, {0x5A, 16}, {0x5B, 150}, {0x5C, 150}, {0x5E, 99}, {0x5F, 90}})
			{
				Record[At] = Value;
			}
		}
		for (std::size_t Gear = 0; Gear < GearCount; ++Gear)
		{
			const std::span<uint8> Record = Block.subspan(Gears + Gear * RecordSize, RecordSize);
			Put16(Record, 0x38, 30000);
			Put16(Record, 0x3A, 30000);
			Record[0x3C] = 250;
			Put32(Record, 0x60, 60000);
			Put32(Record, 0x64, 60000);
			Put16(Record, 0x70, 20000);
			Put16(Record, 0x72, 20000);
		}
		// Every gear Deathblow and the three Infinity attacks a pilot learns at levels 50, 60 and 70.
		constexpr std::size_t LearnedGearDeathblows = 0x16C0 + 4;
		constexpr std::size_t LearnedSize = 0x20;
		constexpr uint16 EveryGearAttack = 0x1FFE;
		for (std::size_t Character = 0; Character < CharacterCount; ++Character)
		{
			Put16(Block, LearnedGearDeathblows + Character * LearnedSize, EveryGearAttack);
		}
	}

	/** The state block's places the state's accessors do not name. */
	constexpr std::size_t CharacterRecords = 0x26C;
	constexpr std::size_t CharacterRecordSize = 0xA4;
	/** A character's counts toward the Deathblows each basic attack teaches: seven words. */
	constexpr std::size_t DeathblowCounters = 0x90;
	constexpr std::size_t GearRecords = 0x978;
	constexpr std::size_t GearRecordSize = 0xA4;
	constexpr std::size_t GearFuel = 0x38;
	constexpr std::size_t GearMaxFuel = 0x3A;
	constexpr std::size_t GearHp = 0x60;
	constexpr std::size_t GearMaxHp = 0x64;
	/** What each character has learned: its Deathblows' word, then its abilities', the first learned at bit 15. */
	constexpr std::size_t LearnedSkills = 0x16C0;
	constexpr std::size_t LearnedSkillsSize = 0x20;
	constexpr std::size_t LearnedAbilities = 0x02;
	constexpr std::size_t ApPerTurn = 0x17;
	constexpr std::size_t ExpToNextLevel = 0x44;
	constexpr std::size_t EtherExpToNextLevel = 0x48;
	constexpr int32 Characters = 11;

	/** The inventory's lists, as the scripts' item codes number them. */
	enum EInventoryList : int32
	{
		Items = 0,
		Weapons = 1,
		Accessories = 2,
		GearWeapons = 3,
		GearParts = 4,
	};

	constexpr int32 Fei = 0;
	constexpr int32 Elly = 1;
	constexpr int32 Citan = 2;
	constexpr uint8 WeltallGear = 0;

	/** Fills a list's first slots with (item, count) entries. */
	void Stock(Weltall::FGameState& State, int32 List, std::initializer_list<std::pair<uint8, uint8>> Entries)
	{
		const std::span<uint8> Counts = State.InventoryCounts(List);
		const std::span<uint8> Ids = State.InventoryItems(List);
		std::size_t Slot = 0;
		for (const auto& [Item, Count] : Entries)
		{
			Ids[Slot] = Item;
			Counts[Slot] = Count;
			++Slot;
		}
	}

	void SetWord(Weltall::FGameState& State, std::size_t At, uint16 Value)
	{
		Weltall::LittleEndian::WriteU16(State.Block(), At, Value);
	}

	void Hurt(Weltall::FGameState& State, int32 Character, int32 HpPercent, int32 EpPercent)
	{
		using EPoints = Weltall::FGameState::ECharacterPoints;
		State.SetCharacterPoints(Character, EPoints::Hp, static_cast<uint16>(State.CharacterPoints(Character, EPoints::MaxHp) * HpPercent / 100));
		State.SetCharacterPoints(Character, EPoints::Ep, static_cast<uint16>(State.CharacterPoints(Character, EPoints::MaxEp) * EpPercent / 100));
	}

	/** Stands the field's leader at (X, Z) of a walkmesh layer, its followers with it and the view snapping to it (logged where there is no ground). */
	bool StandLeader(Weltall::FEventContext& Context, int32 X, int32 Z, int32 Layer)
	{
		// The leader first, then each follower on the same point: the trail they follow starts afresh there.
		std::vector<uint8> Placed{Context.LeaderActor};
		for (const uint8 Member : Context.PartyActors)
		{
			Weltall::FEventActorState* Follower = Member != Context.LeaderActor ? Context.StateOf(Member) : nullptr;
			if (Follower != nullptr && (Follower->Flags0 & Weltall::EventActorFlags0::PartyFollower) != 0)
			{
				Placed.push_back(Member);
			}
		}
		for (const uint8 Actor : Placed)
		{
			Weltall::FEventActorState* State = Context.StateOf(Actor);
			if (State == nullptr || Actor >= Context.Actors.size())
			{
				continue;
			}
			Weltall::FEventActorState Moved = *State;
			Weltall::FFieldActor Record = Context.Actors[Actor];
			Moved.Layer = static_cast<uint16>(Layer);
			if (!Weltall::PlaceActor(Moved, Record, *Context.Walkmesh, X, Z))
			{
				UE_LOG(LogRenderStaging, Error, TEXT("-LeaderAt: layer %d holds no ground at (%d, %d)"), Layer, X, Z);
				return true;
			}
			*State = Moved;
			Context.Actors[Actor] = Record;
			if (Weltall::FSprite* Sprite = Context.Models != nullptr ? Context.Models->ActorSprite(Actor) : nullptr)
			{
				Sprite->FloorY = State->FloorHeight;
				Sprite->Position = State->Position;
				Sprite->Velocity = {};
			}
		}
		Weltall::RestartFollowTrail(Context);
		Context.Camera.SnapTicks = 2;
		UE_LOG(LogRenderStaging, Display, TEXT("Stood the leader at (%d, %d) on layer %d"), X, Z, Layer);
		return true;
	}
}

bool FRenderStaging::Parse(const FString& Params, FRenderStaging& Staging)
{
	FString State;
	if (FParse::Value(*Params, TEXT("State="), State))
	{
		if (State != TEXT("Full") && State != TEXT("Weak") && State != TEXT("Strong") && State != TEXT("Rising"))
		{
			UE_LOG(LogRenderStaging, Error, TEXT("Unknown -State=%s (Full, Weak, Strong, Rising)"), *State);
			return false;
		}
		Staging.bFullState = true;
		Staging.bWeakParty = State == TEXT("Weak");
		Staging.bRisingParty = State == TEXT("Rising");
		Staging.bStrongParty = Staging.bRisingParty || State == TEXT("Strong");
	}
	Staging.bMightyParty = FParse::Param(*Params, TEXT("Mighty"));
	FParse::Value(*Params, TEXT("PartyAp="), Staging.PartyAp);
	if (Staging.PartyAp < 0 || Staging.PartyAp > 7)
	{
		UE_LOG(LogRenderStaging, Error, TEXT("-PartyAp takes 3 to 7"));
		return false;
	}
	// The world map's own screen takes -Party for the party it starts with, and -Looks. Its dash keeps a setting's key ending in "party"
	// (-Setting=...rebalanced_party=true) from reading as the option.
	FString Screen;
	FParse::Value(*Params, TEXT("Screen="), Screen);
	FString Party;
	FString Cards;
	const bool bSavedState = FParse::Value(*Params, TEXT("FieldCards="), Cards);
	if (Screen != TEXT("WorldMap") && FParse::Value(*Params, TEXT("-Party="), Party, false))
	{
		TArray<FString> Parts;
		Party.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() != 3 || !(Staging.bFullState || bSavedState))
		{
			UE_LOG(LogRenderStaging, Error, TEXT("-Party takes a,b,c and a -State or a save (-FieldCards)"));
			return false;
		}
		for (const FString& Part : Parts)
		{
			Staging.Party.Add(FCString::Atoi(*Part));
		}
	}
	FString Ride;
	if (FParse::Value(*Params, TEXT("Ride="), Ride, false))
	{
		TArray<FString> Parts;
		Ride.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() != 3)
		{
			UE_LOG(LogRenderStaging, Error, TEXT("-Ride takes a,b,c"));
			return false;
		}
		for (const FString& Part : Parts)
		{
			Staging.Ride.Add(FCString::Atoi(*Part));
		}
	}
	FString GearOf;
	if (FParse::Value(*Params, TEXT("GearOf="), GearOf, false))
	{
		TArray<FString> Pairs;
		GearOf.ParseIntoArray(Pairs, TEXT(","));
		for (const FString& Pair : Pairs)
		{
			TArray<FString> Parts;
			Pair.ParseIntoArray(Parts, TEXT(":"));
			if (Parts.Num() != 2)
			{
				UE_LOG(LogRenderStaging, Error, TEXT("-GearOf takes character:gear, or several with commas between"));
				return false;
			}
			Staging.GearOf.Add(FCString::Atoi(*Parts[0]));
			Staging.GearOf.Add(FCString::Atoi(*Parts[1]));
		}
	}
	FString Looks;
	if (Screen != TEXT("WorldMap") && FParse::Value(*Params, TEXT("Looks="), Looks, false))
	{
		TArray<FString> Parts;
		Looks.ParseIntoArray(Parts, TEXT(","));
		for (const FString& Part : Parts)
		{
			Staging.Looks.Add(FCString::Atoi(*Part));
		}
	}
	FString Order;
	if (FParse::Value(*Params, TEXT("Order="), Order, false))
	{
		TArray<FString> Parts;
		Order.ParseIntoArray(Parts, TEXT(","));
		for (const FString& Part : Parts)
		{
			Staging.Order.Add(FCString::Atoi(*Part));
		}
	}
	FString Card;
	if (FParse::Value(*Params, TEXT("Card="), Card, false))
	{
		FString Chapter;
		FString Subtitle;
		if (Card.Split(TEXT(","), &Chapter, &Subtitle))
		{
			Staging.CardChapter = FCString::Atoi(*Chapter);
			Staging.CardSubtitle = FCString::Atoi(*Subtitle);
		}
		else
		{
			Staging.CardChapter = FCString::Atoi(*Card);
		}
	}
	FParse::Value(*Params, TEXT("CardAt="), Staging.CardAt);
	FString Area;
	if (FParse::Value(*Params, TEXT("MenuArea="), Area))
	{
		if (Area != TEXT("Screen") && Area != TEXT("Wide"))
		{
			UE_LOG(LogRenderStaging, Error, TEXT("Unknown -MenuArea=%s (Screen, Wide)"), *Area);
			return false;
		}
		Staging.bWideMenuArea = Area == TEXT("Wide");
	}
	// With its dash: the setting controls.original_button_icons would match the bare name.
	FString Icons;
	if (FParse::Value(*Params, TEXT("-Icons="), Icons))
	{
		static const TCHAR* const Sets[] = {TEXT("Keyboard"), TEXT("Xbox"), TEXT("PlayStation"), TEXT("Nintendo"), TEXT("SteamDeck")};
		const int32 Found = TArrayView<const TCHAR* const>(Sets).IndexOfByPredicate([&Icons](const TCHAR* Name) { return Icons == Name; });
		if (Found == INDEX_NONE)
		{
			UE_LOG(LogRenderStaging, Error, TEXT("Unknown -Icons=%s (Keyboard, Xbox, PlayStation, Nintendo, SteamDeck)"), *Icons);
			return false;
		}
		Staging.bIcons = true;
		Staging.Glyphs.Set = static_cast<Weltall::EButtonIconSet>(Found);
	}
	FString Keys;
	if (FParse::Value(*Params, TEXT("Keys="), Keys, false))
	{
		TArray<FString> Parts;
		Keys.ParseIntoArray(Parts, TEXT(","));
		for (const FString& Part : Parts)
		{
			const bool bAxis = Part.EndsWith(TEXT("+")) || Part.EndsWith(TEXT("-"));
			const FString Name = bAxis ? Part.LeftChop(1) : Part;
			Staging.Keys.Keys.Add({TCHAR_TO_UTF8(*Name), bAxis ? (Part.EndsWith(TEXT("+")) ? 1 : -1) : 0});
		}
		Staging.bKeys = true;
	}
	Staging.bMenuAssets = FParse::Param(*Params, TEXT("MenuAssets"));
	FString Saves;
	if (FParse::Value(*Params, TEXT("Saves="), Saves, false))
	{
		Staging.Saves.emplace(std::filesystem::path(*FPaths::ConvertRelativePathToFull(Saves)));
	}
	FString States;
	if (FParse::Value(*Params, TEXT("States="), States, false))
	{
		Staging.States.emplace(std::filesystem::path(*FPaths::ConvertRelativePathToFull(States)));
	}
	FString StateKeys;
	if (FParse::Value(*Params, TEXT("StateKeys="), StateKeys, false))
	{
		TArray<FString> Presses;
		StateKeys.ParseIntoArray(Presses, TEXT(","));
		for (const FString& Press : Presses)
		{
			FString At;
			FString Key;
			const TMap<FString, Weltall::EControlAction> Named{{TEXT("Save"), Weltall::EControlAction::SaveState},
				{TEXT("Load"), Weltall::EControlAction::LoadState}, {TEXT("Next"), Weltall::EControlAction::NextStateSlot},
				{TEXT("Previous"), Weltall::EControlAction::PreviousStateSlot}, {TEXT("Log"), Weltall::EControlAction::MessageLog}};
			if (!Press.Split(TEXT(":"), &At, &Key) || !At.IsNumeric() || !Named.Contains(Key))
			{
				UE_LOG(LogRenderStaging, Error, TEXT("-StateKeys takes S:Key, each after a ',', Key Save, Load, Next, Previous or Log: '%s'"), *Press);
				return false;
			}
			Staging.StateKeys.Add({FCString::Atod(*At), Named[Key]});
		}
	}
	FString Menu;
	if (FParse::Value(*Params, TEXT("Menu="), Menu, false))
	{
		TArray<FString> Parts;
		Menu.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() < 1 || Parts.Num() > 2)
		{
			UE_LOG(LogRenderStaging, Error, TEXT("-Menu takes Mode or Mode,Parameter"));
			return false;
		}
		Staging.MenuMode = FCString::Atoi(*Parts[0]);
		Staging.MenuParameter = Parts.Num() > 1 ? FCString::Atoi(*Parts[1]) : 0;
		FParse::Value(*Params, TEXT("MenuAt="), Staging.MenuAt);
	}
	FString Battle;
	if (FParse::Value(*Params, TEXT("Battle="), Battle, false))
	{
		TArray<FString> Parts;
		Battle.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() < 1 || Parts.Num() > 2)
		{
			UE_LOG(LogRenderStaging, Error, TEXT("-Battle takes Formation or Formation,Kind"));
			return false;
		}
		Staging.BattleFormation = FCString::Atoi(*Parts[0]);
		Staging.BattleKind = Parts.Num() > 1 ? FCString::Atoi(*Parts[1]) : 0;
		FParse::Value(*Params, TEXT("BattleAt="), Staging.BattleAt);
		FParse::Value(*Params, TEXT("BattleEvery="), Staging.BattleEvery);
	}
	FString LeaderAt;
	if (FParse::Value(*Params, TEXT("LeaderAt="), LeaderAt, false))
	{
		TArray<FString> Parts;
		LeaderAt.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() != 3)
		{
			UE_LOG(LogRenderStaging, Error, TEXT("-LeaderAt takes X,Z,Layer"));
			return false;
		}
		for (const FString& Part : Parts)
		{
			Staging.LeaderAt.Add(FCString::Atoi(*Part));
		}
		FParse::Value(*Params, TEXT("LeaderAtTime="), Staging.LeaderAtTime);
	}
	// With its dash: the setting mods.perfect_works.arena would match the bare name.
	FString Arena;
	if (FParse::Value(*Params, TEXT("-Arena="), Arena, false))
	{
		TArray<FString> Parts;
		Arena.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() != 6)
		{
			UE_LOG(LogRenderStaging, Error, TEXT("-Arena takes the match's six bytes: From,Kind,Gear1,Gear2,Wins,Level"));
			return false;
		}
		std::array<uint8, 6> Match{};
		for (int32 Byte = 0; Byte < 6; ++Byte)
		{
			Match[static_cast<std::size_t>(Byte)] = static_cast<uint8>(FCString::Atoi(*Parts[Byte]));
		}
		Staging.Arena = Match;
		FParse::Value(*Params, TEXT("-ArenaAt="), Staging.ArenaAt);
	}
	return true;
}

void FRenderStaging::Begin(FGameLoopScreen& Screen)
{
	Screen.SetMenuDrawingAreaWide(bWideMenuArea);
	if (Saves)
	{
		Screen.SetSaves(&*Saves);
	}
	if (States)
	{
		Screen.SetSaveStates(&*States);
	}
	if (bIcons)
	{
		Screen.SetButtonGlyphs(&Glyphs);
	}
	if (bKeys)
	{
		Keys.Set = Glyphs.Set;
		Screen.SetMenuKeys(&Keys);
	}
	if (bMenuAssets)
	{
		Screen.SetMenuAssets(&Assets);
	}
	if (Weltall::FGameState* State = Screen.GameState(); State != nullptr && bFullState)
	{
		MakeFullGameState(*State);
		ApplyParty(*State);
		for (int32 Character = 0; Character < Characters; ++Character)
		{
			if (bWeakParty)
			{
				State->SetCharacterPoints(Character, Weltall::FGameState::ECharacterPoints::Hp, 1);
			}
			if (bStrongParty)
			{
				State->Block()[LearnedSkills + Character * LearnedSkillsSize + ApPerTurn] = 6;
			}
			if (bRisingParty)
			{
				const std::size_t Record = CharacterRecords + Character * CharacterRecordSize;
				Weltall::LittleEndian::WriteU32(State->Block(), Record + ExpToNextLevel, 1);
				Weltall::LittleEndian::WriteU32(State->Block(), Record + EtherExpToNextLevel, 1);
			}
		}
		if (bRisingParty)
		{
			SetWord(*State, LearnedSkills + Elly * LearnedSkillsSize + LearnedAbilities, 0);
		}
	}
	else if (Weltall::FGameState* Own = Screen.GameState(); Own != nullptr && (!Party.IsEmpty() || !Ride.IsEmpty() || !GearOf.IsEmpty() || !Looks.IsEmpty()))
	{
		// The game's own state, with what is staged of its party.
		ApplyParty(*Own);
	}
	if (Weltall::FGameState* Mighty = Screen.GameState(); Mighty != nullptr && bMightyParty)
	{
		MakeMightyParty(*Mighty);
	}
	if (Weltall::FGameState* State = Screen.GameState(); State != nullptr && PartyAp != 0)
	{
		for (int32 Character = 0; Character < Characters; ++Character)
		{
			State->Block()[LearnedSkills + Character * LearnedSkillsSize + ApPerTurn] = static_cast<uint8>(PartyAp);
		}
	}
	if (Weltall::FGameState* State = Screen.GameState(); State != nullptr && !Order.IsEmpty())
	{
		std::vector<uint8> Shown;
		for (const int32 Character : Order)
		{
			Shown.push_back(static_cast<uint8>(Character));
		}
		Weltall::FPartyOrder Arranged;
		Arranged.Arrange(State->Party(), Shown);
		State->SetPartyOrder(Arranged);
	}
}

void FRenderStaging::ApplyParty(Weltall::FGameState& State) const
{
	if (Party.Num() == 3)
	{
		State.SetParty({static_cast<uint8>(Party[0]), static_cast<uint8>(Party[1]), static_cast<uint8>(Party[2])});
		// Joined and shown in the menus, as a map's scripts may have left only their own party so.
		for (const int32 Character : Party)
		{
			if (Character >= 0 && Character < Characters)
			{
				State.SetJoinedCharacters(static_cast<uint16>(State.JoinedCharacters() | 1u << Character));
				State.SetMenuCharacters(static_cast<uint16>(State.MenuCharacters() | 1u << Character));
			}
		}
	}
	for (int32 Slot = 0; Slot < Ride.Num(); ++Slot)
	{
		State.SetPartySlotInGear(Slot, static_cast<uint8>(Ride[Slot]));
	}
	for (int32 Pair = 0; Pair + 1 < GearOf.Num(); Pair += 2)
	{
		State.SetCharacterGear(GearOf[Pair], static_cast<uint8>(GearOf[Pair + 1]));
	}
	if (!Looks.IsEmpty())
	{
		Weltall::FCharacterLooks Switched;
		for (const int32 Character : Looks)
		{
			Switched.Switch(static_cast<uint8>(Character));
		}
		State.SetCharacterLooks(Switched);
	}
}

std::optional<Weltall::FPressedKey> FRenderKeys::TakePressed()
{
	constexpr double Delay = 1.0;
	if (!bListening || !ListenedAt || Now < *ListenedAt + Delay || !Keys.IsValidIndex(Next))
	{
		return std::nullopt;
	}
	return Keys[Next++];
}

void FRenderKeys::Advance(double Elapsed)
{
	Now = Elapsed;
	if (bListening && !ListenedAt)
	{
		ListenedAt = Elapsed;
	}
}

void FRenderStaging::Advance(FGameLoopScreen& Screen, double Elapsed)
{
	Keys.Advance(Elapsed);
	if (MenuMode >= 0 && !bMenuAsked && Elapsed >= MenuAt)
	{
		if (Weltall::FGameState* State = Screen.GameState())
		{
			ApplyParty(*State);
		}
		bMenuAsked = Screen.RequestMenu(static_cast<uint8>(MenuMode), static_cast<uint8>(MenuParameter));
	}
	if (BattleFormation >= 0 && (!bBattleAsked || BattleEvery > 0.0) && Elapsed >= BattleAt)
	{
		if (Weltall::FGameState* State = Screen.GameState())
		{
			ApplyParty(*State);
			// The field writes its own party back every frame: the battle takes the staged one only if the field holds it too.
			Weltall::FGameLoop* Loop = Screen.Loop();
			if (Weltall::FFieldProgram* Field = Loop != nullptr ? Loop->Field() : nullptr; Field != nullptr && Party.Num() == 3)
			{
				Field->Runtime().Context().PartyCharacters = State->Party();
			}
			// A map's entry may have healed the party: a point from death again as the battle starts.
			for (int32 Character = 0; bWeakParty && Character < Characters; ++Character)
			{
				State->SetCharacterPoints(Character, Weltall::FGameState::ECharacterPoints::Hp, 1);
			}
		}
		if (Screen.RequestBattle(static_cast<uint8>(BattleFormation), static_cast<uint8>(BattleKind)))
		{
			bBattleAsked = true;
			BattleAt = Elapsed + BattleEvery;
		}
	}
	if (!LeaderAt.IsEmpty() && !bLeaderPlaced && Elapsed >= LeaderAtTime)
	{
		Weltall::FGameLoop* Loop = Screen.Loop();
		if (Weltall::FFieldProgram* Field = Loop != nullptr ? Loop->Field() : nullptr)
		{
			bLeaderPlaced = StandLeader(Field->Runtime().Context(), LeaderAt[0], LeaderAt[1], LeaderAt[2]);
		}
	}
	if (Arena && !bArenaAsked && Elapsed >= ArenaAt)
	{
		bArenaAsked = Screen.RequestArena(*Arena);
	}
	if (CardChapter > 0 && !bCardShown && Elapsed >= CardAt)
	{
		bCardShown = Screen.ShowChapterCard(CardChapter, CardSubtitle);
	}
	for (FStateKey& Key : StateKeys)
	{
		if (!Key.bPressed && Elapsed >= Key.At)
		{
			Screen.PressPortKey(Key.Action);
			Key.bPressed = true;
		}
	}
}

bool LoadSaveOfStory(Weltall::FGameSession& Session, const FString& Folder, int32 Story)
{
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Folder, TEXT("*"), true, false);
	Files.Sort();
	std::optional<Weltall::FOriginalSave> Chosen;
	for (const FString& File : Files)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *File))
		{
			continue;
		}
		const std::span<const uint8> Data(Bytes.GetData(), static_cast<std::size_t>(Bytes.Num()));
		auto Saves = Weltall::ReadCardImageSaves(Data);
		if (!Saves)
		{
			// Or one of the port's own save files, a save's data alone.
			auto Own = Weltall::FOriginalSave::FromData(Data);
			if (!Own)
			{
				continue;
			}
			Saves = std::vector<Weltall::FOriginalSave>{std::move(*Own)};
		}
		for (Weltall::FOriginalSave& Save : *Saves)
		{
			if (Save.StoryProgress() <= Story && (!Chosen || Save.StoryProgress() > Chosen->StoryProgress()))
			{
				Chosen.emplace(std::move(Save));
			}
		}
	}
	if (!Chosen)
	{
		UE_LOG(LogRenderStaging, Error, TEXT("No save of the folder was made with the story at %d or before"), Story);
		return false;
	}
	const auto Tables = Weltall::FGearPartTables::Load(Session.Image(), Session.Index());
	if (!Tables)
	{
		UE_LOG(LogRenderStaging, Error, TEXT("The gear parts cannot be read: %hs"), Tables.Error().Message.c_str());
		return false;
	}
	auto Loaded = Weltall::LoadOriginalSave(*Chosen, Session.Game(), *Tables, Session.Text().NameTables);
	if (!Loaded)
	{
		UE_LOG(LogRenderStaging, Error, TEXT("The save cannot be loaded: %hs"), Loaded.Error().Message.c_str());
		return false;
	}
	UE_LOG(LogRenderStaging, Display, TEXT("Loaded the save made with the story at %d"), Chosen->StoryProgress());
	Session.Game() = std::move(*Loaded);
	return true;
}

void MakeFullGameState(Weltall::FGameState& State)
{
	State.SetJoinedCharacters(0x01FF);
	State.SetMenuCharacters(0x07FF);
	State.SetParty({Fei, Elly, Citan});
	State.SetGold(123456);
	Stock(State, Items, {{1, 5}, {6, 3}, {10, 2}, {50, 1}, {57, 12}, {51, 1}, {2, 1}, {3, 2}, {4, 1}, {7, 1}, {12, 3}, {13, 1}, {15, 1}, {19, 1},
		{21, 1}, {25, 1}, {35, 1}, {44, 1}, {58, 2}, {5, 1}, {9, 1}, {8, 120}});
	Stock(State, Weapons, {{1, 3}, {32, 1}, {33, 2}, {50, 2}, {51, 1}, {53, 3}, {54, 1}, {56, 1}, {67, 3}, {68, 1}, {57, 1}, {58, 2}});
	Stock(State, Accessories, {{2, 1}, {3, 2}, {17, 3}, {18, 99}, {90, 5}, {91, 1}, {93, 1}, {94, 2}, {98, 1}, {99, 1}, {104, 1}, {116, 1}, {25, 1}});
	Stock(State, GearParts, {{2, 1}, {5, 2}, {43, 1}, {45, 3}, {49, 1}, {10, 1}, {90, 3}});
	Stock(State, GearWeapons, {{32, 1}, {33, 1}, {51, 3}, {54, 2}, {57, 1}, {61, 1}});
	Hurt(State, Fei, 40, 100);
	Hurt(State, Elly, 75, 50);
	Hurt(State, Citan, 90, 20);
	// Fei's first three Deathblows and abilities, Citan's first and fifth Deathblows and first ability.
	SetWord(State, LearnedSkills + Fei * LearnedSkillsSize, 0xE000);
	SetWord(State, LearnedSkills + Fei * LearnedSkillsSize + LearnedAbilities, 0xE000);
	SetWord(State, LearnedSkills + Citan * LearnedSkillsSize, 0x8800);
	SetWord(State, LearnedSkills + Citan * LearnedSkillsSize + LearnedAbilities, 0x8000);
	for (std::size_t Attack = 0; Attack < 7; ++Attack)
	{
		SetWord(State, CharacterRecords + Fei * CharacterRecordSize + DeathblowCounters + 2 * Attack, static_cast<uint16>((Attack + 1) * 37 % 400));
		SetWord(State, CharacterRecords + Citan * CharacterRecordSize + DeathblowCounters + 2 * Attack, static_cast<uint16>((Attack + 1) * 91 % 700));
	}
	const std::size_t Gear = GearRecords + WeltallGear * GearRecordSize;
	const uint32 MaxHp = Weltall::LittleEndian::ReadU32(State.Block(), Gear + GearMaxHp);
	Weltall::LittleEndian::WriteU32(State.Block(), Gear + GearHp, MaxHp * 2 / 3);
	SetWord(State, Gear + GearFuel, static_cast<uint16>(Weltall::LittleEndian::ReadU16(State.Block(), Gear + GearMaxFuel) / 2));
}
