#include "BattlingView.h"

#include "Battling/BattlingDrawing.h"
#include "Field/FieldScene.h"
#include "Game/BattlingGameProgram.h"
#include "Graphics/Texture.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "Rendering/BattlingTrailsRendering.h"
#include "Rendering/DrawListRendering.h"
#include "Rendering/ScreenBlurRendering.h"
#include "Rendering/SharedRenderResource.h"
#include "Rendering/VramTexture.h"
#include "Rendering/TextureModSources.h"

namespace
{
	/** Battling's screen, its 2D layer fitted in the display's middle as the world map's is; the scene fills the display. */
	constexpr Weltall::FScreenSize BattlingScreen{Weltall::FBattlingFrame::ScreenWidth, Weltall::FBattlingFrame::ScreenHeight};

	/** The clear colour as the GPU fills the screen with it. */
	FLinearColor BattlingClearColor(const Weltall::FDrawColor& Color)
	{
		return FColor(Weltall::FilledChannel(Color.R), Weltall::FilledChannel(Color.G), Weltall::FilledChannel(Color.B)).ReinterpretAsLinear();
	}

	/**
	 * The world's up in view space under the scene's camera (its y runs down: the camera's second column turned the other
	 * way), so that under a depth sorting the gears' shadows are not drawn over what stands well above the ground they lie
	 * on, a gear's own legs among it.
	 */
	FVector3f BattlingFloorUp(const Weltall::FWorldMapFrame& Scene)
	{
		const Weltall::FGteMatrix& Camera = Scene.Camera;
		return -FVector3f(Camera.M[0][1], Camera.M[1][1], Camera.M[2][1]).GetSafeNormal();
	}
}

/**
 * The demonstration's two pictures, as the console's two draw buffers it never clears: each frame of the program is drawn
 * over the one before last, after the last is copied in dimmed. Kept on the rendering thread; a new run of the
 * demonstration starts them black.
 */
class FBattlingTrails
{
public:
	static TSharedRef<FBattlingTrails> Make()
	{
		return TSharedRef<FBattlingTrails>(new FBattlingTrails(), [](FBattlingTrails* Released)
		{
			ENQUEUE_RENDER_COMMAND(ReleaseBattlingTrails)([Released](FRHICommandListImmediate&) { delete Released; });
		});
	}

	/** The latest picture, at Output's size: the frame of the run's Pass drawn into it once, by DrawScene over the frame before. */
	FRDGTextureRef Latest(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FIntRect& Area, uint32 Run, uint32 Pass,
		TFunctionRef<void(FRDGTextureRef)> DrawScene)
	{
		const FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(Output->Desc.Extent, Output->Desc.Format, FClearValueBinding::Black,
			TexCreate_RenderTargetable | TexCreate_ShaderResource);
		const bool bFresh = !bDrawn || Run != RunDrawn || !Pictures[0].IsValid() || Pictures[0]->GetDesc().Extent != Desc.Extent
			|| Pictures[0]->GetDesc().Format != Desc.Format;
		FRDGTextureRef Textures[2];
		for (int32 Index = 0; Index < 2; ++Index)
		{
			if (bFresh)
			{
				Textures[Index] = GraphBuilder.CreateTexture(Desc, TEXT("BattlingTrails"));
				AddClearRenderTargetPass(GraphBuilder, Textures[Index], FLinearColor::Black);
				GraphBuilder.QueueTextureExtraction(Textures[Index], &Pictures[Index]);
			}
			else
			{
				Textures[Index] = GraphBuilder.RegisterExternalTexture(Pictures[Index]);
			}
		}
		if (bFresh || Pass != PassDrawn)
		{
			const int32 Next = 1 - Shown;
			AddBattlingTrailsPass(GraphBuilder, Textures[Shown], Textures[Next], Area);
			DrawScene(Textures[Next]);
			Shown = Next;
			RunDrawn = Run;
			PassDrawn = Pass;
			bDrawn = true;
		}
		return Textures[Shown];
	}

private:
	FBattlingTrails() = default;

	TRefCountPtr<IPooledRenderTarget> Pictures[2];
	int32 Shown = 0;
	uint32 RunDrawn = 0;
	uint32 PassDrawn = 0;
	bool bDrawn = false;
};

/**
 * A displayed frame of the arena: its scene through the field scene's passes, or a pause's blurred picture, or the
 * demonstration's scene over its frames before; then its 2D layer.
 */
class FBattlingScreenFrame final : public IScreenFrame
{
public:
	FBattlingScreenFrame(TSharedRef<const FFieldSceneMesh> InMesh, TSharedRef<FVramTexture> InVram, TSharedRef<FVramTexture> InIconVram)
		: Mesh(MoveTemp(InMesh)), Vram(MoveTemp(InVram)), IconVram(MoveTemp(InIconVram))
	{
	}

	virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const override
	{
		const FScreenArea Rows = Area.Rows(BattlingScreen);
		if (bTrails && Trails.IsValid())
		{
			// Drawn in the painter's order straight over the frame before, as the console draws; the display shows the latest.
			FRDGTextureRef Latest = Trails->Latest(GraphBuilder, Output, Rows, TrailsRun, Pass, [this, &GraphBuilder, &Rows](FRDGTextureRef Into)
			{
				AddFieldSceneOverlayPasses(GraphBuilder, *Mesh, *Vram, Draw, Into, Rows);
			});
			FRHICopyTextureInfo Copy;
			Copy.SourcePosition = FIntVector(Rows.Min.X, Rows.Min.Y, 0);
			Copy.DestPosition = Copy.SourcePosition;
			Copy.Size = FIntVector(Rows.Width(), Rows.Height(), 1);
			AddCopyTexturePass(GraphBuilder, Latest, Output, Copy);
		}
		else if (PictureBlurs > 0)
		{
			// The paused picture drawn whole, then blurred at the size of the original screen's pixels.
			const FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(Output->Desc.Extent, Output->Desc.Format, FClearValueBinding::Black,
				TexCreate_RenderTargetable | TexCreate_ShaderResource);
			FRDGTextureRef Source = GraphBuilder.CreateTexture(Desc, TEXT("BattlingPicture"));
			AddFieldScenePasses(GraphBuilder, *Mesh, *Vram, PictureDraw, Source, Rows);
			AddScreenLayer(GraphBuilder, PictureScreen, PictureColumns, Source, Area);
			const float Step = static_cast<float>(Rows.Height()) / static_cast<float>(Weltall::FBattlingFrame::ScreenHeight);
			for (int32 Blur = 1; Blur <= PictureBlurs; ++Blur)
			{
				FRDGTextureRef Target = Blur == PictureBlurs ? Output : GraphBuilder.CreateTexture(Desc, TEXT("BattlingPictureBlurred"));
				AddScreenBlurPass(GraphBuilder, Source, Target, Rows, Step);
				Source = Target;
			}
		}
		else
		{
			AddFieldScenePasses(GraphBuilder, *Mesh, *Vram, Draw, Output, Rows);
		}
		AddScreenLayer(GraphBuilder, Screen, Columns, Output, Area);
	}

	virtual Weltall::FScreenSize ScreenSize() const override
	{
		return BattlingScreen;
	}

	FFieldSceneDraw Draw;
	/** The 2D layer over the scene: the labels and the HUD. */
	Weltall::FDrawList Screen;
	/** The columns the 2D layer's corners count. */
	int32 Columns = Weltall::FBattlingFrame::ScreenWidth;
	/** A paused frame's picture: the frame the pause captured, its scene and 2D layer, and the blurs it shows with (0 none). */
	FFieldSceneDraw PictureDraw;
	Weltall::FDrawList PictureScreen;
	int32 PictureColumns = Weltall::FBattlingFrame::ScreenWidth;
	int32 PictureBlurs = 0;
	/** A frame of the demonstration: the program's pass it was drawn in, the demonstration's run, and its pictures. */
	bool bTrails = false;
	uint32 Pass = 0;
	uint32 TrailsRun = 0;
	TSharedPtr<FBattlingTrails> Trails;

private:
	/** A 2D layer on the screen's place in the picture's middle: the winner screen's 640 columns span the place of the bout's 320. */
	void AddScreenLayer(FRDGBuilder& GraphBuilder, const Weltall::FDrawList& Layer, int32 LayerColumns, FRDGTextureRef Output, const FScreenArea& Area) const
	{
		if (Layer.Primitives.empty())
		{
			return;
		}
		AddIconDrawListPasses(GraphBuilder, *Vram, *IconVram, Layer, Output, Area.ScreenRect(BattlingScreen), {LayerColumns, BattlingScreen.Height});
	}

	TSharedRef<const FFieldSceneMesh> Mesh;
	TSharedRef<FVramTexture> Vram;
	TSharedRef<FVramTexture> IconVram;
};

FBattlingView::FBattlingView(const FWorldMapRenderOptions& InOptions)
	: Options(InOptions), Vram(ShareRenderResource(new FVramTexture())), IconVram(ShareRenderResource(new FVramTexture())),
	  NoMesh(MakeShared<const FFieldSceneMesh>(Weltall::FFieldScene{})), Trails(FBattlingTrails::Make())
{
}

FBattlingView::~FBattlingView() = default;

void FBattlingView::Follow(const Weltall::FBattlingGameProgram& Program, bool bNewProgram)
{
	if (bNewProgram)
	{
		DisplayFrames.Reset();
		bTrailsShown = false;
	}
	Vram->SetUpscaled(Weltall::UpscalesTextures(Options.Filter, Options.SpriteFilter));
	Vram->Follow(Program.Vram());
	const Weltall::FBattlingFrame* Latest = Program.LatestFrame();
	const Weltall::FBattlingFrame* Previous = Program.PreviousFrame();
	// The demonstration's frames are drawn over one another, each as it is.
	const bool bTrails = Latest != nullptr && Latest->bTrails;
	if (bTrails && !bTrailsShown)
	{
		++TrailsRun;
	}
	bTrailsShown = bTrails;
	bPreviousFollows = Latest != nullptr && Previous != nullptr && !bTrails;
	if (bPreviousFollows)
	{
		Weltall::MatchWorldMapAnchors(Previous->Scene, Latest->Scene, AnchorMatches, MatchScratch);
	}
}

TSharedPtr<const IScreenFrame> FBattlingView::Capture(const Weltall::FBattlingGameProgram& Program, float Alpha) const
{
	const Weltall::FBattlingFrame* Latest = Program.LatestFrame();
	if (Latest == nullptr)
	{
		return nullptr;
	}
	TSharedRef<FBattlingScreenFrame>* Free = DisplayFrames.FindByPredicate([](const TSharedRef<FBattlingScreenFrame>& Candidate) { return Candidate.IsUnique(); });
	TSharedRef<FBattlingScreenFrame> Next = Free != nullptr ? *Free : DisplayFrames.Add_GetRef(MakeShared<FBattlingScreenFrame>(NoMesh, Vram, IconVram));
	const Weltall::FBattlingFrame* Previous = bPreviousFollows ? Program.PreviousFrame() : nullptr;
	Weltall::BlendWorldMapFrames(Latest->Scene, Previous != nullptr ? &Previous->Scene : nullptr, AnchorMatches, Alpha, Blend);
	BuildWorldMapFrameDraw(Latest->Scene, &Blend, Options, Next->Draw, static_cast<float>(Weltall::FBattlingFrame::ScreenHeight),
		static_cast<float>(Weltall::FBattlingFrame::ScreenWidth) / static_cast<float>(Latest->Columns));
	Next->Columns = Latest->Columns;
	Next->Draw.Background = BattlingClearColor(Latest->Clear);
	Next->Draw.FloorUp = BattlingFloorUp(Latest->Scene);
	Next->bTrails = Latest->bTrails;
	Next->Pass = Latest->Pass;
	Next->TrailsRun = TrailsRun;
	Next->Trails = Trails;
	if (Latest->bTrails)
	{
		Next->Draw.bDepthBuffer = false;
	}
	ScreenLatest.Primitives.assign(Latest->Screen.begin(), Latest->Screen.end());
	ScreenLatest.TextButtons.assign(Latest->TextButtons.begin(), Latest->TextButtons.end());
	if (Previous != nullptr)
	{
		ScreenPrevious.Primitives.assign(Previous->Screen.begin(), Previous->Screen.end());
	}
	ScreenBuilder.Screen = {Latest->Columns, Weltall::FBattlingFrame::ScreenHeight};
	ScreenBuilder.Build(ScreenLatest, Previous != nullptr ? &ScreenPrevious : nullptr, Alpha, Next->Screen);
	ShowTextButtons(Next->Screen);
	const Weltall::FBattlingFrame* Picture = Program.PausedPicture();
	Next->PictureBlurs = Picture != nullptr ? Latest->PictureBlurs : 0;
	if (Next->PictureBlurs > 0)
	{
		BuildWorldMapFrameDraw(Picture->Scene, nullptr, Options, Next->PictureDraw, static_cast<float>(Weltall::FBattlingFrame::ScreenHeight),
			static_cast<float>(Weltall::FBattlingFrame::ScreenWidth) / static_cast<float>(Picture->Columns));
		Next->PictureDraw.Background = BattlingClearColor(Picture->Clear);
		Next->PictureDraw.FloorUp = BattlingFloorUp(Picture->Scene);
		Next->PictureColumns = Picture->Columns;
		ScreenPicture.Primitives.assign(Picture->Screen.begin(), Picture->Screen.end());
		ScreenPicture.TextButtons.assign(Picture->TextButtons.begin(), Picture->TextButtons.end());
		ScreenBuilder.Screen = {Picture->Columns, Weltall::FBattlingFrame::ScreenHeight};
		ScreenBuilder.Build(ScreenPicture, nullptr, 1.0f, Next->PictureScreen);
		ShowTextButtons(Next->PictureScreen);
	}
	FollowTextureModSources(*Vram, Program.Vram(), nullptr, {&Next->Draw, Next->PictureBlurs > 0 ? &Next->PictureDraw : nullptr},
		{&Next->Screen, Next->PictureBlurs > 0 ? &Next->PictureScreen : nullptr});
	return Next;
}

void FBattlingView::ShowTextButtons(Weltall::FDrawList& Layer) const
{
	if (Glyphs == nullptr || ScreenBuilder.TextCells.empty())
	{
		return;
	}
	Icons.ApplyToText(Layer, ScreenBuilder.TextCells, *Glyphs);
	IconVram->Follow(Icons.Pictures());
}
