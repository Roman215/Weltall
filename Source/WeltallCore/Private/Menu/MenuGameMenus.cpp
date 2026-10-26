#include "Menu/MenuGameMenus.h"

#include "Field/FieldRequests.h"
#include "Game/GameSession.h"
#include "Menu/MenuGameSound.h"
#include "Menu/MenuProgram.h"
#include "Menu/MenuResources.h"
#include "Sound/GameSoundSession.h"
#include "Story/StoryPacks.h"

namespace Weltall
{
	/** The player's settings as the session holds them: a change taken at once, kept by the host as the screen closes. */
	class FMenuGameSettings final : public IMenuSettings
	{
	public:
		FMenuGameSettings(FMenuGameMenus& InOwner, FGameSession& InSession) : Owner(InOwner), Session(InSession) {}

		const FPlayerSettings& Settings() const override { return Session.Settings(); }
		void Change(const FPlayerSettings& Settings) override
		{
			Session.SetSettings(Settings);
			if (Owner.SettingsFollowed)
			{
				Owner.SettingsFollowed(Settings);
			}
		}
		void Keep() override { Owner.SettingsKept(Session.Settings()); }
		FDisplayModes DisplayModes() const override { return Owner.DisplayModes ? Owner.DisplayModes() : FDisplayModes{}; }
		IMenuAssets* Assets() const override { return Owner.Assets; }
		IMenuDeviceFiles* DeviceFiles() const override { return Owner.DeviceFiles; }
		FMenuMods Mods() const override { return Owner.Mods ? Owner.Mods() : FMenuMods{}; }
		FSoundSources SoundSources() const override { return Session.SoundSources(); }

	private:
		FMenuGameMenus& Owner;
		FGameSession& Session;
	};

	/** One menu the field runs inside itself, a vertical blank a frame. */
	class FMenuGameProgram final : public IGameProgram
	{
	public:
		FMenuGameProgram(FMenuGameMenus& InOwner, FGameSession& InSession, FMenuResources InResources, const FMenuRequest& Request)
			: Owner(InOwner), Session(InSession), Resources(std::move(InResources)), Settings(InOwner, InSession),
			  Sound(*InSession.Sound().Sound(), InSession.Sound().Resident().MainEffects), Program(Services(), Request)
		{
			Owner.Current = &Program;
		}

		~FMenuGameProgram() override
		{
			if (Owner.Current == &Program)
			{
				Owner.Current = nullptr;
			}
		}

		int32 VblanksPerFrame() const override { return 1; }

		std::optional<FError> Frame(std::span<const FPadState> HeldPerVblank) override
		{
			for (const FPadState Held : HeldPerVblank)
			{
				Session.Vblank(Held);
			}
			TResult<bool> Drawn = Program.Step();
			if (!Drawn)
			{
				return Drawn.Error();
			}
			return std::nullopt;
		}

		EGameProgramState State() const override
		{
			if (!Program.HasEnded())
			{
				return EGameProgramState::Running;
			}
			return Program.EndedBySoftReset() ? EGameProgramState::SoftReset : EGameProgramState::Ended;
		}

	private:
		FMenuServices Services()
		{
			FMenuServices Result;
			Result.Image = &Session.Image();
			Result.Index = &Session.Index();
			Result.Resources = &Resources;
			Result.Game = &Session.Game();
			Result.Random = &Session.Random();
			Result.Pad = &Session.Pad();
			Result.Clock = &Session.Clock();
			Result.Sound = &Sound;
			Result.Text.Font = &Session.Text().Font;
			Result.Text.Tables = &Session.Text().NameTables;
			Result.Text.State = &Session.Names();
			Result.Text.ButtonMap = MesKernelButtonMap;
			Result.Text.Speed = Session.Settings().TextSpeed;
			Result.Disc = Session.DiscNumber() + 1;
			Result.Kernel = &Session.Kernel();
			Result.Saves = Session.Saves();
			Result.Options = Session.Settings().Fixes.Menu;
			Result.LoadOptions = Session.Settings().Fixes.Load;
			Result.Settings = Owner.SettingsKept ? &Settings : nullptr;
			Result.Keys = Owner.SettingsKept ? Owner.Keys : nullptr;
			Result.Discs = &Discs;
			Result.Quit = Owner.Quit;
			Result.bLoadsInPlay = true;
			Result.bArrangesParty = true;
			Result.bStoryExpansion = Session.Story() != nullptr && Session.Story()->HasExpansion();
			Result.WayBack = Session.Story() != nullptr ? Session.Story()->WayBack() : nullptr;
			return Result;
		}

		/** The session's discs, which its host's drive looks for. */
		class FSessionDiscs final : public IMenuDiscs
		{
		public:
			explicit FSessionDiscs(FGameSession& InSession) : Session(InSession) {}
			bool HasDisc(int32 Disc) override { return Session.HasDisc(Disc); }

		private:
			FGameSession& Session;
		};

		FMenuGameMenus& Owner;
		FGameSession& Session;
		FMenuResources Resources;
		FMenuGameSettings Settings;
		FMenuGameSound Sound;
		FSessionDiscs Discs{Session};
		FMenuProgram Program;
	};

	TResult<std::unique_ptr<IGameProgram>> FMenuGameMenus::Open(FGameSession& Session, const FFieldRequest& Request)
	{
		TResult<FMenuResources> Resources = FMenuResources::Load(Session.Image(), Session.Index(), Executable);
		if (!Resources)
		{
			return Resources.Error();
		}
		FMenuRequest Menu;
		Menu.Mode = static_cast<EMenuMode>(Request.MenuMode);
		Menu.Param = Request.MenuParam;
		Menu.bGearBoardingLocked = Request.bGearBoardingLocked;
		return std::unique_ptr<IGameProgram>(std::make_unique<FMenuGameProgram>(*this, Session, std::move(*Resources), Menu));
	}
}
