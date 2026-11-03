#pragma once

#include "CoreMinimal.h"
#include "Game/MessageLogScreen.h"
#include "Menu/MenuButtonIcons.h"
#include "Screens/GameScreen.h"

#include <array>
#include <span>
#include <vector>

class FMessageLogFrame;
class FVramTexture;

namespace Weltall
{
	class FMesFont;
	class FMessageLog;
	class IButtonGlyphs;
}

/** The message log (Weltall::FMessageLogScreen) while the player has it open, drawn over the game's frame with the player's buttons. */
class FMessageLogView
{
public:
	FMessageLogView();
	~FMessageLogView();

	/** Opened over the game at Log's newest message (Weltall::FMessageLogScreen's constructor). */
	void Open(const Weltall::FMessageLog& Log, const Weltall::FMesFont& Font, std::span<const uint16, 32> TextClut, std::array<uint8, 3> WindowColour);

	void Close() { Screen.Reset(); }

	bool IsOpen() const { return Screen.IsValid(); }

	/** A display frame with Held down while it is open; closed once the player closes it. */
	void Update(double Seconds, Weltall::FPadState Held);

	void Scroll(int32 Lines);

	/** Background with the log over it while it is open, its buttons as Glyphs show them (none: the font's); Background itself otherwise. */
	TSharedRef<const IScreenFrame> Capture(const Weltall::IButtonGlyphs* Glyphs, TSharedRef<const IScreenFrame> Background) const;

private:
	TUniquePtr<Weltall::FMessageLogScreen> Screen;
	TSharedRef<FVramTexture> Vram;
	TSharedRef<FVramTexture> IconVram;
	mutable Weltall::FMenuButtonIcons Icons;
	mutable std::vector<Weltall::FTextButtonCell> Cells;
	/** Frames handed out; one the rendering thread no longer holds is filled again. */
	mutable TArray<TSharedRef<FMessageLogFrame>> DisplayFrames;
};
