#pragma once

#include "HandposeAssignment.h"
#include "HandshapeCandidate.h"
#include "HandposeTimelineSegment.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class FHandposeTimelinePlaybackController;
class SHandposeTimelineTrack;
class STextBlock;
class SWindow;
class UAnimSequence;
class UAnimSequenceBase;
class UHandposeTimelineConfig;

class SHandposeTimelineWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHandposeTimelineWidget)
    {
    }
    SLATE_END_ARGS()

    void Construct(const FArguments& arguments);
    virtual void Tick(
        const FGeometry& allottedGeometry,
        double currentTime,
        float deltaTime) override;
    virtual FReply OnPreviewKeyDown(
        const FGeometry& myGeometry,
        const FKeyEvent& keyEvent) override;
    virtual FReply OnMouseWheel(
        const FGeometry& myGeometry,
        const FPointerEvent& mouseEvent) override;
    void loadAnimation(UAnimSequenceBase* animation);
    bool canBakeCurrentAnimation() const;
    bool hasAssignments() const { return !assignments.IsEmpty(); }
    bool bakeCurrentAnimation(
        UAnimSequence*& outputAnimation,
        FString& outputPath,
        FString& errorMessage,
        bool& cancelled);
    UAnimSequenceBase* getLoadedAnimation() const;

private:
    FReply onLoadAnimationClicked();
    FReply onLoadConfigClicked();
    FReply onUseSelectedActorClicked();
    FReply onLoopAllClicked();
    FReply onLoopSegmentClicked();
    FReply onSpeedClicked();
    FReply onPlayPauseClicked();
    FReply onBakeClicked();
    void onSegmentSelected(int32 segmentIndex);
    void onHandshapeChosen(
        int32 segmentIndex,
        const FHandshapeCandidate& candidate);
    void onAssignmentSelected(int32 assignmentIndex);
    void onAssignmentChanged(
        int32 assignmentIndex,
        const FHandposeAssignment& assignment,
        bool isFinal);
    void onPlayheadScrubbed(float timeSeconds);
    void stepFrame(int32 frameOffset);
    void onZoomChanged(float sliderValue);
    void onZoomWheel(float wheelDelta);
    void loadConfig(UHandposeTimelineConfig* config);
    void selectSegment(int32 segmentIndex, bool shouldPreview);
    void openHandshapePicker(int32 segmentIndex);
    void rebuildPreviewAnimation(bool preserveTime);
    void linkOverlappingAssignments();
    TArray<FHandshapeCandidate> getCandidatesForSegment(
        const FHandposeTimelineSegment& segment,
        FString& errorMessage) const;
    void refreshTimeline();
    float getCurrentTimeSeconds() const;
    FText getPlayPauseButtonText() const;
    FText getTimecodeText() const;
    TArray<FName> getSegmentationBoneNames(const UAnimSequenceBase& animation) const;
    TSharedPtr<FHandposeTimelinePlaybackController> playbackController;
    TWeakObjectPtr<UAnimSequenceBase> loadedAnimation;
    TStrongObjectPtr<UAnimSequence> previewAnimation;
    TWeakObjectPtr<UHandposeTimelineConfig> loadedConfig;
    TArray<FHandposeTimelineSegment> segments;
    TArray<FHandposeAssignment> assignments;
    int32 selectedSegmentIndex = INDEX_NONE;
    int32 selectedAssignmentIndex = INDEX_NONE;
    float timelineZoom = 1.0f;
    TSharedPtr<SHandposeTimelineTrack> timelineTrack;
    TWeakPtr<SWindow> handshapePickerWindow;
    TSharedPtr<STextBlock> configNameText;
    TSharedPtr<STextBlock> animationNameText;
    TSharedPtr<STextBlock> actorNameText;
    TSharedPtr<STextBlock> statusText;
    TSharedPtr<STextBlock> speedText;
    TSharedPtr<STextBlock> zoomText;
};
