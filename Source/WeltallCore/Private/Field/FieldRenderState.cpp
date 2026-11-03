#include "Field/FieldRenderState.h"

#include "Foundation/ProcessorDivide.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 FullChannel = 0xFF00;

		void StepFade(FFieldFade& Fade, bool bScreenFadedOut)
		{
			if (Fade.Ticks < 1)
			{
				Fade.Ticks = 0;
				const bool bBlack = std::all_of(Fade.Color.begin(), Fade.Color.end(), [](int32 Channel) { return Channel == 0; });
				if (!bScreenFadedOut && bBlack)
				{
					Fade.bActive = false;
				}
				return;
			}
			for (std::size_t Channel = 0; Channel < 3; ++Channel)
			{
				const int32 Next = static_cast<int32>(static_cast<uint32>(Fade.Color[Channel]) + static_cast<uint32>(Fade.Step[Channel]));
				Fade.Color[Channel] = (Next >> 8) > 0xFF ? FullChannel : std::max(Next, 0);
			}
			Fade.Ticks = static_cast<int16>(Fade.Ticks - 1);
		}

		/** The drawing's rule: a layer drawn black goes off, unless the fix keeps one still rising. */
		void SwitchOffIfBlack(FFieldFade& Fade, bool bRisingFromBlack)
		{
			const bool bDrawnBlack = std::all_of(Fade.Color.begin(), Fade.Color.end(), [](int32 Channel) { return (Channel >> 8) == 0; });
			const bool bRising = Fade.Ticks > 0 && std::any_of(Fade.Step.begin(), Fade.Step.end(), [](int32 Step) { return Step > 0; });
			if (bDrawnBlack && !(bRisingFromBlack && bRising))
			{
				Fade.bActive = false;
				Fade.Ticks = 0;
			}
		}
	}

	void FFieldFade::Start(const std::array<int32, 3>& Target, int32 InTicks, uint16 Mode)
	{
		Ticks = static_cast<int16>(InTicks);
		bActive = true;
		BlendMode = Mode;
		for (std::size_t Channel = 0; Channel < 3; ++Channel)
		{
			const uint32 Distance = (static_cast<uint32>(Target[Channel]) << 8) - static_cast<uint32>(Color[Channel]);
			Step[Channel] = ProcessorDivide(static_cast<int32>(Distance), InTicks);
		}
	}

	void FFieldRenderState::StartFadeIn(int32 Ticks)
	{
		if (!bScreenFadedOut)
		{
			return;
		}
		bScreenFadedOut = false;
		FFieldFade& Fade = Fades[0];
		Fade.Color = {FullChannel, FullChannel, FullChannel};
		const int32 Step = ProcessorDivide(-0x10000, Ticks);
		Fade.Step = {Step, Step, Step};
		Fade.bActive = true;
		Fade.Ticks = static_cast<int16>(Ticks);
		Fade.BlendMode = 2;
	}

	void FFieldRenderState::StartFadeOut(int32 Ticks)
	{
		if (bScreenFadedOut)
		{
			return;
		}
		bScreenFadedOut = true;
		FFieldFade& Fade = Fades[0];
		Fade.Color = {};
		const int32 Step = ProcessorDivide(FullChannel, Ticks);
		Fade.Step = {Step, Step, Step};
		Fade.Ticks = static_cast<int16>(Ticks);
		Fade.bActive = true;
		Fade.BlendMode = 2;
	}

	void FFieldRenderState::StartWhiteOut(int32 Ticks)
	{
		FFieldFade& Fade = Fades[0];
		const int32 Step = ProcessorDivide(0x10000, Ticks);
		Fade.Step = {Step, Step, Step};
		Fade.Color = {};
		Fade.Ticks = static_cast<int16>(Ticks + 1);
		Fade.bActive = true;
		Fade.BlendMode = 1;
	}

	void FFieldRenderState::StartWhiteIn(int32 Ticks)
	{
		FFieldFade& Fade = Fades[0];
		const int32 Step = ProcessorDivide(-0x10000, Ticks);
		Fade.Step = {Step, Step, Step};
		Fade.Color = {FullChannel, FullChannel, FullChannel};
		Fade.Ticks = static_cast<int16>(Ticks + 1);
		Fade.bActive = true;
		Fade.BlendMode = 1;
	}

	void FFieldFade::Reset()
	{
		Color = {};
		bActive = false;
		Ticks = 0;
		BlendMode = 2;
	}

	FMdlColor FFieldFade::DrawnColor() const
	{
		return {static_cast<uint8>(static_cast<uint32>(Color[0]) >> 8), static_cast<uint8>(static_cast<uint32>(Color[1]) >> 8),
			static_cast<uint8>(static_cast<uint32>(Color[2]) >> 8)};
	}

	void FFieldRenderState::TickFades(bool bRisingFromBlack)
	{
		for (FFieldFade& Fade : Fades)
		{
			if (Fade.bActive)
			{
				StepFade(Fade, bScreenFadedOut);
			}
		}
		for (FFieldFade& Fade : Fades)
		{
			if (Fade.bActive)
			{
				SwitchOffIfBlack(Fade, bRisingFromBlack);
			}
		}
	}

	void FFieldRenderState::BeginTick()
	{
		VramOperations.clear();
		PictureMoves.clear();
		VramUnits.clear();
		Markers.Placed.clear();
		ClutFade.bLoaded = false;
	}

	void FFieldRenderState::Upload(const FVramImage& Image)
	{
		FFieldVramOperation Operation;
		Operation.Kind = EFieldVramChange::Upload;
		Operation.Rect = Image.Rect;
		Operation.UnitsAt = static_cast<uint32>(VramUnits.size());
		VramUnits.insert(VramUnits.end(), Image.Units.begin(), Image.Units.end());
		VramOperations.push_back(Operation);
		WroteVram(Image.Rect);
	}

	void FFieldRenderState::WroteVram(const FVramRect& Rect)
	{
		const FVramRect& Area = FFieldCover::PictureArea;
		const bool bOverlaps = Rect.Width > 0 && Rect.Height > 0 && Rect.X < Area.X + Area.Width && Area.X < Rect.X + Rect.Width
			&& Rect.Y < Area.Y + Area.Height && Area.Y < Rect.Y + Rect.Height;
		if (bOverlaps)
		{
			Cover.bScreen = false;
		}
	}

	void FFieldRenderState::SaveEffectTextures()
	{
		if (!bEffectTexturesSaved)
		{
			bEffectTexturesSaved = true;
			FFieldVramOperation Operation;
			Operation.Kind = EFieldVramChange::SaveArea;
			Operation.Rect = EffectTextures;
			VramOperations.push_back(Operation);
		}
	}

	void FFieldRenderState::RestoreEffectTextures()
	{
		if (bEffectTexturesSaved)
		{
			bEffectTexturesSaved = false;
			FFieldVramOperation Operation;
			Operation.Kind = EFieldVramChange::RestoreArea;
			Operation.Rect = EffectTextures;
			VramOperations.push_back(Operation);
		}
	}

	void FFieldRenderState::StreamTexturesAgain()
	{
		++TextureStreams;
		bDefeatPicture = false;
	}
}
