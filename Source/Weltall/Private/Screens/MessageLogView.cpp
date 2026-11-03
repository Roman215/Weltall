#include "MessageLogView.h"

#include "Rendering/DrawListRendering.h"
#include "Rendering/SharedRenderResource.h"
#include "Rendering/VramTexture.h"

namespace
{
	constexpr Weltall::FScreenSize LogScreenSize{320, 224};
}

/** A displayed frame with the message log over the game's. */
class FMessageLogFrame final : public IScreenFrame
{
public:
	FMessageLogFrame(TSharedRef<FVramTexture> InVram, TSharedRef<FVramTexture> InIconVram) : Vram(MoveTemp(InVram)), IconVram(MoveTemp(InIconVram)) {}

	virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const override
	{
		if (Background.IsValid())
		{
			Background->AddPasses(GraphBuilder, Output, Area);
		}
		AddIconDrawListPasses(GraphBuilder, *Vram, *IconVram, Log, Output, Area.ScreenRect(LogScreenSize), LogScreenSize);
	}

	virtual Weltall::FScreenSize ScreenSize() const override { return Background.IsValid() ? Background->ScreenSize() : LogScreenSize; }

	TSharedPtr<const IScreenFrame> Background;
	Weltall::FDrawList Log;

private:
	TSharedRef<FVramTexture> Vram;
	TSharedRef<FVramTexture> IconVram;
};

FMessageLogView::FMessageLogView() : Vram(ShareRenderResource(new FVramTexture())), IconVram(ShareRenderResource(new FVramTexture()))
{
}

FMessageLogView::~FMessageLogView() = default;

void FMessageLogView::Open(const Weltall::FMessageLog& Log, const Weltall::FMesFont& Font, std::span<const uint16, 32> TextClut, std::array<uint8, 3> WindowColour)
{
	Screen = MakeUnique<Weltall::FMessageLogScreen>(Log, Font, TextClut, WindowColour);
}

void FMessageLogView::Update(double Seconds, Weltall::FPadState Held)
{
	if (Screen.IsValid() && !Screen->Update(Seconds, Held))
	{
		Screen.Reset();
	}
}

void FMessageLogView::Scroll(int32 Lines)
{
	if (Screen.IsValid())
	{
		Screen->Scroll(Lines);
	}
}

TSharedRef<const IScreenFrame> FMessageLogView::Capture(const Weltall::IButtonGlyphs* Glyphs, TSharedRef<const IScreenFrame> Background) const
{
	if (!Screen.IsValid())
	{
		return Background;
	}
	TSharedRef<FMessageLogFrame>* Free = DisplayFrames.FindByPredicate([](const TSharedRef<FMessageLogFrame>& Candidate) { return Candidate.IsUnique(); });
	TSharedRef<FMessageLogFrame> Next = Free != nullptr ? *Free : DisplayFrames.Add_GetRef(MakeShared<FMessageLogFrame>(Vram, IconVram));
	Screen->Build(Next->Log, Cells);
	if (Glyphs != nullptr && !Cells.empty())
	{
		Icons.ApplyToText(Next->Log, Cells, *Glyphs);
		IconVram->Follow(Icons.Pictures());
	}
	Vram->Follow(Screen->Pictures());
	Next->Background = MoveTemp(Background);
	return Next;
}
