#include "Battle/Show/BattleGearDress.h"

#include "Gears/GearSkeleton.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>

namespace Weltall
{
	namespace
	{
		using FPoint = std::array<double, 3>;

		FPoint Turned(const FGteMatrix& Matrix, const FPoint& Point)
		{
			FPoint Result{};
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				for (std::size_t Column = 0; Column < 3; ++Column)
				{
					Result[Row] += Matrix.M[Row][Column] * Point[Column] / FGteMatrix::One;
				}
			}
			return Result;
		}

		FPoint Placed(const FGteMatrix& Matrix, const FPoint& Point)
		{
			FPoint Result = Turned(Matrix, Point);
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Result[Axis] += Matrix.T[Axis];
			}
			return Result;
		}

		double SquaredLength(const FPoint& Point)
		{
			return Point[0] * Point[0] + Point[1] * Point[1] + Point[2] * Point[2];
		}
	}

	std::optional<FError> FBattleGearDress::FitRedWeltallJoints(const FGearModelPack& Model, std::span<const FGteMatrix> Built)
	{
		bool bValid = true;
		const auto Fit = [&](uint8 Parent, std::initializer_list<uint16> ParentEnd, uint8 Child, std::initializer_list<uint16> ChildEnd,
			uint8 Branch, bool bMoveParent = false) {
			FJointFit Joint;
			Joint.Parent = Parent;
			Joint.Child = Child;
			Joint.bMoveParent = bMoveParent;
			const auto End = [&](uint8 Bone, std::initializer_list<uint16> Indices, FPoint& Center) {
				const uint16 Part = Model.Skeleton[Bone - 1].Part;
				if (Part >= Model.Model.Parts.size())
				{
					bValid = false;
					return 0.0;
				}
				const auto& Vertices = Model.Model.Parts[Part].Vertices;
				if (std::any_of(Indices.begin(), Indices.end(), [&](uint16 Index) { return Index >= Vertices.size(); }))
				{
					bValid = false;
					return 0.0;
				}
				for (const uint16 Index : Indices)
				{
					for (std::size_t Axis = 0; Axis < 3; ++Axis)
					{
						Center[Axis] += static_cast<double>(Vertices[Index][Axis]) / static_cast<double>(Indices.size());
					}
				}
				double RadiusSquared = 0.0;
				for (const uint16 Index : Indices)
				{
					const FPoint Offset = {Vertices[Index][0] - Center[0], Vertices[Index][1] - Center[1], Vertices[Index][2] - Center[2]};
					RadiusSquared = std::max(RadiusSquared, SquaredLength(Offset));
				}
				return std::sqrt(RadiusSquared);
			};
			const double ParentRadius = End(Parent, ParentEnd, Joint.ParentEnd);
			Joint.Radius = std::min(ParentRadius, End(Child, ChildEnd, Joint.ChildEnd));
			// The limbs point down as built. Each end keeps that axis in its own piece's frame.
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Joint.ParentAxis[Axis] = static_cast<double>(Built[Parent].M[1][Axis]) / FGteMatrix::One;
				Joint.ChildAxis[Axis] = static_cast<double>(Built[Child].M[1][Axis]) / FGteMatrix::One;
			}
			for (std::size_t Bone = 1; Bone <= Model.Skeleton.size(); ++Bone)
			{
				std::size_t Above = Bone;
				while (Above != 0 && Above != Branch)
				{
					const uint16 ParentBone = Model.Skeleton[Above - 1].Parent;
					Above = ParentBone == 0xFFFF ? 0 : static_cast<std::size_t>(ParentBone) + 1;
				}
				if (Above == Branch)
				{
					Joint.Moved.push_back(static_cast<uint8>(Bone));
				}
			}
			JointFits.push_back(std::move(Joint));
		};

		// The facing rims, rather than the shoulder horns, elbow points or shin blades.
		Fit(12, {13, 14, 15, 16}, 45, {5, 6, 7, 8, 9}, 8);
		Fit(45, {0, 1, 2, 3, 4}, 44, {1, 5, 8}, 9);
		Fit(44, {2, 4, 11, 13, 14}, 11, {0, 9, 10, 11, 12}, 10);
		Fit(20, {13, 14, 15, 16}, 48, {5, 6, 7, 8, 9}, 16);
		Fit(48, {0, 1, 2, 3, 4}, 47, {1, 5, 8}, 17);
		Fit(47, {2, 4, 11, 13, 14}, 19, {0, 9, 10, 11, 12}, 18);
		Fit(56, {4, 5, 8, 10}, 34, {14, 17, 20, 21}, 56, true);
		Fit(59, {4, 5, 8, 9}, 41, {14, 17, 20, 21}, 59, true);
		return bValid ? std::nullopt : std::optional<FError>{MakeError("the red Weltall's model lacks a limb end")};
	}

	void FBattleGearDress::JoinPieces(std::vector<FGearBone>& Bones) const
	{
		for (const FJointFit& Joint : JointFits)
		{
			const FGteMatrix& Parent = Bones[Joint.Parent].World;
			const FGteMatrix& Child = Bones[Joint.Child].World;
			const FPoint From = Placed(Parent, Joint.ParentEnd);
			const FPoint To = Placed(Child, Joint.ChildEnd);
			const FPoint ParentAxis = Turned(Parent, Joint.ParentAxis);
			const FPoint ChildAxis = Turned(Child, Joint.ChildAxis);
			double Cosine = 0.0;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Cosine += ParentAxis[Axis] * ChildAxis[Axis];
			}
			Cosine /= std::sqrt(SquaredLength(ParentAxis) * SquaredLength(ChildAxis));
			// Tilted open ends overlap by their radius times sin(half the bend), covering the outside of the joint.
			const double Inset = Joint.Radius * std::sqrt((1.0 - std::clamp(Cosine, -1.0, 1.0)) * 0.5);
			for (const uint8 Bone : Joint.Moved)
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					const double Shift = From[Axis] - To[Axis] - ChildAxis[Axis] * Inset;
					Bones[Bone].World.T[Axis] += static_cast<int32>(std::lround(Joint.bMoveParent ? -Shift : Shift));
				}
			}
		}
	}
}
