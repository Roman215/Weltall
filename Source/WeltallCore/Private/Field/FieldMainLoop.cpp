#include "Field/FieldMainLoop.h"

#include "Field/FieldActorSounds.h"
#include "Field/FieldFrameVariables.h"
#include "Field/FieldPartySprites.h"
#include "Game/GameState.h"

#include <algorithm>
#include <array>
#include <span>
#include <string>

namespace Weltall
{
	namespace
	{
		constexpr uint16 StartButton = 0x0800;
		/** Held, Cross keeps Start from pausing. */
		constexpr uint16 CrossButton = 0x0040;
		constexpr uint16 MenuButton = 0x0010;
		constexpr uint16 L2Button = 0x0001;
		constexpr uint16 R2Button = 0x0002;
		/** The leader's floor forbids boarding or leaving the gears. */
		constexpr uint32 NoBoardingFloor = 0xC0;
		constexpr uint16 NoBoardingOverride = 0xFF;
		constexpr uint32 PlayerMenu = 0x80;
		constexpr uint8 NoGear = 0xFF;
		/** Variable 0x002's place among the saved variables: the next map's entrance. */
		constexpr int32 SavedEntrance = 1;
		constexpr uint8 BattleProgram = 2;
		constexpr uint8 WorldMapProgram = 3;
		constexpr uint8 BattlingProgram = 4;
		constexpr uint16 ProgramMask = 0x7F;
		constexpr uint16 NewGameBit = 0x80;
		constexpr uint16 SelectButton = 0x0100;
		constexpr uint8 LoadGameMenu = 2;
		/** Load Game's results: New Game, a game loaded. */
		constexpr uint8 NewGameResult = 0;
		constexpr uint8 LoadedResult = 2;
		constexpr uint32 ModeResultVariable = 0x046;
		constexpr uint32 LeftMapVariable = 0x004;
		constexpr uint16 NewGameMap = 4;
		/** Variable 0x054's place among the saved variables: the entrance a loaded game resumes at. */
		constexpr int32 SavedLoadedEntrance = 0x2A;
		constexpr uint16 PlaceBits = 0x3FFF;
		constexpr uint16 FirstWorldMapPlace = 0x400;

		/**
		 * A map with Select's picture: the map shown while the party holds Item, the scales (16.16) and
		 * the offsets that put the leader's place on it, and whether the parts not yet seen are hidden.
		 */
		struct FMapPictureEntry
		{
			int32 Map;
			int32 ScaleX;
			int32 ScaleZ;
			/** Counted from the first picture's file. */
			int32 Picture;
			uint8 Item;
			int16 OffsetX;
			int16 OffsetY;
			bool bHidesUnseenParts;
		};

		constexpr int32 FirstPictureFile = 0x7FB;
		constexpr std::array<FMapPictureEntry, 10> MapPictures = {{
			{239, 2699, 2674, 3, 0x61, 160, 112, false},
			{240, 2699, 2674, 4, 0x61, 168, 135, false},
			{241, 2699, 2674, 5, 0x61, 152, 124, false},
			{706, 884, 834, 6, 0x62, 152, 122, true},
			{703, 1003, 999, 8, 0x62, 135, 123, false},
			{707, 1385, 1376, 10, 0x62, 188, 80, false},
			{708, 1061, 1061, 9, 0x62, 141, 146, false},
			{445, 2853, 2793, 11, 0x63, 243, 70, false},
			{446, 2853, 2793, 11, 0x63, 243, 70, false},
			{447, 2853, 2793, 11, 0x63, 81, 109, false},
		}};
		/** The map word's bits the pictures' maps are compared with. */
		constexpr int32 PictureMapBits = 0x3FFF;
		/** The saved variable whose bits 3 to 7 tell the parts of the picture seen. */
		constexpr int32 SeenPartsVariable = 0x73;
		constexpr uint16 FirstSeenPartBit = 0x08;
		/** The marker's top left from the leader's place. */
		constexpr int32 MarkerOffsetX = -4;
		constexpr int32 MarkerOffsetY = -12;

		/** Whether the party holds Item: a slot of the first inventory list with it and a count. */
		bool HoldsItem(FGameState& Game, uint8 Item)
		{
			const std::span<const uint8> Items = Game.InventoryItems(0);
			const std::span<const uint8> Counts = Game.InventoryCounts(0);
			for (std::size_t Slot = 0; Slot < Items.size(); ++Slot)
			{
				if (Items[Slot] == Item && Counts[Slot] != 0)
				{
					return true;
				}
			}
			return false;
		}

		/** The leader stands on the ground, neither airborne nor falling; a leader without a script state counts as standing. */
		bool LeaderGrounded(const FEventContext& Context, uint32 Flags)
		{
			const FEventActorState* Leader = Context.States.size() > Context.LeaderActor ? &Context.States[Context.LeaderActor] : nullptr;
			return Leader == nullptr || (Leader->Flags0 & Flags) == 0;
		}

		bool SongLoading(const FEventContext& Context)
		{
			return Context.Songs != nullptr && Context.Songs->IsLoading();
		}
	}

	namespace FieldMainLoop
	{
		bool IsIdle(const FEventContext& Context)
		{
			return !SongLoading(Context) && !Context.bExtraAnimationsLoading && !Context.Render.bEffectTexturesSaved && !Context.MemberLoad.bPending;
		}

		bool PausesOnStart(const FEventContext& Context)
		{
			return (Context.Controls.Repeated & StartButton) != 0 && (Context.PadHeld & CrossButton) == 0 && Context.PauseDisabled == 0;
		}

		bool BattleStarts(const FEventContext& Context)
		{
			if (!Context.bBattleRequested || !IsIdle(Context))
			{
				return false;
			}
			if (Context.Encounters.bBattleDrawn)
			{
				return Context.Movement.ActionMode != 0 || LeaderGrounded(Context, EventActorFlags0::Airborne);
			}
			return Context.DrawBuffer == ProgramBuffer;
		}

		bool MapChangeGoesAhead(FEventContext& Context)
		{
			if (!Context.Requests.bMapChange || SongLoading(Context) || Context.MemberLoad.bPending || Context.bExtraAnimationsLoading)
			{
				return false;
			}
			constexpr int32 MapBits = 0xFFF;
			const int32 File = Context.MapNumber & MapBits;
			if (Context.MapFileRead != File)
			{
				Context.MapFileRead = File;
				return false;
			}
			return Context.Render.Fades[0].Ticks == 0;
		}

		EFieldRequest ProgramStarting(const FEventContext& Context)
		{
			if (Context.DrawBuffer != ProgramBuffer || !IsIdle(Context))
			{
				return EFieldRequest::None;
			}
			if (Context.Requests.bWorldMap)
			{
				return EFieldRequest::WorldMap;
			}
			if (Context.Requests.bBattling)
			{
				return EFieldRequest::Battling;
			}
			if (Context.Requests.bProgram)
			{
				return EFieldRequest::Program;
			}
			return EFieldRequest::None;
		}

		bool TakesUpPlayer(const FEventContext& Context)
		{
			return !Context.Requests.bProgram && !Context.bBattleRequested && !Context.Requests.bWorldMap && IsIdle(Context) && !Context.Requests.bMapChange;
		}

		bool IsGearBoardingLocked(const FEventContext& Context)
		{
			if (Context.GearBoardingOverride != NoBoardingOverride)
			{
				return static_cast<uint8>(Context.GearBoardingOverride) != 0;
			}
			const FEventActorState* Leader = Context.States.size() > Context.LeaderActor ? &Context.States[Context.LeaderActor] : nullptr;
			return Context.PilotSpriteBase == 0 || (Leader != nullptr && (Leader->FloorAttributes & NoBoardingFloor) != 0);
		}

		bool TakeGearBoarding(FEventContext& Context, bool& bButtonsHeld, std::array<bool, 3>& Slots)
		{
			if ((Context.PadHeld & (L2Button | R2Button)) == 0)
			{
				bButtonsHeld = false;
			}
			if ((Context.PadHeld & L2Button) == 0 || !Context.bPlayerControlled || (Context.PadHeld & R2Button) == 0 || bButtonsHeld)
			{
				return false;
			}
			bButtonsHeld = true;
			Context.bGearsLocked = IsGearBoardingLocked(Context);
			if (Context.MenuRequest != FEventContext::NoMenu || !LeaderGrounded(Context, EventActorFlags0::InTheAir) || Context.bGearsLocked
				|| Context.GameState == nullptr)
			{
				return false;
			}
			const FGameState& Game = *Context.GameState;
			// The slots riding exactly as the first slot's member rides (1) or walks (0).
			const uint8 Riding = Game.PartySlotInGear(0) == 0 ? 0 : 1;
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				// An empty slot's character (FF) reads past the characters' records in the original, whose
				// byte there never matters: the slot has no actor to board.
				const std::optional<uint8> Gear = Game.CharacterGear(Context.PartyCharacters[static_cast<std::size_t>(Slot)]);
				Slots[static_cast<std::size_t>(Slot)] = Game.PartySlotInGear(Slot) == Riding && Gear.value_or(NoGear) != NoGear;
			}
			return true;
		}

		bool SelectShowsPicture(const FEventContext& Context)
		{
			return (Context.Controls.Repeated & SelectButton) != 0 && !Context.Requests.bMapChange && Context.bPlayerControlled;
		}

		std::optional<FFieldMapPicture> MapPicture(const FEventContext& Context)
		{
			const int32 Map = Context.MapNumber & PictureMapBits;
			const auto Entry = std::find_if(MapPictures.begin(), MapPictures.end(), [Map](const FMapPictureEntry& Candidate) { return Candidate.Map == Map; });
			if (Entry == MapPictures.end() || Context.GameState == nullptr || !HoldsItem(*Context.GameState, Entry->Item))
			{
				return std::nullopt;
			}
			FFieldMapPicture Picture;
			Picture.File = FirstPictureFile + Entry->Picture;
			if (Entry->bHidesUnseenParts)
			{
				const uint16 Seen = Context.GameState->SavedEventVariable(SeenPartsVariable);
				for (std::size_t Part = 0; Part < FFieldMapPicture::Parts.size(); ++Part)
				{
					const bool bFlag = (Seen & (FirstSeenPartBit << Part)) != 0;
					// The last part's flag reads the other way: set, it hides the part.
					const bool bHidden = Part + 1 == FFieldMapPicture::Parts.size() ? bFlag : !bFlag;
					Picture.HiddenParts |= static_cast<uint8>(bHidden ? 1u << Part : 0u);
				}
			}
			const FEventActorState* Leader = Context.States.size() > Context.LeaderActor ? &Context.States[Context.LeaderActor] : nullptr;
			const int32 X = Leader != nullptr ? static_cast<int16>(static_cast<uint32>(Leader->Position[0]) >> 16) : 0;
			const int32 Z = Leader != nullptr ? static_cast<int16>(static_cast<uint32>(Leader->Position[2]) >> 16) : 0;
			Picture.MarkerX = static_cast<int16>((X * Entry->ScaleX >> 16) + MarkerOffsetX + Entry->OffsetX);
			Picture.MarkerY = static_cast<int16>((-(Z * Entry->ScaleZ) >> 16) + MarkerOffsetY + Entry->OffsetY);
			return Picture;
		}

		bool MoviePlays(const FEventContext& Context)
		{
			return Context.Requests.bMovie && Context.DrawBuffer == ProgramBuffer;
		}

		bool MenuOpens(const FEventContext& Context)
		{
			return Context.MenuRequest != FEventContext::NoMenu && Context.DrawBuffer != ProgramBuffer
				&& LeaderGrounded(Context, EventActorFlags0::InTheAir);
		}

		bool TakeMenuButton(FEventContext& Context)
		{
			if ((Context.Controls.Repeated & MenuButton) == 0 || Context.bMenuLocked || Context.MenuRequest != FEventContext::NoMenu || !Context.bPlayerControlled)
			{
				return false;
			}
			Context.MenuRequest = PlayerMenu;
			Context.MenuProgramParam = Context.MenuParam;
			return true;
		}

		bool MenuStaysShut(const FEventContext& Context)
		{
			return Context.MenuRequest != FEventContext::NoMenu && (Context.MenuRequest & PlayerMenu) != 0 && Context.bMenuLocked;
		}

		void TakeLoadGameResult(FEventContext& Context, uint8 MenuMode, uint8 ModeResult)
		{
			FGameState& Game = *Context.GameState;
			if (ModeResult == NewGameResult && MenuMode == LoadGameMenu)
			{
				Context.bFrameVariablesFrozen = true;
				Context.Variables.Set(ModeResultVariable, NewGameResult);
				Context.Variables.Set(LeftMapVariable, NewGameMap);
				Context.MapNumber = NewGameMap;
				Game.SetFieldEntrance(0);
				Game.SetSavedEventVariable(SavedEntrance, 0);
				Game.SetPlace(NewGameMap);
			}
			if (ModeResult == LoadedResult)
			{
				Context.bFrameVariablesFrozen = true;
				Context.Variables.Set(ModeResultVariable, LoadedResult);
				const uint16 Place = Game.Place() & PlaceBits;
				Context.Variables.Set(LeftMapVariable, Place);
				if (Place < FirstWorldMapPlace)
				{
					Game.SetFieldEntrance(Game.SavedEventVariable(SavedLoadedEntrance));
				}
			}
		}

		std::array<bool, 3> GearsChangedByMenu(const FEventContext& Context, const std::array<uint8, 3>& Riding)
		{
			std::array<bool, 3> Changed{};
			if (Context.PilotSpriteBase == 0 || Context.GameState == nullptr)
			{
				return Changed;
			}
			for (std::size_t Slot = 0; Slot < Changed.size(); ++Slot)
			{
				Changed[Slot] = Context.PartyActors[Slot] != FEventContext::NoActor
					&& Context.GameState->PartySlotInGear(static_cast<int32>(Slot)) != Riding[Slot];
			}
			return Changed;
		}

		std::optional<FError> LeaveField(FEventContext& Context, FFieldRequest& Request)
		{
			Context.Render.RestoreEffectTextures();
			if (auto Error = UpdateFieldFrameVariables(Context, Context.PlayClock))
			{
				return Error;
			}
			for (int32 Effect = 0; Effect < FFieldParticles::EffectCount; ++Effect)
			{
				Context.Particles.End(Effect);
			}
			StopFieldActorSoundsForExit(Context);
			Context.Messages.CloseAll();
			if (Context.PartySprites != nullptr)
			{
				Context.PartySprites->ForgetLoadedKind();
			}
			FGameState& Game = *Context.GameState;
			switch (Request.Kind)
			{
			case EFieldRequest::Battle:
			{
				const FEventActorState* Leader = Context.StateOf(Context.LeaderActor);
				if (Leader == nullptr)
				{
					return MakeError("the field leaves for a battle, and its leader, actor " + std::to_string(Context.LeaderActor) + ", has no script state");
				}
				SaveFieldVariables(Context, *Leader);
				Game.SetFieldSong(static_cast<uint16>(Context.Songs->ChosenSong()));
				Game.SetFieldEntrance(Game.SavedEventVariable(SavedEntrance));
				Request.NextProgram = BattleProgram;
				Request.bFieldKept = !Context.Requests.bBattleNamesNextMap;
				Request.Formation = Context.Encounters.Formation;
				Request.bEncounter = Context.Encounters.bBattleDrawn;
				Request.BattleKind = Request.bEncounter ? Context.EncounterBattleKind : Context.Requests.BattleKind;
				Request.BattleSong = Request.bEncounter ? FFieldEncounters::BattleSong : 0;
				break;
			}
			case EFieldRequest::WorldMap:
				Request.NextProgram = WorldMapProgram;
				break;
			case EFieldRequest::Battling:
				Game.SetFieldSong(static_cast<uint16>(Context.Songs->ChosenSong()));
				Game.SetFieldEntrance(Game.SavedEventVariable(SavedEntrance));
				Request.NextProgram = BattlingProgram;
				Request.bFieldKept = true;
				Request.BattlingParameters = Context.Requests.BattlingParameters;
				break;
			case EFieldRequest::Program:
				Request.NextProgram = static_cast<uint8>(Context.Requests.Program & ProgramMask);
				Request.bNewGame = (Context.Requests.Program & NewGameBit) != 0;
				break;
			default:
				break;
			}
			return std::nullopt;
		}
	}
}
