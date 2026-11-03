#pragma once

#include "Foundation/Types.h"
#include "Text/MesWindow.h"

#include <array>
#include <deque>
#include <vector>

namespace Weltall
{
	/** Which mode's dialogue window a logged message was typed in. */
	enum class EMessageLogSource : uint8
	{
		Field,
		BattleEvent,
		Battling,
	};

	/** A dialogue window the log follows: its mode, and which of the mode's windows (the field has four). */
	struct FMessageLogKey
	{
		EMessageLogSource Source = EMessageLogSource::Field;
		uint8 Window = 0;

		bool operator==(const FMessageLogKey&) const = default;
	};

	/** A message as the player was shown it: its lines as typed, glyph by glyph, inserts and all, every page one after another. */
	struct FLoggedMessage
	{
		std::vector<std::vector<FMesPlacedGlyph>> Lines;
		/** The line a choice ended on, or -1, and the colour its window drew the choice in. */
		int32 Chosen = -1;
		std::array<uint8, 3> ChosenColor{};
	};

	/**
	 * The dialogue the player has been shown, for the message log (the port's): the field's windows, the battle events'
	 * and Battling's tutorial's, as their kernel windows type it, the latest messages kept. A window's string starts a
	 * message at its first glyph; a page the button turns goes on in the same message.
	 */
	class FMessageLog
	{
	public:
		static constexpr std::size_t MostMessages = 200;

		/** What Window's latest Update typed, the window Key's. */
		WELTALLCORE_API void Follow(FMessageLogKey Key, const FMesWindow& Window);

		/** The choice in window Key ended on row Row of its page, which Window drew in its highlight's colour. */
		WELTALLCORE_API void Choose(FMessageLogKey Key, const FMesWindow& Window, int32 Row);

		/** The messages kept, the oldest first. */
		const std::deque<FLoggedMessage>& Messages() const { return Logged; }

		/** Changes whenever a message is added to or changed. */
		uint64 Revision() const { return Changes; }

	private:
		/** A window's message being typed: its number (counted from the log's first) once its first glyph started it. */
		struct FFollowed
		{
			FMessageLogKey Key;
			bool bStarted = false;
			uint64 Number = 0;
			/** The message's line that the page's first is (less than 0 where the message began on a later line of it). */
			int32 PageFirst = 0;
		};

		FFollowed& Followed(FMessageLogKey Key);
		FLoggedMessage* MessageOf(const FFollowed& Window);

		std::deque<FLoggedMessage> Logged;
		/** The number of the oldest message kept. */
		uint64 FirstNumber = 0;
		std::vector<FFollowed> Windows;
		uint64 Changes = 0;
	};

	/** A tick of dialogue window Key (FMesWindow::Update), what it typed recorded in Context's log when it has one. */
	inline void UpdateLoggedWindow(FMesWindow& Window, FMesContext& Context, FMessageLogKey Key)
	{
		Window.Update(Context);
		if (Context.Log != nullptr)
		{
			Context.Log->Follow(Key, Window);
		}
	}

	/** A choice of dialogue window Key ended on Row (FMessageLog::Choose), when Context has a log. */
	inline void LogChoice(const FMesContext& Context, FMessageLogKey Key, const FMesWindow& Window, int32 Row)
	{
		if (Context.Log != nullptr)
		{
			Context.Log->Choose(Key, Window, Row);
		}
	}
}
