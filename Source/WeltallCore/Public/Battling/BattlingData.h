#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "Graphics/MdlModel.h"
#include "Media/DiscIdentification.h"
#include "Text/MessageTable.h"

#include <array>
#include <string>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FPsxExecutable;

	namespace BattlingGears
	{
		/** The roster's gears, numbered as the gear table and the gear files are. */
		constexpr int32 Count = 49;
		/** The gears the menus' demonstration takes the place of some with, and the one the arena unlocks last. */
		constexpr uint8 Weltall = 0;
		constexpr uint8 Vierge = 1;
		constexpr uint8 Argento = 22;
		/** The chain states: 0 before any attack, then A, B, AA, AB, BA, BB, AAA ... BBB. */
		constexpr int32 ChainStates = 15;
		constexpr int32 Moves = ChainStates - 1;

		/** A gear file's HUD pictures: the icon's palette, the icon's 22 rows of 22 bytes, the name's 8 rows of 32. */
		constexpr std::size_t HudPaletteSize = 0x200;
		constexpr int32 IconSize = 22;
		constexpr std::size_t HudNameOffset = 0x3E4;
		constexpr std::size_t HudPicturesSize = HudNameOffset + 0x100;
	}

	/** How an animation ends (the program's table, one entry per animation number). */
	struct FBattlingAnimationKind
	{
		/** 0 plays once, 1 loops, 2 holds its last frame until released, 3 plays once with no follow-on. */
		uint8 Kind = 0;
		/** The animation that follows it, or FFh for none. */
		uint8 Next = 0xFF;
	};

	/**
	 * What Battling's labels and menus print: the program's own strings, read where its code and its tables point (a
	 * translation's words, a mod's), the game's when nothing was read. The formats are the program's printf strings
	 * (FormatBattlingText).
	 */
	struct FBattlingTexts
	{
		static constexpr int32 MenuCount = 8;
		static constexpr int32 PracticeStyleCount = 14;

		/** The winner screen's headings, its score's format (wins, rounds, the opponent's name) and its time's. */
		std::string Winner = "WINNER";
		std::string Level = "LEVEL";
		std::string Matches = "MATCHES";
		std::string Time = "TIME";
		std::string ScoreFormat = "%d/%d VS %s";
		std::string TimeFormat = "%02d'%02d''%02d";
		/** A round's start and end. */
		std::string Ready = "READY";
		std::string Fight = "FIGHT!!";
		std::string Practice = "PRACTICE";
		std::string Start = "START";
		std::string RoundFormat = "ROUND %d";
		std::string RubberBandBattle = "RUBBER BAND BATTLE";
		std::string RubberBandMode = "RUBBER BAND MODE";
		std::string KnockOut = "KNOCK OUT!!";
		std::string DrawGame = "DRAW GAME";
		std::string Replay = "REPLAY";
		/** The set's end, by who won; the defeat's second line names the winner. */
		std::string YouWereVictorious = "YOU WERE VICTORIOUS";
		std::string YouWereDefeated = "YOU WERE DEFEATED";
		std::string ByFormat = "     BY %s";
		std::string PlayerOneVictory = "1PLAYER VICTORY";
		std::string PlayerTwoVictory = "2PLAYER VICTORY";
		std::string ComputerOneVictory = "COM1 VICTORY";
		std::string ComputerTwoVictory = "COM2 VICTORY";
		/** Practice's info: the move's name by chain state (none before any), its strength's format and its frames'. */
		std::array<std::string, BattlingGears::ChainStates> MoveNames{
			"", "A", "B", "AA", "AB", "BA", "BB", "AAA", "AAB", "ABA", "ABB", "BAA", "BAB", "BBA", "BBB"};
		std::string StrengthFormat = "STR:%d";
		std::string FramesFormat = "FRAME:%d-%d";
		/** The menus' values: the computer's levels, practice's styles and frame rate, the matches' no-limit (the font's infinity), on and off. */
		std::array<std::string, 3> LevelNames{"EASY", "NORMAL", "HARD"};
		std::array<std::string, PracticeStyleCount> PracticeStyles{"BYSTANDER", "ON GUARD", "CONTROLLER2", "UP AND AT'EM", "SLOWPOKE",
			"MAGIC FIRER ", "MAGIC JUMPER", "KANGAROO", "GIVE CHASE", "RUN AWAY", "BACK DASH", "EASY BATTLE", "NORMAL BATTLE", "HARD BATTLE"};
		std::string FrameRateFormat = "%dFPS";
		std::string NoLimit = "#";
		std::string On = "ON";
		std::string Off = "OFF";
		std::string VibrationOn = "VIBRATION ON";
		std::string VibrationOff = "VIBRATION OFF";
		/** Between the two gears' names as they are chosen. */
		std::string Versus = "VS";
		/** Each menu's items' labels, by the menu's number. */
		std::array<std::vector<std::string>, MenuCount> MenuLabels{{
			{"CONTINUE BOUT", "GIVE UP"},
			{"GAME LEVEL", "MOTION SPEED", "FRAME RATE", "GEAR 1", "GEAR 2"},
			{"YES", "NO"},
			{"BONUS BATTLING", "", "PRACTICE", "TUTORIAL", "EXIT"},
			{"PLAYER1 VS COM", "PLAYER1 VS PLAYER2", "COM VS COM", "", "NUM OF MATCHES", "COM LEVEL", "RUBBER BAND"},
			{"", ""},
			{"", ""},
			{"RETURN TO PRACTICE", "AI", "FRAME RATE", "EXIT PRACTICE MODE"},
		}};

		bool operator==(const FBattlingTexts&) const = default;
	};

	/** A letter of Battling's label font: its texels' corner in the font's page and its size. */
	struct FBattlingGlyph
	{
		uint8 U = 0;
		uint8 V = 0;
		uint8 Width = 0;
		uint8 Height = 0;
	};

	struct FBattlingProjectileKind
	{
		/** How strongly it turns toward its target each tick (4096 = at once). */
		int16 Homing = 0;
		/** How it moves and what it leaves: 0 two half steps with sprites, 1 a sprite, 2 a line, 3 a trail, 4 and 5 lines of random colours. */
		uint8 Look = 0;
		uint8 Life = 0;
		uint8 Speed = 0;
		/** 0 none, 1 speeds up to 70h, 2 homes less as it ages, 3 stops homing after its first tick. */
		uint8 SpeedMode = 0;
		/** The gear's sound index it plays as it starts, 0 none. */
		uint8 Sound = 0;
	};

	/** A gear's sound set: for each sound index (1-6 hits, 7 walking ... 15 a guarded hit) an entry of the pair table, FFh none. */
	using FBattlingSoundSet = std::array<uint8, 16>;

	/** Battling's own tables, read from its program on the disc. */
	struct FBattlingProgramTables
	{
		/** The kinds table runs to the chain table: 36 entries, the moves' animations reaching 32. */
		static constexpr int32 AnimationCount = 36;
		static constexpr int32 ProjectileKindCount = 7;
		static constexpr int32 SoundPairCount = 0x50;
		static constexpr int32 TrailKindCount = 3;

		std::array<FBattlingAnimationKind, AnimationCount> AnimationKinds{};
		/** For each chain state, the state an attack A or B moves it to. */
		std::array<std::array<uint8, 2>, BattlingGears::ChainStates> Chain{};
		std::array<FBattlingProjectileKind, ProjectileKindCount> ProjectileKinds{};
		/** Up to two sound effects (of the program's effect file) per entry, 0 none. */
		std::array<std::array<uint8, 2>, SoundPairCount> SoundPairs{};
		/** Every gear's set but the four with their own. */
		FBattlingSoundSet DefaultSoundSet{};
		FBattlingSoundSet RankarSoundSet{};
		FBattlingSoundSet DeathSoundSet{};
		FBattlingSoundSet WyrmSoundSet{};
		FBattlingSoundSet ChuChuSoundSet{};
		/** The attack inputs that make each chain state's move (up to three, 0 none). */
		std::array<std::array<uint8, 3>, BattlingGears::ChainStates> ChainInputs{};
		/** How many segments a trail of each kind keeps. */
		std::array<int16, TrailKindCount> TrailLengths{};
		/** The kernel's stick values for the d-pad's buttons (bits 12-15: up, right, down, left), as a digital pad's stick. */
		std::array<uint8, 16> DpadX{};
		std::array<uint8, 16> DpadY{};
		/** The kernel's arctangents, which ratan2 reads. */
		FGteArctangents Arctangents;
		/** El-Fenrir's extra part, which the program keeps in its own data and hangs on the gear in place of one of its own. */
		FMdlPart ElFenrirPart;

		static constexpr int32 LessonCount = 10;
		/**
		 * The tutorial's script machine's programs: the bytes of the program's script area, where each lesson starts in them
		 * (0 the lesson menu's prompt, 1-8 the menu's lessons, 9 the welcome), and where the scripted bout's ending starts.
		 */
		std::vector<uint8> Scripts;
		std::array<uint16, LessonCount> Lessons{};
		uint16 EndingScript = 0;
		/** The eight directions a gear settling on low ground steps against (x and z, -1 to 1). */
		std::array<std::array<int8, 2>, 8> SettleSteps{};
		/** The label font's letters: 0-9, A-Z, then ! : - / # space and the apostrophe. */
		static constexpr int32 GlyphCount = 43;
		std::array<FBattlingGlyph, GlyphCount> FontGlyphs{};
		/** Each gear's name as the labels print it, by the gear's number. */
		std::array<std::string, BattlingGears::Count> GearNames;
		FBattlingTexts Texts;

		WELTALLCORE_API const FBattlingSoundSet& SoundSetOf(int32 Gear) const;

		WELTALLCORE_API static TResult<FBattlingProgramTables> Read(const FCdImage& Image, const FGameFileIndex& Index, const FPsxExecutable& Executable,
			EDiscRelease Release);
	};

	/**
	 * A gear's row of the gear table: the moves' strengths and the computer's traits. Perfect Works Build's arena gives each
	 * gear its own numbers in bytes the game leaves 0 or never reads (the second word, 1Bh on), which only its rules read.
	 */
	struct FBattlingGearStats
	{
		/** A percentage every move's strength is scaled by (100 for every gear); the arena's: the gear's speed scale for its running. */
		int16 Power = 0;
		/** Never read by the game (320); the arena's: the gear's HP. */
		int16 ArenaHp = 0;
		/** The story value from which the arena's menus offer the gear. */
		int16 Threshold = 0;
		/** The computer's traits A-D (80h for every gear). */
		std::array<uint8, 4> Traits{};
		/** The moves A ... BBB (0 for a move the gear lacks). */
		std::array<uint8, BattlingGears::Moves> Strengths{};
		/** The Ether Bullet's strength, 0 for a gear without one. */
		uint8 Ether = 0;
		/** The arena's: a dash tick's heat, the stick's acceleration and deceleration, the Ether Bullet's heat / 8. */
		uint8 ArenaDashHeat = 0;
		uint8 ArenaAcceleration = 0;
		uint8 ArenaDeceleration = 0;
		uint8 ArenaEtherHeat = 0;
		/** The arena's: the knock-down stun at which the gear's hits knock down. */
		uint8 ArenaKnockDown = 0;

		/**
		 * The strength the program reads for a chain state: the byte 9 + State of the row, so state 0 reads trait D,
		 * and the move of state n is Strengths[n - 1].
		 */
		uint8 StrengthOfState(int32 State) const { return State == 0 ? Traits[3] : Strengths[State - 1]; }
	};

	struct FBattlingGearTable
	{
		std::array<FBattlingGearStats, BattlingGears::Count> Gears{};

		/** Entry 2 of the program's archive in directory 48. */
		WELTALLCORE_API static TResult<FBattlingGearTable> Read(const FCdImage& Image, const FGameFileIndex& Index);
	};

	/** Battling's messages (entry 1 of the program's archive in directory 48): the menus' help, the tutorial's and the ending's text. */
	class FBattlingMessages
	{
	public:
		FBattlingMessages() = default;
		FBattlingMessages(const FBattlingMessages&) = delete;
		FBattlingMessages& operator=(const FBattlingMessages&) = delete;
		FBattlingMessages(FBattlingMessages&&) = default;
		FBattlingMessages& operator=(FBattlingMessages&&) = default;

		WELTALLCORE_API static TResult<FBattlingMessages> Read(const FCdImage& Image, const FGameFileIndex& Index);

		const FMessageTable& Table() const { return Messages; }

	private:
		/** The archive the table views, or the table with the player's messages in place (Assets/text). */
		std::vector<uint8> Archive;
		std::vector<uint8> Replaced;
		FMessageTable Messages;
	};

	/** The arena's ground types, bits 24-25 of a cell. */
	enum class EBattlingGround : uint8
	{
		Hard,
		Water,
		Normal,
		Sand,
	};

	/** The ground's height under a point, and what finding it leaves behind on the stack (see FBattlingTerrain::Sample). */
	struct FBattlingTerrainSample
	{
		int32 Height = 0;
		/** The y of the triangle's normal, unnormalized: the outer product's y over the root of its largest component. */
		int32 ScaledNormalY = 0;
		/** The triangle's normal, which the program keeps for whoever asks for the ground's slope next. */
		FGteVector Normal{};
	};

	/**
	 * The arena's heightfield: 128 x 128 cells of 256 units, row-major by z; bits 0-15 the height (y grows down), 16-23 the
	 * texture, 24-25 the ground's type. The heights are kept as the program keeps them once loaded: the file's times 12, in 16
	 * bits.
	 */
	class FBattlingTerrain
	{
	public:
		static constexpr int32 Size = 128;
		static constexpr int32 CellUnits = 256;

		WELTALLCORE_API static TResult<FBattlingTerrain> Read(const FCdImage& Image, const FGameFileIndex& Index);
		WELTALLCORE_API static FBattlingTerrain FromFile(std::span<const uint8> Unpacked);

		/** The cell under (X, Z), which must lie inside the heightfield. */
		uint32 Cell(int32 X, int32 Z) const { return Cells[static_cast<std::size_t>((Z >> 8) * Size + (X >> 8))]; }
		uint32 CellAt(int32 Index) const { return Cells[static_cast<std::size_t>(Index)]; }

		/** Whether the four corners the ground's height at (X, Z) is made from lie inside the heightfield. */
		WELTALLCORE_API static bool Covers(int32 X, int32 Z);

		/**
		 * The height of the ground's plane under (X, Z) for a body at height Y, as the program finds it: the cell split along its
		 * diagonal, the triangle's normal from the kernel's routine and the plane solved from Y. With the bed, water corners lie
		 * 100h deeper and sand ones 40h. (X, Z) must be Covered.
		 */
		int32 Height(int32 X, int32 Y, int32 Z, bool bWithBed) const { return Sample(X, Y, Z, bWithBed).Height; }

		/** The height, and the scaled normal the kernel's triangle routine keeps in its stack frame as it finds it. */
		WELTALLCORE_API FBattlingTerrainSample Sample(int32 X, int32 Y, int32 Z, bool bWithBed) const;

	private:
		std::vector<uint32> Cells;
	};

	/** A joint of a gear's hierarchy as its file gives it. */
	struct FBattlingJointRecord
	{
		/** -1 for a joint on the model's root. */
		int16 Parent = -1;
		/** Its part of the model, -1 for none. */
		int16 Part = -1;
		std::array<int16, 3> Angles{};
		std::array<int16, 3> Translation{};
	};

	/** What an animation channel drives: a joint's rotation about x, y or z, or its translation along one. */
	enum class EBattlingChannelTarget : uint8
	{
		AngleX,
		AngleY,
		AngleZ,
		TranslationX,
		TranslationY,
		TranslationZ,
	};

	struct FBattlingChannel
	{
		uint8 Joint = 0;
		EBattlingChannelTarget Target = EBattlingChannelTarget::AngleX;
		/** A static channel's value, or a keyed channel's stream as an offset into the gear file. */
		uint32 Value = 0;

		bool IsAngle() const { return Target <= EBattlingChannelTarget::AngleZ; }
	};

	struct FBattlingAnimation
	{
		int16 Frames = 0;
		std::vector<FBattlingChannel> StaticChannels;
		std::vector<FBattlingChannel> KeyedChannels;
	};

	/** An animation event's kind, its first byte. */
	enum class EBattlingEventKind : uint8
	{
		Attack,
		Sounds,
		Effect,
		ReturnToBase,
		HidePart,
		ShowPart,
	};

	/** An event of an animation's frames: { first byte the kind, then kind-dependent bytes }. */
	struct FBattlingEvent
	{
		EBattlingEventKind Kind = EBattlingEventKind::Attack;
		/** Attack and effect: what it makes; hide and show: the joint. */
		uint8 What = 0;
		/** Attack and effect: the two joints; sounds: the two effects. */
		uint8 JointA = 0;
		uint8 JointB = 0;
		/** Attack and effect: a vertex of each joint's part (1-based; 0 the joint itself). */
		int16 VertexA = 0;
		int16 VertexB = 0;
		/** Where it lies in the gear file, which tells events apart where the program compares their addresses. */
		uint32 Offset = 0;
	};

	/** An entry of an animation's event list: the event runs on its frames First to Last. */
	struct FBattlingEventSpan
	{
		uint8 First = 0;
		uint8 Last = 0;
		/** Into the gear file's events. */
		uint16 Event = 0;
	};

	/** A move of the move set: the animation it plays (12h + Animation, 0 for none), and its lunge, trail and "from afar" flags. */
	struct FBattlingMove
	{
		uint8 Animation = 0;
		uint8 Lunge = 0;
		uint8 Trail = 0;
		uint8 FromAfar = 0;
	};

	/** A gear file (49/(gear + 2)) unpacked: the model, its hierarchy and animations, the moves and the gear's constants. */
	struct FBattlingGearFile
	{
		std::vector<uint8> Bytes;
		std::vector<FBattlingJointRecord> Joints;
		/** The kernel's MDL model, as an offset into Bytes. */
		uint32 Model = 0;
		/** Null entries for the animations the file leaves out. */
		std::vector<FBattlingAnimation> Animations;
		std::vector<bool> HasAnimation;
		/** Per animation: the blend length and the speed. */
		std::vector<std::array<uint8, 2>> AnimationTable;
		std::array<FBattlingMove, BattlingGears::ChainStates> Moves{};
		/** Per animation: its event list, and whether it has one (a list can be empty). */
		std::vector<std::vector<FBattlingEventSpan>> EventLists;
		std::vector<bool> HasEventList;
		std::vector<FBattlingEvent> Events;
		/** The data block's offset into Bytes. */
		uint32 Data = 0;
		/** The gear's textures: an image list's offset into Bytes, 0 for none. */
		uint32 Textures = 0;
		/** The HUD's pictures of the gear, as an offset into Bytes: its icon's 256 colours, the icon (22 x 22, 8-bit), its name (64 x 8, 4-bit). */
		uint32 HudPictures = 0;

		uint8 TopJoint = 0;
		std::array<uint8, 2> FootJoints{};
		uint8 PushRadius = 0;
		std::array<int16, 2> FootVertices{};
		int16 TopVertex = 0;
		int16 SpeedScale = 0;
		uint8 MoveScale = 0;
		std::array<uint8, 3> Colour{};
		uint8 BodyRadius = 0;
		int16 ModelScale = 0;
		/** The winner screen's view: the pitch, the spin's speed, the camera's height and distance, the model's scale and yaw. */
		int16 WinnerPitch = 0;
		int16 WinnerSpin = 0;
		int16 WinnerCameraHeight = 0;
		int16 WinnerCameraDistance = 0;
		int16 WinnerScale = 0;
		int16 WinnerYaw = 0;
		/** The result screen's camera: its angle from the gear's facing, the eye's and the target's heights, the distance. */
		int16 ResultAngle = 0;
		int16 ResultEyeHeight = 0;
		int16 ResultTargetHeight = 0;
		int16 ResultDistance = 0;
		/** The victory animation loops (else it holds its last frame). */
		bool bVictoryLoops = false;
		/** The joints whose parts every animation starts with hidden. */
		std::vector<uint8> HiddenJoints;

		WELTALLCORE_API static TResult<FBattlingGearFile> Read(const FCdImage& Image, const FGameFileIndex& Index, int32 Gear);
		WELTALLCORE_API static TResult<FBattlingGearFile> Parse(std::vector<uint8> Unpacked);
	};
}
