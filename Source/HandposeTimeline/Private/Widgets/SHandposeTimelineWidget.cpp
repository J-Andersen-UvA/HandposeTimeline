#include "Widgets/SHandposeTimelineWidget.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "AnimPose.h"
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "HandshapeSegmentationBlueprintLibrary.h"
#include "HandshapeSegmentationTypes.h"
#include "HandshapeMatchingBlueprintLibrary.h"
#include "HandshapeMatchingTypes.h"
#include "HandposeTimelineConfig.h"
#include "IContentBrowserSingleton.h"
#include "InputCoreTypes.h"
#include "Playback/HandposeTimelinePlaybackController.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SHandshapePickerWidget.h"
#include "Widgets/SHandposeTimelineTrack.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHandposeTimelineWidget"

namespace
{
constexpr float minimumTimelineZoom = 1.0f;
constexpr float maximumTimelineZoom = 5.0f;
}

void SHandposeTimelineWidget::Construct(const FArguments& arguments)
{
    playbackController = MakeShared<FHandposeTimelinePlaybackController>();

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
                .Text(LOCTEXT("LoadConfig", "Load Config"))
                .ToolTipText(LOCTEXT(
                    "LoadConfigTooltip",
                    "Choose the segmentation and handshape source configuration."))
                .OnClicked(this, &SHandposeTimelineWidget::onLoadConfigClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(8.0f, 0.0f, 12.0f, 0.0f)
            [
                SAssignNew(configNameText, STextBlock)
                .Text(LOCTEXT("NoConfig", "Default settings"))
            ]
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
                .Text(LOCTEXT("LoopAll", "Loop All"))
                .ToolTipText(LOCTEXT("LoopAllTooltip", "Loop the full timeline when using Play."))
                .ButtonColorAndOpacity_Lambda(
                    [this]()
                    {
                        return playbackController->shouldLoopAll()
                            ? FLinearColor(0.05f, 0.4f, 0.75f, 1.0f)
                            : FLinearColor::White;
                    })
                .OnClicked(this, &SHandposeTimelineWidget::onLoopAllClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("LoopSegment", "Loop Segment"))
                .ToolTipText(LOCTEXT(
                    "LoopSegmentTooltip",
                    "Loop the selected segment when using Play or previewing a segment."))
                .ButtonColorAndOpacity_Lambda(
                    [this]()
                    {
                        return playbackController->shouldLoopSegment()
                            ? FLinearColor(0.05f, 0.4f, 0.75f, 1.0f)
                            : FLinearColor::White;
                    })
                .OnClicked(this, &SHandposeTimelineWidget::onLoopSegmentClicked)
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
                .Text(this, &SHandposeTimelineWidget::getPlayPauseButtonText)
                .ToolTipText(LOCTEXT(
                    "PlayPauseTimelineTooltip",
                    "Play the full animation from the playhead or pause active playback."))
                .OnClicked(this, &SHandposeTimelineWidget::onPlayPauseClicked)
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
            SAssignNew(timelineTrack, SHandposeTimelineTrack)
            .CurrentTimeSeconds(this, &SHandposeTimelineWidget::getCurrentTimeSeconds)
            .OnSegmentSelected(this, &SHandposeTimelineWidget::onSegmentSelected)
            .OnScrubbed(this, &SHandposeTimelineWidget::onPlayheadScrubbed)
        ]
    ];

    refreshTimeline();
}

void SHandposeTimelineWidget::Tick(
    const FGeometry& allottedGeometry,
    const double currentTime,
    const float deltaTime)
{
    SCompoundWidget::Tick(allottedGeometry, currentTime, deltaTime);
    playbackController->tick();
}

FReply SHandposeTimelineWidget::OnPreviewKeyDown(
    const FGeometry& myGeometry,
    const FKeyEvent& keyEvent)
{
    if (keyEvent.GetKey() == EKeys::Left)
    {
        stepFrame(-1);
        return FReply::Handled();
    }
    if (keyEvent.GetKey() == EKeys::Right)
    {
        stepFrame(1);
        return FReply::Handled();
    }
    return SCompoundWidget::OnPreviewKeyDown(myGeometry, keyEvent);
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

FReply SHandposeTimelineWidget::onLoadConfigClicked()
{
    FOpenAssetDialogConfig dialogConfig;
    dialogConfig.DialogTitleOverride = LOCTEXT("SelectConfig", "Select Handpose Timeline Config");
    dialogConfig.DefaultPath = TEXT("/HandposeTimeline/HandposeTimeline/Configs");
    dialogConfig.AssetClassNames.Add(UHandposeTimelineConfig::StaticClass()->GetClassPathName());
    dialogConfig.bAllowMultipleSelection = false;

    FContentBrowserModule& contentBrowserModule =
        FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
    const TArray<FAssetData> selectedAssets =
        contentBrowserModule.Get().CreateModalOpenAssetDialog(dialogConfig);
    if (!selectedAssets.IsEmpty())
    {
        loadConfig(Cast<UHandposeTimelineConfig>(selectedAssets[0].GetAsset()));
    }
    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onUseSelectedActorClicked()
{
    AActor* selectedActor = GEditor != nullptr
        ? GEditor->GetSelectedActors()->GetTop<AActor>()
        : nullptr;
    const EHandposeTimelineActorResult result = playbackController->setActor(selectedActor);

    if (result == EHandposeTimelineActorResult::NoActor)
    {
        actorNameText->SetText(LOCTEXT("NoSceneActorSelected", "No actor selected"));
        statusText->SetText(LOCTEXT(
            "SelectSceneActorPrompt",
            "Select an actor in the scene, then click Use Selected Actor."));
    }
    else if (result == EHandposeTimelineActorResult::NoSkeletalMesh)
    {
        actorNameText->SetText(FText::FromString(selectedActor->GetActorLabel()));
        statusText->SetText(LOCTEXT(
            "ActorHasNoSkeletalMesh",
            "The selected actor does not contain a skeletal mesh component."));
    }
    else
    {
        actorNameText->SetText(FText::FromString(selectedActor->GetActorLabel()));
        statusText->SetText(FText::Format(
            LOCTEXT("PreviewActorSelected", "Using {0} for segment previews."),
            FText::FromString(selectedActor->GetActorLabel())));
    }
    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onLoopAllClicked()
{
    playbackController->toggleLoopAll();
    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onLoopSegmentClicked()
{
    playbackController->toggleLoopSegment();
    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onSpeedClicked()
{
    const float playRate = playbackController->cyclePlayRate();
    const FText speedLabel = FMath::IsNearlyEqual(playRate, 1.0f)
        ? LOCTEXT("PreviewSpeedOne", "Speed: 1x")
        : FText::Format(
            LOCTEXT("PreviewSpeedFraction", "Speed: {0}x"),
            FText::AsNumber(playRate));
    speedText->SetText(speedLabel);
    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onPlayPauseClicked()
{
    if (playbackController->isPlaying())
    {
        playbackController->pause();
        return FReply::Handled();
    }

    if (playbackController->shouldLoopSegment() &&
        !segments.IsValidIndex(selectedSegmentIndex))
    {
        statusText->SetText(LOCTEXT(
            "LoopSegmentRequiresSelection",
            "Select a segment before using Loop Segment playback."));
        return FReply::Handled();
    }

    const EHandposeTimelinePlaybackResult result =
        playbackController->shouldLoopSegment()
            ? playbackController->previewSegment(segments[selectedSegmentIndex])
            : playbackController->playTimeline();
    if (result == EHandposeTimelinePlaybackResult::MissingAnimationOrActor)
    {
        statusText->SetText(LOCTEXT(
            "TimelinePlaybackRequiresActor",
            "Load an animation and select a compatible scene actor before playback."));
    }
    else if (result == EHandposeTimelinePlaybackResult::IncompatibleSkeleton)
    {
        statusText->SetText(LOCTEXT(
            "IncompatibleTimelineActor",
            "The selected actor's skeletal mesh is not compatible with the loaded animation."));
    }
    return FReply::Handled();
}

void SHandposeTimelineWidget::onSegmentSelected(const int32 segmentIndex)
{
    selectSegment(segmentIndex, true);
    openHandshapePicker(segmentIndex);
}

void SHandposeTimelineWidget::onHandshapeChosen(
    const int32 segmentIndex,
    const FName handshapeId)
{
    if (!segments.IsValidIndex(segmentIndex))
    {
        return;
    }

    segments[segmentIndex].selectedHandshapeId = handshapeId;
    selectedSegmentIndex = segmentIndex;
    statusText->SetText(FText::Format(
        LOCTEXT(
            "HandshapeAssigned",
            "Assigned {0} to Segment {1}."),
        FText::FromName(handshapeId),
        FText::AsNumber(segmentIndex + 1)));
    refreshTimeline();
}

void SHandposeTimelineWidget::onPlayheadScrubbed(const float timeSeconds)
{
    playbackController->scrubTo(timeSeconds);
}

void SHandposeTimelineWidget::stepFrame(const int32 frameOffset)
{
    UAnimSequenceBase* animation = loadedAnimation.Get();
    if (animation == nullptr || !animation->GetSamplingFrameRate().IsValid())
    {
        return;
    }

    const FFrameRate frameRate = animation->GetSamplingFrameRate();
    const int32 currentFrame = FMath::RoundToInt(
        getCurrentTimeSeconds() * frameRate.AsDecimal());
    const int32 lastFrame = FMath::Max(0, animation->GetNumberOfSampledKeys() - 1);
    const int32 targetFrame = FMath::Clamp(currentFrame + frameOffset, 0, lastFrame);
    onPlayheadScrubbed(static_cast<float>(frameRate.AsSeconds(targetFrame)));
}

void SHandposeTimelineWidget::onZoomChanged(const float sliderValue)
{
    timelineZoom = FMath::Lerp(minimumTimelineZoom, maximumTimelineZoom, sliderValue);
    zoomText->SetText(FText::Format(
        LOCTEXT("ZoomPercent", "{0}%"),
        FText::AsNumber(FMath::RoundToInt(timelineZoom * 100.0f))));
    refreshTimeline();
}

void SHandposeTimelineWidget::loadAnimation(UAnimSequenceBase* animation)
{
    loadedAnimation = animation;
    playbackController->setAnimation(animation);
    segments.Reset();
    selectedSegmentIndex = INDEX_NONE;

    if (animation == nullptr)
    {
        animationNameText->SetText(LOCTEXT("InvalidAnimation", "Invalid animation"));
        statusText->SetText(LOCTEXT("InvalidAnimationStatus", "The selected asset is not an animation."));
        refreshTimeline();
        return;
    }

    animationNameText->SetText(FText::FromString(animation->GetName()));
    FHandshapeSegmentationSettings settings;
    if (const UHandposeTimelineConfig* config = loadedConfig.Get())
    {
        settings = config->getSegmentationSettings();
    }
    else
    {
        settings.boneNames = getSegmentationBoneNames(*animation);
    }
    if (settings.boneNames.IsEmpty())
    {
        statusText->SetText(LOCTEXT(
            "NoFingerBones",
            "No finger bones were found. Expected bone names containing thumb, index, middle, ring, pinky, little, or finger."));
        refreshTimeline();
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
        refreshTimeline();
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
    refreshTimeline();
}

void SHandposeTimelineWidget::loadConfig(UHandposeTimelineConfig* config)
{
    loadedConfig = config;
    if (config == nullptr)
    {
        configNameText->SetText(LOCTEXT("InvalidConfig", "Invalid config"));
        statusText->SetText(LOCTEXT(
            "InvalidConfigStatus",
            "The selected asset is not a Handpose Timeline config."));
        return;
    }

    configNameText->SetText(FText::FromString(config->GetName()));
    statusText->SetText(FText::Format(
        LOCTEXT(
            "ConfigLoaded",
            "Loaded {0}: {1} segmentation bones, handshape folder {2}, and {3} available handshape poses."),
        FText::FromString(config->GetName()),
        FText::AsNumber(config->segmentationSettings.boneNames.Num()),
        FText::FromString(config->getHandshapeContentPath()),
        FText::AsNumber(config->getHandshapePoses().Num())));

    if (loadedAnimation.IsValid())
    {
        loadAnimation(loadedAnimation.Get());
    }
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
    refreshTimeline();

    if (!shouldPreview)
    {
        return;
    }

    const EHandposeTimelinePlaybackResult result = playbackController->previewSegment(segment);
    if (result == EHandposeTimelinePlaybackResult::MissingAnimationOrActor)
    {
        statusText->SetText(LOCTEXT(
            "PreviewRequiresActor",
            "Select a compatible scene actor with Use Selected Actor to preview this segment."));
    }
    else if (result == EHandposeTimelinePlaybackResult::IncompatibleSkeleton)
    {
        statusText->SetText(LOCTEXT(
            "IncompatiblePreviewActor",
            "The selected actor's skeletal mesh is not compatible with the loaded animation."));
    }
    else if (result == EHandposeTimelinePlaybackResult::InvalidFrameRate)
    {
        statusText->SetText(LOCTEXT(
            "InvalidPreviewFrameRate",
            "The loaded animation has an invalid sampling frame rate."));
    }
}

void SHandposeTimelineWidget::openHandshapePicker(const int32 segmentIndex)
{
    if (!segments.IsValidIndex(segmentIndex))
    {
        return;
    }

    if (const TSharedPtr<SWindow> existingWindow = handshapePickerWindow.Pin())
    {
        existingWindow->RequestDestroyWindow();
    }

    FString errorMessage;
    TArray<FHandshapeCandidate> candidates = getCandidatesForSegment(
        segments[segmentIndex],
        errorMessage);

    const TSharedRef<SWindow> pickerWindow = SNew(SWindow)
        .Title(FText::Format(
            LOCTEXT("HandshapePickerTitle", "Handshape Picker · Segment {0}"),
            FText::AsNumber(segmentIndex + 1)))
        .ClientSize(FVector2D(920.0f, 680.0f))
        .IsTopmostWindow(true)
        .SupportsMaximize(true)
        .SupportsMinimize(false);

    const TSharedRef<SHandshapePickerWidget> pickerWidget =
        SNew(SHandshapePickerWidget)
        .SegmentIndex(segmentIndex)
        .Segment(segments[segmentIndex])
        .Candidates(MoveTemp(candidates))
        .EmptyMessage(errorMessage)
        .OnHandshapeChosen(this, &SHandposeTimelineWidget::onHandshapeChosen);
    pickerWidget->setOwnerWindow(pickerWindow);
    pickerWindow->SetContent(pickerWidget);
    FSlateApplication::Get().AddWindow(pickerWindow);
    handshapePickerWindow = pickerWindow;
}

TArray<FHandshapeCandidate> SHandposeTimelineWidget::getCandidatesForSegment(
    const FHandposeTimelineSegment& segment,
    FString& errorMessage) const
{
    TArray<FHandshapeCandidate> candidates;
    const UHandposeTimelineConfig* config = loadedConfig.Get();
    UAnimSequenceBase* animation = loadedAnimation.Get();
    if (config == nullptr)
    {
        errorMessage = TEXT("Load a Handpose Timeline config to rank handshapes.");
        return candidates;
    }
    if (animation == nullptr)
    {
        errorMessage = TEXT("Load an animation before choosing a handshape.");
        return candidates;
    }

    TArray<UAnimSequenceBase*> handshapeAnimations;
    for (const TSoftObjectPtr<UAnimSequence>& handshapeReference : config->getHandshapePoses())
    {
        if (UAnimSequence* handshapeAnimation = handshapeReference.LoadSynchronous())
        {
            handshapeAnimations.Add(handshapeAnimation);
        }
    }
    if (handshapeAnimations.IsEmpty())
    {
        errorMessage = TEXT("The loaded config does not resolve any handshape animations.");
        return candidates;
    }

    const FHandshapeMatchingResult matchingResult =
        UHandshapeMatchingBlueprintLibrary::matchSegmentToHandshapes(
            animation,
            segment.startFrame,
            segment.endFrame,
            handshapeAnimations,
            FAnimPoseEvaluationOptions(),
            config->getSegmentationSettings(),
            config->matchingMethod);
    if (!matchingResult.success)
    {
        errorMessage = matchingResult.errorMessage;
        return candidates;
    }

    candidates.Reserve(matchingResult.matches.Num());
    for (const FHandshapeMatch& match : matchingResult.matches)
    {
        FHandshapeCandidate& candidate = candidates.AddDefaulted_GetRef();
        candidate.handshapeId = match.handshapeId;
        candidate.displayName = FText::FromName(match.handshapeId);
        candidate.handshapeAnimation = match.handshapeAnimation;
        candidate.relevanceScore = 1.0 / (1.0 + FMath::Max(0.0, match.distance));
    }
    FHandshapeCandidate::sortByRelevance(candidates);
    return candidates;
}

void SHandposeTimelineWidget::refreshTimeline()
{
    if (timelineTrack.IsValid())
    {
        timelineTrack->setTimeline(
            loadedAnimation.Get(),
            segments,
            selectedSegmentIndex,
            timelineZoom);
    }
}

float SHandposeTimelineWidget::getCurrentTimeSeconds() const
{
    return playbackController.IsValid()
        ? playbackController->getCurrentTimeSeconds()
        : 0.0f;
}

FText SHandposeTimelineWidget::getPlayPauseButtonText() const
{
    return playbackController.IsValid() && playbackController->isPlaying()
        ? LOCTEXT("PausePlayback", "Pause")
        : LOCTEXT("PlayPlayback", "Play");
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
        FMath::FloorToInt(getCurrentTimeSeconds() * framesPerSecond));
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

#undef LOCTEXT_NAMESPACE
