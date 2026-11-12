#pragma once

#include "CoreMinimal.h"

class UAnimSequence;

struct FHandposePreviewBuildResult
{
    bool success = false;
    UAnimSequence* animation = nullptr;
    FString errorMessage;
};
