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
                    SAssignNew(segmentContainer, SHorizontalBox)
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
    if (isPreviewingSegment &&
        skeletalMeshComponent != nullptr &&
        skeletalMeshComponent->GetPosition() >= previewEndTimeSeconds)
    {
        if (shouldLoopSegmentPreview)
        {
            skeletalMeshComponent->SetPosition(previewStartTimeSeconds, false);
            skeletalMeshComponent->SetPlayRate(previewPlayRate);
            skeletalMeshComponent->Play(false);
        }
        else
        {
            skeletalMeshComponent->Stop();
            skeletalMeshComponent->SetPosition(previewEndTimeSeconds, false);
            isPreviewingSegment = false;
        }
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

FReply SHandposeTimelineWidget::onSegmentClicked(const int32 segmentIndex)
{
    if (segments.IsValidIndex(segmentIndex))
    {
        selectedSegmentIndex = segmentIndex;
        const FHandposeTimelineSegment& segment = segments[segmentIndex];
        statusText->SetText(FText::Format(
            LOCTEXT("SelectedSegment", "Segment {0}: frames {1}-{2}, {3} seconds"),
            FText::AsNumber(segmentIndex + 1),
            FText::AsNumber(segment.startFrame),
            FText::AsNumber(segment.endFrame),
            FText::AsNumber(segment.durationSeconds)));
        rebuildTimeline();
        previewSegment(segment);
    }
    return FReply::Handled();
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

void SHandposeTimelineWidget::previewSegment(
    const FHandposeTimelineSegment& segment)
{
    UAnimSequenceBase* animation = loadedAnimation.Get();
    USkeletalMeshComponent* skeletalMeshComponent = previewSkeletalMeshComponent.Get();
    if (animation == nullptr || skeletalMeshComponent == nullptr)
    {
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
    isPreviewingSegment = true;
}

void SHandposeTimelineWidget::rebuildTimeline()
{
    if (!segmentContainer.IsValid())
    {
        return;
    }

    segmentContainer->ClearChildren();
    if (segments.IsEmpty())
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

    for (int32 segmentIndex = 0; segmentIndex < segments.Num(); ++segmentIndex)
    {
        segmentContainer->AddSlot()
        .AutoWidth()
        .Padding(1.0f, 0.0f)
        [
            createSegmentWidget(segments[segmentIndex], segmentIndex)
        ];
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
    const float segmentWidth = FMath::Max(
        minimumSegmentWidth,
        static_cast<float>(segment.durationSeconds) * pixelsPerSecond * timelineZoom);
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

#undef LOCTEXT_NAMESPACE
