#include "Menu/MenuGearView.h"

#include "Field/FieldGearDrawing.h"
#include "Field/FieldRenderState.h"
#include "Gears/GearShopModel.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The menu's projection distance, which the shop's drawing keeps. */
		constexpr uint16 MenuProjectionDistance = 0x200;

		/** The shop's one light: its direction in the light matrix's first row, its grey in the colour matrix's first column, its ambient. */
		FFieldGearLights ShopLights()
		{
			FFieldGearLights Lights;
			Lights.Words.fill(0);
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				Lights.Words[static_cast<std::size_t>(FFieldGearLights::DirectionWord(0, Axis))] = FGearShopModel::LightDirection[static_cast<std::size_t>(Axis)];
				Lights.Words[static_cast<std::size_t>(FFieldGearLights::ColorWord(0, Axis))] = FGearShopModel::ColorScale;
			}
			Lights.Ambient.fill(FGearShopModel::Ambient);
			return Lights;
		}
	}

	const FFieldGearLights& GearShopLights()
	{
		static const FFieldGearLights Lights = ShopLights();
		return Lights;
	}

	std::optional<FError> EvaluateGearShopFrame(const FGearShopModel& Model, const FMenuGearView& Gear, uint32 Tick, EFieldCulling Culling, FFieldFrame& Frame)
	{
		Frame.Tick = Tick;
		Frame.bSceneDrawn = Gear.bShown;
		Frame.Background = {};
		FGteMatrix Rotation = Gear.Camera;
		Rotation.T = {};
		Frame.View = {};
		Frame.View.View = Gear.Camera;
		Frame.View.Scene = Gear.Camera;
		Frame.View.ScreenRotation = Rotation;
		Frame.View.Billboard = Rotation;
		Frame.View.ProjectionDistance = MenuProjectionDistance;
		Frame.Eye = Gear.Camera.T;
		Frame.Target = Gear.Camera.T;
		if (auto Error = EvaluateFieldGears(Model.Models(), Frame.View, GearShopLights(), GearShopFarBucket, Culling, Gear.bShown, Frame.Gears, GearShopOrderingShift))
		{
			return Error;
		}
		// Far to near; within an entry the last linked first, as the table is drawn.
		const std::vector<FFieldGearLink>& Links = Frame.Gears.Links;
		Frame.Order.clear();
		for (std::size_t Link = 0; Link < Links.size(); ++Link)
		{
			Frame.Order.push_back({static_cast<uint32>(Link), EFieldDrawKind::Gear, Links[Link].Kind});
		}
		std::sort(Frame.Order.begin(), Frame.Order.end(), [&Links](const FFieldDrawItem& A, const FFieldDrawItem& B)
		{
			return Links[A.Index].Bucket != Links[B.Index].Bucket ? Links[A.Index].Bucket > Links[B.Index].Bucket : A.Index > B.Index;
		});
		for (FFieldDrawItem& Item : Frame.Order)
		{
			Item.Index = Links[Item.Index].Index;
		}
		return std::nullopt;
	}
}
