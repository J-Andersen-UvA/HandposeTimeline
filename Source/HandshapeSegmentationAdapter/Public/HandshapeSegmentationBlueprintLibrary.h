#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimPose.h"
#include "HandshapeSegmentationTypes.h"
#include "HandshapeSegmentationBlueprintLibrary.generated.h"

class UAnimSequenceBase;

UCLASS()
class HANDSHAPESEGMENTATIONADAPTER_API UHandshapeSegmentationBlueprintLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Handshape Segmentation")
    static FHandshapeSegmentationResult segmentAnimation(
        const UAnimSequenceBase* animation,
        FAnimPoseEvaluationOptions evaluationOptions,
        const FHandshapeSegmentationSettings& settings);

};
