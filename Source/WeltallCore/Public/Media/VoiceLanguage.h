#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/** The voice track used by new movies and battles; text follows the gameplay disc. */
	enum class EVoiceLanguage : uint8
	{
		Automatic,
		English,
		Japanese,
	};

	/** When movies show subtitles, which the supported undub's discs and Perfect Works Build carry. */
	enum class EMovieSubtitles : uint8
	{
		/** With the movies' Japanese sound (the undub's), as the undub shows them; a translation's with its own sound. */
		Automatic,
		/** Also over the USA's English sound, whose pictures are the same. */
		On,
		Off,
	};

	/** Whose movie subtitles are shown: each a translation of the Japanese sound, in the undub's form. */
	enum class EMovieSubtitleSource : uint8
	{
		/** Perfect Works Build's while the game uses the mod, the undub's otherwise; either where only it is there. */
		Automatic,
		/** The supported undub's, from its discs. */
		Undub,
		/** Perfect Works Build's, from the player's copy of its release. */
		PerfectWorks,
	};

	/** The voices and movie subtitles the player's discs and mods give, which the Sound page's rows offer. */
	struct FSoundSources
	{
		/** USA discs: the English voices. */
		bool bEnglish = false;
		/** The supported undub's discs: the Japanese voices, and its movie subtitles. */
		bool bUndub = false;
		/** Perfect Works Build's movie subtitles. */
		bool bPerfectWorksSubtitles = false;
		/** The game plays with Perfect Works Build, whose subtitles Auto then shows before the undub's. */
		bool bPerfectWorksInUse = false;
		/** A translation's discs with the English voices subtitle their own movies (the French translation's). */
		bool bTranslationSubtitles = false;

		bool HasSubtitles() const { return bUndub || bPerfectWorksSubtitles || bTranslationSubtitles; }
		bool operator==(const FSoundSources&) const = default;
	};
}
