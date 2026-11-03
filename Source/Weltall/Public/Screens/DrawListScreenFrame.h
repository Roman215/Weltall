#pragma once

#include "CoreMinimal.h"
#include "Presentation/DrawList.h"
#include "Screens/GameScreen.h"

#include <memory>
#include <vector>

class FDrawListTextures;

/**
 * A displayed frame of a screen the GPU's 2D rules draw: the latest tick's list, with what changed
 * since the tick before drawn Alpha of the way there, the game screen fitted into the display.
 */
class WELTALL_API FDrawListScreenFrame final : public IScreenFrame
{
public:
	FDrawListScreenFrame(Weltall::FScreenSize InScreen, TSharedRef<FDrawListTextures> InTextures, std::shared_ptr<const Weltall::FDrawList> InLatest,
		std::shared_ptr<const Weltall::FDrawList> InPrevious, std::shared_ptr<const std::vector<int32>> InMatches, float InAlpha);

	virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const override;
	virtual Weltall::FScreenSize ScreenSize() const override { return Screen; }

	/** The frame filled again for another display frame, once the rendering thread has let go of it. */
	void Set(std::shared_ptr<const Weltall::FDrawList> InLatest, std::shared_ptr<const Weltall::FDrawList> InPrevious,
		std::shared_ptr<const std::vector<int32>> InMatches, float InAlpha);

private:
	Weltall::FScreenSize Screen;
	TSharedRef<FDrawListTextures> Textures;
	std::shared_ptr<const Weltall::FDrawList> Latest;
	std::shared_ptr<const Weltall::FDrawList> Previous;
	std::shared_ptr<const std::vector<int32>> Matches;
	float Alpha;
};
