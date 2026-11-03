#pragma once

#include "CoreMinimal.h"
#include "Foundation/Result.h"
#include "Menu/MenuProgram.h"
#include "Story/StoryPacks.h"

/**
 * The port's story documents the game is packaged with (Content/Story): a folder of packs, each a folder of text
 * documents, read in their names' order, hidden ones left out. Android keeps them inside the app's package, where only
 * the engine's own files reach; elsewhere they are plain files.
 */
WELTALL_API Weltall::TResult<Weltall::FStoryPacks> ReadStoryDocuments();

/** Whether the game is packaged with its example scenes (Content/Examples: a mod's mod.txt and disc2 folder). */
WELTALL_API bool HasExampleScenes();

/**
 * Copies the example scenes into ModsFolder as the expansion of its folder ExamplesFolder, replacing a copy there only
 * with bReplace (the player may have changed it). Read through the engine's files, as the story documents are.
 */
WELTALL_API Weltall::EExampleScenesCopy CopyExampleScenes(const FString& ModsFolder, bool bReplace);
