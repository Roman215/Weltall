#include "TitleMarkView.h"

#include "Game/FieldProgram.h"
#include "Game/GameSession.h"
#include "Game/KernelState.h"
#include "Rendering/DrawListRendering.h"
#include "Rendering/SharedRenderResource.h"
#include "Rendering/VramTexture.h"
#include "Shell/GameVersion.h"

namespace
{
	constexpr Weltall::FScreenSize TitleScreen{320, 224};
}

/** A displayed frame with the title's mark over it. */
class FTitleMarkFrame final : public IScreenFrame
{
public:
	explicit FTitleMarkFrame(TSharedRef<FVramTexture> InVram) : Vram(MoveTemp(InVram)) {}

	virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const override
	{
		if (Background.IsValid())
		{
			Background->AddPasses(GraphBuilder, Output, Area);
		}
		FDrawListFrame Frame;
		Frame.Latest = &Mark;
		AddDrawListPasses(GraphBuilder, *Vram, Frame, Output, Area.ScreenRect(TitleScreen), TitleScreen);
	}

	TSharedPtr<const IScreenFrame> Background;
	Weltall::FDrawList Mark;

private:
	TSharedRef<FVramTexture> Vram;
};

FTitleMarkView::FTitleMarkView() : Version(TCHAR_TO_UTF8(*ProjectVersion())), Vram(ShareRenderResource(new FVramTexture()))
{
}

FTitleMarkView::~FTitleMarkView() = default;

TSharedRef<const IScreenFrame> FTitleMarkView::Capture(const Weltall::FGameSession& Session, const Weltall::FFieldProgram& Field,
	TSharedRef<const IScreenFrame> Background, bool bUnderMenu) const
{
	// The title's map also plays the opening movie after New Game: the mark is the title's alone.
	const bool bMovie = Field.Movie() != nullptr || Field.Runtime().LastFrame() == Weltall::EFieldFrame::Movie;
	if (Field.Runtime().Context().RunningMap != Weltall::FKernelState::TitleMap || bMovie)
	{
		return Background;
	}
	TSharedRef<FTitleMarkFrame>* Free = DisplayFrames.FindByPredicate([](const TSharedRef<FTitleMarkFrame>& Candidate) { return Candidate.IsUnique(); });
	TSharedRef<FTitleMarkFrame> Next = Free != nullptr ? *Free : DisplayFrames.Add_GetRef(MakeShared<FTitleMarkFrame>(Vram));
	// The mark fades in and out with the title's picture.
	const Weltall::FEventContext& Context = Field.Runtime().Context();
	Drawing.Build(Version, Weltall::FTitleMarkDrawing::Strength(Context.Render, bUnderMenu), Session.Text().Font, Next->Mark);
	Vram->Follow(Drawing.Pictures());
	Next->Background = MoveTemp(Background);
	return Next;
}
