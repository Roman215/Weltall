#include "Battle/Show/BattleShow.h"

#include "Battle/BattleContext.h"
#include "Battle/BattleEvent.h"

namespace Weltall
{
	void FBattleShow::ShowNumbers()
	{
		// The numbers of the record shown, while a playback runs.
		if (Playback.HasController())
		{
			Battle.ShowNumbers();
		}
	}

	void FBattleShow::StopCountingTasks()
	{
		Sprites.CountedTasks = 0;
		Sprites.bCountingTasks = false;
	}

	void FBattleShow::SignalEventPerform(int32 Slot)
	{
		FinishBattleEventPerform(*Context, static_cast<uint8>(Slot));
	}

	void FBattleShow::BeginEventPerformance(int32 Slot, int32 Target)
	{
		Sprites.CountedTasks = 0;
		Sprites.bCountingTasks = true;
		// The performer faces its target, which the play aims at alone.
		if (Slot >= FBattleContext::SlotCount || Target >= FBattleContext::SlotCount)
		{
			ReportFault(EBattleShowFault::Playback, "a battle event's performance names a slot past the battle's");
			return;
		}
		if (FSprite* Performer = Battle.SlotSprites[static_cast<std::size_t>(Slot)])
		{
			FSprite* Aimed = Battle.SlotSprites[static_cast<std::size_t>(Target)];
			Battle.Play.TargetMask = static_cast<uint16>(1u << (Target & 0x1F));
			Battle.Play.Actor = Performer;
			Performer->Target = Aimed;
			Battle.Play.Targets[0] = Aimed;
			Battle.Play.Targets[1] = nullptr;
		}
		Battle.Play.Record = 1;
	}

	bool FBattleShow::CountSpecialModel()
	{
		// The special model's count: the second frees the model.
		++Battle.CountedWaits;
		return ++Battle.SpecialModelSteps == 2;
	}

	bool FBattleShow::SoundEffectsRegistered(uint16 FileId)
	{
		return Audio != nullptr && Audio->HasEffects(FileId);
	}

	void FBattleShow::RegisterSoundEffects(std::span<const uint8> Seds)
	{
		if (Audio != nullptr)
		{
			Audio->RegisterEffects(Seds);
		}
	}

	void FBattleShow::UnregisterSoundEffects(std::span<const uint8> Seds)
	{
		if (Audio != nullptr)
		{
			Audio->UnregisterEffects(Seds);
		}
	}

	void FBattleShow::SetSoundVolume(uint32 Sound, int32 Volume)
	{
		if (Audio != nullptr)
		{
			Audio->SetEffectVolume(Sound, Volume);
		}
	}

	void FBattleShow::FadeSound(uint32 Sound, int32 Volume, int32 Steps)
	{
		if (Audio != nullptr)
		{
			Audio->FadeEffectVolume(Sound, Volume, Steps);
		}
	}

	void FBattleShow::UploadImage(const FGearTextureBlock& Block, int16 X, int16 Y)
	{
		UploadScratch.Rect = {X, Y, Block.Width, Block.Height};
		UploadScratch.Units.assign(Block.Pixels.begin(), Block.Pixels.end());
		Vram.Upload(UploadScratch);
	}

	void FBattleShow::UploadToPortRows(const FGearTextureBlock& Block, int16 X, int16 Y)
	{
		UploadScratch.Rect = {X, Y, Block.Width, Block.Height};
		UploadScratch.Units.assign(Block.Pixels.begin(), Block.Pixels.end());
		Vram.UploadToPortRows(UploadScratch);
	}

	void FBattleShow::StoreImage(const FVramRect& Rect, std::span<uint16> Into)
	{
		Vram.NoteReadback(Rect);
		std::size_t Unit = 0;
		for (int32 Row = 0; Row < Rect.Height; ++Row)
		{
			for (int32 Column = 0; Column < Rect.Width && Unit < Into.size(); ++Column)
			{
				Into[Unit++] = Vram.At(Rect.X + Column, Rect.Y + Row);
			}
		}
	}

	void FBattleShow::LoadImage(const FVramRect& Rect, std::span<const uint16> From)
	{
		UploadScratch.Rect = Rect;
		UploadScratch.Units.assign(From.begin(), From.end());
		Vram.Upload(UploadScratch);
	}

	TResult<FBattleModelFiles> FBattleShow::ReadGear(int32 Gear, int32 Variant)
	{
		return ObjectFiles->ReadGear(Gear, Variant);
	}

	TResult<FBattleModelFiles> FBattleShow::ReadSpecialModel(int32 Model)
	{
		return ObjectFiles->ReadSpecialModel(Model);
	}

	TResult<FBattleSequenceExtension> FBattleShow::ReadExtension(int32 File)
	{
		return ObjectFiles->ReadExtension(File);
	}

	TResult<FBattleModelFiles> FBattleShow::ReadSceneModel(int32 Scene, int32 Entry)
	{
		return ObjectFiles->ReadSceneModel(Scene, Entry);
	}

	void FBattleShow::CreateProxy(int32 Slot)
	{
		Battle.CreateProxy(Slot, Objects.Height(Slot));
	}
}
