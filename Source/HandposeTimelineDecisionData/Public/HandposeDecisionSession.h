#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HandposeDecisionSession.generated.h"

USTRUCT(BlueprintType)
struct HANDPOSETIMELINEDECISIONDATA_API FHandposeDecisionCandidateSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    FString handposeId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    FString displayName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    int32 rank = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    double matchingDistance = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    double relevanceScore = 0.0;
};

USTRUCT(BlueprintType)
struct HANDPOSETIMELINEDECISIONDATA_API FHandposeDecisionSegmentSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    FString handSide;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    int32 segmentIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    int32 startFrame = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    int32 endFrame = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    double durationSeconds = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    bool hasAssignment = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    TArray<FString> assignmentIds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    TArray<FHandposeDecisionCandidateSnapshot> topCandidates;
};

USTRUCT(BlueprintType)
struct HANDPOSETIMELINEDECISIONDATA_API FHandposeDecisionAssignmentSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    FString assignmentId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    FString handSide;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    FString handposeId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    int32 sourceSegmentIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    TArray<int32> coveredSegmentIndices;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    TArray<FString> skippedFingers;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    int32 blendStartFrame = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    int32 fullPoseStartFrame = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    int32 fullPoseEndFrame = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    int32 blendEndFrame = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct HANDPOSETIMELINEDECISIONDATA_API FHandposeDecisionConfigSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    FString configName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    FString matchingMethod;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    int32 minimumSegmentLength = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    double minimumAccelerationThreshold = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    double quaternionSmoothingSigma = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    double accelerationSmoothingSigma = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    double thresholdFactor = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    TArray<FString> rightHandBoneNames;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    TArray<FString> leftHandBoneNames;
};

USTRUCT(BlueprintType)
struct HANDPOSETIMELINEDECISIONDATA_API FHandposeDecisionSessionData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    int32 schemaVersion = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    FString capturedAtUtc;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    FString sourceAnimationName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    FString sourceAnimationPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    double framesPerSecond = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    int32 totalFrames = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    double durationSeconds = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    FString handposeCatalogVersion = TEXT("1");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    FHandposeDecisionConfigSnapshot config;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    TArray<FHandposeDecisionSegmentSnapshot> segments;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    TArray<FHandposeDecisionAssignmentSnapshot> assignments;
};

UCLASS(BlueprintType)
class HANDPOSETIMELINEDECISIONDATA_API UHandposeDecisionSession : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decision Data")
    FHandposeDecisionSessionData sessionData;
};
