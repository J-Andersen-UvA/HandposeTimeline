#include "Widgets/SHandposeTimelineWidget.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "AnimPose.h"
#include "AssetRegistry/AssetData.h"
#include "Components/SkeletalMeshComponent.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "HandshapeSegmentationBlueprintLibrary.h"
#include "HandshapeSegmentationTypes.h"
#include "IContentBrowserSingleton.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHandposeTimelineWidget"

namespace
{
constexpr float pixelsPerSecond = 180.0f;
constexpr float minimumSegmentWidth = 72.0f;
constexpr float minimumGapWidth = 8.0f;
constexpr float minimumTimelineZoom = 1.0f;
constexpr float maximumTimelineZoom = 5.0f;
}

void SHandposeTimelineWidget::Construct(const FArguments& arguments)
{
    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                .Text(LOCTEXT("LoadAnimation", "Load Animation"))
                .ToolTipText(LOCTEXT("LoadAnimationTooltip", "Choose an animation and generate its handpose segments."))
                .OnClicked(this, &SHandposeTimelineWidget::onLoadAnimationClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(12.0f, 0.0f)
            [
                SAssignNew(animationNameText, STextBlock)
                .Text(LOCTEXT("NoAnimation", "No animation loaded"))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(12.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("UseSelectedActor", "Use Selected Actor"))
                .ToolTipText(LOCTEXT(
                    "UseSelectedActorTooltip",
                    "Use the selected scene actor and its skeletal mesh for segment previews."))
                .OnClicked(this, &SHandposeTimelineWidget::onUseSelectedActorClicked)
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(12.0f, 0.0f)
            [
                SAssignNew(actorNameText, STextBlock)
                .Text(LOCTEXT("NoActor", "No actor selected"))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("LoopSegment", "Loop"))
                .ToolTipText(LOCTEXT(
                    "LoopSegmentTooltip",
                    "Loop the selected segment preview."))
                .ButtonColorAndOpacity_Lambda(
                    [this]()
                    {
                        return shouldLoopSegmentPreview
                            ? FLinearColor(0.05f, 0.4f, 0.75f, 1.0f)
                            : FLinearColor::White;
                    })
                .OnClicked(this, &SHandposeTimelineWidget::onLoopClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 12.0f, 0.0f)
            [
                SNew(SButton)
                .ToolTipText(LOCTEXT(
                    "PreviewSpeedTooltip",
                    "Cycle the segment preview speed between 0.25x, 0.5x, and 1x."))
                .OnClicked(this, &SHandposeTimelineWidget::onSpeedClicked)
                [
                    SAssignNew(speedText, STextBlock)
                    .Text(LOCTEXT("DefaultPreviewSpeed", "Speed: 1x"))
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("TimelineZoom", "Zoom"))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(8.0f, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(140.0f)
                [
                    SNew(SSlider)
                    .Value(0.0f)
                    .OnValueChanged(this, &SHandposeTimelineWidget::onZoomChanged)
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SAssignNew(zoomText, STextBlock)
                .Text(LOCTEXT("DefaultZoom", "100%"))
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.0f, 0.0f, 8.0f, 8.0f)
        [
            SAssignNew(statusText, STextBlock)
            .Text(LOCTEXT("LoadPrompt", "Load an animation to generate segments."))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.0f, 0.0f, 8.0f, 8.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(LOCTEXT("PlayTimeline", "Play"))
                .ToolTipText(LOCTEXT("PlayTimelineTooltip", "Play the full animation from the playhead."))
                .OnClicked(this, &SHandposeTimelineWidget::onPlayClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("PauseTimeline", "Pause"))
                .ToolTipText(LOCTEXT("PauseTimelineTooltip", "Pause playback at the current playhead position."))
                .OnClicked(this, &SHandposeTimelineWidget::onPauseClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(12.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(this, &SHandposeTimelineWidget::getTimecodeText)
                .Font(FAppStyle::GetFontStyle(TEXT("MonospacedText")))
            ]
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(8.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
            .Padding(12.0f)
            [
                SNew(SScrollBox)
                .Orientation(Orient_Horizontal)
                + SScrollBox::Slot()
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SBox)
                        .HeightOverride(20.0f)
                        [
                            SNew(SSlider)
                            .Value(this, &SHandposeTimelineWidget::getPlayheadSliderValue)
                            .ToolTipText(LOCTEXT("PlayheadTooltip", "Drag to scrub the animation."))
                            .OnValueChanged(this, &SHandposeTimelineWidget::onPlayheadScrubbed)
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(segmentContainer, SHorizontalBox)
                    ]
                ]
            ]
        ]
    ];

    rebuildTimeline();
}

void SHandposeTimelineWidget::Tick(
    const FGeometry& allottedGeometry,
    const double currentTime,
    const float deltaTime)
{
    SCompoundWidget::Tick(allottedGeometry, currentTime, deltaTime);

    USkeletalMeshComponent* skeletalMeshComponent = previewSkeletalMeshComponent.Get();
    if ((isPreviewingSegment || isPlayingTimeline) && skeletalMeshComponent != nullptr)
    {
        currentPreviewTimeSeconds = skeletalMeshComponent->GetPosition();
    }
    if (isPreviewingSegment &&
        skeletalMeshComponent != nullptr &&
        currentPreviewTimeSeconds >= previewEndTimeSeconds)
    {
        if (shouldLoopSegmentPreview)
        {
            skeletalMeshComponent->SetPosition(previewStartTimeSeconds, false);
            skeletalMeshComponent->SetPlayRate(previewPlayRate);
            skeletalMeshComponent->Play(false);
            currentPreviewTimeSeconds = previewStartTimeSeconds;
        }
        else
        {
            skeletalMeshComponent->Stop();
            skeletalMeshComponent->SetPosition(previewEndTimeSeconds, false);
            currentPreviewTimeSeconds = previewEndTimeSeconds;
            isPreviewingSegment = false;
        }
    }
    else if (isPlayingTimeline &&
        skeletalMeshComponent != nullptr &&
        loadedAnimation.IsValid() &&
        currentPreviewTimeSeconds >= loadedAnimation->GetPlayLength())
    {
        skeletalMeshComponent->Stop();
        currentPreviewTimeSeconds = loadedAnimation->GetPlayLength();
        isPlayingTimeline = false;
    }
}

FReply SHandposeTimelineWidget::onLoadAnimationClicked()
{
    FOpenAssetDialogConfig dialogConfig;
    dialogConfig.DialogTitleOverride = LOCTEXT("SelectAnimation", "Select Animation");
    dialogConfig.AssetClassNames.Add(UAnimSequence::StaticClass()->GetClassPathName());
    dialogConfig.bAllowMultipleSelection = false;

    FContentBrowserModule& contentBrowserModule =
        FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
    const TArray<FAssetData> selectedAssets =
        contentBrowserModule.Get().CreateModalOpenAssetDialog(dialogConfig);
    if (!selectedAssets.IsEmpty())
    {
        loadAnimation(Cast<UAnimSequenceBase>(selectedAssets[0].GetAsset()));
    }

    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onUseSelectedActorClicked()
{
    AActor* actor = GEditor != nullptr
        ? GEditor->GetSelectedActors()->GetTop<AActor>()
        : nullptr;
    USkeletalMeshComponent* skeletalMeshComponent = actor != nullptr
        ? actor->FindComponentByClass<USkeletalMeshComponent>()
        : nullptr;

    if (actor == nullptr)
    {
        previewActor.Reset();
        previewSkeletalMeshComponent.Reset();
        actorNameText->SetText(LOCTEXT("NoSceneActorSelected", "No actor selected"));
        statusText->SetText(LOCTEXT(
            "SelectSceneActorPrompt",
            "Select an actor in the scene, then click Use Selected Actor."));
        return FReply::Handled();
    }

    if (skeletalMeshComponent == nullptr)
    {
        previewActor.Reset();
        previewSkeletalMeshComponent.Reset();
        actorNameText->SetText(FText::FromString(actor->GetActorLabel()));
        statusText->SetText(LOCTEXT(
            "ActorHasNoSkeletalMesh",
            "The selected actor does not contain a skeletal mesh component."));
        return FReply::Handled();
    }

    previewActor = actor;
    previewSkeletalMeshComponent = skeletalMeshComponent;
    actorNameText->SetText(FText::FromString(actor->GetActorLabel()));
    statusText->SetText(FText::Format(
        LOCTEXT("PreviewActorSelected", "Using {0} for segment previews."),
        FText::FromString(actor->GetActorLabel())));
    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onLoopClicked()
{
    shouldLoopSegmentPreview = !shouldLoopSegmentPreview;
    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onSpeedClicked()
{
    if (FMath::IsNearlyEqual(previewPlayRate, 1.0f))
    {
        previewPlayRate = 0.25f;
    }
    else if (FMath::IsNearlyEqual(previewPlayRate, 0.25f))
    {
        previewPlayRate = 0.5f;
    }
    else
    {
        previewPlayRate = 1.0f;
    }

    const FText speedLabel = FMath::IsNearlyEqual(previewPlayRate, 1.0f)
        ? LOCTEXT("PreviewSpeedOne", "Speed: 1x")
        : FText::Format(
            LOCTEXT("PreviewSpeedFraction", "Speed: {0}x"),
            FText::AsNumber(previewPlayRate));
    speedText->SetText(speedLabel);

    if (USkeletalMeshComponent* skeletalMeshComponent = previewSkeletalMeshComponent.Get())
    {
        skeletalMeshComponent->SetPlayRate(previewPlayRate);
    }
    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onPlayClicked()
{
    UAnimSequenceBase* animation = loadedAnimation.Get();
    USkeletalMeshComponent* skeletalMeshComponent = previewSkeletalMeshComponent.Get();
    if (animation == nullptr || skeletalMeshComponent == nullptr)
    {
        statusText->SetText(LOCTEXT(
            "TimelinePlaybackRequiresActor",
            "Load an animation and select a compatible scene actor before playback."));
        return FReply::Handled();
    }

    const USkeletalMesh* skeletalMesh = skeletalMeshComponent->GetSkeletalMeshAsset();
    if (skeletalMesh == nullptr ||
        animation->GetSkeleton() == nullptr ||
        !animation->GetSkeleton()->IsCompatibleForEditor(skeletalMesh->GetSkeleton()))
    {
        statusText->SetText(LOCTEXT(
            "IncompatibleTimelineActor",
            "The selected actor's skeletal mesh is not compatible with the loaded animation."));
        return FReply::Handled();
    }

    if (currentPreviewTimeSeconds >= animation->GetPlayLength())
    {
        currentPreviewTimeSeconds = 0.0f;
    }

    skeletalMeshComponent->SetUpdateAnimationInEditor(true);
    skeletalMeshComponent->PlayAnimation(animation, false);
    skeletalMeshComponent->SetPosition(currentPreviewTimeSeconds, false);
    skeletalMeshComponent->SetPlayRate(previewPlayRate);
    isPreviewingSegment = false;
    isPlayingTimeline = true;
    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onPauseClicked()
{
    if (USkeletalMeshComponent* skeletalMeshComponent = previewSkeletalMeshComponent.Get())
    {
        currentPreviewTimeSeconds = skeletalMeshComponent->GetPosition();
        skeletalMeshComponent->Stop();
    }
    isPreviewingSegment = false;
    isPlayingTimeline = false;
    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onSegmentClicked(const int32 segmentIndex)
{
    selectSegment(segmentIndex, true);
    return FReply::Handled();
}

void SHandposeTimelineWidget::onPlayheadScrubbed(const float sliderValue)
{
    UAnimSequenceBase* animation = loadedAnimation.Get();
    if (animation == nullptr || timelineBlocks.IsEmpty())
    {
        return;
    }

    const float targetOffset = FMath::Clamp(sliderValue, 0.0f, 1.0f) *
        getTotalTimelineWidth();
    currentPreviewTimeSeconds = getTimeAtTimelineOffset(targetOffset);

    if (USkeletalMeshComponent* skeletalMeshComponent = previewSkeletalMeshComponent.Get())
    {
        const USkeletalMesh* skeletalMesh = skeletalMeshComponent->GetSkeletalMeshAsset();
        if (skeletalMesh != nullptr &&
            animation->GetSkeleton() != nullptr &&
            animation->GetSkeleton()->IsCompatibleForEditor(skeletalMesh->GetSkeleton()))
        {
            skeletalMeshComponent->SetUpdateAnimationInEditor(true);
            skeletalMeshComponent->PlayAnimation(animation, false);
            skeletalMeshComponent->Stop();
            skeletalMeshComponent->SetPosition(currentPreviewTimeSeconds, false);
            skeletalMeshComponent->TickAnimation(0.0f, false);
            skeletalMeshComponent->RefreshBoneTransforms(nullptr);
            skeletalMeshComponent->UpdateBounds();
            skeletalMeshComponent->MarkRenderDynamicDataDirty();
            if (GEditor != nullptr)
            {
                GEditor->RedrawLevelEditingViewports(false);
            }
        }
    }

    isPreviewingSegment = false;
    isPlayingTimeline = false;
}

void SHandposeTimelineWidget::onZoomChanged(const float sliderValue)
{
    timelineZoom = FMath::Lerp(
        minimumTimelineZoom,
        maximumTimelineZoom,
        sliderValue);
    zoomText->SetText(FText::Format(
        LOCTEXT("ZoomPercent", "{0}%"),
        FText::AsNumber(FMath::RoundToInt(timelineZoom * 100.0f))));
    rebuildTimeline();
}

void SHandposeTimelineWidget::loadAnimation(UAnimSequenceBase* animation)
{
    loadedAnimation = animation;
    segments.Reset();
    selectedSegmentIndex = INDEX_NONE;
    isPreviewingSegment = false;
    isPlayingTimeline = false;
    previewStartTimeSeconds = 0.0f;
    previewEndTimeSeconds = 0.0f;
    currentPreviewTimeSeconds = 0.0f;

    if (animation == nullptr)
    {
        animationNameText->SetText(LOCTEXT("InvalidAnimation", "Invalid animation"));
        statusText->SetText(LOCTEXT("InvalidAnimationStatus", "The selected asset is not an animation."));
        rebuildTimeline();
        return;
    }

    animationNameText->SetText(FText::FromString(animation->GetName()));

    FHandshapeSegmentationSettings settings;
    settings.boneNames = getSegmentationBoneNames(*animation);
    if (settings.boneNames.IsEmpty())
    {
        statusText->SetText(LOCTEXT(
            "NoFingerBones",
            "No finger bones were found. Expected bone names containing thumb, index, middle, ring, pinky, little, or finger."));
        rebuildTimeline();
        return;
    }

    const FHandshapeSegmentationResult result =
        UHandshapeSegmentationBlueprintLibrary::segmentAnimation(
            animation,
            FAnimPoseEvaluationOptions(),
            settings);
    if (!result.success)
    {
        statusText->SetText(FText::FromString(result.errorMessage));
        rebuildTimeline();
        return;
    }

    segments.Reserve(result.segments.Num());
    for (const FHandshapeSegment& sourceSegment : result.segments)
    {
        FHandposeTimelineSegment& segment = segments.AddDefaulted_GetRef();
        segment.startFrame = sourceSegment.startFrame;
        segment.endFrame = sourceSegment.endFrame;
        segment.durationSeconds = FMath::Max(
            0.0,
            sourceSegment.endTimeSeconds - sourceSegment.startTimeSeconds);
    }

    statusText->SetText(FText::Format(
        LOCTEXT("SegmentationComplete", "Generated {0} segments using {1} finger bones."),
        FText::AsNumber(segments.Num()),
        FText::AsNumber(settings.boneNames.Num())));
    rebuildTimeline();
}

void SHandposeTimelineWidget::selectSegment(
    const int32 segmentIndex,
    const bool shouldPreview)
{
    if (!segments.IsValidIndex(segmentIndex))
    {
        return;
    }

    selectedSegmentIndex = segmentIndex;
    const FHandposeTimelineSegment& segment = segments[segmentIndex];
    statusText->SetText(FText::Format(
        LOCTEXT("SelectedSegment", "Segment {0}: frames {1}-{2}, {3} seconds"),
        FText::AsNumber(segmentIndex + 1),
        FText::AsNumber(segment.startFrame),
        FText::AsNumber(segment.endFrame),
        FText::AsNumber(segment.durationSeconds)));
    rebuildTimeline();

    if (shouldPreview)
    {
        previewSegment(segment);
    }
}

void SHandposeTimelineWidget::previewSegment(
    const FHandposeTimelineSegment& segment)
{
    UAnimSequenceBase* animation = loadedAnimation.Get();
    USkeletalMeshComponent* skeletalMeshComponent = previewSkeletalMeshComponent.Get();
    if (animation == nullptr || skeletalMeshComponent == nullptr)
    {
        statusText->SetText(LOCTEXT(
            "PreviewRequiresActor",
            "Select a compatible scene actor with Use Selected Actor to preview this segment."));
        return;
    }

    const USkeletalMesh* skeletalMesh = skeletalMeshComponent->GetSkeletalMeshAsset();
    if (skeletalMesh == nullptr ||
        animation->GetSkeleton() == nullptr ||
        !animation->GetSkeleton()->IsCompatibleForEditor(skeletalMesh->GetSkeleton()))
    {
        statusText->SetText(LOCTEXT(
            "IncompatiblePreviewActor",
            "The selected actor's skeletal mesh is not compatible with the loaded animation."));
        return;
    }

    const FFrameRate frameRate = animation->GetSamplingFrameRate();
    if (!frameRate.IsValid())
    {
        statusText->SetText(LOCTEXT(
            "InvalidPreviewFrameRate",
            "The loaded animation has an invalid sampling frame rate."));
        return;
    }

    previewStartTimeSeconds = static_cast<float>(
        frameRate.AsSeconds(segment.startFrame));
    previewEndTimeSeconds = FMath::Min(
        static_cast<float>(frameRate.AsSeconds(segment.endFrame + 1)),
        animation->GetPlayLength());

    skeletalMeshComponent->SetUpdateAnimationInEditor(true);
    skeletalMeshComponent->PlayAnimation(animation, false);
    skeletalMeshComponent->SetPosition(previewStartTimeSeconds, false);
    skeletalMeshComponent->SetPlayRate(previewPlayRate);
    currentPreviewTimeSeconds = previewStartTimeSeconds;
    isPreviewingSegment = true;
    isPlayingTimeline = false;
}

void SHandposeTimelineWidget::rebuildTimeline()
{
    if (!segmentContainer.IsValid())
    {
        return;
    }

    segmentContainer->ClearChildren();
    buildTimelineBlocks();
    if (timelineBlocks.IsEmpty())
    {
        segmentContainer->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("EmptyTimeline", "No segments to display"))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];
        return;
    }

    for (const FTimelineBlock& block : timelineBlocks)
    {
        segmentContainer->AddSlot()
        .AutoWidth()
        .Padding(1.0f, 0.0f)
        [
            block.isGap()
                ? createGapWidget(block)
                : createSegmentWidget(segments[block.segmentIndex], block.segmentIndex)
        ];
    }
}

void SHandposeTimelineWidget::buildTimelineBlocks()
{
    timelineBlocks.Reset();

    UAnimSequenceBase* animation = loadedAnimation.Get();
    if (animation == nullptr)
    {
        return;
    }

    const int32 lastAnimationFrame = animation->GetNumberOfSampledKeys() - 1;
    if (lastAnimationFrame < 0)
    {
        return;
    }

    int32 nextFrame = 0;
    for (int32 segmentIndex = 0; segmentIndex < segments.Num(); ++segmentIndex)
    {
        const FHandposeTimelineSegment& segment = segments[segmentIndex];
        const int32 segmentStart = FMath::Clamp(segment.startFrame, 0, lastAnimationFrame);
        const int32 segmentEnd = FMath::Clamp(segment.endFrame, segmentStart, lastAnimationFrame);

        if (segmentStart > nextFrame)
        {
            timelineBlocks.Add({nextFrame, segmentStart - 1, INDEX_NONE});
        }

        timelineBlocks.Add({segmentStart, segmentEnd, segmentIndex});
        nextFrame = FMath::Max(nextFrame, segmentEnd + 1);
    }

    if (nextFrame <= lastAnimationFrame)
    {
        timelineBlocks.Add({nextFrame, lastAnimationFrame, INDEX_NONE});
    }
}

TArray<FName> SHandposeTimelineWidget::getSegmentationBoneNames(
    const UAnimSequenceBase& animation) const
{
    TArray<FName> boneNames;
    const USkeleton* skeleton = animation.GetSkeleton();
    if (skeleton == nullptr)
    {
        return boneNames;
    }

    static const TArray<FString> fingerNameParts = {
        TEXT("thumb"),
        TEXT("index"),
        TEXT("middle"),
        TEXT("ring"),
        TEXT("pinky"),
        TEXT("little"),
        TEXT("finger")
    };

    const FReferenceSkeleton& referenceSkeleton = skeleton->GetReferenceSkeleton();
    for (const FMeshBoneInfo& boneInfo : referenceSkeleton.GetRawRefBoneInfo())
    {
        const FString lowercaseBoneName = boneInfo.Name.ToString().ToLower();
        if (fingerNameParts.ContainsByPredicate(
            [&lowercaseBoneName](const FString& namePart)
            {
                return lowercaseBoneName.Contains(namePart);
            }))
        {
            boneNames.Add(boneInfo.Name);
        }
    }
    return boneNames;
}

TSharedRef<SWidget> SHandposeTimelineWidget::createSegmentWidget(
    const FHandposeTimelineSegment& segment,
    const int32 segmentIndex)
{
    const float segmentWidth = getSegmentWidth(segment);
    const bool isSelected = selectedSegmentIndex == segmentIndex;

    return SNew(SBox)
        .WidthOverride(segmentWidth)
        .HeightOverride(112.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(
                isSelected
                    ? FLinearColor(0.0f, 0.45f, 0.8f, 1.0f)
                    : FLinearColor::Transparent)
            .Padding(isSelected ? 2.0f : 0.0f)
            [
                SNew(SButton)
                .ToolTipText(FText::Format(
                    LOCTEXT(
                        "SegmentTooltip",
                        "Segment {0}\nFrameRange\n{1}-{2}\n{3} seconds"),
                    FText::AsNumber(segmentIndex + 1),
                    FText::AsNumber(segment.startFrame),
                    FText::AsNumber(segment.endFrame),
                    FText::AsNumber(segment.durationSeconds)))
                .ButtonColorAndOpacity(
                    isSelected
                        ? FLinearColor(0.08f, 0.24f, 0.38f, 1.0f)
                        : FLinearColor::White)
                .OnClicked(this, &SHandposeTimelineWidget::onSegmentClicked, segmentIndex)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::Format(
                            LOCTEXT("SegmentLabel", "Segment {0}"),
                            FText::AsNumber(segmentIndex + 1)))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 6.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::Format(
                            LOCTEXT("SegmentFrames", "FrameRange\n{0}-{1}"),
                            FText::AsNumber(segment.startFrame),
                            FText::AsNumber(segment.endFrame)))
                        .Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::Format(
                            LOCTEXT("SegmentDuration", "{0} seconds"),
                            FText::AsNumber(segment.durationSeconds)))
                        .Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                ]
            ]
        ];
}

TSharedRef<SWidget> SHandposeTimelineWidget::createGapWidget(
    const FTimelineBlock& block) const
{
    return SNew(SBox)
        .WidthOverride(getTimelineBlockWidth(block))
        .HeightOverride(112.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(FLinearColor(0.12f, 0.12f, 0.12f, 1.0f))
            .Padding(6.0f)
            .ToolTipText(FText::Format(
                LOCTEXT("UnsegmentedFramesTooltip", "Unsegmented frames\nFrameRange\n{0}-{1}"),
                FText::AsNumber(block.startFrame),
                FText::AsNumber(block.endFrame)))
        ];
}

float SHandposeTimelineWidget::getSegmentWidth(
    const FHandposeTimelineSegment& segment) const
{
    return FMath::Max(
        minimumSegmentWidth,
        static_cast<float>(segment.durationSeconds) * pixelsPerSecond * timelineZoom);
}

float SHandposeTimelineWidget::getTimelineBlockWidth(
    const FTimelineBlock& block) const
{
    if (!block.isGap() && segments.IsValidIndex(block.segmentIndex))
    {
        return getSegmentWidth(segments[block.segmentIndex]);
    }

    UAnimSequenceBase* animation = loadedAnimation.Get();
    if (animation == nullptr || !animation->GetSamplingFrameRate().IsValid())
    {
        return minimumGapWidth;
    }

    const double durationSeconds = animation->GetSamplingFrameRate().AsSeconds(
        block.endFrame - block.startFrame + 1);
    return FMath::Max(
        minimumGapWidth,
        static_cast<float>(durationSeconds) * pixelsPerSecond * timelineZoom);
}

float SHandposeTimelineWidget::getPlayheadOffset() const
{
    UAnimSequenceBase* animation = loadedAnimation.Get();
    if (animation == nullptr || timelineBlocks.IsEmpty())
    {
        return 0.0f;
    }

    const FFrameRate frameRate = animation->GetSamplingFrameRate();
    if (!frameRate.IsValid())
    {
        return 0.0f;
    }

    float offset = 0.0f;
    for (const FTimelineBlock& block : timelineBlocks)
    {
        const float blockWidth = getTimelineBlockWidth(block);
        const float startTime = static_cast<float>(frameRate.AsSeconds(block.startFrame));
        const float endTime = static_cast<float>(frameRate.AsSeconds(block.endFrame + 1));
        if (currentPreviewTimeSeconds <= endTime)
        {
            const float duration = FMath::Max(endTime - startTime, UE_SMALL_NUMBER);
            const float alpha = FMath::Clamp(
                (currentPreviewTimeSeconds - startTime) / duration,
                0.0f,
                1.0f);
            return offset + 1.0f + alpha * blockWidth;
        }
        offset += blockWidth + 2.0f;
    }
    return offset;
}

float SHandposeTimelineWidget::getPlayheadSliderValue() const
{
    const float timelineWidth = getTotalTimelineWidth();
    return timelineWidth > 0.0f
        ? FMath::Clamp(getPlayheadOffset() / timelineWidth, 0.0f, 1.0f)
        : 0.0f;
}

float SHandposeTimelineWidget::getTotalTimelineWidth() const
{
    float width = 0.0f;
    for (const FTimelineBlock& block : timelineBlocks)
    {
        width += getTimelineBlockWidth(block) + 2.0f;
    }
    return width;
}

float SHandposeTimelineWidget::getTimeAtTimelineOffset(const float offset) const
{
    UAnimSequenceBase* animation = loadedAnimation.Get();
    if (animation == nullptr || !animation->GetSamplingFrameRate().IsValid())
    {
        return 0.0f;
    }

    const FFrameRate frameRate = animation->GetSamplingFrameRate();
    float blockOffset = 0.0f;
    for (const FTimelineBlock& block : timelineBlocks)
    {
        const float blockWidth = getTimelineBlockWidth(block);
        if (offset <= blockOffset + blockWidth + 2.0f)
        {
            const float alpha = FMath::Clamp(
                (offset - blockOffset - 1.0f) / blockWidth,
                0.0f,
                1.0f);
            const float startTime = static_cast<float>(frameRate.AsSeconds(block.startFrame));
            const float endTime = static_cast<float>(frameRate.AsSeconds(block.endFrame + 1));
            return FMath::Lerp(startTime, endTime, alpha);
        }
        blockOffset += blockWidth + 2.0f;
    }

    return animation->GetPlayLength();
}

FText SHandposeTimelineWidget::getTimecodeText() const
{
    UAnimSequenceBase* animation = loadedAnimation.Get();
    if (animation == nullptr || !animation->GetSamplingFrameRate().IsValid())
    {
        return FText::FromString(TEXT("00:00:00:00"));
    }

    const int32 framesPerSecond = FMath::Max(
        1,
        FMath::RoundToInt(animation->GetSamplingFrameRate().AsDecimal()));
    int32 remainingFrames = FMath::Max(
        0,
        FMath::FloorToInt(currentPreviewTimeSeconds * framesPerSecond));
    const int32 frames = remainingFrames % framesPerSecond;
    remainingFrames /= framesPerSecond;
    const int32 seconds = remainingFrames % 60;
    remainingFrames /= 60;
    const int32 minutes = remainingFrames % 60;
    const int32 hours = remainingFrames / 60;

    return FText::FromString(FString::Printf(
        TEXT("%02d:%02d:%02d:%02d"),
        hours,
        minutes,
        seconds,
        frames));
}

#undef LOCTEXT_NAMESPACE
