#include "Story/StoryLibrary.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	FStoryLibrary::FStoryLibrary(FStoryPacks Own, std::span<const FExpansionMod> Mods) : OwnPacks(std::move(Own))
	{
		for (const FExpansionMod& Mod : Mods)
		{
			FExpansion Expansion{Mod, OwnPacks, std::nullopt};
			if (Mod.Folder.size() > ExpansionFolderBytes)
			{
				Expansion.Packs.reset();
				Expansion.Problem = MakeError("its folder's name is longer than a save can keep (" + std::to_string(ExpansionFolderBytes) + " bytes)");
			}
			else if (std::optional<FError> Error = Expansion.Packs->ReadExpansion(Mod.Documents, Mod.Folder))
			{
				Expansion.Packs.reset();
				Expansion.Problem = std::move(Error);
			}
			else if (!Expansion.Packs->HasExpansion())
			{
				Expansion.bUnchanged = Expansion.Packs->LeftAsExported() > OwnPacks.LeftAsExported();
				Expansion.Packs.reset();
				Expansion.Problem = MakeError(Expansion.bUnchanged ? "its documents are as Export Disc 2 wrote them, which changes nothing until one is edited"
					: "its disc2 folder holds no document of the second disc's story");
			}
			Found.push_back(std::move(Expansion));
		}
	}

	const FStoryLibrary::FExpansion* FStoryLibrary::Expansion(std::string_view Folder) const
	{
		const auto Named = std::find_if(Found.begin(), Found.end(), [Folder](const FExpansion& Expansion) { return Expansion.Packs && Expansion.Mod.Folder == Folder; });
		return Named != Found.end() ? &*Named : nullptr;
	}

	const FStoryLibrary::FExpansion* FStoryLibrary::Offered(const FExpansionOptions& Options) const
	{
		if (!Options.bEnabled)
		{
			return nullptr;
		}
		if (const FExpansion* Chosen = Expansion(Options.Folder))
		{
			return Chosen;
		}
		const auto First = std::find_if(Found.begin(), Found.end(), [](const FExpansion& Expansion) { return Expansion.Packs.has_value(); });
		return First != Found.end() ? &*First : nullptr;
	}

	const FStoryPacks& FStoryLibrary::For(std::string_view Folder) const
	{
		const FExpansion* Named = Expansion(Folder);
		return Named != nullptr ? *Named->Packs : OwnPacks;
	}

	void FStoryLibrary::SetForAnotherDisc(std::string_view Folder, std::vector<std::string> Documents)
	{
		const auto Named = std::find_if(Found.begin(), Found.end(), [Folder](const FExpansion& Expansion) { return Expansion.Mod.Folder == Folder; });
		if (Named != Found.end())
		{
			Named->ForAnotherDisc = std::move(Documents);
		}
	}
}
