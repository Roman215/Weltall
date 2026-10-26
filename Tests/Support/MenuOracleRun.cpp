#include "Support/MenuOracleRun.h"

#include "Game/GameStateNames.h"
#include "Game/PsxRandom.h"
#include "Game/SaveStore.h"
#include "Game/SaveWrite.h"
#include "Input/KernelPad.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Menu/MenuResources.h"
#include "Menu/MenuSaveHeader.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Support/MenuFrameDigest.h"
#include "Support/OracleDigest.h"
#include "Timing/PlayClock.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace Weltall::Testing
{
	namespace
	{
		/** The sound the menu plays, recorded frame by frame, and the output mode it sets. */
		class FRecordingSound final : public IMenuSound
		{
		public:
			void PlaySystemEffect(uint8 Effect) override { Frame.push_back(Effect); }
			void Pause() override { ++Pauses; }
			void Resume() override { ++Resumes; }
			int32 OutputMode() const override { return Mode; }
			void SetOutputMode(int32 NewMode) override { Mode = NewMode; }

			std::vector<uint8> Frame;
			int32 Mode = 1;
			int32 Pauses = 0;
			int32 Resumes = 0;
		};

		FPadState FromReport(uint16 Report)
		{
			return {static_cast<uint16>(Report >> 8 | (Report & 0xFF) << 8)};
		}

		/** Where a save's place name is typed: drawn from the BIOS's font on a console, which the oracle has not, so left out. */
		constexpr FVramRect SaveTitleImage{0x140, 0xE0, 0x40, 0x20};
		constexpr std::string_view SaveName = "BASLUS-00664";

		/** A temporary folder of stores for one scenario, removed after it. */
		struct FScenarioStores
		{
			std::filesystem::path Path;
			std::optional<FSaveStore> Store;

			explicit FScenarioStores(const std::string& Name) : Path(std::filesystem::temp_directory_path() / ("WeltallMenu_" + Name))
			{
				std::error_code Error;
				std::filesystem::remove_all(Path, Error);
				Store.emplace(Path);
			}

			~FScenarioStores()
			{
				std::error_code Error;
				std::filesystem::remove_all(Path, Error);
			}
		};

		/** The cards' contents as the oracle digests them: each card's saves by name, each its name, its header and its data. */
		std::vector<uint8> CardBytes(const FSaveStore& Store, const FMenuResources& Resources)
		{
			std::vector<uint8> Bytes;
			for (int32 Card = 0; Card < 2; ++Card)
			{
				Bytes.insert(Bytes.end(), {'c', 'a', 'r', 'd'});
				for (int32 Slot = 0; Slot < FSaveStore::SlotsPerStore; ++Slot)
				{
					TResult<std::optional<std::vector<uint8>>> File = Store.ReadBytes(Card * FSaveStore::SlotsPerStore + Slot);
					REQUIRE(File);
					if (!*File)
					{
						continue;
					}
					Bytes.insert(Bytes.end(), SaveName.begin(), SaveName.end());
					Bytes.push_back(static_cast<uint8>('0' + Slot));
					Bytes.push_back(0);
					const std::array<uint8, SaveHeaderSize> Header = MenuSaveHeader(Resources, Slot, **File);
					Bytes.insert(Bytes.end(), Header.begin(), Header.end());
					Bytes.insert(Bytes.end(), (*File)->begin(), (*File)->end());
				}
			}
			return Bytes;
		}

		int32 RepeatedBlendKeys(const FMenuFrame& Frame)
		{
			std::vector<uint32> Keys;
			for (const FMenuPrimitive& Primitive : Frame.Primitives)
			{
				if (Primitive.BlendKey != 0)
				{
					Keys.push_back(Primitive.BlendKey);
				}
			}
			std::sort(Keys.begin(), Keys.end());
			return static_cast<int32>(Keys.end() - std::unique(Keys.begin(), Keys.end()));
		}
	}

	FMenuRun RunMenuScenario(const FIso9660& Disc, const FMenuScenario& Scenario)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		const FMesResident Resident = LoadMesResident(Disc);
		const auto Resources = FMenuResources::Load(Disc.Image(), *Index, *Executable);
		REQUIRE(Resources);
		auto NewGame = LoadNewGameState(Disc.Image(), *Index, Resident.NameTables);
		REQUIRE(NewGame);
		FMenuRun Run;
		Run.Game = std::move(*NewGame);
		FGameState& Game = Run.Game;
		Game.SetPlayTime(Scenario.PlayTime);
		Game.MenuLastCommand = Scenario.LastCommand;
		for (const auto& [Offset, Bytes] : Scenario.Pokes)
		{
			std::copy(Bytes.begin(), Bytes.end(), Game.Block().begin() + static_cast<std::ptrdiff_t>(Offset));
		}
		FPsxRandom Random(Scenario.Seed);
		FKernelPad Pad;
		FPlayClock Clock;
		FRecordingSound Sound;
		Sound.Mode = Scenario.OutputMode;
		FGameStateNames Names(Game);
		std::optional<FScenarioStores> Stores;
		if (Scenario.Cards)
		{
			// A scenario runs on the USA's discs and the French translation's, at once under ctest: each its own folder.
			Stores.emplace(std::string(Resources->IsFrench() ? "French_" : "") + Scenario.Name);
			for (int32 Card = 0; Card < 2; ++Card)
			{
				for (const FMenuCardSave& Save : (*Scenario.Cards)[static_cast<std::size_t>(Card)])
				{
					auto Saved = LoadNewGameState(Disc.Image(), *Index, Resident.NameTables);
					REQUIRE(Saved);
					for (const auto& [Offset, Bytes] : Save.Pokes)
					{
						std::copy(Bytes.begin(), Bytes.end(), Saved->Block().begin() + static_cast<std::ptrdiff_t>(Offset));
					}
					Saved->SetPlayTime(Save.PlayTime);
					const FOriginalSave Written = WriteOriginalSave(*Saved, Save.Slot, 0, Resident.NameTables, Resident.Font.LeadByte());
					REQUIRE_FALSE(Stores->Store->Copy(Card * FSaveStore::SlotsPerStore + Save.Slot, Written.Data()));
				}
			}
		}

		FMenuServices Services;
		Services.Image = &Disc.Image();
		Services.Index = &*Index;
		Services.Resources = &*Resources;
		Services.Game = &Game;
		Services.Random = &Random;
		Services.Pad = &Pad;
		Services.Clock = &Clock;
		Services.Sound = &Sound;
		Services.Text.Font = &Resident.Font;
		Services.Text.Tables = &Resident.NameTables;
		Services.Text.State = &Names;
		Services.Text.ButtonMap = MesKernelButtonMap;
		Services.Disc = Scenario.Disc;
		Services.Options = Scenario.Options;
		Services.LoadOptions = FSaveLoadOptions::Original();
		Services.Saves = Stores ? &*Stores->Store : nullptr;
		FMenuProgram Menu(Services, {Scenario.Mode, Scenario.Param, Scenario.bGearLocked});

		std::vector<uint16> Script;
		for (const auto& [Buttons, Count] : Scenario.Pad)
		{
			Script.insert(Script.end(), static_cast<std::size_t>(Count), Buttons);
		}
		// One frame past the scenario's last, whose code the oracle runs up to its vertical blank: what the
		// menu leaves is compared after it.
		for (int32 Frame = 0; Frame <= Scenario.Frames; ++Frame)
		{
			const uint16 Held = Script.empty() ? 0 : Script[std::min(static_cast<std::size_t>(Frame), Script.size() - 1)];
			FRecordedUploads Uploads;
			Sound.Frame.clear();
			const TResult<bool> Drawn = Menu.Frame(FromReport(Held), &Uploads);
			if (!Drawn)
			{
				if (Scenario.bStopsAtError)
				{
					Run.Error = Drawn.Error().Message;
					break;
				}
				FAIL(Drawn.Error().Message);
			}
			if (!*Drawn)
			{
				break;
			}
			if (Frame < Scenario.Frames)
			{
				std::vector<uint8> Gear;
				if (Menu.GearModel() != nullptr && Menu.Snapshot().Gear.bShown)
				{
					Gear = GearShopFrameBytes(Menu.Snapshot(), *Menu.GearModel());
				}
				std::erase_if(Uploads.Images, [](const FVramImage& Image) { return Image.Rect == SaveTitleImage; });
				Run.Frames.push_back(MenuFrameDigest(Menu.Snapshot(), Sound.Frame, Uploads.Images, Gear));
				if (Frame == Scenario.KeptFrame)
				{
					Run.KeptBytes = MenuFrameBytes(Menu.Snapshot(), Sound.Frame, Uploads.Images, Gear);
				}
				Run.RepeatedKeys += RepeatedBlendKeys(Menu.Snapshot());
				for (const FMenuPrimitive& Primitive : Menu.Snapshot().Primitives)
				{
					if (Primitive.Code == MenuGpu::GouraudQuad && Primitive.Corners[0].Color == FDrawColor{0, 0xFF, 0})
					{
						Run.LongestLearningBar = std::max(Run.LongestLearningBar, Primitive.Corners[1].X - Primitive.Corners[0].X);
					}
				}
			}
		}
		FOracleDigest Leaves;
		Leaves.Raw(Game.Block());
		Leaves.I32(Game.PlayTime(), Random.Seed());
		Leaves.U8(Game.ProgramResult, Game.MenuLastCommand, Sound.Mode);
		if (Stores)
		{
			Leaves.Raw(CardBytes(*Stores->Store, *Resources));
		}
		Run.Leaves = Leaves.Hex().substr(0, 8);
		return Run;
	}

	std::string JoinedDigests(const std::vector<std::string>& Digests)
	{
		std::string Text;
		for (std::size_t First = 0; First < Digests.size();)
		{
			std::size_t End = First + 1;
			while (End < Digests.size() && Digests[End] == Digests[First])
			{
				++End;
			}
			Text += (Text.empty() ? "" : " ") + Digests[First] + (End - First > 1 ? "*" + std::to_string(End - First) : "");
			First = End;
		}
		return Text;
	}

	void CheckMenuScenario(const FMenuScenario& Scenario, const char* ExpectedLeaves, const char* ExpectedFrames)
	{
		REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
		const FMenuRun Run = RunMenuScenario(*Disc, Scenario);
		CHECK(JoinedDigests(Run.Frames) == ExpectedFrames);
		CHECK(Run.Leaves == ExpectedLeaves);
		CHECK(Run.RepeatedKeys == 0);
	}

	void CheckFrenchMenuScenario(const FMenuScenario& Scenario, const char* ExpectedLeaves, const char* ExpectedFrames)
	{
		REQUIRE_PATCHED_DISC(Disc, EDiscRelease::UsaDisc1);
		const FMenuRun Run = RunMenuScenario(*Disc, Scenario);
		CHECK(JoinedDigests(Run.Frames) == ExpectedFrames);
		CHECK(Run.Leaves == ExpectedLeaves);
		CHECK(Run.RepeatedKeys == 0);
	}
}
