#include "Field/FieldSpriteStandIns.h"

#include "Graphics/GteMatrix.h"
#include "Graphics/SpriteDrawing.h"
#include "Graphics/Vram.h"

namespace Weltall
{
	namespace
	{
		/** As the field makes a party member's sprite: part records, part coordinates in texels << 3, weight. */
		constexpr int32 PartRecords = 0x20;
		constexpr int32 FieldSizeShift = 3;
		constexpr int16 FieldSpriteWeight = 0x2000;
		constexpr int32 WholeScale = 0x1000;

		/** The stand-ins' uploads, into the port's rows. */
		class FPortRows final : public IVramUploads
		{
		public:
			explicit FPortRows(FVram& InVram) : Vram(InVram) {}
			void Upload(const FVramImage& Image) override { Vram.UploadToPortRows(Image); }

		private:
			FVram& Vram;
		};
	}

	FFieldSpriteStandIns::FFieldSpriteStandIns(int32 Places, int16 InAreaX, int16 InAreaY, int16 InClutX, int16 InClutRow)
		: AreaX(InAreaX), AreaY(InAreaY), ClutX(InClutX), ClutRow(InClutRow), StandIns(static_cast<std::size_t>(Places))
	{
		Kernel.Random = &Random;
	}

	void FFieldSpriteStandIns::Clear()
	{
		Kernel.EndTasks();
		for (FStandIn& StandIn : StandIns)
		{
			if (StandIn.File != nullptr)
			{
				Kernel.Free(StandIn.Sprite);
			}
			StandIn.File = nullptr;
			StandIn.bFailed = false;
		}
	}

	void FFieldSpriteStandIns::Make(FStandIn& StandIn, int32 Place, const FSpriteFile& File, const FVramImage* Picture, FVram& Vram)
	{
		if (StandIn.File != nullptr)
		{
			Kernel.Free(StandIn.Sprite);
		}
		const int16 TextureX = Picture != nullptr ? Picture->Rect.X : static_cast<int16>(AreaX + AreaWidth * Place);
		const int16 TextureY = Picture != nullptr ? Picture->Rect.Y : AreaY;
		if (Picture != nullptr)
		{
			Vram.UploadToPortRows(*Picture);
		}
		Kernel.Create(StandIn.Sprite, File, ClutX, static_cast<int16>(ClutRow + Place), TextureX, TextureY);
		Kernel.SetPartCapacity(StandIn.Sprite, PartRecords);
		Kernel.SetSizeShift(StandIn.Sprite, FieldSizeShift);
		StandIn.Sprite.Weight = FieldSpriteWeight;
		StandIn.Sprite.Listener = this;
		StandIn.File = &File;
		StandIn.bFailed = false;
	}

	void FFieldSpriteStandIns::Step(FStandIn& StandIn, const FSprite& Followed, int32 Animation, uint32 Tick, bool bMade, FVram& Vram)
	{
		FSprite& Sprite = StandIn.Sprite;
		Sprite.SetTimeScale(static_cast<int32>(Followed.TimeScale()));
		// The step the followed sprite took after it was drawn last, then what it started since: a start's first step
		// comes with the next tick's, as the followed one's does (a stand-in just made takes it at once, to have a frame).
		if (!bMade && StandIn.Tick != Tick && StandIn.bFollowedAnimates)
		{
			Kernel.TickAnimation(Sprite);
		}
		StandIn.Tick = Tick;
		if (bMade || Followed.Starts != StandIn.SeenStarts || Animation != StandIn.Asked)
		{
			Sprite.Facing = Followed.Facing;
			Kernel.SetAnimation(Sprite, Animation);
			StandIn.SeenStarts = Followed.Starts;
			StandIn.Asked = Animation;
			if (bMade)
			{
				Kernel.TickAnimation(Sprite);
			}
		}
		if (Sprite.Facing != Followed.Facing)
		{
			Kernel.SetFacing(Sprite, Followed.Facing);
		}
		Kernel.BuildPendingFrames();
		Kernel.EndTasks();
		FPortRows Into(Vram);
		Kernel.FlushUploads(Into);
	}

	const FSprite* FFieldSpriteStandIns::Follow(int32 Place, const FSpriteFile& File, int32 Animation, const FSprite& Followed, bool bAnimates, int32 Scale,
		uint32 Tick, const FGteArctangents* Arctangents, FVram& Vram, const FVramImage* Picture)
	{
		FStandIn& StandIn = StandIns[static_cast<std::size_t>(Place)];
		const bool bMade = StandIn.File != &File;
		if (bMade)
		{
			Make(StandIn, Place, File, Picture, Vram);
		}
		if (StandIn.bFailed)
		{
			return nullptr;
		}
		Kernel.Arctangents = Arctangents;
		FSprite& Sprite = StandIn.Sprite;
		Sprite.Position = Followed.Position;
		// A jump's waits on the fall and the landing read the vertical speed and the floor, which only the followed one's actor moves.
		Sprite.Velocity[1] = Followed.Velocity[1];
		Sprite.FloorY = Followed.FloorY;
		Sprite.Scale = static_cast<int16>(Followed.Scale * Scale >> 12);
		Step(StandIn, Followed, Animation, Tick, bMade, Vram);
		StandIn.bFollowedAnimates = bAnimates;
		if (Kernel.Fault() != ESpriteFault::None)
		{
			Kernel.ClearFault();
			StandIn.bFailed = true;
			return nullptr;
		}
		// The pass's tint, blend and hidden anchor groups, as the followed sprite carries them.
		constexpr uint32 PassFlags = SpriteDrawFlags::BlendMask | 0xFFu << SpriteDrawFlags::HiddenGroupsShift;
		Sprite.Red = Followed.Red;
		Sprite.Green = Followed.Green;
		Sprite.Blue = Followed.Blue;
		Sprite.Code = Followed.Code;
		Sprite.DrawFlags = (Sprite.DrawFlags & ~PassFlags) | (Followed.DrawFlags & PassFlags);
		Kernel.ApplyColorBlend(Sprite);
		return &Sprite;
	}

	FFieldSpritePass FFieldSpriteStandIns::PassFor(const FSprite& Followed, const FSprite& StandIn, const FFieldSpritePass& Pass, int32 Scale)
	{
		FFieldSpritePass Result = Pass;
		Result.Matrix = Scale == WholeScale ? Pass.Matrix : ScaleColumns(Pass.Matrix, {Scale, Scale, Scale});
		const std::array<int32, 2> From = SpritePivotOffset(Followed);
		const std::array<int32, 2> To = SpritePivotOffset(StandIn);
		Result.Matrix.T = {Pass.Matrix.T[0] - From[0] + To[0], Pass.Matrix.T[1] - From[1] + To[1], Pass.Matrix.T[2]};
		// The wading line lies as far below the feet, in the stand-in's texels.
		Result.ClipLine = Pass.ClipLine * WholeScale / Scale;
		return Result;
	}
}
