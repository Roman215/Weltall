#include "Rendering/TextureModLibrary.h"

#include "Graphics/TextureRegion.h"
#include "Graphics/TextureSheet.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"

#include <filesystem>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray64<uint8> SolidPng(int32 Width, int32 Height, uint8 Blue = 255)
	{
		IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		const auto Png = Module.CreateImageWrapper(EImageFormat::PNG);
		TArray<uint8> Pixels;
		Pixels.Init(255, Width * Height * 4);
		for (int32 Offset = 0; Offset < Pixels.Num(); Offset += 4) Pixels[Offset + 2] = Blue;
		if (!Png || !Png->SetRaw(Pixels.GetData(), Pixels.Num(), Width, Height, ERGBFormat::RGBA, 8)) return {};
		return Png->GetCompressed();
	}

	FString KeyName(uint64 Key)
	{
		return UTF8_TO_TCHAR(Weltall::AssetKeyName(Key).c_str());
	}

	Weltall::FImageRgba8 Picture(int32 Width, int32 Height)
	{
		return {Width, Height, std::vector<uint8>(static_cast<std::size_t>(Width * Height * 4), 200)};
	}

	/** Key's lookup once its reading, if one starts, has arrived. */
	FTextureModLookup Read(uint64 Key, int32 Width, int32 Height, int64 MaxPixels = 1 << 20)
	{
		const FTextureModLookup Lookup = FTextureModLibrary::Find(Key, Width, Height, MaxPixels);
		if (!Lookup.bPending) return Lookup;
		FTextureModLibrary::WaitForDecodes();
		return FTextureModLibrary::Find(Key, Width, Height, MaxPixels);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureModLibraryTest, "Weltall.Rendering.TextureCapturesAndHighResolutionReplacements",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTextureModLibraryTest::RunTest(const FString&)
{
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/TextureModLibrary"));
	IFileManager::Get().DeleteDirectory(*Folder, false, true);
	ON_SCOPE_EXIT { FTextureModLibrary::Configure(Folder, false, false); IFileManager::Get().DeleteDirectory(*Folder, false, true); };
	constexpr uint64 Key = 0x0123456789ABCDEFull;
	const FString Captured = Folder / FTextureModLibrary::OriginalsFolder / TEXT("captured") / (KeyName(Key) + TEXT(".png"));
	FTextureModLibrary::Configure(Folder, false, true);
	TestTrue(TEXT("A new picture needs capturing"), FTextureModLibrary::NeedsCapture(Key));
	FTextureModLibrary::Capture(Key, Picture(4, 2));
	TestFalse(TEXT("A queued picture is not captured again"), FTextureModLibrary::NeedsCapture(Key));
	TestFalse(TEXT("Capture alone installs no replacement"), Read(Key, 4, 2).Image.IsValid());
	FTextureModLibrary::WaitForCaptures();
	TestTrue(TEXT("The original PNG is named by its key under the originals"), IFileManager::Get().FileExists(*Captured));

	// An edited copy may sit anywhere outside the originals, with a description before its key.
	const FString Edited = Folder / TEXT("village") / (TEXT("door-") + KeyName(Key) + TEXT(".png"));
	if (!FFileHelper::SaveArrayToFile(SolidPng(8, 4), *Edited)) return false;
	const uint64 Stretched = Key + 1;
	if (!FFileHelper::SaveArrayToFile(SolidPng(6, 5), *(Folder / (KeyName(Stretched) + TEXT(".png"))))) return false;
	const uint64 TooLarge = Key + 4;
	if (!FFileHelper::SaveArrayToFile(SolidPng(65, 2), *(Folder / (KeyName(TooLarge) + TEXT(".png"))))) return false;
	const uint64 OriginalOnly = Key + 2;
	if (!FFileHelper::SaveArrayToFile(SolidPng(4, 2), *(Folder / FTextureModLibrary::OriginalsFolder / TEXT("field") / (KeyName(OriginalOnly) + TEXT(".png"))))) return false;
	// Hidden files and folders are left out: macOS's companion of a copied file ("._" and its name, an AppleDouble
	// header and no picture) sorts before the picture it stands beside.
	TArray<uint8> Companion;
	Companion.Init(0, 4096);
	const uint8 AppleDouble[] = {0x00, 0x05, 0x16, 0x07, 0x00, 0x02, 0x00, 0x00};
	FMemory::Memcpy(Companion.GetData(), AppleDouble, sizeof(AppleDouble));
	if (!FFileHelper::SaveArrayToFile(Companion, *(Folder / TEXT("village") / (TEXT("._door-") + KeyName(Key) + TEXT(".png"))))) return false;
	const uint64 Hidden = Key + 3;
	if (!FFileHelper::SaveArrayToFile(SolidPng(4, 2), *(Folder / TEXT(".backup") / (KeyName(Hidden) + TEXT(".png"))))) return false;
	FTextureModLibrary::Configure(Folder, true, false);
	TestTrue(TEXT("A replacement is read in the background"), FTextureModLibrary::Find(Key, 4, 2, 1 << 20).bPending);
	TestTrue(TEXT("Asked again before it arrives, it is still being read"), FTextureModLibrary::Find(Key, 4, 2, 1 << 20).bPending);
	const auto Replacement = Read(Key, 4, 2).Image;
	TestTrue(TEXT("Replacement found in a subfolder by the key its name ends with"), Replacement.IsValid());
	if (Replacement)
	{
		TestEqual(TEXT("Twice the region's width retained"), Replacement->Width, 8);
		TestEqual(TEXT("Twice the region's height retained"), Replacement->Height, 4);
		TestEqual(TEXT("PNG RGBA data retained"), Replacement->Rgba[2], static_cast<uint8>(255));
		const FTextureModLookup Again = FTextureModLibrary::Find(Key, 4, 2, 1 << 20);
		TestTrue(TEXT("Cached PNG shares its storage"), Again.Image == Replacement && !Again.bPending);
	}
	const auto Other = Read(Stretched, 4, 2).Image;
	TestTrue(TEXT("A PNG of another size and shape than its region is taken, to be stretched over it"), Other.IsValid() && Other->Width == 6 && Other->Height == 5);
	TestFalse(TEXT("A PNG more than 16 times its region's size either way is refused"), Read(TooLarge, 4, 2).Image.IsValid());
	TestFalse(TEXT("A refused PNG is not read again"), FTextureModLibrary::Find(TooLarge, 4, 2, 1 << 20).bPending);
	TestFalse(TEXT("Originals are never read as replacements"), Read(OriginalOnly, 4, 2).Image.IsValid());
	TestFalse(TEXT("A picture in a hidden folder is not read"), Read(Hidden, 4, 2).Image.IsValid());

	// A sheet: a layout beside a PNG twice its size, its second picture a texel to the right of its first.
	Weltall::FTextureSheetLayout Layout{9, 4, {{0x5100, 0, 0, 4, 2}, {0x5200, 5, 0, 4, 4}}};
	IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const auto SheetPng = Module.CreateImageWrapper(EImageFormat::PNG);
	TArray<uint8> SheetPixels;
	SheetPixels.Init(0, 18 * 8 * 4);
	SheetPixels[(0 * 18 + 10) * 4] = 77;
	if (!SheetPng || !SheetPng->SetRaw(SheetPixels.GetData(), SheetPixels.Num(), 18, 8, ERGBFormat::RGBA, 8)) return false;
	if (!FFileHelper::SaveArrayToFile(SheetPng->GetCompressed(), *(Folder / TEXT("party/cells-0.png")))) return false;
	if (!FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(Weltall::WriteTextureSheetLayout(Layout).c_str()), *(Folder / TEXT("party/cells-0.tsv")))) return false;
	FTextureModLibrary::Configure(Folder, false, false);
	FTextureModLibrary::Configure(Folder, true, false);
	const uint64 Started = FTextureModLibrary::DecodesStarted();
	TestTrue(TEXT("A sheet's picture is read in the background"), FTextureModLibrary::Find(0x5200, 4, 4, 1 << 20).bPending);
	TestTrue(TEXT("Another picture of a sheet being read waits for the same reading"), FTextureModLibrary::Find(0x5100, 4, 2, 1 << 20).bPending);
	TestEqual(TEXT("A sheet is read once for all its pictures"), FTextureModLibrary::DecodesStarted() - Started, static_cast<uint64>(1));
	FTextureModLibrary::WaitForDecodes();
	const auto OnSheet = FTextureModLibrary::Find(0x5200, 4, 4, 1 << 20).Image;
	TestTrue(TEXT("A sheet's picture is found by its key"), OnSheet.IsValid());
	if (OnSheet)
	{
		TestEqual(TEXT("The sheet's scale carries to its picture"), OnSheet->Width, 8);
		TestEqual(TEXT("The picture is cut from its place on the sheet"), OnSheet->Rgba[0], static_cast<uint8>(77));
	}
	TestTrue(TEXT("The sheet's other picture comes with it"), FTextureModLibrary::Find(0x5100, 4, 2, 1 << 20).Image.IsValid());
	FTextureModLibrary::Configure(Folder, false, false);
	FTextureModLibrary::Configure(Folder, true, false);
	TestFalse(TEXT("A sheet's picture its layout sizes otherwise than its region is refused"), Read(0x5100, 4, 4).Image.IsValid());

	// The same sheet at one and a half times its layout's height and fourteen ninths of its width: each picture is cut
	// at its edges' nearest pixels.
	const auto WiderPng = Module.CreateImageWrapper(EImageFormat::PNG);
	TArray<uint8> WiderPixels;
	WiderPixels.Init(0, 14 * 6 * 4);
	WiderPixels[(0 * 14 + 8) * 4] = 99;
	if (!WiderPng || !WiderPng->SetRaw(WiderPixels.GetData(), WiderPixels.Num(), 14, 6, ERGBFormat::RGBA, 8)) return false;
	FTextureModLibrary::Configure(Folder, false, false);
	if (!FFileHelper::SaveArrayToFile(WiderPng->GetCompressed(), *(Folder / TEXT("party/cells-0.png")))) return false;
	FTextureModLibrary::Configure(Folder, true, false);
	const auto Cut = Read(0x5200, 4, 4).Image;
	const auto Beside = FTextureModLibrary::Find(0x5100, 4, 2, 1 << 20).Image;
	if (TestTrue(TEXT("A sheet of no whole multiple of its layout is read"), Cut.IsValid() && Beside.IsValid()))
	{
		TestEqual(TEXT("A picture runs from its left edge's nearest pixel to its right edge's"), Cut->Width, 6);
		TestEqual(TEXT("A picture runs from its top edge's nearest pixel to its bottom edge's"), Cut->Height, 6);
		TestEqual(TEXT("The picture is cut at its left edge's nearest pixel"), Cut->Rgba[0], static_cast<uint8>(99));
		TestEqual(TEXT("The first picture is cut to its own edges"), Beside->Width * 100 + Beside->Height, 6 * 100 + 3);
	}

	FTextureModLibrary::Configure(Folder, false, true);
	TestFalse(TEXT("An exported original is not captured again"), FTextureModLibrary::NeedsCapture(OriginalOnly));
	FTextureModLibrary::Configure(Folder, false, false);
	TestFalse(TEXT("Disabled library is inactive"), FTextureModLibrary::IsActive());
	TestFalse(TEXT("Disabled library returns no replacement"), Read(Key, 4, 2).Image.IsValid());

	constexpr uint64 Pending = Key + 3;
	FTextureModLibrary::Configure(Folder, false, true);
	FTextureModLibrary::Capture(Pending, Picture(2, 2));
	FTextureModLibrary::Configure(Folder + TEXT("-other"), false, false);
	FTextureModLibrary::WaitForCaptures();
	TestTrue(TEXT("A queued capture keeps its original folder across a settings change"),
		IFileManager::Get().FileExists(*(Folder / FTextureModLibrary::OriginalsFolder / TEXT("captured") / (KeyName(Pending) + TEXT(".png")))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureModBudgetTest, "Weltall.Rendering.TextureBudgetsKeepPinnedImagesAndDeferDecoding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTextureModBudgetTest::RunTest(const FString&)
{
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/TextureModBudget"));
	IFileManager::Get().DeleteDirectory(*Folder, false, true);
	IFileManager::Get().MakeDirectory(*Folder, true);
	ON_SCOPE_EXIT { FTextureModLibrary::Configure(Folder, false, false); IFileManager::Get().DeleteDirectory(*Folder, false, true); };
	const auto Small = SolidPng(512, 512);
	const auto Large = SolidPng(4096, 4096);
	if (Small.IsEmpty() || Large.IsEmpty()) return false;
	constexpr uint64 First = 0x1000;
	constexpr uint64 Deferred = 0x2000;
	if (!FFileHelper::SaveArrayToFile(Small, *(Folder / (KeyName(First) + TEXT(".png"))))) return false;
	const FString DeferredFile = Folder / (KeyName(Deferred) + TEXT(".png"));
	if (!FFileHelper::SaveArrayToFile(Small, *DeferredFile)) return false;
	for (uint64 Number = 0; Number < 4; ++Number)
	{
		if (!FFileHelper::SaveArrayToFile(Large, *(Folder / (KeyName(0x3000 + Number) + TEXT(".png"))))) return false;
	}
	FTextureModLibrary::Configure(Folder, true, false);
	const auto Pinned = Read(First, 256, 256).Image;
	TestTrue(TEXT("First image loaded"), Pinned.IsValid());
	if (!Pinned) return false;

	const FTextureModLookup OverBudget = Read(Deferred, 256, 256, 512 * 511);
	TestFalse(TEXT("A PNG larger than the budget is not decoded"), OverBudget.Image.IsValid());
	TestTrue(TEXT("Deferred PNG reports its budget limit"), OverBudget.bOverBudget);
	FFileHelper::SaveStringToFile(TEXT("Not a PNG"), *DeferredFile);
	const FTextureModLookup Again = FTextureModLibrary::Find(Deferred, 256, 256, 512 * 511);
	TestFalse(TEXT("Repeated budget miss remains deferred"), Again.Image.IsValid() || Again.bPending);
	TestTrue(TEXT("Cached dimensions avoid rereading the file during budget misses"), Again.bOverBudget);
	if (!FFileHelper::SaveArrayToFile(Small, *DeferredFile)) return false;
	TestTrue(TEXT("A later sufficient budget can decode the PNG"), Read(Deferred, 256, 256, 512 * 512).Image.IsValid());

	// Asked for together, the large pictures are read side by side.
	for (uint64 Number = 0; Number < 4; ++Number) FTextureModLibrary::Find(0x3000 + Number, 256, 256, 4096 * 4096);
	FTextureModLibrary::WaitForDecodes();
	for (uint64 Number = 0; Number < 4; ++Number)
	{
		TestTrue(TEXT("Large PNG fills decoded cache"), FTextureModLibrary::Find(0x3000 + Number, 256, 256, 4096 * 4096).Image.IsValid());
	}
	TestTrue(TEXT("CPU eviction retains identity while a renderer pins the image"), FTextureModLibrary::Find(First, 256, 256, 1 << 20).Image == Pinned);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureModLinkTest, "Weltall.Rendering.TextureCaptureRefusesPartialFileLinks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTextureModLinkTest::RunTest(const FString&)
{
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/TextureModLinks"));
	const FString CapturedFolder = Folder / FTextureModLibrary::OriginalsFolder / TEXT("captured");
	IFileManager::Get().DeleteDirectory(*Folder, false, true);
	IFileManager::Get().MakeDirectory(*CapturedFolder, true);
	ON_SCOPE_EXIT { FTextureModLibrary::Configure(Folder, false, false); IFileManager::Get().DeleteDirectory(*Folder, false, true); };
	constexpr uint64 Key = 0x42;
	const FString Victim = Folder / TEXT("existing-mod.png");
	const FString Partial = CapturedFolder / (KeyName(Key) + TEXT(".png.partial"));
	const FString Original = TEXT("Existing mod contents must survive a capture.");
	if (!FFileHelper::SaveStringToFile(Original, *Victim)) return false;
	std::error_code Error;
	std::filesystem::create_symlink(std::filesystem::path(*Victim), std::filesystem::path(*Partial), Error);
	if (Error)
	{
		AddInfo(TEXT("Symbolic-link creation is unavailable; link-specific check skipped."));
		return true;
	}
	FTextureModLibrary::Configure(Folder, false, true);
	FTextureModLibrary::Capture(Key, Picture(2, 2));
	FTextureModLibrary::WaitForCaptures();
	FString After;
	FFileHelper::LoadFileToString(After, *Victim);
	TestEqual(TEXT("A partial link cannot truncate an existing mod inside the texture folder"), After, Original);
	TestFalse(TEXT("Rejected capture was not published"), IFileManager::Get().FileExists(*(CapturedFolder / (KeyName(Key) + TEXT(".png")))));
	std::filesystem::remove(std::filesystem::path(*Partial), Error);
	return true;
}

#endif
