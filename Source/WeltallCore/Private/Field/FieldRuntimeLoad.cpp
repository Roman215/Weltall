#include "Field/FieldRuntime.h"

#include "Field/EventMachine.h"
#include "Field/EventPlacement.h"
#include "Field/FieldFollowers.h"
#include "Field/FieldLayers.h"
#include "Field/FieldMapChange.h"
#include "Field/FieldModels.h"
#include "Field/FieldPartySprites.h"
#include "Field/FieldReturn.h"
#include "Field/FieldScene.h"
#include "Field/FieldScriptFixes.h"
#include "Field/FieldSpriteFiles.h"
#include "Field/FieldTextures.h"
#include "Field/FieldSprites.h"
#include "Game/GameState.h"
#include "Game/PsxRandom.h"
#include "Graphics/GteVectors.h"
#include "Story/DiscText.h"
#include "Story/StoryPacks.h"
#include "Text/MessageTable.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		constexpr int32 MapBits = 0xFFF;
		constexpr int32 GearModeBits = 0xC000;
		/** Where a map's encounter section keeps the weights of its 16 formations. */
		constexpr std::size_t EncounterWeightsAt = 0x200;
		/** The projection distance the GTE holds for the scripts of a map's init. */
		constexpr uint16 InitProjectionDistance = 0x200;
		/** Room for a tick's VRAM changes without growing: the original keeps the last 32. */
		constexpr std::size_t VramOperationsPerTick = 32;
		/** The picture the field takes of its screen before a map's entry starts at full brightness (16.16). */
		constexpr int32 EntryCoverBrightness = 0x800000;

		FError LoadError(int32 Number, const std::string& What)
		{
			return MakeError("field map " + std::to_string(Number & MapBits) + ": " + What);
		}

		/** The palettes' rows in VRAM: a sprite slot's is row 480 plus its number, 256 colours wide. */
		constexpr int16 PaletteRowsTop = 480;
		constexpr int16 PaletteRowWidth = 256;

		/**
		 * Slot Borrow.Slot of a map of its own takes slot Borrow.SourceSlot of disc map Borrow.Map: that slot's sprite
		 * file in place of the base map's, its images at the slot's own place in VRAM and its palette at the slot's own
		 * row, both uploaded after the base map's so that they show.
		 */
		std::optional<FError> BorrowSprite(const FSpriteBorrow& Borrow, FFieldMap& Map, const FCdImage& Image, const FGameFileIndex& Index)
		{
			TResult<FFieldArchive> Other = LoadFieldArchive(Image, Index, Borrow.Map);
			if (!Other)
			{
				return Other.Error();
			}
			const TResult<std::span<const uint8>> File = FieldSpriteFileBytes(Other->Section(EFieldSection::SpriteSets), static_cast<uint32>(Borrow.SourceSlot));
			if (!File)
			{
				return MakeError("map " + std::to_string(Borrow.Map) + ": " + File.Error().Message);
			}
			TResult<std::vector<uint8>> Replaced = ReplaceFieldSpriteFile(Map.SpriteSets, static_cast<uint32>(Borrow.Slot), *File);
			if (!Replaced)
			{
				return MakeError("the base map: " + Replaced.Error().Message);
			}
			const FFieldSpriteSlot& At = Map.SpriteSlots[static_cast<std::size_t>(Borrow.Slot)];
			TResult<std::vector<FVramImage>> Images = ReadFieldSpriteSlotImages(*Other, Borrow.SourceSlot, At);
			if (!Images)
			{
				return MakeError("map " + std::to_string(Borrow.Map) + ": " + Images.Error().Message);
			}
			// The palette: the row the other map's uploads leave at its slot's row, moved to this slot's.
			TResult<FFieldMap> OtherMap = LoadFieldMap(Image, Index, Borrow.Map);
			if (!OtherMap)
			{
				return OtherMap.Error();
			}
			const int32 SourceRow = PaletteRowsTop + Borrow.SourceSlot;
			FVramImage Palette{{0, static_cast<int16>(PaletteRowsTop + Borrow.Slot), PaletteRowWidth, 1}, std::vector<uint16>(static_cast<std::size_t>(PaletteRowWidth))};
			bool bFound = false;
			for (const FVramImage& Upload : OtherMap->TextureUploads)
			{
				const FVramRect& Rect = Upload.Rect;
				if (SourceRow < Rect.Y || SourceRow >= Rect.Y + Rect.Height)
				{
					continue;
				}
				const int32 First = std::max<int32>(Rect.X, 0);
				const int32 Last = std::min<int32>(Rect.X + Rect.Width, PaletteRowWidth);
				for (int32 Column = First; Column < Last; ++Column)
				{
					Palette.Units[static_cast<std::size_t>(Column)] = Upload.Units[static_cast<std::size_t>((SourceRow - Rect.Y) * Rect.Width + (Column - Rect.X))];
					bFound = true;
				}
			}
			if (!bFound)
			{
				return MakeError("map " + std::to_string(Borrow.Map) + " uploads no palette for its slot " + std::to_string(Borrow.SourceSlot));
			}
			Map.SpriteSets = std::move(*Replaced);
			Map.TextureUploads.insert(Map.TextureUploads.end(), std::make_move_iterator(Images->begin()), std::make_move_iterator(Images->end()));
			Map.TextureUploads.push_back(std::move(Palette));
			return std::nullopt;
		}

		/**
		 * A map of its own over its base map's parts: another map's encounter section when the document names one,
		 * and, for a map made from its base, the document's scripts and messages in place of the base's (a map over
		 * its base keeps them, and the document is added to them once they are read: AddToBaseScripts). Its script
		 * actors are the base's first actor definitions, so it may have no more than the base map's scripts have.
		 */
		std::optional<FError> BuildMapOfItsOwn(const FStoryDocument& Document, FFieldMap& Map, const FCdImage& Image, const FGameFileIndex& Index,
			const IDiscText& DiscText)
		{
			if (!Document.Patch.KeepsBaseScripts())
			{
				TResult<FFieldMapOfItsOwn> Built = Document.Patch.Build(&DiscText);
				if (!Built)
				{
					return MakeError(Document.Name + ": " + Built.Error().Message);
				}
				const TResult<FEventScripts> Base = FEventScripts::Parse(Map.EventScripts);
				const TResult<FEventScripts> Own = FEventScripts::Parse(Built->EventScripts);
				if (!Base || !Own)
				{
					return MakeError(Document.Name + ": its scripts or its base map's cannot be read");
				}
				if (Own->ActorCount() > Base->ActorCount())
				{
					return MakeError(Document.Name + ": " + std::to_string(Own->ActorCount()) + " script actors, more than the " + std::to_string(Base->ActorCount())
						+ " of field map " + std::to_string(*Document.Patch.Base()));
				}
				Map.EventScripts = std::move(Built->EventScripts);
				Map.Messages = std::move(Built->Messages);
			}
			if (const std::optional<int32> From = Document.Patch.EncountersFrom())
			{
				TResult<FFieldArchive> Other = LoadFieldArchive(Image, Index, *From);
				if (!Other)
				{
					return Other.Error();
				}
				Map.Encounters = std::move(Other->Section(EFieldSection::Encounters));
			}
			for (const FSpriteBorrow& Borrow : Document.Patch.SpritesFrom())
			{
				if (const std::optional<FError> Unborrowed = BorrowSprite(Borrow, Map, Image, Index))
				{
					return MakeError(Document.Name + ": sprite " + std::to_string(Borrow.Slot) + " from map " + std::to_string(Borrow.Map) + " slot "
						+ std::to_string(Borrow.SourceSlot) + ": " + Unborrowed->Message);
				}
			}
			return std::nullopt;
		}

		/** A map over its base: the document added to the base map's scripts and messages, which must be the ones it was written for. */
		std::optional<FError> AddToBaseScripts(const FStoryDocument& Document, FEventScripts& Scripts, std::vector<uint8>& Messages, const IDiscText& DiscText)
		{
			const TResult<bool> Added = Document.Patch.Apply(Scripts, Messages, &DiscText);
			if (!Added)
			{
				return MakeError(Document.Name + ": " + Added.Error().Message);
			}
			if (!*Added)
			{
				return MakeError(Document.Name + ": field map " + std::to_string(*Document.Patch.Base()) + " or the disc's text is not what it expects");
			}
			return std::nullopt;
		}
	}

	FFieldRuntime::FFieldRuntime(const FFieldServices& InServices) : Services(InServices), DiscFiles(InServices.Image, InServices.Index, InServices.TranslationCreditsCue)
	{
	}

	FFieldRuntime::~FFieldRuntime() = default;

	void FFieldRuntime::SetOptions(const FEventOptions& Options)
	{
		Services.Options = Options;
		FieldContext.Options = Options;
		if (ActorSprites)
		{
			ActorSprites->System().bMirroredPartsPlacedByArt = Options.bSpriteFramesAligned;
		}
	}

	void FFieldRuntime::SetDisc(uint8 Disc)
	{
		Services.Disc = Disc;
		FieldContext.Disc = Disc;
	}

	std::optional<FError> FFieldRuntime::EnterMap(int32 Number)
	{
		return Enter(Number, nullptr);
	}

	std::optional<FError> FFieldRuntime::EnterMap(int32 Number, const FFieldKernelState& Kernel, const FFieldEntry& InEntry)
	{
		return Enter(Number, &Kernel, false, InEntry);
	}

	std::optional<FError> FFieldRuntime::EnterMapAfterBattle(int32 Number, const FFieldKernelState& Kernel, const FFieldEntry& InEntry)
	{
		if (!Kernel.Saved)
		{
			return LoadError(Number, "the field comes back to a state it never saved");
		}
		return Enter(Number, &Kernel, true, InEntry);
	}

	FFieldKernelState FFieldRuntime::KernelState() const
	{
		FFieldKernelState Kernel;
		Kernel.EncounterWeights = FieldContext.Encounters.Weights;
		Kernel.BattleFormation = FieldContext.Encounters.Formation;
		Kernel.Clock = FieldContext.Clock;
		Kernel.MenuParam = FieldContext.MenuProgramParam;
		Kernel.MenusPending = FieldContext.MenusPending;
		Kernel.bGearsLocked = FieldContext.bGearsLocked;
		Kernel.BattlingResult = FieldContext.BattlingResult;
		if (ActorSprites)
		{
			Kernel.SpriteRingPlace = ActorSprites->System().RingPlace();
		}
		Kernel.Saved = SavedForReturn;
		return Kernel;
	}

	std::optional<FError> FFieldRuntime::Enter(int32 Number, const FFieldKernelState* Kernel, bool bReturning, const FFieldEntry& InEntry)
	{
		SavedForReturn = Kernel != nullptr ? Kernel->Saved : std::nullopt;
		if (auto Error = LoadMap(Number, nullptr, Kernel, bReturning))
		{
			return Error;
		}
		FieldContext.bReturning = bReturning;
		if (auto Error = bReturning ? RestoreFieldForReturn(*SavedForReturn, *Services.Game, FieldContext) : FEventMachine::InitMap(FieldContext))
		{
			return Error;
		}
		if (auto Error = FinishLoad(Number, bReturning))
		{
			return Error;
		}
		if (auto Error = ActorSprites->System().FaultError())
		{
			return Error;
		}
		if (FieldContext.Gears.Count != 0)
		{
			FieldContext.Gears.Models->SetSnapPoses(true);
		}
		Entry = InEntry;
		bEntryReturning = bReturning;
		// The picture the field took of the screen before the load: it fades and zooms away over the entry, adding;
		// after a lost battle or with the battle event's fade it covers the screen.
		FieldContext.Render.Cover.Take(!Entry.bAfterDefeat && !Entry.bBattleEventFade, true);
		CoverBrightness = EntryCoverBrightness;
		PendingRequest = {};
		bBoardingButtonsHeld = false;
		if (Entry.bBattleEventFade)
		{
			Phase = EPhase::EntryFade;
			return std::nullopt;
		}
		return BeginEntry();
	}

	std::optional<FError> FFieldRuntime::LoadMap(int32 Number, const FEventContext* Before, const FFieldKernelState* Kernel, bool bReturning)
	{
		const bool bExpandedStory = Services.Game != nullptr && Services.Game->StoryMode() == EStoryMode::Expanded;
		const FStoryDocument* Own = Services.Story != nullptr ? Services.Story->MapOfItsOwn(Number & MapBits, bExpandedStory) : nullptr;
		auto Loaded = LoadFieldMap(*Services.Image, *Services.Index, Own != nullptr ? *Own->Patch.Base() : Number & MapBits);
		if (!Loaded)
		{
			return Loaded.Error();
		}
		// The disc's own text, for the documents that name a message of it.
		const FGameDiscText DiscText(*Services.Image, *Services.Index);
		if (Own != nullptr)
		{
			if (const std::optional<FError> Unbuilt = BuildMapOfItsOwn(*Own, *Loaded, *Services.Image, *Services.Index, DiscText))
			{
				return LoadError(Number, Unbuilt->Message);
			}
		}
		auto Parsed = FEventScripts::Parse(Loaded->EventScripts);
		if (Parsed)
		{
			// A map over its base runs the base map's scripts, so it takes that map's fixes before the document is added.
			const bool bOverBase = Own != nullptr && Own->Patch.KeepsBaseScripts();
			ApplyFieldScriptFixes(bOverBase ? *Own->Patch.Base() : Number & MapBits, *Parsed, Services.Options);
			if (bOverBase)
			{
				if (const std::optional<FError> Unfit = AddToBaseScripts(*Own, *Parsed, Loaded->Messages, DiscText))
				{
					return LoadError(Number, Unfit->Message);
				}
			}
			const std::optional<FError> Unfit = Services.Story != nullptr
				? Services.Story->ApplyToMap(Number & MapBits, *Parsed, Loaded->Messages, Services.Options, bExpandedStory, &DiscText) : std::nullopt;
			if (Unfit)
			{
				return LoadError(Number, "an addition to it: " + Unfit->Message);
			}
		}
		auto Files = ParseFieldSpriteFiles(Loaded->SpriteSets);
		auto Messages = FMessageTable::Parse(Loaded->Messages);
		if (!Parsed || !Files || !Messages)
		{
			return LoadError(Number, "its scripts, sprite files or messages cannot be read");
		}
		FSpriteSystem::FRingPlace Ring = ActorSprites != nullptr ? ActorSprites->System().RingPlace() : FSpriteSystem::FRingPlace{};
		if (Kernel != nullptr)
		{
			Ring = Kernel->SpriteRingPlace;
		}
		ActorSprites.reset();
		MapData = std::make_unique<FFieldMap>(std::move(*Loaded));
		Scripts = std::make_unique<FEventScripts>(std::move(*Parsed));
		MapSpriteFiles = std::move(*Files);

		// A fresh context is the field's globals as a map's load resets them; a map change keeps the rest.
		FEventContext Fresh;
		if (Before != nullptr)
		{
			KeepAcrossMaps(*Before, Fresh);
		}
		else if (Kernel != nullptr)
		{
			Fresh.Encounters.Weights = Kernel->EncounterWeights;
			Fresh.Encounters.Formation = Kernel->BattleFormation;
			Fresh.Clock = Kernel->Clock;
			Fresh.MenuProgramParam = Kernel->MenuParam;
			Fresh.MenusPending = Kernel->MenusPending;
			Fresh.bGearsLocked = Kernel->bGearsLocked;
			Fresh.BattlingResult = Kernel->BattlingResult;
		}
		FieldContext = std::move(Fresh);
		FieldContext.Render.VramOperations.reserve(VramOperationsPerTick);
		FieldContext.Render.Markers.Placed.reserve(FFieldScreenMarkers::Count);
		// A map entered keeps the effects' textures aside until its entry's last tick; a map change has
		// kept them aside before its load.
		if (Before == nullptr)
		{
			FieldContext.Render.SaveEffectTextures();
		}
		FEventContext& Context = FieldContext;
		Context.Scripts = Scripts.get();
		Context.MapNumber = Number;
		Context.RunningMap = Number & MapBits;
		Context.Zones = MapData->Zones;
		Context.Walkmesh = &MapData->Walkmesh;
		Context.MapModels = MapData->Models;
		Context.Arctangents = Services.Arctangents;
		Context.Random = Services.Random;
		Context.Songs = Services.Songs;
		Context.SoundEffects = Services.SoundEffects;
		Context.SoundBanks = Services.SoundBanks;
		Context.Files = &DiscFiles;
		Context.GameState = Services.Game;
		Context.Options = Services.Options;
		Context.EncounterRate = Services.EncounterRate;
		Context.Unreached = Services.Unreached;
		Context.ViewTurn = ViewTurn;
		Context.bAlwaysRun = bAlwaysRun;
		Context.bAnalogMovement = bAnalogMovement;
		Context.bWalkIntoDoors = bWalkIntoDoors;
		Context.Disc = Services.Disc;
		Context.SettleTicks = FEventContext::MapStartSettleTicks;
		Context.Encounters.Pending.fill(FFieldEncounters::NoEncounter);
		// A map without encounter data keeps the place's weights from before.
		if (MapData->Encounters.size() >= EncounterWeightsAt + FFieldEncounters::FormationCount)
		{
			std::copy_n(MapData->Encounters.begin() + EncounterWeightsAt, FFieldEncounters::FormationCount, Context.Encounters.Weights.begin());
		}
		Context.Variables.EnterField(*Services.Game);
		Context.Actors = CreateFieldActors(MapData->Actors);
		Context.States.resize(static_cast<std::size_t>(Scripts->ActorCount()));
		for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
		{
			FEventActorState& State = Context.States[Actor];
			State.Reset(Services.Random->Next());
			if (!PlaceActorOnLoad(State, Context.Actors[Actor], MapData->Walkmesh))
			{
				return LoadError(Number, "actor " + std::to_string(Actor) + " stands on no floor of its layer");
			}
		}
		if (auto Error = SetUpFieldMime(Context))
		{
			return Error;
		}
		if (auto Error = bReturning ? Services.PartySprites->ReloadFiles(SavedForReturn->PartySpriteFiles)
									: Services.PartySprites->LoadForMap((Number & GearModeBits) != 0, *Services.Game, Context.PartyCharacters))
		{
			return Error;
		}
		Context.PartySprites = Services.PartySprites;
		FFieldMessages& Windows = Context.Messages;
		Windows.Table = std::move(*Messages);
		Windows.Replaced = &MapData->ReplacedMessages;
		Windows.Text = Services.Text;
		Windows.EnterMap();
		// The init's scripts project through the view the lights' setup leaves: a look-at of nothing.
		Context.ScriptView.View = FieldLookAt({}, {});
		Context.ScriptView.View.T = {};
		Context.ScriptView.ProjectionDistance = InitProjectionDistance;
		Context.Camera.View.View = Context.ScriptView.View;
		ActorSprites = std::make_unique<FFieldSprites>(MapSpriteFiles, MapData->SpriteSlots, *Services.PartySprites, Context.States, *Services.Image, *Services.Index);
		ActorSprites->System().Random = Services.Random;
		ActorSprites->System().Arctangents = Services.Arctangents;
		ActorSprites->System().SetRingPlace(Ring);
		ActorSprites->System().bMirroredPartsPlacedByArt = Services.Options.bSpriteFramesAligned;
		Context.Models = ActorSprites.get();
		if (!DiscGearFiles)
		{
			DiscGearFiles = std::make_unique<FFieldDiscGearFiles>(*Services.Image, *Services.Index);
		}
		Context.Gears.Files = DiscGearFiles.get();
		++Loads;
		// The load's lights: the map's colours, under its reset camera; the back colour stays as it was.
		const FGte Lighting = FieldLightingAtLoad(FieldMapLights(MapData->Lights));
		GteLight = GteLightUnderGears = Lighting.LightMatrix;
		GteColors = Lighting.ColorMatrix;
		bGteColorsKnown = true;
		return std::nullopt;
	}

	std::optional<FError> FFieldRuntime::FinishLoad(int32 Number, bool bReturning)
	{
		FEventContext& Context = FieldContext;
		if (Context.Camera.FollowedActor >= Context.Actors.size())
		{
			// The original would read a record past its actors'.
			return LoadError(Number, "its camera follows actor " + std::to_string(Context.Camera.FollowedActor) + ", which it does not have");
		}
		Context.Camera.WorldRotation = RotationXyz(Context.Camera.WorldAngles);
		// The panorama is made once, from what the init's scripts left.
		Context.Render.Backdrop = Context.Render.Panorama;
		const FFieldActor& Followed = Context.Actors[Context.Camera.FollowedActor];
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Context.Camera.TargetGoal[Axis] = static_cast<int32>(static_cast<uint32>(Followed.Translation[Axis]) << 16);
		}
		for (FFieldActor& Record : Context.Actors)
		{
			Record.Matrix = RotationXyz(Record.Rotation).M;
			Record.WorldMatrix = Record.Matrix;
			Record.Position = Record.Translation;
		}
		if (auto Error = LoadFieldGears(Context))
		{
			return LoadError(Number, Error->Message);
		}
		TakeGearLoadColors();
		if (bReturning)
		{
			if (auto Error = RestoreFieldSprites(*SavedForReturn, *Services.Game, Context))
			{
				return Error;
			}
		}
		SetFieldModelDrawModes(Context.Actors, Context.Render.Fog.bOn);
		if (auto Error = PlaceParty(Number))
		{
			return Error;
		}
		FSpriteSystem& Kernel = ActorSprites->System();
		for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
		{
			const FEventActorState& State = Context.States[Actor];
			FSprite* Sprite = ActorSprites->ActorSprite(static_cast<int32>(Actor));
			if ((Context.Actors[Actor].Flags & FieldActorFlag::NoModel) == 0 || Sprite == nullptr)
			{
				continue;
			}
			if ((State.Flags1 & EventActorFlags1::NoAnimations) != 0)
			{
				Kernel.SetDirection(*Sprite, static_cast<int16>(State.Yaw));
			}
			else
			{
				Kernel.SetFacing(*Sprite, static_cast<int16>(Context.Camera.ViewYaw + Context.ViewTurn + State.Yaw));
			}
		}
		return std::nullopt;
	}

	std::optional<FError> FFieldRuntime::PlaceParty(int32 Number)
	{
		// The load's layer steps take the actors' own heights as platforms': the word the load passes
		// them is never set, and the call before it always leaves it non-zero.
		if (const std::optional<int32> Actor = PlacePartyOnLeader(FieldContext, true))
		{
			return LoadError(Number, "party actor " + std::to_string(*Actor) + " cannot be placed on the leader");
		}
		return std::nullopt;
	}
}
