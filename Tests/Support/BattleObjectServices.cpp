#include "Support/BattleObjectServices.h"

#include "Foundation/LittleEndian.h"

namespace Weltall::Testing
{
	namespace
	{
		constexpr int32 NoPack = 0xFE;
		constexpr int32 OutsidePacks = 0xFF;
		/** The low halves of the two sprite resources an effect spawns from: the common effects' and the effect's own. */
		constexpr int32 CommonEffects = 0xBE10;
		constexpr int32 EffectOwn = 0xA474;
	}

	std::array<int32, 2> PackPlace(const FGearCodeRef& Ref)
	{
		if (!Ref)
		{
			return {NoPack, 0};
		}
		return Ref.Offset < Ref.Pack->Bytes.size() ? std::array<int32, 2>{Ref.Pack->Slot, static_cast<int32>(Ref.Offset)} : std::array<int32, 2>{OutsidePacks, 0};
	}

	void FRecordingObjectServices::UploadImage(const FGearTextureBlock& Block, int16 X, int16 Y)
	{
		Add("upload", {X, Y, Block.Width, Block.Height});
		if (Vram != nullptr)
		{
			Vram->Upload({{X, Y, Block.Width, Block.Height}, Block.Pixels});
		}
	}

	void FRecordingObjectServices::UploadToPortRows(const FGearTextureBlock& Block, int16 X, int16 Y)
	{
		Add("port upload", {X, Y, Block.Width, Block.Height});
		if (Vram != nullptr)
		{
			Vram->UploadToPortRows({{X, Y, Block.Width, Block.Height}, Block.Pixels});
		}
	}

	void FRecordingObjectServices::StoreImage(const FVramRect& Rect, std::span<uint16> Into)
	{
		Add("store", {Rect.X, Rect.Y, Rect.Width, Rect.Height});
		if (Vram == nullptr)
		{
			return;
		}
		std::size_t Unit = 0;
		for (int32 Row = 0; Row < Rect.Height; ++Row)
		{
			for (int32 Column = 0; Column < Rect.Width && Unit < Into.size(); ++Column)
			{
				Into[Unit++] = Vram->At(Rect.X + Column, Rect.Y + Row);
			}
		}
	}

	void FRecordingObjectServices::LoadImage(const FVramRect& Rect, std::span<const uint16> From)
	{
		Add("load", {Rect.X, Rect.Y, Rect.Width, Rect.Height});
		if (Vram != nullptr)
		{
			Vram->Upload({Rect, std::vector<uint16>(From.begin(), From.end())});
		}
	}

	void FRecordingObjectServices::StopCountingTasks()
	{
		TasksMade = 0;
		TasksCounted = 0;
	}

	void FRecordingObjectServices::BeginEventPerformance(int32 Slot, int32 Target)
	{
		TasksMade = 0;
		TasksCounted = 1;
		Add("performer", {Slot, Target});
		Record = 1;
	}

	void FRecordingObjectServices::SpawnEffect(const FBattleBoneEffect& Effect)
	{
		const std::array<int32, 2> Event = PackPlace(Effect.Event);
		Add("spawn", {Effect.bEffectResource ? EffectOwn : CommonEffects, Effect.Animation, Effect.Point[0], Effect.Point[1], Effect.Point[2], Effect.Direction,
			Effect.Scale, Event[0], Event[1], Effect.Slot});
	}

	void FRecordingObjectServices::TakeEffectResource(const FBattleSequenceExtension& Extension)
	{
		Add("resource", {Extension.Pack.Slot, static_cast<int32>(Extension.Sprite.Offset)});
	}

	uint16 SoundFileId(std::span<const uint8> Seds)
	{
		constexpr std::size_t FileIdOffset = 0x14;
		return Seds.size() < FileIdOffset + 2 ? uint16{0} : LittleEndian::ReadU16(Seds, FileIdOffset);
	}

	void FRecordingObjectServices::RegisterSoundEffects(std::span<const uint8> Seds)
	{
		const uint16 FileId = SoundFileId(Seds);
		Registered.insert(FileId);
		Add("register", {FileId});
	}

	void FRecordingObjectServices::UnregisterSoundEffects(std::span<const uint8> Seds)
	{
		const uint16 FileId = SoundFileId(Seds);
		Registered.erase(FileId);
		Add("unregister", {FileId});
	}

	TResult<FBattleModelFiles> FRecordingObjectServices::ReadGear(int32 Gear, int32 Variant)
	{
		const FBattleObjectTables::FGearEntry& Entry = Tables.Gears[static_cast<std::size_t>(Gear)];
		Add("read", {BattleObjectFiles::GearDirectory, Entry.First + 1});
		Add("read", {BattleObjectFiles::GearDirectory, Entry.First + 2});
		if (Variant != 0 && Variant <= Entry.LastVariant)
		{
			Add("read", {BattleObjectFiles::GearDirectory, Entry.First + 2 + Variant});
		}
		return BattleObjectFiles::ReadGear(Image, Index, Tables, Gear, Variant);
	}

	TResult<FBattleModelFiles> FRecordingObjectServices::ReadSpecialModel(int32 Model)
	{
		Add("read", {BattleObjectFiles::SpecialDirectory, 2 * Model + 1});
		Add("read", {BattleObjectFiles::SpecialDirectory, 2 * Model + 2});
		return BattleObjectFiles::ReadSpecialModel(Image, Index, Model);
	}

	TResult<FBattleSequenceExtension> FRecordingObjectServices::ReadExtension(int32 File)
	{
		Add("read", {BattleObjectFiles::ExtensionDirectory, File});
		return BattleObjectFiles::ReadExtension(Image, Index, File);
	}

	TResult<FBattleModelFiles> FRecordingObjectServices::ReadSceneModel(int32 Scene, int32 Entry)
	{
		Add("read scene", {Scene, Entry});
		return BattleObjectFiles::ReadSceneModel(Image, Index, Scene, Entry);
	}
}
