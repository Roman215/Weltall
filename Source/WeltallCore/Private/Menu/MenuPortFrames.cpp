#include "Menu/MenuPortFrames.h"

#include "Foundation/Sha1.h"
#include "Menu/MenuResources.h"
#include "Menu/MenuSpriteSheet.h"
#include "Text/MesFont.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <vector>

namespace Weltall
{
	namespace
	{
		/** The Gear screen's "Gear Option" label: its p, its "tion" and its O; and "Status", whose last letter is an s. */
		constexpr int32 GearOptionLabel = 0x13C;
		constexpr int32 StatusLabel = 0x12B;
		constexpr int32 StatusOrb = 0x10F;
		constexpr int32 SelectedOrbStep = 0xD;
		/** The s of "Status": its columns in the word, outlines included. */
		constexpr int32 StatusS = 26;
		constexpr int32 LetterSWidth = 6;
		/** Each piece of "Options" from the O's place: the p sits two rows lower, its stroke reaching down. */
		constexpr int32 PAfterO = 6;
		constexpr int32 PDrop = 2;
		constexpr int32 TionAfterO = 11;
		/** The s takes the column of the n's outline as its own. */
		constexpr int32 SAfterO = 30;
		/** The O of "Options" beside the title's Sound orb, where "Sound" starts. */
		constexpr int32 TitleOptionsX = -85;
		constexpr int32 TitleOptionsY = 49;
		/** The Normal Menu's eighth orb: Status's, a step further up the arc; its label beside it as Status's is. */
		constexpr int32 OrbStepX = -10;
		constexpr int32 OrbStepY = -16;
		constexpr int32 OptionsX = -145;
		constexpr int32 OptionsY = -71;
		/** The USA release's letters of "Options" (OptionsWord's parts' 4-bit texels, a byte each, row by row), by SHA-1. */
		constexpr std::string_view UsaOptionsLetters = "06e75f48f49d0011a603286a0a78788cc98e3b09";
		/**
		 * A translation's own label that names an options screen: a frame's first parts (all of them where Parts is 0),
		 * known by their texels (PartTexels) by SHA-1.
		 */
		struct FTranslatedLabel
		{
			int32 Frame = 0;
			std::size_t Parts = 0;
			std::string_view Letters;
		};

		/**
		 * The Spanish translation's Gear screen's "Opcion" (its Gear Option), and the Italian's "Opzioni" (the first three
		 * parts of its "Opzioni Gear"), on both discs.
		 */
		constexpr FTranslatedLabel TranslatedOptionsLabels[] = {
			{GearOptionLabel, 0, "346c1adf2c8674c76ebea0256d444249ba09f52b"},
			{GearOptionLabel, 3, "415d047f5f8e40dc30addd074c5995bae6c7cbae"},
		};
		/** The typed word's cell from the O's place: its outline a pixel up and left of the body. */
		constexpr int32 TypedWordX = -1;
		constexpr int32 TypedWordY = -1;
		/** The title's File row (its Continue's) and its Load, the third command up the inner arc from its first place. */
		constexpr std::size_t TitleFileRow = 8;
		constexpr std::size_t TitleLoad = 2;
		constexpr std::size_t InnerArcPlace = 7;

		FMenuSpritePart Placed(FMenuSpritePart Part, int32 X, int32 Y)
		{
			Part.X = static_cast<int16>(X);
			Part.Y = static_cast<int16>(Y);
			return Part;
		}

		/** "Options" with its O at (X, Y) from the frame's origin; none where the sheet lacks the pieces. */
		std::vector<FMenuSpritePart> OptionsWord(const FMenuSpriteSheet& Sheet, int32 X, int32 Y)
		{
			const std::span<const FMenuSpritePart> GearOption = Sheet.Frame(GearOptionLabel);
			const std::span<const FMenuSpritePart> Status = Sheet.Frame(StatusLabel);
			if (GearOption.size() < 3 || Status.empty())
			{
				return {};
			}
			FMenuSpritePart S = Status.front();
			S.U = static_cast<uint16>(S.U + StatusS);
			S.Width = LetterSWidth;
			return {Placed(GearOption[2], X, Y), Placed(GearOption[0], X + PAfterO, Y + PDrop), Placed(GearOption[1], X + TionAfterO, Y),
				Placed(S, X + SAfterO, Y)};
		}

		/** VRAM's unit at (X, Y) as the archive leaves it, the image last over it; none where it lies in none. */
		std::optional<uint16> ArchiveUnit(std::span<const FVramImage> Archive, int32 X, int32 Y)
		{
			const FVramImage* Holder = nullptr;
			for (const FVramImage& Image : Archive)
			{
				const FVramRect& Rect = Image.Rect;
				if (X >= Rect.X && X < Rect.X + Rect.Width && Y >= Rect.Y && Y < Rect.Y + Rect.Height)
				{
					Holder = &Image;
				}
			}
			if (Holder == nullptr)
			{
				return std::nullopt;
			}
			return Holder->Units[static_cast<std::size_t>((Y - Holder->Rect.Y) * Holder->Rect.Width + X - Holder->Rect.X)];
		}

		/** Parts' 4-bit texels, a byte each, row by row; empty where one lies outside the archive's images. */
		std::vector<uint8> PartTexels(std::span<const FMenuSpritePart> Parts, std::span<const FVramImage> Archive)
		{
			std::vector<uint8> Texels;
			for (const FMenuSpritePart& Part : Parts)
			{
				if (Part.Depth != 0)
				{
					return {};
				}
				const int32 PageX = Part.ImageX & 0xFFC0;
				const int32 PageY = Part.ImageY & 0xFF00;
				for (int32 Row = 0; Row < Part.Height; ++Row)
				{
					for (int32 Column = 0; Column < Part.Width; ++Column)
					{
						const int32 U = Part.U + Column;
						const std::optional<uint16> Unit = ArchiveUnit(Archive, PageX + U / 4, PageY + Part.V + Row);
						if (!Unit)
						{
							return {};
						}
						Texels.push_back(static_cast<uint8>(*Unit >> (4 * (U % 4)) & 0xF));
					}
				}
			}
			return Texels;
		}

		/** A 15-bit colour's brightness, its three channels summed. */
		int32 Brightness(uint16 Colour)
		{
			return (Colour & 31) + (Colour >> 5 & 31) + (Colour >> 10 & 31);
		}

		/**
		 * The colours a typed word takes to look the labels' lettering (FMenuSheetPicture's values 1 to 3): its body the
		 * brightest of the labels' CLUT, its outline the darkest drawn one; white on black where the CLUT is not there.
		 */
		std::array<uint16, 4> LetteringColours(const FMenuSpriteSheet& Sheet, std::span<const FVramImage> Archive)
		{
			uint16 Body = 0xFFFF;
			uint16 Outline = 0x8000;
			const std::span<const FMenuSpritePart> Status = Sheet.Frame(StatusLabel);
			if (!Status.empty())
			{
				int32 Brightest = -1;
				int32 Darkest = 1 << 16;
				for (int32 Entry = 0; Entry < 16; ++Entry)
				{
					const std::optional<uint16> Colour = ArchiveUnit(Archive, Status.front().ClutX + Entry, Status.front().ClutY);
					if (!Colour || *Colour == 0)
					{
						continue;
					}
					if (Brightness(*Colour) > Brightest)
					{
						Brightest = Brightness(*Colour);
						Body = *Colour;
					}
					if (Brightness(*Colour) < Darkest)
					{
						Darkest = Brightness(*Colour);
						Outline = *Colour;
					}
				}
			}
			return {0, Body, Outline, Body};
		}

		std::vector<FMenuSpritePart> Moved(std::span<const FMenuSpritePart> Parts, int32 X, int32 Y)
		{
			std::vector<FMenuSpritePart> Result(Parts.begin(), Parts.end());
			for (FMenuSpritePart& Part : Result)
			{
				Part.X = static_cast<int16>(Part.X + X);
				Part.Y = static_cast<int16>(Part.Y + Y);
			}
			return Result;
		}
	}

	void AddMenuPortFrames(FMenuSpriteSheet& Sheet, const FMenuMainTables& Tables)
	{
		std::vector<FMenuSpritePart> Title = OptionsWord(Sheet, TitleOptionsX, TitleOptionsY);
		std::vector<FMenuSpritePart> Normal = OptionsWord(Sheet, OptionsX, OptionsY);
		std::vector<FMenuSpritePart> Orb = Moved(Sheet.Frame(StatusOrb), OrbStepX, OrbStepY);
		std::vector<FMenuSpritePart> Selected = Moved(Sheet.Frame(StatusOrb + SelectedOrbStep), OrbStepX, OrbStepY);
		Sheet.SetFrame(MenuPortFrame::TitleOptions, std::move(Title));
		Sheet.SetFrame(MenuPortFrame::OptionsOrb, std::move(Orb));
		Sheet.SetFrame(MenuPortFrame::Options, std::move(Normal));
		Sheet.SetFrame(MenuPortFrame::SelectedOptionsOrb, std::move(Selected));
		for (std::size_t Command = 0; Command < Tables.TitleCommands.size(); ++Command)
		{
			const FMenuArcCommand& Shown = Tables.TitleCommands[Command];
			const int32 StepX = Tables.ArcX[Command + 1] - Tables.ArcX[Command];
			const int32 StepY = Tables.ArcY[Command + 1] - Tables.ArcY[Command];
			const int32 Raised = static_cast<int32>(Command);
			Sheet.SetFrame(MenuPortFrame::RaisedTitleOrbs + Raised, Moved(Sheet.Frame(Shown.Orb), StepX, StepY));
			Sheet.SetFrame(MenuPortFrame::RaisedTitleOrbs + Raised + SelectedOrbStep, Moved(Sheet.Frame(Shown.Orb + SelectedOrbStep), StepX, StepY));
			Sheet.SetFrame(MenuPortFrame::RaisedTitleLabels + Raised, Moved(Sheet.Frame(Shown.Label), StepX, StepY));
		}
		Sheet.SetFrame(MenuPortFrame::RaisedTitleOptions,
			OptionsWord(Sheet, TitleOptionsX + Tables.ArcX[1] - Tables.ArcX[0], TitleOptionsY + Tables.ArcY[1] - Tables.ArcY[0]));
		const FMenuArcCommand& Load = Tables.SubCommands[TitleFileRow][TitleLoad];
		const std::size_t LoadPlace = InnerArcPlace + TitleLoad;
		const int32 StepX = Tables.ArcX[LoadPlace + 1] - Tables.ArcX[LoadPlace];
		const int32 StepY = Tables.ArcY[LoadPlace + 1] - Tables.ArcY[LoadPlace];
		Sheet.SetFrame(MenuPortFrame::TopLoadOrb, Moved(Sheet.Frame(Load.Orb), StepX, StepY));
		Sheet.SetFrame(MenuPortFrame::TopLoadOrb + SelectedOrbStep, Moved(Sheet.Frame(Load.Orb + SelectedOrbStep), StepX, StepY));
		Sheet.SetFrame(MenuPortFrame::TopLoad, Moved(Sheet.Frame(Load.Label), StepX, StepY));
	}

	bool HasUsaOptionsLetters(const FMenuSpriteSheet& Sheet, std::span<const FVramImage> Archive)
	{
		const std::vector<uint8> Texels = PartTexels(OptionsWord(Sheet, 0, 0), Archive);
		return !Texels.empty() && FSha1::ToHex(FSha1::Hash(Texels)) == UsaOptionsLetters;
	}

	bool UseTranslatedOptionsLabel(FMenuSpriteSheet& Sheet, const FMenuMainTables& Tables, std::span<const FVramImage> Archive)
	{
		for (const FTranslatedLabel& Known : TranslatedOptionsLabels)
		{
			std::span<const FMenuSpritePart> Frame = Sheet.Frame(Known.Frame);
			if (Known.Parts != 0)
			{
				if (Frame.size() < Known.Parts)
				{
					continue;
				}
				Frame = Frame.first(Known.Parts);
			}
			const std::vector<uint8> Texels = PartTexels(Frame, Archive);
			if (Texels.empty() || FSha1::ToHex(FSha1::Hash(Texels)) != Known.Letters)
			{
				continue;
			}
			// Its top left where the O of "Options" stands.
			const std::vector<FMenuSpritePart> Label(Frame.begin(), Frame.end());
			int32 Left = Label.front().X;
			int32 Top = Label.front().Y;
			for (const FMenuSpritePart& Part : Label)
			{
				Left = std::min<int32>(Left, Part.X);
				Top = std::min<int32>(Top, Part.Y);
			}
			const auto At = [&Label, Left, Top](int32 X, int32 Y) { return Moved(Label, X - Left, Y - Top); };
			Sheet.SetFrame(MenuPortFrame::TitleOptions, At(TitleOptionsX, TitleOptionsY));
			Sheet.SetFrame(MenuPortFrame::Options, At(OptionsX, OptionsY));
			Sheet.SetFrame(MenuPortFrame::RaisedTitleOptions, At(TitleOptionsX + Tables.ArcX[1] - Tables.ArcX[0], TitleOptionsY + Tables.ArcY[1] - Tables.ArcY[0]));
			return true;
		}
		return false;
	}

	void TypeMenuPortWords(FMenuSpriteSheet& Sheet, const FMenuMainTables& Tables, const FMesFont& Font, std::span<const FVramImage> Archive)
	{
		FMenuSheetPicture Word = MenuLetteredWord(Font, LetteringColours(Sheet, Archive), "Options");
		FMenuSpritePart Part;
		Part.Width = static_cast<int16>(Word.Width);
		Part.Height = static_cast<int16>(Word.Height);
		Part.Picture = Sheet.AddPicture(std::move(Word));
		const auto At = [&Part](int32 X, int32 Y)
		{
			return std::vector<FMenuSpritePart>{Placed(Part, X + TypedWordX, Y + TypedWordY)};
		};
		Sheet.SetFrame(MenuPortFrame::TitleOptions, At(TitleOptionsX, TitleOptionsY));
		Sheet.SetFrame(MenuPortFrame::Options, At(OptionsX, OptionsY));
		Sheet.SetFrame(MenuPortFrame::RaisedTitleOptions, At(TitleOptionsX + Tables.ArcX[1] - Tables.ArcX[0], TitleOptionsY + Tables.ArcY[1] - Tables.ArcY[0]));
	}
}
