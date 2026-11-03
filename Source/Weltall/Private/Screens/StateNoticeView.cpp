#include "StateNoticeView.h"

#include "Rendering/DrawListRendering.h"
#include "Rendering/SharedRenderResource.h"
#include "Rendering/VramTexture.h"

namespace
{
	constexpr Weltall::FScreenSize GameScreen{320, 224};
}

/** A displayed frame with a save state's line over it. */
class FStateNoticeFrame final : public IScreenFrame
{
public:
	explicit FStateNoticeFrame(TSharedRef<FVramTexture> InVram) : Vram(MoveTemp(InVram)) {}

	virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const override
	{
		if (Background.IsValid())
		{
			Background->AddPasses(GraphBuilder, Output, Area);
		}
		FDrawListFrame Frame;
		Frame.Latest = &Line;
		AddDrawListPasses(GraphBuilder, *Vram, Frame, Output, Area.ScreenRect(GameScreen), GameScreen);
	}

	virtual Weltall::FScreenSize ScreenSize() const override { return Background.IsValid() ? Background->ScreenSize() : GameScreen; }

	TSharedPtr<const IScreenFrame> Background;
	Weltall::FDrawList Line;

private:
	TSharedRef<FVramTexture> Vram;
};

FStateNoticeView::FStateNoticeView() : Vram(ShareRenderResource(new FVramTexture()))
{
}

FStateNoticeView::~FStateNoticeView() = default;

TSharedRef<const IScreenFrame> FStateNoticeView::Capture(const Weltall::FMesFont& Font, TSharedRef<const IScreenFrame> Background) const
{
	if (!Notice.IsShown())
	{
		return Background;
	}
	TSharedRef<FStateNoticeFrame>* Free = DisplayFrames.FindByPredicate([](const TSharedRef<FStateNoticeFrame>& Candidate) { return Candidate.IsUnique(); });
	TSharedRef<FStateNoticeFrame> Next = Free != nullptr ? *Free : DisplayFrames.Add_GetRef(MakeShared<FStateNoticeFrame>(Vram));
	Notice.Build(Font, Next->Line);
	Vram->Follow(Notice.Pictures());
	Next->Background = MoveTemp(Background);
	return Next;
}
