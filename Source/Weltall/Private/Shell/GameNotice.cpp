#include "Shell/GameNotice.h"

#include "Foundation/Result.h"
#include "GenericPlatform/GenericPlatformOutputDevices.h"
#include "Menu/OptionsText.h"
#include "Misc/Paths.h"
#include "Shell/GameVersion.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include <cctype>
#include <filesystem>
#include <string>

namespace
{
	/** The widest the text runs before it wraps, in Slate's units: a comfortable line on any display. */
	constexpr float NoticeWidth = 1100.0f;
	/** A phone's screen is small and its pixels fine: everything is drawn larger there. */
	constexpr float NoticeScale = PLATFORM_ANDROID || PLATFORM_IOS ? 1.6f : 1.0f;
}

FGameNotice MakeGameNotice(const FText& Text, bool bOverPicture, TArray<FNoticeButton> Buttons)
{
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 16);
	const auto Lines = [&Font](const FText& Shown)
	{
		return SNew(STextBlock).Text(Shown).Font(Font).ColorAndOpacity(FLinearColor::White).AutoWrapText(true);
	};
	const TSharedRef<STextBlock> Above = Lines(Text);
	const TSharedRef<SVerticalBox> Column = SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[Above];
	TSharedPtr<STextBlock> Below;
	TArray<TSharedRef<SWidget>> Pressed;
	if (!Buttons.IsEmpty())
	{
		Below = Lines(FText::GetEmpty());
		const TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		for (FNoticeButton& Button : Buttons)
		{
			const TSharedRef<SWidget> Shown = SNew(SButton)
				.ContentPadding(FMargin(28.0f, 14.0f))
				.OnClicked_Lambda([Action = MoveTemp(Button.Pressed)]()
				{
					Action();
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(Button.Label).Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
				];
			Row->AddSlot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 24.0f, 0.0f))[Shown];
			Pressed.Add(Shown);
		}
		Column->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(FMargin(0.0f, 28.0f, 0.0f, 20.0f))[Row];
		Column->AddSlot().AutoHeight()[Below.ToSharedRef()];
	}
	const TSharedRef<SBorder> Widget = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, bOverPicture ? 0.85f : 1.0f))
		.Padding(FMargin(48.0f, 40.0f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SDPIScaler)
			.DPIScale(NoticeScale)
			[
				SNew(SBox)
				.MaxDesiredWidth(NoticeWidth)
				[
					// A long list of files not used scrolls rather than run off the screen.
					SNew(SScrollBox) + SScrollBox::Slot()[Column]
				]
			]
		];
	return {Widget, Above, Below, Pressed};
}

FText GameStoppedText(const Weltall::FError& Error, bool bStarted, bool bLogButton)
{
	namespace Text = Weltall::OptionsText::Stop;
	// The reason as a sentence of its own.
	std::string Reason = Error.Message;
	if (!Reason.empty())
	{
		Reason[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(Reason[0])));
		if (Reason.back() != '.')
		{
			Reason += '.';
		}
	}
	std::string Message = std::string(bStarted ? Text::Stopped : Text::CannotStart) + "\n\n" + Reason + "\n\n";
	if (bLogButton)
	{
		Message += Text::LogPicked;
	}
	else
	{
		const FString LogFile = FPaths::ConvertRelativePathToFull(FGenericPlatformOutputDevices::GetAbsoluteLogFilename());
		const std::u8string LogName = std::filesystem::path(*LogFile).make_preferred().u8string();
		Message += Weltall::OptionsText::Fill(Text::Log, {std::string(LogName.begin(), LogName.end())});
	}
	if (bStarted)
	{
		Message += "\n\n" + std::string(Text::SavesKept);
	}
	Message += "\n\n" + Weltall::OptionsText::Fill(Text::Version, {std::string(TCHAR_TO_UTF8(*GameVersion()))});
	return FText::FromString(FString(UTF8_TO_TCHAR(Message.c_str())));
}
