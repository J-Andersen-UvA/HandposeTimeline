#pragma once

#include "CoreMinimal.h"
#include "HandshapeMatchingTypes.h"

class UAnimSequence;

struct FHandposeMirrorResult
{
    bool success = false;
    UAnimSequence* animation = nullptr;
    FString errorMessage;
};

class FHandposeMirrorService
{
public:
    static FHandposeMirrorResult remapAnimationToOppositeHand(
        UAnimSequence* sourceAnimation,
        const FHandshapeBoneMapping& sourceMapping,
        const FHandshapeBoneMapping& targetMapping,
        EAxis::Type flipAxis);
    static TArray<FName> getTargetBoneNames(
        const FHandshapeBoneMapping& sourceMapping,
        const FHandshapeBoneMapping& targetMapping);
};
