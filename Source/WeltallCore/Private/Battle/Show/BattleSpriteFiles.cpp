#include "Battle/Show/BattleSpriteFiles.h"

#include "Foundation/LittleEndian.h"
#include "Game/EllysHair.h"
#include "Media/GameFileIndex.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		constexpr uint32 OwnSections = 3;
		constexpr uint32 BankMagic = 0x20736477;
		constexpr uint32 EffectsMagic = 0x73646573;

		/** The setup program and where its table of the characters' two files lies in it. */
		constexpr int32 SetupDirectory = 12;
		constexpr int32 StartSoundsFile = 2;
		constexpr int32 SetupFile = 4;
		constexpr std::size_t CharacterFilesOffset = 0x55BC;
		constexpr std::size_t TextureWidthsOffset = 0x562C;
		constexpr std::size_t FirstColumnOffset = 0x5638;

		TResult<std::vector<uint8>> ReadWhole(const FCdImage& Image, const FGameFileIndex& Index, int32 Directory, int32 Number, bool bSectors = false)
		{
			const FIndexRecord* Record = Index.Find(Directory, Number);
			if (Record == nullptr || !Record->IsFile())
			{
				return MakeError("the disc lacks file " + std::to_string(Number) + " of directory " + std::to_string(Directory));
			}
			return bSectors ? ReadFileSectors(Image, *Record) : ReadFileBytes(Image, *Record);
		}

		TResult<FBattleSpriteFile> ParseFile(TResult<std::vector<uint8>> Bytes, const std::string& What)
		{
			if (!Bytes)
			{
				return Bytes.Error();
			}
			TResult<FBattleSpriteFile> File = FBattleSpriteFile::Parse(*Bytes);
			if (!File)
			{
				return MakeError(What + ": " + File.Error().Message);
			}
			return File;
		}
	}

	TResult<FBattleSpriteFile> FBattleSpriteFile::Parse(std::span<const uint8> Bytes)
	{
		TResult<FSpriteFile> Sprite = FSpriteFile::ParseWithSections(Bytes);
		if (!Sprite)
		{
			return Sprite.Error();
		}
		FBattleSpriteFile File;
		File.Main = std::move(*Sprite);
		const uint32 Count = ReadU32(Bytes, 0);
		if (Bytes.size() < 4 + 4 * std::size_t{Count} + 4)
		{
			return MakeError("a battle sprite file's sections run past it");
		}
		for (uint32 Section = OwnSections; Section < Count; ++Section)
		{
			const uint32 Offset = ReadU32(Bytes, 4 + 4 * std::size_t{Section});
			const uint32 End = ReadU32(Bytes, 4 + 4 * (std::size_t{Section} + 1));
			if (Offset + 4 > Bytes.size() || End < Offset || End > Bytes.size())
			{
				return MakeError("a battle sprite file's section " + std::to_string(Section) + " lies past it");
			}
			FBattleSpriteSection Entry{EBattleSpriteSectionKind::SpriteResource, Offset, End - Offset};
			const uint32 Magic = ReadU32(Bytes, Offset);
			std::optional<FSpriteFile> SectionSprite;
			if (Magic == BankMagic)
			{
				Entry.Kind = EBattleSpriteSectionKind::SoundBank;
			}
			else if (Magic == EffectsMagic)
			{
				Entry.Kind = EBattleSpriteSectionKind::SoundEffects;
			}
			else
			{
				TResult<FSpriteFile> Resource = FSpriteFile::Parse(Bytes.subspan(Offset, End - Offset));
				if (!Resource)
				{
					return MakeError("a battle sprite file's section " + std::to_string(Section) + ": " + Resource.Error().Message);
				}
				SectionSprite = std::move(*Resource);
			}
			File.FurtherSections.push_back(Entry);
			File.SectionSprites.push_back(std::move(SectionSprite));
		}
		return File;
	}

	TResult<FBattleSpriteFiles> FBattleSpriteFiles::Load(const FCdImage& Image, const FGameFileIndex& Index)
	{
		TResult<std::vector<uint8>> Setup = ReadWhole(Image, Index, SetupDirectory, SetupFile);
		if (!Setup)
		{
			return Setup.Error();
		}
		if (Setup->size() < FirstColumnOffset + 2)
		{
			return MakeError("the battle's setup program is too short for its tables of the characters' files and textures");
		}
		FBattleSpriteFiles Files;
		Files.Image = &Image;
		Files.Index = &Index;
		Files.SetVoiceFiles(Image, Index);
		for (std::size_t Character = 0; Character < CharacterCount; ++Character)
		{
			Files.Characters[Character] = {ReadU32(*Setup, CharacterFilesOffset + 8 * Character), ReadU32(*Setup, CharacterFilesOffset + 8 * Character + 4)};
		}
		for (std::size_t Character = 0; Character < WidthCount; ++Character)
		{
			Files.Widths[Character] = (*Setup)[TextureWidthsOffset + Character];
		}
		Files.FirstColumn = ReadU16(*Setup, FirstColumnOffset);
		const FIndexRecord* Group = Index.Find(EffectDirectory, 1);
		if (Group == nullptr || Group->Size >= 0 || -Group->Size < EffectFirstFile)
		{
			return MakeError("the disc has no battle effects");
		}
		Files.Effects = (-Group->Size - EffectFirstFile + 2) / 2;
		return Files;
	}

	TResult<FBattleSpriteFile> FBattleSpriteFiles::ReadCharacterSprite(int32 Character) const
	{
		return ParseFile(ReadWhole(*VoiceImage, *VoiceIndex, PartyDirectory, static_cast<int32>(Characters[static_cast<std::size_t>(Character)].Sprite)),
			"character " + std::to_string(Character) + "'s battle sprite");
	}

	void FBattleSpriteFile::RecolourPurpleHair()
	{
		EllysHair::RecolourSpriteFile(Main);
		for (std::optional<FSpriteFile>& Section : SectionSprites)
		{
			if (Section)
			{
				EllysHair::RecolourSpriteFile(*Section);
			}
		}
	}

	TResult<FBattleSpriteFile> FBattleSpriteFiles::ReadAttacks(int32 Character) const
	{
		TResult<FBattleSpriteFile> File = ParseFile(ReadWhole(*VoiceImage, *VoiceIndex, PartyDirectory,
			static_cast<int32>(Characters[static_cast<std::size_t>(Character)].Attacks)), "character " + std::to_string(Character) + "'s attacks");
		if (File)
		{
			File->bVoices = true;
		}
		return File;
	}

	TResult<FBattleSpriteFile> FBattleSpriteFiles::ReadEffect(int32 Effect) const
	{
		if (Effect < 0 || Effect >= Effects)
		{
			return MakeError("the disc has no battle effect " + std::to_string(Effect));
		}
		return ParseFile(ReadWhole(*Image, *Index, EffectDirectory, EffectFirstFile + 2 * Effect), "effect " + std::to_string(Effect));
	}

	bool FBattleSpriteFiles::IsEffectStandIn(int32 Effect) const
	{
		constexpr std::string_view StandIn = "It's CDMAKE Dummy !";
		if (Effect < 0 || Effect >= Effects)
		{
			return false;
		}
		const TResult<std::vector<uint8>> Bytes = ReadWhole(*Image, *Index, EffectDirectory, EffectFirstFile + 2 * Effect);
		return Bytes && Bytes->size() >= StandIn.size() && std::equal(StandIn.begin(), StandIn.end(), Bytes->begin());
	}

	TResult<std::vector<uint8>> FBattleSpriteFiles::ReadEffectStream(int32 Effect) const
	{
		if (Effect < 0 || Effect >= Effects)
		{
			return MakeError("the disc has no battle effect " + std::to_string(Effect));
		}
		const int32 Number = EffectFirstFile + 1 + 2 * Effect;
		// The effect's load streams a file only when its size, rounded up to a word, is past 10h bytes.
		const FIndexRecord* Record = Index->Find(EffectDirectory, Number);
		if (Record != nullptr && Record->IsFile() && ((static_cast<uint32>(Record->Size) + 3) & ~3u) <= 0x10)
		{
			return std::vector<uint8>{};
		}
		return ReadWhole(*Image, *Index, EffectDirectory, Number, true);
	}

	TResult<FBattleSpriteFile> FBattleSpriteFiles::ReadCommon() const
	{
		return ParseFile(ReadWhole(*Image, *Index, CommonDirectory, CommonFile), "the common effects");
	}

	TResult<std::vector<uint8>> FBattleSpriteFiles::ReadCommonImages() const
	{
		return ReadWhole(*Image, *Index, CommonDirectory, CommonImagesFile);
	}

	TResult<std::vector<uint8>> FBattleSpriteFiles::ReadCommonSounds() const
	{
		return ReadWhole(*Image, *Index, CommonDirectory, CommonSoundsFile);
	}

	TResult<std::vector<uint8>> FBattleSpriteFiles::ReadStartSounds() const
	{
		return ReadWhole(*Image, *Index, SetupDirectory, StartSoundsFile);
	}

	TResult<std::vector<uint8>> FBattleSpriteFiles::ReadSystemSounds() const
	{
		return ReadWhole(*Image, *Index, CommonDirectory, SystemBankFile);
	}

	TResult<std::vector<uint8>> FBattleSpriteFiles::ReadEnemySounds() const
	{
		return ReadWhole(*VoiceImage, *VoiceIndex, CommonDirectory, EnemyBankFile);
	}
}
