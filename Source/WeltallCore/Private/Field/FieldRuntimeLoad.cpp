#include "Field/FieldRuntime.h"

#include "Field/EventMachine.h"
#include "Field/EventPlacement.h"
#include "Field/FieldFollowers.h"
#include "Field/FieldLayers.h"
#include "Field/FieldMapChange.h"
#include "Field/FieldMapStory.h"
#include "Field/FieldModels.h"
#include "Field/FieldPartySprites.h"
#include "Field/FieldReturn.h"
#include "Field/FieldScene.h"
#include "Field/FieldSpriteFiles.h"
#include "Field/FieldSprites.h"
#include "Foundation/Crc32.h"
#include "Game/GameState.h"
#include "Game/PsxRandom.h"
#include "Graphics/GteVectors.h"
#include "Input/KernelPad.h"
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
		/** The pad's directions, logical bits 12-15. */
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
		const FFieldMapStory Story{Services.Story, Services.Options, Services.Game != nullptr && Services.Game->StoryMode() == EStoryMode::Expanded};
		auto Loaded = LoadFieldMap(*Services.Image, *Services.Index, FieldMapReadFrom(Number & MapBits, Story));
		if (!Loaded)
		{
			return Loaded.Error();
		}
		std::vector<const FStoryDocument*> LeftOut;
		auto Parsed = PutStoryInFieldMap(Number & MapBits, *Loaded, *Services.Image, *Services.Index, Story, LeftOut);
		if (!Parsed)
		{
			return LoadError(Number, Parsed.Error().Message);
		}
		// A map of its own is its document: over a base that is not the one it was written for, there is nothing to play.
		const auto Unmade = std::find_if(LeftOut.begin(), LeftOut.end(), [](const FStoryDocument* Document) { return Document->Patch.Base().has_value(); });
		if (Unmade != LeftOut.end())
		{
			return LoadError(Number, (*Unmade)->Name + ": field map " + std::to_string(*(*Unmade)->Patch.Base()) + " or the disc's text is not what it expects");
		}
		auto Files = ParseFieldSpriteFiles(Loaded->SpriteSets);
		auto Messages = FMessageTable::Parse(Loaded->Messages);
		if (!Files || !Messages)
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
		Context.ScriptsKey = Crc32(MapData->EventScripts);
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
		// A map the party comes back to (after a battle, a menu) has nothing new waiting: no pause there.
		DoorPauseLeft = bReturning ? 0 : DoorPauseVblanks;
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
