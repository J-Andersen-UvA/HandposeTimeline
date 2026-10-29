#pragma once

#include "AnimPose.h"
#include "HandshapeMatchingTypes.h"
#include "HandshapeSegmentationTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HandshapeMatchingBlueprintLibrary.generated.h"

class UAnimSequenceBase;

UCLASS()
class HANDSHAPESEGMENTATIONADAPTER_API UHandshapeMatchingBlueprintLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Handshape Matching")
    static FHandshapeMatchingResult matchSegmentToHandshapes(
        const UAnimSequenceBase* animation,
        int32 startFrame,
        int32 endFrame,
        const TArray<UAnimSequenceBase*>& handshapeAnimations,
        FAnimPoseEvaluationOptions evaluationOptions,
        const FHandshapeSegmentationSettings& settings,
        EHandshapeMatchingMethod matchingMethod);
};
