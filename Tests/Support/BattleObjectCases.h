#pragma once

#include "Battle/BattleContext.h"
#include "Battle/Show/BattleObjectFiles.h"
#include "Battle/Show/BattleSceneGraphics.h"
#include "Battle/Show/BattleShowCamera.h"
#include "Battle/Show/BattleShowFloor.h"
#include "Battle/Show/BattleShowObjects.h"
#include "Battle/Show/BattleStageFiles.h"
#include "Game/PsxRandom.h"
#include "Graphics/GteMath.h"
#include "Media/GameFileIndex.h"
#include "Support/BattleObjectServices.h"
#include "Support/OracleDigest.h"
#include "Support/XorShift32.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FIso9660;
}

namespace Weltall::Testing
{
	/** The disc's files the objects' oracle cases draw from. */
	struct FBattleObjectData
	{
		const FCdImage* Image = nullptr;
		std::optional<FGameFileIndex> Index;
		FGteArctangents Arctangents;
		std::optional<FBattleObjectTables> Tables;
		std::vector<FBattleStageFiles> Stages;
		/** The scenes with a model entry, and their graphics. */
		std::vector<FBattleSceneGraphics> ModelScenes;
		std::vector<int32> ModelSceneNumbers;
	};

	bool ReadBattleObjectData(const FIso9660& Disc, FBattleObjectData& Data);

	/** A case's world: the rules' state the objects read, the camera, the floor, the GTE, the game's seed, the calls outside and the objects. */
	struct FBattleObjectCase
	{
		explicit FBattleObjectCase(const FBattleObjectData& Data);

		std::unique_ptr<FBattleContext> Context = std::make_unique<FBattleContext>();
		FBattleCamera Camera;
		FBattleFloor Floor;
		FGte Gte;
		FPsxRandom Random;
		FRecordingObjectServices Services;
		/** The show's fixes on, but Regurus unfinished as the original's: a case drawing it waits for its opening for good. */
		FBattleShowOptions ShowOptions;
		FBattleObjects Objects;
	};

	/**
	 * How a case's frames go (the objects' oracle digests their state frame by frame, the drawing oracle their
	 * ordering table): at most MostFrames of them, the state digested before the first when DigestStart is given,
	 * each frame drawn under the case's view into its draw buffer, then digested unless the drawing ended the case.
	 */
	struct FBattleObjectCaseFrames
	{
		uint32 MostFrames = 0;
		std::function<void(FBattleObjectCase&, FOracleDigest&)> DigestStart;
		std::function<void(FBattleObjectCase&, const FGteMatrix&, int32)> Draw;
		std::function<void(FBattleObjectCase&, FOracleDigest&)> DigestFrame;
	};

	/**
	 * A case's stage and scene (a scene with a model entry) in place of the ones it draws, the draws still made; with an
	 * entry of the scene, slot 3 holding it, the view aimed at the first enemy slot holding it and the case's sequences
	 * started on that slot while its object stands; Sequence, when given, started there before the first frame and in place
	 * of each one drawn (the rig's stage_choice, scene_choice, entry_choice and sequence_choice).
	 */
	struct FBattleObjectCaseChoice
	{
		std::optional<int32> Stage;
		std::optional<int32> Scene;
		std::optional<uint8> Entry;
		std::optional<int32> Sequence;
	};

	/**
	 * A case as the objects' oracle draws one: a case made from G (a stage, a scene's model enemies,
	 * party gears, sometimes a special model, a view), then its frames, each maybe starting one of an object's own
	 * sequences. Where the original reads what it does not hold, the port faults and the case ends, the digest taking
	 * a 1 there as the oracle's does; the fault's message is returned, empty when none.
	 */
	std::string RunBattleObjectCase(FXorShift32& G, const FBattleObjectData& Data, FOracleDigest& Digest, const FBattleObjectCaseFrames& Frames,
		const FBattleObjectCaseChoice& Chosen = {});
}
