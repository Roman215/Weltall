#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Game/CharacterLooks.h"
#include "Game/ClearGame.h"
#include "Game/PartyOrder.h"
#include "Game/StoryMode.h"

#include <array>
#include <optional>
#include <span>
#include <string>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FMesNameTables;

	/**
	 * What the game keeps from one moment of play to the next, and saves: the kernel's state block,
	 * byte for byte as every program shares it (names, the characters' and gears' records, the
	 * inventory, the party, the field's saved event variables...), the battle scripts' globals and
	 * the play time; and of the port's own, how the game tells the second disc's story, the order the player keeps
	 * the party in, the looks, and whether the game goes on from a clear file.
	 */
	class FGameState
	{
	public:
		static constexpr std::size_t BlockSize = 0x2358;
		/** The characters' names (11), then the gears' (20), in the message encoding. */
		static constexpr int32 NameCount = 31;
		static constexpr std::size_t NameBytes = 0x14;
		/** Event variables 0x000-0x3FE, the half the field keeps from map to map. */
		static constexpr int32 SavedEventVariableCount = 0x200;
		static constexpr int32 BattleGlobalCount = 16;
		static constexpr uint8 EmptySlot = 0xFF;
		static constexpr int32 CharacterCount = 11;
		static constexpr int32 GearCount = 20;
		/** The inventory's lists, numbered as the high byte of the scripts' item codes. */
		static constexpr int32 InventoryListCount = 5;

		/** A character's current and highest HP and EP. */
		enum class ECharacterPoints : uint8
		{
			Hp,
			MaxHp,
			Ep,
			MaxEp,
		};

		/**
		 * The state a new game starts from: the new-game file, its names turned from glyph indices
		 * into the message encoding through the name tables' glyph map. Its play time starts at zero;
		 * the console's counts on from power-on.
		 */
		WELTALLCORE_API static TResult<FGameState> NewGame(std::span<const uint8> NewGameFile, const FMesNameTables& NameTables);

		std::span<const uint8, BlockSize> Block() const { return Bytes; }
		std::span<uint8, BlockSize> Block() { return Bytes; }

		/** A name's bytes up to its terminator. */
		WELTALLCORE_API std::span<const uint8> Name(int32 Index) const;

		/** Saved event variable Index: the one the scripts address as 2 x Index. */
		WELTALLCORE_API uint16 SavedEventVariable(int32 Index) const;
		WELTALLCORE_API void SetSavedEventVariable(int32 Index, uint16 Value);

		/** The characters in the party's three slots, EmptySlot where nobody is. */
		WELTALLCORE_API std::array<uint8, 3> Party() const;
		WELTALLCORE_API void SetParty(const std::array<uint8, 3>& Characters);

		WELTALLCORE_API uint32 Gold() const;
		WELTALLCORE_API void SetGold(uint32 Value);

		/** An inventory list's slots: how many of the item each holds, and the item's number in the list. */
		WELTALLCORE_API std::span<uint8> InventoryCounts(int32 List);
		WELTALLCORE_API std::span<uint8> InventoryItems(int32 List);
		/** One fewer of an item, from the first slot holding some, an emptied slot's item FFh; false where none is held. */
		WELTALLCORE_API bool TakeItem(int32 List, uint8 Item);

		/** Bit c set: character c has joined. */
		WELTALLCORE_API uint16 JoinedCharacters() const;
		WELTALLCORE_API void SetJoinedCharacters(uint16 Characters);
		/** A second mask of characters: the menus offer a character only with its bit set here too. */
		WELTALLCORE_API uint16 MenuCharacters() const;
		WELTALLCORE_API void SetMenuCharacters(uint16 Characters);
		/** Bit c set: character c cannot be moved in Member Change. */
		WELTALLCORE_API uint16 LockedCharacters() const;
		WELTALLCORE_API void SetLockedCharacters(uint16 Characters);

		/** Whether the member in a party slot (0-2) rides its gear: its byte, nonzero when it does. */
		WELTALLCORE_API uint8 PartySlotInGear(int32 Slot) const;
		WELTALLCORE_API void SetPartySlotInGear(int32 Slot, uint8 InGear);

		/**
		 * The gear a character pilots, 0xFF for none, read as the field reads it for any number: past
		 * the characters, the byte at the same place of a later record; nothing past the block.
		 */
		WELTALLCORE_API std::optional<uint8> CharacterGear(int32 Character) const;
		/** Sets a character's gear, for any number as CharacterGear reads it; false past the block. */
		WELTALLCORE_API bool SetCharacterGear(int32 Character, uint8 Gear);

		/** A character's weight, and the weight food has added to it. */
		WELTALLCORE_API uint8 CharacterWeight(int32 Character) const;
		WELTALLCORE_API uint8 CharacterWeightGain(int32 Character) const;
		WELTALLCORE_API void SetCharacterWeightGain(int32 Character, uint8 Gain);

		WELTALLCORE_API uint16 CharacterPoints(int32 Character, ECharacterPoints Which) const;
		WELTALLCORE_API void SetCharacterPoints(int32 Character, ECharacterPoints Which, uint16 Value);

		/** Every gear's HP and fuel back to their maxima. */
		WELTALLCORE_API void RestoreGears();
		/** A gear's HP and the most it holds; nothing for a gear the state keeps no record of. */
		WELTALLCORE_API std::optional<uint32> GearHp(int32 Gear) const;
		WELTALLCORE_API std::optional<uint32> GearMaxHp(int32 Gear) const;
		WELTALLCORE_API bool SetGearHp(int32 Gear, uint32 Hp);

		/**
		 * A character's record and learned skills copied over another's, and a gear's record over another's, as
		 * the story makes its stand-ins (Citan with his sword, the grown Emeralda); false, copying nothing,
		 * unless the state keeps both records.
		 */
		WELTALLCORE_API bool CopyCharacter(int32 From, int32 To);
		WELTALLCORE_API bool CopyGear(int32 From, int32 To);

		/** The special skills a character has been given, a bit each; nothing for a character without a record. */
		WELTALLCORE_API std::optional<uint16> CharacterSpecialSkills(int32 Character) const;
		WELTALLCORE_API bool SetCharacterSpecialSkills(int32 Character, uint16 Skills);

		/** The story's flag word, which the battles read. */
		WELTALLCORE_API uint16 StoryFlags() const;
		WELTALLCORE_API void SetStoryFlags(uint16 Flags);

		/** The arena's gears player 1 has beaten against the computer, a bit each by the arena's gear number. */
		WELTALLCORE_API std::array<uint8, 8> ArenaDefeatedGears() const;
		WELTALLCORE_API void SetArenaDefeatedGears(const std::array<uint8, 8>& Gears);
		/**
		 * The arena's settings word: bits 0-3 are 1 once it is written, 4 and 5 the two pads' vibration, 6-12 the wins a set
		 * takes, 13-15 the computer's level, 16 the gear won by beating every other.
		 */
		WELTALLCORE_API uint32 ArenaSettings() const;
		WELTALLCORE_API void SetArenaSettings(uint32 Settings);

		/** The disc play continues on when the state is loaded, 0 for the first: the one it was saved on. */
		WELTALLCORE_API int32 SavedDisc() const;

		/**
		 * Where play resumes when a program starts from this state: a field map, or from 400h up a
		 * place on the world map. Bits 14 and 15 are the field's gear-mode flags.
		 */
		WELTALLCORE_API uint16 Place() const;
		WELTALLCORE_API void SetPlace(uint16 Place);

		/**
		 * The field's place as it leaves the field or saves: the map (bits 14 and 15 its gear mode),
		 * the camera's yaw, the entrance its scripts set for the next map, in the word the world map
		 * keeps its scene in, and the song asked for.
		 */
		WELTALLCORE_API void SetFieldPlace(uint16 Map, uint16 CameraYaw, uint16 Entrance, uint16 Song);
		/** The camera's yaw the field leaves with (4096 a turn), the scene's word (the world map's scene, or the field's next entrance) and the field's song. */
		WELTALLCORE_API uint16 CameraYaw() const;
		WELTALLCORE_API uint16 Scene() const;
		WELTALLCORE_API uint16 FieldSong() const;
		/** The field's song alone, and its entrance for the next map alone (in the scene's word). */
		WELTALLCORE_API void SetFieldSong(uint16 Song);
		WELTALLCORE_API void SetFieldEntrance(uint16 Entrance);

		/**
		 * Where the world map starts, as the field's scripts leave for it: the place (400h up), the
		 * entrance, the camera's yaw (half a turn from the field's) and the scene.
		 */
		WELTALLCORE_API void SetWorldMapPlace(uint16 Place, uint16 Entrance, uint16 CameraYaw, uint16 Scene);

		/**
		 * Where the world map keeps the party, which the field's scripts read and set: x, y and z (the
		 * world's coordinates >> 12) and the direction it starts facing with flag 2000h.
		 */
		WELTALLCORE_API std::array<uint16, 4> WorldMapPosition() const;
		WELTALLCORE_API void SetWorldMapPosition(const std::array<uint16, 4>& Position);

		/** The world map's flags, which the field's scripts read and set (bit 2000h: start where the party stood). */
		WELTALLCORE_API uint16 WorldMapFlags() const;
		WELTALLCORE_API void SetWorldMapFlags(uint16 Flags);
		/** The Yggdrasil's place on the world map and its heading, in whole units, as the world map keeps them. */
		WELTALLCORE_API void SetWorldMapVehicle(int16 X, int16 Y, int16 Z, int16 Heading);

		/** Where the world map keeps its sea object (the Thames), which the field's scripts read: x and z. */
		WELTALLCORE_API std::array<uint16, 2> SeaObjectPlace() const;
		/** Where the world map keeps its air object (Shevat), which the field's scripts read: x and z, whole units. */
		WELTALLCORE_API std::array<uint16, 2> AirObjectPlace() const;

		/** Values the battles' scripts keep from one battle to the next. */
		std::array<uint16, BattleGlobalCount>& BattleGlobals() { return Globals; }
		const std::array<uint16, BattleGlobalCount>& BattleGlobals() const { return Globals; }

		/** The play time the menus show and the saves keep, in vertical blanks counted as sixtieths of a second. */
		uint32 PlayTime() const { return PlayVblanks; }
		void SetPlayTime(uint32 Vblanks) { PlayVblanks = Vblanks; }

		/** How the game tells the second disc's story: none chosen in a new game, the player's choice once asked. */
		EStoryMode StoryMode() const { return Mode; }
		void SetStoryMode(EStoryMode InMode) { Mode = InMode; }
		/** The Disc 2 expansion the game plays where it tells the second disc expanded, by its mod's folder (ExpansionMods.h). */
		const std::string& Expansion() const { return ExpansionFolder; }
		void SetExpansion(std::string Folder) { ExpansionFolder = std::move(Folder); }

		/** The order the player keeps the party in (FPartyOrder): none in a new game, the story's order shown where none is for the party. */
		const FPartyOrder& PartyOrder() const { return Order; }
		void SetPartyOrder(const FPartyOrder& InOrder) { Order = InOrder; }

		/** How the player has the second Citan and the grown Emeralda drawn (FCharacterLooks): as the story has them in a new game. */
		const FCharacterLooks& CharacterLooks() const { return Looks; }
		void SetCharacterLooks(const FCharacterLooks& InLooks) { Looks = InLooks; }

		/** What the game is after the ending (EClearGame): none in a new game. */
		EClearGame ClearGame() const { return Clear; }
		void SetClearGame(EClearGame Kind) { Clear = Kind; }
		/** Whether the game comes after the ending: the clear file's or a New Game+, Fei's and Elly's looks unlocked. */
		bool IsClearGame() const { return Clear != EClearGame::None; }

		/**
		 * What the last program to end left the next, kept by the console but never saved: its result
		 * (1 after a battle lost, 2 after a game loaded), and the party's turns in the last battle that
		 * did more than guard or charge (an escape counts).
		 */
		uint8 ProgramResult = 0;
		uint16 BattleActingTurns = 0;
		/** The Normal Menu's command the cursor starts on (6 Status for a new game), which the menu keeps where saving is not allowed. */
		uint8 MenuLastCommand = 0;

		/** A save state's writer and reader go through every member (SaveStateMembers.h). */
		template <typename FArchive>
		friend void VisitMembers(FArchive& Ar, FGameState& Value);

	private:
		std::array<uint8, BlockSize> Bytes{};
		std::array<uint16, BattleGlobalCount> Globals{};
		uint32 PlayVblanks = 0;
		EStoryMode Mode = EStoryMode::Unchosen;
		std::string ExpansionFolder;
		FPartyOrder Order;
		FCharacterLooks Looks;
		EClearGame Clear = EClearGame::None;
	};

	/** The new game's state from the disc, loaded as the kernel loads it when New Game is chosen. */
	WELTALLCORE_API TResult<FGameState> LoadNewGameState(const FCdImage& Image, const FGameFileIndex& Index, const FMesNameTables& NameTables);
}
