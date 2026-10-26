#include "Battling/BattlingData.h"

#include "BattlingFiles.h"
#include "Formats/Lzss.h"
#include "Foundation/LittleEndian.h"
#include "Foundation/ProcessorDivide.h"
#include "Graphics/GteVectors.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Text/MessageOverrides.h"
#include "Text/MessageTableIds.h"

#include <algorithm>
#include <optional>

#include <string>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		/** Where a release's Battling program (directory 1, unpacked) keeps each table, as offsets in it; the kernel's d-pad tables by address in the executable. */
		struct FBattlingTablesLayout
		{
			EDiscRelease Release;
			int32 ProgramFile;
			std::size_t AnimationKinds;
			std::size_t Chain;
			std::size_t ProjectileKinds;
			std::size_t SoundPairs;
			std::size_t RankarSounds;
			std::size_t DeathSounds;
			std::size_t WyrmSounds;
			std::size_t DefaultSounds;
			std::size_t ChuChuSounds;
			std::size_t TrailLengths;
			std::size_t ChainInputs;
			std::size_t ScriptsStart;
			std::size_t ScriptsEnd;
			std::size_t Lessons;
			std::size_t SettleSteps;
			std::size_t EndingScript;
			std::size_t FontGlyphs;
			std::size_t Roster;
			std::size_t ElFenrirPart;
			uint32 DpadX;
			uint32 DpadY;
		};

		using BattlingFiles::DataDirectory;
		using BattlingFiles::GearDirectory;
		using BattlingFiles::ProgramDirectory;
		using BattlingFiles::ReadPacked;
		constexpr std::array<FBattlingTablesLayout, 2> Layouts{{
			{EDiscRelease::UsaDisc1, 13, 0x21640, 0x21688, 0x21604, 0x223F0, 0x22470, 0x22480, 0x22490, 0x224A0, 0x224B0, 0x21738, 0x22AB4, 0x21448, 0x21600, 0x2156C, 0x21594, 0x215D4,
				0x21740, 0x21E74, 0x224C0, 0x8005020C, 0x8005021C},
			{EDiscRelease::UsaDisc2, 13, 0x21640, 0x21688, 0x21604, 0x223F0, 0x22470, 0x22480, 0x22490, 0x224A0, 0x224B0, 0x21738, 0x22AB4, 0x21448, 0x21600, 0x2156C, 0x21594, 0x215D4,
				0x21740, 0x21E74, 0x224C0, 0x8005020C, 0x8005021C},
		}};

		/** The gears whose sound sets are their own. */
		constexpr int32 ChuChu = 9;
		constexpr int32 Wyrm = 0x1B;
		constexpr int32 Rankar = 0x1D;
		constexpr int32 Death = 0x24;

		constexpr int32 ArchiveFile = 3;
		constexpr int32 TerrainFile = 4;
		constexpr std::size_t GearRowSize = 32;
		constexpr int32 HeightScale = 12;
		constexpr int32 HeaderWords = 10;
		constexpr std::size_t RosterEntrySize = 12;
		constexpr uint32 MessagesEntry = 1;
		/** Where the program is loaded, which its pointers to its scripts count from. */
		constexpr uint32 ProgramAddress = 0x8006FAF0;

		/** A label the code prints: where its code builds the string's address (the lui; an addiu or ori follows). */
		struct FCodeText
		{
			std::string FBattlingTexts::*Text;
			uint32 Lui;
		};
		constexpr std::array<FCodeText, 32> CodeTexts{{
			{&FBattlingTexts::Winner, 0x800729EC},
			{&FBattlingTexts::Level, 0x80072A18},
			{&FBattlingTexts::Matches, 0x80072A28},
			{&FBattlingTexts::Time, 0x80072A38},
			{&FBattlingTexts::ScoreFormat, 0x80072AF4},
			{&FBattlingTexts::TimeFormat, 0x80083D98},
			{&FBattlingTexts::Ready, 0x800755BC},
			{&FBattlingTexts::Fight, 0x80075524},
			{&FBattlingTexts::Practice, 0x800755F4},
			{&FBattlingTexts::Start, 0x80075578},
			{&FBattlingTexts::RoundFormat, 0x80075640},
			{&FBattlingTexts::RubberBandBattle, 0x80075680},
			{&FBattlingTexts::RubberBandMode, 0x800755AC},
			{&FBattlingTexts::KnockOut, 0x80075324},
			{&FBattlingTexts::DrawGame, 0x8007530C},
			{&FBattlingTexts::Replay, 0x8007A450},
			{&FBattlingTexts::YouWereVictorious, 0x8007AFDC},
			{&FBattlingTexts::YouWereDefeated, 0x8007B054},
			{&FBattlingTexts::ByFormat, 0x8007B084},
			{&FBattlingTexts::PlayerOneVictory, 0x8007AFEC},
			{&FBattlingTexts::PlayerTwoVictory, 0x8007B09C},
			{&FBattlingTexts::ComputerOneVictory, 0x8007AFFC},
			{&FBattlingTexts::ComputerTwoVictory, 0x8007B0AC},
			{&FBattlingTexts::StrengthFormat, 0x80076738},
			{&FBattlingTexts::FramesFormat, 0x80076764},
			{&FBattlingTexts::FrameRateFormat, 0x8007FA6C},
			{&FBattlingTexts::NoLimit, 0x8007FEDC},
			{&FBattlingTexts::On, 0x8007FF40},
			{&FBattlingTexts::Off, 0x8007FF30},
			{&FBattlingTexts::VibrationOn, 0x8007FCB4},
			{&FBattlingTexts::VibrationOff, 0x8007FCA0},
			{&FBattlingTexts::Versus, 0x8007F6D4},
		}};
		/** The tables of strings: the moves' names by chain state, the computer's levels, practice's styles. */
		constexpr uint32 MoveNamesTable = 0x80091198;
		constexpr uint32 LevelNamesTable = 0x800912F4;
		constexpr uint32 PracticeStylesTable = 0x8009132C;
		/** The menus (0x3C bytes each: +4 their items, +8 the count) and their items (0x14 bytes: +4 the label). */
		constexpr uint32 MenusTable = 0x800915AC;
		constexpr uint32 MenuSize = 0x3C;
		constexpr uint32 ItemSize = 0x14;
		/** How far after its lui the code looks for the instruction that finishes an address. */
		constexpr uint32 AddressReach = 8;

		/** The program's word at an address, or none past it. */
		std::optional<uint32> ProgramWord(std::span<const uint8> Program, uint32 Address)
		{
			const uint32 Offset = Address - ProgramAddress;
			if (Address < ProgramAddress || Program.size() < 4 || Offset > Program.size() - 4)
			{
				return std::nullopt;
			}
			return ReadU32(Program, Offset);
		}

		/** The address a lui and the first addiu or ori after it on the same register make, or none. */
		std::optional<uint32> CodeAddress(std::span<const uint8> Program, uint32 Lui)
		{
			const std::optional<uint32> High = ProgramWord(Program, Lui);
			if (!High || *High >> 26 != 0x0F)
			{
				return std::nullopt;
			}
			const uint32 Register = *High >> 16 & 31;
			for (uint32 Step = 1; Step <= AddressReach; ++Step)
			{
				const std::optional<uint32> Word = ProgramWord(Program, Lui + 4 * Step);
				if (!Word)
				{
					return std::nullopt;
				}
				const uint32 Op = *Word >> 26;
				if ((Op == 0x09 || Op == 0x0D) && (*Word >> 21 & 31) == Register)
				{
					const uint32 Low = *Word & 0xFFFF;
					return (*High << 16) + (Op == 0x09 ? static_cast<uint32>(static_cast<int32>(static_cast<int16>(Low))) : Low);
				}
			}
			return std::nullopt;
		}

		/** The string at an address of the program; address 0, where a table holds none, the empty string the original finds there. */
		std::optional<std::string> ProgramString(std::span<const uint8> Program, uint32 Address)
		{
			if (Address == 0)
			{
				return std::string();
			}
			const uint32 Offset = Address - ProgramAddress;
			if (Address < ProgramAddress || Offset >= Program.size())
			{
				return std::nullopt;
			}
			const auto First = Program.begin() + static_cast<std::ptrdiff_t>(Offset);
			const auto End = std::find(First, Program.end(), uint8{0});
			if (End == Program.end())
			{
				return std::nullopt;
			}
			return std::string(First, End);
		}

		/** The strings a table of pointers points at, one each. */
		bool ReadStringTable(std::span<const uint8> Program, uint32 Table, std::span<std::string> Into)
		{
			for (std::size_t Entry = 0; Entry < Into.size(); ++Entry)
			{
				const std::optional<uint32> Pointer = ProgramWord(Program, Table + static_cast<uint32>(4 * Entry));
				std::optional<std::string> Text = Pointer ? ProgramString(Program, *Pointer) : std::nullopt;
				if (!Text)
				{
					return false;
				}
				Into[Entry] = std::move(*Text);
			}
			return true;
		}

		/** Every label and menu text the program prints, where its code and tables point. */
		TResult<FBattlingTexts> ReadTexts(std::span<const uint8> Program)
		{
			FBattlingTexts Texts;
			for (const FCodeText& Code : CodeTexts)
			{
				const std::optional<uint32> Address = CodeAddress(Program, Code.Lui);
				std::optional<std::string> Text = Address ? ProgramString(Program, *Address) : std::nullopt;
				if (!Text)
				{
					return MakeError("a label of the Battling program's code does not point at a string");
				}
				Texts.*Code.Text = std::move(*Text);
			}
			// The moves' first name (before any move) is an empty string other labels share, into which the French
			// translation's longer start label runs: it stays empty.
			if (!ReadStringTable(Program, MoveNamesTable + 4, std::span<std::string>(Texts.MoveNames).subspan(1))
				|| !ReadStringTable(Program, LevelNamesTable, Texts.LevelNames)
				|| !ReadStringTable(Program, PracticeStylesTable, Texts.PracticeStyles))
			{
				return MakeError("the Battling program's tables of names point past it");
			}
			for (uint32 Menu = 0; Menu < FBattlingTexts::MenuCount; ++Menu)
			{
				const std::optional<uint32> Items = ProgramWord(Program, MenusTable + MenuSize * Menu + 4);
				const std::optional<uint32> Count = ProgramWord(Program, MenusTable + MenuSize * Menu + 8);
				std::vector<std::string>& Labels = Texts.MenuLabels[Menu];
				if (!Items || !Count || (*Count & 0xFFFF) != Labels.size())
				{
					return MakeError("the Battling program's menu " + std::to_string(Menu) + " is not the game's");
				}
				for (uint32 Item = 0; Item < Labels.size(); ++Item)
				{
					const std::optional<uint32> Label = ProgramWord(Program, *Items + ItemSize * Item + 4);
					std::optional<std::string> Text = Label ? ProgramString(Program, *Label) : std::nullopt;
					if (!Text)
					{
						return MakeError("the Battling program's menu " + std::to_string(Menu) + " has a label past it");
					}
					Labels[Item] = std::move(*Text);
				}
			}
			return Texts;
		}

		const FBattlingTablesLayout* FindLayout(EDiscRelease Release)
		{
			for (const FBattlingTablesLayout& Candidate : Layouts)
			{
				if (Candidate.Release == GameplayRelease(Release))
				{
					return &Candidate;
				}
			}
			return nullptr;
		}

		FBattlingSoundSet ReadSoundSet(std::span<const uint8> Program, std::size_t Offset)
		{
			FBattlingSoundSet Set{};
			for (std::size_t Index = 0; Index < Set.size(); ++Index)
			{
				Set[Index] = Program[Offset + Index];
			}
			return Set;
		}

		bool Fits(std::span<const uint8> Bytes, std::size_t Offset, std::size_t Size)
		{
			return Offset <= Bytes.size() && Size <= Bytes.size() - Offset;
		}

		/**
		 * A script's instructions run to its end (00) inside the area, each of a kind the machine runs without looping forever,
		 * timed ones for at least a tick, and none drives a fighter before the script has chosen one.
		 */
		bool IsRunnableScript(std::span<const uint8> Scripts, std::size_t At)
		{
			bool bChosen = false;
			while (At < Scripts.size())
			{
				const uint8 Op = Scripts[At];
				if (Op == 0)
				{
					return true;
				}
				// 10h and 11h send the machine back to the same instruction for ever; nothing past 22h is an instruction.
				if (Op == 0x10 || Op == 0x11 || Op > 0x22)
				{
					return false;
				}
				const bool bDrives = (0x04 <= Op && Op <= 0x0F) || Op == 0x14 || Op == 0x1C || Op == 0x1D || Op == 0x1F || Op == 0x20 || Op == 0x22;
				if (bDrives && !bChosen)
				{
					return false;
				}
				bChosen = bChosen || Op == 0x02 || Op == 0x03;
				const bool bTimed = Op == 0x01 || Op == 0x0E || Op == 0x0F || Op == 0x14;
				if (bTimed && (At + 1 >= Scripts.size() || Scripts[At + 1] == 0))
				{
					return false;
				}
				std::size_t Length = 1;
				switch (Op)
				{
				case 0x01:
				case 0x0E:
				case 0x0F:
				case 0x12:
				case 0x14:
				case 0x16:
				case 0x17:
				case 0x18:
				case 0x1C:
				case 0x1D:
				case 0x1F:
				case 0x20:
				case 0x21:
				case 0x22:
					Length = 2;
					break;
				case 0x15:
					Length = 3;
					break;
				default:
					break;
				}
				At += Length;
			}
			return false;
		}

		/** Reads a script's start from a pointer of the program into the area that begins at Start. */
		TResult<uint16> ReadScriptStart(std::span<const uint8> Program, std::size_t At, std::size_t Start, std::size_t End)
		{
			const uint32 Address = ReadU32(Program, At);
			if (Address < ProgramAddress + Start || ProgramAddress + End <= Address)
			{
				return MakeError("a Battling script starts outside the program's script area");
			}
			return static_cast<uint16>(Address - ProgramAddress - Start);
		}

		/** An animation's channel bytes: the type (6-8 move along x, y, z; 3-5 turn about them; bit 7 not read), the joint, the value. */
		TResult<FBattlingChannel> ReadChannel(std::span<const uint8> Bytes, std::size_t At, std::size_t Animation, bool bKeyed, std::size_t JointCount)
		{
			FBattlingChannel Channel;
			const uint8 Type = Bytes[At] & 0x7F;
			if (Type < 3 || Type > 8)
			{
				return MakeError("a gear animation's channel drives nothing the program binds");
			}
			Channel.Target = static_cast<EBattlingChannelTarget>(Type - 3);
			Channel.Joint = Bytes[At + 1];
			if (Channel.Joint >= JointCount)
			{
				return MakeError("a gear animation's channel drives a joint the hierarchy lacks");
			}
			const uint16 Value = ReadU16(Bytes, At + 2);
			Channel.Value = bKeyed ? static_cast<uint32>(Animation + Value) : Value;
			if (bKeyed && Channel.Value >= Bytes.size())
			{
				return MakeError("a gear animation's keyed channel starts past its file");
			}
			return Channel;
		}

		TResult<FBattlingEvent> ReadEvent(std::span<const uint8> Bytes, std::size_t At)
		{
			if (!Fits(Bytes, At, 8))
			{
				return MakeError("a gear animation's event lies past its file");
			}
			const uint8 Kind = Bytes[At];
			if (Kind > static_cast<uint8>(EBattlingEventKind::ShowPart))
			{
				return MakeError("a gear animation's event is of no kind the program runs");
			}
			FBattlingEvent Event;
			Event.Kind = static_cast<EBattlingEventKind>(Kind);
			Event.What = Bytes[At + 1];
			Event.JointA = Bytes[At + 2];
			Event.JointB = Bytes[At + 3];
			Event.VertexA = ReadS16(Bytes, At + 4);
			Event.VertexB = ReadS16(Bytes, At + 6);
			Event.Offset = static_cast<uint32>(At);
			return Event;
		}
	}

	TResult<std::vector<uint8>> BattlingFiles::ReadPacked(const FCdImage& Image, const FGameFileIndex& Index, int32 Directory, int32 Number)
	{
		const FIndexRecord* Record = Index.Find(Directory, Number);
		if (Record == nullptr || !Record->IsFile())
		{
			return MakeError("the disc lacks Battling's file " + std::to_string(Directory) + "/" + std::to_string(Number));
		}
		const TResult<std::vector<uint8>> Sectors = ReadFileSectors(Image, *Record);
		if (!Sectors)
		{
			return Sectors.Error();
		}
		return UnpackLzss(*Sectors);
	}

	const FBattlingSoundSet& FBattlingProgramTables::SoundSetOf(int32 Gear) const
	{
		switch (Gear)
		{
		case ChuChu:
			return ChuChuSoundSet;
		case Wyrm:
			return WyrmSoundSet;
		case Rankar:
			return RankarSoundSet;
		case Death:
			return DeathSoundSet;
		default:
			return DefaultSoundSet;
		}
	}

	TResult<FBattlingProgramTables> FBattlingProgramTables::Read(const FCdImage& Image, const FGameFileIndex& Index, const FPsxExecutable& Executable,
		EDiscRelease Release)
	{
		const FBattlingTablesLayout* Layout = FindLayout(Release);
		if (Layout == nullptr)
		{
			return MakeError("no Battling tables are known for this release");
		}
		const TResult<std::vector<uint8>> Unpacked = ReadPacked(Image, Index, ProgramDirectory, Layout->ProgramFile);
		if (!Unpacked)
		{
			return Unpacked.Error();
		}
		const std::span<const uint8> Program = *Unpacked;
		if (!Fits(Program, Layout->ChuChuSounds, 16))
		{
			return MakeError("the Battling program is too short to hold its tables");
		}

		FBattlingProgramTables Tables;
		for (std::size_t Animation = 0; Animation < Tables.AnimationKinds.size(); ++Animation)
		{
			Tables.AnimationKinds[Animation] = {Program[Layout->AnimationKinds + 2 * Animation], Program[Layout->AnimationKinds + 2 * Animation + 1]};
		}
		for (std::size_t State = 0; State < Tables.Chain.size(); ++State)
		{
			Tables.Chain[State] = {Program[Layout->Chain + 2 * State], Program[Layout->Chain + 2 * State + 1]};
		}
		for (std::size_t Kind = 0; Kind < Tables.ProjectileKinds.size(); ++Kind)
		{
			const std::size_t At = Layout->ProjectileKinds + 8 * Kind;
			Tables.ProjectileKinds[Kind] = {ReadS16(Program, At), Program[At + 2], Program[At + 3], Program[At + 4], Program[At + 5], Program[At + 6]};
		}
		for (std::size_t Pair = 0; Pair < Tables.SoundPairs.size(); ++Pair)
		{
			Tables.SoundPairs[Pair] = {Program[Layout->SoundPairs + 2 * Pair], Program[Layout->SoundPairs + 2 * Pair + 1]};
		}
		Tables.DefaultSoundSet = ReadSoundSet(Program, Layout->DefaultSounds);
		Tables.RankarSoundSet = ReadSoundSet(Program, Layout->RankarSounds);
		Tables.DeathSoundSet = ReadSoundSet(Program, Layout->DeathSounds);
		Tables.WyrmSoundSet = ReadSoundSet(Program, Layout->WyrmSounds);
		Tables.ChuChuSoundSet = ReadSoundSet(Program, Layout->ChuChuSounds);
		TResult<FMdlPart> ElFenrirPart = ParseMdlPart(Program.subspan(Layout->ElFenrirPart));
		if (!ElFenrirPart)
		{
			return MakeError("El-Fenrir's part in the Battling program: " + ElFenrirPart.Error().Message);
		}
		Tables.ElFenrirPart = std::move(*ElFenrirPart);
		for (std::size_t State = 0; State < Tables.ChainInputs.size(); ++State)
		{
			for (std::size_t Input = 0; Input < 3; ++Input)
			{
				Tables.ChainInputs[State][Input] = Program[Layout->ChainInputs + 3 * State + Input];
			}
		}
		for (std::size_t Kind = 0; Kind < Tables.TrailLengths.size(); ++Kind)
		{
			Tables.TrailLengths[Kind] = ReadS16(Program, Layout->TrailLengths + 2 * Kind);
		}
		if (!Fits(Program, Layout->ScriptsStart, Layout->ScriptsEnd - Layout->ScriptsStart))
		{
			return MakeError("the Battling program is too short to hold its scripts");
		}
		Tables.Scripts.assign(Program.begin() + static_cast<std::ptrdiff_t>(Layout->ScriptsStart), Program.begin() + static_cast<std::ptrdiff_t>(Layout->ScriptsEnd));
		for (std::size_t Lesson = 0; Lesson < Tables.Lessons.size(); ++Lesson)
		{
			const TResult<uint16> Start = ReadScriptStart(Program, Layout->Lessons + 4 * Lesson, Layout->ScriptsStart, Layout->ScriptsEnd);
			if (!Start)
			{
				return Start.Error();
			}
			Tables.Lessons[Lesson] = *Start;
		}
		Tables.EndingScript = static_cast<uint16>(Layout->EndingScript - Layout->ScriptsStart);
		if (!IsRunnableScript(Tables.Scripts, Tables.EndingScript)
			|| !std::all_of(Tables.Lessons.begin(), Tables.Lessons.end(), [&](uint16 Start) { return IsRunnableScript(Tables.Scripts, Start); }))
		{
			return MakeError("a Battling script runs an instruction its machine cannot");
		}
		for (std::size_t Step = 0; Step < Tables.SettleSteps.size(); ++Step)
		{
			Tables.SettleSteps[Step] = {static_cast<int8>(static_cast<int32>(ReadU32(Program, Layout->SettleSteps + 8 * Step))),
				static_cast<int8>(static_cast<int32>(ReadU32(Program, Layout->SettleSteps + 8 * Step + 4)))};
		}
		if (!Fits(Program, Layout->FontGlyphs, 4 * FBattlingProgramTables::GlyphCount) || !Fits(Program, Layout->Roster, RosterEntrySize * BattlingGears::Count))
		{
			return MakeError("the Battling program is too short to hold its font and roster");
		}
		for (std::size_t Glyph = 0; Glyph < Tables.FontGlyphs.size(); ++Glyph)
		{
			const std::size_t At = Layout->FontGlyphs + 4 * Glyph;
			Tables.FontGlyphs[Glyph] = {Program[At], Program[At + 1], Program[At + 2], Program[At + 3]};
		}
		for (std::size_t Gear = 0; Gear < Tables.GearNames.size(); ++Gear)
		{
			// The roster lists the gears in their numbers' order: its number, the model's name, then the shown name.
			const std::size_t At = Layout->Roster + RosterEntrySize * Gear;
			const uint32 Name = ReadU32(Program, At + 8) - ProgramAddress;
			if (ReadU32(Program, At) != Gear || Name >= Program.size())
			{
				return MakeError("the Battling roster's gear " + std::to_string(Gear) + " is not where its number says");
			}
			const auto First = Program.begin() + static_cast<std::ptrdiff_t>(Name);
			Tables.GearNames[Gear].assign(First, std::find(First, Program.end(), uint8{0}));
		}
		TResult<FBattlingTexts> Texts = ReadTexts(Program);
		if (!Texts)
		{
			return Texts.Error();
		}
		Tables.Texts = std::move(*Texts);
		if (!Executable.Contains(Layout->DpadX, 16) || !Executable.Contains(Layout->DpadY, 16))
		{
			return MakeError("the executable lacks the kernel's d-pad tables");
		}
		const std::span<const uint8> DpadX = Executable.Read(Layout->DpadX, 16);
		const std::span<const uint8> DpadY = Executable.Read(Layout->DpadY, 16);
		std::copy(DpadX.begin(), DpadX.end(), Tables.DpadX.begin());
		std::copy(DpadY.begin(), DpadY.end(), Tables.DpadY.begin());
		TResult<FGteArctangents> Arctangents = FGteArctangents::Read(Executable);
		if (!Arctangents)
		{
			return Arctangents.Error();
		}
		Tables.Arctangents = *Arctangents;
		for (const FBattlingAnimationKind& Kind : Tables.AnimationKinds)
		{
			if (Kind.Kind > 3)
			{
				return MakeError("an animation of the Battling program ends in no known way");
			}
		}
		return Tables;
	}

	TResult<FBattlingGearTable> FBattlingGearTable::Read(const FCdImage& Image, const FGameFileIndex& Index)
	{
		const TResult<std::vector<uint8>> Archive = ReadPacked(Image, Index, DataDirectory, ArchiveFile);
		if (!Archive)
		{
			return Archive.Error();
		}
		const std::span<const uint8> Bytes = *Archive;
		if (Bytes.size() < 12 || ReadU32(Bytes, 0) < 2)
		{
			return MakeError("Battling's archive has no gear table");
		}
		const std::size_t Start = ReadU32(Bytes, 8);
		if (!Fits(Bytes, Start, GearRowSize * BattlingGears::Count))
		{
			return MakeError("Battling's gear table runs past its archive");
		}
		FBattlingGearTable Table;
		for (std::size_t Gear = 0; Gear < Table.Gears.size(); ++Gear)
		{
			const std::size_t At = Start + GearRowSize * Gear;
			FBattlingGearStats& Stats = Table.Gears[Gear];
			Stats.Power = ReadS16(Bytes, At);
			Stats.ArenaHp = ReadS16(Bytes, At + 2);
			Stats.Threshold = ReadS16(Bytes, At + 4);
			for (std::size_t Trait = 0; Trait < Stats.Traits.size(); ++Trait)
			{
				Stats.Traits[Trait] = Bytes[At + 6 + Trait];
			}
			for (std::size_t Move = 0; Move < Stats.Strengths.size(); ++Move)
			{
				Stats.Strengths[Move] = Bytes[At + 10 + Move];
			}
			Stats.Ether = Bytes[At + 0x18];
			Stats.ArenaDashHeat = Bytes[At + 0x1B];
			Stats.ArenaAcceleration = Bytes[At + 0x1C];
			Stats.ArenaDeceleration = Bytes[At + 0x1D];
			Stats.ArenaEtherHeat = Bytes[At + 0x1E];
			Stats.ArenaKnockDown = Bytes[At + 0x1F];
		}
		return Table;
	}

	TResult<FBattlingMessages> FBattlingMessages::Read(const FCdImage& Image, const FGameFileIndex& Index)
	{
		TResult<std::vector<uint8>> Archive = ReadPacked(Image, Index, DataDirectory, ArchiveFile);
		if (!Archive)
		{
			return Archive.Error();
		}
		FBattlingMessages Messages;
		Messages.Archive = std::move(*Archive);
		const std::span<const uint8> Bytes = Messages.Archive;
		if (Bytes.size() < 12 || ReadU32(Bytes, 0) < 2)
		{
			return MakeError("Battling's archive has no message table");
		}
		const std::size_t Start = ReadU32(Bytes, 4 * MessagesEntry);
		const std::size_t End = ReadU32(Bytes, 4 * (MessagesEntry + 1));
		if (End < Start || !Fits(Bytes, Start, End - Start))
		{
			return MakeError("Battling's message table runs past its archive");
		}
		TResult<FMessageTable> Table = FMessageTable::Parse(Bytes.subspan(Start, End - Start));
		if (!Table)
		{
			return Table.Error();
		}
		Messages.Messages = *Table;
		if (std::optional<FAppliedMessages> Applied = ApplyMessageOverrides(Image, MessageTables::Battling, Bytes.subspan(Start, End - Start), EMessageWindows::Program))
		{
			Messages.Replaced = std::move(Applied->Table);
			Messages.Messages = *FMessageTable::Parse(Messages.Replaced);
		}
		return Messages;
	}

	TResult<FBattlingTerrain> FBattlingTerrain::Read(const FCdImage& Image, const FGameFileIndex& Index)
	{
		const TResult<std::vector<uint8>> Unpacked = ReadPacked(Image, Index, DataDirectory, TerrainFile);
		if (!Unpacked)
		{
			return Unpacked.Error();
		}
		if (Unpacked->size() < static_cast<std::size_t>(Size * Size * 4))
		{
			return MakeError("Battling's heightfield is shorter than its 128 x 128 cells");
		}
		return FromFile(*Unpacked);
	}

	FBattlingTerrain FBattlingTerrain::FromFile(std::span<const uint8> Unpacked)
	{
		FBattlingTerrain Terrain;
		Terrain.Cells.resize(static_cast<std::size_t>(Size * Size));
		for (std::size_t Cell = 0; Cell < Terrain.Cells.size(); ++Cell)
		{
			const uint32 Value = ReadU32(Unpacked, 4 * Cell);
			const uint32 Height = static_cast<uint16>((Value & 0xFFFF) * HeightScale);
			Terrain.Cells[Cell] = (Value & 0xFFFF0000u) | Height;
		}
		return Terrain;
	}

	bool FBattlingTerrain::Covers(int32 X, int32 Z)
	{
		const int32 First = (Z >> 8) * Size + (X >> 8);
		return First >= 0 && First + Size + 1 < Size * Size;
	}

	FBattlingTerrainSample FBattlingTerrain::Sample(int32 X, int32 Y, int32 Z, bool bWithBed) const
	{
		const int32 First = (Z >> 8) * Size + (X >> 8);
		const auto Corner = [&](int32 Offset) {
			const uint32 Value = CellAt(First + Offset);
			int16 CornerHeight = static_cast<int16>(Value);
			if (bWithBed)
			{
				const uint32 Ground = (Value >> 24) & 3;
				if (Ground == static_cast<uint32>(EBattlingGround::Water))
				{
					CornerHeight = static_cast<int16>(CornerHeight + 0xC0 + 0x40);
				}
				else if (Ground == static_cast<uint32>(EBattlingGround::Sand))
				{
					CornerHeight = static_cast<int16>(CornerHeight + 0x40);
				}
			}
			return CornerHeight;
		};

		// The corners keep 16 bits of their places, as the program builds them in short vectors.
		const int16 X0 = static_cast<int16>(X & 0xFF00);
		const int16 Z0 = static_cast<int16>(Z & 0xFF00);
		const int16 X1 = static_cast<int16>(X0 + CellUnits);
		const int16 Z1 = static_cast<int16>(Z0 + CellUnits);
		const FGteVector Origin{X0, Corner(0), Z0};
		const FGteVector Far{X1, Corner(Size + 1), Z1};
		const FGteVector RightCorner{X1, Corner(1), Z0};
		const FGteVector BackCorner{X0, Corner(Size), Z1};

		// Which side of the diagonal from the cell's origin to its far corner (X, Z) lies on, in the program's 32 bits.
		const uint32 Side = static_cast<uint32>(Z0 - Z1) * static_cast<uint32>(X) + static_cast<uint32>(X1 - X0) * static_cast<uint32>(Z)
			+ static_cast<uint32>(X0 * Z1) - static_cast<uint32>(X1 * Z0);
		const bool bFirstTriangle = static_cast<int32>(Side) < 0;
		const FGteVector& Second = bFirstTriangle ? Far : BackCorner;
		const FGteVector& Third = bFirstTriangle ? RightCorner : Far;

		// The kernel's triangle normal, step by step: (P2 - P0) x (P1 - P0), divided by the root of its largest component,
		// normalized.
		const FGteLongVector ToSecond{Second[0] - Origin[0], Second[1] - Origin[1], Second[2] - Origin[2]};
		const FGteLongVector ToThird{Third[0] - Origin[0], Third[1] - Origin[1], Third[2] - Origin[2]};
		FGteLongVector Scaled = OuterProduct0(ToThird, ToSecond);
		int32 Largest = LargestComponent(Scaled);
		if (Largest < 0)
		{
			Largest = static_cast<int32>(0u - static_cast<uint32>(Largest));
		}
		const int32 Root = SquareRoot0(Largest);
		for (int32& Component : Scaled)
		{
			Component = ProcessorDivide(Component, Root);
		}
		const FGteVector Normal = VectorNormalS(Scaled);

		const int32 Plane = Origin[0] * Normal[0] + Origin[1] * Normal[1] + Origin[2] * Normal[2];
		const int32 Here = X * Normal[0] + Y * Normal[1] + Z * Normal[2];
		return {Y + ProcessorDivide(Plane - Here, Normal[1]), Scaled[1], Normal};
	}

	TResult<FBattlingGearFile> FBattlingGearFile::Read(const FCdImage& Image, const FGameFileIndex& Index, int32 Gear)
	{
		TResult<std::vector<uint8>> Unpacked = ReadPacked(Image, Index, GearDirectory, Gear + 2);
		if (!Unpacked)
		{
			return Unpacked.Error();
		}
		return Parse(std::move(*Unpacked));
	}

	TResult<FBattlingGearFile> FBattlingGearFile::Parse(std::vector<uint8> Unpacked)
	{
		FBattlingGearFile File;
		File.Bytes = std::move(Unpacked);
		const std::span<const uint8> Bytes = File.Bytes;
		if (Bytes.size() < 4 * HeaderWords)
		{
			return MakeError("a gear file is shorter than its header");
		}
		std::array<uint32, HeaderWords> Header{};
		for (std::size_t Word = 0; Word < Header.size(); ++Word)
		{
			Header[Word] = ReadU32(Bytes, 4 * Word);
		}
		const uint32 Hierarchy = Header[0];
		const uint32 Animations = Header[2];
		const uint32 AnimationTable = Header[5];
		const uint32 MoveSet = Header[6];
		File.Model = Header[1];
		File.Data = Header[4];
		File.Textures = Header[3];
		File.HudPictures = Header[8];
		if (Header[7] != 0 || !Fits(Bytes, Hierarchy, 4) || !Fits(Bytes, Animations, 4) || !Fits(Bytes, MoveSet, 4 * BattlingGears::ChainStates)
			|| AnimationTable < File.Data || MoveSet < AnimationTable || !Fits(Bytes, File.Data, 0x34) || File.Model >= Bytes.size()
			|| !Fits(Bytes, File.HudPictures, BattlingGears::HudPicturesSize))
		{
			return MakeError("a gear file's header points past it");
		}

		const uint32 JointCount = ReadU32(Bytes, Hierarchy);
		if (!Fits(Bytes, Hierarchy + 4, 16 * static_cast<std::size_t>(JointCount)))
		{
			return MakeError("a gear file's hierarchy runs past it");
		}
		for (uint32 Joint = 0; Joint < JointCount; ++Joint)
		{
			const std::size_t At = Hierarchy + 4 + 16 * Joint;
			FBattlingJointRecord Record;
			Record.Parent = ReadS16(Bytes, At);
			Record.Part = ReadS16(Bytes, At + 2);
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Record.Angles[Axis] = ReadS16(Bytes, At + 4 + 2 * Axis);
				Record.Translation[Axis] = ReadS16(Bytes, At + 10 + 2 * Axis);
			}
			if (Record.Parent >= static_cast<int16>(Joint) || Record.Parent < -1)
			{
				return MakeError("a gear file's joint hangs from one that does not come before it");
			}
			File.Joints.push_back(Record);
		}

		const uint32 AnimationCount = ReadU32(Bytes, Animations);
		if (!Fits(Bytes, Animations + 4, 4 * static_cast<std::size_t>(AnimationCount)))
		{
			return MakeError("a gear file's animation list runs past it");
		}
		for (uint32 Number = 0; Number < AnimationCount; ++Number)
		{
			const uint32 At = ReadU32(Bytes, Animations + 4 + 4 * Number);
			FBattlingAnimation Animation;
			File.HasAnimation.push_back(At != 0);
			if ((At & 1) != 0)
			{
				return MakeError("a gear animation lies at an odd offset, which the program's step would take for its end");
			}
			if (At != 0)
			{
				if (!Fits(Bytes, At, 8))
				{
					return MakeError("a gear animation lies past its file");
				}
				Animation.Frames = ReadS16(Bytes, At);
				const int16 Statics = ReadS16(Bytes, At + 4);
				const int16 Keyed = ReadS16(Bytes, At + 6);
				if (Statics < 0 || Keyed < 0 || !Fits(Bytes, At + 8, 4 * static_cast<std::size_t>(Statics + Keyed)))
				{
					return MakeError("a gear animation's channels run past its file");
				}
				for (int32 Channel = 0; Channel < Statics + Keyed; ++Channel)
				{
					const bool bKeyed = Channel >= Statics;
					TResult<FBattlingChannel> Read = ReadChannel(Bytes, At + 8 + 4 * static_cast<std::size_t>(Channel), At, bKeyed, JointCount);
					if (!Read)
					{
						return Read.Error();
					}
					(bKeyed ? Animation.KeyedChannels : Animation.StaticChannels).push_back(*Read);
				}
			}
			File.Animations.push_back(std::move(Animation));
		}

		for (uint32 At = AnimationTable; At + 1 < MoveSet; At += 2)
		{
			File.AnimationTable.push_back({Bytes[At], Bytes[At + 1]});
		}
		for (std::size_t State = 0; State < File.Moves.size(); ++State)
		{
			const std::size_t At = MoveSet + 4 * State;
			File.Moves[State] = {Bytes[At], Bytes[At + 1], Bytes[At + 2], Bytes[At + 3]};
		}

		const std::size_t Data = File.Data;
		File.TopJoint = Bytes[Data];
		File.FootJoints = {Bytes[Data + 1], Bytes[Data + 2]};
		File.PushRadius = Bytes[Data + 3];
		File.FootVertices = {ReadS16(Bytes, Data + 4), ReadS16(Bytes, Data + 6)};
		File.TopVertex = ReadS16(Bytes, Data + 8);
		File.SpeedScale = ReadS16(Bytes, Data + 0xC);
		File.MoveScale = Bytes[Data + 0xF];
		File.Colour = {Bytes[Data + 0x10], Bytes[Data + 0x11], Bytes[Data + 0x12]};
		File.BodyRadius = Bytes[Data + 0x13];
		File.ModelScale = ReadS16(Bytes, Data + 0x20);
		File.WinnerPitch = ReadS16(Bytes, Data + 0x14);
		File.WinnerSpin = ReadS16(Bytes, Data + 0x16);
		File.WinnerCameraHeight = ReadS16(Bytes, Data + 0x18);
		File.WinnerCameraDistance = ReadS16(Bytes, Data + 0x1A);
		File.WinnerScale = ReadS16(Bytes, Data + 0x1C);
		File.WinnerYaw = ReadS16(Bytes, Data + 0x1E);
		File.ResultAngle = ReadS16(Bytes, Data + 0x24);
		File.ResultEyeHeight = ReadS16(Bytes, Data + 0x26);
		File.ResultTargetHeight = ReadS16(Bytes, Data + 0x28);
		File.ResultDistance = ReadS16(Bytes, Data + 0x2A);
		File.bVictoryLoops = ReadS16(Bytes, Data + 0x2C) != 0;
		const std::size_t Hidden = Data + ReadU32(Bytes, Data + 0x30);
		const uint8 HiddenCount = Bytes[Data + 0xE];
		if (!Fits(Bytes, Hidden, HiddenCount))
		{
			return MakeError("a gear file's hidden parts run past it");
		}
		for (uint8 Entry = 0; Entry < HiddenCount; ++Entry)
		{
			const uint8 Joint = Bytes[Hidden + Entry];
			if (Joint >= JointCount || File.Joints[Joint].Part < 0)
			{
				return MakeError("a gear file hides a joint without a part");
			}
			File.HiddenJoints.push_back(Joint);
		}
		for (const uint8 Joint : {File.TopJoint, File.FootJoints[0], File.FootJoints[1]})
		{
			if (Joint >= JointCount)
			{
				return MakeError("a gear file's body lies on a joint it lacks");
			}
		}

		// Each animation's event list: entries { first frame, last frame, the event's offset in the data block }, ended by FFh.
		if (!Fits(Bytes, Data + 0x34, 2 * static_cast<std::size_t>(AnimationCount)))
		{
			return MakeError("a gear file's event index runs past it");
		}
		File.EventLists.resize(AnimationCount);
		File.HasEventList.resize(AnimationCount);
		for (uint32 Number = 0; Number < AnimationCount; ++Number)
		{
			const int16 List = ReadS16(Bytes, Data + 0x34 + 2 * Number);
			if (List == 0)
			{
				continue;
			}
			File.HasEventList[Number] = true;
			std::size_t At = Data + static_cast<std::size_t>(List);
			for (;;)
			{
				if (List < 0 || !Fits(Bytes, At, 1))
				{
					return MakeError("a gear animation's event list runs past its file");
				}
				if (Bytes[At] == 0xFF)
				{
					break;
				}
				if (!Fits(Bytes, At, 4))
				{
					return MakeError("a gear animation's event list runs past its file");
				}
				const std::size_t EventAt = Data + static_cast<std::size_t>(ReadS16(Bytes, At + 2));
				std::size_t EventIndex = File.Events.size();
				for (std::size_t Known = 0; Known < File.Events.size(); ++Known)
				{
					if (File.Events[Known].Offset == EventAt)
					{
						EventIndex = Known;
					}
				}
				if (EventIndex == File.Events.size())
				{
					TResult<FBattlingEvent> Event = ReadEvent(Bytes, EventAt);
					if (!Event)
					{
						return Event.Error();
					}
					File.Events.push_back(*Event);
				}
				File.EventLists[Number].push_back({Bytes[At], Bytes[At + 1], static_cast<uint16>(EventIndex)});
				At += 4;
			}
		}
		return File;
	}
}
