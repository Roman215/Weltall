#include "Text/MessageLog.h"

#include <algorithm>

namespace Weltall
{
	FMessageLog::FFollowed& FMessageLog::Followed(FMessageLogKey Key)
	{
		const auto Found = std::find_if(Windows.begin(), Windows.end(), [Key](const FFollowed& Window) { return Window.Key == Key; });
		if (Found != Windows.end())
		{
			return *Found;
		}
		FFollowed& Added = Windows.emplace_back();
		Added.Key = Key;
		return Added;
	}

	FLoggedMessage* FMessageLog::MessageOf(const FFollowed& Window)
	{
		if (!Window.bStarted || Window.Number < FirstNumber || Window.Number - FirstNumber >= Logged.size())
		{
			return nullptr;
		}
		return &Logged[static_cast<std::size_t>(Window.Number - FirstNumber)];
	}

	void FMessageLog::Follow(FMessageLogKey Key, const FMesWindow& Window)
	{
		FFollowed& Typing = Followed(Key);
		if (Window.TookString())
		{
			Typing.bStarted = false;
		}
		else if (Window.ClearedPage())
		{
			if (const FLoggedMessage* Message = MessageOf(Typing))
			{
				Typing.PageFirst = static_cast<int32>(Message->Lines.size());
			}
		}
		const std::span<const FMesPlacedGlyph> Glyphs = Window.TypedGlyphs();
		for (const FMesTypedLine& Line : Window.TypedLines())
		{
			if (Line.Count == 0)
			{
				continue;
			}
			FLoggedMessage* Message = MessageOf(Typing);
			if (Message == nullptr)
			{
				if (Logged.size() == MostMessages)
				{
					Logged.pop_front();
					++FirstNumber;
				}
				Logged.emplace_back();
				Typing.bStarted = true;
				Typing.Number = FirstNumber + Logged.size() - 1;
				// The message's first line is the first it shows a glyph on.
				Typing.PageFirst = -Line.Number;
				Message = &Logged.back();
			}
			const std::size_t Index = static_cast<std::size_t>(std::max(Typing.PageFirst + Line.Number, 0));
			if (Message->Lines.size() <= Index)
			{
				Message->Lines.resize(Index + 1);
			}
			const std::span<const FMesPlacedGlyph> Typed = Glyphs.subspan(static_cast<std::size_t>(Line.First), static_cast<std::size_t>(Line.Count));
			Message->Lines[Index].insert(Message->Lines[Index].end(), Typed.begin(), Typed.end());
			++Changes;
		}
	}

	void FMessageLog::Choose(FMessageLogKey Key, const FMesWindow& Window, int32 Row)
	{
		FLoggedMessage* Message = MessageOf(Followed(Key));
		if (Message == nullptr)
		{
			return;
		}
		const int32 Line = Followed(Key).PageFirst + Row;
		if (Line < 0 || Line >= static_cast<int32>(Message->Lines.size()))
		{
			return;
		}
		Message->Chosen = Line;
		Message->ChosenColor = Window.HighlightColor();
		++Changes;
	}
}
