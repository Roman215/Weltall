#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/SpriteFile.h"

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/** What a section past a Battle sprite file's own three holds, told apart by its magic as the program does. */
	enum class EBattleSpriteSectionKind : uint8
	{
		/** `wds `: a sound bank, loaded into sound RAM in place of the battle's system bank. */
		SoundBank,
		/** `seds`: sound effects, registered with the sound driver. */
		SoundEffects,
		/** Anything else: a sprite resource, which the effect's sprites take as their alternate file. */
		SpriteResource,
	};

	struct FBattleSpriteSection
	{
		EBattleSpriteSectionKind Kind = EBattleSpriteSectionKind::SpriteResource;
		uint32 Offset = 0;
		uint32 Size = 0;
	};

	/**
	 * One of Battle's sprite files (a combatant's, an attack's, an effect's, the common one, a scene's
	 * sprite enemy): a sprite file whose header counts sections past its three, each a sound bank, sound
	 * effects or a further sprite resource.
	 */
	class FBattleSpriteFile
	{
	public:
		WELTALLCORE_API static TResult<FBattleSpriteFile> Parse(std::span<const uint8> Bytes);

		const FSpriteFile& Sprite() const { return Main; }
		/** The file in memory, which the scripts may write their variables into. */
		FSpriteFile& Sprite() { return Main; }
		const std::vector<FBattleSpriteSection>& Sections() const { return FurtherSections; }
		/** A sprite resource section's sprite, parsed as the program's installer parses it (in memory, which its scripts may write). */
		const FSpriteFile* SectionSprite(std::size_t Index) const
		{
			return SectionSprites[Index] ? &*SectionSprites[Index] : nullptr;
		}
		FSpriteFile* SectionSprite(std::size_t Index) { return SectionSprites[Index] ? &*SectionSprites[Index] : nullptr; }
		/** The animation table's first word: bits 6-11 the animation extension it needs, bits 12-13 the gears it drops. */
		uint16 AnimationWord() const { return Main.Word(Main.Tables().Animations); }
		int32 Extension() const { return AnimationWord() >> 6 & 0x3F; }
		int32 DropsGears() const { return AnimationWord() >> 12 & 3; }
		/** Its sound bank is a character's recorded voice: a party attack file's, which the voice track's disc gives. */
		bool HoldsVoices() const { return bVoices; }

		/** Its sprites' palettes recoloured with Elly's purple hair (EllysHair). */
		WELTALLCORE_API void RecolourPurpleHair();

	private:
		friend class FBattleSpriteFiles;

		FSpriteFile Main;
		bool bVoices = false;
		std::vector<FBattleSpriteSection> FurtherSections;
		std::vector<std::optional<FSpriteFile>> SectionSprites;
	};

	/** A party character's two battle sprite files in directory 45: its sprite and its attack animations. */
	struct FBattleCharacterFiles
	{
		uint32 Sprite = 0;
		uint32 Attacks = 0;
	};

	/**
	 * Battle's sprite files on the disc: the party's (directory 45, by the setup program's table per
	 * character), the effects (directory 14, effect e files 2e + 22h and its VRAM stream 2e + 23h) and
	 * the common effects (directory 44 file 2).
	 */
	class FBattleSpriteFiles
	{
	public:
		static constexpr int32 CharacterCount = 11;
		static constexpr int32 PartyDirectory = 45;
		static constexpr int32 EffectDirectory = 14;
		static constexpr int32 EffectFirstFile = 0x22;
		static constexpr int32 CommonDirectory = 44;
		static constexpr int32 CommonImagesFile = 1;
		static constexpr int32 CommonFile = 2;
		static constexpr int32 CommonSoundsFile = 3;
		/** The battle's own sound bank (a `wds`, which a 3D performer's playback loads) and the enemies' (a sprite enemy's). */
		static constexpr int32 SystemBankFile = 5;
		static constexpr int32 EnemyBankFile = 7;
		/** The characters the setup program's table of texture widths covers. */
		static constexpr int32 WidthCount = 12;

		/** Reads the setup program's table of the characters' files (directory 12 file 4). */
		WELTALLCORE_API static TResult<FBattleSpriteFiles> Load(const FCdImage& Image, const FGameFileIndex& Index);
		/** Selects compatible USA or undub files for the party's voices and the enemies' bank. */
		void SetVoiceFiles(const FCdImage& InImage, const FGameFileIndex& InIndex) { VoiceImage = &InImage; VoiceIndex = &InIndex; }

		const FBattleCharacterFiles& Character(int32 Character) const { return Characters[static_cast<std::size_t>(Character)]; }
		/** How many VRAM columns a character's battle sprite takes, by the setup program's table: its members stand side by side from 100h. */
		uint8 TextureWidth(int32 Character) const { return Widths[static_cast<std::size_t>(Character)]; }
		/** Where the setup program starts placing the party's textures, past 100h (its own data). */
		uint16 FirstTextureColumn() const { return FirstColumn; }
		/** How many effects the disc holds (files 22h on of directory 14, two each). */
		int32 EffectCount() const { return Effects; }

		WELTALLCORE_API TResult<FBattleSpriteFile> ReadCharacterSprite(int32 Character) const;
		WELTALLCORE_API TResult<FBattleSpriteFile> ReadAttacks(int32 Character) const;
		WELTALLCORE_API TResult<FBattleSpriteFile> ReadEffect(int32 Effect) const;
		/** Whether an effect's file is one of the build tool's stand-ins ("It's CDMAKE Dummy !"), which nothing plays. */
		WELTALLCORE_API bool IsEffectStandIn(int32 Effect) const;
		/** An effect's VRAM stream (images and palettes), whole sectors as the drive streams them; empty for none. */
		WELTALLCORE_API TResult<std::vector<uint8>> ReadEffectStream(int32 Effect) const;
		WELTALLCORE_API TResult<FBattleSpriteFile> ReadCommon() const;
		/** The effects' images (an image list), which the setup uploads and effects reload. */
		WELTALLCORE_API TResult<std::vector<uint8>> ReadCommonImages() const;
		/** The battle's common sound effects (a `seds` file). */
		WELTALLCORE_API TResult<std::vector<uint8>> ReadCommonSounds() const;
		/** The battle kinds' start sounds (a `seds` file, directory 12 file 2). */
		WELTALLCORE_API TResult<std::vector<uint8>> ReadStartSounds() const;
		/** The sound banks 44/5 and 44/7 (`wds`). */
		WELTALLCORE_API TResult<std::vector<uint8>> ReadSystemSounds() const;
		WELTALLCORE_API TResult<std::vector<uint8>> ReadEnemySounds() const;

	private:
		const FCdImage* Image = nullptr;
		const FGameFileIndex* Index = nullptr;
		const FCdImage* VoiceImage = nullptr;
		const FGameFileIndex* VoiceIndex = nullptr;
		std::array<FBattleCharacterFiles, CharacterCount> Characters{};
		std::array<uint8, WidthCount> Widths{};
		uint16 FirstColumn = 0;
		int32 Effects = 0;
	};
}
