#include "MenuIconSheet.h"

#include "Graphics/Vram.h"
#include "Menu/MenuButtonIcons.h"
#include "Menu/MenuDrawing.h"
#include "Menu/MenuPrimitive.h"
#include "Rendering/DrawListRendering.h"
#include "Rendering/SharedRenderResource.h"
#include "Rendering/VramTexture.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace
{
	/** The icons each set's rows show: its face buttons, shoulders, specials, sticks and D-pad; for the keyboard, keys of each kind of label. */
	const std::vector<std::pair<Weltall::EButtonIconSet, std::vector<const char*>>>& SheetIcons()
	{
		using Weltall::EButtonIconSet;
		static const std::vector<std::pair<EButtonIconSet, std::vector<const char*>>> Icons = {
			{EButtonIconSet::Xbox, {"A", "B", "X", "Y", "LB", "RB", "LT", "RT", "View", "Menu", "LS", "RS", "DPad_Up", "DPad_Left", "LeftStick_Up"}},
			{EButtonIconSet::PlayStation, {"Cross", "Circle", "Square", "Triangle", "L1", "R1", "L2", "R2", "Create", "Options", "L3", "R3", "DPad_Down"}},
			{EButtonIconSet::Nintendo, {"A", "B", "X", "Y", "L", "R", "ZL", "ZR", "Minus", "Plus", "LS", "RS", "DPad_Right"}},
			{EButtonIconSet::SteamDeck, {"A", "B", "X", "Y", "L1", "R1", "L2", "R2", "View", "Menu", "L3", "R3"}},
			{EButtonIconSet::Keyboard, {"E", "Q", "1", "Enter", "Space", "Esc", "Backspace", "Tab", "LeftShift", "Up", "Left", "PageUp", "F12", "MouseLeft"}},
		};
		return Icons;
	}

	constexpr int32 PageSize = 256;

	/** The sheet's frame: its list over the pictures' VRAM, fitted to the display as the menus are. */
	class FMenuIconSheetFrame final : public IScreenFrame
	{
	public:
		FMenuIconSheetFrame(TSharedRef<FVramTexture> InPictures, const Weltall::FDrawList& InList) : Pictures(MoveTemp(InPictures)), List(InList) {}

		virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const override
		{
			const FDrawListFrame Frame{&List, nullptr, nullptr, 1.0f};
			AddDrawListPasses(GraphBuilder, *Pictures, Frame, Output, Area.ScreenRect(Weltall::MenuScreenSize), Weltall::MenuScreenSize);
		}

	private:
		TSharedRef<FVramTexture> Pictures;
		Weltall::FDrawList List;
	};
}

FMenuIconSheetScreen::FMenuIconSheetScreen() : Pictures(ShareRenderResource(new FVramTexture()))
{
	constexpr Weltall::FDrawColor WindowGrey{0x38, 0x38, 0x40};
	List.Primitives.push_back(Weltall::MakeGouraudRect({0, 0, 320, 224}, {WindowGrey, WindowGrey, WindowGrey, WindowGrey}));
	Weltall::FVram Vram;
	int32 Page = 0;
	int32 ShelfX = 0;
	int32 ShelfY = 0;
	int32 ShelfHeight = 0;
	float Top = 4.0f;
	std::vector<std::pair<const char*, Weltall::EMenuIconSlot>> Row;
	for (const auto& [Set, Icons] : SheetIcons())
	{
		// A row of every icon in a face button's slot, one in the hints' box, and for a controller a row of the battle's
		// shapes: its four face buttons as bare symbols, then as clusters.
		for (const Weltall::EMenuIconSlot RowSlot : {Weltall::EMenuIconSlot::FaceButton, Weltall::EMenuIconSlot::Hint, Weltall::EMenuIconSlot::Symbol})
		{
			Row.clear();
			if (RowSlot != Weltall::EMenuIconSlot::Symbol)
			{
				for (const char* Icon : Icons)
				{
					Row.emplace_back(Icon, RowSlot);
				}
			}
			else if (Set != Weltall::EButtonIconSet::Keyboard)
			{
				for (const Weltall::EMenuIconSlot Slot : {Weltall::EMenuIconSlot::Symbol, Weltall::EMenuIconSlot::Cluster})
				{
					for (std::size_t Face = 0; Face < 4; ++Face)
					{
						Row.emplace_back(Icons[Face], Slot);
					}
				}
			}
			if (Row.empty())
			{
				continue;
			}
			float Left = 8.0f;
			float RowHeight = 0.0f;
			for (const auto& [Icon, Slot] : Row)
			{
				Weltall::FMenuIconPicture Picture = Weltall::DrawMenuButtonIcon(Set, Icon, Slot);
				if (ShelfX + Picture.Width > PageSize)
				{
					ShelfY += ShelfHeight;
					ShelfX = 0;
					ShelfHeight = 0;
				}
				if (ShelfY + Picture.Height > PageSize)
				{
					++Page;
					ShelfX = 0;
					ShelfY = 0;
					ShelfHeight = 0;
				}
				const int32 PageX = Page % 4 * PageSize;
				const int32 PageY = Page / 4 * PageSize;
				Weltall::FDrawPrimitive Quad;
				Quad.Texture = Weltall::FDrawTexture{0, Weltall::FTexturePage::FromAttribute(Weltall::MenuGpu::TexturePage(2, 0, PageX, PageY)), {}};
				Quad.bModulate = false;
				const float Width = static_cast<float>(Picture.Width) / Weltall::MenuIconTexelsPerPixel;
				const float Height = static_cast<float>(Picture.Height) / Weltall::MenuIconTexelsPerPixel;
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					const bool bRight = (Corner & 1) != 0;
					const bool bBottom = (Corner & 2) != 0;
					Quad.Corners[Corner].X = Left + (bRight ? Width : 0.0f);
					Quad.Corners[Corner].Y = Top + (bBottom ? Height : 0.0f);
					Quad.Corners[Corner].U = static_cast<float>(ShelfX + (bRight ? Picture.Width : 0));
					Quad.Corners[Corner].V = static_cast<float>(ShelfY + (bBottom ? Picture.Height : 0));
				}
				List.Primitives.push_back(Quad);
				Vram.Upload({{static_cast<int16>(PageX + ShelfX), static_cast<int16>(PageY + ShelfY), static_cast<int16>(Picture.Width),
					static_cast<int16>(Picture.Height)}, std::move(Picture.Texels)});
				ShelfX += static_cast<int32>(Width * Weltall::MenuIconTexelsPerPixel);
				ShelfHeight = std::max(ShelfHeight, static_cast<int32>(Height * Weltall::MenuIconTexelsPerPixel));
				Left += Width + 3.0f;
				RowHeight = std::max(RowHeight, Height);
			}
			Top += RowHeight + 3.0f;
		}
		Top += 1.0f;
	}
	Pictures->Follow(Vram);
}

TSharedRef<const IScreenFrame> FMenuIconSheetScreen::CaptureFrame() const
{
	return MakeShared<FMenuIconSheetFrame>(Pictures, List);
}
