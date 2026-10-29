#pragma once

#include "CoreMinimal.h"

class UAnimSequence;
struct FHandposeAssignment;

struct FHandposePreviewBuildResult
{
    bool success = false;
    UAnimSequence* animation = nullptr;
    FString errorMessage;
};

class FHandposePreviewBuilder
{
public:
    static FHandposePreviewBuildResult build(
        UAnimSequence* sourceAnimation,
        const TArray<FHandposeAssignment>& assignments,
        const TArray<FName>& boneNames);
};
