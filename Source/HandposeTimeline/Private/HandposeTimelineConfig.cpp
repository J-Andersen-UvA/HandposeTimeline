#include "HandposeTimelineConfig.h"

#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"

UHandposeTimelineConfig::UHandposeTimelineConfig()
{
    segmentationSettings.boneNames = {
        TEXT("RightHandIndex"),
        TEXT("RightHandIndex1"),
        TEXT("RightHandIndex2"),
        TEXT("RightHandIndex3"),
        TEXT("RightHandThumb"),
        TEXT("RightHandThumb1"),
        TEXT("RightHandThumb2"),
        TEXT("RightHandThumb3"),
        TEXT("RightHandMiddle"),
        TEXT("RightHandMiddle1"),
        TEXT("RightHandMiddle2"),
        TEXT("RightHandMiddle3"),
        TEXT("RightHandPinky"),
        TEXT("RightHandPinky1"),
        TEXT("RightHandPinky2"),
        TEXT("RightHandPinky3"),
        TEXT("RightHandRing"),
        TEXT("RightHandRing1"),
        TEXT("RightHandRing2"),
        TEXT("RightHandRing3")
    };
    segmentationSettings.minimumSegmentLength = 2;
    segmentationSettings.minimumAccelerationThreshold = 0.25;
    segmentationSettings.quaternionSmoothingSigma = 1.5;
    segmentationSettings.accelerationSmoothingSigma = 1.5;
    segmentationSettings.thresholdFactor = 0.5;
    handshapeContentFolder.Path = TEXT("/HandposeTimeline/HandposeTimeline/Handshapes");
}

FHandshapeSegmentationSettings UHandposeTimelineConfig::getSegmentationSettings() const
{
    return segmentationSettings;
}

FString UHandposeTimelineConfig::getHandshapeContentPath() const
{
    return handshapeContentFolder.Path;
}

TArray<TSoftObjectPtr<UAnimSequence>> UHandposeTimelineConfig::getHandshapePoses() const
{
    TArray<TSoftObjectPtr<UAnimSequence>> poses = handshapePoses;
    for (const TSoftObjectPtr<UAnimSequence>& folderPose : findHandshapesInContentFolder())
    {
        poses.AddUnique(folderPose);
    }
    return poses;
}

void UHandposeTimelineConfig::loadHandshapesFromContentFolder()
{
    Modify();
    handshapePoses = findHandshapesInContentFolder();
    MarkPackageDirty();
    PostEditChange();
}

TArray<TSoftObjectPtr<UAnimSequence>>
UHandposeTimelineConfig::findHandshapesInContentFolder() const
{
    TArray<TSoftObjectPtr<UAnimSequence>> poses;
    if (handshapeContentFolder.Path.IsEmpty())
    {
        return poses;
    }

    FARFilter filter;
    filter.PackagePaths.Add(*handshapeContentFolder.Path);
    filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
    filter.bRecursivePaths = includeHandshapeSubfolders;

    FAssetRegistryModule& assetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> assets;
    assetRegistryModule.Get().GetAssets(filter, assets);
    for (const FAssetData& asset : assets)
    {
        poses.Add(TSoftObjectPtr<UAnimSequence>(asset.ToSoftObjectPath()));
    }
    poses.Sort(
        [](const TSoftObjectPtr<UAnimSequence>& left,
           const TSoftObjectPtr<UAnimSequence>& right)
        {
            return left.ToSoftObjectPath().ToString() <
                right.ToSoftObjectPath().ToString();
        });
    return poses;
}
