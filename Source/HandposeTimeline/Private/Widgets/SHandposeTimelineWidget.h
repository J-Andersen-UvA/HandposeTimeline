#pragma once

#include "AnimationPlacementService.h"
#include "FaceTouchCorrectionTypes.h"
#include "HandposeAssignment.h"
#include "HandshapeCandidate.h"
#include "HandposeTimelineSegment.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"
#include "HandposeTimelineConfig.h" 

class FHandposeTimelinePlaybackController;
class AActor;
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

    virtual ~SHandposeTimelineWidget() override;
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
    bool hasPendingChanges() const;
    bool hasAssignments() const;
    bool hasCustomPlaybackRange() const;
    void getPlaybackRange(int32& startFrame, int32& endFrame) const;
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
    FReply onFaceTouchCorrectionModeClicked();
    FReply onRotate180Clicked();
    FReply onAnimationPlacementClicked();
    FReply onLoopAllClicked();
    FReply onSpeedClicked();
    FReply onPlayPauseClicked();
    FReply onShortcutsHelpClicked();
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
    void onFaceTouchCorrectionSelected(
        EHandposeSide handSide,
        int32 correctionIndex);
    void onFaceTouchCorrectionChanged(
        EHandposeSide handSide,
        int32 correctionIndex,
        const FFaceTouchCorrectionBlock& correction,
        bool isFinal);
    void onFaceTouchCorrectionEditRequested(
        EHandposeSide handSide,
        int32 correctionIndex);
    FReply adjustFaceTouchNormalOffset(
        EHandposeSide handSide,
        int32 correctionIndex,
        float offsetDelta);
    void onFaceSurfaceClicked(const FFaceTouchSurfaceLocation& surfaceLocation);
    void onFaceTouchPointsChosen(const TArray<FName>& pointIds);
    TArray<FName> getFaceTouchPointIds() const;
    void removeSelectedAssignment();
    void removeSelectedFaceTouchCorrection();
    void onPlayheadScrubbed(float timeSeconds);
    void stepFrame(int32 frameOffset);
    void setTrimStartAtPlayhead();
    void setTrimEndAtPlayhead();
    void applyPlaybackRange();
    void onZoomChanged(float sliderValue);
    void onZoomWheel(float wheelDelta);
    void loadConfig(UHandposeTimelineConfig* config);
    void restoreLastConfig();
    void saveLastConfig(const UHandposeTimelineConfig& config) const;
    void restoreLastActor();
    void saveLastActor(const AActor& actor) const;
    void restoreAnimationPlacementSettings();
    void saveAnimationPlacementSettings() const;
    void applyAnimationPlacementSettings(
        const struct FAnimationPlacementEditorSettings& settings);
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
    TArray<FFaceTouchCorrectionBlock>& getFaceTouchCorrections(EHandposeSide handSide);
    const TArray<FFaceTouchCorrectionBlock>& getFaceTouchCorrections(
        EHandposeSide handSide) const;
    int32& getSelectedSegmentIndex(EHandposeSide handSide);
    int32& getSelectedAssignmentIndex(EHandposeSide handSide);
    int32& getSelectedFaceTouchCorrectionIndex(EHandposeSide handSide);
    TArray<FHandposeAssignment> getAllAssignments() const;
    void refreshTimeline();
    float getCurrentTimeSeconds() const;
    FText getPlayPauseButtonText() const;
    FText getTimecodeText() const;
    FText getTrimRangeText() const;
    int32 getCurrentFrame() const;
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
    TArray<FFaceTouchCorrectionBlock> rightFaceTouchCorrections;
    TArray<FFaceTouchCorrectionBlock> leftFaceTouchCorrections;
    int32 selectedRightSegmentIndex = INDEX_NONE;
    int32 selectedLeftSegmentIndex = INDEX_NONE;
    int32 selectedRightAssignmentIndex = INDEX_NONE;
    int32 selectedLeftAssignmentIndex = INDEX_NONE;
    int32 selectedRightFaceTouchCorrectionIndex = INDEX_NONE;
    int32 selectedLeftFaceTouchCorrectionIndex = INDEX_NONE;
    EHandposeSide selectedHandSide = EHandposeSide::Right;
    float timelineZoom = 1.0f;
    int32 trimStartFrame = 0;
    int32 trimEndFrame = 0;
    bool faceTouchCorrectionModeEnabled = false;
    bool rotate180Enabled = false;
    bool animationPlacementEnabled = false;
    TWeakObjectPtr<AActor> groundPlaneActor;
    FAnimationPlacementLegChain leftLegPlacement;
    FAnimationPlacementLegChain rightLegPlacement;
    TSharedPtr<SScrollBox> horizontalTimelineScrollBox;
    TSharedPtr<SHandposeTimelineTrack> rightTimelineTrack;
    TSharedPtr<SHandposeTimelineTrack> leftTimelineTrack;
    TWeakPtr<SWindow> handshapePickerWindow;
    TWeakPtr<SWindow> faceTouchPointPickerWindow;
    TWeakPtr<SWindow> animationPlacementWindow;
    FFaceTouchSurfaceLocation pendingFaceTouchSurfaceLocation;
    EHandposeSide pendingFaceTouchHandSide = EHandposeSide::Right;
    int32 pendingFaceTouchFrame = INDEX_NONE;
    TSharedPtr<STextBlock> configNameText;
    TSharedPtr<STextBlock> animationNameText;
    TSharedPtr<STextBlock> actorNameText;
    TSharedPtr<STextBlock> statusText;
    TSharedPtr<STextBlock> speedText;
    TSharedPtr<STextBlock> zoomText;
};
