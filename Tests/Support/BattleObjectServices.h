#pragma once

#include "Battle/Show/BattleObjectFiles.h"
#include "Battle/Show/BattleShowObjects.h"
#include "Graphics/Vram.h"
#include "Support/OracleDigest.h"

#include <set>
#include <string>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
}

namespace Weltall::Testing
{
	/** A sound effect file's id (+14h), 0 for a file too short to hold one. */
	uint16 SoundFileId(std::span<const uint8> Seds);

	/** A pack place as the objects' oracle digests it: the pack's number and the offset, (FEh, 0) for none, (FFh, 0) past its pack. */
	std::array<int32, 2> PackPlace(const FGearCodeRef& Ref);

	/**
	 * The calls the objects make outside themselves, answered and recorded as the objects' oracle
	 * stubs them in the original: the disc's files read whole,
	 * the sound files registered in a set, each call an event of its name and values; the images go through
	 * Vram when a case keeps one (the stage's tint oracle's).
	 */
	class FRecordingObjectServices final : public IBattleObjectServices
	{
	public:
		struct FEvent
		{
			std::string Name;
			std::vector<int32> Values;
		};

		FRecordingObjectServices(const FCdImage& InImage, const FGameFileIndex& InIndex, const FBattleObjectTables& InTables)
			: Image(InImage), Index(InIndex), Tables(InTables)
		{
		}

		// What a case sets.
		std::set<uint16> Registered;
		int32 Record = 1;
		bool bNumbersPending = false;
		bool bEffectLoaded = false;
		uint16 SystemSet = 0;
		uint16 EventSet = 0;
		/** The kernel's words the signalled sequences set and clear: the tasks made, and whether they are counted. */
		uint32 TasksMade = 0;
		uint32 TasksCounted = 0;
		int32 SpecialSteps = 0;
		std::vector<FEvent> Events;
		FVram* Vram = nullptr;

		int32 PlayRecord() override { return Record; }
		void ShowNumbers() override { Add("numbers", {}); }
		bool NumbersPending() override { return bNumbersPending; }
		void StopCountingTasks() override;
		void SignalEventPerform(int32 Slot) override { Add("signal", {Slot & 0xFF}); }
		void CountModelRecord() override { Add("record", {}); }
		void BeginEventPerformance(int32 Slot, int32 Target) override;
		void SpawnEffect(const FBattleBoneEffect& Effect) override;
		void ObjectsSwapped(int32, int32) override {}
		void RequestEffect(int16 Effect) override { Add("effect", {Effect}); }
		bool EffectLoaded() override { return bEffectLoaded; }
		bool CountSpecialModel() override { return ++SpecialSteps == 2; }
		void TakeEffectResource(const FBattleSequenceExtension& Extension) override;
		bool SoundEffectsRegistered(uint16 FileId) override { return Registered.contains(FileId); }
		void RegisterSoundEffects(std::span<const uint8> Seds) override;
		void UnregisterSoundEffects(std::span<const uint8> Seds) override;
		uint32 SystemSoundBase() override { return static_cast<uint32>(SystemSet) << 16; }
		uint32 EventSoundBase() override { return static_cast<uint32>(EventSet) << 16; }
		void PlaySound(uint32 Sound) override { Add("play", {static_cast<int32>(Sound)}); }
		void SetSoundVolume(uint32 Sound, int32 Volume) override { Add("volume", {static_cast<int32>(Sound), static_cast<int16>(Volume)}); }
		void FadeSound(uint32 Sound, int32 Volume, int32 Steps) override { Add("fade", {static_cast<int32>(Sound), Volume, Steps}); }
		void UploadImage(const FGearTextureBlock& Block, int16 X, int16 Y) override;
		void UploadToPortRows(const FGearTextureBlock& Block, int16 X, int16 Y) override;
		void MoveImage(const FVramRect& Rect, int32 X, int32 Y) override { Add("move", {Rect.X, Rect.Y, Rect.Width, Rect.Height, X, Y}); }
		void StoreImage(const FVramRect& Rect, std::span<uint16> Into) override;
		void LoadImage(const FVramRect& Rect, std::span<const uint16> From) override;
		TResult<FBattleModelFiles> ReadGear(int32 Gear, int32 Variant) override;
		TResult<FBattleModelFiles> ReadSpecialModel(int32 Model) override;
		TResult<FBattleSequenceExtension> ReadExtension(int32 File) override;
		TResult<FBattleModelFiles> ReadSceneModel(int32 Scene, int32 Entry) override;
		void AbortReads() override { Add("abort", {}); }
		void CreateProxy(int32) override {}

	private:
		void Add(const char* Name, std::vector<int32> Values) { Events.push_back({Name, std::move(Values)}); }

		const FCdImage& Image;
		const FGameFileIndex& Index;
		const FBattleObjectTables& Tables;
	};
}
