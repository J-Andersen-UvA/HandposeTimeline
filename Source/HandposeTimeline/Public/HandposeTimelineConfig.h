#pragma once

#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "HandposeAsset.h"
#include "HandshapeMatchingTypes.h"
#include "HandshapeSegmentationTypes.h"
#include "HandposeTimelineConfig.generated.h"

class UAnimSequence;
class UHandposeAsset;

USTRUCT(BlueprintType)
struct HANDPOSETIMELINE_API FHandposeSegmentationParameters
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Segmentation", meta = (ClampMin = "1"))
    int32 minimumSegmentLength = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Segmentation", meta = (ClampMin = "0.0"))
    double minimumAccelerationThreshold = 0.25;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Segmentation", meta = (ClampMin = "0.01"))
    double quaternionSmoothingSigma = 1.5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Segmentation", meta = (ClampMin = "0.01"))
    double accelerationSmoothingSigma = 1.5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Segmentation", meta = (ClampMin = "0.0"))
    double thresholdFactor = 0.5;
};

UCLASS(BlueprintType)
class HANDPOSETIMELINE_API UHandposeTimelineConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    UHandposeTimelineConfig();
    virtual void PostLoad() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Segmentation")
    FHandposeSegmentationParameters sharedSegmentationSettings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Segmentation|Bones")
    TArray<FName> rightHandBoneNames;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Segmentation|Bones")
    TArray<FName> leftHandBoneNames;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Handshapes",
        meta = (ContentDir, LongPackageName))
    FDirectoryPath handshapeContentFolder;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handshapes")
    bool includeHandshapeSubfolders = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handshapes")
    EHandshapeMatchingMethod matchingMethod = EHandshapeMatchingMethod::Euclidean;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handshapes")
    TArray<TSoftObjectPtr<UAnimSequence>> handshapePoses;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handshapes")
    TArray<TSoftObjectPtr<UHandposeAsset>> handposeAssets;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Output",
        meta = (ContentDir, LongPackageName))
    FDirectoryPath bakedAnimationOutputFolder;

    UFUNCTION(
        CallInEditor,
        Category = "Handshapes",
        meta = (DisplayName = "Load Handshapes From Content Folder"))
    void loadHandshapesFromContentFolder();

    FHandshapeSegmentationSettings getSegmentationSettings() const;
    FHandshapeSegmentationSettings getSegmentationSettings(EHandposeSide handSide) const;
    FString getHandshapeContentPath() const;
    TArray<TSoftObjectPtr<UAnimSequence>> getHandshapePoses() const;
    TArray<TSoftObjectPtr<UHandposeAsset>> getHandposeAssets() const;
    FString getBakedAnimationOutputPath() const;

private:
    UPROPERTY()
    FHandshapeSegmentationSettings segmentationSettings_DEPRECATED;

    UPROPERTY()
    FHandshapeSegmentationSettings leftHandSegmentationSettings_DEPRECATED;

    TArray<TSoftObjectPtr<UAnimSequence>> findHandshapesInContentFolder() const;
    TArray<TSoftObjectPtr<UHandposeAsset>> findHandposeAssetsInContentFolder() const;
};
