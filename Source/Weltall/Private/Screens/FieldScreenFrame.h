#pragma once

#include "CoreMinimal.h"
#include "Field/FieldFrame.h"
#include "Presentation/DrawList.h"
#include "RenderGraphResources.h"
#include "Rendering/FieldSceneRendering.h"
#include "Screens/GameScreen.h"

#include <vector>

class FMoviePictureTexture;
class FVramTexture;

/**
 * The field's picture of the screen: the frame it showed last, drawn once at the display's
 * resolution on the rendering thread from the frame that showed it, and kept for the frames that
 * draw it over the field. None when nothing was shown before it (black).
 */
class FFieldCoverPicture final
{
public:
	/** A picture of Shown (none: black), shared by the game thread's frames and drawn and released on the rendering thread. */
	static TSharedRef<FFieldCoverPicture> Make(TSharedPtr<const IScreenFrame> Shown);

	/** The picture as Area shows it, at its size, drawn the first time it is asked for (and again for another). Rendering thread. */
	FRDGTextureRef Texture(FRDGBuilder& GraphBuilder, const FScreenArea& Area);

	/** The picture at the field's own 320 x 224, a pixel of the screen a texel, drawn once; none for a black picture. Rendering thread. */
	FRDGTextureRef ScreenPixels(FRDGBuilder& GraphBuilder);

private:
	/** A picture kept, and the scale its area laid screens out at. */
	struct FDrawn
	{
		TRefCountPtr<IPooledRenderTarget> Target;
		int32 WholeScale = 0;
	};

	FRDGTextureRef Draw(FRDGBuilder& GraphBuilder, const FScreenArea& Area, FDrawn& Into);

	TSharedPtr<const IScreenFrame> Shown;
	FDrawn Drawn;
	FDrawn DrawnAtScreen;
};

/**
 * A frame standing still, as the field does under a menu: drawn once into a picture the size of the
 * output (what it draws past its area included), then copied as it is until the output or the area
 * changes. Drawn and released on the rendering thread.
 */
class FFieldStillFrame final : public IScreenFrame
{
public:
	static TSharedRef<FFieldStillFrame> Make(TSharedRef<const IScreenFrame> Frame);

	virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const override;
	virtual Weltall::FScreenSize ScreenSize() const override { return Frame->ScreenSize(); }

private:
	explicit FFieldStillFrame(TSharedRef<const IScreenFrame> InFrame) : Frame(MoveTemp(InFrame)) {}

	TSharedRef<const IScreenFrame> Frame;
	mutable TRefCountPtr<IPooledRenderTarget> Drawn;
	mutable FScreenArea DrawnArea;
};

/**
 * Writes the screen's pixels the frame says VRAM holds into Vram's texture, from the pictures of the
 * screen they were copied from (Pictures by their count of pictures taken; one missing writes black).
 * Game thread: the writing is queued behind VRAM's last sending.
 */
void WriteScreenIntoVram(const TSharedRef<FVramTexture>& Vram, const std::vector<Weltall::FFieldScreenInVram>& Written,
	const TMap<int32, TSharedPtr<FFieldCoverPicture>>& Pictures);

/**
 * A displayed frame of the field: its scene, or the picture of a movie it plays full screen, then the
 * screen's 2D layer over it (the cockpit's instruments, the fades, the message windows, the markers,
 * Select's picture: what changes drawn Alpha of the way from the tick before's), and the picture of
 * the screen among the layer where the tick drew it, all textured from VRAM as the latest tick left it.
 */
class FFieldScreenFrame final : public IScreenFrame
{
public:
	FFieldScreenFrame(TSharedRef<const FFieldSceneMesh> InMesh, TSharedRef<FVramTexture> InVram);

	virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const override;

	FFieldSceneDraw Draw;
	/** A movie the field shows in place of its scene, the whole screen its picture. */
	TSharedPtr<FMoviePictureTexture> MoviePicture;
	Weltall::FDrawList Layer;
	Weltall::FDrawList PreviousLayer;
	std::vector<int32> Matches;
	bool bPreviousLayer = false;
	float Alpha = 1.0f;
	Weltall::FFieldFrameCover Cover;
	TSharedPtr<FFieldCoverPicture> Picture;
	/** The player's button pictures the layer's text shows (Weltall::FMenuButtonIcons::PicturesVram), when it shows any. */
	TSharedPtr<FVramTexture> IconVram;

	const TSharedRef<const FFieldSceneMesh>& GetMesh() const { return Mesh; }

private:
	TSharedRef<const FFieldSceneMesh> Mesh;
	TSharedRef<FVramTexture> Vram;
};
