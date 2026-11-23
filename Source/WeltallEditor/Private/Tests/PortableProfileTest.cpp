#include "Foundation/WholeFiles.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shell/PortableProfile.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FTemporaryProfile
	{
		const FString Folder = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("PortableProfile"), FGuid::NewGuid().ToString());
		~FTemporaryProfile() { IFileManager::Get().DeleteDirectory(*Folder, false, true); }
	};

	bool Put(const std::filesystem::path& Path, Weltall::uint8 Value)
	{
		return !Weltall::ReplaceWholeFile(Path, std::span(&Value, 1));
	}

	int32 Read(const std::filesystem::path& Path)
	{
		const auto Bytes = Weltall::ReadWholeFile(Path);
		return Bytes && *Bytes && (**Bytes).size() == 1 ? (**Bytes)[0] : -1;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortableProfileImportTest, "Weltall.Shell.PortableProfileImportsUserFilesOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPortableProfileImportTest::RunTest(const FString&)
{
	const FTemporaryProfile Temp;
	const std::filesystem::path Root(*Temp.Folder);
	const auto Settings = Root / "Config" / "Settings.txt";
	const auto Saves = Root / "Data" / "Saves";
	const auto State = Saves.parent_path() / "States" / "Slot 1.state";
	const auto Portable = Root / "Portable";
	if (!TestTrue(TEXT("Set up the original and portable profiles"), Put(Settings, 1) && Put(Saves / "Card1/No01.sav", 2)
		&& Put(Saves / "Cards/Export.mcr", 3) && Put(Saves / "LastSaved.txt", 4) && Put(State, 5)
		&& Put(Portable / "Saves/Card1/No01.sav", 6) && Put(Saves / "Card1/No02.sav.new", 7)))
	{
		return false;
	}
	TestFalse(TEXT("Imports the profile"), ImportPortableProfile(Portable, Settings, Saves).has_value());
	TestEqual(TEXT("Copies settings"), Read(Portable / "Settings.txt"), 1);
	TestEqual(TEXT("Keeps the portable save"), Read(Portable / "Saves/Card1/No01.sav"), 6);
	TestEqual(TEXT("Keeps the original save"), Read(Saves / "Card1/No01.sav"), 2);
	TestEqual(TEXT("Copies exported cards"), Read(Portable / "Saves/Cards/Export.mcr"), 3);
	TestEqual(TEXT("Copies the last slot selection"), Read(Portable / "Saves/LastSaved.txt"), 4);
	TestEqual(TEXT("Copies quick saves"), Read(Portable / "States/Slot 1.state"), 5);
	TestEqual(TEXT("Leaves unfinished writes out"), Read(Portable / "Saves/Card1/No02.sav.new"), -1);
	std::filesystem::remove(Portable / "States/Slot 1.state");
	TestFalse(TEXT("Reopens the portable profile"), ImportPortableProfile(Portable, Settings, Saves).has_value());
	TestEqual(TEXT("A deleted quick save stays deleted"), Read(Portable / "States/Slot 1.state"), -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortableProfileRetryTest, "Weltall.Shell.PortableProfileRetriesAnIncompleteImport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPortableProfileRetryTest::RunTest(const FString&)
{
	const FTemporaryProfile Temp;
	const std::filesystem::path Root(*Temp.Folder);
	const auto Settings = Root / "User" / "Settings.txt";
	const auto Saves = Settings.parent_path() / "Saves";
	const auto Portable = Root / "Portable";
	const auto Obstruction = Portable / "Saves";
	if (!TestTrue(TEXT("Set up a profile with a blocked save folder"), Put(Settings, 1) && Put(Saves / "Card1/No01.sav", 2)
		&& Put(Obstruction, 3)))
	{
		return false;
	}
	TestTrue(TEXT("Import reports the failure"), ImportPortableProfile(Portable, Settings, Saves).has_value());
	TestFalse(TEXT("Incomplete import has no marker"), std::filesystem::exists(Portable / ".profile-imported"));
	TestEqual(TEXT("The original save remains available"), Read(Saves / "Card1/No01.sav"), 2);
	std::filesystem::remove(Obstruction);
	TestFalse(TEXT("The next launch can retry"), ImportPortableProfile(Portable, Settings, Saves).has_value());
	TestEqual(TEXT("Copies the missed save"), Read(Portable / "Saves/Card1/No01.sav"), 2);
	return true;
}

#endif
