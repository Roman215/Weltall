#include "Field/FieldPresenter.h"

#include "Game/GameState.h"

#include "Field/FieldMap.h"
#include "Field/FieldMoonGlow.h"
#include "Field/FieldRuntime.h"
#include "Field/FieldScenePictures.h"
#include "Field/FieldScreenLayer.h"
#include "Field/FieldSprites.h"
#include "Field/FieldTextureRegions.h"
#include "Formats/Tim.h"
#include "Field/FieldRenderState.h"
#include "Field/FieldVramChanges.h"
#include "Graphics/ClutFade.h"
#include "Graphics/Mdec.h"
#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Graphics/SpriteSystem.h"
#include "Media/GameFileIndex.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		constexpr int32 FieldDirectory = 4;
		constexpr int32 SystemImagesFile = 0xA7;
		/** Where every map's entry puts the message font's CLUTs. */
		constexpr int16 TextClutX = 256;
		constexpr int16 TextClutY = 240;
		/** A place's CLUT row when the TIM's CLUT is not placed. */
		constexpr int16 ClutNotPlaced = -1;
		/** The characters' drawn things are keyed by actor: its sprite's passes (its shadow is the last key). */
		constexpr uint32 KeyShift = 3;
		constexpr uint32 ActorKeyMask = ~((1u << KeyShift) - 1);
		/** The rows between the field's two draw buffers. */
		constexpr int16 FieldBufferHeight = 0x100;
		constexpr uint8 FullUnitBrightness = 128;

		/** A 15-bit unit drawn modulated by a brightness, as the GPU modulates a texel: each channel times it / 128, at most 31. */
		uint16 ModulateUnit(uint16 Unit, uint8 Brightness)
		{
			uint16 Result = static_cast<uint16>(Unit & 0x8000);
			for (int32 Shift = 0; Shift < 15; Shift += 5)
			{
				const int32 Channel = std::min(31, (Unit >> Shift & 31) * Brightness >> 7);
				Result = static_cast<uint16>(Result | Channel << Shift);
			}
			return Result;
		}
		/** Where the system images leave the compass's colours, which the palette each tick loads there repeats. */
		constexpr FVramRect CompassColorsPlace{0, 0xFB, 16, 1};

		std::array<int32, 3> Whole(const std::array<int32, 3>& Point)
		{
			return {Point[0] >> 16, Point[1] >> 16, Point[2] >> 16};
		}

		std::array<int32, 3> Fixed(const std::array<int32, 3>& Point)
		{
			return {Point[0] * 0x10000, Point[1] * 0x10000, Point[2] * 0x10000};
		}

		std::array<float, 3> ToFloat(const std::array<int32, 3>& Vector)
		{
			return {static_cast<float>(Vector[0]), static_cast<float>(Vector[1]), static_cast<float>(Vector[2])};
		}
	}

	FFieldPresenter::FFieldPresenter(const FFieldDrawingTables& InTables, const FCdImage& InImage, const FGameFileIndex& InIndex)
		: DrawingTables(InTables), Image(&InImage), Index(&InIndex), Members(std::make_unique<FFieldMemberStandIns>(InImage, InIndex))
	{
	}

	TResult<FFieldPresenter> FFieldPresenter::Start(const FFieldDrawingTables& Tables, const FCdImage& Image, const FGameFileIndex& Index)
	{
		FFieldPresenter Presenter(Tables, Image, Index);
		Presenter.FieldVram.TrackWrites();
		const FIndexRecord* Record = Index.Find(FieldDirectory, SystemImagesFile);
		if (Record == nullptr)
		{
			return MakeError("the disc lacks the field's system images");
		}
		const TResult<std::vector<uint8>> Archive = ReadFileSectors(Image, *Record);
		if (!Archive)
		{
			return Archive.Error();
		}
		const std::span<const uint8> Bytes = *Archive;
		if (Bytes.size() < 4 + 4 * FFieldDrawingTables::SystemImageCount)
		{
			return MakeError("the field's system images are cut short");
		}
		for (int32 Entry = 0; Entry < FFieldDrawingTables::SystemImageCount; ++Entry)
		{
			const uint32 Offset = LittleEndian::ReadU32(Bytes, 4 + 4 * static_cast<std::size_t>(Entry));
			if (Offset >= Bytes.size())
			{
				return MakeError("a field system image lies past its file");
			}
			const TResult<FTim> Tim = ParseTim(Bytes.subspan(Offset));
			if (!Tim)
			{
				return MakeError("field system image " + std::to_string(Entry) + ": " + Tim.Error().Message);
			}
			if (auto Error = Presenter.PlaceTim(*Tim, Tables.SystemImages[static_cast<std::size_t>(Entry)]))
			{
				return *Error;
			}
		}
		// The compass's colours, which its palette repeats for each direction.
		Presenter.ReadVram(CompassColorsPlace, Presenter.Scratch);
		std::copy_n(Presenter.Scratch.Units.begin(), Presenter.CompassColors.size(), Presenter.CompassColors.begin());
		return Presenter;
	}

	std::optional<FError> FFieldPresenter::PlaceTim(const FTim& Tim, const FFieldImagePlace& Place)
	{
		// As the field places a TIM: its CLUT first, only as much of it as the place asks for.
		if (Tim.Clut && Place.ClutY != ClutNotPlaced)
		{
			const FVramRect& Own = Tim.Clut->Rect;
			Scratch.Rect = {Place.ClutX, Place.ClutY, Place.ClutWidth != 0 ? Place.ClutWidth : Own.Width, Place.ClutHeight != 0 ? Place.ClutHeight : Own.Height};
			const std::size_t Units = static_cast<std::size_t>(Scratch.Rect.Width) * static_cast<std::size_t>(Scratch.Rect.Height);
			if (Units > Tim.Clut->Units.size())
			{
				return MakeError("a field image's CLUT is smaller than its place");
			}
			Scratch.Units.assign(Tim.Clut->Units.begin(), Tim.Clut->Units.begin() + static_cast<std::ptrdiff_t>(Units));
			FieldVram.Upload(Scratch);
		}
		// The image keeps what kind of picture it is; the scratch image that other uploads reuse does not.
		FVramImage Picture = Tim.Image;
		Picture.Rect.X = Place.X;
		Picture.Rect.Y = Place.Y;
		FieldVram.Upload(Picture);
		return std::nullopt;
	}

	TResult<FTim> FFieldPresenter::ReadTimFile(int32 File) const
	{
		const FIndexRecord* Record = Index->Find(FieldDirectory, File);
		if (Record == nullptr)
		{
			return MakeError("the disc lacks field image file " + std::to_string(File));
		}
		const TResult<std::vector<uint8>> Bytes = ReadFileSectors(*Image, *Record);
		if (!Bytes)
		{
			return Bytes.Error();
		}
		TResult<FTim> Tim = ParseTim(*Bytes);
		if (!Tim)
		{
			return MakeError("field image file " + std::to_string(File) + ": " + Tim.Error().Message);
		}
		return Tim;
	}

	std::optional<FError> FFieldPresenter::LoadImage(int32 File, const FFieldImagePlace& Place)
	{
		const TResult<FTim> Tim = ReadTimFile(File);
		if (!Tim)
		{
			return Tim.Error();
		}
		return PlaceTim(*Tim, Place);
	}

	std::optional<FError> FFieldPresenter::EnterMap(FFieldRuntime& Runtime)
	{
		FEventContext& Context = Runtime.Context();
		const FVram::FKindScope SceneryKind(FieldVram, EVramUploadKind::Scenery);
		{
			const FVram::FKindScope TextKind(FieldVram, EVramUploadKind::Text);
			Scratch.Rect = {TextClutX, TextClutY, static_cast<int16>(DrawingTables.TextClut.size()), 1};
			Scratch.Units.assign(DrawingTables.TextClut.begin(), DrawingTables.TextClut.end());
			FieldVram.Upload(Scratch);
		}
		// The archive's images go in as the load reads the archive, before the scripts' init; the init's VRAM changes
		// follow, then the gears, which load at the end of the map's load (over any slot's images where they lie), and
		// the texture stream comes in after them.
		UploadFieldArchiveImages(Runtime.Map(), FieldVram);
		ApplyVramOperations(Context.Render, Context.Render.VramOperations);
		ApplyGearVramChanges(Context);
		UploadMapTextureStream(Runtime);
		FlushSpriteUploads(Context);
		// The picture of the screen moved once the textures are in.
		ApplyVramOperations(Context.Render, Context.Render.PictureMoves);
		TResult<FFieldScene> Scene = BuildFieldScene(Runtime.Map());
		if (!Scene)
		{
			return MakeError("the map's scenery cannot be built: " + Scene.Error().Message);
		}
		if (Context.Options.bScenePictures)
		{
			MarkScenePictures(Context.RunningMap, *Scene);
		}
		MapScene = std::move(*Scene);
		MapSpriteSheets = FieldSpriteSheetRegions(Runtime.Map());
		MapLoadsSeen = Runtime.MapLoads();
		GearLoadsSeen = Context.Gears.Loads;
		TextureStreamsSeen = Context.Render.TextureStreams;
		Ticks = 0;
		if (StandIns)
		{
			StandIns->Clear();
		}
		if (GearModels)
		{
			GearModels->Clear();
		}
		Members->Clear();
		CutsceneWatch.Reset();
		PartyWatch.Reset();
		return std::nullopt;
	}

	void FFieldPresenter::UploadMapTextureStream(FFieldRuntime& Runtime)
	{
		const FEventContext& Context = Runtime.Context();
		UploadFieldTextureStream(Runtime.Map(), FieldVram, Context.Movement.ActionMode != 0);
		if (Context.Options.bMoonGlowFades)
		{
			FadeMoonGlow(Context.RunningMap, FieldVram);
		}
	}

	void FFieldPresenter::FollowTextureStreams(FFieldRuntime& Runtime)
	{
		if (Runtime.Context().Render.TextureStreams != TextureStreamsSeen)
		{
			UploadMapTextureStream(Runtime);
			TextureStreamsSeen = Runtime.Context().Render.TextureStreams;
		}
	}

	void FFieldPresenter::SetPilotStandIns(bool bOn)
	{
		if (!bOn)
		{
			StandIns.reset();
		}
		else if (!StandIns)
		{
			StandIns = std::make_unique<FFieldPilotStandIns>(*Image, *Index);
		}
	}

	void FFieldPresenter::SetGearStandIns(EFieldGearModels Where)
	{
		if (Where == EFieldGearModels::Off)
		{
			GearModels.reset();
			EffectLooks.fill(EEffectLook::Ended);
			EffectShifts.fill({});
		}
		else if (!GearModels)
		{
			GearModels = std::make_unique<FFieldGearStandIns>(*Image, *Index);
		}
		bGearModelsInCutscenes = Where == EFieldGearModels::Everywhere;
	}

	std::optional<FError> FFieldPresenter::Tick(FFieldRuntime& Runtime, std::span<const FPadState> HeldPerVblank, EFieldCulling Culling, FFieldFrame& Frame,
		FFieldPlayerCamera* Player)
	{
		if (!MapScene)
		{
			return MakeError("the field's drawing has no map");
		}
		Frame.Characters.clear();
		Frame.Quads.clear();
		SceneBeforeTick.reset();
		// The map's first ticks draw nothing but the screen's 2D layer, over black.
		const bool bSceneDrawn = Runtime.Context().SettleTicks == 0;
		const int32 PicturesTaken = Runtime.Context().Render.Cover.Taken;
		const FVram::FKindScope SceneryKind(FieldVram, EVramUploadKind::Scenery);
		// Gears loaded again between ticks (after a movie) place their textures before the tick's changes, and the
		// map's textures stream in after them.
		if (Runtime.Context().Gears.Loads != GearLoadsSeen)
		{
			ApplyGearVramChanges(Runtime.Context());
			GearLoadsSeen = Runtime.Context().Gears.Loads;
		}
		FollowTextureStreams(Runtime);
		Drawn = &Frame;
		TickingRuntime = &Runtime;
		SpriteModelCapture.clear();
		SpriteModelError.reset();
		if (GearModels)
		{
			GearModels->BeginTick();
		}
		bCutsceneWatched = false;
		std::optional<FError> Error = Runtime.Tick(HeldPerVblank, this);
		Drawn = nullptr;
		TickingRuntime = nullptr;
		if (Error)
		{
			return Error;
		}
		if (SpriteModelError)
		{
			return SpriteModelError;
		}
		FEventContext& Context = Runtime.Context();
		const bool bNewMap = Runtime.MapLoads() != MapLoadsSeen;
		// A picture taken of the screen is the frame shown last, drawn from VRAM as it stood.
		bTookPicture = Context.Render.Cover.Taken != PicturesTaken;
		if (bTookPicture)
		{
			VramBeforeTick = FieldVram;
		}
		// A frame that runs no tick (the pause, the fades, the menu's shading) shows the last tick's characters still.
		if (Runtime.LastFrame() == EFieldFrame::Tick)
		{
			HeldCharacters.assign(Frame.Characters.begin(), Frame.Characters.end());
			HeldQuads.assign(Frame.Quads.begin(), Frame.Quads.end());
		}
		else
		{
			Frame.Characters.assign(HeldCharacters.begin(), HeldCharacters.end());
			Frame.Quads.assign(HeldQuads.begin(), HeldQuads.end());
		}
		if (bNewMap)
		{
			// The frame changed maps: VRAM and the scenery as the next map's load leaves them, and none of the
			// last map's characters.
			Frame.Characters.clear();
			Frame.Quads.clear();
			SceneBeforeTick = std::move(MapScene);
			HeldCharacters.clear();
			HeldQuads.clear();
			SpriteModelsShown.clear();
			if (auto LoadError = EnterMap(Runtime))
			{
				return LoadError;
			}
		}
		else
		{
			ApplyVramOperations(Context.Render, Context.Render.VramOperations);
			ApplyGearVramChanges(Context);
			GearLoadsSeen = Context.Gears.Loads;
			FollowTextureStreams(Runtime);
			ApplyVramOperations(Context.Render, Context.Render.PictureMoves);
		}
		// The pause's picture goes into the buffer on display, the one the last tick did not draw into.
		if (Runtime.LastFrame() == EFieldFrame::Paused)
		{
			const FVram::FKindScope HudKind(FieldVram, EVramUploadKind::MenusAndHud);
			Scratch = DrawingTables.PauseImage;
			Scratch.Rect.X = static_cast<int16>(Scratch.Rect.X + FFieldDrawingTables::PauseX);
			Scratch.Rect.Y = static_cast<int16>(Scratch.Rect.Y + FFieldDrawingTables::PauseY + FieldBufferHeight * ((Context.DrawBuffer + 1) & 1));
			FieldVram.Upload(Scratch);
		}
		// Every tick loads, after the scripts' VRAM changes, the compass's palette, the directions the camera may not turn to black.
		if (Runtime.LastFrame() == EFieldFrame::Tick)
		{
			const FVram::FKindScope HudKind(FieldVram, EVramUploadKind::MenusAndHud);
			const auto Palette = FieldCompassPalette(DrawingTables.Compass, CompassColors, Context.Camera.ForbiddenDirections);
			Scratch.Rect = {CompassColorsPlace.X, CompassColorsPlace.Y, static_cast<int16>(Palette.size()), 1};
			Scratch.Units.assign(Palette.begin(), Palette.end());
			FieldVram.Upload(Scratch);
		}
		if (auto PictureError = FollowMapPicture(Context.Render.MapPicture))
		{
			return PictureError;
		}
		if (auto FaceError = LoadFaces(Context))
		{
			return FaceError;
		}
		{
			const FVram::FKindScope TextKind(FieldVram, EVramUploadKind::Text);
			TypeFieldMessageText(Context.Messages, DrawingTables, FieldVram, Scratch);
		}
		FlushSpriteUploads(Context);
		// After the sprites' uploads, as the frame makes them: the scrollers' copies, then the faded CLUT rows.
		ApplyVramOperations(Context.Render, Context.Render.LineScrollMoves);
		if (Context.Render.ClutFade.bLoaded)
		{
			ClutRowsFaded.Rect = Context.Render.ClutFade.Rect;
			FieldVram.Upload(ClutRowsFaded);
		}

		// A map change's fade and Select's picture draw on black, without the field; the menu's shading
		// darkens the field's last picture.
		const EFieldFrame Kind = Runtime.LastFrame();
		const bool bFieldDrawn = bSceneDrawn && (Kind == EFieldFrame::Tick || Kind == EFieldFrame::Paused || Kind == EFieldFrame::MenuShade);
		Frame.Tick = Ticks++;
		Frame.bSceneDrawn = bFieldDrawn;
		Frame.Background = bFieldDrawn ? Context.Render.BackgroundColor : FMdlColor{};
		GameView.View = Context.Camera.View;
		GameView.Eye = Whole(Context.Camera.Eye);
		GameView.Target = Whole(Context.Camera.Target);
		const FFieldPresentedView& Shown = Player != nullptr ? Player->Present(Context, Runtime.LastFrame() == EFieldFrame::Tick, bNewMap) : GameView;
		Frame.View = Shown.View;
		Frame.ShownOffsets = Shown.Offsets;
		Frame.UnroundedView = Shown.Unrounded;
		Frame.bUnrounded = Shown.bUnrounded;
		Frame.Eye = Shown.Eye;
		Frame.Target = Shown.Target;
		CaptureScenery(Context, Culling, Shown, Frame);
		if (!Shown.bGameView)
		{
			MoveCharacters(Context.Camera.View.View, Shown.View.View, Frame);
		}
		// The panorama reads the eye and the target in whole units, which the presented view keeps.
		Frame.Backdrop = bFieldDrawn && Context.Arctangents != nullptr
			? PlaceFieldBackdrop(Context.Render.Backdrop, Context.Render.Panorama.bHidden, Fixed(Shown.Eye), Fixed(Shown.Target), Shown.View,
				*Context.Arctangents)
			: FFieldBackdrop{};
		CaptureShadows(Runtime, Shown.View.View, Frame);
		// The children's models the last tick drew, which the frames that run no tick show still.
		if (Runtime.LastFrame() == EFieldFrame::Tick)
		{
			SpriteModelsShown.swap(SpriteModelCapture);
		}
		EvaluateFieldSpriteModels(SpriteModelsShown, Shown.View, Context.Render.Fog, Shown.FarBucket(Context.Render.FarBucket), Culling, Frame.SpriteModels);
		// The gears first: a model's thrusters go where its pose has put its back.
		if (auto GearError = CaptureGears(Runtime, Culling, Shown, Frame))
		{
			return GearError;
		}
		if (auto ParticleError = CaptureParticles(Context, Shown.View, Frame))
		{
			return ParticleError;
		}
		OrderFrame(Frame);
		WriteOverScreen();
		Frame.ScreenInVram.assign(ScreenInVram.begin(), ScreenInVram.end());
		DrawFieldCompass(Context.Render.Compass, DrawingTables.Compass, Shown.View.Rotation, Shown.View.Pitch, CompassFrame);
		CaptureScreen(Runtime, Shown, Frame);
		return std::nullopt;
	}

	FVramRect FFieldPresenter::UploadMoviePicture(const FMdecPicture& Picture, const FFieldMovieRequest& Movie, uint32 Number)
	{
		const FVram::FKindScope MovieKind(FieldVram, EVramUploadKind::Other);
		const std::size_t Buffer = (Number & 1) * 2;
		const int16 Height = static_cast<int16>(std::min<int32>(Picture.Height, std::max<int32>(Movie.Height, 0)));
		Scratch.Rect = {Movie.Buffers[Buffer], Movie.Buffers[Buffer + 1], static_cast<int16>(Picture.Width), Height};
		Scratch.Units.resize(static_cast<std::size_t>(Picture.Width) * static_cast<std::size_t>(Height));
		ToRgb15(Picture, Scratch.Units);
		FieldVram.Upload(Scratch);
		return Scratch.Rect;
	}

	void FFieldPresenter::ApplyVramOperations(const FFieldRenderState& Render, std::span<const FFieldVramOperation> Operations)
	{
		for (const FFieldVramOperation& Operation : Operations)
		{
			ApplyVramOperation(Render, Operation);
		}
	}

	void FFieldPresenter::ApplyVramOperation(const FFieldRenderState& Render, const FFieldVramOperation& Operation)
	{
		const FVramRect& Rect = Operation.Rect;
		const std::size_t Count = static_cast<std::size_t>(std::max(0, Rect.Width * Rect.Height));
		switch (Operation.Kind)
		{
		case EFieldVramChange::Copy:
		{
			// The screen's pixels the copy carries go where it writes, once its write has gone over what was there.
			WriteOverScreen();
			ScreenCarried.clear();
			CarryScreenInVram(ScreenInVram, Operation, Render.Cover, ScreenCarried);
			ReadVram(Rect, Scratch);
			if (Operation.Brightness != FullUnitBrightness)
			{
				for (uint16& Unit : Scratch.Units)
				{
					Unit = ModulateUnit(Unit, Operation.Brightness);
				}
			}
			Scratch.Rect = {Operation.ToX, Operation.ToY, Rect.Width, Rect.Height};
			FieldVram.UploadCopy(Scratch, Rect);
			WriteOverScreen();
			ScreenInVram.insert(ScreenInVram.end(), ScreenCarried.begin(), ScreenCarried.end());
			break;
		}
		case EFieldVramChange::Clear:
		{
			// The library holds the rectangle within VRAM: the width to 0-1023, the height to 0-511.
			constexpr int16 WidestClear = 1023;
			constexpr int16 TallestClear = 511;
			Scratch.Rect = {Rect.X, Rect.Y, std::clamp<int16>(Rect.Width, 0, WidestClear), std::clamp<int16>(Rect.Height, 0, TallestClear)};
			Scratch.Units.assign(static_cast<std::size_t>(Scratch.Rect.Width * Scratch.Rect.Height), 0);
			FieldVram.UploadFill(Scratch);
			break;
		}
		case EFieldVramChange::Upload:
		{
			const std::size_t At = std::min<std::size_t>(Operation.UnitsAt, Render.VramUnits.size());
			const std::size_t Available = std::min(Count, Render.VramUnits.size() - At);
			Scratch.Rect = Rect;
			Scratch.Units.assign(Render.VramUnits.begin() + static_cast<std::ptrdiff_t>(At),
				Render.VramUnits.begin() + static_cast<std::ptrdiff_t>(At + Available));
			Scratch.Units.resize(Count, 0);
			FieldVram.Upload(Scratch);
			break;
		}
		case EFieldVramChange::SaveArea:
			ReadVram(Rect, SavedArea);
			FieldVram.NoteReadback(Rect);
			break;
		case EFieldVramChange::RestoreArea:
			FieldVram.Upload(SavedArea);
			break;
		case EFieldVramChange::SaveClutRows:
			ReadVram(Rect, ClutRowsKept);
			FieldVram.NoteReadback(Rect);
			ClutRowsFaded = ClutRowsKept;
			break;
		case EFieldVramChange::FadeClutRow:
		{
			const std::size_t RowUnits = static_cast<std::size_t>(std::max<int32>(0, ClutRowsKept.Rect.Width));
			const std::size_t From = static_cast<std::size_t>(std::max(0, Operation.Row)) * RowUnits;
			if (From + RowUnits <= ClutRowsKept.Units.size())
			{
				FadeClutColors(std::span<const uint16>(ClutRowsKept.Units).subspan(From, RowUnits), Operation.Factor,
					std::span<uint16>(ClutRowsFaded.Units).subspan(From, RowUnits));
			}
			break;
		}
		}
	}

	void FFieldPresenter::WriteOverScreen()
	{
		for (const FVramRect& Written : FieldVram.TakeWrites())
		{
			WriteOverScreenInVram(ScreenInVram, Written);
		}
	}

	void FFieldPresenter::ReadVram(const FVramRect& Rect, FVramImage& Into) const
	{
		Into.Rect = Rect;
		Into.Units.resize(static_cast<std::size_t>(std::max(0, Rect.Width * Rect.Height)));
		for (int32 Y = 0; Y < Rect.Height; ++Y)
		{
			for (int32 X = 0; X < Rect.Width; ++X)
			{
				Into.Units[static_cast<std::size_t>(Y * Rect.Width + X)] = FieldVram.At(Rect.X + X, Rect.Y + Y);
			}
		}
	}

	void FFieldPresenter::FlushSpriteUploads(const FEventContext& Context)
	{
		const FVram::FKindScope CharactersKind(FieldVram, EVramUploadKind::Characters);
		Context.Models->SpriteSystem()->FlushUploads(FieldVram);
	}

	std::optional<FError> FFieldPresenter::LoadFaces(const FEventContext& Context)
	{
		const FVram::FKindScope PortraitsKind(FieldVram, EVramUploadKind::Portraits);
		// A slot's faces go into VRAM each time the disc has read them, as the messages' faces cache has it: a face read
		// again after the cache was emptied covers whatever the scripts loaded over it meanwhile.
		for (std::size_t Slot = 0; Slot < FacesLoaded.size(); ++Slot)
		{
			const FFieldPortraitSlot& Cached = Context.Messages.Portraits[Slot];
			const bool bRead = Cached.Load == FFieldPortraitSlot::Read || Cached.Load == FFieldPortraitSlot::Ready;
			if (!bRead || Cached.Character == FFieldPortraitSlot::None || FacesLoaded[Slot] == Cached.ReadNumber)
			{
				continue;
			}
			const std::optional<std::array<int32, 2>> Files = DrawingTables.FaceFiles(Cached.Character);
			if (!Files)
			{
				return MakeError("portrait " + std::to_string(Cached.Character) + " has no faces' files");
			}
			for (std::size_t Face = 0; Face < 2; ++Face)
			{
				if (auto Error = LoadImage((*Files)[Face], DrawingTables.FacePlaces[2 * Slot + Face]))
				{
					return Error;
				}
			}
			FacesLoaded[Slot] = Cached.ReadNumber;
		}
		return std::nullopt;
	}

	void FFieldPresenter::DrawSprite(int32 Actor, const FSprite& Sprite, const FFieldSpritePass& Pass)
	{
		if (Drawn == nullptr)
		{
			return;
		}
		const FEventContext* Context = TickingRuntime != nullptr ? &TickingRuntime->Context() : nullptr;
		if (Context != nullptr && !bCutsceneWatched)
		{
			// Once a tick, as its events left the field, before its first sprite is drawn: whether a cutscene plays and
			// whether the party walks as one, and the members its slots show while it does and no cutscene plays.
			CutsceneWatch.Watch(*Context);
			PartyWatch.Watch(*Context);
			bCutsceneWatched = true;
			if (Context->GameState != nullptr)
			{
				Members->Arrange(*Context, Context->GameState->PartyOrder(), Context->GameState->CharacterLooks(),
					!CutsceneWatch.InCutscene() && PartyWatch.Together());
			}
		}
		// The party drawn in its player's order: a party slot's sprite as the member shown in its place.
		const FFieldMemberStandIns::FLook Look =
			Context != nullptr ? Members->Follow(*Context, TickingRuntime->Sprites(), Actor, Sprite, StandIns != nullptr, Ticks, FieldVram) : FFieldMemberStandIns::FLook{};
		const FSprite& Looked = Look.StandIn != nullptr ? *Look.StandIn : Sprite;
		// A gear whose model stands in for its sprite is drawn with the frame's gears; in a cutscene, only when the
		// player has the models there too.
		if (GearModels && TickingRuntime != nullptr && (bGearModelsInCutscenes || !CutsceneWatch.InCutscene())
			&& GearModels->Follow(*TickingRuntime, Actor, Looked, Pass, FieldVram, Look.Gear))
		{
			return;
		}
		const FSprite* Shown = &Looked;
		FFieldSpritePass Used = Look.StandIn != nullptr ? FFieldMemberStandIns::PassFor(Sprite, *Look.StandIn, Pass) : Pass;
		if (StandIns && Context != nullptr)
		{
			const FFieldMemberStandIns::FLookedFile LookedFile = Members->LookedFileFor(*Context, Actor);
			if (const FSprite* StandIn = StandIns->Follow(*Context, TickingRuntime->Sprites(), Actor, Sprite, Ticks, FieldVram, Members->PicturedFor(*Context, Actor),
					LookedFile.File, LookedFile.Picture))
			{
				Shown = StandIn;
				Used = FFieldPilotStandIns::PassFor(Sprite, *StandIn, Pass);
			}
		}
		Parts.clear();
		if (Used.Clip == ESpriteClip::None)
		{
			DrawSpriteParts(*Shown, Used.Matrix, Used.OtIndex, &Parts);
		}
		else
		{
			DrawSpritePartsClipped(*Shown, Used.Matrix, Used.OtIndex, Used.Clip, Used.ClipLine, Parts);
		}
		const uint32 ActorKey = static_cast<uint32>(Actor) << KeyShift;
		uint32 PassNumber = 0;
		if (!Drawn->Characters.empty() && (Drawn->Characters.back().BlendKey & ActorKeyMask) == ActorKey)
		{
			PassNumber = (Drawn->Characters.back().BlendKey & ~ActorKeyMask) + 1;
		}
		FFieldCharacterView& View = Drawn->Characters.emplace_back();
		View.BlendKey = ActorKey | PassNumber;
		View.Origin = ToFloat(Used.Matrix.T);
		View.World = Whole(Sprite.Position);
		View.Clip = Pass.Clip;
		const uint16 Character = static_cast<uint16>(Drawn->Characters.size() - 1);
		for (const FSpriteQuad& Part : Parts)
		{
			Drawn->Quads.push_back({Part, Character});
		}
	}
}
