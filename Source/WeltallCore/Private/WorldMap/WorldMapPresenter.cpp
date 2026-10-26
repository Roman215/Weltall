#include "WorldMap/WorldMapPresenter.h"

#include "Field/FieldGearStandIns.h"
#include "Formats/Lzss.h"
#include "Formats/Tim.h"
#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Gears/GearPackets.h"
#include "Graphics/SpriteDrawing.h"
#include "Media/GameFileIndex.h"
#include "Text/MesRender.h"
#include "WorldMap/WorldMapRuntime.h"
#include "WorldMap/WorldMapStateLayout.h"
#include "WorldMap/WorldMapTables.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace Weltall
{
	namespace
	{
		constexpr int32 PaletteColors = 256;
		/** A gear's sprite walks in its animation 1; it stands in its others (parked, it kneels). */
		constexpr int32 GearWalkAnimation = 1;
		/** A task heads along (sin, -cos) of its heading in the world, which the sprites' space mirrors in z: a quarter turn from a field actor's yaw. */
		constexpr int32 HeadingToYaw = -0x400;
		constexpr int32 FullTurn = 0x1000;
		/** A task's place holds 12 bits of fraction. */
		constexpr float TaskUnit = 4096.0f;
		/** The gears' models are tested through the field's drawing: nothing of them lies too far for it. */
		constexpr int32 GearFarBucket = 0xFFFF;
		/** A cloth's anchor among its model's: past any bone's part. */
		constexpr uint32 ClothAnchor = 0xFF;
		/** The models' textures' palette (y 1F0h) makes 16 fog levels, of which the load uploads 15 over it. */
		constexpr int32 ObjectPaletteRow = 0x1F0;
		constexpr int32 ObjectFogLevels = 16;
		constexpr int32 ObjectFogRowsLoaded = 15;
		/** The terrain's two palettes (y 1E0h, 1E1h) make 32 levels each, loaded from y 1B0h, the first palette's first. */
		constexpr int32 TerrainPaletteRow = 0x1E0;
		constexpr int32 TerrainPalettes = 2;
		constexpr int32 TerrainFogLevels = 32;
		constexpr int32 TerrainFogRow = 0x1B0;
		/** Every playable scene starts with exit code 2, for which the display's start sets the fog from this depth. */
		constexpr int32 FogNear = 0xB00;
		/** The waves' phase down a quadrant's rows: the streamer's start sets it and nothing changes it. */
		constexpr int32 WaveRowPhase = 0x400;
		/** A sprite's depth to the ordering-table entry the frame task links it into. */
		constexpr int32 SpriteEntryShift = 4;
		/** A palette animation's image: eight colours. */
		constexpr std::size_t PaletteImageBytes = 16;
		/** The message font's palettes, as the scene's start loads them: the even lines' at (130h, 1E0h), the odd lines' after them. */
		constexpr int16 TextClutX = 0x130;
		constexpr int16 TextClutY = 0x1E0;
		constexpr uint16 EvenTextClut = 0x7813;
		constexpr uint16 OddTextClut = 0x7814;
		/** The windows' text images: 4-bit at x 3C0h, the place name's from y 180h, the notice's after it; their text from (160, 120). */
		constexpr int16 TextVramX = 0x3C0;
		constexpr int16 PlaceNameVramY = 0x180;
		constexpr int16 NoticeVramY = 0x18D;
		constexpr int32 WindowX = 0xA0;
		constexpr int32 WindowY = 0x78;
		/** A strip of the image holds two lines, one in each plane of its 13 texel rows; a line is drawn in halves of 256 pixels. */
		constexpr int16 TextRows = 13;
		constexpr int32 LineSpacing = 14;
		constexpr int32 ImageMargin = 3;
		constexpr int32 HalfUnits = 0x40;
		constexpr int32 HalfPixels = 0x100;
		/** The box behind a window's text: black, averaging, from 7 pixels left of the text and 5 above. */
		constexpr int32 BoxLeft = 7;
		constexpr int32 BoxTop = 5;
		constexpr int32 BoxExtraWidth = 13;
		constexpr int32 BoxExtraHeight = 10;
		/** The notice's backdrop: the 128 x 64 texels at (80h, 0) of the models' 4-bit page at (380h, 100h), averaging, behind its window. */
		constexpr int16 BackdropLeft = 0x98;
		constexpr int16 BackdropTop = 0x70;
		constexpr int16 BackdropRight = 0x128;
		constexpr int16 BackdropBottom = 0xB4;
		constexpr uint8 BackdropU = 0x80;
		constexpr uint8 BackdropLastU = 0xFF;
		constexpr uint8 BackdropLastV = 0x3F;
		constexpr uint16 BackdropPage = 0x1E;
		constexpr uint16 BackdropClut = 0x7913;
		/** The world map's screen. */
		constexpr int16 ScreenWidth = 320;
		constexpr int16 ScreenHeight = 216;
	}

	FWorldMapPresenter::FWorldMapPresenter(const FWorldMapDrawingTables& InTables, const FCdImage& InImage, const FGameFileIndex& InIndex,
		std::span<const uint16, 32> InTextClut)
		: Tables(InTables), Image(&InImage), Index(&InIndex)
	{
		std::copy(InTextClut.begin(), InTextClut.end(), TextClut.begin());
		PlaceNameText.VramY = PlaceNameVramY;
		NoticeText.VramY = NoticeVramY;
	}

	FWorldMapPresenter::~FWorldMapPresenter() = default;

	void FWorldMapPresenter::SetGearStandIns(EFieldGearModels Where)
	{
		if (Where == EFieldGearModels::Off)
		{
			GearModels.reset();
		}
		else if (!GearModels)
		{
			GearModels = std::make_unique<FFieldGearStandIns>(*Image, *Index);
			GearPlaces = {};
		}
		bGearModelsInCutscenes = Where == EFieldGearModels::Everywhere;
	}

	std::optional<FError> FWorldMapPresenter::LoadTextures(int32 File, int32 PaletteRow, int32 PaletteRows, int32 Levels, int32 FogRow, int32 FogRows,
		const std::array<uint8, 3>& Fog)
	{
		const FIndexRecord* Record = Index->Find(WorldMapFiles::Directory, File);
		if (Record == nullptr)
		{
			return MakeError("the disc lacks the world map's texture file " + std::to_string(File));
		}
		const TResult<std::vector<uint8>> Packed = ReadFileSectors(*Image, *Record);
		if (!Packed)
		{
			return Packed.Error();
		}
		const TResult<std::vector<uint8>> Archive = UnpackLzss(*Packed);
		if (!Archive)
		{
			return MakeError("world map texture file " + std::to_string(File) + ": " + Archive.Error().Message);
		}
		const TResult<std::vector<FVramImage>> Images = ReadTimArchive(*Archive);
		if (!Images)
		{
			return MakeError("world map texture file " + std::to_string(File) + ": " + Images.Error().Message);
		}
		for (const FVramImage& Upload : *Images)
		{
			MapVram.Upload(Upload);
		}
		// The palettes as the textures left them, each faded into its levels, then as many rows as the load takes.
		std::vector<uint16> Palette(PaletteColors);
		std::vector<uint16> Faded(static_cast<std::size_t>(PaletteRows * Levels * PaletteColors));
		for (int32 Row = 0; Row < PaletteRows; ++Row)
		{
			for (int32 Color = 0; Color < PaletteColors; ++Color)
			{
				Palette[static_cast<std::size_t>(Color)] = MapVram.At(Color, PaletteRow + Row);
			}
			MakeWorldMapFogPalettes(Palette, Levels, Fog,
				std::span<uint16>(Faded).subspan(static_cast<std::size_t>(Row * Levels * PaletteColors), static_cast<std::size_t>(Levels * PaletteColors)));
		}
		Scratch.Rect = {0, static_cast<int16>(FogRow), PaletteColors, static_cast<int16>(FogRows)};
		Scratch.Units.assign(Faded.begin(), Faded.begin() + static_cast<std::ptrdiff_t>(FogRows * PaletteColors));
		MapVram.Upload(Scratch);
		return std::nullopt;
	}

	std::optional<FError> FWorldMapPresenter::EnterScene(FWorldMapRuntime& Runtime)
	{
		if (GearModels)
		{
			// A new scene: the gears stand afresh where they are next met.
			GearModels->Clear();
			GearPlaces = {};
		}
		const int32 Header = Runtime.Scene().GroupHeader;
		// As the scene's start loads them: the models' textures, then the terrain's over them.
		if (auto Error = LoadTextures(WorldMapFiles::ObjectTextures(Header), ObjectPaletteRow, 1, ObjectFogLevels, ObjectPaletteRow, ObjectFogRowsLoaded,
				Tables.ObjectFogColor))
		{
			return Error;
		}
		if (auto Error = LoadTextures(WorldMapFiles::TerrainTextures(Header), TerrainPaletteRow, TerrainPalettes, TerrainFogLevels, TerrainFogRow,
				TerrainPalettes * TerrainFogLevels, Tables.FogColor))
		{
			return Error;
		}
		TResult<FWorldMapModelPackets> Packets = BuildWorldMapModelPackets(Runtime.Scene(), Tables);
		if (!Packets)
		{
			return Packets.Error();
		}
		ModelPackets = std::move(*Packets);
		if (Runtime.Context().StrayHiddenWrite >= 0)
		{
			if (auto Error = ApplyStrayHiddenWrite(Runtime.Scene(), Runtime.Context().StrayHiddenWrite, ModelPackets))
			{
				return Error;
			}
		}
		if (!Runtime.IsCutscene())
		{
			Scratch.Rect = {TextClutX, TextClutY, static_cast<int16>(TextClut.size()), 1};
			Scratch.Units.assign(TextClut.begin(), TextClut.end());
			MapVram.Upload(Scratch);
		}
		PlaceNameText.Units.clear();
		NoticeText.Units.clear();
		Animations.clear();
		AnimationUploads.clear();
		if (auto Error = StartAnimations(Runtime.Scene().AnimatedTexturesA, Tables.PaletteAnimations, true))
		{
			return Error;
		}
		if (auto Error = StartAnimations(Runtime.Scene().AnimatedTexturesB, Tables.TileAnimations, false))
		{
			return Error;
		}
		// The first frame's billboards: the streamer's window set about the start, and no block culled yet.
		const FWorldMapContext& Context = Runtime.Context();
		BillboardArea.BlockX = Context.Start.X >> 23;
		BillboardArea.BlockZ = Context.Start.Z >> 23;
		BillboardArea.Across = FWorldMapDrawingTables::DrawnBlocksAcross;
		BillboardArea.BlockStates.assign(FWorldMapDrawingTables::DrawnBlocks, 0);
		Runtime.Context().Sprites.FlushUploads(MapVram);
		Scene = &Runtime.Scene();
		Ticks = 0;
		return std::nullopt;
	}

	std::optional<FError> FWorldMapPresenter::Tick(FWorldMapRuntime& Runtime, std::span<const FPadState> HeldPerVblank, EWorldMapCulling Culling,
		FWorldMapFrame& Frame)
	{
		if (Scene == nullptr)
		{
			return MakeError("the world map's drawing has no scene");
		}
		Builder.Begin(Frame);
		Drawn = &Frame;
		DrawnCulling = Culling;
		bGearsBegun = false;
		std::optional<FError> Error = Runtime.Tick(HeldPerVblank, this);
		if (!Error && GearError)
		{
			Error = std::exchange(GearError, std::nullopt);
		}
		// The world is the frame task's last drawing: a frame of the pause draws nothing.
		const bool bWorldDrawn = Drawn == nullptr;
		Drawn = nullptr;
		if (Error)
		{
			return Error;
		}
		const FWorldMapContext& Context = Runtime.Context();
		Runtime.Context().Sprites.FlushUploads(MapVram);
		if (Runtime.PassEnded())
		{
			StepAnimations();
		}
		bDrewFrame = bWorldDrawn;
		if (bWorldDrawn)
		{
			Builder.Finish();
			Frame.Tick = Ticks++;
			Frame.WaverShifts.clear();
			if (Context.Waver)
			{
				Frame.WaverShifts.assign(Context.Waver->Shifts.begin(), Context.Waver->Shifts.end());
			}
			DrawWindows(Context, Frame.Windows);
			DrawScreen(Context, Frame.Screen);
		}
		return std::nullopt;
	}

	std::optional<FError> FWorldMapPresenter::StartAnimations(std::span<const uint8> Section, std::span<const FWorldMapTextureAnimation> Known, bool bPalettes)
	{
		// The section: a count, then each animation's offset from the section's start to its images.
		if (Section.size() < 4)
		{
			return MakeError("a world map scene's texture animations lack their count");
		}
		const uint32 Count = LittleEndian::ReadU32(Section, 0);
		if (Count > Known.size() || Section.size() < 4 + std::size_t{Count} * 4)
		{
			return MakeError("a world map scene animates " + std::to_string(Count) + " textures where the program knows " + std::to_string(Known.size()));
		}
		for (std::size_t Number = 0; Number < Count; ++Number)
		{
			FTextureAnimation& Running = Animations.emplace_back();
			Running.Animation = &Known[Number];
			const FVramRect& Rect = Running.Animation->Rect;
			Running.ImageBytes = bPalettes ? PaletteImageBytes : static_cast<std::size_t>(Rect.Width) * static_cast<std::size_t>(Rect.Height) * 2;
			const std::size_t First = LittleEndian::ReadU32(Section, 4 + Number * 4);
			int32 LastImage = 0;
			for (const FWorldMapAnimationFrame& Frame : Running.Animation->Frames)
			{
				LastImage = std::max<int32>(LastImage, Frame.Image);
			}
			const std::size_t Needed = First + (static_cast<std::size_t>(LastImage) + 1) * Running.ImageBytes;
			if (Running.Animation->Frames.empty() || std::ranges::any_of(Running.Animation->Frames, [](const FWorldMapAnimationFrame& Frame) { return Frame.Image < 0; })
				|| Needed > Section.size())
			{
				return MakeError("a world map texture animation shows images its scene lacks");
			}
			Running.Images = Section.subspan(First, Needed - First);
		}
		return std::nullopt;
	}

	void FWorldMapPresenter::StepAnimations()
	{
		AnimationUploads.clear();
		for (FTextureAnimation& Running : Animations)
		{
			const std::vector<FWorldMapAnimationFrame>& Frames = Running.Animation->Frames;
			if (--Running.PassesLeft != 0)
			{
				continue;
			}
			++Running.Frame;
			Running.PassesLeft = static_cast<uint16>(Frames[Running.Frame].Passes);
			if (Frames[Running.Frame].Passes < 0)
			{
				Running.Frame = 0;
				Running.PassesLeft = static_cast<uint16>(Frames[0].Passes);
			}
			FVramImage& Upload = AnimationUploads.emplace_back();
			Upload.Rect = Running.Animation->Rect;
			Upload.Units.resize(Running.ImageBytes / 2);
			const std::size_t At = static_cast<std::size_t>(Frames[Running.Frame].Image) * Running.ImageBytes;
			for (std::size_t Unit = 0; Unit < Upload.Units.size(); ++Unit)
			{
				Upload.Units[Unit] = LittleEndian::ReadU16(Running.Images, At + Unit * 2);
			}
			MapVram.Upload(Upload);
		}
	}

	void FWorldMapPresenter::DrawWindows(const FWorldMapContext& Context, FDrawList& Windows)
	{
		Windows.Primitives.clear();
		// Linked at the nearest entry by their tasks in slot order, the place name's first: the notice's is drawn first.
		if (Context.bNoticeBackdrop)
		{
			FDrawPrimitive& Backdrop = Windows.Primitives.emplace_back();
			const std::array<std::array<int16, 2>, 4> Corners{
				{{BackdropLeft, BackdropTop}, {BackdropRight, BackdropTop}, {BackdropLeft, BackdropBottom}, {BackdropRight, BackdropBottom}}};
			for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
			{
				FDrawVertex& Vertex = Backdrop.Corners[Corner];
				Vertex.X = static_cast<float>(Corners[Corner][0]);
				Vertex.Y = static_cast<float>(Corners[Corner][1]);
				Vertex.U = static_cast<float>((Corner & 1) != 0 ? BackdropLastU : BackdropU);
				Vertex.V = static_cast<float>((Corner & 2) != 0 ? BackdropLastV : 0);
			}
			Backdrop.Texture = FDrawTexture{0, FTexturePage::FromAttribute(BackdropPage), FClutPosition::FromAttribute(BackdropClut)};
			Backdrop.Blend = EDrawBlend::SemiTransparent;
		}
		DrawWindow(Context, Context.NoticeWindow, NoticeText, Windows);
		DrawWindow(Context, Context.PlaceNameWindow, PlaceNameText, Windows);
	}

	void FWorldMapPresenter::DrawWindow(const FWorldMapContext& Context, const FMesWindow& Window, FWindowText& Text, FDrawList& Windows)
	{
		if (!Window.Drew())
		{
			return;
		}
		if (Window.HasBox())
		{
			const FDrawColor Black{0, 0, 0};
			const FScreenRect Rect{static_cast<int16>(WindowX - BoxLeft), static_cast<int16>(WindowY - BoxTop),
				static_cast<int16>(Window.WidthUnits() * 4 + BoxExtraWidth), static_cast<int16>(Window.Lines() * LineSpacing + BoxExtraHeight)};
			FDrawPrimitive& Box = Windows.Primitives.emplace_back(MakeGouraudRect(Rect, {Black, Black, Black, Black}));
			Box.bGouraud = false;
			Box.Blend = EDrawBlend::SemiTransparent;
			Box.UntexturedBlend = EBlendMode::Average;
		}
		// Each line's first 256 pixels from the window's page, then whatever lies past them from the next page, the last row first.
		const int32 Unit = Context.Text != nullptr && Context.Text->Font != nullptr ? Context.Text->Font->UnitPixels() : 4;
		for (const bool bSecondHalf : {false, true})
		{
			const int32 PageX = TextVramX + (bSecondHalf ? HalfUnits : 0);
			const FTexturePage Page = FTexturePage::FromAttribute(static_cast<uint16>((PageX & 0x3FF) >> 6 | (Text.VramY & 0x100) >> 4));
			for (int32 Row = Window.Lines() - 1; Row >= 0; --Row)
			{
				const FMesWindow::FLine& Line = Window.Row(Row);
				const int32 Rest = Line.WidthUnits * Unit - (bSecondHalf ? HalfPixels : 0);
				if (Rest <= 0)
				{
					continue;
				}
				const int32 Pixels = bSecondHalf ? Rest : std::min(Rest, HalfPixels);
				const FClutPosition Clut = FClutPosition::FromAttribute((Line.Strip & 1) != 0 ? OddTextClut : EvenTextClut);
				const uint8 U = static_cast<uint8>((TextVramX & 0x3F) * 4);
				const uint8 V = static_cast<uint8>(Text.VramY + Line.Strip / 2 * TextRows);
				Windows.Primitives.push_back(MakeSprite(WindowX + (bSecondHalf ? HalfPixels : 0), WindowY + LineSpacing * Row, Pixels, TextRows, {0, Page, Clut}, U, V));
				Windows.Primitives.back().bModulate = false;
			}
		}
		if (Window.Stepped())
		{
			// A line's whole strip is uploaded.
			const int32 Pitch = Window.WidthUnits() + ImageMargin;
			Text.Units.resize(static_cast<std::size_t>(Pitch) * TextRows);
			DrawMesTypedLines(Context.Text->Font, Window, Text.Units, Pitch, [this, &Text, Pitch](int32 Strip)
			{
				Scratch.Rect = {TextVramX, static_cast<int16>(Text.VramY + Strip / 2 * TextRows), static_cast<int16>(Pitch), TextRows};
				Scratch.Units = Text.Units;
				MapVram.Upload(Scratch);
			});
		}
	}

	void FWorldMapPresenter::DrawScreen(const FWorldMapContext& Context, FDrawList& Screen) const
	{
		Screen.Primitives.clear();
		if (Context.FadeShown >= 0)
		{
			// The fade task's screen-wide quad of its level, in the blend mode the scene gave it (subtracting as a scene starts).
			const uint8 Level = static_cast<uint8>(std::min(Context.FadeShown, 0xFF));
			const FDrawColor Color{Level, Level, Level};
			FDrawPrimitive& Fade = Screen.Primitives.emplace_back(MakeGouraudRect({0, 0, ScreenWidth, ScreenHeight}, {Color, Color, Color, Color}));
			Fade.Blend = EDrawBlend::SemiTransparent;
			Fade.UntexturedBlend = static_cast<EBlendMode>(Context.FadePageBlend & 3);
			Fade.BlendKey = WorldMapBlendKeys::Fade;
		}
	}

	FWorldMapDrawState FWorldMapPresenter::DrawState(const FWorldMapContext& Context) const
	{
		FWorldMapDrawState State;
		State.Camera = Context.CameraMatrix;
		State.Target = Context.CameraTarget;
		State.Pitch = Context.CameraPitch;
		State.Yaw = Context.CameraYaw;
		State.Eye = Context.Orbit.Eye;
		State.ScreenCenterY = Context.GteCenterY;
		State.ProjectionDistance = Context.ProjectionDistance;
		State.WavePhase = Context.WavePhase;
		State.WaveRowPhase = WaveRowPhase;
		State.FogNear = FogNear;
		State.Tick = Ticks;
		State.DrawBuffer = Context.DrawBuffer;
		if (PlayerOffsets != FWorldMapCameraOffsets{})
		{
			if (const std::optional<FWorldMapCameraBasis> Basis = WorldMapCameraBasis(Context))
			{
				const FWorldMapPlayerView View = WorldMapPlayerView(*Basis, PlayerOffsets);
				State.Camera = View.Camera;
				State.Pitch = View.Pitch;
				State.Yaw = View.Yaw;
				State.Eye = View.Eye;
				State.EyeBack = View.EyeBack;
				State.SortBack = View.Camera.T[2] - Context.CameraMatrix.T[2];
			}
		}
		return State;
	}

	void FWorldMapPresenter::DrawParticles(const FWorldMapContext& Context)
	{
		if (Drawn == nullptr)
		{
			return;
		}
		DrawWorldMapParticles(Context.Particles, Tables, Context.Ground, DrawState(Context), DrawnCulling, Builder);
	}

	void FWorldMapPresenter::DrawSprite(const FWorldMapContext& Context, int32 Slot, const FSprite& Sprite, int32 OtIndex)
	{
		if (Drawn == nullptr)
		{
			return;
		}
		const FWorldMapDrawState State = DrawState(Context);
		if (!(State.Camera == Context.CameraMatrix))
		{
			// The game measured the sprite's depth under its own camera: the entry it is linked into is the view's.
			FGte Gte;
			Gte.Transform = State.Camera;
			OtIndex = WorldMapSortDepth(Gte.Rtps({static_cast<int16>(Sprite.Position[0] >> 16), static_cast<int16>(Sprite.Position[1] >> 16),
				static_cast<int16>(Sprite.Position[2] >> 16)}).ScreenZ, State) >> SpriteEntryShift;
		}
		if (FollowGear(Context, Slot, Sprite, State, OtIndex))
		{
			return;
		}
		DrawWorldMapSprite(Sprite, Slot, State, OtIndex, Builder);
	}

	bool FWorldMapPresenter::FollowGear(const FWorldMapContext& Context, int32 Slot, const FSprite& Sprite, const FWorldMapDrawState& State, int32 OtIndex)
	{
		if (!GearModels || (Context.bCutscene && !bGearModelsInCutscenes) || Slot < 0 || Slot >= FWorldMapContext::TaskCount)
		{
			return false;
		}
		// A party slot's gear by the file its sprite is made from.
		std::size_t Party = GearPlaces.size();
		for (std::size_t Each = 0; Each < GearPlaces.size(); ++Each)
		{
			if (Context.GearFiles[Each] && Sprite.PrimaryFile == &*Context.GearFiles[Each] && Context.GearsShown[Each] != FGameState::EmptySlot)
			{
				Party = Each;
			}
		}
		if (Party == GearPlaces.size())
		{
			return false;
		}
		if (!bGearsBegun)
		{
			GearModels->BeginTick();
			for (FGearPlace& Place : GearPlaces)
			{
				Place.bFollowed = false;
			}
			bGearsBegun = true;
		}
		const FWorldMapTask& Task = Context.Tasks[static_cast<std::size_t>(Slot)];
		// The sprite's parts are drawn through its matrix under the camera: its second column's length is a part unit's height.
		const FGteMatrix Placed = SpriteDrawMatrix(Sprite, WorldMapSpriteRotation(Sprite), State.Camera);
		const std::array<std::array<int16, 3>, 3>& M = Placed.M;
		FFieldGearStandIns::FSight Sight;
		Sight.Gear = Context.GearsShown[Party];
		Sight.bGoes = Sprite.Animation() == GearWalkAnimation;
		Sight.Position = Sprite.Position;
		Sight.FloorY = static_cast<int16>(Sprite.Position[1] >> 16);
		Sight.Yaw = static_cast<uint16>((Task.Heading + HeadingToYaw) & (FullTurn - 1));
		Sight.Up = std::sqrt(static_cast<float>(M[0][1] * M[0][1] + M[1][1] * M[1][1] + M[2][1] * M[2][1])) / static_cast<float>(FGteMatrix::One);
		FGearPlace& Place = GearPlaces[Party];
		if (Place.bKnown && Place.Slot == Slot)
		{
			// The sprites stand about the camera's target, which moves with the party: the ground covered is the task's own.
			FWorldMapVector Covered{Task.Position.X - Place.At.X, 0, Task.Position.Z - Place.At.Z, 0};
			Context.Ground.WrapDelta(Covered);
			Sight.Covered = {static_cast<float>(Covered.X) / TaskUnit, -static_cast<float>(Covered.Z) / TaskUnit};
		}
		Place.bKnown = true;
		Place.At = Task.Position;
		Place.Slot = Slot;
		Place.OtIndex = OtIndex;
		Place.bFollowed = GearModels->FollowGear(Slot, Sprite, Sight, Context.Arctangents, MapVram);
		return Place.bFollowed;
	}

	std::optional<FError> FWorldMapPresenter::DrawGears(const FWorldMapDrawState& State)
	{
		if (!bGearsBegun)
		{
			// No gear's sprite was met this tick: the models are hidden.
			GearModels->BeginTick();
			for (FGearPlace& Place : GearPlaces)
			{
				Place.bFollowed = false;
			}
			bGearsBegun = true;
		}
		if (auto Error = GearModels->Pose(true))
		{
			return Error;
		}
		FFieldView View;
		View.View = State.Camera;
		View.ProjectionDistance = static_cast<uint16>(State.ProjectionDistance);
		FFieldGearsFrame& Gears = GearsFrame;
		Gears.Items.clear();
		Gears.Objects.clear();
		Gears.Polygons.clear();
		Gears.Cloths.clear();
		Gears.ClothSpaces.clear();
		Gears.Links.clear();
		Gears.TextureSeeds.clear();
		const FGteMatrix NoLight = FGteMatrix::Identity();
		for (int32 Module = 0; Module < GearModels->ModuleCount(); ++Module)
		{
			if (auto Error = AddFieldGears(GearModels->Module(Module), FFieldGearStandIns::LayoutSlot(Module, 0), GearModels->Tints(Module), View,
					FFieldGearStandIns::Lights(), GearFarBucket, EFieldCulling::InRange, true, Gears, 0, &NoLight))
			{
				return Error;
			}
		}
		// A model stands whole where its sprite would: every polygon in its sprite's entry, the farthest drawn first.
		const auto EntryOf = [this](int32 LayoutSlot) -> std::optional<uint16>
		{
			for (const FGearPlace& Place : GearPlaces)
			{
				if (Place.bFollowed && GearModels->LayoutSlotOf(Place.Slot) == LayoutSlot)
				{
					return static_cast<uint16>(std::clamp(Place.OtIndex, 0, 0xFFFF));
				}
			}
			return std::nullopt;
		};
		const std::size_t First = Builder.LinkedCount();
		FGte Gte = WorldMapDrawingGte(State);
		int32 LastSlot = -1;
		uint32 PartOfSlot = 0;
		for (std::size_t Each = 0; Each < Gears.Items.size(); ++Each)
		{
			const FFieldGearLayoutItem& Item = Gears.Items[Each];
			const FFieldGearObject& Object = Gears.Objects[Each];
			PartOfSlot = Item.Slot == LastSlot ? PartOfSlot + 1 : 0;
			LastSlot = Item.Slot;
			const std::optional<uint16> Entry = Item.Packets != nullptr && Object.bDrawn && Object.bFine ? EntryOf(Item.Slot) : std::nullopt;
			if (!Entry)
			{
				continue;
			}
			const FMdlPart& Part = *Item.Part;
			Builder.AnchorFine(EWorldMapAnchorKind::Gear, static_cast<uint32>(Item.Slot) << 8 | PartOfSlot, Object.Matrix, Object.Fine);
			Gte.Transform = Object.Matrix;
			GearProjected.resize(Part.Vertices.size());
			for (std::size_t Vertex = 0; Vertex < Part.Vertices.size(); ++Vertex)
			{
				GearProjected[Vertex] = Gte.Rtps(Part.Vertices[Vertex]);
			}
			for (uint32 Number = 0; Number < Item.PolygonCount; ++Number)
			{
				const FFieldGearPolygon& Lit = Gears.Polygons[Item.FirstPolygon + Number];
				if (!Lit.bDrawn)
				{
					continue;
				}
				const FMdlPrimitive& Primitive = Part.Primitives[Number];
				const FMdlPrimitiveTraits& Traits = Primitive.Traits();
				const uint8 Code = Item.Packets->Packets[Number].Code;
				FWorldMapPolygon& Polygon = Builder.Add(*Entry);
				Polygon.Layer = EWorldMapLayer::Gear;
				Polygon.CornerCount = Traits.Corners;
				Polygon.bGouraud = Traits.bGouraud;
				Polygon.bTextured = Traits.bTextured;
				Polygon.bRaw = Traits.bTextured && (Code & 1) != 0;
				Polygon.bSemiTransparent = (Code & 2) != 0;
				Polygon.bPortRows = Item.bPortRows;
				uint8 LowU = 0xFF;
				uint8 LowV = 0xFF;
				uint8 HighU = 0;
				uint8 HighV = 0;
				for (std::size_t Corner = 0; Corner < static_cast<std::size_t>(Traits.Corners); ++Corner)
				{
					const std::size_t Vertex = Primitive.Vertices[Corner];
					const FGteVector& At = Part.Vertices[Vertex];
					const std::array<float, 3> Turned = Object.Fine.Turn({static_cast<float>(At[0]), static_cast<float>(At[1]), static_cast<float>(At[2])});
					Polygon.View[Corner] = {Turned[0] + Object.Fine.T[0], Turned[1] + Object.Fine.T[1], Turned[2] + Object.Fine.T[2]};
					Polygon.ScreenX[Corner] = GearProjected[Vertex].ScreenX;
					Polygon.ScreenY[Corner] = GearProjected[Vertex].ScreenY;
					const FMdlColor& Colour = Lit.Colors[Traits.bGouraud ? Corner : 0];
					Polygon.Colors[Corner] = {Colour.R, Colour.G, Colour.B};
					if (Traits.bTextured)
					{
						Polygon.U[Corner] = Primitive.Uvs[Corner].U;
						Polygon.V[Corner] = Primitive.Uvs[Corner].V;
						Polygon.TexCoords[Corner] = {static_cast<float>(Polygon.U[Corner]), static_cast<float>(Polygon.V[Corner])};
						LowU = std::min(LowU, Polygon.U[Corner]);
						LowV = std::min(LowV, Polygon.V[Corner]);
						HighU = std::max(HighU, Polygon.U[Corner]);
						HighV = std::max(HighV, Polygon.V[Corner]);
					}
				}
				if (Traits.bTextured)
				{
					Polygon.TexturePage = Item.Packets->Textures[Number].TexturePage;
					Polygon.Clut = Item.Packets->Textures[Number].Clut;
					Polygon.TexelMin = {LowU, LowV};
					Polygon.TexelMax = {HighU, HighV};
				}
			}
		}
		// The cloths' points lie in their models' own spaces: an anchor a model for its cloths.
		for (const FFieldGearClothSpace& Space : Gears.ClothSpaces)
		{
			const std::optional<uint16> Entry = EntryOf(Space.Slot);
			if (!Entry)
			{
				continue;
			}
			Builder.AnchorFine(EWorldMapAnchorKind::Gear, static_cast<uint32>(Space.Slot) << 8 | ClothAnchor, FGteMatrix::Identity(), Space.Space);
			for (const FGearDrawnClothTriangle& Triangle : Gears.Cloths)
			{
				if (Triangle.Slot != Space.Slot)
				{
					continue;
				}
				FWorldMapPolygon& Polygon = Builder.Add(*Entry);
				Polygon.Layer = EWorldMapLayer::Gear;
				Polygon.CornerCount = 3;
				Polygon.bGouraud = true;
				Polygon.bTextured = true;
				Polygon.bPortRows = true;
				Polygon.TexturePage = Triangle.TexturePage;
				Polygon.Clut = Triangle.Clut;
				uint8 LowU = 0xFF;
				uint8 LowV = 0xFF;
				uint8 HighU = 0;
				uint8 HighV = 0;
				for (std::size_t Corner = 0; Corner < 3; ++Corner)
				{
					const FGteVector& At = Triangle.Points[Corner];
					const std::array<float, 3> Turned = Space.Space.Turn({static_cast<float>(At[0]), static_cast<float>(At[1]), static_cast<float>(At[2])});
					Polygon.View[Corner] = {Turned[0] + Space.Space.T[0], Turned[1] + Space.Space.T[1], Turned[2] + Space.Space.T[2]};
					Polygon.Colors[Corner] = {Triangle.Colors[Corner].R, Triangle.Colors[Corner].G, Triangle.Colors[Corner].B};
					Polygon.U[Corner] = Triangle.Uvs[Corner].U;
					Polygon.V[Corner] = Triangle.Uvs[Corner].V;
					Polygon.TexCoords[Corner] = {static_cast<float>(Polygon.U[Corner]), static_cast<float>(Polygon.V[Corner])};
					LowU = std::min(LowU, Polygon.U[Corner]);
					LowV = std::min(LowV, Polygon.V[Corner]);
					HighU = std::max(HighU, Polygon.U[Corner]);
					HighV = std::max(HighV, Polygon.V[Corner]);
				}
				Polygon.TexelMin = {LowU, LowV};
				Polygon.TexelMax = {HighU, HighV};
			}
		}
		Builder.LinkNearestFirst(First);
		return std::nullopt;
	}

	void FWorldMapPresenter::DrawWorld(const FWorldMapContext& Context)
	{
		if (Drawn == nullptr)
		{
			return;
		}
		const FWorldMapDrawState State = DrawState(Context);
		Drawn->TextureSeeds.clear();
		if (GearModels)
		{
			GearError = DrawGears(State);
			Drawn->TextureSeeds.assign(GearsFrame.TextureSeeds.begin(), GearsFrame.TextureSeeds.end());
		}
		if (Context.bTerrainRestarted)
		{
			BillboardArea.BlockX = Context.Start.X >> 23;
			BillboardArea.BlockZ = Context.Start.Z >> 23;
		}
		if (Context.FrameTask != EWorldMapFrameTask::Cutscene)
		{
			DrawWorldMapBillboards(*Scene, Scene->Terrain, BillboardArea, Context.CameraRoll, State, DrawnCulling, Context.Options.bTreesInDepthOrder, Builder);
		}
		const int16 VehicleHeading = static_cast<int16>(LittleEndian::ReadU16(Context.Game->Block(), WorldMapStateLayout::VehicleHeading));
		DrawWorldMapShadows(std::span<const FWorldMapShadow>(Context.Shadows.data(), static_cast<std::size_t>(Context.ShadowsShown)), Context.Ground,
			VehicleHeading, State, DrawnCulling, Builder);
		const std::size_t VehicleObjects = Context.FrameTask != EWorldMapFrameTask::Cutscene ? VehicleObjectCount : 0;
		DrawWorldMapObjects(Context.Objects, *Scene, ModelPackets, Tables, Context.Ground, State, DrawnCulling, VehicleObjects, Builder);
		DrawWorldMapTerrain(Scene->Terrain, Tables, State, DrawnCulling, Builder, &BillboardArea.BlockStates);
		BillboardArea.BlockX = State.Target.X >> 23;
		BillboardArea.BlockZ = State.Target.Z >> 23;
		BillboardArea.Across = WorldMapBlocksAcross(DrawnCulling);
		DrawWorldMapSky(Tables, State, DrawnCulling, Builder);
		DrawWorldMapClouds(Context.Clouds, Tables, State, DrawnCulling, Builder);
		Drawn->Minimap.Primitives.clear();
		// A cutscene's minimap is never set up: its packets draw nothing, whatever the switch says.
		if (!Context.bCutscene && LittleEndian::ReadU16(Context.Game->Block(), WorldMapStateLayout::MinimapHidden) == 0)
		{
			DrawWorldMapMinimap(Context, Tables, State, Drawn->Minimap);
		}
		Drawn->ScreenCenterY = State.ScreenCenterY;
		Drawn->ProjectionDistance = State.ProjectionDistance;
		Drawn->Camera = State.Camera;
		Drawn->SortBack = State.SortBack;
		Drawn->TargetX = State.Target.X >> 12;
		Drawn->TargetZ = State.Target.Z >> 12;
		Drawn->Yaw = State.Yaw;
		const std::optional<FWorldMapCameraBasis> Basis = WorldMapCameraBasis(Context);
		Drawn->bPlayerView = Basis.has_value();
		Drawn->CameraBasis = Basis.value_or(FWorldMapCameraBasis{});
		Drawn->ShownOffsets = Basis ? WorldMapPlayerView(*Basis, PlayerOffsets).Shown : FWorldMapCameraOffsets{};
		Drawn->WorldUnitsX = Scene->Terrain.BlocksX() * FWorldMapTerrain::BlockSide;
		Drawn->WorldUnitsZ = Scene->Terrain.BlocksZ() * FWorldMapTerrain::BlockSide;
		Drawn = nullptr;
	}
}
