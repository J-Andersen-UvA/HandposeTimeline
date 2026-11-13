#pragma once

#include "CoreMinimal.h"

class UAnimSequence;

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
};
