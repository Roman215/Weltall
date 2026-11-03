#pragma once

#include "Battle/Show/BattleShowHud.h"
#include "Battle/Show/BattleShowSeams.h"
#include "Foundation/Types.h"
#include "Foundation/UnreachedCases.h"
#include "Text/MessageTable.h"

#include <optional>

namespace Weltall
{
	class FBattleEventFiles;

	/**
	 * The battle's HUD in the game (FBattleHud) behind what the show and the program call: made for each battle in the
	 * show's VRAM, typing the scene's names and the battle's messages into its boxes, and showing a battle event's
	 * message in its window with the portrait read from the event's files.
	 */
	class FBattleGameHud final : public IBattleShowHud
	{
	public:
		/**
		 * Resources, Text, EventFiles and Unreached must outlive the HUD; Settings are the kernel's window colour and blend.
		 * Without the event's files a message's portrait stops the HUD. A message that starts past its table (a translation's
		 * tool can leave one so, and the game types the memory after it) is typed as none where Unreached lets play carry on.
		 */
		WELTALLCORE_API FBattleGameHud(const FBattleHudResources& Resources, FMesContext& Text, const FBattleHudSettings& Settings,
			const FBattleEventFiles* EventFiles = nullptr, FUnreachedCases* Unreached = nullptr);

		WELTALLCORE_API void Begin(FBattleContext& Context, FVram& Vram) override;
		WELTALLCORE_API void BeginMessageBoxes() override;
		WELTALLCORE_API void Flip() override;
		WELTALLCORE_API void Draw(FBattleHudFrame& Hud, bool bHidden) override;
		WELTALLCORE_API void Tick(uint8 InputMode) override;
		WELTALLCORE_API void TypeMessage(uint8 Box, uint8 Entry) override;
		WELTALLCORE_API void TypeActionName(uint8 Message) override;
		WELTALLCORE_API void ShowNameBox(uint8 Entry) override;
		WELTALLCORE_API void ShowMessageBox(int32 Box, bool bShown) override;
		WELTALLCORE_API void HideMessageBoxes() override;
		WELTALLCORE_API void PlayMenuCue(EBattleMenuCue Cue, uint8 Slot, uint8 Arg) override;
		WELTALLCORE_API void PlayEventCue(const FBattleEventCue& Cue) override;
		WELTALLCORE_API bool HasStopped() const override;

		/** Why the HUD stopped, if it did. */
		WELTALLCORE_API EBattleHudFault Fault() const;
		/** Whether the lists' cursor's sprites are shown (a list's cursor or the event's arrow). */
		WELTALLCORE_API bool IsListCursorShown() const;
		/** The pictures the HUD's sheet holds (FBattleHud::TextureRegions); none before the battle begins. */
		WELTALLCORE_API std::span<const FTextureRegion> TextureRegions() const;

	private:
		/** Entry Number of Table as the game reads it; past the table, or starting past its bytes, the HUD stops. */
		std::span<const uint8> Entry(const std::optional<FMessageTable>& Table, uint8 Number);

		const FBattleHudResources& Resources;
		FMesContext& Text;
		FBattleHudSettings Settings;
		const FBattleEventFiles* EventFiles = nullptr;
		FUnreachedCases* Unreached = nullptr;
		FBattleContext* Context = nullptr;
		std::optional<FBattleHud> Hud;
		std::optional<FMessageTable> Names;
		std::optional<FMessageTable> Messages;
		EBattleHudFault TableFault = EBattleHudFault::None;
	};
}
