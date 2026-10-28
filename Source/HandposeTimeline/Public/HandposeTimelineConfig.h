#pragma once

#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
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
    TArray<TSoftObjectPtr<UAnimSequence>> handshapePoses;

    FHandshapeSegmentationSettings getSegmentationSettings() const;
    FString getHandshapeContentPath() const;
    TArray<TSoftObjectPtr<UAnimSequence>> getHandshapePoses() const;
};
