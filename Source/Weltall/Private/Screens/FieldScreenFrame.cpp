#include "FieldScreenFrame.h"

#include "Field/FieldRenderState.h"
#include "Presentation/ScreenLayout.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "Rendering/DrawListRendering.h"
#include "Rendering/FieldPictureRendering.h"
#include "Rendering/FieldScreenUnitsRendering.h"
#include "Rendering/MovieRendering.h"
#include "Rendering/VramTexture.h"

namespace
{
	/** The field's screen, which its 2D layer is placed on. */
	constexpr Weltall::FScreenSize FieldScreen{320, 224};
	constexpr float NeutralBrightness = 128.0f;
}

TSharedRef<FFieldCoverPicture> FFieldCoverPicture::Make(TSharedPtr<const IScreenFrame> Shown)
{
	FFieldCoverPicture* Made = new FFieldCoverPicture();
	Made->Shown = MoveTemp(Shown);
	return TSharedRef<FFieldCoverPicture>(Made, [](FFieldCoverPicture* Released)
	{
		ENQUEUE_RENDER_COMMAND(ReleaseFieldCoverPicture)([Released](FRHICommandListImmediate&) { delete Released; });
	});
}

FRDGTextureRef FFieldCoverPicture::Texture(FRDGBuilder& GraphBuilder, const FScreenArea& Area)
{
	return Draw(GraphBuilder, Area.At(FIntPoint::ZeroValue), Drawn);
}

FRDGTextureRef FFieldCoverPicture::ScreenPixels(FRDGBuilder& GraphBuilder)
{
	return Draw(GraphBuilder, FScreenArea(FIntRect(0, 0, FieldScreen.Width, FieldScreen.Height)), DrawnAtScreen);
}

FRDGTextureRef FFieldCoverPicture::Draw(FRDGBuilder& GraphBuilder, const FScreenArea& Area, FDrawn& Into)
{
	const FIntPoint Size = Area.Size();
	if (Into.Target.IsValid() && Into.Area == Area)
	{
		return GraphBuilder.RegisterExternalTexture(Into.Target);
	}
	if (!Shown.IsValid())
	{
		return nullptr;
	}
	FRDGTextureRef Picture = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(Size, PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("FieldCoverPicture"));
	AddClearRenderTargetPass(GraphBuilder, Picture, FLinearColor::Black);
	Shown->AddPasses(GraphBuilder, Picture, Area);
	GraphBuilder.QueueTextureExtraction(Picture, &Into.Target);
	Into.Area = Area;
	return Picture;
}

TSharedRef<FFieldStillFrame> FFieldStillFrame::Make(TSharedRef<const IScreenFrame> Frame)
{
	return TSharedRef<FFieldStillFrame>(new FFieldStillFrame(MoveTemp(Frame)), [](FFieldStillFrame* Released)
	{
		ENQUEUE_RENDER_COMMAND(ReleaseFieldStillFrame)([Released](FRHICommandListImmediate&) { delete Released; });
	});
}

void FFieldStillFrame::AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const
{
	const FRDGTextureDesc& Desc = Output->Desc;
	if (Drawn.IsValid() && Drawn->GetDesc().Extent == Desc.Extent && Drawn->GetDesc().Format == Desc.Format && DrawnArea == Area)
	{
		AddCopyTexturePass(GraphBuilder, GraphBuilder.RegisterExternalTexture(Drawn), Output);
		return;
	}
	FRDGTextureRef Picture = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(Desc.Extent, Desc.Format, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("FieldStillPicture"));
	AddClearRenderTargetPass(GraphBuilder, Picture, FLinearColor::Black);
	Frame->AddPasses(GraphBuilder, Picture, Area);
	GraphBuilder.QueueTextureExtraction(Picture, &Drawn);
	DrawnArea = Area;
	AddCopyTexturePass(GraphBuilder, Picture, Output);
}

void WriteScreenIntoVram(const TSharedRef<FVramTexture>& Vram, const std::vector<Weltall::FFieldScreenInVram>& Written,
	const TMap<int32, TSharedPtr<FFieldCoverPicture>>& Pictures)
{
	static std::vector<Weltall::FVramRect> Exclusions;
	Exclusions.clear();
	for (const auto& Region : Written) Exclusions.push_back(Region.Rect);
	Vram->SetReplacementExclusions(Exclusions);
	struct FWrite
	{
		FFieldScreenUnits Units;
		TSharedPtr<FFieldCoverPicture> Picture;
	};
	TArray<FWrite> Writes;
	for (const Weltall::FFieldScreenInVram& Region : Written)
	{
		FWrite& Write = Writes.AddDefaulted_GetRef();
		Write.Units.Rect = FIntRect(Region.Rect.X, Region.Rect.Y, Region.Rect.X + Region.Rect.Width, Region.Rect.Y + Region.Rect.Height);
		Write.Units.ScreenOffset = FIntPoint(Region.ScreenX, Region.ScreenY);
		Write.Units.Brightness = Region.Brightness;
		if (const TSharedPtr<FFieldCoverPicture>* Found = Pictures.Find(Region.Taken))
		{
			Write.Picture = *Found;
		}
	}
	if (Writes.IsEmpty())
	{
		return;
	}
	ENQUEUE_RENDER_COMMAND(WriteFieldScreenIntoVram)([Vram, Writes = MoveTemp(Writes)](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		FRDGTextureRef Units = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Vram->Texture(), TEXT("PlayStationVram")));
		for (const FWrite& Write : Writes)
		{
			FRDGTextureRef Source = Write.Picture.IsValid() ? Write.Picture->ScreenPixels(GraphBuilder) : nullptr;
			AddFieldScreenUnitsPass(GraphBuilder, Source, Units, Write.Units);
		}
		GraphBuilder.SetTextureAccessFinal(Units, ERHIAccess::SRVMask);
		GraphBuilder.Execute();
	});
}

FFieldScreenFrame::FFieldScreenFrame(TSharedRef<const FFieldSceneMesh> InMesh, TSharedRef<FVramTexture> InVram)
	: Mesh(MoveTemp(InMesh)), Vram(MoveTemp(InVram))
{
}

void FFieldScreenFrame::AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const
{
	const bool bScreenPicture = Cover.bShown && Cover.bScreen && Picture.IsValid();
	FRDGTextureRef PictureTexture = bScreenPicture ? Picture->Texture(GraphBuilder, Area) : nullptr;
	const FIntRect ScreenRect = Area.ScreenRect(FieldScreen);
	const FScreenArea Rows = Area.Rows(FieldScreen);
	if (MoviePicture.IsValid())
	{
		const Weltall::FScreenRect WholeScreen{0, 0, static_cast<int16>(FieldScreen.Width), static_cast<int16>(FieldScreen.Height)};
		AddMoviePasses(GraphBuilder, *MoviePicture, Output, ScreenRect, FieldScreen, WholeScreen);
	}
	else
	{
		AddFieldScenePasses(GraphBuilder, *Mesh, *Vram, Draw, Output, Rows);
	}
	FDrawListFrame Frame{&Layer, bPreviousLayer ? &PreviousLayer : nullptr, bPreviousLayer ? &Matches : nullptr, Alpha};
	Frame.bHideRightAnchored = Compass == Weltall::ECompassPlacement::Off;
	if (!Frame.bHideRightAnchored && !MoviePicture.IsValid() && !Draw.bClipped && !Area.bTouchOverlay)
	{
		const int32 FieldRight = Draw.PictureStretch.bStaged ? Rows.Min.X + FieldPictureArea(Rows.Size()).Max.X : Rows.Max.X;
		const int32 CompassRight = Area.Min.X + Weltall::CompassRightEdge(Area.Width(), Area.Height(), Compass);
		Frame.RightShift = FMath::Min(CompassRight, FieldRight) - ScreenRect.Max.X;
	}
	Frame.End = Cover.LayerIndex;
	const auto AddLayer = [&]()
	{
		if (IconVram.IsValid())
		{
			AddIconDrawListPasses(GraphBuilder, *Vram, *IconVram, Frame, Output, ScreenRect, FieldScreen);
		}
		else
		{
			AddDrawListPasses(GraphBuilder, *Vram, Frame, Output, ScreenRect, FieldScreen);
		}
	};
	AddLayer();
	if (PictureTexture != nullptr)
	{
		// The picture covers the whole area it was drawn at, scaled about its centre.
		const float Scale = static_cast<float>(Cover.Scale) / Weltall::FFieldCover::UnitScale;
		const FVector2f Centre = FVector2f(Area.Min) + FVector2f(Area.Size()) * 0.5f;
		const FVector2f Half = FVector2f(Area.Size()) * 0.5f * Scale;
		const FVector4f Corners[4] = {FVector4f(-1.0f, -1.0f, 0.0f, 0.0f), FVector4f(1.0f, -1.0f, 1.0f, 0.0f), FVector4f(-1.0f, 1.0f, 0.0f, 1.0f), FVector4f(1.0f, 1.0f, 1.0f, 1.0f)};
		TArray<FVector4f, TInlineAllocator<6>> Quad;
		for (const int32 Corner : {0, 1, 2, 1, 3, 2})
		{
			Quad.Add(Corners[Corner]);
		}
		FFieldPictureDraw Drawn;
		Drawn.Vertices = Quad;
		Drawn.Placement = FVector4f(Half.X, Half.Y, Centre.X, Centre.Y);
		Drawn.ColorScale = static_cast<float>(Cover.Brightness) / NeutralBrightness;
		Drawn.Blend = Cover.bAdditive ? EFieldPictureBlend::Add : EFieldPictureBlend::OpaqueBlackTransparent;
		Drawn.bScissor = true;
		Drawn.Scissor = Area;
		AddFieldPicturePass(GraphBuilder, PictureTexture, Output, Drawn);
	}
	Frame.First = Cover.LayerIndex;
	Frame.End = Layer.Primitives.size();
	AddLayer();
}
