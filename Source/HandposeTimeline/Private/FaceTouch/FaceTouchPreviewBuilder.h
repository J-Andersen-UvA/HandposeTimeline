#pragma once

#include "CoreMinimal.h"
#include "FaceTouchCorrectionTypes.h"
#include "Preview/HandposePreviewTypes.h"

class AActor;
class UAnimSequence;

class FFaceTouchPreviewBuilder
{
public:
    static FHandposePreviewBuildResult applyCorrections(
        UAnimSequence* sourceAnimation,
        AActor* actor,
        const TArray<FFaceTouchCorrectionBlock>& corrections);
};
