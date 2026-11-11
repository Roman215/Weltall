#include "BattleSpeedMarkView.h"

#include "Rendering/DrawListRendering.h"
#include "Rendering/SharedRenderResource.h"
#include "Rendering/VramTexture.h"

/** A displayed frame with the battle speed's mark over it. */
class FBattleSpeedMarkFrame final : public IScreenFrame
{
public:
	explicit FBattleSpeedMarkFrame(TSharedRef<FVramTexture> InVram) : Vram(MoveTemp(InVram)) {}

	virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const override
	{
		if (Background.IsValid())
		{
			Background->AddPasses(GraphBuilder, Output, Area);
		}
		const FIntRect Rows = Area.Rows(ScreenSize());
		const Weltall::FDisplayRect Place = Weltall::PlaceBattleSpeedMark(Rows.Width(), Rows.Height());
		const FIntPoint Origin = Rows.Min + FIntPoint(Place.X, Place.Y);
		FDrawListFrame Frame;
		Frame.Latest = &Mark;
		AddDrawListPasses(GraphBuilder, *Vram, Frame, Output, FIntRect(Origin, Origin + FIntPoint(Place.Width, Place.Height)), Weltall::BattleSpeedMarkScreen);
	}

	virtual Weltall::FScreenSize ScreenSize() const override
	{
		return Background.IsValid() ? Background->ScreenSize() : IScreenFrame::ScreenSize();
	}

	TSharedPtr<const IScreenFrame> Background;
	Weltall::FDrawList Mark;

private:
	TSharedRef<FVramTexture> Vram;
};

FBattleSpeedMarkView::FBattleSpeedMarkView() : Vram(ShareRenderResource(new FVramTexture()))
{
}

FBattleSpeedMarkView::~FBattleSpeedMarkView() = default;

TSharedRef<const IScreenFrame> FBattleSpeedMarkView::Capture(Weltall::EBattleSpeed Speed, const Weltall::FMesFont& Font, TSharedRef<const IScreenFrame> Background) const
{
	if (Speed == Weltall::EBattleSpeed::Normal)
	{
		return Background;
	}
	TSharedRef<FBattleSpeedMarkFrame>* Free = DisplayFrames.FindByPredicate([](const TSharedRef<FBattleSpeedMarkFrame>& Candidate) { return Candidate.IsUnique(); });
	TSharedRef<FBattleSpeedMarkFrame> Next = Free != nullptr ? *Free : DisplayFrames.Add_GetRef(MakeShared<FBattleSpeedMarkFrame>(Vram));
	Drawing.Build(Speed, Font, Next->Mark);
	Vram->Follow(Drawing.Pictures());
	Next->Background = MoveTemp(Background);
	return Next;
}
