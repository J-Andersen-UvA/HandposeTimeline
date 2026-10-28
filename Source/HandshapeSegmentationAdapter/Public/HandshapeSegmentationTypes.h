#pragma once

#include "CoreMinimal.h"
#include "HandshapeSegmentationTypes.generated.h"

USTRUCT(BlueprintType)
struct HANDSHAPESEGMENTATIONADAPTER_API FHandshapeQuaternionTrack
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Segmentation")
    FName boneName = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Segmentation")
    TArray<FQuat> localRotations;
};

USTRUCT(BlueprintType)
struct HANDSHAPESEGMENTATIONADAPTER_API FHandshapeSegment
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Segmentation")
    int32 startFrame = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Segmentation")
    int32 endFrame = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Segmentation")
    double startTimeSeconds = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Segmentation")
    double endTimeSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct HANDSHAPESEGMENTATIONADAPTER_API FHandshapeSegmentationSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Segmentation")
    TArray<FName> boneNames;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Segmentation", meta = (ClampMin = "1"))
    int32 minimumSegmentLength = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Segmentation", meta = (ClampMin = "0.0"))
    double minimumAccelerationThreshold = 0.5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Segmentation", meta = (ClampMin = "0.01"))
    double quaternionSmoothingSigma = 2.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Segmentation", meta = (ClampMin = "0.01"))
    double accelerationSmoothingSigma = 1.5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Segmentation", meta = (ClampMin = "0.0"))
    double thresholdFactor = 0.5;
};

USTRUCT(BlueprintType)
struct HANDSHAPESEGMENTATIONADAPTER_API FHandshapeSegmentationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Segmentation")
    bool success = false;

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Segmentation")
    FString errorMessage;

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Segmentation")
    int32 frameRateNumerator = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Segmentation")
    int32 frameRateDenominator = 1;

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Segmentation")
    int32 frameCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Segmentation")
    TArray<FHandshapeQuaternionTrack> quaternionTracks;

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Segmentation")
    TArray<FHandshapeSegment> segments;
};
