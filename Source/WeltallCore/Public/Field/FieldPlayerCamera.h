#pragma once

#include "Field/FieldCamera.h"
#include "Field/FieldCameraOffsets.h"
#include "Foundation/Types.h"

#include <algorithm>
#include <array>
#include <limits>

namespace Weltall
{
	struct FEventContext;

	/**
	 * What the player does to the field's view in a display frame: turns it (degrees, the way R1 turns
	 * the camera), tilts it (degrees, the eye higher) and zooms it (halvings of the distance: positive
	 * nearer), or puts its zoom and turn back as the game frames it (the tilt is a setting, which stays).
	 */
	struct FFieldViewInput
	{
		/** How far a stick held all the way moves the view in a second. */
		static constexpr float TurnPerSecond = 180.0f;
		static constexpr float TiltPerSecond = 60.0f;
		static constexpr float ZoomPerSecond = 1.0f;

		float Turn = 0.0f;
		float Tilt = 0.0f;
		float Zoom = 0.0f;
		bool bReset = false;
		/** The mouse's notches of the zoom's binding, turned forward (the zoom has them too): what scrolls the port's lists. */
		float Wheel = 0.0f;

		void Add(const FFieldViewInput& More)
		{
			Turn += More.Turn;
			Tilt += More.Tilt;
			Zoom += More.Zoom;
			bReset = bReset || More.bReset;
			Wheel += More.Wheel;
		}
	};

	/** The view a field frame is shown with: the game's camera moved by the player's offsets. */
	struct FFieldPresentedView
	{
		FFieldView View;
		/** The offsets it shows. */
		FFieldViewOffsets Offsets;
		/** The eye and the target as the view has them (whole units), which tell a cut from a move. */
		std::array<int32, 3> Eye{};
		std::array<int32, 3> Target{};
		/** How many ordering-table entries farther the eye is than the game's: the far bucket moves back with it. */
		int32 FarBucketShift = 0;
		/** Whether the view is the game's own. */
		bool bGameView = true;
		/** View.View without the whole steps the game's look-at takes (UnroundedFieldView), where it is known. */
		FAffine Unrounded;
		bool bUnrounded = false;

		/** The far bucket the scripts set, moved back with the eye. */
		uint16 FarBucket(int32 GameFarBucket) const
		{
			return static_cast<uint16>(std::clamp<int32>(GameFarBucket + FarBucketShift, 0, std::numeric_limits<uint16>::max()));
		}
	};

	/**
	 * Whether a script's shot has the game's camera, as the original's state tells it: the camera on
	 * the scripts' paths (script mode); or, with the player locked out, a zoom, pitch or perspective
	 * ramp still moving, a shake, or a turn neither L1 nor R1 started. With the player in control
	 * those are the map's own camera (a pitch set by where the leader stands, a quake), and easing
	 * back to the follow is control returning.
	 */
	WELTALLCORE_API bool IsFieldCameraScripted(const FFieldCamera& Camera, bool bPlayerInControl);

	/** How far the view may turn from the game's camera each way (degrees, the way R1 turns). */
	struct FFieldTurnRange
	{
		float Down = -180.0f;
		float Up = 180.0f;
		/** No direction is closed: the view turns all the way round. */
		bool bAllRound = true;
	};

	/**
	 * How far the map lets the view turn from the game's camera: nowhere while the player's turns are
	 * locked or either direction mask holds every direction; otherwise as far each way as the
	 * directions next to the camera's own (the one it rests on or turns to) are neither forbidden nor
	 * left by themselves; all the way round when no direction is.
	 */
	WELTALLCORE_API FFieldTurnRange FieldViewTurnRange(const FFieldCamera& Camera);

	/**
	 * An eye moved about the target: its distance scaled by Zoom, raised by Tilt degrees (up to 85
	 * degrees above the target, or the game's own if it is higher), turned by Turn degrees about the
	 * vertical the way R1 turns the camera (16.16 points).
	 */
	WELTALLCORE_API FFieldPoint OffsetFieldEye(const FFieldPoint& Eye, const FFieldPoint& Target, float Zoom, float Tilt, float Turn);

	/** An eye and the target it looks at. */
	struct FFieldViewPoints
	{
		FFieldPoint Eye{};
		FFieldPoint Target{};
	};

	/**
	 * A view moved by the offsets: the eye and the target turned together about the vertical through
	 * Pivot (the followed actor, which so keeps its place on the screen where the game's target is held
	 * away from it), then the eye moved about the target by the zoom and the tilt (OffsetFieldEye).
	 */
	WELTALLCORE_API FFieldViewPoints OffsetFieldView(const FFieldPoint& Eye, const FFieldPoint& Target, const FFieldPoint& Pivot, float Zoom, float Tilt, float Turn);

	/**
	 * The player's camera on the field: offsets on the game's camera that only change what is shown
	 * (the logic keeps the game's camera). A script's shot eases them out, and so does a scene that
	 * keeps the player from moving for more than a moment while the offsets yield to scenes; control's
	 * return eases them back; the turn keeps within the directions the map allows; the steady follow, eased out and
	 * back with the offsets, keeps the view closer to the party than the game's camera does; the view is built from the same
	 * points the game's own is, a tick behind once the map has settled, as the original draws.
	 */
	class FFieldPlayerCamera
	{
	public:
		/** Easing the offsets out or back in takes this many ticks. */
		static constexpr int32 EaseTicks = 16;
		/** A turn back into the map's directions moves as the camera's own turns do: 45 degrees over 8 ticks. */
		static constexpr float ReturnTurnPerTick = 45.0f / 8.0f;
		/** The ticks the player may be locked out before the framing steps aside: a door or a short word does not move the view. */
		static constexpr int32 LockedMomentTicks = 30;
		/** The steady follow closes this share of its distance to the camera's goal each tick (the game's own: an eighth, and none within 8 units). */
		static constexpr int32 SteadyFollowDivisor = 3;
		/**
		 * A script's shot is shown with the camera's points averaged over this many ticks. The game's camera eases toward
		 * its goals only while they lie as many units away as its divisor or more, so a slow move goes a unit in one tick
		 * and none in the next: a wobble a sharp picture shows on every edge.
		 */
		static constexpr int32 SmoothedTicks = 4;
		/** A camera point moving farther than this in a tick (16.16) cut to another shot: its average starts again there. */
		static constexpr int32 SmoothedCut = 256 << 16;

		/** Offsets: the player's, which the view's input changes; they must outlive the camera. */
		explicit FFieldPlayerCamera(FFieldCameraOffsets& InOffsets) : Offsets(InOffsets) {}

		/** The player's input since the last frame, applied at the next. */
		void AddInput(const FFieldViewInput& Input) { Pending.Add(Input); }

		/**
		 * The view a frame of the field is shown with, once the frame's logic has run: the input is
		 * applied (while no script drives the camera), and the offsets step toward what the game's
		 * camera allows if the camera moved this frame (bTicked), or take it at once on a new map
		 * (bNewMap) and while the map settles.
		 */
		WELTALLCORE_API const FFieldPresentedView& Present(const FEventContext& Context, bool bTicked, bool bNewMap);

		/** The view's turn from the game's, as the view's yaws count it (4096 a turn): the controls and the facing follow it. */
		WELTALLCORE_API int16 ViewTurn() const;

		/** How much of the offsets shows: 0 while a script's shot has them eased out, 1 in the player's hands. */
		float Weight() const { return OffsetWeight; }
		/** The turn the view shows (degrees, the way R1 turns). */
		float AppliedTurn() const { return TurnApplied; }
		/** The view the last frame was shown with. */
		const FFieldPresentedView& ShownView() const { return Shown; }

		/**
		 * The offsets the next frame would show were the input since the last frame applied now: a display between
		 * frames shows the player's moves at once (the last frame's while nothing is pending or a script's shot has the
		 * camera).
		 */
		WELTALLCORE_API FFieldViewOffsets LiveOffsets() const;

		/**
		 * The last frame's view as other offsets would show it, without the whole steps the game's look-at takes (as the
		 * frame's own Unrounded): the display moves a frame from the offsets it shows to the live ones between two of
		 * these, where the game's steps, different for each, would shake a slow turn.
		 */
		WELTALLCORE_API FAffine UnroundedViewWithOffsets(const FFieldViewOffsets& Shows) const;

	private:
		/** What the last frame's view was built from, besides the offsets. */
		struct FShownBasis
		{
			FFieldCamera Camera;
			FFieldPoint Eye{};
			FFieldPoint Target{};
			FFieldPoint Pivot{};
			bool bOwnTick = false;
			bool bScripted = false;
			/** The steady follow has the camera's points away from the game's. */
			bool bFollowMoved = false;
			FFieldTurnRange Range;
		};

		void ApplyInput(const FFieldCamera& Camera, bool bScripted);
		/** How far across the ground the steady follow shows the camera from the game's, after its own step when the camera moved (16.16). */
		FFieldPoint StepSteadyFollow(const FFieldCamera& Camera, bool bTicked, bool bSnap);
		/** The eye and the target the basis is drawn from, moved by the offsets as they show. */
		FFieldViewPoints DrawnPoints(const FFieldViewOffsets& Shows) const;
		/** The view of the basis moved by the offsets as they show. */
		void BuildShown(FFieldPresentedView& Out, const FFieldViewOffsets& Shows) const;
		/** The camera's points averaged over its last ticks, after this one's when the camera moved (16.16). */
		std::array<FFieldPoint, 2> StepSmoothed(const FFieldPoint& Eye, const FFieldPoint& Target, bool bTicked, bool bSnap);

		FFieldCameraOffsets& Offsets;
		FFieldViewInput Pending;
		float OffsetWeight = 1.0f;
		/** The ticks in a row the player has been locked out. */
		int32 LockedTicks = 0;
		/** The player's turn as far as the map allows it now, and as the view shows it after the offsets' weight. */
		float TurnShown = 0.0f;
		float TurnApplied = 0.0f;
		/** The turn this frame's input added: a turn the player makes is shown at once. */
		float TurnInput = 0.0f;
		bool bStarted = false;
		/** Where the steady follow has the target across the ground (x and z, 16.16). */
		std::array<int32, 2> SteadyTarget{};
		/** The camera's eye and target before the shake at its last SmoothedTicks ticks, the latest first (16.16). */
		std::array<std::array<FFieldPoint, 2>, SmoothedTicks> SmoothedPoints{};
		/**
		 * The game's shaken eye and target and the view's pivot at the last tick, and at the tick before,
		 * which a settled map's frame is drawn from.
		 */
		FFieldPoint LastEye{};
		FFieldPoint LastTarget{};
		FFieldPoint LastPivot{};
		FFieldPoint DrawnEye{};
		FFieldPoint DrawnTarget{};
		FFieldPoint DrawnPivot{};
		FFieldPresentedView Shown;
		FShownBasis Basis;
	};
}
