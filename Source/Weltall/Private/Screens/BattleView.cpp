#include "BattleView.h"

#include "Battle/BattleRuntime.h"
#include "Battle/Show/BattleObjectsFrame.h"
#include "Battle/Show/BattlePresentation.h"
#include "Battle/Show/BattleShowFrame.h"
#include "Battle/Show/BattleShowTransition.h"
#include "Field/FieldScene.h"
#include "FieldScreenFrame.h"
#include "Game/BattleGameProgram.h"
#include "Rendering/BattleRendering.h"
#include "Rendering/BattleScreenWaveRendering.h"
#include "Rendering/BattleTransitionRendering.h"
#include "Rendering/SharedRenderResource.h"
#include "Rendering/VramTexture.h"
#include "Rendering/TextureModLibrary.h"
#include "Rendering/TextureModSources.h"

/** A displayed frame of the battle: its drawing and what it is textured from. */
class FBattleScreenFrame final : public IScreenFrame
{
public:
	FBattleScreenFrame(TSharedRef<const FFieldSceneMesh> InMesh, TSharedRef<FVramTexture> InVram, TSharedRef<FVramTexture> InIconVram)
		: Mesh(MoveTemp(InMesh)), Vram(MoveTemp(InVram)), IconVram(MoveTemp(InIconVram))
	{
	}

	virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FIntRect& Area) const override
	{
		AddBattleFramePasses(GraphBuilder, *Mesh, *Vram, *IconVram, Draw, Output, Area);
	}

	FBattleFrameDraw Draw;

private:
	TSharedRef<const FFieldSceneMesh> Mesh;
	TSharedRef<FVramTexture> Vram;
	TSharedRef<FVramTexture> IconVram;
};

/** A displayed frame of the battle's transition: the picture of the screen before the battle, in the transition's triangles. */
class FBattleTransitionFrame final : public IScreenFrame
{
public:
	virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FIntRect& Area) const override
	{
		AddBattleTransitionPasses(GraphBuilder, Picture.IsValid() ? Picture->Texture(GraphBuilder, Area.Size()) : nullptr, Draw, Output, Area);
	}

	FBattleTransitionDraw Draw;
	TSharedPtr<FFieldCoverPicture> Picture;
};

/** A displayed frame of the screen wave: the picture of the screen before it, in the wave's mesh over the frames before. */
class FBattleScreenWaveFrame final : public IScreenFrame
{
public:
	virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FIntRect& Area) const override
	{
		Buffers->AddPasses(GraphBuilder, Picture->Texture(GraphBuilder, Area.Size()), Draw, Output, Area);
	}

	FBattleScreenWaveDraw Draw;
	TSharedPtr<FFieldCoverPicture> Picture;
	TSharedPtr<FBattleScreenWaveBuffers> Buffers;
};

FBattleView::FBattleView(const FFieldRenderOptions& InOptions)
	: Options(InOptions), Vram(ShareRenderResource(new FVramTexture())), NoMesh(MakeShared<const FFieldSceneMesh>(Weltall::FFieldScene{})),
	  IconVram(ShareRenderResource(new FVramTexture()))
{
}

FBattleView::~FBattleView() = default;

void FBattleView::Follow(const Weltall::FBattleGameProgram& Program, bool bNewProgram, const TSharedPtr<const IScreenFrame>& Shown)
{
	bPreviousFollows = false;
	if (bNewProgram)
	{
		bKept = false;
		DisplayFrames.Reset();
		TransitionFrames.Reset();
		Transition.Reset();
		ScreenBefore = FFieldCoverPicture::Make(Shown);
	}
	if (ScreenBefore.IsValid() && !Program.Runtime().IsInTransition())
	{
		ScreenBefore.Reset();
	}
	const Weltall::FBattlePresentation& Presentation = Program.Presentation();
	if (!Presentation.HasBegun())
	{
		return;
	}
	if (Presentation.Show.ScreenWave().IsRunning())
	{
		// The wave's frames show only the picture of the screen before it, drawn from VRAM as it was: the capture the wave
		// stored over the effects' VRAM is followed once it is over, and the frame after it is not blended from the one before.
		bKept = false;
		if (!ScreenWave.IsValid())
		{
			ScreenWave = FFieldCoverPicture::Make(Shown);
			WaveBuffers = FBattleScreenWaveBuffers::Make();
		}
		return;
	}
	ScreenWave.Reset();
	WaveBuffers.Reset();
	WaveFrames.Reset();
	const Weltall::FBattleShowFrame& Frame = Presentation.Show.Frame();
	if (Frame.Vram != nullptr)
	{
		Vram->SetUpscaled(Weltall::UpscalesTextures(Options.Filter, Options.SpriteFilter));
		Vram->Follow(*Frame.Vram);
	}
	if (!bKept || Frame.Step != LatestKept.Step)
	{
		Swap(PreviousKept, LatestKept);
		bPreviousFollows = bKept;
		Weltall::KeepBattleFrame(Frame, LatestKept);
		bKept = true;
	}
}

TSharedPtr<const IScreenFrame> FBattleView::Capture(const Weltall::FBattleGameProgram& Program, float Alpha) const
{
	const Weltall::FBattleRuntime& Runtime = Program.Runtime();
	const Weltall::FBattleDefaultTransition* Shatter = Runtime.DefaultTransition();
	const Weltall::FBattleScreenMesh* Mesh = Runtime.SetupTransition();
	if (Shatter != nullptr || Mesh != nullptr)
	{
		TSharedRef<FBattleTransitionFrame>* Free =
			TransitionFrames.FindByPredicate([](const TSharedRef<FBattleTransitionFrame>& Candidate) { return Candidate.IsUnique(); });
		TSharedRef<FBattleTransitionFrame> Next = Free != nullptr ? *Free : TransitionFrames.Add_GetRef(MakeShared<FBattleTransitionFrame>());
		if (Shatter != nullptr)
		{
			Transition.Build(*Shatter, Runtime.TransitionFrames(), Alpha, Next->Draw);
		}
		else
		{
			Transition.Build(*Mesh, Runtime.TransitionBuffer(), Runtime.TransitionFrames(), Alpha, Next->Draw);
		}
		Next->Picture = ScreenBefore;
		return Next;
	}
	const Weltall::FBattlePresentation& Presentation = Program.Presentation();
	if (!Presentation.HasBegun())
	{
		return nullptr;
	}
	if (ScreenWave.IsValid() && Presentation.Show.ScreenWave().IsRunning())
	{
		TSharedRef<FBattleScreenWaveFrame>* FreeWave =
			WaveFrames.FindByPredicate([](const TSharedRef<FBattleScreenWaveFrame>& Candidate) { return Candidate.IsUnique(); });
		TSharedRef<FBattleScreenWaveFrame> Next = FreeWave != nullptr ? *FreeWave : WaveFrames.Add_GetRef(MakeShared<FBattleScreenWaveFrame>());
		Next->Draw = BattleScreenWaveDraw(Presentation.Show.ScreenWave(), Alpha);
		Next->Picture = ScreenWave;
		Next->Buffers = WaveBuffers;
		return Next;
	}
	const Weltall::FBattleShowFrame& Frame = Presentation.Show.Frame();
	TSharedRef<FBattleScreenFrame>* Free = DisplayFrames.FindByPredicate([](const TSharedRef<FBattleScreenFrame>& Candidate) { return Candidate.IsUnique(); });
	TSharedRef<FBattleScreenFrame> Next = Free != nullptr ? *Free : DisplayFrames.Add_GetRef(MakeShared<FBattleScreenFrame>(NoMesh, Vram, IconVram));
	const bool bBlends = bKept && LatestKept.Step == Frame.Step;
	if (bBlends)
	{
		Weltall::BlendBattleFrames(LatestKept, bPreviousFollows ? &PreviousKept : nullptr, Alpha, Blend, BlendScratch);
	}
	// A replacement may draw where the original's texels do not.
	Weltall::FTexelKindCache* Kinds = nullptr;
	if (Frame.Vram != nullptr && !FTextureModLibrary::IsReplacing())
	{
		TexelKinds.Follow(*Frame.Vram);
		Kinds = &TexelKinds;
	}
	BuildBattleFrameDraw(Frame, bBlends ? &Blend : nullptr, Options, Next->Draw, Kinds);
	HudFrame.Primitives.assign(Frame.Hud.Primitives.begin(), Frame.Hud.Primitives.end());
	HudFrame.TextButtons.assign(Frame.Hud.TextButtons.begin(), Frame.Hud.TextButtons.end());
	if (Glyphs != nullptr)
	{
		Icons.PlacePrompt(HudFrame.Primitives, *Glyphs);
	}
	HudFrame.Number = Frame.Step + 1;
	HudBuilder.Build(HudFrame, nullptr, 1.0f, Next->Draw.Hud);
	if (Glyphs != nullptr && (!HudBuilder.Icons.empty() || !HudBuilder.TextCells.empty()))
	{
		// The pictures by their places in the list first: a text's buttons add primitives to it.
		Icons.Apply(Next->Draw.Hud, HudBuilder.Icons, *Glyphs, Program.HudSheet(), Weltall::BattleHudButtonPictures);
		Icons.ApplyToText(Next->Draw.Hud, HudBuilder.TextCells, *Glyphs);
		IconVram->Follow(Icons.Pictures());
	}
	if (Frame.Vram != nullptr)
	{
		FollowTextureModSources(*Vram, *Frame.Vram, nullptr, {&Next->Draw.Scene, &Next->Draw.OverFade, &Next->Draw.OverHud}, {&Next->Draw.Fade, &Next->Draw.Hud},
			Frame.Objects != nullptr ? std::span<const Weltall::FTexturePictureSeed>(Frame.Objects->TextureSeeds) : std::span<const Weltall::FTexturePictureSeed>(),
			Program.HudTextureRegions());
	}
	return Next;
}
