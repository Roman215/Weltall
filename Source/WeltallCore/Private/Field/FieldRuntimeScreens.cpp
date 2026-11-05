#include "Field/EventMachine.h"
#include "Field/FieldGearBoarding.h"
#include "Field/FieldGears.h"
#include "Field/FieldMainLoop.h"
#include "Field/FieldPartySprites.h"
#include "Field/FieldPartyChange.h"
#include "Field/FieldRuntime.h"
#include "Field/FieldTick.h"
#include "Game/GameState.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 MainLoopStrayWait = 0xFF;
		constexpr uint16 SelectButton = 0x0100;
		/** Select's picture brightens over 16 frames, from 0 by 8, and darkens over 16 from full. */
		constexpr int32 MapPictureFadeFrames = 16;
		constexpr int32 MapPictureFadeStep = 8;
		/** The menu the field reloads the party's sprites and the map after: Member Change, as the scripts ask for it. */
		constexpr uint32 MemberChangeMenu = 1;
		constexpr uint32 MenuModeBits = 0x7F;
		constexpr uint32 PlayerMenu = 0x80;
		/** The map's script the field runs after the other menus: the first actor's third. */
		constexpr int32 AfterMenuScript = 3;
		constexpr int32 GearModeBits = 0xC000;
		/**
		 * After a movie shown over the screen, which wrote over VRAM past the screen's columns, the map's textures stream
		 * in again and the field comes back through a dissolve of 32 ticks from the movie's last picture.
		 */
		constexpr uint8 DissolveFromStill = 1;
		constexpr int32 MovieDissolveTicks = 0x20;
		/**
		 * The menu's shading of the field's picture: 32 darkening steps (0 to 31) before it; after it two
		 * steps held at 31 while the field's VRAM is put back, then 33 lightening ones (31 down to 0, then
		 * 0 again), or 31 on toward black (32 to 62) when the scripts keep the screen dark.
		 */
		constexpr int32 MenuDarkeningFrames = 32;
		constexpr int32 MenuHeldFrames = 2;
		constexpr int32 MenuLighteningFrames = 33;
		constexpr int32 MenuKeptDarkFrames = 31;
		constexpr int32 MenuDarkest = MenuDarkeningFrames - 1;
	}

	void FFieldRuntime::BeginMapPicture(const FFieldMapPicture& Picture)
	{
		FieldContext.Render.MapPicture = Picture;
		FramesLeft = MapPictureFadeFrames;
		Phase = EPhase::MapPictureIn;
	}

	std::optional<FError> FFieldRuntime::TickMapPicture()
	{
		// Each of the picture's frames swaps the draw buffers; only the frames it is held on read the pad.
		FFieldMapPicture& Picture = FieldContext.Render.MapPicture;
		Picture.bShown = true;
		FieldContext.DrawBuffer ^= 1;
		switch (Phase)
		{
		case EPhase::MapPictureIn:
			Picture.Brightness = static_cast<uint8>((MapPictureFadeFrames - FramesLeft) * MapPictureFadeStep);
			if (--FramesLeft == 0)
			{
				Phase = EPhase::MapPictureHeld;
			}
			return std::nullopt;
		case EPhase::MapPictureHeld:
			Picture.Brightness = FFieldMapPicture::FullBrightness;
			ReadFieldInput(FieldContext, TakeInput().Pad);
			if ((FieldContext.Controls.Repeated & SelectButton) != 0)
			{
				FramesLeft = MapPictureFadeFrames;
				Phase = EPhase::MapPictureOut;
			}
			return std::nullopt;
		default:
			Picture.Brightness = static_cast<uint8>(FramesLeft * MapPictureFadeStep);
			if (--FramesLeft != 0)
			{
				return std::nullopt;
			}
			Phase = EPhase::MainLoop;
			return IdleChecks(EStep::AfterMapPicture);
		}
	}

	std::optional<FError> FFieldRuntime::TickMovieLead()
	{
		// Before the movie the field waits for its reads with ticks of its own (no music's poll, no
		// requests), until a tick has drawn into buffer 0; then again, at least one tick.
		if (auto Error = RunFrame(MainLoopStrayWait, false))
		{
			return Error;
		}
		if (FieldContext.DrawBuffer != 0)
		{
			return std::nullopt;
		}
		if (Phase == EPhase::MovieLead)
		{
			Phase = EPhase::MovieLeadAgain;
			return std::nullopt;
		}
		StartMovie();
		return std::nullopt;
	}

	void FFieldRuntime::StartMovie()
	{
		FEventContext& Context = FieldContext;
		FreeFieldGearsForMovie(Context);
		for (int32 Effect = 0; Effect < FFieldParticles::EffectCount; ++Effect)
		{
			Context.Particles.End(Effect);
		}
		Context.Movie.Frame = 0;
		Context.Movie.bStarted = true;
		FFieldRequest Movie;
		Movie.Kind = EFieldRequest::Movie;
		Movie.Movie = Context.Requests.Movie;
		Movie.MovieView = Context.MovieView;
		PendingRequest = Movie;
		Phase = EPhase::MovieFirstPicture;
	}

	std::optional<FError> FFieldRuntime::EndMovie()
	{
		FEventContext& Context = FieldContext;
		TakeGearLoadColors();
		if (auto Error = ReloadFieldGears(Context))
		{
			return Error;
		}
		if (Context.MovieView != EFieldMovieView::InScene)
		{
			Context.Render.StreamTexturesAgain();
			Context.Requests.DissolveTicks = MovieDissolveTicks;
			Context.Requests.Dissolve = DissolveFromStill;
		}
		Context.Render.Panorama.bHidden = Context.ReturnFade != 0;
		Context.Requests.Movie.Cues = FFieldMovieRequest::NoCues;
		Context.MovieView = EFieldMovieView::FullScreen;
		Context.Requests.bMovie = false;
		return std::nullopt;
	}

	void FFieldRuntime::OpenMenu()
	{
		FEventContext& Context = FieldContext;
		FFieldRequest& Menu = MenuRequested;
		Menu = {};
		Menu.Kind = EFieldRequest::Menu;
		Menu.MenuMode = static_cast<uint8>(Context.MenuRequest & MenuModeBits);
		Menu.bMenuByButton = (Context.MenuRequest & PlayerMenu) != 0;
		Menu.MenuParam = Context.MenuProgramParam;
		Context.bGearsLocked = FieldMainLoop::IsGearBoardingLocked(Context);
		Menu.bGearBoardingLocked = Context.bGearsLocked;
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			RidingAtMenu[static_cast<std::size_t>(Slot)] = Services.Game->PartySlotInGear(Slot);
		}
		CharactersAtMenu = Services.Game->Party();
		// The field's last picture darkens before the menu opens over it, a step a vertical blank.
		MenuShadeStep = 0;
		FramesLeft = MenuDarkeningFrames;
		Phase = EPhase::MenuShadeOut;
	}

	void FFieldRuntime::BeginMenuLightening(uint8 ModeResult)
	{
		FEventContext& Context = FieldContext;
		FieldMainLoop::TakeLoadGameResult(Context, static_cast<uint8>(Context.MenuRequest & MenuModeBits), ModeResult);
		MenuShadeStep = 0;
		FramesLeft = MenuHeldFrames + (Context.ReturnFade == 0 ? MenuLighteningFrames : MenuKeptDarkFrames);
		Phase = EPhase::MenuShadeIn;
	}

	std::optional<FError> FFieldRuntime::TickMenuShade()
	{
		// Every step draws the field's last picture under the grey; the field itself stands still.
		FFieldMenuShade& Shade = FieldContext.Render.MenuShade;
		Shade.bShown = true;
		const int32 Step = MenuShadeStep++;
		--FramesLeft;
		if (Phase == EPhase::MenuShadeOut)
		{
			Shade.Level = static_cast<uint8>(Step);
			if (FramesLeft == 0)
			{
				Stop(MenuRequested, EStep::AfterMenu);
			}
			return std::nullopt;
		}
		// Back from the darkest step, or on toward black when the scripts keep the screen dark (FE 86).
		const int32 Moved = Step - MenuHeldFrames;
		const int32 Level = FieldContext.ReturnFade != 0 ? MenuDarkest + 1 + Moved : std::max(MenuDarkest - Moved, 0);
		Shade.Level = static_cast<uint8>(Moved < 0 ? MenuDarkest : Level);
		if (FramesLeft != 0)
		{
			return std::nullopt;
		}
		// As after a movie, a screen the scripts keep dark hides the backdrop until the next map.
		FieldContext.Render.Panorama.bHidden = FieldContext.ReturnFade != 0;
		Phase = EPhase::MainLoop;
		if (auto Error = CloseMenu())
		{
			return Error;
		}
		return IdleChecks(EStep::AfterMenu);
	}

	std::optional<FError> FFieldRuntime::CloseMenu()
	{
		FEventContext& Context = FieldContext;
		FPartySpriteFiles& Party = *Services.PartySprites;
		if (Context.MenuRequest == MemberChangeMenu)
		{
			// The new party's sprite files, then the map again (or the one FE CF named), its sprites left alone until then.
			for (int32 Slot = 0; Slot < FPartySpriteFiles::Slots; ++Slot)
			{
				Party.SetFile(Slot, FPartySpriteFiles::NoFile);
			}
			Party.ForgetLoadedKind();
			if (auto Error = Party.LoadForMap((Context.MapNumber & GearModeBits) != 0, *Services.Game, Context.PartyCharacters))
			{
				return Error;
			}
			Context.Requests.bMapChange = true;
			Context.bActorSpritesReleased = true;
		}
		else
		{
			if ((Context.MenuRequest & PlayerMenu) != 0 && Services.Game->Party() != CharactersAtMenu)
			{
				if (auto Error = RefreshFieldParty(Context, Party, *Services.Game))
				{
					return Error;
				}
			}
			else
			{
				for (int32 Slot = 0; Slot < FPartySpriteFiles::Slots; ++Slot)
				{
					if (Party.File(Slot) != FPartySpriteFiles::NoFile)
					{
						if (auto Error = Party.Unpack(Slot, Party.File(Slot)))
						{
							return Error;
						}
					}
				}
			}
			Context.bAfterMenuPass = true;
			std::optional<FError> Error = FEventMachine::RunScriptAside(Context, AfterMenuScript);
			if (!Error)
			{
				// The gears whose riding the menu changed are boarded or left.
				Error = BoardOrLeaveFieldGears(Context, FieldMainLoop::GearsChangedByMenu(Context, RidingAtMenu), false);
			}
			Context.bAfterMenuPass = false;
			if (Error)
			{
				return Error;
			}
		}
		Context.MenuRequest = FEventContext::NoMenu;
		Context.MenusPending = 0;
		return std::nullopt;
	}
}
