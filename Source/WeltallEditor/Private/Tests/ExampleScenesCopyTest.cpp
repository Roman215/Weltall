#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Mods/ExpansionMods.h"
#include "Shell/StoryDocuments.h"
#include "Story/StoryLibrary.h"

#include <filesystem>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExampleScenesCopyTest, "Weltall.Shell.TheExampleScenesCopyIntoModsAsAnExpansionThatPlays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	int32 Copy(const FString& Mods, bool bReplace)
	{
		return static_cast<int32>(CopyExampleScenes(Mods, bReplace));
	}

	constexpr int32 Copied = static_cast<int32>(Weltall::EExampleScenesCopy::Copied);
	constexpr int32 AlreadyThere = static_cast<int32>(Weltall::EExampleScenesCopy::AlreadyThere);
}

bool FExampleScenesCopyTest::RunTest(const FString&)
{
	const FString Mods = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/ExampleScenesMods"));
	IFileManager::Get().DeleteDirectory(*Mods, false, true);
	ON_SCOPE_EXIT
	{
		IFileManager::Get().DeleteDirectory(*Mods, false, true);
	};
	if (!TestTrue(TEXT("The project has its example scenes"), HasExampleScenes())) return false;
	TestEqual(TEXT("They are copied into a Mods folder that does not exist yet"), Copy(Mods, false), Copied);

	// Read back as the game reads the Mods folder: an expansion of the examples' folder, with their documents.
	const auto Read = ReadStoryDocuments();
	if (!TestTrue(TEXT("The port's own documents read"), Read.HasValue())) return false;
	const std::vector<Weltall::FExpansionMod> Found = Weltall::FindExpansionMods(std::filesystem::path(*Mods));
	if (!TestEqual(TEXT("The copy is one expansion"), static_cast<int32>(Found.size()), 1)) return false;
	TestEqual(TEXT("in the examples' folder"), FString(UTF8_TO_TCHAR(Found.front().Folder.c_str())), FString(UTF8_TO_TCHAR(std::string(Weltall::ExamplesFolder).c_str())));
	TestEqual(TEXT("named by its mod.txt"), FString(UTF8_TO_TCHAR(Found.front().Info.Name.c_str())), TEXT("Disc 2 examples"));
	const Weltall::FStoryLibrary Library(*Read, Found);
	const Weltall::FStoryLibrary::FExpansion* Examples = Library.Expansion(Weltall::ExamplesFolder);
	if (!TestNotNull(TEXT("It can be played"), Examples)) return false;
	TestTrue(TEXT("with the way back of its own"), Examples->Packs->WayBack() != nullptr);

	// A copy there already is kept, the player's changes with it, unless the copy is to replace it.
	const FString Changed = Mods / UTF8_TO_TCHAR(std::string(Weltall::ExamplesFolder).c_str()) / TEXT("disc2/notes.txt");
	TestTrue(TEXT("A file of the player's is added to the copy"), FFileHelper::SaveStringToFile(TEXT("Mine."), *Changed));
	TestEqual(TEXT("A second copy finds the first"), Copy(Mods, false), AlreadyThere);
	TestTrue(TEXT("and leaves it as it is"), IFileManager::Get().FileExists(*Changed));
	TestEqual(TEXT("A copy that replaces it"), Copy(Mods, true), Copied);
	TestFalse(TEXT("leaves nothing of the player's"), IFileManager::Get().FileExists(*Changed));
	TestEqual(TEXT("and the same expansion"), static_cast<int32>(Weltall::FindExpansionMods(std::filesystem::path(*Mods)).size()), 1);
	return true;
}

#endif
