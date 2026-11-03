#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Media/GameFileIndex.h"
#include "Media/GameLibrary.h"
#include "Media/PsxExecutable.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Mods/ExpansionMods.h"
#include "Shell/ExpansionTrials.h"
#include "Shell/StoryDocuments.h"
#include "Story/StoryLibrary.h"

#include <filesystem>
#include <optional>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpansionTrialsTest, "Weltall.Shell.TheExpansionsTriedOnTheGamesDiscKeepTheirDocumentsForAnotherDisc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	std::optional<Weltall::FGameFileIndex> ReadIndex(const Weltall::FIso9660& Disc)
	{
		const auto Executable = Weltall::ReadBootExecutable(Disc);
		auto Index = Executable ? Weltall::FGameFileIndex::Read(*Executable) : Weltall::TResult<Weltall::FGameFileIndex>(Weltall::MakeError("no executable"));
		return Index ? std::optional<Weltall::FGameFileIndex>(std::move(*Index)) : std::nullopt;
	}

	/** Waits for the trial on its worker, as the game's frames would, for at most a minute. */
	bool Taken(FExpansionTrials& Trials)
	{
		for (int32 Wait = 0; Wait < 6000; ++Wait)
		{
			if (Trials.Take())
			{
				return true;
			}
			FPlatformProcess::Sleep(0.01f);
		}
		return false;
	}
}

bool FExpansionTrialsTest::RunTest(const FString&)
{
	Weltall::FGameLibrary Discs = Weltall::FGameLibrary::Scan(std::filesystem::path(*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Games"))));
	const Weltall::FIso9660* First = Discs.Find(Weltall::EDiscRelease::UsaDisc1);
	const Weltall::FIso9660* Second = Discs.Find(Weltall::EDiscRelease::UsaDisc2);
	if (First == nullptr || Second == nullptr)
	{
		AddInfo(TEXT("Skipped: the USA discs are not both in the Games folder"));
		return true;
	}
	const std::optional<Weltall::FGameFileIndex> FirstIndex = ReadIndex(*First);
	const std::optional<Weltall::FGameFileIndex> SecondIndex = ReadIndex(*Second);
	if (!TestTrue(TEXT("The discs' file indexes read"), FirstIndex && SecondIndex)) return false;

	const FString Mods = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/ExpansionTrialsMods"));
	IFileManager::Get().DeleteDirectory(*Mods, false, true);
	ON_SCOPE_EXIT
	{
		IFileManager::Get().DeleteDirectory(*Mods, false, true);
	};
	if (!TestEqual(TEXT("The example scenes are copied into a Mods folder"), static_cast<int32>(CopyExampleScenes(Mods, false)),
		static_cast<int32>(Weltall::EExampleScenesCopy::Copied))) return false;
	const auto Own = ReadStoryDocuments();
	if (!TestTrue(TEXT("The port's own documents read"), Own.HasValue())) return false;
	Weltall::FStoryLibrary Library(*Own, Weltall::FindExpansionMods(std::filesystem::path(*Mods)));
	if (!TestEqual(TEXT("The examples are an expansion"), static_cast<int32>(Library.Expansions().size()), 1)) return false;
	const Weltall::FStoryLibrary::FExpansion& Examples = Library.Expansions().front();

	// On the second disc alone, the Mass Driver's three maps of their own are built on maps only the first disc holds.
	FExpansionTrials Trials;
	TestFalse(TEXT("Nothing is taken before a trial starts"), Trials.Take());
	Trials.Start(Library, Second->Image(), *SecondIndex, Weltall::FEventOptions());
	if (!TestTrue(TEXT("The trial on the second disc finishes"), Taken(Trials))) return false;
	TestEqual(TEXT("and keeps the three maps of the first disc's"), static_cast<int32>(Examples.ForAnotherDisc.size()), 3);

	// Tried on the same disc again, nothing runs; on the first, which a game reads its files from once it has it, all fit.
	Trials.Start(Library, Second->Image(), *SecondIndex, Weltall::FEventOptions());
	TestFalse(TEXT("The same disc is not tried twice"), Trials.Take());
	Trials.Start(Library, First->Image(), *FirstIndex, Weltall::FEventOptions());
	if (!TestTrue(TEXT("The trial on the first disc finishes"), Taken(Trials))) return false;
	TestEqual(TEXT("and keeps none"), static_cast<int32>(Examples.ForAnotherDisc.size()), 0);
	return true;
}

#endif
