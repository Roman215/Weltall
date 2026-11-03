#include "Gears/GearShopModel.h"

#include "Foundation/LittleEndian.h"
#include "Media/GameFileIndex.h"

#include <string>
#include <utility>

namespace Weltall
{
	namespace
	{
		/** The shop's overlay, and where it is loaded. */
		constexpr int32 ShopDirectory = 16;
		constexpr int32 ShopFile = 10;
		constexpr uint32 ShopBase = 0x801C5000;
		/** Its tables, by gear: the first of the gear's two files (u16), the animation (u8), the height and the scale (s16). */
		constexpr uint32 FilesTable = 0x801D6D7C;
		constexpr uint32 AnimationsTable = 0x801D6DA0;
		constexpr uint32 HeightsTable = 0x801D6DB4;
		constexpr uint32 ScalesTable = 0x801D6DD8;
		/** The first gear model's first file; a gear model's files follow two by two. */
		constexpr int32 FirstGearModelFile = FGearFileNumbers::Of(0).Motion;
		/** The field's pace for a menu frame of one vblank: a tick every other frame. */
		constexpr uint32 FieldPaceExtraVblanks = 0;
	}

	TResult<FGearShopTable> ReadGearShopTable(const FCdImage& Image, const FGameFileIndex& Index)
	{
		const FIndexRecord* Record = Index.Find(ShopDirectory, ShopFile);
		if (Record == nullptr)
		{
			return MakeError("the gear shop's code is not on the disc");
		}
		auto Bytes = ReadFileSectors(Image, *Record);
		if (!Bytes)
		{
			return Bytes.Error();
		}
		const std::vector<uint8>& Code = *Bytes;
		if (Code.size() < ScalesTable - ShopBase + 2 * FGearShopTable::Gears)
		{
			return MakeError("the gear shop's code is too short for its gears' table");
		}
		FGearShopTable Table;
		for (int32 Gear = 0; Gear < FGearShopTable::Gears; ++Gear)
		{
			FGearShopGear& Entry = Table.Entries[static_cast<std::size_t>(Gear)];
			const std::size_t Half = static_cast<std::size_t>(2 * Gear);
			const int32 File = LittleEndian::ReadU16(Code, FilesTable - ShopBase + Half);
			Entry.GearModel = File == 0 ? FGearShopGear::NoModel : (File - FirstGearModelFile) / 2;
			if (File != 0 && ((File - FirstGearModelFile) % 2 != 0 || Entry.GearModel < 0 || Entry.GearModel >= FGearFileNumbers::ModelCount))
			{
				return MakeError("the gear shop's gear " + std::to_string(Gear) + " reads file " + std::to_string(File) + ", no gear model's first");
			}
			Entry.Animation = Code[AnimationsTable - ShopBase + static_cast<std::size_t>(Gear)];
			Entry.Height = LittleEndian::ReadS16(Code, HeightsTable - ShopBase + Half);
			Entry.Scale = LittleEndian::ReadS16(Code, ScalesTable - ShopBase + Half);
		}
		return Table;
	}

	FGearShopModel::FGearShopModel() : Module(std::make_unique<FGearModels>())
	{
	}

	FGte FGearShopModel::ShopBuildLighting()
	{
		FGte Lighting;
		for (std::size_t Channel = 0; Channel < 3; ++Channel)
		{
			Lighting.BackColor[Channel] = Ambient * 16;
			Lighting.ColorMatrix.M[Channel][0] = ColorScale;
		}
		return Lighting;
	}

	std::optional<FError> FGearShopModel::Open(const FGearShopModelOptions& InOptions)
	{
		Options = InOptions;
		FPsxRandom* Random = Module->Random;
		const FGteArctangents* Arctangents = Module->Arctangents;
		Module = std::make_unique<FGearModels>();
		Module->Random = Random;
		Module->Arctangents = Arctangents;
		Module->BuildLighting = ShopBuildLighting();
		return Module->Init(Pool, Options.Models);
	}

	std::optional<FError> FGearShopModel::Show(const FGearShopGear& Gear, FGearFiles Files)
	{
		if (Gear.GearModel == FGearShopGear::NoModel)
		{
			return MakeError("the gear shop shows a gear without a model");
		}
		if (Module->Model(Slot) != nullptr)
		{
			Module->Free(Slot);
		}
		if (auto Error = Module->Load(Slot, std::move(Files.Model), std::move(Files.Motion), TexturePlace, FGteVector{}))
		{
			return Error;
		}
		FGearModel* Model = Module->Model(Slot);
		if (Model == nullptr || Model->Bones.empty())
		{
			return MakeError("the gear shop's gear model " + std::to_string(Gear.GearModel) + " did not load");
		}
		Model->FloorY = Gear.Height;
		Model->Scale = Gear.Scale;
		FGearBone& Root = Model->Bones[0];
		Root.Angles[1] = static_cast<int16>(Root.Angles[1] + YawTurn);
		Root.Angles[0] = static_cast<int16>(Root.Angles[0] + PitchTurn);
		return Module->PlayAnimation(Slot, 0, Gear.Animation);
	}

	std::optional<FError> FGearShopModel::Frame()
	{
		return Module->Tick(Options.bFieldPace ? FieldPaceExtraVblanks : OriginalExtraVblanks);
	}

	void FGearShopModel::Close()
	{
		Module->FreeAll();
	}
}
