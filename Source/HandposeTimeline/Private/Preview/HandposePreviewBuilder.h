#pragma once

#include "CoreMinimal.h"
#include "HandshapeMatchingTypes.h"
#include "Preview/HandposePreviewTypes.h"

class UAnimSequence;
struct FHandposeAssignment;

class FHandposePreviewBuilder
{
public:
    static FHandposePreviewBuildResult build(
        UAnimSequence* sourceAnimation,
        const TArray<FHandposeAssignment>& assignments,
        const TArray<FName>& rightBoneNames,
        const TArray<FName>& leftBoneNames,
        const FHandshapeBoneMapping& rightBoneMapping,
        const FHandshapeBoneMapping& leftBoneMapping,
        EAxis::Type mirrorFlipAxis);
};
