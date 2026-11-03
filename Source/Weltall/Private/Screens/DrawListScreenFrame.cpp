#include "Screens/DrawListScreenFrame.h"

#include "Rendering/DrawListRendering.h"
#include "Rendering/DrawListTextures.h"

FDrawListScreenFrame::FDrawListScreenFrame(Weltall::FScreenSize InScreen, TSharedRef<FDrawListTextures> InTextures,
	std::shared_ptr<const Weltall::FDrawList> InLatest, std::shared_ptr<const Weltall::FDrawList> InPrevious,
	std::shared_ptr<const std::vector<int32>> InMatches, float InAlpha)
	: Screen(InScreen), Textures(MoveTemp(InTextures)), Latest(std::move(InLatest)), Previous(std::move(InPrevious)),
	  Matches(std::move(InMatches)), Alpha(InAlpha)
{
}

void FDrawListScreenFrame::Set(std::shared_ptr<const Weltall::FDrawList> InLatest, std::shared_ptr<const Weltall::FDrawList> InPrevious,
	std::shared_ptr<const std::vector<int32>> InMatches, float InAlpha)
{
	Latest = std::move(InLatest);
	Previous = std::move(InPrevious);
	Matches = std::move(InMatches);
	Alpha = InAlpha;
}

void FDrawListScreenFrame::AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const
{
	const FDrawListFrame Frame{Latest.get(), Previous.get(), Matches.get(), Alpha};
	AddDrawListPasses(GraphBuilder, *Textures, Frame, Output, Area.ScreenRect(Screen), Screen);
}
