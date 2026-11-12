#include "HandposeTimelineConfig.h"

#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HandposeAsset.h"
#include "Modules/ModuleManager.h"

namespace
{
FName toLeftBoneName(const FName rightBoneName)
{
    FString leftBoneName = rightBoneName.ToString();
    leftBoneName.ReplaceInline(TEXT("Right"), TEXT("Left"));
    leftBoneName.ReplaceInline(TEXT("_r"), TEXT("_l"));
    return FName(leftBoneName);
}

FHandshapeBoneMapping makeDefaultRightBoneMapping()
{
    FHandshapeBoneMapping mapping;
    mapping.thumb = {
        TEXT("RightHandThumb1"), TEXT("RightHandThumb2"),
        TEXT("RightHandThumb3"), TEXT("RightHandThumb4")};
    mapping.index = {
        TEXT("RightHandIndex"), TEXT("RightHandIndex1"),
        TEXT("RightHandIndex2"), TEXT("RightHandIndex3"),
        TEXT("RightHandIndex4")};
    mapping.middle = {
        TEXT("RightHand"), TEXT("RightHandMiddle1"),
        TEXT("RightHandMiddle2"), TEXT("RightHandMiddle3"),
        TEXT("RightHandMiddle4")};
    mapping.ring = {
        TEXT("RightHandRing"), TEXT("RightHandRing1"),
        TEXT("RightHandRing2"), TEXT("RightHandRing3"),
        TEXT("RightHandRing4")};
    mapping.pinky = {
        TEXT("RightHandPinky"), TEXT("RightHandPinky1"),
        TEXT("RightHandPinky2"), TEXT("RightHandPinky3"),
        TEXT("RightHandPinky4")};
    return mapping;
}

FHandshapeBoneMapping makeLeftBoneMapping(const FHandshapeBoneMapping& right)
{
    FHandshapeBoneMapping left = right;
    left.thumb.metacarpal = toLeftBoneName(right.thumb.metacarpal);
    left.thumb.proximal = toLeftBoneName(right.thumb.proximal);
    left.thumb.distal = toLeftBoneName(right.thumb.distal);
    left.thumb.tip = toLeftBoneName(right.thumb.tip);
    FHandshapeFingerBoneMapping* leftFingers[] = {
        &left.index, &left.middle, &left.ring, &left.pinky};
    const FHandshapeFingerBoneMapping* rightFingers[] = {
        &right.index, &right.middle, &right.ring, &right.pinky};
    for (int32 fingerIndex = 0; fingerIndex < 4; ++fingerIndex)
    {
        leftFingers[fingerIndex]->metacarpal =
            toLeftBoneName(rightFingers[fingerIndex]->metacarpal);
        leftFingers[fingerIndex]->proximal =
            toLeftBoneName(rightFingers[fingerIndex]->proximal);
        leftFingers[fingerIndex]->middle =
            toLeftBoneName(rightFingers[fingerIndex]->middle);
        leftFingers[fingerIndex]->distal =
            toLeftBoneName(rightFingers[fingerIndex]->distal);
        leftFingers[fingerIndex]->tip =
            toLeftBoneName(rightFingers[fingerIndex]->tip);
    }
    return left;
}

TArray<FName> getMappedBones(
    const FHandshapeBoneMapping& mapping,
    const bool includeTips)
{
    TArray<FName> bones = {
        mapping.thumb.metacarpal,
        mapping.thumb.proximal,
        mapping.thumb.distal,
        mapping.index.metacarpal,
        mapping.index.proximal,
        mapping.index.middle,
        mapping.index.distal,
        mapping.middle.metacarpal,
        mapping.middle.proximal,
        mapping.middle.middle,
        mapping.middle.distal,
        mapping.ring.metacarpal,
        mapping.ring.proximal,
        mapping.ring.middle,
        mapping.ring.distal,
        mapping.pinky.metacarpal,
        mapping.pinky.proximal,
        mapping.pinky.middle,
        mapping.pinky.distal
    };
    if (includeTips)
    {
        bones.Insert(mapping.thumb.tip, 3);
        bones.Insert(mapping.index.tip, 8);
        bones.Insert(mapping.middle.tip, 13);
        bones.Insert(mapping.ring.tip, 18);
        bones.Add(mapping.pinky.tip);
    }
    bones.RemoveAll([](const FName boneName) { return boneName.IsNone(); });
    return bones;
}

TArray<FName> getMappedTips(const FHandshapeBoneMapping& mapping)
{
    return {
        mapping.thumb.tip,
        mapping.index.tip,
        mapping.middle.tip,
        mapping.ring.tip,
        mapping.pinky.tip
    };
}

}

UHandposeTimelineConfig::UHandposeTimelineConfig()
{
    rightHandBoneMapping = makeDefaultRightBoneMapping();
    leftHandBoneMapping = makeLeftBoneMapping(rightHandBoneMapping);
    rightHandBoneNames = getMappedBones(rightHandBoneMapping, false);
    leftHandBoneNames = getMappedBones(leftHandBoneMapping, false);
    handshapeContentFolder.Path = TEXT("/HandposeTimeline/HandposeTimeline/Handposes");
    bakedAnimationOutputFolder.Path = TEXT("/Game/HandposeTimeline/Baked");
}

void UHandposeTimelineConfig::PostLoad()
{
    Super::PostLoad();
    if (!segmentationSettings_DEPRECATED.boneNames.IsEmpty())
    {
        rightHandBoneNames = segmentationSettings_DEPRECATED.boneNames;
        leftHandBoneNames =
            leftHandSegmentationSettings_DEPRECATED.boneNames;
        sharedSegmentationSettings.minimumSegmentLength =
            segmentationSettings_DEPRECATED.minimumSegmentLength;
        sharedSegmentationSettings.minimumAccelerationThreshold =
            segmentationSettings_DEPRECATED.minimumAccelerationThreshold;
        sharedSegmentationSettings.quaternionSmoothingSigma =
            segmentationSettings_DEPRECATED.quaternionSmoothingSigma;
        sharedSegmentationSettings.accelerationSmoothingSigma =
            segmentationSettings_DEPRECATED.accelerationSmoothingSigma;
        sharedSegmentationSettings.thresholdFactor =
            segmentationSettings_DEPRECATED.thresholdFactor;
        segmentationSettings_DEPRECATED.boneNames.Reset();
        leftHandSegmentationSettings_DEPRECATED.boneNames.Reset();
        MarkPackageDirty();
    }
}

FHandshapeSegmentationSettings UHandposeTimelineConfig::getSegmentationSettings() const
{
    return getSegmentationSettings(EHandposeSide::Right);
}

FHandshapeSegmentationSettings UHandposeTimelineConfig::getSegmentationSettings(
    const EHandposeSide handSide) const
{
    FHandshapeSegmentationSettings settings;
    settings.boneNames = handSide == EHandposeSide::Left
        ? leftHandBoneNames
        : rightHandBoneNames;
    settings.minimumSegmentLength = sharedSegmentationSettings.minimumSegmentLength;
    settings.minimumAccelerationThreshold =
        sharedSegmentationSettings.minimumAccelerationThreshold;
    settings.quaternionSmoothingSigma =
        sharedSegmentationSettings.quaternionSmoothingSigma;
    settings.accelerationSmoothingSigma =
        sharedSegmentationSettings.accelerationSmoothingSigma;
    settings.thresholdFactor = sharedSegmentationSettings.thresholdFactor;
    return settings;
}

const FHandshapeBoneMapping& UHandposeTimelineConfig::getBoneMapping(
    const EHandposeSide handSide) const
{
    return handSide == EHandposeSide::Left
        ? leftHandBoneMapping
        : rightHandBoneMapping;
}

EAxis::Type UHandposeTimelineConfig::getHandposeMirrorFlipAxis() const
{
    switch (handposeMirrorFlipAxis)
    {
    case EHandposeMirrorFlipAxis::Y:
        return EAxis::Y;
    case EHandposeMirrorFlipAxis::Z:
        return EAxis::Z;
    case EHandposeMirrorFlipAxis::X:
        return EAxis::X;
    case EHandposeMirrorFlipAxis::None:
    default:
        return EAxis::None;
    }
}

TArray<FName> UHandposeTimelineConfig::getConfiguredBoneNames(
    const EHandposeSide handSide) const
{
    return getMappedBones(getBoneMapping(handSide), false);
}

TArray<FName> UHandposeTimelineConfig::getApplyBoneNames(
    const EHandposeSide handSide) const
{
    const TArray<FName>& applyBoneNames = handSide == EHandposeSide::Left
        ? leftHandApplyBoneNames
        : rightHandApplyBoneNames;
    if (!applyBoneNames.IsEmpty())
    {
        return applyBoneNames;
    }
    return handSide == EHandposeSide::Left
        ? leftHandBoneNames
        : rightHandBoneNames;
}

TArray<FName> UHandposeTimelineConfig::getLeafBoneNames(
    const EHandposeSide handSide) const
{
    return getMappedTips(getBoneMapping(handSide));
}

FString UHandposeTimelineConfig::getHandshapeContentPath() const
{
    return handshapeContentFolder.Path;
}

TArray<TSoftObjectPtr<UAnimSequence>> UHandposeTimelineConfig::getHandshapePoses() const
{
    TArray<TSoftObjectPtr<UAnimSequence>> poses = handshapePoses;
    if (getHandposeAssets().IsEmpty())
    {
        for (const TSoftObjectPtr<UAnimSequence>& folderPose : findHandshapesInContentFolder())
        {
            poses.AddUnique(folderPose);
        }
    }
    return poses;
}

TArray<TSoftObjectPtr<UHandposeAsset>> UHandposeTimelineConfig::getHandposeAssets() const
{
    TArray<TSoftObjectPtr<UHandposeAsset>> assets = handposeAssets;
    for (const TSoftObjectPtr<UHandposeAsset>& folderAsset : findHandposeAssetsInContentFolder())
    {
        assets.AddUnique(folderAsset);
    }
    return assets;
}

FString UHandposeTimelineConfig::getBakedAnimationOutputPath() const
{
    return bakedAnimationOutputFolder.Path;
}

void UHandposeTimelineConfig::loadHandshapesFromContentFolder()
{
    Modify();
    handposeAssets = findHandposeAssetsInContentFolder();
    if (handposeAssets.IsEmpty())
    {
        handshapePoses = findHandshapesInContentFolder();
    }
    else
    {
        handshapePoses.Reset();
    }
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

TArray<TSoftObjectPtr<UHandposeAsset>>
UHandposeTimelineConfig::findHandposeAssetsInContentFolder() const
{
    TArray<TSoftObjectPtr<UHandposeAsset>> assets;
    if (handshapeContentFolder.Path.IsEmpty())
    {
        return assets;
    }

    FARFilter filter;
    filter.PackagePaths.Add(*handshapeContentFolder.Path);
    filter.ClassPaths.Add(UHandposeAsset::StaticClass()->GetClassPathName());
    filter.bRecursivePaths = includeHandshapeSubfolders;

    FAssetRegistryModule& assetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> foundAssets;
    assetRegistryModule.Get().GetAssets(filter, foundAssets);
    for (const FAssetData& asset : foundAssets)
    {
        assets.Add(TSoftObjectPtr<UHandposeAsset>(asset.ToSoftObjectPath()));
    }
    assets.Sort(
        [](const TSoftObjectPtr<UHandposeAsset>& left,
           const TSoftObjectPtr<UHandposeAsset>& right)
        {
            return left.ToSoftObjectPath().ToString() <
                right.ToSoftObjectPath().ToString();
        });
    return assets;
}
