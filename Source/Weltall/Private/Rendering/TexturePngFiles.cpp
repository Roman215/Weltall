#include "Rendering/TexturePngFiles.h"

#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WeltallLog.h"

bool IsSafeTexturePath(const FString& File, const std::filesystem::path& Root)
{
	std::error_code Error;
	auto Requested = std::filesystem::absolute(std::filesystem::path(*File), Error).lexically_normal();
	if (Error || Root.empty()) return false;
	if (!Requested.has_filename()) Requested = Requested.parent_path();
	const auto Current = std::filesystem::weakly_canonical(Root, Error);
	if (Error || Current != Root) return false;
	const auto Target = std::filesystem::weakly_canonical(Requested, Error);
	if (Error) return false;
	const auto Relative = Target.lexically_relative(Root);
	if (Relative.empty() || Relative.is_absolute() || *Relative.begin() == "..") return false;
	if (Relative == ".") return true;
	// Root may be reached through links (a drive's alias, a linked home folder); nothing below it may be one. Those are
	// the requested path's last parts, as many as the target lies below Root.
	auto Part = Requested;
	for (auto Below = Relative.begin(); Below != Relative.end(); ++Below, Part = Part.parent_path())
	{
		const auto Status = std::filesystem::symlink_status(Part, Error);
		if (Error == std::errc::no_such_file_or_directory) { Error.clear(); continue; }
		if (Error || std::filesystem::is_symlink(Status)) return false;
	}
	return true;
}

std::filesystem::path CanonicalTextureRoot(const FString& Root)
{
	std::error_code Error;
	auto Canonical = std::filesystem::weakly_canonical(std::filesystem::path(*Root), Error);
	return Error ? std::filesystem::path() : Canonical;
}

bool WriteTexturePng(IImageWrapperModule& Module, const FString& File, const std::filesystem::path& Root, const uint8* Rgba, int32 Width, int32 Height,
	bool bFolderSafe)
{
	const FString Partial = File + TEXT(".partial");
	if (!bFolderSafe && !IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true))
	{
		UE_LOG(LogWeltall, Warning, TEXT("Cannot create texture folder: %s"), *FPaths::GetPath(File));
		return false;
	}
	const auto IsLink = [](const FString& Name)
	{
		std::error_code Error;
		return std::filesystem::is_symlink(std::filesystem::symlink_status(std::filesystem::path(*Name), Error));
	};
	if (bFolderSafe ? IsLink(File) || IsLink(Partial) : !IsSafeTexturePath(File, Root) || !IsSafeTexturePath(Partial, Root))
	{
		UE_LOG(LogWeltall, Warning, TEXT("Texture path contains a link or leaves its assets folder: %s"), *File);
		return false;
	}
	if (IFileManager::Get().FileExists(*File)) return true;
	const auto Png = Module.CreateImageWrapper(EImageFormat::PNG);
	if (!Png || !Png->SetRaw(Rgba, static_cast<int64>(Width) * Height * 4, Width, Height, ERGBFormat::RGBA, 8)) return false;
	if (!FFileHelper::SaveArrayToFile(Png->GetCompressed(), *Partial) || !IFileManager::Get().Move(*File, *Partial, false))
	{
		IFileManager::Get().Delete(*Partial);
		if (IFileManager::Get().FileExists(*File)) return true;
		UE_LOG(LogWeltall, Warning, TEXT("Cannot write texture: %s"), *File);
		return false;
	}
	return true;
}
