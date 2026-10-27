#pragma once

#include "Anim/SlopPose.h"
#include "ReferenceSkeleton.h"

namespace Slop::Anim
{
	// The fixed mapping between the original's world (x along the stage, y up, +z toward the camera,
	// metres) and Unreal's (Y along the stage, Z up, -X toward the camera, centimetres). The camera
	// sees the same picture in both; the map is a reflection, so rotations are conjugated by it.
	FVector WorldToUE(const FVector& G);
	FVector UEToWorld(const FVector& U);
	FVector2D StageToUE2D(const FVector2D& G);

	// Binds the rig to one skeletal mesh (the MakeHuman game skeleton: pelvis, spine_01..03,
	// neck_01, head, clavicle/upperarm/lowerarm/hand, thigh/calf/foot/ball, fingers). The rig's
	// proportions are measured from the mesh's reference pose, so the hands and feet reach exactly
	// the targets the poses name. Every frame it turns the solved rig into bone-space transforms.
	class FMeshRig
	{
	public:
		// Returns false (with Error set) when a required bone is missing.
		bool Init(const FReferenceSkeleton& Ref, double Height, FString& Error);
		// Writes this mesh's proportions into the rig (rest offsets, limb lengths, ankle height).
		void ConfigureRig(FRig& Rig) const;
		// Component-space rotation for a rig-space one.
		FQuat RigToMesh(const FQuat& Q) const;
		FVector RigToMeshPos(const FVector& P) const { return MapV(P) * 100.0; }
		// The mesh component's world transform for the rig root's world transform (original space).
		FTransform ComponentTransform(const FXf& Root, const FVector& WorldScale) const;
		// Bone-space transforms for the whole skeleton from the solved rig.
		void Pose(const FRig& Rig, TArray<FTransform>& OutLocal) const;
		int32 NumBones() const { return RefLocal.Num(); }
		bool IsValid() const { return bValid; }

	private:
		FVector MapV(const FVector& V) const { return Right * V.X + Up * V.Y - Fwd * V.Z; }
		FVector UnmapV(const FVector& U) const { return FVector(FVector::DotProduct(U, Right), FVector::DotProduct(U, Up), -FVector::DotProduct(U, Fwd)); }
		FVector RestRig(int32 Bone) const { return UnmapV(RefCS[Bone].GetLocation()) / 100.0; }

		bool bValid = false;
		double H = 1.8;
		TArray<int32> Parent;
		TArray<FTransform> RefLocal;
		TArray<FTransform> RefCS;
		// Mesh axes in component space.
		FVector Right = FVector(-1, 0, 0), Up = FVector(0, 0, 1), Fwd = FVector(0, 1, 0);
		int32 BoneOf[(int32)EJoint::Count];
		// Rig frames in the bind pose, so a joint's turn is measured from where the mesh was built.
		FQuat BindFrame[(int32)EJoint::Count];
		FVector RestPos[(int32)EJoint::Count];
		double Lens[4] = {0, 0, 0, 0};
		double AnkleH = 0.08;
		struct FFinger { int32 Bone; int32 Side; FVector AxisLocal; double FistRad; };
		TArray<FFinger> Fingers;
	};
}
