#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "HandposeAssignment.generated.h"

class UAnimSequenceBase;

USTRUCT(BlueprintType)
struct HANDPOSETIMELINE_API FHandposeAssignment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline")
    FGuid assignmentId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline")
    int32 sourceSegmentIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline")
    FName handshapeId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline")
    TSoftObjectPtr<UAnimSequenceBase> handshapeAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline")
    int32 blendStartFrame = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline")
    int32 fullPoseStartFrame = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline")
    int32 fullPoseEndFrame = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline")
    int32 blendEndFrame = INDEX_NONE;

    bool isValid() const;
    float evaluateWeight(double frame) const;
    void clampToTimeline(int32 lastFrame);
    void shiftBy(int32 frameDelta, int32 lastFrame);
};
