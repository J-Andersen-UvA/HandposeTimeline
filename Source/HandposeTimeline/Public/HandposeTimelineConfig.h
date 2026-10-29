#pragma once

#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "HandshapeMatchingTypes.h"
#include "HandshapeSegmentationTypes.h"
#include "HandposeTimelineConfig.generated.h"

class UAnimSequence;

UCLASS(BlueprintType)
class HANDPOSETIMELINE_API UHandposeTimelineConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    UHandposeTimelineConfig();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Segmentation")
    FHandshapeSegmentationSettings segmentationSettings;

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
    FString getHandshapeContentPath() const;
    TArray<TSoftObjectPtr<UAnimSequence>> getHandshapePoses() const;
    FString getBakedAnimationOutputPath() const;

private:
    TArray<TSoftObjectPtr<UAnimSequence>> findHandshapesInContentFolder() const;
};
