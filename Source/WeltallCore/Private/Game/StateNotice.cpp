#include "Game/StateNotice.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr FScreenRect Screen{0, 0, 320, 224};
		/** Clear of the edge a television's overscan hides. */
		constexpr int32 Left = 12;
		constexpr int32 Top = 10;
		constexpr int32 FullStrength = 255;
	}

	int32 FStateNotice::Strength() const
	{
		if (!IsShown())
		{
			return 0;
		}
		if (Age <= HoldSeconds)
		{
			return FullStrength;
		}
		const double Remaining = 1.0 - (Age - HoldSeconds) / FadeSeconds;
		return std::clamp(static_cast<int32>(Remaining * FullStrength), 0, FullStrength);
	}

	void FStateNotice::Build(const FMesFont& Font, FDrawList& Out)
	{
		Out.Primitives.clear();
		const int32 Level = Strength();
		if (Level <= 0)
		{
			return;
		}
		Words.Prepare(Line, Font);
		Words.Add(Left, Top, Level, Screen, Out);
	}
}
