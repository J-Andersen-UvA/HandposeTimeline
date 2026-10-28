#pragma once

#include "CoreMinimal.h"
#include "HandposeTimelineSegment.generated.h"

USTRUCT(BlueprintType)
struct HANDPOSETIMELINE_API FHandposeTimelineSegment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline")
    int32 startFrame = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline")
    int32 endFrame = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline", meta = (ClampMin = "0.0"))
    double durationSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline")
    FName selectedHandshapeId = NAME_None;

    bool isValid() const;
    int32 getFrameCount() const;
    bool hasSelectedHandshape() const;
};
