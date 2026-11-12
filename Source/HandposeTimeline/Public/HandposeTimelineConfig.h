#pragma once

#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "HandposeAsset.h"
#include "HandshapeMatchingTypes.h"
#include "HandshapeSegmentationTypes.h"
#include "HandposeTimelineConfig.generated.h"

class UAnimSequence;
class UHandposeAsset;

UENUM(BlueprintType)
enum class EHandposeMirrorFlipAxis : uint8
{
    None,
    X,
    Y,
    Z
};

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
        Category = "Handshapes|Bone Mapping",
        meta = (EditCondition = "descriptorMethod == EHandshapeDescriptorMethod::HandSkeleton", EditConditionHides))
    FHandshapeBoneMapping rightHandBoneMapping;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Handshapes|Bone Mapping",
        meta = (EditCondition = "descriptorMethod == EHandshapeDescriptorMethod::HandSkeleton", EditConditionHides))
    FHandshapeBoneMapping leftHandBoneMapping;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handshapes|Mirroring")
    EHandposeMirrorFlipAxis handposeMirrorFlipAxis = EHandposeMirrorFlipAxis::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handshapes|Apply Bones")
    TArray<FName> rightHandApplyBoneNames;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handshapes|Apply Bones")
    TArray<FName> leftHandApplyBoneNames;

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
    EHandshapeDescriptorMethod descriptorMethod = EHandshapeDescriptorMethod::JointRotations;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Handshapes|Descriptor Bones",
        meta = (
            DisplayName = "Use Virtual Leaf Bones",
            EditCondition = "descriptorMethod == EHandshapeDescriptorMethod::HandSkeleton",
            EditConditionHides))
    bool useVirtualLeafBones = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Handshapes|Descriptor Bones",
        meta = (
            DisplayName = "Virtual Leaf Bone Extension (cm)",
            ClampMin = "1.0",
            ClampMax = "3.0",
            UIMin = "1.0",
            UIMax = "3.0",
            Units = "cm",
            EditCondition = "descriptorMethod == EHandshapeDescriptorMethod::HandSkeleton && useVirtualLeafBones",
            EditConditionHides))
    double virtualLeafBoneExtensionCm = 2.0;

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
    const FHandshapeBoneMapping& getBoneMapping(EHandposeSide handSide) const;
    EAxis::Type getHandposeMirrorFlipAxis() const;
    TArray<FName> getConfiguredBoneNames(EHandposeSide handSide) const;
    TArray<FName> getApplyBoneNames(EHandposeSide handSide) const;
    TArray<FName> getLeafBoneNames(EHandposeSide handSide) const;
    FString getHandshapeContentPath() const;
    TArray<TSoftObjectPtr<UAnimSequence>> getHandshapePoses() const;
    TArray<TSoftObjectPtr<UHandposeAsset>> getHandposeAssets() const;
    FString getBakedAnimationOutputPath() const;

private:
    UPROPERTY()
    FHandshapeSegmentationSettings segmentationSettings_DEPRECATED;

    UPROPERTY()
    FHandshapeSegmentationSettings leftHandSegmentationSettings_DEPRECATED;

    UPROPERTY()
    TArray<FName> rightHandLeafBoneNames_DEPRECATED;

    UPROPERTY()
    TArray<FName> leftHandLeafBoneNames_DEPRECATED;

    TArray<TSoftObjectPtr<UAnimSequence>> findHandshapesInContentFolder() const;
    TArray<TSoftObjectPtr<UHandposeAsset>> findHandposeAssetsInContentFolder() const;
};
