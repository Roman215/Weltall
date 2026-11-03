#include "Battle/Show/BattlePresentation.h"

#include "Battle/BattleContext.h"
#include "Foundation/LittleEndian.h"
#include "Media/PsxExecutable.h"

namespace Weltall
{
	namespace
	{
		/** The kernel's table of the kinds' start sounds, three bytes a kind. */
		constexpr uint32 StartSoundsAddress = 0x8004F388;
		/** Where a `seds` file keeps its set's id. */
		constexpr std::size_t PresentationSedsSetId = 0x14;
		/** The message box the action's name is typed into and shown in. */
		constexpr int32 PresentationActionNameBox = 7;
	}

	TResult<FBattleStartSounds> FBattleStartSounds::Read(const FPsxExecutable& Executable)
	{
		const std::span<const uint8> Table = Executable.Read(StartSoundsAddress, KindCount * PerKind);
		if (Table.empty())
		{
			return MakeError("the executable holds no battle start sounds");
		}
		FBattleStartSounds Sounds;
		for (std::size_t Kind = 0; Kind < Sounds.Kinds.size(); ++Kind)
		{
			for (std::size_t Effect = 0; Effect < PerKind; ++Effect)
			{
				Sounds.Kinds[Kind][Effect] = Table[Kind * PerKind + Effect];
			}
		}
		return Sounds;
	}

	FBattlePresentation::FBattlePresentation(const FBattlePresentationFiles& InFiles, IBattleShowSound* InSound, IBattleShowHud* InHud)
		: Files(InFiles)
		, Sound(InSound)
		, Hud(InHud)
	{
	}

	FBattlePresentation::~FBattlePresentation()
	{
		if (bBegun)
		{
			Show.End();
		}
	}

	void FBattlePresentation::FrameCamera(uint16 Slots)
	{
		Show.FrameCamera(Slots);
	}

	void FBattlePresentation::SetMessageBox(uint8 Box, uint8 Entry)
	{
		if (Hud != nullptr)
		{
			Hud->TypeMessage(Box, Entry);
		}
	}

	void FBattlePresentation::ShowNameBox(uint8 Entry)
	{
		if (Hud != nullptr)
		{
			Hud->ShowNameBox(Entry);
		}
	}

	void FBattlePresentation::BeginPlayback(bool bStays, uint8 Actor, uint8 Target, uint8 NextReady)
	{
		Show.BeginPlayback(bStays, Actor, Target, NextReady);
	}

	void FBattlePresentation::CancelPlayback()
	{
		Show.CancelPlayback();
	}

	void FBattlePresentation::SetTargetMarkers(uint16 Slots)
	{
		Show.SetTargetMarkers(Slots);
	}

	void FBattlePresentation::PlaySound(uint8 Effect)
	{
		Show.PlayCommonSound(Effect);
	}

	void FBattlePresentation::BoardGear(uint8 Slot)
	{
		Show.BoardGear(Slot);
	}

	void FBattlePresentation::ShowTurnDamage(uint8 Slot, const std::array<uint32, 3>& Amounts)
	{
		Show.ShowTurnDamage(Slot, Amounts);
	}

	void FBattlePresentation::BeginEvent()
	{
		Show.BeginEvent();
	}

	void FBattlePresentation::LoadBattleBank()
	{
		Show.LoadSystemSoundBank();
	}

	void FBattlePresentation::PlayEventCue(const FBattleEventCue& Cue)
	{
		switch (Cue.Cue)
		{
		case EBattleEventCue::OpenWindow:
		case EBattleEventCue::ShowPortrait:
		case EBattleEventCue::CloseWindow:
		case EBattleEventCue::OpenCommandHud:
		case EBattleEventCue::CloseCommandHud:
			if (Hud != nullptr)
			{
				Hud->PlayEventCue(Cue);
			}
			break;
		default:
			Show.PlayEventCue(Cue);
			break;
		}
	}

	bool FBattlePresentation::IsBlocking() const
	{
		return Show.HoldsRules();
	}

	void FBattlePresentation::PlayTurnCue(EBattleTurnCue Cue, uint8 Arg)
	{
		switch (Cue)
		{
		case EBattleTurnCue::ActionName:
			if (Hud != nullptr)
			{
				Hud->TypeActionName(Arg);
			}
			break;
		case EBattleTurnCue::ActionNameBox:
			if (Hud != nullptr)
			{
				Hud->ShowMessageBox(PresentationActionNameBox, Arg != 0);
			}
			break;
		case EBattleTurnCue::HideMessageBoxes:
			if (Hud != nullptr)
			{
				Hud->HideMessageBoxes();
			}
			break;
		case EBattleTurnCue::ReadyNext:
			Show.PrepareNext(Arg);
			break;
		case EBattleTurnCue::RefreshPoses:
			Show.RefreshPoses();
			break;
		}
	}

	void FBattlePresentation::PlayMenuCue(EBattleMenuCue Cue, uint8 Slot, uint8 Arg)
	{
		if (Hud != nullptr)
		{
			Hud->PlayMenuCue(Cue, Slot, Arg);
		}
	}

	void FBattlePresentation::PlayEndCue(EBattleEndCue Cue, uint8 Arg)
	{
		switch (Cue)
		{
		case EBattleEndCue::FinishLoads:
			Show.FinishLoads();
			break;
		case EBattleEndCue::Pose:
			Show.PlayEndPose(static_cast<EBattleEndPose>(Arg));
			break;
		case EBattleEndCue::Dim:
			Show.Dim();
			break;
		case EBattleEndCue::FadeMusic:
			Show.FadeMusic();
			break;
		case EBattleEndCue::ResultSound:
			Show.PlaySound(static_cast<uint32>(Show.Battle.SystemSoundSet) << 16 | Arg);
			break;
		case EBattleEndCue::StopSounds:
			if (Sound != nullptr)
			{
				Sound->StopAllEffects();
			}
			break;
		}
	}

	void FBattlePresentation::BeginBattle(FBattleContext& InContext)
	{
		Context = &InContext;
	}

	void FBattlePresentation::PlayOpeningCue(EBattleOpeningCue Cue, uint8 Arg)
	{
		switch (Cue)
		{
		case EBattleOpeningCue::StartSounds:
			PlayStartSounds(Arg);
			break;
		case EBattleOpeningCue::SetupFinished:
			if (Hud != nullptr)
			{
				Hud->Begin(*Context, Show.Vram);
			}
			break;
		case EBattleOpeningCue::Presentation:
			BeginShow(Arg != 0);
			break;
		case EBattleOpeningCue::MessageBoxes:
			if (Hud != nullptr)
			{
				Hud->BeginMessageBoxes();
			}
			break;
		case EBattleOpeningCue::StartSoundsReleased:
			ReleaseStartSounds(Arg != 0);
			break;
		}
	}

	void FBattlePresentation::PlayStartSounds(uint8 InKind)
	{
		Kind = InKind;
		TResult<std::vector<uint8>> Seds = Files.SpriteFiles->ReadStartSounds();
		if (!Seds || Seds->size() < PresentationSedsSetId + 2)
		{
			Show.Stop(EBattleShowFault::File);
			return;
		}
		StartSeds = std::move(*Seds);
		if (Sound != nullptr)
		{
			Sound->RegisterEffects(StartSeds);
		}
		if (Kind == FBattleStartSounds::SilentKind)
		{
			return;
		}
		if (Kind >= FBattleStartSounds::KindCount)
		{
			Show.Stop(EBattleShowFault::StartSounds);
			return;
		}
		const uint32 Set = static_cast<uint32>(LittleEndian::ReadU16(StartSeds, PresentationSedsSetId)) << 16;
		for (const uint8 Effect : Files.StartSounds->Kinds[Kind])
		{
			if (Effect != FBattleStartSounds::None && Sound != nullptr)
			{
				Sound->PlaySystemEffect(Set | Effect);
			}
		}
	}

	void FBattlePresentation::ReleaseStartSounds(bool bStopEffects)
	{
		if (Sound == nullptr || StartSeds.empty())
		{
			return;
		}
		if (bStopEffects)
		{
			Sound->StopEffects(StartSeds);
		}
		Sound->UnregisterEffects(StartSeds);
	}

	void FBattlePresentation::BeginShow(bool bEvent)
	{
		TResult<FBattleStageFiles> StageFiles = FBattleStageFiles::Load(*Files.Image, *Files.Index, Context->Formation.Stage);
		TResult<FBattleSceneGraphics> Graphics = FBattleSceneGraphics::Load(*Files.Image, *Files.Index, Context->Formation.Scene);
		if (!StageFiles || !Graphics)
		{
			Show.Stop(EBattleShowFault::File);
			return;
		}
		Stage.emplace(std::move(*StageFiles));
		SceneGraphics.emplace(std::move(*Graphics));
		IdsSprite.reset();
		for (int32 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
		{
			if (Context->PartyLooks().IsId(Context->PartyCharacters[static_cast<std::size_t>(Slot)]) && !IdsSprite)
			{
				TResult<FIdsBattleSprite> Read = FIdsBattleSprite::Load(*Files.Image, *Files.Index);
				if (!Read)
				{
					Show.Stop(EBattleShowFault::File);
					return;
				}
				IdsSprite.emplace(std::move(*Read));
			}
		}
		FBattleShowSetup Setup;
		Setup.Context = Context;
		Setup.Arctangents = Files.Arctangents;
		Setup.SpriteFiles = Files.SpriteFiles;
		Setup.Stage = &*Stage;
		Setup.SceneGraphics = &*SceneGraphics;
		Setup.IdsSprite = IdsSprite ? &*IdsSprite : nullptr;
		Setup.Sheet = Files.Sheet;
		Setup.MarkerModel = Files.MarkerModel;
		Setup.ObjectFiles = Files.ObjectFiles;
		Setup.EventFiles = Files.EventFiles;
		Setup.Kind = Kind;
		Setup.bEvent = bEvent;
		Setup.Sound = Sound;
		Setup.Hud = Hud;
		Show.Begin(Setup);
		bBegun = true;
	}

	void FBattlePresentation::BeginFrame()
	{
		// A show whose files could not be read has stopped: the battle ends with this frame.
		if (!bBegun)
		{
			return;
		}
		Show.BeginFrame();
	}

	bool FBattlePresentation::IsHolding() const
	{
		return bBegun && Show.IsHolding();
	}

	void FBattlePresentation::HoldBlank()
	{
		Show.HoldBlank();
	}

	bool FBattlePresentation::DrewFirstBuffer() const
	{
		return bBegun && Show.Battle.DrawBuffer == 0;
	}

	int32 FBattlePresentation::CatchUpSteps() const
	{
		return bBegun ? Show.FrameCatchUpSteps() : 0;
	}

	bool FBattlePresentation::FinishFrame()
	{
		return !bBegun || Show.EndFrame();
	}

	bool FBattlePresentation::ResumeRules()
	{
		return !bBegun || Show.ResumeRules();
	}

	bool FBattlePresentation::HasStarted() const
	{
		return Show.HasStarted();
	}

	void FBattlePresentation::TickHud(uint8 InputMode)
	{
		if (Hud != nullptr)
		{
			Hud->Tick(InputMode);
		}
	}

	void FBattlePresentation::ShowPause(bool bInPaused)
	{
		bPaused = bInPaused;
		if (Sound == nullptr)
		{
			return;
		}
		if (bPaused)
		{
			Sound->PauseSound();
		}
		else
		{
			Sound->ResumeSound();
		}
	}

	bool FBattlePresentation::HasStopped() const
	{
		return Fault() != EBattleShowFault::None;
	}

	EBattleShowFault FBattlePresentation::Fault() const
	{
		if (Show.Fault() == EBattleShowFault::None && Hud != nullptr && Hud->HasStopped())
		{
			return EBattleShowFault::Hud;
		}
		return Show.Fault();
	}
}
