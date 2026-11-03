#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Gears/GearDress.h"
#include "Gears/GearFiles.h"
#include "Gears/GearPackets.h"
#include "Gears/GearPoseTrack.h"
#include "Graphics/TexturePictures.h"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	struct FGearBone;

	/**
	 * One gear's model worn over another gear's moving skeleton: Fenrir's pieces on the first Heimdal's bones as it plays
	 * Heimdal's moves, so that Fenrir without its sword fights with Heimdal's fists and kicks; Regurus' on Vierge's, so
	 * that Elly fights in it with Vierge's moves; Id's red gears on Fei's, so that Fei as Id fights in them with his own
	 * gear's moves. Each piece takes the turn its counterpart has made from the pose both models were built in, the wearer
	 * keeps its own bone lengths, and each leg is bent so that its foot stands where the performer's does. Pieces of the
	 * performer's own that nothing of the wearer's stands for can be kept, drawn on the performer's bones as they stand
	 * and textured from the port's rows. Only the drawing changes: the object that moves, its effects and its sounds are
	 * the performer's.
	 */
	class FBattleGearDress final : public IGearDress
	{
	public:
		/** A wearer's bone: the performer's bone whose turn it takes, and the one it takes while that one is hidden. */
		struct FPairedBone
		{
			uint8 Driver = 0;
			uint8 StandIn = 0;
			bool bNeverShown = false;
		};

		/** A wearer's leg and the performer's foot it stands in. */
		struct FLeg
		{
			uint8 Hip = 0;
			uint8 Knee = 0;
			uint8 Ankle = 0;
			/** The bone of the piece the leg stands on. */
			uint8 Sole = 0;
			/** The bones that turn with the thigh and with the shin when the leg is bent (0 for none). */
			std::array<uint8, 3> Thigh{};
			std::array<uint8, 4> Shin{};
			uint8 PerformerFoot = 0;
			/** The performer's pieces whose lowest point the sole matches. */
			std::array<uint8, 2> PerformerSoles{};
		};

		static constexpr int16 NoScene = -1;

		/** How the wearer's hips follow the performer's. */
		enum class EHips : uint8
		{
			/** Their place scaled by the two hips' heights as built. */
			Scaled,
			/** Raised by the difference of those heights: a gear built hovering higher on legs as long. */
			Raised,
		};

		/** Which bones of a wearer's skeleton follow which of a performer's, for the models they were drawn up for. */
		struct FPairing
		{
			uint8 PerformerGear = 0;
			/** The wearer: a gear's model, or, for a model of no party gear, a battle scene's model entry. */
			uint8 WearerGear = 0;
			int16 WearerScene = NoScene;
			uint8 WearerEntry = 0;
			std::size_t PerformerBones = 0;
			std::span<const FPairedBone> Bones;
			std::span<const FLeg> Legs;
			EHips Hips = EHips::Scaled;
			/** The feet's places and heights scaled as the hips' are, so a wearer of another size makes the moves at its own. */
			bool bScaledMoves = false;
			/** The performer's bones whose own pieces are drawn too, whenever the performer shows them. */
			std::span<const uint8> PerformerPieces{};
		};

		/** The pairing of PerformerGear's skeleton and WearerGear's model, if the port has one. */
		WELTALLCORE_API static const FPairing* FindPairing(uint8 PerformerGear, uint8 WearerGear);

		/**
		 * The pairing that draws Fei's gear FeisGear as Id's: the red Weltall of his first disc's fights over Weltall
		 * and Weltall-2, the red gear of his second disc's over Xenogears; none for another gear.
		 */
		WELTALLCORE_API static const FPairing* IdsPairing(uint8 FeisGear);

		/**
		 * The dress of a wearer's model over a performer's skeleton as Pairing pairs them, each from its packs as the battle
		 * reads them (each motion pack's record 0 the pose its model was built in), its parts and texture regions built for
		 * the texture place Place (none without one: a user that draws the wearer's model itself only places its bones). An
		 * error for models other than the ones the pairing was drawn up for.
		 */
		WELTALLCORE_API static TResult<FBattleGearDress> Make(const FPairing& Pairing, const FGearModelPack& Performer, FGearMotionPack& PerformerMotion,
			FGearModelPack&& Wearer, FGearMotionPack& WearerMotion, const FGearTexturePlace* Place);

		const FGearModelPack& Model() const override { return *Wearer; }
		/** The pairing it was made by. */
		const FPairing& Pairing() const { return *Paired; }
		/** Each of the wearer's parts as its bone draws it, in bone order. */
		const std::vector<FGearBonePackets>& Packets() const override { return BonePackets; }
		/** The wearer's texture regions where Place puts its textures, then the kept pieces', with their loaded colours and keys. */
		const std::shared_ptr<const std::vector<FTexturePictureSeed>>& TextureSeeds() const override { return Seeds; }

		/**
		 * The performer's own pieces the pairing keeps, each as its bone draws it, in bone order: their pages and CLUTs in
		 * the port's rows, where PerformerTextures go at PerformerPlace (Place moved down past the console's rows), so that
		 * the wearer's textures at Place leave them whole.
		 */
		const std::vector<FGearBonePackets>& PerformerPackets() const { return KeptPackets; }
		const FGearTexturePlace& PerformerPlace() const { return KeptPlace; }
		std::span<const FGearTextureBlock> PerformerTextures() const { return KeptTextures; }

		/**
		 * The wearer's bones placed over the performer's as they stand: each one's matrix in the model's frame, its part,
		 * and whether it is shown (while its counterpart is). Bones is made the wearer's skeleton when it is not.
		 */
		WELTALLCORE_API void Place(std::span<const FGearBone> Performer, std::vector<FGearBone>& Bones) const override;

		/**
		 * The wearer's pose over the performer standing in Pose (the performer's bones' local turns and places, the root
		 * left out), and which of the wearer's bones are shown while the performer's are as Shown has them (null: all).
		 */
		WELTALLCORE_API FGearPose PoseOver(const FGearPose& Pose, std::span<const uint8> Shown = {}, std::vector<uint8>* WearerShown = nullptr) const;

	private:
		using FTurn = std::array<std::array<double, 3>, 3>;
		using FPoint = std::array<double, 3>;

		struct FBoneFit
		{
			/** The wearer's bone's turn in the built pose against its driver's, and against its stand-in's. */
			FTurn FromDriver{};
			FTurn FromStandIn{};
			/** Its place in its parent's frame. */
			FPoint Place{};
		};

		struct FLegFit
		{
			/** The wearer's ankle from the performer's foot, and the performer's foot's turn, in the built pose. */
			FPoint AnkleFromFoot{};
			FTurn PerformerFootBuilt{};
			/** From the wearer's hip to its ankle as built: as straight as the leg goes. */
			double Reach = 0.0;
			std::vector<FPoint> SolePoints;
			std::array<std::vector<FPoint>, 2> PerformerSolePoints;
			/** How far the wearer's sole stood below the performer's as both were built (a few units either way). */
			double SoleGap = 0.0;
		};

		struct FJointFit
		{
			uint8 Parent = 0;
			uint8 Child = 0;
			FPoint ParentEnd{};
			FPoint ChildEnd{};
			FPoint ParentAxis{};
			FPoint ChildAxis{};
			double Radius = 0.0;
			bool bMoveParent = false;
			std::vector<uint8> Moved;
		};

		/** Fit the red Weltall's open limb ends, keeping their pieces rigid and the solved feet in place. */
		std::optional<FError> FitRedWeltallJoints(const FGearModelPack& Model, std::span<const FGteMatrix> Built);
		void JoinPieces(std::vector<FGearBone>& Bones) const;

		/**
		 * The packets of the performer's pieces Pairing keeps, built for its textures' place in the port's rows, and the
		 * seeds of their texture regions after Pictures.
		 */
		std::optional<FError> KeepPerformerPieces(const FPairing& Pairing, const FGearModelPack& Performer, const FGearTexturePlace& Place,
			std::vector<FTexturePictureSeed>& Pictures);
		/** The bones' places down the skeleton from their turns, the hips at the performer's (scaled or raised) lowered by Drop. */
		void PlaceBones(std::span<const FGearBone> Performer, std::span<const FTurn> Turns, double Drop, std::vector<FPoint>& Places) const;

		const FPairing* Paired = nullptr;
		std::vector<FGearSkeletonEntry> PerformerSkeleton;
		std::shared_ptr<const FGearModelPack> Wearer;
		std::vector<FGearBonePackets> BonePackets;
		std::shared_ptr<const std::vector<FTexturePictureSeed>> Seeds;
		std::vector<FGearBonePackets> KeptPackets;
		FGearTexturePlace KeptPlace;
		std::vector<FGearTextureBlock> KeptTextures;
		std::vector<FBoneFit> Fits;
		std::vector<FLegFit> LegFits;
		std::vector<FJointFit> JointFits;
		/** The wearer's hips' height over the performer's, both as built, and the height a raised wearer's hips go up by. */
		double HipScale = 1.0;
		double HipRaise = 0.0;
		/** What the performer's feet's places and heights are scaled by. */
		double MoveScale = 1.0;
	};
}
