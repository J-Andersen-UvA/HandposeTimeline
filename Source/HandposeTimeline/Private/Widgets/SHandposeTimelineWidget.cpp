#include "Widgets/SHandposeTimelineWidget.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "AnimPose.h"
#include "AssetRegistry/AssetData.h"
#include "Baking/HandposeBakeService.h"
#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "HandshapeSegmentationBlueprintLibrary.h"
#include "HandshapeSegmentationTypes.h"
#include "HandshapeMatchingBlueprintLibrary.h"
#include "HandshapeMatchingTypes.h"
#include "HandposeAsset.h"
#include "HandposeDecisionDataService.h"
#include "HandposeTimelineConfig.h"
#include "IContentBrowserSingleton.h"
#include "IDesktopPlatform.h"
#include "InputCoreTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Playback/HandposeTimelinePlaybackController.h"
#include "Preview/HandposePreviewBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SHandshapePickerWidget.h"
#include "Widgets/SHandposeFingerSelectionWidget.h"
#include "Widgets/SHandposeTimelineTrack.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHandposeTimelineWidget"

namespace
{
constexpr float minimumTimelineZoom = 1.0f;
constexpr float maximumTimelineZoom = 5.0f;
const TCHAR* configPersistenceSection = TEXT("HandposeTimeline");
const TCHAR* configPersistenceKey = TEXT("LastConfigAsset");

FString handSideToString(const EHandposeSide handSide)
{
    return handSide == EHandposeSide::Left ? TEXT("Left") : TEXT("Right");
}

FString fingerToString(const EHandposeFinger finger)
{
    switch (finger)
    {
    case EHandposeFinger::Thumb:
        return TEXT("Thumb");
    case EHandposeFinger::Index:
        return TEXT("Index");
    case EHandposeFinger::Middle:
        return TEXT("Middle");
    case EHandposeFinger::Ring:
        return TEXT("Ring");
    case EHandposeFinger::Pinky:
        return TEXT("Pinky");
    default:
        return TEXT("Unknown");
    }
}
}

class SHandposeTimelineVerticalScrollBox : public SScrollBox
{
public:
    void setOnZoomWheel(TFunction<void(float)> callback)
    {
        onZoomWheel = MoveTemp(callback);
    }

    void setOnHorizontalWheel(TFunction<void(float)> callback)
    {
        onHorizontalWheel = MoveTemp(callback);
    }

    virtual FReply OnMouseWheel(
        const FGeometry& myGeometry,
        const FPointerEvent& mouseEvent) override
    {
        if (mouseEvent.IsControlDown() && onZoomWheel)
        {
            onZoomWheel(mouseEvent.GetWheelDelta());
            return FReply::Handled();
        }
        if (mouseEvent.IsShiftDown() && onHorizontalWheel)
        {
            onHorizontalWheel(mouseEvent.GetWheelDelta());
            return FReply::Handled();
        }
        return SScrollBox::OnMouseWheel(myGeometry, mouseEvent);
    }

private:
    TFunction<void(float)> onZoomWheel;
    TFunction<void(float)> onHorizontalWheel;
};

void SHandposeTimelineWidget::Construct(const FArguments& arguments)
{
    playbackController = MakeShared<FHandposeTimelinePlaybackController>();
    TSharedPtr<SHandposeTimelineVerticalScrollBox> verticalTimelineScrollBox;

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
            .Padding(8.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("Bake", "Bake"))
                .ToolTipText(LOCTEXT(
                    "BakeTooltip",
                    "Bake the current handshape preview to the configured output folder."))
                .OnClicked(this, &SHandposeTimelineWidget::onBakeClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(8.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("SaveDecisionData", "Save Decision Data"))
                .ToolTipText(LOCTEXT(
                    "SaveDecisionDataTooltip",
                    "Capture the final timeline decisions and immediately write the Data Asset and JSON."))
                .OnClicked(this, &SHandposeTimelineWidget::onSaveDecisionDataClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(8.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("ImportDecisionData", "Import Decision Data"))
                .ToolTipText(LOCTEXT(
                    "ImportDecisionDataTooltip",
                    "Import decision-data JSON and create or update its Data Asset."))
                .OnClicked(this, &SHandposeTimelineWidget::onImportDecisionDataClicked)
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
                    .Value_Lambda(
                        [this]()
                        {
                            return (timelineZoom - minimumTimelineZoom) /
                                (maximumTimelineZoom - minimumTimelineZoom);
                        })
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
            SAssignNew(horizontalTimelineScrollBox, SScrollBox)
            .Orientation(Orient_Horizontal)
            + SScrollBox::Slot()
            [
                SAssignNew(verticalTimelineScrollBox, SHandposeTimelineVerticalScrollBox)
                .Orientation(Orient_Vertical)
                + SScrollBox::Slot()
                .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                [
                    SAssignNew(rightTimelineTrack, SHandposeTimelineTrack)
                    .HandSide(EHandposeSide::Right)
                    .CurrentTimeSeconds(this, &SHandposeTimelineWidget::getCurrentTimeSeconds)
                    .OnSegmentSelected(this, &SHandposeTimelineWidget::onSegmentSelected)
                    .OnScrubbed(this, &SHandposeTimelineWidget::onPlayheadScrubbed)
                    .OnAssignmentSelected(this, &SHandposeTimelineWidget::onAssignmentSelected)
                    .OnAssignmentChanged(this, &SHandposeTimelineWidget::onAssignmentChanged)
                    .OnAssignmentFingerEditRequested(
                        this,
                        &SHandposeTimelineWidget::onAssignmentFingerEditRequested)
                ]
                + SScrollBox::Slot()
                [
                    SAssignNew(leftTimelineTrack, SHandposeTimelineTrack)
                    .HandSide(EHandposeSide::Left)
                    .CurrentTimeSeconds(this, &SHandposeTimelineWidget::getCurrentTimeSeconds)
                    .OnSegmentSelected(this, &SHandposeTimelineWidget::onSegmentSelected)
                    .OnScrubbed(this, &SHandposeTimelineWidget::onPlayheadScrubbed)
                    .OnAssignmentSelected(this, &SHandposeTimelineWidget::onAssignmentSelected)
                    .OnAssignmentChanged(this, &SHandposeTimelineWidget::onAssignmentChanged)
                    .OnAssignmentFingerEditRequested(
                        this,
                        &SHandposeTimelineWidget::onAssignmentFingerEditRequested)
                ]
            ]
        ]
    ];

    verticalTimelineScrollBox->setOnZoomWheel(
        [this](const float wheelDelta)
        {
            onZoomWheel(wheelDelta);
        });
    verticalTimelineScrollBox->setOnHorizontalWheel(
        [this](const float wheelDelta)
        {
            if (horizontalTimelineScrollBox.IsValid())
            {
                horizontalTimelineScrollBox->SetScrollOffset(
                    horizontalTimelineScrollBox->GetScrollOffset() - wheelDelta * 48.0f);
            }
        });

    refreshTimeline();
    restoreLastConfig();
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
    if (keyEvent.GetKey() == EKeys::SpaceBar && !keyEvent.IsRepeat())
    {
        return onPlayPauseClicked();
    }
    if (keyEvent.GetKey() == EKeys::Delete)
    {
        removeSelectedAssignment();
        return FReply::Handled();
    }
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

FReply SHandposeTimelineWidget::OnPreviewMouseButtonDown(
    const FGeometry& myGeometry,
    const FPointerEvent& mouseEvent)
{
    FSlateApplication::Get().SetKeyboardFocus(
        SharedThis(this),
        EFocusCause::Mouse);
    return SCompoundWidget::OnPreviewMouseButtonDown(myGeometry, mouseEvent);
}

FReply SHandposeTimelineWidget::OnMouseWheel(
    const FGeometry& myGeometry,
    const FPointerEvent& mouseEvent)
{
    if (mouseEvent.IsControlDown())
    {
        onZoomWheel(mouseEvent.GetWheelDelta());
        return FReply::Handled();
    }
    return SCompoundWidget::OnMouseWheel(myGeometry, mouseEvent);
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

    const TArray<FHandposeTimelineSegment>& selectedSegments =
        getSegments(selectedHandSide);
    const int32 selectedSegmentIndex = selectedHandSide == EHandposeSide::Left
        ? selectedLeftSegmentIndex
        : selectedRightSegmentIndex;
    if (playbackController->shouldLoopSegment() &&
        !selectedSegments.IsValidIndex(selectedSegmentIndex))
    {
        statusText->SetText(LOCTEXT(
            "LoopSegmentRequiresSelection",
            "Select a segment before using Loop Segment playback."));
        return FReply::Handled();
    }

    const EHandposeTimelinePlaybackResult result =
        playbackController->shouldLoopSegment()
            ? playbackController->previewSegment(selectedSegments[selectedSegmentIndex])
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

FReply SHandposeTimelineWidget::onBakeClicked()
{
    UAnimSequence* outputAnimation = nullptr;
    FString outputPath;
    FString errorMessage;
    bool cancelled = false;
    const bool success = bakeCurrentAnimation(
        outputAnimation,
        outputPath,
        errorMessage,
        cancelled);
    if (cancelled)
    {
        statusText->SetText(LOCTEXT("BakeCancelled", "Bake cancelled."));
    }
    else if (!success)
    {
        statusText->SetText(FText::FromString(errorMessage));
    }
    else
    {
        statusText->SetText(FText::Format(
            LOCTEXT("BakeSucceeded", "Baked {0} to {1}."),
            FText::FromString(outputAnimation->GetName()),
            FText::FromString(outputPath)));
    }
    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onSaveDecisionDataClicked()
{
    const FHandposeDecisionDataResult result = captureDecisionData();
    statusText->SetText(result.success
        ? FText::Format(
            LOCTEXT("DecisionDataSaved", "Saved decision data to {0} and {1}."),
            FText::FromString(result.assetPath),
            FText::FromString(result.jsonPath))
        : FText::FromString(result.errorMessage));
    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onImportDecisionDataClicked()
{
    IDesktopPlatform* desktopPlatform = FDesktopPlatformModule::Get();
    if (desktopPlatform == nullptr)
    {
        statusText->SetText(LOCTEXT(
            "DecisionDataImportUnavailable",
            "The platform file dialog is unavailable."));
        return FReply::Handled();
    }

    TArray<FString> selectedFiles;
    const FString defaultPath = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("HandposeTimeline"),
        TEXT("DecisionData"));
    if (!desktopPlatform->OpenFileDialog(
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
        TEXT("Import HandposeTimeline Decision Data"),
        defaultPath,
        FString(),
        TEXT("HandposeTimeline Decision Data (*.json)|*.json"),
        EFileDialogFlags::None,
        selectedFiles) || selectedFiles.IsEmpty())
    {
        return FReply::Handled();
    }

    const FHandposeDecisionDataResult result =
        FHandposeDecisionDataService::importJson(selectedFiles[0]);
    statusText->SetText(result.success
        ? FText::Format(
            LOCTEXT("DecisionDataImported", "Imported decision data to {0}."),
            FText::FromString(result.assetPath))
        : FText::FromString(result.errorMessage));
    return FReply::Handled();
}

bool SHandposeTimelineWidget::canBakeCurrentAnimation() const
{
    return Cast<UAnimSequence>(loadedAnimation.Get()) != nullptr &&
        loadedConfig.IsValid() &&
        hasAssignments();
}

bool SHandposeTimelineWidget::hasAssignments() const
{
    return !rightAssignments.IsEmpty() || !leftAssignments.IsEmpty();
}

bool SHandposeTimelineWidget::bakeCurrentAnimation(
    UAnimSequence*& outputAnimation,
    FString& outputPath,
    FString& errorMessage,
    bool& cancelled)
{
    outputAnimation = nullptr;
    outputPath.Reset();
    errorMessage.Reset();
    cancelled = false;

    UAnimSequence* sourceAnimation = Cast<UAnimSequence>(loadedAnimation.Get());
    UHandposeTimelineConfig* config = loadedConfig.Get();
    if (sourceAnimation == nullptr || config == nullptr)
    {
        errorMessage = TEXT("Load an animation and config before baking.");
        return false;
    }
    if (!hasAssignments())
    {
        errorMessage = TEXT("Assign at least one handshape before baking.");
        return false;
    }
    if (!previewAnimation.IsValid())
    {
        rebuildPreviewAnimation(false);
    }

    const FHandposeBakeResult result = FHandposeBakeService::bake(
        previewAnimation.Get(),
        sourceAnimation,
        config->getBakedAnimationOutputPath());
    outputAnimation = result.animation;
    outputPath = result.outputPath;
    errorMessage = result.errorMessage;
    cancelled = result.cancelled;
    if (result.success)
    {
        const FHandposeDecisionDataResult decisionResult = captureDecisionData();
        if (!decisionResult.success)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("HandposeTimeline could not capture decision data: %s"),
                *decisionResult.errorMessage);
        }
    }
    return result.success;
}

UAnimSequenceBase* SHandposeTimelineWidget::getLoadedAnimation() const
{
    return loadedAnimation.Get();
}

void SHandposeTimelineWidget::onSegmentSelected(
    const EHandposeSide handSide,
    const int32 segmentIndex)
{
    selectSegment(handSide, segmentIndex, true);
    openHandshapePicker(handSide, segmentIndex);
}

void SHandposeTimelineWidget::onHandshapeChosen(
    const int32 segmentIndex,
    const FHandshapeCandidate& candidate)
{
    const EHandposeSide handSide = candidate.handSide;
    TArray<FHandposeTimelineSegment>& segments = getSegments(handSide);
    TArray<FHandposeAssignment>& assignments = getAssignments(handSide);
    int32& selectedSegmentIndex = getSelectedSegmentIndex(handSide);
    int32& selectedAssignmentIndex = getSelectedAssignmentIndex(handSide);
    if (!segments.IsValidIndex(segmentIndex) || !candidate.isValid())
    {
        return;
    }

    segments[segmentIndex].selectedHandshapeId = candidate.handshapeId;
    selectedHandSide = handSide;
    selectedSegmentIndex = segmentIndex;
    selectedAssignmentIndex = assignments.IndexOfByPredicate(
        [segmentIndex](const FHandposeAssignment& assignment)
        {
            return assignment.sourceSegmentIndex == segmentIndex ||
                assignment.coveredSegmentIndices.Contains(segmentIndex);
        });
    if (selectedAssignmentIndex == INDEX_NONE)
    {
        const FHandposeTimelineSegment& segment = segments[segmentIndex];
        const int32 lastFrame = loadedAnimation.IsValid()
            ? FMath::Max(0, loadedAnimation->GetNumberOfSampledKeys() - 1)
            : segment.endFrame;
        const int32 previousSegmentEnd = segmentIndex > 0
            ? segments[segmentIndex - 1].endFrame
            : -1;
        const int32 nextSegmentStart = segments.IsValidIndex(segmentIndex + 1)
            ? segments[segmentIndex + 1].startFrame
            : lastFrame + 1;
        FHandposeAssignment& assignment = assignments.AddDefaulted_GetRef();
        assignment.assignmentId = FGuid::NewGuid();
        assignment.sourceSegmentIndex = segmentIndex;
        assignment.blendStartFrame = FMath::Clamp(
            previousSegmentEnd + 1,
            0,
            segment.startFrame);
        assignment.fullPoseStartFrame = segment.startFrame;
        assignment.fullPoseEndFrame = segment.endFrame;
        assignment.blendEndFrame = FMath::Clamp(
            nextSegmentStart - 1,
            segment.endFrame,
            lastFrame);
        selectedAssignmentIndex = assignments.Num() - 1;
    }
    FHandposeAssignment& assignment = assignments[selectedAssignmentIndex];
    assignment.handshapeId = candidate.handshapeId;
    assignment.handshapeAnimation = candidate.handshapeAnimation;
    assignment.handSide = handSide;
    assignment.skippedFingers = candidate.skippedFingers;
    linkOverlappingAssignments(handSide);
    updateAssignmentCoverage(handSide);
    statusText->SetText(FText::Format(
        LOCTEXT(
            "HandshapeAssigned",
            "Assigned {0} to Segment {1}."),
        FText::FromName(candidate.handshapeId),
        FText::AsNumber(segmentIndex + 1)));
    rebuildPreviewAnimation(true);
    refreshTimeline();
}

void SHandposeTimelineWidget::onAssignmentSelected(
    const EHandposeSide handSide,
    const int32 assignmentIndex)
{
    TArray<FHandposeAssignment>& assignments = getAssignments(handSide);
    int32& selectedAssignmentIndex = getSelectedAssignmentIndex(handSide);
    int32& selectedSegmentIndex = getSelectedSegmentIndex(handSide);
    if (!assignments.IsValidIndex(assignmentIndex))
    {
        return;
    }
    selectedAssignmentIndex = assignmentIndex;
    selectedHandSide = handSide;
    selectedSegmentIndex = assignments[assignmentIndex].sourceSegmentIndex;
    const FHandposeAssignment& assignment = assignments[assignmentIndex];
    statusText->SetText(FText::Format(
        LOCTEXT(
            "AssignmentSelected",
            "{0}: influence frames {1}-{2}, full pose frames {3}-{4}."),
        FText::FromName(assignment.handshapeId),
        FText::AsNumber(assignment.blendStartFrame),
        FText::AsNumber(assignment.blendEndFrame),
        FText::AsNumber(assignment.fullPoseStartFrame),
        FText::AsNumber(assignment.fullPoseEndFrame)));
    refreshTimeline();
}

void SHandposeTimelineWidget::onAssignmentChanged(
    const EHandposeSide handSide,
    const int32 assignmentIndex,
    const FHandposeAssignment& assignment,
    const bool isFinal)
{
    TArray<FHandposeAssignment>& assignments = getAssignments(handSide);
    int32& selectedAssignmentIndex = getSelectedAssignmentIndex(handSide);
    if (!assignments.IsValidIndex(assignmentIndex))
    {
        return;
    }
    assignments[assignmentIndex] = assignment;
    assignments[assignmentIndex].handSide = handSide;
    selectedAssignmentIndex = assignmentIndex;
    selectedHandSide = handSide;
    statusText->SetText(FText::Format(
        LOCTEXT(
            "AssignmentChanged",
            "{0}: influence frames {1}-{2}, full pose frames {3}-{4}."),
        FText::FromName(assignment.handshapeId),
        FText::AsNumber(assignment.blendStartFrame),
        FText::AsNumber(assignment.blendEndFrame),
        FText::AsNumber(assignment.fullPoseStartFrame),
        FText::AsNumber(assignment.fullPoseEndFrame)));
    if (isFinal)
    {
        linkOverlappingAssignments(handSide);
        updateAssignmentCoverage(handSide);
        rebuildPreviewAnimation(true);
        refreshTimeline();
    }
}

void SHandposeTimelineWidget::onAssignmentFingerEditRequested(
    const EHandposeSide handSide,
    const int32 assignmentIndex)
{
    const TArray<FHandposeAssignment>& assignments = getAssignments(handSide);
    if (!assignments.IsValidIndex(assignmentIndex))
    {
        return;
    }

    const FHandposeAssignment& assignment = assignments[assignmentIndex];
    const TSharedRef<SWindow> fingerWindow = SNew(SWindow)
        .Title(FText::Format(
            LOCTEXT("EditSkippedFingersTitle", "Skip Fingers · {0}"),
            FText::FromName(assignment.handshapeId)))
        .ClientSize(FVector2D(390.0f, 300.0f))
        .SupportsMaximize(false)
        .SupportsMinimize(false);
    const TSharedRef<SHandposeFingerSelectionWidget> fingerWidget =
        SNew(SHandposeFingerSelectionWidget)
        .InitialSkippedFingers(assignment.skippedFingers)
        .OnSkippedFingersChosen(
            this,
            &SHandposeTimelineWidget::onAssignmentSkippedFingersChosen,
            handSide,
            assignmentIndex);
    fingerWidget->setOwnerWindow(fingerWindow);
    fingerWindow->SetContent(fingerWidget);

    const TSharedPtr<SWindow> parentWindow =
        FSlateApplication::Get().FindWidgetWindow(SharedThis(this));
    if (parentWindow.IsValid())
    {
        FSlateApplication::Get().AddWindowAsNativeChild(
            fingerWindow,
            parentWindow.ToSharedRef());
    }
    else
    {
        FSlateApplication::Get().AddWindow(fingerWindow);
    }
}

void SHandposeTimelineWidget::onAssignmentSkippedFingersChosen(
    const TArray<EHandposeFinger>& skippedFingers,
    const EHandposeSide handSide,
    const int32 assignmentIndex)
{
    TArray<FHandposeAssignment>& assignments = getAssignments(handSide);
    if (!assignments.IsValidIndex(assignmentIndex))
    {
        return;
    }

    assignments[assignmentIndex].skippedFingers = skippedFingers;
    selectedHandSide = handSide;
    getSelectedAssignmentIndex(handSide) = assignmentIndex;
    statusText->SetText(FText::Format(
        LOCTEXT(
            "AssignmentSkippedFingersChanged",
            "Updated skipped fingers for {0}."),
        FText::FromName(assignments[assignmentIndex].handshapeId)));
    rebuildPreviewAnimation(true);
    refreshTimeline();
}

void SHandposeTimelineWidget::removeSelectedAssignment()
{
    TArray<FHandposeAssignment>& assignments = getAssignments(selectedHandSide);
    int32& selectedAssignmentIndex = getSelectedAssignmentIndex(selectedHandSide);
    if (!assignments.IsValidIndex(selectedAssignmentIndex))
    {
        return;
    }

    const FHandposeAssignment removedAssignment = assignments[selectedAssignmentIndex];
    assignments.RemoveAt(selectedAssignmentIndex);
    selectedAssignmentIndex = INDEX_NONE;
    updateAssignmentCoverage(selectedHandSide);

    statusText->SetText(FText::Format(
        LOCTEXT("AssignmentRemoved", "Removed assigned handshape {0}."),
        FText::FromName(removedAssignment.handshapeId)));
    rebuildPreviewAnimation(true);
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

void SHandposeTimelineWidget::onZoomWheel(const float wheelDelta)
{
    constexpr float zoomStep = 0.05f;
    const float currentSliderValue =
        (timelineZoom - minimumTimelineZoom) /
        (maximumTimelineZoom - minimumTimelineZoom);
    onZoomChanged(FMath::Clamp(
        currentSliderValue + wheelDelta * zoomStep,
        0.0f,
        1.0f));
}

void SHandposeTimelineWidget::loadAnimation(UAnimSequenceBase* animation)
{
    loadedAnimation = animation;
    previewAnimation.Reset();
    playbackController->setAnimation(animation);
    rightSegments.Reset();
    leftSegments.Reset();
    rightAssignments.Reset();
    leftAssignments.Reset();
    selectedRightSegmentIndex = INDEX_NONE;
    selectedLeftSegmentIndex = INDEX_NONE;
    selectedRightAssignmentIndex = INDEX_NONE;
    selectedLeftAssignmentIndex = INDEX_NONE;
    selectedHandSide = EHandposeSide::Right;

    if (animation == nullptr)
    {
        animationNameText->SetText(LOCTEXT("InvalidAnimation", "Invalid animation"));
        statusText->SetText(LOCTEXT("InvalidAnimationStatus", "The selected asset is not an animation."));
        refreshTimeline();
        return;
    }

    animationNameText->SetText(FText::FromString(animation->GetName()));
    FString errorMessage;
    if (!generateSegments(
            *animation,
            EHandposeSide::Right,
            rightSegments,
            errorMessage) ||
        !generateSegments(
            *animation,
            EHandposeSide::Left,
            leftSegments,
            errorMessage))
    {
        statusText->SetText(FText::FromString(errorMessage));
        refreshTimeline();
        return;
    }

    statusText->SetText(FText::Format(
        LOCTEXT(
            "SegmentationComplete",
            "Generated {0} right-hand segments and {1} left-hand segments."),
        FText::AsNumber(rightSegments.Num()),
        FText::AsNumber(leftSegments.Num())));
    refreshTimeline();
}

void SHandposeTimelineWidget::resetSession()
{
    if (const TSharedPtr<SWindow> pickerWindow = handshapePickerWindow.Pin())
    {
        pickerWindow->RequestDestroyWindow();
    }
    handshapePickerWindow.Reset();

    playbackController->pause();
    playbackController->setActor(nullptr);
    playbackController = MakeShared<FHandposeTimelinePlaybackController>();
    loadedAnimation.Reset();
    previewAnimation.Reset();
    rightSegments.Reset();
    leftSegments.Reset();
    rightAssignments.Reset();
    leftAssignments.Reset();
    selectedRightSegmentIndex = INDEX_NONE;
    selectedLeftSegmentIndex = INDEX_NONE;
    selectedRightAssignmentIndex = INDEX_NONE;
    selectedLeftAssignmentIndex = INDEX_NONE;
    selectedHandSide = EHandposeSide::Right;
    timelineZoom = minimumTimelineZoom;

    if (horizontalTimelineScrollBox.IsValid())
    {
        horizontalTimelineScrollBox->SetScrollOffset(0.0f);
    }
    animationNameText->SetText(LOCTEXT("NoAnimationAfterReset", "No animation loaded"));
    actorNameText->SetText(LOCTEXT("NoActorAfterReset", "No actor selected"));
    statusText->SetText(LOCTEXT(
        "SessionReset",
        "Load an animation to generate segments."));
    speedText->SetText(LOCTEXT("ResetPreviewSpeed", "Speed: 1x"));
    zoomText->SetText(LOCTEXT("ResetZoom", "100%"));
    refreshTimeline();
}

bool SHandposeTimelineWidget::generateSegments(
    UAnimSequenceBase& animation,
    const EHandposeSide handSide,
    TArray<FHandposeTimelineSegment>& outputSegments,
    FString& errorMessage) const
{
    FHandshapeSegmentationSettings settings;
    if (const UHandposeTimelineConfig* config = loadedConfig.Get())
    {
        settings = config->getSegmentationSettings(handSide);
    }
    else
    {
        settings.boneNames = getSegmentationBoneNames(animation, handSide);
    }
    if (settings.boneNames.IsEmpty())
    {
        errorMessage = handSide == EHandposeSide::Left
            ? TEXT("No left-hand finger bones were found.")
            : TEXT("No right-hand finger bones were found.");
        return false;
    }

    const FHandshapeSegmentationResult result =
        UHandshapeSegmentationBlueprintLibrary::segmentAnimation(
            &animation,
            FAnimPoseEvaluationOptions(),
            settings);
    if (!result.success)
    {
        errorMessage = result.errorMessage;
        return false;
    }

    outputSegments.Reserve(result.segments.Num());
    for (const FHandshapeSegment& sourceSegment : result.segments)
    {
        FHandposeTimelineSegment& segment = outputSegments.AddDefaulted_GetRef();
        segment.startFrame = sourceSegment.startFrame;
        segment.endFrame = sourceSegment.endFrame;
        segment.durationSeconds = FMath::Max(
            0.0,
            sourceSegment.endTimeSeconds - sourceSegment.startTimeSeconds);
    }
    return true;
}

void SHandposeTimelineWidget::loadConfig(UHandposeTimelineConfig* config)
{
    loadedConfig.Reset(config);
    if (config == nullptr)
    {
        configNameText->SetText(LOCTEXT("InvalidConfig", "Invalid config"));
        statusText->SetText(LOCTEXT(
            "InvalidConfigStatus",
            "The selected asset is not a Handpose Timeline config."));
        return;
    }

    saveLastConfig(*config);
    configNameText->SetText(FText::FromString(config->GetName()));
    statusText->SetText(FText::Format(
        LOCTEXT(
            "ConfigLoaded",
            "Loaded {0}: {1} right-hand bones, {2} left-hand bones, handshape folder {3}, and {4} available handshape entries."),
        FText::FromString(config->GetName()),
        FText::AsNumber(config->rightHandBoneNames.Num()),
        FText::AsNumber(config->leftHandBoneNames.Num()),
        FText::FromString(config->getHandshapeContentPath()),
        FText::AsNumber(
            config->getHandposeAssets().Num() + config->getHandshapePoses().Num())));

    if (loadedAnimation.IsValid())
    {
        loadAnimation(loadedAnimation.Get());
    }
}

void SHandposeTimelineWidget::restoreLastConfig()
{
    if (GConfig == nullptr)
    {
        return;
    }

    FString configAssetPath;
    if (!GConfig->GetString(
            configPersistenceSection,
            configPersistenceKey,
            configAssetPath,
            GEditorPerProjectIni) ||
        configAssetPath.IsEmpty())
    {
        return;
    }

    if (UHandposeTimelineConfig* config = Cast<UHandposeTimelineConfig>(
            FSoftObjectPath(configAssetPath).TryLoad()))
    {
        loadConfig(config);
    }
}

void SHandposeTimelineWidget::saveLastConfig(
    const UHandposeTimelineConfig& config) const
{
    if (GConfig == nullptr)
    {
        return;
    }

    GConfig->SetString(
        configPersistenceSection,
        configPersistenceKey,
        *config.GetPathName(),
        GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

void SHandposeTimelineWidget::selectSegment(
    const EHandposeSide handSide,
    const int32 segmentIndex,
    const bool shouldPreview)
{
    TArray<FHandposeTimelineSegment>& segments = getSegments(handSide);
    int32& selectedSegmentIndex = getSelectedSegmentIndex(handSide);
    if (!segments.IsValidIndex(segmentIndex))
    {
        return;
    }

    selectedHandSide = handSide;
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

void SHandposeTimelineWidget::openHandshapePicker(
    const EHandposeSide handSide,
    const int32 segmentIndex)
{
    const TArray<FHandposeTimelineSegment>& segments = getSegments(handSide);
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
        handSide,
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
    const EHandposeSide handSide,
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
    TMultiMap<UAnimSequenceBase*, UHandposeAsset*> assetsByPredictionHandpose;
    for (const TSoftObjectPtr<UHandposeAsset>& assetReference : config->getHandposeAssets())
    {
        UHandposeAsset* handposeAsset = assetReference.LoadSynchronous();
        if (handposeAsset == nullptr || !handposeAsset->isValid())
        {
            continue;
        }
        if (handposeAsset->handSide != handSide)
        {
            continue;
        }

        UAnimSequence* predictionHandpose = handposeAsset->predictionHandpose.LoadSynchronous();
        UAnimSequence* postProcessedHandpose =
            handposeAsset->postProcessedHandpose.LoadSynchronous();
        if (predictionHandpose == nullptr || postProcessedHandpose == nullptr)
        {
            continue;
        }

        handshapeAnimations.AddUnique(predictionHandpose);
        assetsByPredictionHandpose.Add(predictionHandpose, handposeAsset);
    }
    for (const TSoftObjectPtr<UAnimSequence>& handshapeReference : config->getHandshapePoses())
    {
        if (handSide != EHandposeSide::Right)
        {
            break;
        }
        if (UAnimSequence* handshapeAnimation = handshapeReference.LoadSynchronous())
        {
            handshapeAnimations.AddUnique(handshapeAnimation);
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
            config->getSegmentationSettings(handSide),
            config->matchingMethod);
    if (!matchingResult.success)
    {
        errorMessage = matchingResult.errorMessage;
        return candidates;
    }

    candidates.Reserve(matchingResult.matches.Num());
    for (const FHandshapeMatch& match : matchingResult.matches)
    {
        TArray<UHandposeAsset*> linkedAssets;
        assetsByPredictionHandpose.MultiFind(match.handshapeAnimation, linkedAssets);
        if (linkedAssets.IsEmpty())
        {
            FHandshapeCandidate& candidate = candidates.AddDefaulted_GetRef();
            candidate.handshapeId = match.handshapeId;
            candidate.displayName = FText::FromName(match.handshapeId);
            candidate.handshapeAnimation = match.handshapeAnimation;
            candidate.handSide = handSide;
            candidate.matchingDistance = match.distance;
            candidate.relevanceScore = 1.0 / (1.0 + FMath::Max(0.0, match.distance));
            continue;
        }

        for (UHandposeAsset* handposeAsset : linkedAssets)
        {
            FHandshapeCandidate& candidate = candidates.AddDefaulted_GetRef();
            candidate.handshapeId = handposeAsset->GetFName();
            candidate.displayName = FText::FromString(handposeAsset->GetName());
            candidate.previewTexture = handposeAsset->thumbnail;
            candidate.handshapeAnimation = handposeAsset->postProcessedHandpose;
            candidate.handSide = handSide;
            candidate.matchingDistance = match.distance;
            candidate.relevanceScore = 1.0 / (1.0 + FMath::Max(0.0, match.distance));
        }
    }
    FHandshapeCandidate::sortByRelevance(candidates);
    return candidates;
}

void SHandposeTimelineWidget::rebuildPreviewAnimation(const bool preserveTime)
{
    UAnimSequence* sourceAnimation = Cast<UAnimSequence>(loadedAnimation.Get());
    const TArray<FHandposeAssignment> assignments = getAllAssignments();
    if (sourceAnimation == nullptr || assignments.IsEmpty())
    {
        previewAnimation.Reset();
        playbackController->setAnimation(loadedAnimation.Get());
        return;
    }

    const float previousTime = preserveTime ? getCurrentTimeSeconds() : 0.0f;
    const TArray<FName> rightBoneNames = loadedConfig.IsValid()
        ? loadedConfig->getSegmentationSettings(EHandposeSide::Right).boneNames
        : getSegmentationBoneNames(*sourceAnimation, EHandposeSide::Right);
    const TArray<FName> leftBoneNames = loadedConfig.IsValid()
        ? loadedConfig->getSegmentationSettings(EHandposeSide::Left).boneNames
        : getSegmentationBoneNames(*sourceAnimation, EHandposeSide::Left);
    const FHandposePreviewBuildResult result = FHandposePreviewBuilder::build(
        sourceAnimation,
        assignments,
        rightBoneNames,
        leftBoneNames);
    if (!result.success || result.animation == nullptr)
    {
        statusText->SetText(FText::Format(
            LOCTEXT("PreviewBuildFailed", "Could not build handpose preview: {0}"),
            FText::FromString(result.errorMessage)));
        return;
    }

    previewAnimation.Reset(result.animation);
    playbackController->setAnimation(previewAnimation.Get());
    if (preserveTime)
    {
        playbackController->scrubTo(FMath::Clamp(
            previousTime,
            0.0f,
            previewAnimation->GetPlayLength()));
    }
}

void SHandposeTimelineWidget::linkOverlappingAssignments(const EHandposeSide handSide)
{
    TArray<FHandposeAssignment>& assignments = getAssignments(handSide);
    TArray<int32> sortedIndices;
    sortedIndices.Reserve(assignments.Num());
    for (int32 assignmentIndex = 0; assignmentIndex < assignments.Num(); ++assignmentIndex)
    {
        sortedIndices.Add(assignmentIndex);
    }
    sortedIndices.Sort(
        [&assignments](const int32 left, const int32 right)
        {
            return assignments[left].blendStartFrame < assignments[right].blendStartFrame;
        });

    for (int32 sortedIndex = 0; sortedIndex + 1 < sortedIndices.Num(); ++sortedIndex)
    {
        FHandposeAssignment& left = assignments[sortedIndices[sortedIndex]];
        FHandposeAssignment& right = assignments[sortedIndices[sortedIndex + 1]];
        if (right.blendStartFrame > left.blendEndFrame)
        {
            continue;
        }
        left.fullPoseEndFrame = FMath::Clamp(
            right.blendStartFrame,
            left.fullPoseStartFrame,
            left.blendEndFrame);
        right.fullPoseStartFrame = FMath::Clamp(
            left.blendEndFrame,
            right.blendStartFrame,
            right.fullPoseEndFrame);
    }
}

void SHandposeTimelineWidget::updateAssignmentCoverage(
    const EHandposeSide handSide)
{
    TArray<FHandposeTimelineSegment>& segments = getSegments(handSide);
    TArray<FHandposeAssignment>& assignments = getAssignments(handSide);
    for (FHandposeTimelineSegment& segment : segments)
    {
        segment.selectedHandshapeId = NAME_None;
    }

    for (FHandposeAssignment& assignment : assignments)
    {
        assignment.coveredSegmentIndices.Reset();
        for (int32 segmentIndex = 0; segmentIndex < segments.Num(); ++segmentIndex)
        {
            FHandposeTimelineSegment& segment = segments[segmentIndex];
            if (assignment.blendStartFrame <= segment.startFrame &&
                assignment.blendEndFrame >= segment.endFrame)
            {
                assignment.coveredSegmentIndices.Add(segmentIndex);
                segment.selectedHandshapeId = assignment.handshapeId;
            }
        }
    }
}

void SHandposeTimelineWidget::refreshTimeline()
{
    if (rightTimelineTrack.IsValid())
    {
        rightTimelineTrack->setTimeline(
            loadedAnimation.Get(),
            rightSegments,
            rightAssignments,
            selectedRightSegmentIndex,
            selectedRightAssignmentIndex,
            timelineZoom);
    }
    if (leftTimelineTrack.IsValid())
    {
        leftTimelineTrack->setTimeline(
            loadedAnimation.Get(),
            leftSegments,
            leftAssignments,
            selectedLeftSegmentIndex,
            selectedLeftAssignmentIndex,
            timelineZoom);
    }
}

TArray<FHandposeTimelineSegment>& SHandposeTimelineWidget::getSegments(
    const EHandposeSide handSide)
{
    return handSide == EHandposeSide::Left ? leftSegments : rightSegments;
}

const TArray<FHandposeTimelineSegment>& SHandposeTimelineWidget::getSegments(
    const EHandposeSide handSide) const
{
    return handSide == EHandposeSide::Left ? leftSegments : rightSegments;
}

TArray<FHandposeAssignment>& SHandposeTimelineWidget::getAssignments(
    const EHandposeSide handSide)
{
    return handSide == EHandposeSide::Left ? leftAssignments : rightAssignments;
}

const TArray<FHandposeAssignment>& SHandposeTimelineWidget::getAssignments(
    const EHandposeSide handSide) const
{
    return handSide == EHandposeSide::Left ? leftAssignments : rightAssignments;
}

int32& SHandposeTimelineWidget::getSelectedSegmentIndex(const EHandposeSide handSide)
{
    return handSide == EHandposeSide::Left
        ? selectedLeftSegmentIndex
        : selectedRightSegmentIndex;
}

int32& SHandposeTimelineWidget::getSelectedAssignmentIndex(
    const EHandposeSide handSide)
{
    return handSide == EHandposeSide::Left
        ? selectedLeftAssignmentIndex
        : selectedRightAssignmentIndex;
}

TArray<FHandposeAssignment> SHandposeTimelineWidget::getAllAssignments() const
{
    TArray<FHandposeAssignment> assignments = rightAssignments;
    assignments.Append(leftAssignments);
    return assignments;
}

FHandposeDecisionDataResult SHandposeTimelineWidget::captureDecisionData() const
{
    UAnimSequenceBase* animation = loadedAnimation.Get();
    const UHandposeTimelineConfig* config = loadedConfig.Get();
    if (animation == nullptr || config == nullptr)
    {
        FHandposeDecisionDataResult result;
        result.errorMessage = TEXT("Load an animation and config before saving decision data.");
        return result;
    }

    FHandposeDecisionSessionData sessionData;
    sessionData.capturedAtUtc = FDateTime::UtcNow().ToIso8601();
    sessionData.sourceAnimationName = animation->GetName();
    sessionData.sourceAnimationPath = animation->GetPathName();
    sessionData.framesPerSecond = animation->GetSamplingFrameRate().AsDecimal();
    sessionData.totalFrames = animation->GetNumberOfSampledKeys();
    sessionData.durationSeconds = animation->GetPlayLength();
    sessionData.config.configName = config->GetName();
    sessionData.config.matchingMethod = StaticEnum<EHandshapeMatchingMethod>()->GetNameStringByValue(
        static_cast<int64>(config->matchingMethod));
    sessionData.config.minimumSegmentLength =
        config->sharedSegmentationSettings.minimumSegmentLength;
    sessionData.config.minimumAccelerationThreshold =
        config->sharedSegmentationSettings.minimumAccelerationThreshold;
    sessionData.config.quaternionSmoothingSigma =
        config->sharedSegmentationSettings.quaternionSmoothingSigma;
    sessionData.config.accelerationSmoothingSigma =
        config->sharedSegmentationSettings.accelerationSmoothingSigma;
    sessionData.config.thresholdFactor =
        config->sharedSegmentationSettings.thresholdFactor;
    for (const FName boneName : config->rightHandBoneNames)
    {
        sessionData.config.rightHandBoneNames.Add(boneName.ToString());
    }
    for (const FName boneName : config->leftHandBoneNames)
    {
        sessionData.config.leftHandBoneNames.Add(boneName.ToString());
    }

    const auto appendAssignments =
        [&sessionData](
            const EHandposeSide handSide,
            const TArray<FHandposeAssignment>& assignments)
        {
            for (const FHandposeAssignment& assignment : assignments)
            {
                FHandposeDecisionAssignmentSnapshot& snapshot =
                    sessionData.assignments.AddDefaulted_GetRef();
                snapshot.assignmentId = assignment.assignmentId.ToString(
                    EGuidFormats::DigitsWithHyphensLower);
                snapshot.handSide = handSideToString(handSide);
                snapshot.handposeId = assignment.handshapeId.ToString();
                snapshot.sourceSegmentIndex = assignment.sourceSegmentIndex;
                snapshot.coveredSegmentIndices = assignment.coveredSegmentIndices;
                snapshot.blendStartFrame = assignment.blendStartFrame;
                snapshot.fullPoseStartFrame = assignment.fullPoseStartFrame;
                snapshot.fullPoseEndFrame = assignment.fullPoseEndFrame;
                snapshot.blendEndFrame = assignment.blendEndFrame;
                for (const EHandposeFinger finger : assignment.skippedFingers)
                {
                    snapshot.skippedFingers.Add(fingerToString(finger));
                }
            }
        };
    appendAssignments(EHandposeSide::Right, rightAssignments);
    appendAssignments(EHandposeSide::Left, leftAssignments);

    const auto appendSegments =
        [this, &sessionData](
            const EHandposeSide handSide,
            const TArray<FHandposeTimelineSegment>& segments,
            const TArray<FHandposeAssignment>& assignments)
        {
            for (int32 segmentIndex = 0; segmentIndex < segments.Num(); ++segmentIndex)
            {
                const FHandposeTimelineSegment& segment = segments[segmentIndex];
                FHandposeDecisionSegmentSnapshot& snapshot =
                    sessionData.segments.AddDefaulted_GetRef();
                snapshot.handSide = handSideToString(handSide);
                snapshot.segmentIndex = segmentIndex;
                snapshot.startFrame = segment.startFrame;
                snapshot.endFrame = segment.endFrame;
                snapshot.durationSeconds = segment.durationSeconds;
                for (const FHandposeAssignment& assignment : assignments)
                {
                    if (assignment.sourceSegmentIndex == segmentIndex ||
                        assignment.coveredSegmentIndices.Contains(segmentIndex))
                    {
                        snapshot.assignmentIds.Add(assignment.assignmentId.ToString(
                            EGuidFormats::DigitsWithHyphensLower));
                    }
                }
                snapshot.hasAssignment = !snapshot.assignmentIds.IsEmpty();

                FString candidateError;
                const TArray<FHandshapeCandidate> candidates = getCandidatesForSegment(
                    handSide,
                    segment,
                    candidateError);
                const int32 candidateCount = FMath::Min(10, candidates.Num());
                for (int32 candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex)
                {
                    const FHandshapeCandidate& candidate = candidates[candidateIndex];
                    FHandposeDecisionCandidateSnapshot& candidateSnapshot =
                        snapshot.topCandidates.AddDefaulted_GetRef();
                    candidateSnapshot.handposeId = candidate.handshapeId.ToString();
                    candidateSnapshot.displayName = candidate.displayName.ToString();
                    candidateSnapshot.rank = candidateIndex + 1;
                    candidateSnapshot.matchingDistance = candidate.matchingDistance;
                    candidateSnapshot.relevanceScore = candidate.relevanceScore;
                }
            }
        };

    // Capture plain values so training data remains valid after Unreal assets move.
    appendSegments(EHandposeSide::Right, rightSegments, rightAssignments);
    appendSegments(EHandposeSide::Left, leftSegments, leftAssignments);
    return FHandposeDecisionDataService::capture(sessionData);
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
    const UAnimSequenceBase& animation,
    const EHandposeSide handSide) const
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
        const bool isRequestedSide = handSide == EHandposeSide::Left
            ? lowercaseBoneName.Contains(TEXT("left")) ||
                lowercaseBoneName.Contains(TEXT("_l_")) ||
                lowercaseBoneName.StartsWith(TEXT("l_"))
            : lowercaseBoneName.Contains(TEXT("right")) ||
                lowercaseBoneName.Contains(TEXT("_r_")) ||
                lowercaseBoneName.StartsWith(TEXT("r_"));
        if (isRequestedSide && fingerNameParts.ContainsByPredicate(
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
