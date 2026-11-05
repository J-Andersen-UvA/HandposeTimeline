#pragma once

#include "HandposeAssignment.h"
#include "HandshapeCandidate.h"
#include "HandposeTimelineSegment.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class FHandposeTimelinePlaybackController;
class SScrollBox;
class SHandposeTimelineTrack;
class STextBlock;
class SWindow;
class UAnimSequence;
class UAnimSequenceBase;
class UHandposeTimelineConfig;
struct FHandposeDecisionDataResult;

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
    virtual FReply OnPreviewMouseButtonDown(
        const FGeometry& myGeometry,
        const FPointerEvent& mouseEvent) override;
    virtual FReply OnMouseWheel(
        const FGeometry& myGeometry,
        const FPointerEvent& mouseEvent) override;
    virtual bool SupportsKeyboardFocus() const override { return true; }
    void loadAnimation(UAnimSequenceBase* animation);
    void resetSession();
    bool canBakeCurrentAnimation() const;
    bool hasAssignments() const;
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
    FReply onSaveDecisionDataClicked();
    FReply onImportDecisionDataClicked();
    void onSegmentSelected(EHandposeSide handSide, int32 segmentIndex);
    void onHandshapeChosen(
        int32 segmentIndex,
        const FHandshapeCandidate& candidate);
    void onAssignmentSelected(EHandposeSide handSide, int32 assignmentIndex);
    void onAssignmentChanged(
        EHandposeSide handSide,
        int32 assignmentIndex,
        const FHandposeAssignment& assignment,
        bool isFinal);
    void onAssignmentFingerEditRequested(
        EHandposeSide handSide,
        int32 assignmentIndex);
    void onAssignmentSkippedFingersChosen(
        const TArray<EHandposeFinger>& skippedFingers,
        EHandposeSide handSide,
        int32 assignmentIndex);
    void removeSelectedAssignment();
    void onPlayheadScrubbed(float timeSeconds);
    void stepFrame(int32 frameOffset);
    void onZoomChanged(float sliderValue);
    void onZoomWheel(float wheelDelta);
    void loadConfig(UHandposeTimelineConfig* config);
    void restoreLastConfig();
    void saveLastConfig(const UHandposeTimelineConfig& config) const;
    void selectSegment(EHandposeSide handSide, int32 segmentIndex, bool shouldPreview);
    void openHandshapePicker(EHandposeSide handSide, int32 segmentIndex);
    void rebuildPreviewAnimation(bool preserveTime);
    void linkOverlappingAssignments(EHandposeSide handSide);
    void updateAssignmentCoverage(EHandposeSide handSide);
    FHandposeDecisionDataResult captureDecisionData() const;
    TArray<FHandshapeCandidate> getCandidatesForSegment(
        EHandposeSide handSide,
        const FHandposeTimelineSegment& segment,
        FString& errorMessage) const;
    bool generateSegments(
        UAnimSequenceBase& animation,
        EHandposeSide handSide,
        TArray<FHandposeTimelineSegment>& outputSegments,
        FString& errorMessage) const;
    TArray<FHandposeTimelineSegment>& getSegments(EHandposeSide handSide);
    const TArray<FHandposeTimelineSegment>& getSegments(EHandposeSide handSide) const;
    TArray<FHandposeAssignment>& getAssignments(EHandposeSide handSide);
    const TArray<FHandposeAssignment>& getAssignments(EHandposeSide handSide) const;
    int32& getSelectedSegmentIndex(EHandposeSide handSide);
    int32& getSelectedAssignmentIndex(EHandposeSide handSide);
    TArray<FHandposeAssignment> getAllAssignments() const;
    void refreshTimeline();
    float getCurrentTimeSeconds() const;
    FText getPlayPauseButtonText() const;
    FText getTimecodeText() const;
    TArray<FName> getSegmentationBoneNames(
        const UAnimSequenceBase& animation,
        EHandposeSide handSide) const;
    TSharedPtr<FHandposeTimelinePlaybackController> playbackController;
    TWeakObjectPtr<UAnimSequenceBase> loadedAnimation;
    TStrongObjectPtr<UAnimSequence> previewAnimation;
    TStrongObjectPtr<UHandposeTimelineConfig> loadedConfig;
    TArray<FHandposeTimelineSegment> rightSegments;
    TArray<FHandposeTimelineSegment> leftSegments;
    TArray<FHandposeAssignment> rightAssignments;
    TArray<FHandposeAssignment> leftAssignments;
    int32 selectedRightSegmentIndex = INDEX_NONE;
    int32 selectedLeftSegmentIndex = INDEX_NONE;
    int32 selectedRightAssignmentIndex = INDEX_NONE;
    int32 selectedLeftAssignmentIndex = INDEX_NONE;
    EHandposeSide selectedHandSide = EHandposeSide::Right;
    float timelineZoom = 1.0f;
    TSharedPtr<SScrollBox> horizontalTimelineScrollBox;
    TSharedPtr<SHandposeTimelineTrack> rightTimelineTrack;
    TSharedPtr<SHandposeTimelineTrack> leftTimelineTrack;
    TWeakPtr<SWindow> handshapePickerWindow;
    TSharedPtr<STextBlock> configNameText;
    TSharedPtr<STextBlock> animationNameText;
    TSharedPtr<STextBlock> actorNameText;
    TSharedPtr<STextBlock> statusText;
    TSharedPtr<STextBlock> speedText;
    TSharedPtr<STextBlock> zoomText;
};
