#include "ChapterCardView.h"

#include "Game/ChapterCard.h"
#include "Game/GameSession.h"
#include "Rendering/DrawListRendering.h"
#include "Rendering/SharedRenderResource.h"
#include "Rendering/VramTexture.h"

/** A displayed frame with a chapter's card over it. */
class FChapterCardFrame final : public IScreenFrame
{
public:
	explicit FChapterCardFrame(TSharedRef<FVramTexture> InVram) : Vram(MoveTemp(InVram)) {}

	virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const override
	{
		if (Background.IsValid())
		{
			Background->AddPasses(GraphBuilder, Output, Area);
		}
		FDrawListFrame Frame;
		Frame.Latest = &Card;
		AddDrawListPasses(GraphBuilder, *Vram, Frame, Output, Area.ScreenRect(Weltall::ChapterCardScreen), Weltall::ChapterCardScreen);
	}

	TSharedPtr<const IScreenFrame> Background;
	Weltall::FDrawList Card;

private:
	TSharedRef<FVramTexture> Vram;
};

FChapterCardView::FChapterCardView() : Vram(ShareRenderResource(new FVramTexture()))
{
}

FChapterCardView::~FChapterCardView() = default;

TSharedRef<const IScreenFrame> FChapterCardView::Capture(const Weltall::FGameSession& Session, TSharedRef<const IScreenFrame> Background) const
{
	const std::string Line = Session.ChapterCardLine();
	if (Line.empty() || Session.ChapterCard().Level() <= 0)
	{
		return Background;
	}
	TSharedRef<FChapterCardFrame>* Free = DisplayFrames.FindByPredicate([](const TSharedRef<FChapterCardFrame>& Candidate) { return Candidate.IsUnique(); });
	TSharedRef<FChapterCardFrame> Next = Free != nullptr ? *Free : DisplayFrames.Add_GetRef(MakeShared<FChapterCardFrame>(Vram));
	Drawing.Build(Line, Session.ChapterCard().Level(), Session.Text().Font, Next->Card);
	Vram->Follow(Drawing.Pictures());
	Next->Background = MoveTemp(Background);
	return Next;
}
