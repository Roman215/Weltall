#pragma once

#include "CoreMinimal.h"
#include "Field/FieldBackdrop.h"
#include "Field/FieldRenderOptions.h"
#include "Field/FieldSceneFrame.h"
#include "Graphics/TextureRegion.h"
#include "RenderGraphFwd.h"
#include "RenderGraphResources.h"

#include <span>
#include <vector>

class FVramTexture;

namespace Weltall
{
	struct FFieldFrame;
	struct FFieldGearLayoutItem;
	class FTexelKindCache;
}

/**
 * A corner as the field-scene shader reads it; the layout matches FFieldCorner in FieldScene.usf, on Vulkan too
 * (whose buffer layout puts the texel coordinates on 8 bytes: Polygon keeps them there).
 */
struct FFieldSceneCorner
{
	FVector3f Position;
	uint32 Polygon = 0;
	FVector2f TexCoord;
	uint32 Object = 0;
	uint32 Corner = 0;
};
static_assert(sizeof(FFieldSceneCorner) == 32, "the field-scene shader reads 32-byte corners");

/** A polygon's drawing state as the field-scene shader reads it; the layout matches FFieldPolygon in FieldScene.usf. */
struct FFieldScenePolygonState
{
	/** Its texture page and CLUT as FVramTexture::Pack packs them, or FVramTexture::NoTexture. */
	uint32 Texture = 0;
	uint32 Flags = 0;
	uint32 TexelMin = 0;
	uint32 TexelMax = 0;
};
static_assert(sizeof(FFieldScenePolygonState) == 16, "the field-scene shader reads 16-byte polygons");

// The presentation's options are the player's settings, which the core keeps.
using EFieldSorting = Weltall::EFieldSorting;
using EFieldTextureFilter = Weltall::EFieldTextureFilter;
using EFieldAntiAliasing = Weltall::EFieldAntiAliasing;
using FFieldRenderOptions = Weltall::FFieldRenderOptions;

/**
 * A map's scenery on the GPU, made once per map: every polygon's corners in its model's space
 * with their texture coordinates (a quad as the triangles 0-1-2 and 1-3-2), and each polygon's
 * texture and flags; after them the gears' models loaded when it was made (each floor shadow and
 * each bone's part, as the core lays them out), remade whenever those change. A corner the scene
 * joins to another model (Field/FieldSceneJoins.h) is that model's point, drawn by its matrix, so the
 * two models' edges come out of the same numbers and meet to the pixel. The GPU buffers are
 * made on the rendering thread when first drawn.
 */
class WELTALL_API FFieldSceneMesh
{
public:
	explicit FFieldSceneMesh(const Weltall::FFieldScene& Scene);
	/** The scenery and the gears laid out as Gears, whose models' revision is GearLayout; the map's sprites drawn from SpriteSheets. */
	FFieldSceneMesh(const Weltall::FFieldScene& Scene, std::span<const Weltall::FFieldGearLayoutItem> Gears, uint32 GearLayout,
		std::span<const Weltall::FTextureRegion> SpriteSheets = {});
	~FFieldSceneMesh();

	/** The gears' polygons follow the scenery's: the frame's gear polygon Index is mesh polygon FirstGearPolygon() + Index. */
	uint32 FirstGearPolygon() const { return SceneryPolygons; }
	uint32 GearPolygons() const { return Polygons() - SceneryPolygons; }
	/** The revision of the gears' models the mesh holds. */
	uint32 GearLayout() const { return GearRevision; }
	/** The first object row the gears' objects are drawn by: after the scenery's models and the identity. */
	uint32 FirstGearObject() const { return GearObjectRow; }
	/** How a gear polygon blends: bit 0 semi-transparent, bit 1 textured, bits 2-3 its page's blend mode. */
	uint8 GearBlend(uint32 Index) const { return GearBlends[static_cast<int32>(Index)]; }

	/**
	 * The pictures its polygons read, the scenery's as Weltall::FieldSceneryTextureRegions finds them, the map's sprite
	 * sheets, then the gears', merged as the scenery's are.
	 */
	TConstArrayView<Weltall::FTextureRegion> TextureRegions() const { return MakeArrayView(Regions.data(), static_cast<int32>(Regions.size())); }

	/** The scenery's polygons with a joined corner, in rising order: drawn from corners of their own while a join of theirs does not hold. */
	TConstArrayView<uint32> JoinedPolygons() const { return JoinedPolygonList; }

	/** The polygon's corners in the mesh, three per triangle it is drawn as. */
	uint32 FirstCorner(uint32 Polygon) const { return PolygonCorners[Polygon]; }
	uint32 CornerCount(uint32 Polygon) const { return PolygonCorners[Polygon + 1] - PolygonCorners[Polygon]; }
	uint32 Corners() const { return static_cast<uint32>(CornerData.Num()); }
	/** The polygon a corner of the mesh belongs to. */
	uint32 CornerPolygon(uint32 Corner) const { return CornerData[static_cast<int32>(Corner)].Polygon; }
	uint32 Polygons() const { return static_cast<uint32>(PolygonData.Num()); }
	TConstArrayView<FFieldScenePolygonState> States() const { return PolygonData; }

	/** The mesh's buffers in a render graph, made on the first call. Rendering thread only. */
	FRDGBufferRef CornerBuffer(FRDGBuilder& GraphBuilder) const;
	FRDGBufferRef PolygonBuffer(FRDGBuilder& GraphBuilder) const;

private:
	void AddScenery(const Weltall::FFieldScene& Scene);
	void AddGears(std::span<const Weltall::FFieldGearLayoutItem> Gears);

	TArray<FFieldSceneCorner> CornerData;
	TArray<FFieldScenePolygonState> PolygonData;
	TArray<uint32> PolygonCorners;
	TArray<uint32> JoinedPolygonList;
	uint32 SceneryPolygons = 0;
	uint32 GearRevision = 0;
	uint32 GearObjectRow = 0;
	TArray<uint8> GearBlends;
	std::vector<Weltall::FTextureRegion> Regions;
	mutable TRefCountPtr<FRDGPooledBuffer> PooledCorners;
	mutable TRefCountPtr<FRDGPooledBuffer> PooledPolygons;
};

/** How a stretch of the drawing meets the depth buffer and the pixels the characters' sprites and the 3D models mark in it. */
enum class EFieldRunDepth : uint8
{
	/** As the painter's order draws it: no depth. */
	None,
	/** The sprites' and the models' opaque texels: their depth less the margin, marking their pixels, no colour. */
	Mask,
	/** As Mask, beside the models' marks alone, once they are made: the scenery drawn over the models (OverModels) where none stands. */
	MaskBesideModels,
	/** As MaskBesideModels, beside the sprites' marks too: the ground drawn over the models (GroundOverModels). */
	MaskBesideMarks,
	/**
	 * As the painter's order draws it outside the marked pixels, its depth kept there; against the depth inside them
	 * (the scenery sorted by depth: against the depth everywhere).
	 */
	Masked,
	/**
	 * As Masked, but over the models' marks as the painter's order draws it, keeping no depth there: what the models stand
	 * in or show through (the battle's stage, its effects). The sprites' rule: in the order outside the sprites' marks,
	 * against the depth inside them. The scenery's: against the depth beside the models' marks.
	 */
	OverModels,
	/** As OverModels, but never over the sprites' marks: the ground the sprites stand on (FScenePolygonItem::bGround). */
	GroundOverModels,
	/** A sprite's or a model's own texels: inside the marked pixels, against the depth less the margin, noting where it drew. */
	InsideMask,
	/**
	 * A sprite's or a model's depth again where it just drew, as far behind it as the margin: what is drawn after it
	 * covers it unless clearly behind it. No colour.
	 */
	PushedBack,
	/**
	 * A shadow on its floor: never over a sprite's marked pixels, elsewhere as the painter's order draws it; where the
	 * drawing knows the floor's up (FFieldSceneDraw::FloorUp), not where what was drawn there before it stands well above
	 * its floor (a ledge or a wall in front of it), and over a model's pixels only where it stands close to the floor
	 * (a gear's soles).
	 */
	OutsideMask,
};

/** Whose depth a run's polygons stand at against the depth buffer, and the margins they keep there. */
enum class EFieldDepthRule : uint8
{
	/** Sprites and particles: their ordering-table entries' far edges (the sprites' margin), the rest at their own depth. */
	Entry,
	/**
	 * Polygons of 3D models (the gears', the children's) at their own depth, marking their pixels with a bit of their
	 * own: within the models' margin of the nearest the order decides, beyond it the depth.
	 */
	Model,
	/**
	 * The scenery sorted by depth (EFieldSorting::DepthBufferWithScenery): within the decals' margin of the nearest
	 * surface, measured across the polygon's plane, the order decides; beyond it the depth.
	 */
	Scenery,
};

/** A stretch of the drawing done with one state. */
struct FFieldDrawRun
{
	uint32 FirstVertex = 0;
	uint32 VertexCount = 0;
	/** Which texels the shader keeps (the passes of FieldScene.usf). */
	uint8 Pass = 0;
	Weltall::EBlendMode Blend = Weltall::EBlendMode::Average;
	EFieldRunDepth Depth = EFieldRunDepth::None;
	/** Where it is tested against the depth, it also writes its own. */
	bool bDepthWrite = false;
	EFieldDepthRule Rule = EFieldDepthRule::Entry;
};

/** A corner of the screen wave's grid: where it is drawn, and the place of the scene drawn so far it shows, both on the 320 x 224 screen. */
struct FFieldWaveCorner
{
	FVector2f Position;
	FVector2f Source;
};

/**
 * A scene staged before a painted picture (FFieldSceneObject::bPicture), as a view wider than the original's screen
 * draws it: the picture stretched across about the screen's middle, as far as 21:9, and everything else moved sideways
 * with it by where it stands, keeping its shape. Made with the frame's drawing; the view's width applies it as the
 * frame is drawn (StretchFieldPictureScene).
 */
struct FFieldPictureStretch
{
	/** Whether the frame has a picture: when not, the rest is empty. */
	bool bStaged = false;
	/** The objects (by their rows' number, rows / 3) of the pictures. */
	TArray<int32> Pictures;
	/** Per object, the object whose place in view space moves it, or NoAnchor: a picture, the identity, the cloths' view. */
	TArray<int32> ObjectAnchors;
	/**
	 * Per dynamic corner, the point (in its object's space) whose place in view space moves it: a character's origin,
	 * a particle's middle, a child's model's origin, a cloth's gear's place; where CornerMoves is 0 the corner moves
	 * with its object instead.
	 */
	TArray<FVector3f> CornerAnchors;
	TArray<uint8> CornerMoves;

	static constexpr int32 NoAnchor = -1;
};

/**
 * One frame of the field as the GPU draws it, made on the game thread from the core's frame: the
 * scenery's mesh drawn by its models' matrices, the characters' and the particles' quads, already
 * in view space, and the models whose vertices or texels changed, each tick, as their own corners.
 */
struct FFieldSceneDraw
{
	/** Three rows per placed model, model to view space; the view-space quads take the last, the identity. */
	TArray<FVector4f> Objects;
	/** Per polygon, its corners' colors this frame (the first's top byte FieldColorFlags): the scenery's, the gears', then the dynamic polygons'. */
	TArray<FUintVector4> Colors;
	/**
	 * The dynamic corners and states, after the mesh's: the characters' quads, the particles' from
	 * ParticleCorners, the children's models' polygons, the polygons of models whose vertices or texels
	 * changed, the panorama's BackdropCornerCount from BackdropCorners, then the gears' cloths' triangles.
	 */
	TArray<FFieldSceneCorner> DynamicCorners;
	TArray<FFieldScenePolygonState> DynamicPolygons;
	/** The whole cells the characters' quads show, of which a quad clipped at a waterline shows some rows. */
	TArray<Weltall::FTextureRegion> SpriteCells;
	uint32 ParticleCorners = 0;
	/** Per polygon of the children's models, the dynamic corner it starts at, or NoRedirect for one not drawn. */
	TArray<uint32> SpriteModelCorners;
	uint32 BackdropCorners = 0;
	uint32 BackdropCornerCount = 0;
	/** The panorama's packets across the widest view drawn. */
	std::vector<Weltall::FFieldBackdropQuad> BackdropQuads;
	/** The pictures its strip is drawn from (Weltall::PanoramaTextureRegions). */
	TArray<Weltall::FTextureRegion> BackdropRegions;
	/** Per polygon of the mesh, the dynamic corner its changed copy starts at, or NoRedirect to draw the mesh's. */
	TArray<uint32> Redirects;
	/** The first corner (in the whole numbering) of the gears' cloth triangles, three each. */
	uint32 FirstClothCorner = 0;
	/** Whether the gears' part of the frame fits the mesh: when not, nothing of the gears is drawn. */
	bool bGearsDrawn = false;
	/** The corners in drawing order, the mesh's first and then the dynamic ones. */
	TArray<uint32> Order;
	TArray<FFieldDrawRun> Runs;
	/**
	 * The screen wave: the runs before WaveRuns are drawn, then drawn again through the grid's
	 * triangles (the whole target first, as it is, beside the wave), and the rest of the runs over it.
	 */
	bool bWave = false;
	int32 WaveRuns = 0;
	TArray<FFieldWaveCorner> WaveGrid;
	/** The grid a view wider than the screen is drawn through instead, the wave carried on across it. */
	TArray<FFieldWaveCorner> WideWaveGrid;
	/** The rectangle of the 320 x 224 screen the scene is drawn in, when the scripts narrow it. */
	bool bClipped = false;
	FIntRect Clip;
	FLinearColor Background = FLinearColor::Black;
	float ProjectionDistance = 0.0f;
	EFieldTextureFilter Filter = EFieldTextureFilter::Nearest;
	EFieldTextureFilter SpriteFilter = EFieldTextureFilter::Nearest;
	EFieldAntiAliasing AntiAliasing = EFieldAntiAliasing::None;
	bool bDepthBuffer = false;
	/** The world's up in view space, which the shadows' floors face; zero where the shadows are drawn as the painter's order alone. */
	FVector3f FloorUp = FVector3f::ZeroVector;
	FFieldPictureStretch PictureStretch;

	static constexpr uint32 NoRedirect = 0xFFFFFFFF;
};

/** Makes a frame's drawing of the scenery alone from the core's evaluation of it (under the options' culling), reusing Draw's memory. */
WELTALL_API void BuildFieldSceneDraw(const Weltall::FFieldScene& Scene, const Weltall::FFieldSceneFrame& Frame, const Weltall::FFieldView& View,
	const FFieldSceneMesh& Mesh, const FFieldRenderOptions& Options, FFieldSceneDraw& Draw);

/**
 * Makes a displayed frame's drawing of the field from its latest tick's frame: what moves between
 * ticks (the models, the characters, the projection) drawn Alpha of the way from the tick before's
 * Previous, unless the view cut or the thing jumped; cels and colours the latest tick's. Reuses Draw's memory.
 * TexelKinds, following the VRAM the frame is drawn from exactly as the GPU will have it, lets a semi-transparent
 * polygon's pass with no texels to draw be left out. Returns whether it drew between the two (no cut).
 */
WELTALL_API bool BuildFieldFrameDraw(const Weltall::FFieldScene& Scene, const Weltall::FFieldFrame& Latest, const Weltall::FFieldFrame* Previous, float Alpha,
	const FFieldSceneMesh& Mesh, const FFieldRenderOptions& Options, FFieldSceneDraw& Draw, Weltall::FTexelKindCache* TexelKinds = nullptr);

/**
 * Moves a frame's drawing from the view it was made under (From, world to view) to another (To): every object's
 * rows, so the scene and what is placed in view space with it are shown as To sees them. The player's moves of the
 * view between two frames, shown at once.
 */
WELTALL_API void MoveFieldSceneView(FFieldSceneDraw& Draw, const Weltall::FAffine& From, const Weltall::FAffine& To);

/**
 * Moves a field scene drawn under Latest's view, Alpha of the way from Previous's where it blended, to the same views
 * without the whole steps the game's look-at takes, where the frames know them: the rounding shakes a moving camera by
 * a fraction of the original's pixel from tick to tick, which a display many times its size shows.
 */
WELTALL_API void UnroundFieldSceneView(FFieldSceneDraw& Draw, const Weltall::FFieldFrame& Latest, const Weltall::FFieldFrame* Previous, float Alpha);

/**
 * Adds the passes that draw a field frame into Area of Output: the field's 320 x 224 screen fills
 * the area's height and is widened across to fill its width; the clear colour fills the whole area.
 * Textures come from Vram.
 */
WELTALL_API void AddFieldScenePasses(FRDGBuilder& GraphBuilder, const FFieldSceneMesh& Mesh, const FVramTexture& Vram, const FFieldSceneDraw& Draw,
	FRDGTextureRef Output, const FIntRect& Area);

/**
 * Adds the pass that draws a frame's polygons straight over what Area of Output holds, placed as AddFieldScenePasses
 * places them, in the painter's order: what a frame draws in front of a layer drawn after its scene.
 */
WELTALL_API void AddFieldSceneOverlayPasses(FRDGBuilder& GraphBuilder, const FFieldSceneMesh& Mesh, const FVramTexture& Vram, const FFieldSceneDraw& Draw,
	FRDGTextureRef Output, const FIntRect& Area);

/**
 * How far a scene staged before a picture stretches across a target of Size: its width over the original screen's (the
 * 320 x 224 screen, which fills the target's height), as far as 21:9; 1 for a target no wider than the screen.
 */
WELTALL_API float FieldPictureStretchFactor(FIntPoint Size);

/** The part of a target of Size a scene staged before a picture is drawn in: the whole target, or 21:9 of it about its middle. */
WELTALL_API FIntRect FieldPictureArea(FIntPoint Size);

/**
 * A frame's object rows and dynamic corners as a scene staged before a picture is drawn Factor times as wide
 * (FFieldPictureStretch): each picture's rows scaled across, every other object moved by its anchor's place, each
 * dynamic corner that has an anchor moved by it. Returns false, leaving both untouched, when there is nothing to stretch.
 */
WELTALL_API bool StretchFieldPictureScene(const FFieldSceneDraw& Draw, float Factor, TArray<FVector4f>& Objects, TArray<FFieldSceneCorner>& Corners);

/** Maps the field's 320 x 224 screen into a target of the given size (scale xy, then offset zw): its height fills the target's, centred across. */
WELTALL_API FVector4f FieldScreenPlacement(FIntPoint Size);

/** Field render target samples, with a single-sample fallback for affected Android Vulkan adapters. */
WELTALL_API uint8 FieldSceneSampleCount(EFieldAntiAliasing AntiAliasing, FStringView AndroidAdapterName);
