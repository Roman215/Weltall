#include "FieldView.h"

#include "FieldScreenFrame.h"
#include "Field/FieldGears.h"
#include "Field/FieldRenderState.h"
#include "Gears/GearModels.h"
#include "Game/FieldMovie.h"
#include "Game/FieldProgram.h"
#include "Rendering/SharedRenderResource.h"
#include "Rendering/VramTexture.h"
#include "Rendering/TextureModLibrary.h"
#include "Rendering/TextureModSources.h"

#include <algorithm>

FFieldView::FFieldView(const FFieldRenderOptions& InOptions)
	: Options(InOptions), Vram(ShareRenderResource(new FVramTexture())), IconVram(ShareRenderResource(new FVramTexture()))
{
}

FFieldView::~FFieldView() = default;

void FFieldView::Follow(const Weltall::FFieldProgram& Program, bool bNewProgram, const TSharedPtr<const IScreenFrame>& Shown)
{
	if (bNewProgram)
	{
		ScreenBefore = Shown;
		Mesh.Reset();
		Movie.Reset();
		ShownMovie = {};
		Picture.Reset();
		PicturesTaken = 0;
		Pictures.Reset();
		ScreenWritten.clear();
		DisplayFrames.Reset();
		Still.Reset();
	}
	const Weltall::FFieldFrame* Latest = Program.LatestFrame();
	if (Latest == nullptr)
	{
		return;
	}
	// Before the drawing moves on to the new frame: the picture is of the one shown before it.
	if (Latest->Cover.Taken != PicturesTaken)
	{
		PicturesTaken = Latest->Cover.Taken;
		TakePicture(Program);
	}
	// A new map's frame starts its scenery, and nothing blends across it; a gear loaded or freed
	// changes the gears' part of the mesh.
	const Weltall::FEventContext& Context = Program.Runtime().Context();
	if (!Mesh.IsValid() || Program.Runtime().MapLoads() != MeshMapLoads || Program.Drawing().GearLayout(Context) != MeshGears)
	{
		Program.Drawing().LayOutGears(Context, GearLayout);
		MeshGears = Program.Drawing().GearLayout(Context);
		Mesh = MakeShared<const FFieldSceneMesh>(Program.Drawing().Scene(), GearLayout, MeshGears, Program.Drawing().SpriteSheets());
		MeshMapLoads = Program.Runtime().MapLoads();
		DisplayFrames.Reset();
	}
	// The screen's pixels VRAM holds go in again over each sending of the core's units, which lack them.
	Vram->SetUpscaled(Weltall::UpscalesTextures(Options.Filter, Options.SpriteFilter));
	const bool bSent = Vram->Follow(Program.Drawing().Vram());
	if (bSent || Latest->ScreenInVram != ScreenWritten)
	{
		ScreenWritten = Latest->ScreenInVram;
		WriteScreenIntoVram(Vram, ScreenWritten, Pictures);
	}
	for (auto Kept = Pictures.CreateIterator(); Kept; ++Kept)
	{
		const int32 Taken = Kept.Key();
		const bool bHeld = std::any_of(ScreenWritten.begin(), ScreenWritten.end(), [Taken](const Weltall::FFieldScreenInVram& Region) { return Region.Taken == Taken; });
		if (!bHeld && Taken != static_cast<int32>(PicturesTaken))
		{
			Kept.RemoveCurrent();
		}
	}
	if (const Weltall::FFieldFrame* Previous = Program.PreviousFrame())
	{
		Weltall::MatchBlendKeys(Previous->Screen, Latest->Screen, Matches, MatchScratch);
	}
	const Weltall::FFieldMovie* FieldMovie = Program.Movie();
	// The movie's frame it ended in still shows its last picture: the field draws nothing over it before its next tick.
	if (FieldMovie == nullptr && Program.Runtime().LastFrame() == Weltall::EFieldFrame::Movie && ShownMovie.Picture.IsValid())
	{
		return;
	}
	if (FieldMovie != nullptr)
	{
		Movie.Follow(FieldMovie->Player(), Options.MovieFilter);
	}
	else
	{
		Movie.Reset();
	}
	const bool bMovieShown = FieldMovie != nullptr && FieldMovie->View() != Weltall::EFieldMovieView::InScene && Movie.Texture().IsValid();
	ShownMovie.Picture = bMovieShown ? Movie.Texture() : nullptr;
	ShownMovie.bOverField = bMovieShown && FieldMovie->View() == Weltall::EFieldMovieView::OverField;
}

void FFieldView::TakePicture(const Weltall::FFieldProgram& Program)
{
	const Weltall::FFieldFrame* Shown = Program.FrameBeforeLatest();
	if (Shown == nullptr || !Mesh.IsValid())
	{
		// Before the field has shown a frame the screen holds the program before's last.
		Picture = FFieldCoverPicture::Make(ScreenBefore);
		Pictures.Add(static_cast<int32>(PicturesTaken), Picture);
		return;
	}
	ScreenBefore.Reset();
	// The frame shown last, whole, with the VRAM (and the screen's pixels it held) and the mesh it was drawn with.
	const TSharedRef<FVramTexture> ShownVram = ShareRenderResource(new FVramTexture());
	ShownVram->SetUpscaled(Weltall::UpscalesTextures(Options.Filter, Options.SpriteFilter));
	ShownVram->Follow(Program.Drawing().PictureVram());
	WriteScreenIntoVram(ShownVram, Shown->ScreenInVram, Pictures);
	const TSharedRef<FFieldScreenFrame> Source = MakeShared<FFieldScreenFrame>(Mesh.ToSharedRef(), ShownVram);
	Fill(*Source, Program.Drawing().PictureScene(), Program.Drawing().PictureVram(), *Shown, nullptr, 1.0f, nullptr, ShownMovie);
	FollowTextureModSources(*ShownVram, Program.Drawing().PictureVram(), Mesh.Get(), {&Source->Draw}, {&Source->Layer}, Shown->Gears.TextureSeeds);
	Picture = FFieldCoverPicture::Make(Source);
	Pictures.Add(static_cast<int32>(PicturesTaken), Picture);
}

Weltall::FTexelKindCache* FFieldView::TexelKindsOf(const Weltall::FVram& SceneVram, const Weltall::FFieldFrame& Now) const
{
	// A replacement may draw where the original's texels do not, and the screen's pixels written into VRAM are not the core's.
	if (FTextureModLibrary::IsReplacing() || !Now.ScreenInVram.empty())
	{
		return nullptr;
	}
	TexelKinds.Follow(SceneVram);
	return &TexelKinds;
}

bool FFieldView::Fill(FFieldScreenFrame& Display, const Weltall::FFieldScene& Scene, const Weltall::FVram& SceneVram, const Weltall::FFieldFrame& Now,
	const Weltall::FFieldFrame* Before, float Alpha, const std::vector<int32>* LayerMatches, const FShownMovie& Shown) const
{
	Display.MoviePicture = Shown.Picture;
	Display.Compass = Options.Compass;
	Display.Alpha = Alpha;
	Display.Cover = {};
	Display.Picture = nullptr;
	if (Shown.Picture.IsValid())
	{
		// Full screen the picture alone; over the field the message windows over it, as the frame drew them.
		if (Shown.bOverField)
		{
			Display.Layer.Primitives.assign(Now.Screen.Primitives.begin(), Now.Screen.Primitives.end());
			ShowTextButtons(Display, Now);
		}
		else
		{
			Display.Layer.Primitives.clear();
		}
		Display.bPreviousLayer = false;
		Display.Alpha = 1.0f;
		return false;
	}
	const bool bBlended = BuildFieldFrameDraw(Scene, Now, Before, Alpha, *Display.GetMesh(), Options, Display.Draw, TexelKindsOf(SceneVram, Now));
	Display.Layer.Primitives.assign(Now.Screen.Primitives.begin(), Now.Screen.Primitives.end());
	Display.bPreviousLayer = Before != nullptr && LayerMatches != nullptr;
	if (Display.bPreviousLayer)
	{
		Display.PreviousLayer.Primitives.assign(Before->Screen.Primitives.begin(), Before->Screen.Primitives.end());
		Display.Matches.assign(LayerMatches->begin(), LayerMatches->end());
	}
	ShowTextButtons(Display, Now);
	Display.Cover = Now.Cover;
	// A picture drawn from the screen needs the picture; one the frame does not show is not kept alive by it.
	Display.Picture = Now.Cover.bShown && Now.Cover.bScreen ? Picture : nullptr;
	return bBlended;
}

void FFieldView::ShowTextButtons(FFieldScreenFrame& Display, const Weltall::FFieldFrame& Now) const
{
	Display.IconVram = nullptr;
	if (Glyphs == nullptr || Now.TextButtons.empty())
	{
		return;
	}
	const uint32 Primitives = static_cast<uint32>(Display.Layer.Primitives.size());
	Icons.ApplyToText(Display.Layer, Now.TextButtons, *Glyphs, Display.bPreviousLayer ? &Display.Matches : nullptr, &Display.Cover.LayerIndex);
	if (Display.Layer.Primitives.size() != Primitives)
	{
		IconVram->Follow(Icons.Pictures());
		Display.IconVram = IconVram;
	}
}

TSharedPtr<const IScreenFrame> FFieldView::Capture(const Weltall::FFieldProgram& Program, float Alpha) const
{
	const Weltall::FFieldFrame* Latest = Program.LatestFrame();
	if (Latest == nullptr || !Mesh.IsValid())
	{
		return nullptr;
	}
	TSharedRef<FFieldScreenFrame>* Free = DisplayFrames.FindByPredicate([](const TSharedRef<FFieldScreenFrame>& Candidate) { return Candidate.IsUnique(); });
	TSharedRef<FFieldScreenFrame> Next = Free != nullptr ? *Free : DisplayFrames.Add_GetRef(MakeShared<FFieldScreenFrame>(Mesh.ToSharedRef(), Vram));
	const Weltall::FFieldFrame* Previous = Program.PreviousFrame();
	const bool bBlended = Fill(*Next, Program.Drawing().Scene(), Program.Drawing().Vram(), *Latest, Previous, Alpha, &Matches, ShownMovie);
	UnroundFieldSceneView(Next->Draw, *Latest, bBlended ? Previous : nullptr, Alpha);
	// The player's offsets show live rather than a tick behind: what the drawing shows of them moved to what they are now.
	Weltall::FFieldViewOffsets Drawn = Latest->ShownOffsets;
	if (bBlended && Previous != nullptr)
	{
		const Weltall::FFieldViewOffsets& Before = Previous->ShownOffsets;
		Drawn.Zoom = FMath::Lerp(Before.Zoom, Drawn.Zoom, Alpha);
		Drawn.Tilt = FMath::Lerp(Before.Tilt, Drawn.Tilt, Alpha);
		Drawn.Turn = Drawn.Turn - (1.0f - Alpha) * FMath::UnwindDegrees(Drawn.Turn - Before.Turn);
	}
	const Weltall::FFieldPlayerCamera& Player = Program.PlayerView();
	const Weltall::FFieldViewOffsets Live = Player.LiveOffsets();
	if (!(Live == Drawn))
	{
		MoveFieldSceneView(Next->Draw, Player.UnroundedViewWithOffsets(Drawn), Player.UnroundedViewWithOffsets(Live));
	}
	FollowTextureModSources(*Vram, Program.Drawing().Vram(), Mesh.Get(), {&Next->Draw}, {&Next->Layer}, Latest->Gears.TextureSeeds);
	return Next;
}

TSharedPtr<const IScreenFrame> FFieldView::CaptureStill(const Weltall::FFieldProgram& Program) const
{
	const Weltall::FFieldFrame* Latest = Program.LatestFrame();
	if (Latest == nullptr || !Mesh.IsValid())
	{
		return nullptr;
	}
	// The field does not tick under a menu: its picture is drawn once and shown again until it does, or its look changes.
	if (Still.IsValid() && StillMapLoads == MeshMapLoads && StillTick == Latest->Tick && StillOptions == Options && StillMesh == Mesh)
	{
		return Still;
	}
	const TSharedRef<FFieldScreenFrame> Next = MakeShared<FFieldScreenFrame>(Mesh.ToSharedRef(), Vram);
	Next->Compass = Options.Compass;
	BuildFieldFrameDraw(Program.Drawing().Scene(), *Latest, nullptr, 1.0f, *Mesh, Options, Next->Draw, TexelKindsOf(Program.Drawing().Vram(), *Latest));
	// The picture of the screen keeps its place among the layer's primitives that remain.
	Next->Cover = Latest->Cover;
	Next->Cover.LayerIndex = 0;
	for (std::size_t Index = 0; Index < Latest->Screen.Primitives.size(); ++Index)
	{
		if (Latest->Screen.Primitives[Index].BlendKey == Weltall::FFieldMenuShade::BlendKey)
		{
			continue;
		}
		if (Index < Latest->Cover.LayerIndex)
		{
			++Next->Cover.LayerIndex;
		}
		Next->Layer.Primitives.push_back(Latest->Screen.Primitives[Index]);
	}
	Next->Picture = Latest->Cover.bShown && Latest->Cover.bScreen ? Picture : nullptr;
	FollowTextureModSources(*Vram, Program.Drawing().Vram(), Mesh.Get(), {&Next->Draw}, {&Next->Layer}, Latest->Gears.TextureSeeds);
	Still = FFieldStillFrame::Make(Next);
	StillMapLoads = MeshMapLoads;
	StillTick = Latest->Tick;
	StillOptions = Options;
	StillMesh = Mesh;
	return Still;
}
