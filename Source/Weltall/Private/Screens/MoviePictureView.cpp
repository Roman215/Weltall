#include "Screens/MoviePictureView.h"

#include "Media/MoviePlayer.h"
#include "Rendering/MovieRendering.h"
#include "Rendering/SharedRenderResource.h"

/** A displayed frame of a movie's picture, filled again once the rendering thread has let go of it. */
class FMoviePictureFrame final : public IScreenFrame
{
public:
	virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const override
	{
		if (!Picture.IsValid())
		{
			return;
		}
		AddMoviePasses(GraphBuilder, *Picture, Output, Area.ScreenRect(Screen), Screen, Window);
	}

	virtual Weltall::FScreenSize ScreenSize() const override
	{
		return Screen;
	}

	TSharedPtr<FMoviePictureTexture> Picture;
	Weltall::FScreenSize Screen;
	Weltall::FScreenRect Window;
};

void FMoviePictureView::Follow(const Weltall::FMoviePlayer& Player, Weltall::EMovieFilter Filter)
{
	const std::shared_ptr<const Weltall::FMdecPicture> Shown = Player.Picture();
	if (Shown && (Player.PictureNumber() != PictureNumber || Picture->Filter() != Filter))
	{
		PictureNumber = Player.PictureNumber();
		const std::shared_ptr<const Weltall::FMovieSubtitlePicture> Line = Player.Subtitle();
		if (Line != SubtitleLine)
		{
			SubtitleLine = Line;
			Subtitle.Reset();
			if (Line)
			{
				Subtitle = ShareRenderResource(new FMovieSubtitleTexture(*Line, FIntPoint(Shown->Width, Shown->Height)));
			}
		}
		Picture = ShareRenderResource(new FMoviePictureTexture(*Shown, Filter, Subtitle));
	}
}

void FMoviePictureView::Reset()
{
	Picture.Reset();
	PictureNumber = 0;
	SubtitleLine.reset();
	Subtitle.Reset();
}

TSharedRef<const IScreenFrame> FMoviePictureView::Capture(Weltall::FScreenSize Screen, Weltall::FScreenRect Window) const
{
	TSharedRef<FMoviePictureFrame>* Free = Frames.FindByPredicate([](const TSharedRef<FMoviePictureFrame>& Candidate) { return Candidate.IsUnique(); });
	TSharedRef<FMoviePictureFrame> Frame = Free != nullptr ? *Free : Frames.Add_GetRef(MakeShared<FMoviePictureFrame>());
	Frame->Picture = Picture;
	Frame->Screen = Screen;
	Frame->Window = Window;
	return Frame;
}
