#pragma once

#include "CoreMinimal.h"
#include "Foundation/Result.h"
#include "Story/StoryPacks.h"

/**
 * The port's story documents the game is packaged with (Content/Story): a folder of packs, each a folder of text
 * documents, read in their names' order, hidden ones left out. Android keeps them inside the app's package, where only
 * the engine's own files reach; elsewhere they are plain files.
 */
WELTALL_API Weltall::TResult<Weltall::FStoryPacks> ReadStoryDocuments();
