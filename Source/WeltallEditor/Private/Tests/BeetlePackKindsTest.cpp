#include "Foundation/Crc32.h"
#include "Graphics/BeetleTextureNames.h"
#include "Graphics/TextureRegion.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "Rendering/TextureModLibrary.h"
#include "Rendering/VramTexture.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeetlePackKindsTest, "Weltall.Rendering.BeetlePacksLeaveOutTheKindsOfPictureThePlayerHides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBeetlePackKindsTest::RunTest(const FString&)
{
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/BeetlePackKinds"));
	IFileManager::Get().DeleteDirectory(*Folder, false, true);
	IFileManager::Get().MakeDirectory(*Folder, true);
	ON_SCOPE_EXIT
	{
		FTextureModLibrary::Configure(Folder, false, false);
		IFileManager::Get().DeleteDirectory(*Folder, false, true);
	};
	// A line of typed text: a 4-bit upload of 4 x 8 units through a palette of 16 colours, and a pack's picture of it.
	Weltall::FVram Vram;
	Weltall::FVramImage Line{{64, 0, 4, 8}, std::vector<Weltall::uint16>(32, 0x1111)};
	Line.Kind = Weltall::EVramUploadKind::Text;
	Vram.Upload(Line);
	const Weltall::FVramImage Clut{{0, 480, 16, 1}, std::vector<Weltall::uint16>(16, 0x03E0)};
	Vram.Upload(Clut);
	Weltall::FTexturePage Page;
	Page.X = 64;
	Page.Depth = Weltall::ETextureDepth::Indexed4;
	const Weltall::FTextureRegion Region{FVramTexture::Pack(Page, {0, 480}), 0, 0, 16, 8};

	TArray<uint8> Pixels;
	Pixels.Init(255, 16 * 8 * 4);
	IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const auto Png = Module.CreateImageWrapper(EImageFormat::PNG);
	const FString Name = UTF8_TO_TCHAR(Weltall::BeetleTextureFileName({Weltall::Crc32OfUnits(Line.Units), Weltall::Crc32OfUnits(Clut.Units)}).c_str());
	const bool bSaved = Png && Png->SetRaw(Pixels.GetData(), Pixels.Num(), 16, 8, ERGBFormat::RGBA, 8)
		&& FFileHelper::SaveArrayToFile(Png->GetCompressed(), *(Folder / TEXT("Xenogears-texture-replacements") / (Name + TEXT(".png"))));
	if (!TestTrue(TEXT("The pack's picture is saved"), bSaved)) return false;

	const uint64 Key = Weltall::TextureRegionKey(Vram, Region);
	constexpr int64 Budget = 1 << 20;
	const uint8 AllButText = static_cast<uint8>(0xFF & ~(1u << static_cast<uint32>(Weltall::EVramUploadKind::Text)));
	FTextureModLibrary::Configure(Folder, true, false, AllButText);
	const FTextureModLookup Hidden = FTextureModLibrary::FindPacked(Vram, Region, Key, Budget);
	TestFalse(TEXT("With text hidden, the pack's picture of a line of text is not read"), Hidden.bPending || Hidden.Image.IsValid());

	FTextureModLibrary::Configure(Folder, true, false);
	const FTextureModLookup Shown = FTextureModLibrary::FindPacked(Vram, Region, Key, Budget);
	TestTrue(TEXT("With every kind shown, it is"), Shown.bPending || Shown.Image.IsValid());
	FTextureModLibrary::WaitForDecodes();
	return true;
}

#endif
