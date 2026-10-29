#pragma once

#include "CoreMinimal.h"

class UAnimSequence;

struct FHandposeBakeResult
{
    bool success = false;
    bool cancelled = false;
    UAnimSequence* animation = nullptr;
    FString outputPath;
    FString errorMessage;
};

class FHandposeBakeService
{
public:
    static FHandposeBakeResult bake(
        UAnimSequence* previewAnimation,
        const UAnimSequence* sourceAnimation,
        const FString& outputRoot);
};
