#include "Battle/Show/BattleGearDress.h"

#include "Battle/Show/BattleRegurus.h"
#include "Game/CharacterLooks.h"
#include "Gears/GearPoseTrack.h"
#include "Gears/GearSkeleton.h"
#include "Gears/GearTextureRegions.h"
#include "Graphics/GteMatrix.h"
#include "Graphics/Vram.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace Weltall
{
	namespace
	{
		using FPairedBone = FBattleGearDress::FPairedBone;
		using FLeg = FBattleGearDress::FLeg;

		/**
		 * Fenrir's 56 bones on the first Heimdal's 45, by the pieces they carry: hips, waist, chest and head; the arms and
		 * shoulder pads side by side; the legs. Fenrir's grip takes Heimdal's fist on that side, or its open hand while the
		 * fist is put away; the strip its sword is held along is never shown.
		 */
		constexpr std::array<FPairedBone, 56> FenrirOnHeimdal = {{
			{},
			{1}, {16}, {16}, {31}, {31}, {31}, {20}, {20}, {21}, {39},
			{22}, {23, 24}, {23, 24}, {31}, {33}, {31}, {1}, {1}, {3}, {4},
			{36}, {5}, {5}, {1}, {1}, {9}, {10}, {38}, {11}, {11},
			{34}, {34}, {34}, {33}, {33}, {26}, {26}, {27}, {41}, {28},
			{31}, {14}, {6}, {15}, {12}, {23, 24, true}, {40}, {34}, {33}, {29},
			{30}, {42}, {32}, {6}, {12},
		}};

		constexpr std::array<FLeg, 2> FenrirLegs = {{
			{19, 20, 43, 54, {19, 42}, {20, 21, 22, 23}, 6, {6, 7}},
			{26, 27, 45, 55, {26, 44}, {27, 28, 29, 30}, 12, {12, 13}},
		}};

		constexpr std::size_t HeimdalBones = 45;

		constexpr FBattleGearDress::FPairing FenrirPairing{FCharacterLooks::Heimdal, FCharacterLooks::Fenrir, FBattleGearDress::NoScene, 0, HeimdalBones,
			FenrirOnHeimdal, FenrirLegs};

		/**
		 * Regurus' 60 bones on Vierge's 70: hips, waist and chest; the head and the wings it carries (on Vierge's back plate);
		 * the long hip skirts on her side blades; the arms, where Regurus hangs the forearm from the wrist and the shin from
		 * the ankle, so every bone of such a chain takes Vierge's forearm's or shin's turn and the chain comes back to the
		 * elbow or knee; the wand in Vierge's x<0 hand. Vierge's drones (her hidden bones) have no counterpart.
		 */
		constexpr std::array<FPairedBone, 60> RegurusOnVierge = {{
			{},
			{1}, {1}, {37}, {2}, {3}, {11}, {11}, {5}, {11}, {11},
			{6}, {7}, {14}, {14}, {9}, {14}, {14}, {31}, {25}, {26},
			{27}, {29}, {29}, {29}, {19}, {20}, {21}, {23}, {23}, {23},
			{41}, {44}, {11}, {12}, {12}, {12}, {3}, {4}, {14}, {15},
			{15}, {15}, {7}, {8}, {32}, {33}, {29}, {30}, {27}, {47},
			{25}, {47}, {23}, {24}, {21}, {46}, {19}, {46}, {24},
		}};

		constexpr std::array<FLeg, 2> RegurusLegs = {{
			{5, 6, 34, 34, {5, 38, 8}, {6, 9, 10, 33}, 12, {12, 12}},
			{12, 13, 40, 40, {12, 44, 15}, {13, 16, 17, 39}, 15, {15, 15}},
		}};

		constexpr std::size_t ViergeBones = 70;

		/** Vierge's sixteen drones, hidden on her back plate and side blades but while Aerods flies them: her own pieces. */
		constexpr std::array<uint8, 16> ViergesDrones = {48, 49, 50, 51, 52, 53, 54, 55, 57, 59, 63, 64, 65, 66, 67, 68};

		constexpr FBattleGearDress::FPairing RegurusPairing{BattleRegurus::Vierge, BattleRegurus::Regurus, FBattleGearDress::NoScene, 0, ViergeBones,
			RegurusOnVierge, RegurusLegs, FBattleGearDress::EHips::Scaled, false, ViergesDrones};

		/**
		 * Id's red Weltall (scene 74's first model, Elly's fight against it, built as Weltall-2 is) on Weltall's 52 bones:
		 * the spine on Weltall's, the shoulders' chains on its shoulders, the one hand on each side on its fist or open hand,
		 * the four wings on its two, the front skirt plates on its waist, the legs on its legs. It stands 1.28 times
		 * Weltall's height, so it makes the moves at its own size.
		 */
		constexpr std::array<FPairedBone, 62> RedWeltallOnWeltall = {{
			{}, {1}, {16}, {17}, {18}, {18}, {25}, {25}, {26}, {27},
			{28}, {30, 29}, {25}, {18}, {19}, {19}, {20}, {21}, {22}, {23, 24},
			{19}, {31}, {32}, {51}, {51}, {50}, {50}, {35}, {35}, {35},
			{2}, {3}, {3}, {5}, {5}, {6}, {7}, {9}, {10}, {10},
			{12}, {12}, {13}, {14}, {48}, {49}, {25}, {47}, {46}, {19},
			{51}, {51}, {51}, {50}, {50}, {50}, {38}, {5}, {8}, {42},
			{12}, {15},
		}};

		constexpr std::array<FLeg, 2> RedWeltallLegsOnWeltall = {{
			{32, 33, 35, 36, {32, 56}, {33, 34, 57}, 7, {7, 8}},
			{39, 40, 42, 43, {39, 59}, {40, 41, 60}, 14, {14, 15}},
		}};

		/**
		 * The red Weltall on Weltall-2's 83 bones, nearly bone for bone; its knee on Weltall-2's shin, past the short link
		 * Weltall-2's knee has, so the ankle goes where the shin points.
		 */
		constexpr std::array<FPairedBone, 62> RedWeltallOnWeltall2 = {{
			{}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9},
			{10}, {12, 11}, {13}, {14}, {15}, {16}, {17}, {18}, {19}, {20, 21},
			{22}, {23}, {24}, {33}, {34}, {25}, {26}, {41}, {73}, {76},
			{42}, {43}, {44}, {46}, {46}, {47}, {48}, {51}, {52}, {53},
			{55}, {55}, {56}, {57}, {60}, {61}, {13}, {63}, {62}, {22},
			{34}, {34}, {34}, {26}, {26}, {26}, {77}, {46}, {48}, {80},
			{55}, {57},
		}};

		constexpr std::array<FLeg, 2> RedWeltallLegsOnWeltall2 = {{
			{32, 33, 35, 36, {32, 56}, {33, 34, 57}, 48, {48, 49}},
			{39, 40, 42, 43, {39, 59}, {40, 41, 60}, 57, {57, 58}},
		}};

		/**
		 * The red gear of Id's second disc's fight (scene 70's first model) on Xenogears' 71 bones: Xenogears' own pieces
		 * coloured red, on fewer bones, its hips built higher on whole legs as Xenogears hovers on its own. The forearms hang
		 * from the wrists, so the whole chain takes the forearm's turn. Its great blades are Xenogears' own, which it keeps
		 * hidden but in some moves: they are out as Xenogears' are.
		 */
		constexpr std::array<FPairedBone, 37> RedGearOnXenogears = {{
			{}, {1}, {14}, {37}, {17}, {17}, {27}, {30}, {27}, {65},
			{42}, {38}, {40}, {41}, {20}, {21}, {22}, {23}, {23}, {23},
			{25}, {26}, {22}, {22}, {18}, {19}, {31}, {32}, {33}, {33},
			{33}, {35}, {36}, {32}, {32}, {28}, {29},
		}};

		constexpr uint8 WeltallGear = FCharacterLooks::Weltall;
		constexpr uint8 Weltall2Gear = FCharacterLooks::Weltall2;
		constexpr uint8 XenogearsGear = FCharacterLooks::Xenogears;
		constexpr int16 RedWeltallScene = 74;
		constexpr int16 RedGearScene = 70;
		constexpr std::size_t WeltallBones = 52;
		constexpr std::size_t Weltall2Bones = 83;
		constexpr std::size_t XenogearsBones = 71;

		constexpr FBattleGearDress::FPairing RedWeltallOverWeltall{WeltallGear, WeltallGear, RedWeltallScene, 0, WeltallBones, RedWeltallOnWeltall,
			RedWeltallLegsOnWeltall, FBattleGearDress::EHips::Scaled, true};
		constexpr FBattleGearDress::FPairing RedWeltallOverWeltall2{Weltall2Gear, Weltall2Gear, RedWeltallScene, 0, Weltall2Bones, RedWeltallOnWeltall2,
			RedWeltallLegsOnWeltall2};
		constexpr FBattleGearDress::FPairing RedGearOverXenogears{XenogearsGear, XenogearsGear, RedGearScene, 0, XenogearsBones, RedGearOnXenogears, {},
			FBattleGearDress::EHips::Raised};

		/** A grounded foot is the performer's within this many model units of its floor, fading to none this much higher. */
		constexpr double GroundedHeight = 100.0;
		constexpr double GroundedFade = 200.0;
		/** The parts are built lit, to be relit as they are drawn, as the objects' own are. */
		constexpr int32 PartLightMode = 2;

		using FTurn = std::array<std::array<double, 3>, 3>;
		using FPoint = std::array<double, 3>;

		FTurn Times(const FTurn& A, const FTurn& B)
		{
			FTurn Product{};
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				for (std::size_t Column = 0; Column < 3; ++Column)
				{
					Product[Row][Column] = A[Row][0] * B[0][Column] + A[Row][1] * B[1][Column] + A[Row][2] * B[2][Column];
				}
			}
			return Product;
		}

		FTurn Transposed(const FTurn& Turn)
		{
			FTurn Result{};
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				for (std::size_t Column = 0; Column < 3; ++Column)
				{
					Result[Row][Column] = Turn[Column][Row];
				}
			}
			return Result;
		}

		FPoint Apply(const FTurn& Turn, const FPoint& Point)
		{
			return {Turn[0][0] * Point[0] + Turn[0][1] * Point[1] + Turn[0][2] * Point[2], Turn[1][0] * Point[0] + Turn[1][1] * Point[1] + Turn[1][2] * Point[2],
				Turn[2][0] * Point[0] + Turn[2][1] * Point[1] + Turn[2][2] * Point[2]};
		}

		FPoint Plus(const FPoint& A, const FPoint& B) { return {A[0] + B[0], A[1] + B[1], A[2] + B[2]}; }
		FPoint Minus(const FPoint& A, const FPoint& B) { return {A[0] - B[0], A[1] - B[1], A[2] - B[2]}; }
		FPoint Scaled(const FPoint& A, double By) { return {A[0] * By, A[1] * By, A[2] * By}; }
		double Dot(const FPoint& A, const FPoint& B) { return A[0] * B[0] + A[1] * B[1] + A[2] * B[2]; }
		FPoint Cross(const FPoint& A, const FPoint& B) { return {A[1] * B[2] - A[2] * B[1], A[2] * B[0] - A[0] * B[2], A[0] * B[1] - A[1] * B[0]}; }
		double Length(const FPoint& A) { return std::sqrt(Dot(A, A)); }

		FTurn Identity()
		{
			return {{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
		}

		FTurn TurnOf(const std::array<float, 4>& Quaternion)
		{
			const double X = Quaternion[0], Y = Quaternion[1], Z = Quaternion[2], W = Quaternion[3];
			return {{{1.0 - 2.0 * (Y * Y + Z * Z), 2.0 * (X * Y - Z * W), 2.0 * (X * Z + Y * W)},
				{2.0 * (X * Y + Z * W), 1.0 - 2.0 * (X * X + Z * Z), 2.0 * (Y * Z - X * W)},
				{2.0 * (X * Z - Y * W), 2.0 * (Y * Z + X * W), 1.0 - 2.0 * (X * X + Y * Y)}}};
		}

		FTurn TurnOf(const FGteMatrix& Matrix)
		{
			FTurn Turn{};
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				for (std::size_t Column = 0; Column < 3; ++Column)
				{
					Turn[Row][Column] = Matrix.M[Row][Column] / static_cast<double>(FGteMatrix::One);
				}
			}
			return Turn;
		}

		FGteMatrix GteOf(const FTurn& Turn, const FPoint& Place)
		{
			FGteMatrix Matrix;
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				for (std::size_t Column = 0; Column < 3; ++Column)
				{
					const double Entry = std::round(Turn[Row][Column] * FGteMatrix::One);
					Matrix.M[Row][Column] = static_cast<int16>(std::clamp(Entry, double{std::numeric_limits<int16>::min()}, double{std::numeric_limits<int16>::max()}));
				}
				Matrix.T[Row] = static_cast<int32>(std::lround(Place[Row]));
			}
			return Matrix;
		}

		/** The smallest turn that takes direction From to direction To. */
		FTurn TurnBetween(const FPoint& From, const FPoint& To)
		{
			const FPoint A = Scaled(From, 1.0 / Length(From));
			const FPoint B = Scaled(To, 1.0 / Length(To));
			const FPoint Axis = Cross(A, B);
			const double Cosine = Dot(A, B);
			if (Cosine < -0.999999)
			{
				// Half a turn about any axis square to From.
				FPoint Square = Cross(A, std::abs(A[0]) < 0.9 ? FPoint{1.0, 0.0, 0.0} : FPoint{0.0, 1.0, 0.0});
				Square = Scaled(Square, 1.0 / Length(Square));
				FTurn Half{};
				for (std::size_t Row = 0; Row < 3; ++Row)
				{
					for (std::size_t Column = 0; Column < 3; ++Column)
					{
						Half[Row][Column] = 2.0 * Square[Row] * Square[Column] - (Row == Column ? 1.0 : 0.0);
					}
				}
				return Half;
			}
			const FTurn Skew = {{{0.0, -Axis[2], Axis[1]}, {Axis[2], 0.0, -Axis[0]}, {-Axis[1], Axis[0], 0.0}}};
			const FTurn SkewSquared = Times(Skew, Skew);
			FTurn Turn = Identity();
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				for (std::size_t Column = 0; Column < 3; ++Column)
				{
					Turn[Row][Column] += Skew[Row][Column] + SkewSquared[Row][Column] / (1.0 + Cosine);
				}
			}
			return Turn;
		}

		/** A skeleton's bones as its model was built: each one's turn and place in the model's frame. */
		struct FBuiltPose
		{
			std::vector<FTurn> Turns;
			std::vector<FPoint> Places;
			/** Each bone's place in its parent's frame. */
			std::vector<FPoint> Locals;
		};

		TResult<FBuiltPose> BuiltPose(const FGearModelPack& Pack, FGearMotionPack& Motion)
		{
			TResult<FGearPose> Pose = ReadGearPose(Pack.Skeleton, Motion, 0);
			if (!Pose)
			{
				return Pose.Error();
			}
			FBuiltPose Built;
			const std::size_t Count = Pack.Skeleton.size() + 1;
			Built.Turns.assign(Count, Identity());
			Built.Places.assign(Count, FPoint{});
			Built.Locals.assign(Count, FPoint{});
			for (int32 Bone = 1; static_cast<std::size_t>(Bone) < Count && static_cast<std::size_t>(Bone) < Pose->size(); ++Bone)
			{
				const std::size_t At = static_cast<std::size_t>(Bone);
				Built.Turns[At] = TurnOf(GearBoneModelTurn(*Pose, Pack.Skeleton, Bone));
				const std::array<float, 3> Place = GearBoneModelPlace(*Pose, Pack.Skeleton, Bone);
				Built.Places[At] = {Place[0], Place[1], Place[2]};
				const std::array<float, 3>& Local = (*Pose)[At].Position;
				Built.Locals[At] = {Local[0], Local[1], Local[2]};
			}
			return Built;
		}

		/** A part's vertices, or nothing for a bone without one. */
		std::vector<FPoint> PartPoints(const FGearModelPack& Pack, std::size_t Bone)
		{
			std::vector<FPoint> Points;
			const uint16 Part = Pack.Skeleton[Bone - 1].Part;
			if (Part < Pack.Model.Parts.size())
			{
				for (const FGteVector& Vertex : Pack.Model.Parts[Part].Vertices)
				{
					Points.push_back({static_cast<double>(Vertex[0]), static_cast<double>(Vertex[1]), static_cast<double>(Vertex[2])});
				}
			}
			return Points;
		}

		/** How far down (the model's y grows downward) a piece's lowest point lies from its bone, turned so. */
		double Lowest(std::span<const FPoint> Points, const FTurn& Turn)
		{
			double Low = -std::numeric_limits<double>::infinity();
			for (const FPoint& Point : Points)
			{
				Low = std::max(Low, Apply(Turn, Point)[1]);
			}
			return Low;
		}

		int32 ParentOf(const FGearModelPack& Pack, std::size_t Bone)
		{
			const uint16 Parent = Pack.Skeleton[Bone - 1].Parent;
			return Parent == 0xFFFF ? 0 : Parent + 1;
		}

		bool Holds(std::size_t Count, uint8 Bone)
		{
			return Bone > 0 && Bone < Count;
		}
	}

	const FBattleGearDress::FPairing* FBattleGearDress::FindPairing(uint8 PerformerGear, uint8 WearerGear)
	{
		for (const FPairing* Pairing : {&FenrirPairing, &RegurusPairing})
		{
			if (PerformerGear == Pairing->PerformerGear && WearerGear == Pairing->WearerGear)
			{
				return Pairing;
			}
		}
		return nullptr;
	}

	const FBattleGearDress::FPairing* FBattleGearDress::IdsPairing(uint8 FeisGear)
	{
		for (const FPairing* Pairing : {&RedWeltallOverWeltall, &RedWeltallOverWeltall2, &RedGearOverXenogears})
		{
			if (FeisGear == Pairing->PerformerGear)
			{
				return Pairing;
			}
		}
		return nullptr;
	}

	TResult<FBattleGearDress> FBattleGearDress::Make(const FPairing& Pairing, const FGearModelPack& Performer, FGearMotionPack& PerformerMotion,
		FGearModelPack&& Wearer, FGearMotionPack& WearerMotion, const FGearTexturePlace* Place)
	{
		const std::size_t PerformerCount = Performer.Skeleton.size() + 1;
		const std::size_t WearerCount = Wearer.Skeleton.size() + 1;
		const std::string Pair = "gear " + std::to_string(Pairing.WearerGear) + " over gear " + std::to_string(Pairing.PerformerGear);
		if (PerformerCount != Pairing.PerformerBones || WearerCount != Pairing.Bones.size())
		{
			return MakeError(Pair + ": the skeletons are not the ones the pairing was drawn up for");
		}
		for (std::size_t Bone = 1; Bone < WearerCount; ++Bone)
		{
			const FPairedBone& Paired = Pairing.Bones[Bone];
			if (!Holds(PerformerCount, Paired.Driver) || (Paired.StandIn != 0 && !Holds(PerformerCount, Paired.StandIn)))
			{
				return MakeError(Pair + ": bone " + std::to_string(Bone) + " follows a bone the performer lacks");
			}
		}
		for (const uint8 Bone : Pairing.PerformerPieces)
		{
			if (!Holds(PerformerCount, Bone) || Performer.Skeleton[Bone - 1].Part >= Performer.Model.Parts.size())
			{
				return MakeError(Pair + ": the performer's bone " + std::to_string(Bone) + " has no piece to keep");
			}
		}
		if (!Pairing.PerformerPieces.empty() && (Performer.Header.Flags & GearModelFlags::AbsoluteTextures) != 0)
		{
			return MakeError(Pair + ": the performer's textures go where its file says, not into the port's rows");
		}
		for (const FLeg& Leg : Pairing.Legs)
		{
			if (Wearer.Skeleton[Leg.Sole - 1].Part >= Wearer.Model.Parts.size() || Performer.Skeleton[Leg.PerformerSoles[0] - 1].Part >= Performer.Model.Parts.size()
				|| Performer.Skeleton[Leg.PerformerSoles[1] - 1].Part >= Performer.Model.Parts.size())
			{
				return MakeError(Pair + ": a foot has no piece to stand on");
			}
		}
		TResult<FBuiltPose> PerformerPose = BuiltPose(Performer, PerformerMotion);
		if (!PerformerPose)
		{
			return PerformerPose.Error();
		}
		TResult<FBuiltPose> WearerPose = BuiltPose(Wearer, WearerMotion);
		if (!WearerPose)
		{
			return WearerPose.Error();
		}

		FBattleGearDress Dress;
		Dress.Paired = &Pairing;
		Dress.PerformerSkeleton = Performer.Skeleton;
		Dress.Fits.resize(WearerCount);
		for (std::size_t Bone = 1; Bone < WearerCount; ++Bone)
		{
			const FPairedBone& Paired = Pairing.Bones[Bone];
			FBoneFit& Fit = Dress.Fits[Bone];
			Fit.FromDriver = Times(Transposed(PerformerPose->Turns[Paired.Driver]), WearerPose->Turns[Bone]);
			Fit.FromStandIn = Paired.StandIn != 0 ? Times(Transposed(PerformerPose->Turns[Paired.StandIn]), WearerPose->Turns[Bone]) : Fit.FromDriver;
			Fit.Place = WearerPose->Locals[Bone];
		}
		const double PerformerHips = PerformerPose->Places[1][1];
		if (Pairing.Hips == EHips::Raised)
		{
			Dress.HipRaise = WearerPose->Places[1][1] - PerformerHips;
		}
		else
		{
			Dress.HipScale = PerformerHips != 0.0 ? WearerPose->Places[1][1] / PerformerHips : 1.0;
		}
		Dress.MoveScale = Pairing.bScaledMoves ? Dress.HipScale : 1.0;
		for (const FLeg& Leg : Pairing.Legs)
		{
			FLegFit Fit;
			Fit.AnkleFromFoot = Minus(WearerPose->Places[Leg.Ankle], Scaled(PerformerPose->Places[Leg.PerformerFoot], Dress.MoveScale));
			Fit.Reach = Length(Minus(WearerPose->Places[Leg.Ankle], WearerPose->Places[Leg.Hip]));
			Fit.PerformerFootBuilt = PerformerPose->Turns[Leg.PerformerFoot];
			Fit.SolePoints = PartPoints(Wearer, Leg.Sole);
			Fit.PerformerSolePoints = {PartPoints(Performer, Leg.PerformerSoles[0]), PartPoints(Performer, Leg.PerformerSoles[1])};
			double PerformerLow = -std::numeric_limits<double>::infinity();
			for (std::size_t Sole = 0; Sole < Leg.PerformerSoles.size(); ++Sole)
			{
				const uint8 Bone = Leg.PerformerSoles[Sole];
				PerformerLow = std::max(PerformerLow, Lowest(Fit.PerformerSolePoints[Sole], PerformerPose->Turns[Bone]) + PerformerPose->Places[Bone][1]);
			}
			Fit.SoleGap = Lowest(Fit.SolePoints, WearerPose->Turns[Leg.Sole]) + WearerPose->Places[Leg.Sole][1] - PerformerLow * Dress.MoveScale;
			Dress.LegFits.push_back(std::move(Fit));
		}

		if (Place != nullptr)
		{
			std::vector<FGearBone> Bones;
			MakeGearSkeleton(Wearer.Skeleton, Bones);
			const FGearTexturePlace* Placed = (Wearer.Header.Flags & GearModelFlags::AbsoluteTextures) == 0 ? Place : nullptr;
			TResult<std::vector<FGearBonePackets>> Packets = BuildGearPackets(Wearer.Model, Bones, PartLightMode, Placed, FGte{});
			if (!Packets)
			{
				return MakeError(Pair + "'s parts: " + Packets.Error().Message);
			}
			Dress.BonePackets = std::move(*Packets);
			const std::vector<FTextureRegion> Regions = GearTextureRegions(Wearer.Model, {}, Placed);
			std::vector<FTexturePictureSeed> Pictures = GearTextureSeeds(Wearer, Placed, Regions);
			if (!Pairing.PerformerPieces.empty())
			{
				if (auto Error = Dress.KeepPerformerPieces(Pairing, Performer, *Place, Pictures))
				{
					return MakeError(Pair + "'s own pieces: " + Error->Message);
				}
			}
			Dress.Seeds = std::make_shared<const std::vector<FTexturePictureSeed>>(std::move(Pictures));
		}
		if (Pairing.WearerScene == RedWeltallScene)
		{
			std::vector<FGteMatrix> Built(WearerCount);
			for (std::size_t Bone = 1; Bone < WearerCount; ++Bone)
			{
				Built[Bone] = GteOf(WearerPose->Turns[Bone], WearerPose->Places[Bone]);
			}
			if (const auto Error = Dress.FitRedWeltallJoints(Wearer, Built))
			{
				return *Error;
			}
		}
		Dress.Wearer = std::make_shared<const FGearModelPack>(std::move(Wearer));
		return Dress;
	}

	std::optional<FError> FBattleGearDress::KeepPerformerPieces(const FPairing& Pairing, const FGearModelPack& Performer, const FGearTexturePlace& Place,
		std::vector<FTexturePictureSeed>& Pictures)
	{
		// The performer's textures the wearer's would overwrite at Place go to the same place in the port's rows.
		KeptPlace = {Place.X, static_cast<int16>(Place.Y + FVram::Height), Place.ClutX, static_cast<int16>(Place.ClutY + FVram::Height)};
		KeptTextures = Performer.Textures;
		std::vector<FGearBone> Bones;
		MakeGearSkeleton(Performer.Skeleton, Bones);
		TResult<std::vector<FGearBonePackets>> Packets = BuildGearPackets(Performer.Model, Bones, PartLightMode, &KeptPlace, FGte{});
		if (!Packets)
		{
			return Packets.Error();
		}
		FMdlModel Kept;
		for (FGearBonePackets& Built : *Packets)
		{
			if (std::find(Pairing.PerformerPieces.begin(), Pairing.PerformerPieces.end(), Built.Bone) != Pairing.PerformerPieces.end())
			{
				Kept.Parts.push_back(Performer.Model.Parts[Built.Part]);
				KeptPackets.push_back(std::move(Built));
			}
		}
		const std::vector<FTextureRegion> Regions = GearTextureRegions(Kept, {}, &KeptPlace);
		const std::vector<FTexturePictureSeed> KeptSeeds = GearTextureSeeds(Performer, &KeptPlace, Regions);
		Pictures.insert(Pictures.end(), KeptSeeds.begin(), KeptSeeds.end());
		return std::nullopt;
	}

	FGearPose FBattleGearDress::PoseOver(const FGearPose& Pose, std::span<const uint8> Shown, std::vector<uint8>* WearerShown) const
	{
		std::vector<FGearBone> Performer;
		MakeGearSkeleton(PerformerSkeleton, Performer);
		for (std::size_t Bone = 1; Bone < Performer.size() && Bone < Shown.size(); ++Bone)
		{
			Performer[Bone].bShown = Shown[Bone];
		}
		ApplyGearPose(Pose, Performer);
		ComputeGearBones(Performer, FGteMatrix::One);
		std::vector<FGearBone> Worn;
		Place(Performer, Worn);
		if (WearerShown != nullptr)
		{
			WearerShown->resize(Worn.size());
			for (std::size_t Bone = 0; Bone < Worn.size(); ++Bone)
			{
				(*WearerShown)[Bone] = Worn[Bone].bShown;
			}
		}
		return GearPoseOfPlaced(Worn);
	}

	void FBattleGearDress::PlaceBones(std::span<const FGearBone> Performer, std::span<const FTurn> Turns, double Drop, std::vector<FPoint>& Places) const
	{
		const std::array<int32, 3>& Hips = Performer[1].World.T;
		Places.assign(Turns.size(), FPoint{});
		for (std::size_t Bone = 1; Bone < Turns.size(); ++Bone)
		{
			const int32 Parent = ParentOf(*Wearer, Bone);
			if (Parent != 0)
			{
				const std::size_t Above = static_cast<std::size_t>(Parent);
				Places[Bone] = Plus(Places[Above], Apply(Turns[Above], Fits[Bone].Place));
			}
			else if (Bone == 1)
			{
				Places[Bone] = {static_cast<double>(Hips[0]) * HipScale, static_cast<double>(Hips[1]) * HipScale + HipRaise + Drop, static_cast<double>(Hips[2]) * HipScale};
			}
			else
			{
				Places[Bone] = Fits[Bone].Place;
			}
		}
	}

	void FBattleGearDress::Place(std::span<const FGearBone> Performer, std::vector<FGearBone>& Bones) const
	{
		const std::size_t Count = Fits.size();
		if (Bones.size() != Count)
		{
			MakeGearSkeleton(Wearer->Skeleton, Bones);
		}
		std::vector<FTurn> Turns(Count, Identity());
		for (std::size_t Bone = 1; Bone < Count; ++Bone)
		{
			const FPairedBone& Follows = Paired->Bones[Bone];
			const bool bStandIn = Follows.StandIn != 0 && Performer[Follows.Driver].bShown == 0;
			Turns[Bone] = Times(TurnOf(Performer[bStandIn ? Follows.StandIn : Follows.Driver].World), bStandIn ? Fits[Bone].FromStandIn : Fits[Bone].FromDriver);
			Bones[Bone].bShown = !Follows.bNeverShown && (Performer[Follows.Driver].bShown != 0 || (Follows.StandIn != 0 && Performer[Follows.StandIn].bShown != 0)) ? 1 : 0;
		}

		// Where each ankle goes: carried with the performer's foot from where the two stood as built, then up or down so the
		// sole's lowest point is the performer's, as near as they were built (the sole's piece wherever its bone hangs from).
		std::vector<FPoint> Places;
		PlaceBones(Performer, Turns, 0.0, Places);
		std::vector<FPoint> Targets;
		std::vector<double> Grounded;
		for (std::size_t Index = 0; Index < Paired->Legs.size(); ++Index)
		{
			const FLeg& Leg = Paired->Legs[Index];
			const FLegFit& Fit = LegFits[Index];
			const FGearBone& Foot = Performer[Leg.PerformerFoot];
			const FTurn FootSinceBuilt = Times(TurnOf(Foot.World), Transposed(Fit.PerformerFootBuilt));
			const FPoint FootPlace = {static_cast<double>(Foot.World.T[0]), static_cast<double>(Foot.World.T[1]), static_cast<double>(Foot.World.T[2])};
			FPoint Target = Plus(Scaled(FootPlace, MoveScale), Apply(FootSinceBuilt, Fit.AnkleFromFoot));
			double PerformerLow = -std::numeric_limits<double>::infinity();
			for (std::size_t Sole = 0; Sole < Leg.PerformerSoles.size(); ++Sole)
			{
				const FGearBone& Piece = Performer[Leg.PerformerSoles[Sole]];
				PerformerLow = std::max(PerformerLow, Lowest(Fit.PerformerSolePoints[Sole], TurnOf(Piece.World)) + static_cast<double>(Piece.World.T[1]));
			}
			const double SoleBelowAnkle = Lowest(Fit.SolePoints, Turns[Leg.Sole]) + Places[Leg.Sole][1] - Places[Leg.Ankle][1];
			Target[1] = PerformerLow * MoveScale + Fit.SoleGap - SoleBelowAnkle;
			Targets.push_back(Target);
			Grounded.push_back(std::clamp((PerformerLow + GroundedHeight + GroundedFade) / GroundedFade, 0.0, 1.0));
		}

		// A leg stretches no straighter than it was built: a grounded one short of its foot lowers the hips until it reaches.
		double Drop = 0.0;
		for (std::size_t Index = 0; Index < Paired->Legs.size(); ++Index)
		{
			const FLeg& Leg = Paired->Legs[Index];
			const double Reach = LegFits[Index].Reach;
			const FPoint Toward = Minus(Targets[Index], Places[Leg.Hip]);
			const double Across = Toward[0] * Toward[0] + Toward[2] * Toward[2];
			if (Across < Reach * Reach && Length(Toward) > Reach)
			{
				Drop = std::max(Drop, Grounded[Index] * (Toward[1] - std::sqrt(Reach * Reach - Across)));
			}
		}
		if (Drop != 0.0)
		{
			PlaceBones(Performer, Turns, Drop, Places);
		}

		// Each leg bent in the plane its knee already bends in so its ankle is at its target.
		for (std::size_t Index = 0; Index < Paired->Legs.size(); ++Index)
		{
			const FLeg& Leg = Paired->Legs[Index];
			const FPoint Hip = Places[Leg.Hip];
			const FPoint Thigh = Minus(Places[Leg.Knee], Hip);
			const FPoint Shin = Minus(Places[Leg.Ankle], Places[Leg.Knee]);
			const double Upper = Length(Thigh);
			const double Lower = Length(Shin);
			const FPoint Toward = Minus(Targets[Index], Hip);
			const double Distance = std::clamp(Length(Toward), std::abs(Upper - Lower) + 1.0, std::min(LegFits[Index].Reach, Upper + Lower));
			const FPoint Axis = Scaled(Toward, 1.0 / Length(Toward));
			FPoint Bend = Minus(Thigh, Scaled(Axis, Dot(Thigh, Axis)));
			Bend = Length(Bend) > 1e-6 ? Scaled(Bend, 1.0 / Length(Bend)) : FPoint{0.0, 0.0, 1.0};
			const double Along = (Upper * Upper - Lower * Lower + Distance * Distance) / (2.0 * Distance);
			const FPoint Knee = Plus(Plus(Hip, Scaled(Axis, Along)), Scaled(Bend, std::sqrt(std::max(Upper * Upper - Along * Along, 0.0))));
			const FTurn ThighTurn = TurnBetween(Thigh, Minus(Knee, Hip));
			const FTurn ShinTurn = Times(TurnBetween(Apply(ThighTurn, Shin), Minus(Plus(Hip, Scaled(Axis, Distance)), Knee)), ThighTurn);
			for (const uint8 Bone : Leg.Thigh)
			{
				if (Bone != 0)
				{
					Turns[Bone] = Times(ThighTurn, Turns[Bone]);
				}
			}
			for (const uint8 Bone : Leg.Shin)
			{
				if (Bone != 0)
				{
					Turns[Bone] = Times(ShinTurn, Turns[Bone]);
				}
			}
			PlaceBones(Performer, Turns, Drop, Places);
		}

		for (std::size_t Bone = 1; Bone < Count; ++Bone)
		{
			Bones[Bone].World = GteOf(Turns[Bone], Places[Bone]);
		}
		JoinPieces(Bones);
	}
}
