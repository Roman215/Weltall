#include "Field/FieldPresenter.h"

#include "Field/FieldMessages.h"
#include "Field/FieldRuntime.h"
#include "Field/FieldScreenLayer.h"
#include "Formats/Tim.h"

namespace Weltall
{
	void FFieldPresenter::PlaceWindowsForView(const FEventContext& Context, const FFieldPresentedView& Shown)
	{
		for (std::size_t Slot = 0; Slot < WindowShifts.size(); ++Slot)
		{
			const FFieldMessageWindow& Window = Context.Messages.Windows[Slot];
			if (!Window.bOpen)
			{
				bWindowPlaced[Slot] = false;
				WindowShifts[Slot] = {};
				continue;
			}
			if (bWindowPlaced[Slot] && WindowOpenings[Slot] == Window.Openings)
			{
				continue;
			}
			// A window stays where it opened: placed for the view shown as it opens, as the game places it at its speaker.
			bWindowPlaced[Slot] = true;
			WindowOpenings[Slot] = Window.Openings;
			WindowShifts[Slot] = {};
			const FFieldMessageWindow::FHeadPlace& Place = Window.HeadPlace;
			const bool bSpeaker = Window.Speaker >= 0 && static_cast<std::size_t>(Window.Speaker) < Context.Actors.size();
			if (Shown.bGameView || !bSpeaker || (!Place.bX && !Place.bY))
			{
				continue;
			}
			const std::array<int32, 2> Head = FieldMessageSpeakerHead(Context, static_cast<uint32>(Window.Speaker), Shown.View.View, Shown.View.ProjectionDistance);
			const std::array<int32, 2> Placed = PlaceFieldMessageAtHead(Place, Head[0], Head[1]);
			// The window's place is its frame's top left, 8 above where its place's y is.
			constexpr int32 FrameAbovePlace = 8;
			WindowShifts[Slot] = {static_cast<int16>(Place.bX ? Placed[0] - Window.Rect.X : 0),
				static_cast<int16>(Place.bY ? Placed[1] - (Window.Rect.Y + FrameAbovePlace) : 0)};
		}
	}

	void FFieldPresenter::CaptureScreen(const FFieldRuntime& Runtime, const FFieldPresentedView& Shown, FFieldFrame& Frame)
	{
		const FEventContext& Context = Runtime.Context();
		const FFieldRenderState& Render = Context.Render;
		Frame.DrawClip = Render.DrawClip;
		Frame.bScreenWave = Frame.bSceneDrawn && Render.ScreenWave.bDrawn;
		Frame.WideWave.clear();
		if (Frame.bScreenWave)
		{
			Render.ScreenWave.BuildCells(Frame.Wave);
			Frame.WideWave.resize(FFieldScreenWave::WideColumns * FFieldScreenWave::Rows);
			Render.ScreenWave.BuildWideCells(std::span<FFieldScreenWaveCell, FFieldScreenWave::WideColumns * FFieldScreenWave::Rows>(Frame.WideWave));
		}
		const FFieldCover& Cover = Render.Cover;
		FFieldFrameCover& Picture = Frame.Cover;
		Picture.bShown = Cover.bShown;
		Picture.bAdditive = Cover.bAdditive;
		Picture.bScreen = Cover.bScreen;
		// The tick draws the columns of the buffer it draws into.
		Picture.Brightness = Cover.Brightness[static_cast<std::size_t>(Context.DrawBuffer & 1)];
		Picture.Scale = Cover.Scale;
		Picture.Taken = Cover.Taken;
		PlaceWindowsForView(Context, Shown);
		Picture.LayerIndex = BuildFieldScreenLayer(Context, DrawingTables, Runtime.LastFrame(), Frame.Screen, &CompassFrame, &WindowShifts, &Frame.TextButtons);
	}

	std::optional<FError> FFieldPresenter::FollowMapPicture(const FFieldMapPicture& Picture)
	{
		const FVram::FKindScope HudKind(FieldVram, EVramUploadKind::MenusAndHud);
		constexpr FVramRect PictureArea{0x300, 0x100, 0xA0, 0x100};
		constexpr int16 PictureClutY = 0xF6;
		if (Picture.bShown == bMapPictureUp)
		{
			return std::nullopt;
		}
		bMapPictureUp = Picture.bShown;
		if (!Picture.bShown)
		{
			FieldVram.Upload(MapPictureArea);
			return std::nullopt;
		}
		ReadVram(PictureArea, MapPictureArea);
		FieldVram.NoteReadback(PictureArea);
		if (auto Error = LoadImage(Picture.File, {PictureArea.X, PictureArea.Y, 0, PictureClutY, 0, 0}))
		{
			return Error;
		}
		if (Picture.HiddenParts == 0)
		{
			return std::nullopt;
		}
		// The parts the party has not seen, drawn over the picture from the same places of the parts' file.
		TResult<FTim> PartsImage = ReadTimFile(FFieldMapPicture::PartsFile);
		if (!PartsImage)
		{
			return PartsImage.Error();
		}
		const FVramImage& Source = PartsImage->Image;
		for (std::size_t Part = 0; Part < FFieldMapPicture::Parts.size(); ++Part)
		{
			if ((Picture.HiddenParts >> Part & 1) == 0)
			{
				continue;
			}
			// The rectangles are in the 8-bit picture's pixels, two to a unit.
			const FFieldMapPicture::FPart& Rect = FFieldMapPicture::Parts[Part];
			Scratch.Rect = {static_cast<int16>(PictureArea.X + Rect.X / 2), static_cast<int16>(PictureArea.Y + Rect.Y), static_cast<int16>(Rect.Width / 2), Rect.Height};
			Scratch.Units.resize(static_cast<std::size_t>(Scratch.Rect.Width * Scratch.Rect.Height));
			for (int32 Row = 0; Row < Scratch.Rect.Height; ++Row)
			{
				for (int32 Unit = 0; Unit < Scratch.Rect.Width; ++Unit)
				{
					const std::size_t From = static_cast<std::size_t>((Rect.Y + Row) * Source.Rect.Width + Rect.X / 2 + Unit);
					Scratch.Units[static_cast<std::size_t>(Row * Scratch.Rect.Width + Unit)] = From < Source.Units.size() ? Source.Units[From] : 0;
				}
			}
			FieldVram.Upload(Scratch);
		}
		return std::nullopt;
	}
}
