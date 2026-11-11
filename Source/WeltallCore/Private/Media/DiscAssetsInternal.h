#pragma once

#include "Media/DiscAssets.h"

#include <fstream>
#include <map>

namespace Weltall
{
	struct FDiscAssetEntry
	{
		FIndexRecord Record;
		std::filesystem::path Path;
		uint64 Bytes = 0;
		bool bMovie = false;
	};

	namespace DiscAssets
	{
		/** Returns false when an optional override's root is unavailable. Export always requires a valid root. */
		TResult<bool> SafePath(const std::filesystem::path& Root, const std::filesystem::path& Relative, bool bOptionalOverride = false);
		TResult<bool> PrepareFolder(const std::filesystem::path& Root, const std::filesystem::path& Relative);
		TResult<bool> WriteManifest(const std::filesystem::path& Root, const std::string& Manifest);
		TResult<bool> ValidateManifest(const std::filesystem::path& Root, const std::string& Manifest);
		TResult<bool> Publish(const std::filesystem::path& Partial, const std::filesystem::path& Target);
	}

	struct FDiscAssetStore::FState
	{
		FState(std::filesystem::path InFolder, const FCdImage& InImage) : Folder(std::move(InFolder)), Image(InImage)
		{
			Image.UseDiscAlone();
		}
		std::filesystem::path Folder;
		FCdImage Image;
		std::vector<FDiscAssetEntry> Entries;
		std::map<uint32, FDiscAssetEntry> Movies;
		std::string Manifest;
		bool bEnabled = false;
		std::shared_ptr<const FMusicReplacements> Music;
		std::shared_ptr<const FMessageOverrides> Messages;
		std::shared_ptr<const FMesFontSheet> FontSheet;
		mutable std::map<uint32, std::shared_ptr<const IByteSource>> MovieFiles;
	};

	struct FDiscAssetExport::FState
	{
		FState(std::filesystem::path InFolder, const FCdImage& InImage, std::vector<FDiscAssetEntry> InEntries)
			: Folder(std::move(InFolder)), Image(InImage), Entries(std::move(InEntries)) {}
		std::filesystem::path Folder;
		FCdImage Image;
		std::vector<FDiscAssetEntry> Entries;
		FDiscAssetExportStatus Progress;
		std::size_t Next = 0;
		uint64 Written = 0;
		std::filesystem::path Partial;
		std::ofstream Output;
	};
}
