#pragma once

#include "HandposeAssignment.h"
#include "Timeline/HandposeTimelineLayout.h"
#include "Widgets/SLeafWidget.h"

class UAnimSequenceBase;

DECLARE_DELEGATE_OneParam(FOnHandposeAssignmentSelected, int32)
DECLARE_DELEGATE_ThreeParams(
    FOnHandposeAssignmentChanged,
    int32,
    const FHandposeAssignment&,
    bool)

class SHandposeAssignmentLane : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SHandposeAssignmentLane)
    {
    }
        SLATE_EVENT(FOnHandposeAssignmentSelected, OnAssignmentSelected)
        SLATE_EVENT(FOnHandposeAssignmentChanged, OnAssignmentChanged)
    SLATE_END_ARGS()

    void Construct(const FArguments& arguments);
    void setTimeline(
        UAnimSequenceBase* animation,
        const TArray<FHandposeTimelineSegment>& segments,
        const TArray<FHandposeAssignment>& newAssignments,
        int32 newSelectedAssignmentIndex,
        float zoom);

    virtual FVector2D ComputeDesiredSize(float layoutScaleMultiplier) const override;
    virtual int32 OnPaint(
        const FPaintArgs& arguments,
        const FGeometry& allottedGeometry,
        const FSlateRect& clippingRect,
        FSlateWindowElementList& outDrawElements,
        int32 layerId,
        const FWidgetStyle& widgetStyle,
        bool parentEnabled) const override;
    virtual FReply OnMouseButtonDown(
        const FGeometry& myGeometry,
        const FPointerEvent& mouseEvent) override;
    virtual FReply OnMouseMove(
        const FGeometry& myGeometry,
        const FPointerEvent& mouseEvent) override;
    virtual FReply OnMouseButtonUp(
        const FGeometry& myGeometry,
        const FPointerEvent& mouseEvent) override;

private:
    enum class EDragTarget
    {
        None,
        Move,
        BlendStart,
        FullPoseStart,
        FullPoseEnd,
        BlendEnd
    };

    struct FHitResult
    {
        int32 assignmentIndex = INDEX_NONE;
        EDragTarget target = EDragTarget::None;
    };

    FHitResult hitTest(float localX, float localY) const;
    void updateDrag(float localX, bool isFinal);
    float getFrameOffset(int32 frame, bool useExclusiveEnd = false) const;
    FLinearColor getAssignmentColor(int32 assignmentIndex) const;

    FHandposeTimelineLayout layout;
    TArray<FHandposeAssignment> assignments;
    int32 selectedAssignmentIndex = INDEX_NONE;
    int32 draggedAssignmentIndex = INDEX_NONE;
    EDragTarget dragTarget = EDragTarget::None;
    FHandposeAssignment assignmentAtDragStart;
    int32 cursorFrameAtDragStart = 0;
    FOnHandposeAssignmentSelected onAssignmentSelected;
    FOnHandposeAssignmentChanged onAssignmentChanged;
};
