#include "Battle/Show/BattleShow.h"

#include "Battle/BattleContext.h"
#include "Battle/Show/BattleShowLoad.h"
#include "Battle/Show/BattleShowVram.h"
#include "Battle/Show/BattleStageFiles.h"
#include "Game/PsxRandom.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The display's projection: the screen's centre and the distance. */
		constexpr int32 CentreX = 0xA0;
		constexpr int32 CentreY = 0xA4;
		constexpr uint16 ProjectionDistance = 0x200;
		/** The opening's fades: to white at once, then (without an event) back from it over 40 steps. */
		constexpr int32 SubtractMode = 2;
		constexpr int32 FadeInSteps = 0x14;
		/** The enemies drawn as 3D models set how many catch-up steps the first frame's objects take. */
		constexpr uint8 LastModelType = 0x10;
		/** The average depths' scales as the console's start leaves them: a third and a quarter, in 4096ths, each over four. */
		constexpr int16 AverageScale3 = 0x155;
		constexpr int16 AverageScale4 = 0x100;
		/** The dim behind the result screen: to a quarter over 64 steps; the music's fade after a defeat. */
		constexpr int32 DimSteps = 0x40;
		constexpr int32 DimLevel = 0x40;
		constexpr int32 MusicFadeSteps = 0xF0;
		/** The ordering table's last entry, where the stage's backdrop is linked. */
		constexpr int32 BackdropEntry = 0xFFF;
	}

	FBattleShow::FBattleShow() : Playback(*this)
	{
		Vram.SetUploadKind(EVramUploadKind::ModelsAndEffects);
		Fade.Sprites = &Sprites;
		Shake.Sprites = &Sprites;
		TintFade.Sprites = &Sprites;
		TintFade.Layers = &Tints;
		TintFade.Stage = this;
		Numbers.Sprites = &Sprites;
		Markers.Sprites = &Sprites;
		Battle.Sprites = &Sprites;
		Battle.Gte = &Gte;
		Battle.Camera = &Camera;
		Battle.Floor = &Floor;
		Battle.Fade = &Fade;
		Battle.Shake = &Shake;
		Battle.TintFade = &TintFade;
		Battle.Numbers = &Numbers;
		Battle.Services = this;
	}

	FBattleShow::~FBattleShow() = default;

	void FBattleShow::Begin(const FBattleShowSetup& Setup)
	{
		Context = Setup.Context;
		Stage = Setup.Stage;
		SpriteFiles = Setup.SpriteFiles;
		MarkerModel = Setup.MarkerModel;
		ObjectFiles = Setup.ObjectFiles;
		EventFiles = Setup.EventFiles;
		Audio = Setup.Sound;
		Hud = Setup.Hud;
		Playback.Reset();
		ExtensionSprites.clear();
		FrameCount = 0;
		Displayed = FBattleShowFrame{};
		Sprites.ClearFault();
		Sprites.Random = Context->Random;
		Sprites.Arctangents = Setup.Arctangents;
		Battle.Random = Context->Random;
		Battle.Arctangents = Setup.Arctangents;
		Battle.Context = Context;
		Numbers.Random = Context->Random;
		Numbers.GearPressed = &Context->Party.bGearPressed;
		Numbers.Sheet = Setup.Sheet;
		Numbers.Reserve();
		Arctangents = Setup.Arctangents;
		Objects.Context = Context;
		Objects.Camera = &Camera;
		Objects.Floor = &Floor;
		Objects.Tables = &ObjectFiles->Tables();
		Objects.Gte = &Gte;
		Objects.Services = this;
		Objects.ShowOptions = &Options;
		Objects.Tasks = &Sprites;
		Objects.Random = Context->Random;
		Objects.Arctangents = Setup.Arctangents;
		Objects.BattleKind = Setup.Kind;
		Extensions.Sprites = &Sprites;
		Extensions.Random = Context->Random;
		Extensions.Arctangents = Setup.Arctangents;
		Extensions.Begin();

		// The display's area; the average-depth scales libgte's InitGeom set, which nothing in battle changes.
		Gte.OffsetX = CentreX << 16;
		Gte.OffsetY = CentreY << 16;
		Gte.ProjectionDistance = ProjectionDistance;
		Gte.AverageScale3 = AverageScale3;
		Gte.AverageScale4 = AverageScale4;
		ExtensionDrawing.Gte = &Gte;
		ExtensionDrawing.View = &Camera.View;
		ExtensionDrawing.Quads = &PartScratch;
		ModelDrawing.Gte = &Gte;
		ModelDrawing.View = &Camera.View;
		ModelDrawing.Frame = &Shown;
		ModelDrawing.Links = &Links;
		ModelDrawing.Drawn = &DrawnModels;
		ModelDrawing.Reset();

		// The end of the setup: the objects, the stage.
		BuildStage();

		// The presentation's state.
		InitFrameState();
		Battle.Begin();
		Battle.Play = FBattlePlayState{};
		Numbers.bPlaying = false;
		Numbers.CloseTotal();
		Markers.Reset();
		Numbers.RunningTotal = 0;
		Numbers.LastTotal = 0;
		// The framing's pitch is the program's own data as it loads, which only a playback sets again.
		Camera.Pitch = FBattleCamera::DefaultPitch;
		Camera.Init();
		Load = std::make_unique<FBattleShowLoad>(*this, Setup);
		Sprites.LinkTask(*Load);
		if (Audio != nullptr)
		{
			Audio->UnloadGearBattleBank();
		}
		const std::optional<FBattleCameraPreset> First = Stage->Record.CameraPreset(0);
		if (First)
		{
			Camera.DesiredEye = First->Eye;
			Camera.Eye = First->Eye;
			Camera.DesiredTarget = First->Target;
			Camera.Target = First->Target;
		}

		// The battle's opening: the screen black, faded in unless an event holds it; the living framed.
		Fade.Start(0, SubtractMode, 0xFF, 0xFF, 0xFF);
		if (!Setup.bEvent)
		{
			Fade.Start(FadeInSteps, SubtractMode, 0, 0, 0);
		}
		FrameSlots(Context->Living);
		CheckObjects();
	}

	const IKernelTask* FBattleShow::LoadTask() const
	{
		return Load.get();
	}

	void FBattleShow::End()
	{
		Battle.End();
		Load.reset();
		Context = nullptr;
	}

	void FBattleShow::InitFrameState()
	{
		// The model enemies counted as the original counts them: every two past the first two take a catch-up step.
		int32 Models = 0;
		for (int32 Slot = FBattleContext::PartySlotCount; Slot < FBattleContext::SlotCount; ++Slot)
		{
			const FBattleSlot& Record = Context->Slots[static_cast<std::size_t>(Slot)];
			if (Record.Occupant <= LastModelType && Record.InGear != 0)
			{
				++Models;
			}
		}
		CatchUp = std::max(Models / 2 - 1, 0);
		CatchUpSteps = 0;
		PendingCatchUp = 0;
		WaveHold.Stop();
		bCoveredByWave = false;
		Battle.DrawBuffer = 1;
		bStarted = false;
	}

	void FBattleShow::BuildStage()
	{
		const FBattleStageRecord& Record = Stage->Record;
		Objects.Init(Record);
		{
			const FVram::FKindScope SceneryKind(Vram, EVramUploadKind::Scenery);
			Objects.CreateStage(*Stage);
		}
		StageTint.Measure(Stage->Model.Pack.Textures);
		Backdrop = MakeBattleStageBackdrop(Record);
		Floor.Reset(Record.FloorVertices(), Record.FloorTriangles());
		const std::array<uint8, 3> Clear = Record.ClearColour();
		const uint8 ClearFlag = Record.ClearFlag();
		Battle.Clear = {ClearFlag, 0, 0, 0};
		if (ClearFlag != 0)
		{
			Battle.Clear = {ClearFlag, Clear[0], Clear[1], Clear[2]};
		}
		Battle.SavedClear = {};
		Battle.bStageHidden = false;
		Tints = FBattleTintLayers{};
	}

	void FBattleShow::BeginTint()
	{
		StageTint.Begin(Tints, Objects, Backdrop, ClearColour());
	}

	void FBattleShow::EndTint()
	{
		StageTint.End(Objects, Backdrop, ClearColour());
	}

	void FBattleShow::FrameSlots(uint32 Slots)
	{
		Camera.Frame(Slots, Battle.CameraSubjects(), Gte);
	}

	void FBattleShow::BeginFrame()
	{
		Battle.DrawBuffer ^= 1;
		ModelDrawing.Buffer = Battle.DrawBuffer;
		if (Hud != nullptr)
		{
			Hud->Flip();
		}
		Shown.Step = ++FrameCount;
		Shown.Drawers.clear();
		Shown.Quads.clear();
		Shown.Triangles.clear();
		Shown.Tiles.clear();
		Shown.Lines.clear();
		Shown.ScreenQuads.clear();
		Shown.Models.clear();
		Shown.ModelPolygons.clear();
		DrawnModels.clear();
		Shown.Hud.Primitives.clear();
		Shown.Hud.TextButtons.clear();
		Shown.Order.clear();
		Links.clear();

		// The camera's step, and its view built with the stage's frame.
		Camera.Step(Battle.CameraSubjects(), Gte, *Arctangents);
		Camera.BuildView(Shake.Shake.Offset);
		if (!Battle.bStageHidden)
		{
			Objects.StepStageClutAnimations(CatchUp);
			StageTint.Apply(Tints, Objects, Backdrop, ClearColour());
		}
		Objects.DrawStage(Camera.View, !Battle.bStageHidden);
		StageLinkCount = Objects.Frame().Links.size();
		if (Battle.bStageHidden)
		{
			Shown.Backdrop = FBattleBackdropFrame{};
		}
		else
		{
			FGte Drawing = Gte;
			Drawing.Transform = Camera.View;
			DrawBattleStageBackdrop(Backdrop, Camera.Eye, Camera.Target, Drawing, *Arctangents, Shown.Backdrop);
		}
		Shown.View = Camera.View;
		Shown.Eye = Camera.Eye;
		Shown.Target = Camera.Target;
		Shown.bStageDrawn = !Battle.bStageHidden;

		// The objects drawn; the stage's and the objects' links come before the show's, the backdrop between them.
		Objects.UpdateDraw(Camera.View, CatchUp, Battle.DrawBuffer);
		const std::vector<FBattleObjectLink>& ObjectLinks = Objects.Frame().Links;
		for (std::size_t Link = 0; Link < ObjectLinks.size(); ++Link)
		{
			if (Link == StageLinkCount && !Battle.bStageHidden)
			{
				Links.push_back({EBattleShowLink::Backdrop, 0, BackdropEntry});
			}
			Links.push_back({EBattleShowLink::Object, static_cast<uint32>(Link), ObjectLinks[Link].Entry});
		}
		if (ObjectLinks.size() == StageLinkCount && !Battle.bStageHidden)
		{
			Links.push_back({EBattleShowLink::Backdrop, 0, BackdropEntry});
		}

		// The sprites and the tasks: the pending frames built, the tasks drawn, then stepped.
		Sprites.BuildPendingFrames();
		Shown.CentreX = Gte.OffsetX >> 16;
		Shown.CentreY = Gte.OffsetY >> 16;
		Shown.ProjectionDistance = Gte.ProjectionDistance;
		RunDraws();
		Sprites.RunTasks();
		for (int32 Step = 0; Step < CatchUpSteps; ++Step)
		{
			Camera.Step(Battle.CameraSubjects(), Gte, *Arctangents);
			Sprites.RunTasks();
		}
		if (Sprites.Fault() != ESpriteFault::None)
		{
			ReportFault(EBattleShowFault::Sprites);
		}
		CheckObjects();

		// The HUD, by the input mode.
		if (Hud != nullptr)
		{
			Hud->Draw(Shown.Hud, Battle.Play.bHudHidden);
			Links.push_back({EBattleShowLink::Hud, 0, 1});
		}
	}

	bool FBattleShow::EndFrame()
	{
		DisplayFrame();
		return FinishFrame();
	}

	void FBattleShow::HoldBlank()
	{
		WaveHold.Blank();
	}

	void FBattleShow::DisplayFrame()
	{
		// The screen wave's blanks come before the display; a host that does not show them leaves them out.
		WaveHold.Stop();
		// The frame took one vertical blank, unless the screen wave ran in it: then the next one catches up.
		CatchUp = PendingCatchUp;
		CatchUpSteps = PendingCatchUp;
		PendingCatchUp = 0;

		// The display: VRAM's queued uploads, the drawing.
		{
			const FVram::FKindScope CharactersKind(Vram, EVramUploadKind::Characters);
			Sprites.FlushUploads(Vram);
		}
		Shown.Vram = &Vram;
		Shown.VramRevision = Vram.Revision();
		Shown.bCleared = Battle.Clear[0] != 0 || bCoveredByWave;
		Shown.ClearColor = bCoveredByWave ? std::array<uint8, 3>{} : std::array<uint8, 3>{Battle.Clear[1], Battle.Clear[2], Battle.Clear[3]};
		bCoveredByWave = false;
		BuildOrder();
		// The frame is whole: it is the one shown until the next is.
		Objects.ExchangeFrame(DisplayedObjects);
		Shown.Objects = &DisplayedObjects;
		std::swap(Shown, Displayed);
		DrawnModels.clear();

		LoadAnimExtension();
	}

	void FBattleShow::LoadAnimExtension()
	{
		// The effects of the module in memory run its code: another module read over it leaves them running the new one's.
		if (Sprites.RequestedExtension != LoadedExtension && Extensions.AnyRunning())
		{
			ReportFault(EBattleShowFault::Extension);
		}
		LoadedExtension = Sprites.RequestedExtension;
	}

	bool FBattleShow::FinishFrame()
	{
		const bool bBack = Playback.FinishFrame();
		CheckObjects();
		return bBack;
	}

	bool FBattleShow::ResumeRules()
	{
		const bool bReturned = Playback.ResumeRules();
		CheckObjects();
		return bReturned;
	}

	void FBattleShow::BeginPlayback(bool bStays, uint8 Actor, uint8 Target, uint8 NextReady)
	{
		Playback.Begin(bStays, Actor, Target, NextReady);
	}

	void FBattleShow::CancelPlayback()
	{
		Playback.Cancel();
	}

	void FBattleShow::SetTargetMarkers(uint16 Slots)
	{
		Markers.Set(Slots, Context->TurnActor, Battle.SlotSprites);
		// The objects' ambient pulses on the same mask.
		Objects.PulseMask = Slots;
	}

	void FBattleShow::PrepareNext(uint8 Slot)
	{
		Playback.PrepareNext(Slot);
	}

	void FBattleShow::ShowTurnDamage(uint8 Slot, const std::array<uint32, 3>& Amounts)
	{
		Playback.ShowTurnDamage(Slot, Amounts);
	}

	void FBattleShow::RefreshPoses()
	{
		Playback.RefreshPoses();
	}

	void FBattleShow::BoardGear(uint8 Slot)
	{
		Playback.BoardGear(Slot);
	}

	void FBattleShow::FinishLoads()
	{
		Playback.FinishLoads();
	}

	void FBattleShow::BeginEvent()
	{
		if (EventFiles == nullptr)
		{
			ReportFault(EBattleShowFault::File);
			return;
		}
		Playback.BeginEvent(*EventFiles);
	}

	void FBattleShow::PlayEventCue(const FBattleEventCue& Cue)
	{
		Playback.PlayEventCue(Cue);
	}

	void FBattleShow::PlayEndPose(EBattleEndPose Pose)
	{
		Playback.PlayEndPose(static_cast<int32>(Pose));
	}

	void FBattleShow::Dim()
	{
		Fade.Start(DimSteps, SubtractMode, DimLevel, DimLevel, DimLevel);
	}

	void FBattleShow::FadeMusic()
	{
		SetMusicVolume(0, MusicFadeSteps);
	}

	bool FBattleShow::UploadCommonImages()
	{
		TResult<std::vector<uint8>> Images = SpriteFiles->ReadCommonImages();
		if (!Images || !UploadBattleImageList(Vram, *Images, 0))
		{
			ReportFault(EBattleShowFault::File);
			return false;
		}
		return true;
	}

	TResult<FBattleSpriteFile> FBattleShow::ReadAttacks(uint8 Character) const
	{
		if (Character >= FBattleSpriteFiles::CharacterCount)
		{
			return MakeError("a party slot without a character reads an attack file");
		}
		return SpriteFiles->ReadAttacks(Character);
	}

	void FBattleShow::UnregisterSoundEffects(const FBattleSpriteFile& File)
	{
		if (Audio == nullptr)
		{
			return;
		}
		for (const FBattleSpriteSection& Section : File.Sections())
		{
			if (Section.Kind == EBattleSpriteSectionKind::SoundEffects)
			{
				Audio->UnregisterEffects(File.Sprite().From(Section.Offset).first(Section.Size));
			}
		}
	}

	void FBattleShow::UnloadSystemSoundBank()
	{
		if (Audio != nullptr)
		{
			Audio->UnloadBattleBank();
		}
	}

	void FBattleShow::LoadEnemySoundBank()
	{
		TResult<std::vector<uint8>> Bank = SpriteFiles->ReadEnemySounds();
		if (!Bank)
		{
			ReportFault(EBattleShowFault::File);
			return;
		}
		UnloadSystemSoundBank();
		if (Audio != nullptr)
		{
			Audio->LoadBattleVoiceBank(*Bank);
		}
	}

	void FBattleShow::LoadSystemSoundBank()
	{
		if (Battle.Play.bSystemBankLoaded)
		{
			return;
		}
		TResult<std::vector<uint8>> Bank = SpriteFiles->ReadSystemSounds();
		if (!Bank)
		{
			ReportFault(EBattleShowFault::File);
			return;
		}
		// Loaded unless the driver holds a bank of its id already.
		if (Audio != nullptr && Audio->HasBank(Bank->size() >= 0x22 ? static_cast<uint16>((*Bank)[0x20] | (*Bank)[0x21] << 8) : 0))
		{
			return;
		}
		UnloadSystemSoundBank();
		if (Audio != nullptr)
		{
			Audio->LoadBattleBank(*Bank);
		}
		Battle.Play.bSystemBankLoaded = true;
		Battle.Play.bHitVoices = false;
	}

	void FBattleShow::ReloadSystemSoundBank()
	{
		UnloadSystemSoundBank();
		TResult<std::vector<uint8>> Bank = SpriteFiles->ReadSystemSounds();
		if (!Bank)
		{
			ReportFault(EBattleShowFault::File);
			return;
		}
		if (Audio != nullptr)
		{
			Audio->LoadGearBattleBank(*Bank);
		}
	}

	void FBattleShow::BuildOrder()
	{
		// The ordering table far to near; within an entry the last linked first.
		Shown.Order.assign(Links.begin(), Links.end());
		std::stable_sort(Shown.Order.begin(), Shown.Order.end(), [](const FBattleShowLink& A, const FBattleShowLink& B) { return A.Entry > B.Entry; });
		auto First = Shown.Order.begin();
		while (First != Shown.Order.end())
		{
			const auto Last = std::find_if(First, Shown.Order.end(), [First](const FBattleShowLink& Link) { return Link.Entry != First->Entry; });
			std::reverse(First, Last);
			First = Last;
		}
	}

	void FBattleShow::PlayCommonSound(uint8 Effect)
	{
		if (Battle.SystemSoundSet < 0)
		{
			ReportFault(EBattleShowFault::SystemSounds);
			return;
		}
		PlaySystemSound(static_cast<uint32>(Battle.SystemSoundSet) << 16 | Effect);
	}

	void FBattleShow::Stop(EBattleShowFault Fault)
	{
		ReportFault(Fault);
	}

	void FBattleShow::CheckObjects()
	{
		if (Objects.Faulted())
		{
			ReportFault(EBattleShowFault::Objects);
		}
	}

	void FBattleShow::ReportFault(EBattleShowFault Fault, const char* Reason)
	{
		if (FirstFault == EBattleShowFault::None)
		{
			FirstFault = Fault;
			FirstFaultReason = Reason;
		}
	}
}
