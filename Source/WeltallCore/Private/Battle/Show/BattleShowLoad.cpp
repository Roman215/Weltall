#include "Battle/Show/BattleShowLoad.h"

#include "Battle/BattleContext.h"
#include "Battle/Show/BattleSceneGraphics.h"
#include "Battle/Show/BattleSceneModels.h"
#include "Battle/Show/BattleShowVram.h"
#include "Battle/Show/IdsBattleSprite.h"
#include "Foundation/LittleEndian.h"

namespace Weltall
{
	namespace
	{
		/** The setup's texture places for the party's sprites: along row 1C0h from 100h. */
		constexpr int16 PartyTextureX = 0x100;
		constexpr int16 PartyTextureY = 0x1C0;
		/** The types an enemy's slot can have that name an entry of the scene, and the characters the party's tables cover. */
		constexpr uint8 SceneTypes = 8;
		constexpr uint8 PartyTypes = 0x11;
		/** A party sprite's room for parts: the 300h bytes the setup gives it. */
		constexpr int32 PartyPartRecords = 0x20;
		constexpr int32 RunInAnimation = 0x17;
		/** The common effects' resource: its textures at (380h, 0), its palettes at row 1D1h. */
		constexpr int16 CommonTextureX = 0x380;
		constexpr int16 CommonClutY = 0x1D1;
		/** The sound effects file's set id. */
		constexpr std::size_t SedsSetId = 0x14;
		/** Frames the setup waits once the gears are made, before it waits for the party to land. */
		constexpr int32 LandingDelay = 0x10;
		constexpr int32 ClutYFirst = 0x1C0;
	}

	FBattleShowLoad::FBattleShowLoad(FBattleShow& InShow, const FBattleShowSetup& Setup)
		: Show(InShow), SpriteFiles(Setup.SpriteFiles), Scene(Setup.SceneGraphics), IdsSprite(Setup.IdsSprite), Kind(Setup.Kind)
	{
	}

	void FBattleShowLoad::Fail(EBattleShowFault Fault)
	{
		Show.ReportFault(Fault);
		Step = EStep::Done;
		Show.Sprites.UnlinkTask(*this);
	}

	bool FBattleShowLoad::OnFoot(int32 Slot) const
	{
		const FBattleSlot& Record = Show.Context->Slots[static_cast<std::size_t>(Slot)];
		return Record.Occupant < PartyTypes && Record.InGear == 0;
	}

	bool FBattleShowLoad::DrawnAsId(int32 Slot) const
	{
		return IdsSprite != nullptr && Show.Context->PartyLooks().IsId(Show.Context->PartyCharacters[static_cast<std::size_t>(Slot)]);
	}

	void FBattleShowLoad::Update()
	{
		// Each step runs once the disc has delivered what the step before read: in the port, at the next frame.
		switch (Step)
		{
		case EStep::Scene:
			LoadScene();
			if (Step == EStep::Done)
			{
				return;
			}
			CreateEnemies();
			// The party's sprites: the members on foot's sprite files, as the player has them drawn.
			for (int32 Slot = 0; Slot < PartySlots && Step != EStep::Done; ++Slot)
			{
				if (OnFoot(Slot))
				{
					TResult<FBattleSpriteFile> File = DrawnAsId(Slot) ? IdsSprite->File() : SpriteFiles->ReadCharacterSprite(Show.Context->DrawnOccupant(Slot));
					if (!File)
					{
						Fail(EBattleShowFault::File);
						return;
					}
					if (Show.Context->PartyLooks().HasPurpleHair(Show.Context->DrawnOccupant(Slot)))
					{
						File->RecolourPurpleHair();
					}
					PartyFiles[static_cast<std::size_t>(Slot)] = std::move(*File);
					Resources[static_cast<std::size_t>(Slot)] = {&*PartyFiles[static_cast<std::size_t>(Slot)], 0, 0, 0};
					Show.Battle.WritableFiles.push_back(&PartyFiles[static_cast<std::size_t>(Slot)]->Sprite());
				}
			}
			if (Step != EStep::Done)
			{
				Step = EStep::Party;
			}
			return;
		case EStep::Party:
		{
			CreateParty();
			if (Step == EStep::Done)
			{
				return;
			}
			TResult<std::vector<uint8>> Images = SpriteFiles->ReadCommonImages();
			TResult<FBattleSpriteFile> Common = SpriteFiles->ReadCommon();
			TResult<std::vector<uint8>> Sounds = SpriteFiles->ReadCommonSounds();
			if (!Images || !Common || !Sounds || Sounds->size() < SedsSetId + 2)
			{
				Fail(EBattleShowFault::File);
				return;
			}
			CommonImages = std::move(*Images);
			CommonFile = std::move(*Common);
			CommonSounds = std::move(*Sounds);
			Step = EStep::Effects;
			return;
		}
		case EStep::Effects:
			LoadEffects();
			Step = EStep::Sounds;
			return;
		case EStep::Sounds:
			// The sounds' step, once the sound's transfers are done (at once in the port).
			CommonImages = {};
			QueueGears();
			if (Step != EStep::Done)
			{
				Step = EStep::Gears;
			}
			return;
		case EStep::Gears:
			if (!Show.Objects.GearModelsPending())
			{
				Countdown = LandingDelay;
				Step = EStep::Landing;
			}
			return;
		case EStep::Landing:
			Land();
			return;
		case EStep::Done:
			return;
		}
	}

	void FBattleShowLoad::LoadScene()
	{
		const std::vector<FBattleSceneEntry>& Entries = Scene->Entries();
		const std::optional<std::vector<int16>> Columns = BattleSceneColumns(*Scene);
		if (Entries.size() > SceneEntries || !Columns)
		{
			Fail(EBattleShowFault::SceneGraphics);
			return;
		}
		std::array<uint8, SlotCount> Types{};
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			Types[static_cast<std::size_t>(Slot)] = Show.Context->Slots[static_cast<std::size_t>(Slot)].Occupant;
		}
		FBattleSceneImageSets Sets;
		for (std::size_t Index = 0; Index < Entries.size(); ++Index)
		{
			const FBattleSceneEntry& Entry = Entries[Index];
			const int16 EntryColumn = (*Columns)[Index];
			if (Entry.IsSprite())
			{
				const std::optional<int16> SetColumn = Sets.Place(*Scene, Index, EntryColumn, Show.Vram);
				if (!SetColumn || Entry.Data <= 7 || Entry.Data >= Scene->KeptEnd())
				{
					Fail(EBattleShowFault::SceneGraphics);
					return;
				}
				const FBattleSpriteFile* File = SceneFile(Entry.Data);
				if (File == nullptr)
				{
					Fail(EBattleShowFault::SceneGraphics);
					return;
				}
				Resources[PartySlots + Index] = {File, *SetColumn, FBattleSceneImageSets::TextureY, Entry.PaletteSet};
				continue;
			}
			if (!Entry.HasOwnImages())
			{
				// A part of an earlier entry's model: made with it.
				continue;
			}
			Resources[PartySlots + Index] = {};
			const FBattleSceneModels Models = CreateBattleSceneModels(Show.Objects, *Scene, Index, EntryColumn, Types);
			if (Models.bOutsideFile)
			{
				Fail(EBattleShowFault::SceneGraphics);
				return;
			}
			for (int32 Made = 0; Made < Models.Count; ++Made)
			{
				const int32 Slot = Models.Slots[static_cast<std::size_t>(Made)];
				Show.Battle.CreateProxy(Slot, Show.Objects.Height(Slot));
			}
		}
	}

	const FBattleSpriteFile* FBattleShowLoad::SceneFile(uint32 Data)
	{
		for (int32 Kept = 0; Kept < SceneFileCount; ++Kept)
		{
			if (SceneFileData[static_cast<std::size_t>(Kept)] == Data)
			{
				return &*SceneFiles[static_cast<std::size_t>(Kept)];
			}
		}
		const std::span<const uint8> Bytes = Scene->Bytes();
		TResult<FBattleSpriteFile> File = FBattleSpriteFile::Parse(Bytes.subspan(Data, Scene->KeptEnd() - Data));
		if (!File || SceneFileCount >= SceneEntries)
		{
			return nullptr;
		}
		const std::size_t Kept = static_cast<std::size_t>(SceneFileCount++);
		SceneFileData[Kept] = Data;
		SceneFiles[Kept] = std::move(*File);
		Show.Battle.WritableFiles.push_back(&SceneFiles[Kept]->Sprite());
		return &*SceneFiles[Kept];
	}

	void FBattleShowLoad::CreateEnemies()
	{
		for (int32 Slot = PartySlots; Slot < SlotCount && Step != EStep::Done; ++Slot)
		{
			const uint8 Type = Show.Context->Slots[static_cast<std::size_t>(Slot)].Occupant;
			if (Type < SceneTypes && Resources[PartySlots + Type].File != nullptr)
			{
				CreateSprite(Slot, Resources[PartySlots + Type]);
			}
		}
	}

	void FBattleShowLoad::CreateSprite(int32 Slot, const FResource& Resource)
	{
		// The combatant faces the way the setup aimed its slot.
		const FBattleContext& Context = *Show.Context;
		FBattleCombatantLook Look;
		Look.ClutY = ClutYFirst + Slot;
		Look.TextureX = Resource.TextureX;
		Look.TextureY = Resource.TextureY;
		Look.PaletteSet = Resource.PaletteSet;
		Look.Animation = 1;
		Look.bFacesLeft = Context.Slots[static_cast<std::size_t>(Slot)].FacesLeft != 0;
		Show.Battle.CreateCombatant(Slot, Resource.File->Sprite(), Look);
	}

	void FBattleShowLoad::CreateParty()
	{
		uint16 Column = SpriteFiles->FirstTextureColumn();
		for (int32 Slot = 0; Slot < PartySlots && Step != EStep::Done; ++Slot)
		{
			if (!OnFoot(Slot))
			{
				continue;
			}
			const uint8 Character = Show.Context->DrawnOccupant(Slot);
			if (Character >= FBattleSpriteFiles::WidthCount || Character >= FBattleSpriteFiles::CharacterCount)
			{
				Fail(EBattleShowFault::File);
				return;
			}
			FResource& Resource = Resources[static_cast<std::size_t>(Slot)];
			Resource.TextureX = static_cast<int16>(PartyTextureX + Column);
			Resource.TextureY = PartyTextureY;
			Column = static_cast<uint16>(Column + SpriteFiles->TextureWidth(Character));
			if (DrawnAsId(Slot))
			{
				Resource.TextureX = FIdsBattleSprite::TextureX;
				Resource.TextureY = FIdsBattleSprite::TextureY;
				for (const FVramImage& Picture : IdsSprite->Images())
				{
					Show.Vram.UploadToPortRows(Picture);
				}
			}
			CreateSprite(Slot, Resource);
			if (Step == EStep::Done)
			{
				return;
			}
			AttackFiles[static_cast<std::size_t>(Slot)] = SpriteFiles->Character(Character).Attacks;
			Show.Sprites.SetPartCapacity(*Show.Battle.SlotSprites[static_cast<std::size_t>(Slot)], PartyPartRecords);
		}
		if (Kind != 0)
		{
			return;
		}
		// The run-in: each member lands on the floor at its own place.
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			FSprite* Sprite = Show.Battle.SlotSprites[static_cast<std::size_t>(Slot)];
			if (Sprite == nullptr)
			{
				continue;
			}
			Show.Battle.SnapToFloor(*Sprite);
			Sprite->SavedPoint = {static_cast<int16>(Sprite->Position[0] >> 16), static_cast<int16>(Sprite->Position[1] >> 16), static_cast<int16>(Sprite->Position[2] >> 16)};
			Show.Sprites.SetAnimation(*Sprite, RunInAnimation);
		}
	}

	void FBattleShowLoad::LoadEffects()
	{
		// The effects' images, the common effects' resource, the battle's sounds, then the objects' shadows.
		UploadBattleImageList(Show.Vram, CommonImages, 0);
		Show.Sprites.ParseResource(Show.Battle.CommonEffects, CommonFile->Sprite(), CommonTextureX, 0, 0, CommonClutY);
		Show.Battle.WritableFiles.push_back(&CommonFile->Sprite());
		if (Show.Audio != nullptr)
		{
			Show.Audio->RegisterEffects(CommonSounds);
		}
		Show.Battle.SystemSoundSet = LittleEndian::ReadU16(CommonSounds, SedsSetId);
		Show.Objects.EnableShadows();
	}

	void FBattleShowLoad::QueueGears()
	{
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			const FBattleSlot& Record = Show.Context->Slots[static_cast<std::size_t>(Slot)];
			if (Record.Occupant < PartyTypes && Record.InGear != 0)
			{
				Show.Objects.QueueGearModel(Slot);
			}
		}
	}

	void FBattleShowLoad::Land()
	{
		// The landing: after its countdown, the battle starts once every member on foot stands on the floor.
		if (Countdown != 0)
		{
			--Countdown;
			return;
		}
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			const FSprite* Sprite = Show.Battle.SlotSprites[static_cast<std::size_t>(Slot)];
			if (Show.Context->Slots[static_cast<std::size_t>(Slot)].InGear == 0 && Sprite != nullptr && static_cast<int16>(Sprite->Position[1] >> 16) != Sprite->FloorY)
			{
				return;
			}
		}
		Show.bStarted = true;
		Step = EStep::Done;
		Show.Sprites.UnlinkTask(*this);
	}
}
