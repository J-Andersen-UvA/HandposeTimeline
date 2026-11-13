#pragma once

#include "CoreMinimal.h"

class UAnimSequence;
class USkeletalMesh;

struct ANIMATIONPLACEMENT_API FAnimationPlacementLegChain
{
    TArray<FName> bones;
    FName toeBone;
};

struct ANIMATIONPLACEMENT_API FAnimationGroundSettings
{
    int32 solveFrame = 0;
    FTransform skeletalMeshComponentTransform = FTransform::Identity;
    FVector planeWorldPoint = FVector::ZeroVector;
    FVector planeWorldNormal = FVector::UpVector;
    FAnimationPlacementLegChain leftLeg;
    FAnimationPlacementLegChain rightLeg;
};

struct ANIMATIONPLACEMENT_API FAnimationPlacementResult
{
    bool success = false;
    UAnimSequence* animation = nullptr;
    FString errorMessage;
};

class ANIMATIONPLACEMENT_API FAnimationPlacementService
{
public:
    static FAnimationPlacementResult rotate180(UAnimSequence* sourceAnimation);
    static FAnimationPlacementResult placeFeetOnGround(
        UAnimSequence* sourceAnimation,
        const USkeletalMesh* skeletalMesh,
        const FAnimationGroundSettings& settings);
};
