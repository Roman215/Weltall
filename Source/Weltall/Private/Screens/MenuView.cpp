#include "MenuView.h"

#include "Gears/GearModels.h"
#include "Gears/GearShopModel.h"
#include "Menu/MenuGearView.h"
#include "Menu/MenuProgram.h"
#include "Menu/MenuResources.h"
#include "Menu/MenuStoreHint.h"
#include "Rendering/DrawListRendering.h"
#include "Rendering/SharedRenderResource.h"
#include "Rendering/VramTexture.h"
#include "Rendering/TextureModSources.h"
#include "WeltallLog.h"

namespace
{
	/** Glyphs that show no binding: without the player's, a menu's button pictures stay the game's and only a frame's own keys are drawn. */
	class FMenuViewNoGlyphs final : public Weltall::IButtonGlyphs
	{
	public:
		virtual Weltall::FButtonGlyph Glyph(Weltall::EPadButton) const override { return {}; }
		virtual uint32 Revision() const override { return 0; }
	};
}

/**
 * A displayed frame of the menu: the picture under it (or the gear shop's gear on black), then its layers
 * over the menu's VRAM, the player's button pictures over theirs.
 */
class FMenuScreenFrame final : public IScreenFrame
{
public:
	FMenuScreenFrame(TSharedRef<FVramTexture> InVram, TSharedRef<FVramTexture> InIconVram) : Vram(MoveTemp(InVram)), IconVram(MoveTemp(InIconVram)) {}

	virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const override
	{
		if (GearMesh.IsValid())
		{
			// The screen's rows cleared to black, the gear drawn on the field's widened screen.
			AddFieldScenePasses(GraphBuilder, *GearMesh, *Vram, GearDraw, Output, Area.Rows(Weltall::MenuScreenSize));
		}
		else if (Background.IsValid())
		{
			Background->AddPasses(GraphBuilder, Output, Area);
		}
		if (Layer.Primitives.empty())
		{
			return;
		}
		const FIntRect ScreenRect = Area.ScreenRect(Weltall::MenuScreenSize);
		// The menu's frame is drawn between two of its frames already: one list, drawn as it is.
		AddIconDrawListPasses(GraphBuilder, *Vram, *IconVram, Layer, Output, ScreenRect, Weltall::MenuScreenSize);
	}

	TSharedPtr<const IScreenFrame> Background;
	Weltall::FDrawList Layer;
	/** The gear shop's gear and the mesh it is drawn from; no mesh in the other modes. */
	TSharedPtr<const FFieldSceneMesh> GearMesh;
	FFieldSceneDraw GearDraw;

private:
	TSharedRef<FVramTexture> Vram;
	TSharedRef<FVramTexture> IconVram;
};

FMenuView::FMenuView(const FFieldRenderOptions& InOptions)
	: Options(InOptions), Vram(ShareRenderResource(new FVramTexture())), IconVram(ShareRenderResource(new FVramTexture()))
{
}

FMenuView::~FMenuView() = default;

void FMenuView::Follow(const Weltall::FMenuProgram& Menu)
{
	const Weltall::FMenuFrame& Snapshot = Menu.Snapshot();
	if (Snapshot.Number == 0)
	{
		return;
	}
	if (Latest >= 0)
	{
		const uint32 LatestNumber = Frames[static_cast<std::size_t>(Latest)].Number;
		if (Snapshot.Number == LatestNumber)
		{
			return;
		}
		// A frame numbered before the latest kept is a new menu's: nothing blends across menus.
		if (Snapshot.Number < LatestNumber)
		{
			Reset();
		}
	}
	const int32 Next = Latest < 0 ? 0 : 1 - Latest;
	Weltall::FMenuFrame& Kept = Frames[static_cast<std::size_t>(Next)];
	Kept.Primitives.assign(Snapshot.Primitives.begin(), Snapshot.Primitives.end());
	Weltall::AddStorePairHint(Snapshot.Stores, Menu.Resources().Sheet(), Kept.Primitives);
	Kept.Layer = Snapshot.Layer;
	Kept.Gear = Snapshot.Gear;
	Kept.Keys = Snapshot.Keys;
	Kept.Number = Snapshot.Number;
	const Weltall::FGearShopModel* Model = Menu.GearModel();
	bGearScene = Model != nullptr;
	if (Model != nullptr)
	{
		FollowGear(*Model, Snapshot, Next);
	}
	bPrevious = Latest >= 0;
	Latest = Next;
	Vram->SetUpscaled(Weltall::UpscalesTextures(Options.Filter, Options.SpriteFilter));
	Vram->Follow(Menu.Vram());
	SourceVram = &Menu.Vram();
	Sheet = &Menu.Resources().Sheet();
}

void FMenuView::FollowGear(const Weltall::FGearShopModel& Model, const Weltall::FMenuFrame& Snapshot, int32 Next)
{
	const Weltall::FGearModels& Models = Model.Models();
	if (!GearMesh.IsValid() || MeshModels != &Models || GearMesh->GearLayout() != Models.Revision)
	{
		Weltall::LayOutFieldGears(Models, GearLayout);
		GearMesh = MakeShared<const FFieldSceneMesh>(NoScenery, GearLayout, Models.Revision);
		MeshModels = &Models;
	}
	Weltall::FFieldFrame& Frame = GearFrames[static_cast<std::size_t>(Next)];
	if (const std::optional<Weltall::FError> Error = Weltall::EvaluateGearShopFrame(Model, Snapshot.Gear, Snapshot.Number, Options.Culling, Frame))
	{
		UE_LOG(LogWeltall, Error, TEXT("The gear shop's gear cannot be drawn: %hs"), Error->Message.c_str());
		Frame.bSceneDrawn = false;
		Frame.Order.clear();
	}
}

void FMenuView::Reset()
{
	Latest = -1;
	bPrevious = false;
	Sheet = nullptr;
	bGearScene = false;
	GearMesh.Reset();
	MeshModels = nullptr;
	SourceVram = nullptr;
}

TSharedPtr<const IScreenFrame> FMenuView::Capture(TSharedPtr<const IScreenFrame> Background, float Alpha) const
{
	if (Latest < 0)
	{
		return nullptr;
	}
	TSharedRef<FMenuScreenFrame>* Free = DisplayFrames.FindByPredicate([](const TSharedRef<FMenuScreenFrame>& Candidate) { return Candidate.IsUnique(); });
	TSharedRef<FMenuScreenFrame> Next = Free != nullptr ? *Free : DisplayFrames.Add_GetRef(MakeShared<FMenuScreenFrame>(Vram, IconVram));
	const std::size_t Now = static_cast<std::size_t>(Latest);
	const Weltall::FMenuFrame* Before = bPrevious ? &Frames[1 - Now] : nullptr;
	Builder.Build(Frames[Now], Before, Alpha, Next->Layer);
	if (Sheet != nullptr && !Builder.Icons.empty())
	{
		static const FMenuViewNoGlyphs NoGlyphs;
		Icons.Apply(Next->Layer, Builder.Icons, Glyphs != nullptr ? *Glyphs : NoGlyphs, *Sheet, Weltall::MenuSheetButtonPicturesOf(bJapaneseCommands),
			Frames[Now].Keys);
		IconVram->Follow(Icons.Pictures());
	}
	if (bGearScene && GearMesh.IsValid())
	{
		// The gear shop draws no picture under it.
		Next->Background = nullptr;
		Next->GearMesh = GearMesh;
		BuildFieldFrameDraw(NoScenery, GearFrames[Now], bPrevious ? &GearFrames[1 - Now] : nullptr, Alpha, *GearMesh, Options, Next->GearDraw);
	}
	else
	{
		Next->Background = MoveTemp(Background);
		Next->GearMesh = nullptr;
	}
	if (SourceVram != nullptr)
	{
		FollowTextureModSources(*Vram, *SourceVram, Next->GearMesh.Get(), {Next->GearMesh.IsValid() ? &Next->GearDraw : nullptr}, {&Next->Layer},
			Next->GearMesh.IsValid() ? std::span<const Weltall::FTexturePictureSeed>(GearFrames[Now].Gears.TextureSeeds) : std::span<const Weltall::FTexturePictureSeed>());
	}
	return Next;
}
