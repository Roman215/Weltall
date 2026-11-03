#pragma once

#include "CoreMinimal.h"
#include "Game/StateNotice.h"
#include "Screens/GameScreen.h"

#include <string>

class FStateNoticeFrame;
class FVramTexture;

namespace Weltall
{
	class FMesFont;
}

/** What a save state's key told (Weltall::FStateNotice), drawn over the game's frame while it shows. */
class FStateNoticeView
{
public:
	FStateNoticeView();
	~FStateNoticeView();

	/** Line shown from now, in place of any shown. */
	void Show(std::string Line) { Notice.Show(MoveTemp(Line)); }

	/** Real time passes. */
	void Advance(double Seconds) { Notice.Advance(Seconds); }

	/** Background with the line over it while it shows, in the game's font; Background itself otherwise. */
	TSharedRef<const IScreenFrame> Capture(const Weltall::FMesFont& Font, TSharedRef<const IScreenFrame> Background) const;

private:
	mutable Weltall::FStateNotice Notice;
	TSharedRef<FVramTexture> Vram;
	/** Frames handed out; one the rendering thread no longer holds is filled again. */
	mutable TArray<TSharedRef<FStateNoticeFrame>> DisplayFrames;
};
