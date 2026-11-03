#include "Field/FieldPresenter.h"

#include "Field/EventContext.h"
#include "Field/FieldGearDrawing.h"
#include "Field/FieldGears.h"
#include "Field/FieldRuntime.h"
#include "Gears/GearModels.h"

namespace Weltall
{
	namespace
	{
		/** The stand-ins' models' revision, spread over the bits of the map's own gears' layout number. */
		constexpr uint32 StandInLayoutSpread = 0x9E3779B1;
	}

	uint32 FFieldPresenter::GearLayout(const FEventContext& Context) const
	{
		const uint32 Revision = GearModels ? GearModels->Revision() : 0;
		return FieldGearLayout(Context.Gears) ^ Revision * StandInLayoutSpread;
	}

	void FFieldPresenter::LayOutGears(const FEventContext& Context, std::vector<FFieldGearLayoutItem>& Items) const
	{
		Items.clear();
		if (Context.Gears.Models)
		{
			AddFieldGearLayout(*Context.Gears.Models, 0, Items);
		}
		if (GearModels)
		{
			for (int32 Module = 0; Module < GearModels->ModuleCount(); ++Module)
			{
				AddFieldGearLayout(GearModels->Module(Module), FFieldGearStandIns::LayoutSlot(Module, 0), Items);
			}
		}
	}

	std::optional<FError> FFieldPresenter::CaptureGears(const FFieldRuntime& Runtime, EFieldCulling Culling, const FFieldPresentedView& Shown, FFieldFrame& Frame)
	{
		const FEventContext& Context = Runtime.Context();
		FFieldGearsFrame& Gears = Frame.Gears;
		const int32 FarBucket = Shown.FarBucket(Context.Render.FarBucket);
		if (!Context.Gears.Models)
		{
			Gears.Items.clear();
			Gears.Objects.clear();
			Gears.Polygons.clear();
			Gears.Cloths.clear();
			Gears.ClothSpaces.clear();
			Gears.Links.clear();
			Gears.TextureSeeds.clear();
		}
		else
		{
			// The field draws its gears only while it counts some: a script's release can leave models loaded.
			const bool bDrawn = Frame.bSceneDrawn && Context.Gears.Count != 0;
			if (auto Error = EvaluateFieldGears(*Context.Gears.Models, Shown.View, Context.Render.GearLights, FarBucket, Culling, bDrawn, Gears, FieldGearOrderingShift,
					&Runtime.LightUnderGears()))
			{
				return Error;
			}
		}
		if (GearModels)
		{
			if (auto Error = GearModels->Pose(Runtime.LastFrame() == EFieldFrame::Tick))
			{
				return Error;
			}
			for (int32 Module = 0; Module < GearModels->ModuleCount(); ++Module)
			{
				if (auto Error = AddFieldGears(GearModels->Module(Module), FFieldGearStandIns::LayoutSlot(Module, 0), GearModels->Tints(Module), Shown.View,
						FFieldGearStandIns::Lights(), FarBucket, Culling, Frame.bSceneDrawn, Gears, FieldGearOrderingShift, &Runtime.LightUnderGears()))
				{
					return Error;
				}
			}
		}
		Gears.Layout = GearLayout(Context);
		return std::nullopt;
	}

	void FFieldPresenter::ApplyGearVramChanges(const FEventContext& Context)
	{
		const FVram::FKindScope ModelsKind(FieldVram, EVramUploadKind::ModelsAndEffects);
		if (!Context.Gears.Models)
		{
			return;
		}
		const FGearModels& Models = *Context.Gears.Models;
		for (const FGearUpload& Upload : Models.Uploads())
		{
			Scratch.Rect = {Upload.X, Upload.Y, Upload.Width, Upload.Height};
			Scratch.Units.assign(Upload.Pixels.begin(), Upload.Pixels.end());
			FieldVram.Upload(Scratch);
		}
		for (const FGearVramMove& Move : Models.VramMoves())
		{
			const FVramRect From{Move.X, Move.Y, Move.Width, Move.Height};
			ReadVram(From, Scratch);
			Scratch.Rect = {Move.ToX, Move.ToY, Move.Width, Move.Height};
			FieldVram.UploadCopy(Scratch, From);
		}
	}
}
