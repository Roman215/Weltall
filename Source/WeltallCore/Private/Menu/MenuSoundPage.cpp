#include "Menu/MenuSoundPage.h"

#include "Menu/SettingsRows.h"

#include <algorithm>

namespace Weltall::SoundPage
{
	namespace
	{
		namespace Text = OptionsText::Sound;
		using SettingsRows::ChoiceName;
		using SettingsRows::Heading;
		using SettingsRows::OnOff;
		using SettingsRows::Shown;
		using SettingsRows::StepChoice;

		enum : int32 { VolumeHeading, Master, Music, Effects, Voices, Movies, OutputHeading, Output, InBackground, LanguageHeading, Language, Subtitles,
			SubtitleSource, Count };

		/** A volume's step, in percent. */
		constexpr int32 VolumeStep = 10;

		struct FVolumeRow
		{
			const OptionsText::FRow& Words;
			int32 FSoundVolumes::*Volume;
		};

		/** The volumes' rows, from Master on. */
		const std::array<FVolumeRow, 5> VolumeRows{{
			{Text::Master, &FSoundVolumes::Master},
			{Text::Music, &FSoundVolumes::Music},
			{Text::Effects, &FSoundVolumes::Effects},
			{Text::VoiceVolume, &FSoundVolumes::Voices},
			{Text::Movies, &FSoundVolumes::Movies},
		}};

		/**
		 * A row showing what the game has where there is nothing to choose between, and in its help why: the cursor stops
		 * there for the help, and Left and Right change nothing.
		 */
		FSettingsRow Fixed(const OptionsText::FRow& Words, std::string_view Value, const std::array<std::string_view, 3>& Help)
		{
			return {std::string(Words.Label), std::string(Value), OptionsText::Fill(Help)};
		}

		/** The discs give the voices of one language only. */
		bool OneLanguage(const FSoundSources& Sources)
		{
			return Sources.bEnglish != Sources.bUndub;
		}

		bool BothSubtitles(const FSoundSources& Sources)
		{
			return Sources.bUndub && Sources.bPerfectWorksSubtitles;
		}
	}

	int32 RowCount(const FSoundSources& Sources)
	{
		return Sources.HasSubtitles() ? Count : SubtitleSource;
	}

	FSettingsRow Row(int32 Row, const FPlayerSettings& Settings, const FSoundSources& Sources, bool bDiscsPicked)
	{
		switch (Row)
		{
		case VolumeHeading:
			return Heading(Text::Volume);
		case OutputHeading:
			return Heading(Text::OutputHeading);
		case LanguageHeading:
			return Heading(Text::Language);
		case Output:
			return Shown(Text::Output, ChoiceName(Settings.SoundOutput, Text::OutputValues));
		case InBackground:
			return Shown(Text::InBackground, OnOff(Settings.Display.bSoundInBackground));
		case Language:
			if (OneLanguage(Sources))
			{
				const auto& Why = bDiscsPicked ? (Sources.bEnglish ? Text::OnlyEnglishPicked : Text::OnlyJapanesePicked)
											   : (Sources.bEnglish ? Text::OnlyEnglish : Text::OnlyJapanese);
				return Fixed(Text::Voices, Text::VoiceValues[Sources.bEnglish ? 1 : 2], Why);
			}
			return Shown(Text::Voices, ChoiceName(Settings.VoiceLanguage, Text::VoiceValues));
		case Subtitles:
		{
			if (!Sources.HasSubtitles())
			{
				return Fixed(Text::Subtitles, Text::NoSubtitles, bDiscsPicked ? Text::NoSubtitlesPicked : Text::NoSubtitlesHelp);
			}
			FSettingsRow Choice = Shown(Text::Subtitles, ChoiceName(Settings.MovieSubtitles, Text::SubtitleValues));
			if (!Sources.bUndub)
			{
				Choice.Help = OptionsText::Fill(Sources.bTranslationSubtitles ? Text::TranslationSubtitles : Text::SubtitlesWithoutUndub);
			}
			return Choice;
		}
		case SubtitleSource:
			if (!BothSubtitles(Sources))
			{
				std::string_view Only = Text::TranslationSource;
				if (Sources.bUndub || Sources.bPerfectWorksSubtitles)
				{
					Only = Text::SubtitleSourceValues[Sources.bUndub ? 1 : 2];
				}
				return Fixed(Text::SubtitleSource, Only, Text::OnlySource);
			}
			return Shown(Text::SubtitleSource, ChoiceName(Settings.MovieSubtitleSource, Text::SubtitleSourceValues));
		default:
		{
			const FVolumeRow& Volume = VolumeRows[static_cast<std::size_t>(Row - Master)];
			return Shown(Volume.Words, OptionsText::Fill(Text::VolumeValue, {std::to_string(Settings.Volumes.*Volume.Volume)}));
		}
		}
	}

	bool Step(int32 Row, int32 Direction, FPlayerSettings& Settings, const FSoundSources& Sources)
	{
		switch (Row)
		{
		case Output:
			return StepChoice(Settings.SoundOutput, Direction, 3);
		case InBackground:
			Settings.Display.bSoundInBackground = !Settings.Display.bSoundInBackground;
			return true;
		case Language:
			return !OneLanguage(Sources) && StepChoice(Settings.VoiceLanguage, Direction, 3);
		case Subtitles:
			return Sources.HasSubtitles() && StepChoice(Settings.MovieSubtitles, Direction, 3);
		case SubtitleSource:
			return BothSubtitles(Sources) && StepChoice(Settings.MovieSubtitleSource, Direction, 3);
		case Master:
		case Music:
		case Effects:
		case Voices:
		case Movies:
		{
			// A volume between the steps (the settings file's) goes to the step past it.
			int32& Volume = Settings.Volumes.*VolumeRows[static_cast<std::size_t>(Row - Master)].Volume;
			const int32 Next = Direction > 0 ? Volume / VolumeStep * VolumeStep + VolumeStep : (Volume + VolumeStep - 1) / VolumeStep * VolumeStep - VolumeStep;
			const int32 Kept = std::clamp(Next, 0, FSoundVolumes::Full);
			if (Kept == Volume)
			{
				return false;
			}
			Volume = Kept;
			return true;
		}
		default:
			return false;
		}
	}
}
