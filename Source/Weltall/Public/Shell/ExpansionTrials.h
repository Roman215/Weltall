#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Field/EventContext.h"
#include "Foundation/Result.h"
#include "Story/ExpansionTrial.h"

#include <string>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FStoryLibrary;
}

/**
 * The Disc 2 expansions of a story library tried on the disc a game reads its files from (Weltall::TryExpansion), on a
 * worker so that the game never waits for it: as the game thread takes what each found, it is logged and kept in the
 * library, where the Modding page reads which expansions have documents for another disc.
 */
class WELTALL_API FExpansionTrials
{
public:
	FExpansionTrials() = default;
	FExpansionTrials(const FExpansionTrials&) = delete;
	FExpansionTrials& operator=(const FExpansionTrials&) = delete;
	/** Waits for a trial that is running: it reads the library and the disc. */
	~FExpansionTrials();

	/**
	 * Tries Library's playable expansions on Image and Index, with the player's settings Options, unless they were last
	 * tried on Image; a trial still running is waited for and taken first. Library must outlive the trial; the worker
	 * reads a copy of Image without the player's Assets, whose store is the game thread's.
	 */
	void Start(Weltall::FStoryLibrary& Library, const Weltall::FCdImage& Image, const Weltall::FGameFileIndex& Index, const Weltall::FEventOptions& Options);

	/** Takes a trial that has finished: what it found logged and kept in the library; false where none has. */
	bool Take();

private:
	struct FTried
	{
		std::string Folder;
		Weltall::TResult<Weltall::FExpansionTrial> Trial;
	};

	void Keep(std::vector<FTried> Tried);

	Weltall::FStoryLibrary* Library = nullptr;
	const Weltall::FCdImage* TriedOn = nullptr;
	TFuture<std::vector<FTried>> Running;
};
