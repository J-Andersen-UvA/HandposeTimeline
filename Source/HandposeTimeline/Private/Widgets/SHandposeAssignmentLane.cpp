#include "Widgets/SHandposeAssignmentLane.h"

#include "Animation/AnimSequenceBase.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

namespace
{
constexpr float laneHeight = 70.0f;
constexpr float clipHeight = 58.0f;
constexpr float handleRadius = 7.0f;
constexpr float edgeHitWidth = 7.0f;
constexpr int32 curveSampleCount = 32;
}

void SHandposeAssignmentLane::Construct(const FArguments& arguments)
{
    onAssignmentSelected = arguments._OnAssignmentSelected;
    onAssignmentChanged = arguments._OnAssignmentChanged;
    SetCanTick(false);
}

void SHandposeAssignmentLane::setTimeline(
    UAnimSequenceBase* animation,
    const TArray<FHandposeTimelineSegment>& segments,
    const TArray<FHandposeAssignment>& newAssignments,
    const int32 newSelectedAssignmentIndex,
    const float zoom)
{
    layout.setTimeline(animation, segments, zoom);
    assignments = newAssignments;
    selectedAssignmentIndex = newSelectedAssignmentIndex;
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

FVector2D SHandposeAssignmentLane::ComputeDesiredSize(float) const
{
    return FVector2D(FMath::Max(1.0f, layout.getTotalWidth()), laneHeight);
}

int32 SHandposeAssignmentLane::OnPaint(
    const FPaintArgs& arguments,
    const FGeometry& allottedGeometry,
    const FSlateRect& clippingRect,
    FSlateWindowElementList& outDrawElements,
    const int32 layerId,
    const FWidgetStyle& widgetStyle,
    const bool parentEnabled) const
{
    const FSlateBrush* brush = FAppStyle::GetBrush(TEXT("WhiteBrush"));
    const ESlateDrawEffect drawEffects = parentEnabled
        ? ESlateDrawEffect::None
        : ESlateDrawEffect::DisabledEffect;

    float blockOffset = 0.0f;
    for (const FHandposeTimelineBlock& block : layout.getBlocks())
    {
        const float blockWidth = layout.getBlockWidth(block);
        FSlateDrawElement::MakeBox(
            outDrawElements,
            layerId,
            allottedGeometry.ToPaintGeometry(
                FVector2D(blockWidth, clipHeight),
                FSlateLayoutTransform(FVector2D(blockOffset, 0.0f))),
            brush,
            drawEffects,
            block.isGap()
                ? FLinearColor(0.24f, 0.24f, 0.24f, 1.0f)
                : FLinearColor(0.07f, 0.07f, 0.07f, 1.0f));
        const bool isCovered = assignments.ContainsByPredicate(
            [&block](const FHandposeAssignment& assignment)
            {
                return assignment.blendStartFrame <= block.endFrame &&
                    assignment.blendEndFrame >= block.startFrame;
            });
        if (!block.isGap() && !isCovered && blockWidth >= 62.0f)
        {
            FSlateDrawElement::MakeText(
                outDrawElements,
                layerId + 1,
                allottedGeometry.ToPaintGeometry(
                    FVector2D(blockWidth - 8.0f, 18.0f),
                    FSlateLayoutTransform(FVector2D(blockOffset + 4.0f, 21.0f))),
                NSLOCTEXT("HandposeAssignmentLane", "Unassigned", "Unassigned"),
                FAppStyle::GetFontStyle(TEXT("SmallFont")),
                drawEffects,
                FLinearColor(0.58f, 0.58f, 0.58f, 1.0f));
        }
        blockOffset += blockWidth;
    }

    for (int32 assignmentIndex = 0; assignmentIndex < assignments.Num(); ++assignmentIndex)
    {
        const FHandposeAssignment& assignment = assignments[assignmentIndex];
        if (!assignment.isValid())
        {
            continue;
        }
        const float left = getFrameOffset(assignment.blendStartFrame);
        const float right = getFrameOffset(assignment.blendEndFrame, true);
        const float width = FMath::Max(2.0f, right - left);
        FLinearColor color = getAssignmentColor(assignmentIndex);
        color.A = assignmentIndex == selectedAssignmentIndex ? 0.96f : 0.78f;
        FSlateDrawElement::MakeBox(
            outDrawElements,
            layerId + 1,
            allottedGeometry.ToPaintGeometry(
                FVector2D(width, clipHeight),
                FSlateLayoutTransform(FVector2D(left, 0.0f))),
            brush,
            drawEffects,
            color);

        if (assignmentIndex == selectedAssignmentIndex)
        {
            const FLinearColor selectionColor(0.0f, 0.72f, 0.9f, 1.0f);
            FSlateDrawElement::MakeBox(
                outDrawElements,
                layerId + 2,
                allottedGeometry.ToPaintGeometry(
                    FVector2D(width, 2.0f),
                    FSlateLayoutTransform(FVector2D(left, 0.0f))),
                brush,
                drawEffects,
                selectionColor);
            FSlateDrawElement::MakeBox(
                outDrawElements,
                layerId + 2,
                allottedGeometry.ToPaintGeometry(
                    FVector2D(width, 2.0f),
                    FSlateLayoutTransform(FVector2D(left, clipHeight - 2.0f))),
                brush,
                drawEffects,
                selectionColor);
        }

        TArray<FVector2D> curvePoints;
        curvePoints.Reserve(curveSampleCount + 1);
        const double frameRange = FMath::Max(
            1.0,
            static_cast<double>(assignment.blendEndFrame - assignment.blendStartFrame));
        for (int32 sampleIndex = 0; sampleIndex <= curveSampleCount; ++sampleIndex)
        {
            const double alpha = static_cast<double>(sampleIndex) / curveSampleCount;
            const double frame = assignment.blendStartFrame + frameRange * alpha;
            curvePoints.Add(FVector2D(
                FMath::Lerp(left, right, static_cast<float>(alpha)),
                FMath::Lerp(33.0f, 5.0f, assignment.evaluateWeight(frame))));
        }
        FSlateDrawElement::MakeLines(
            outDrawElements,
            layerId + 3,
            allottedGeometry.ToPaintGeometry(),
            curvePoints,
            drawEffects,
            FLinearColor::White,
            true,
            1.5f);

        const FText label = FText::Format(
            NSLOCTEXT("HandposeAssignmentLane", "AssignmentLabel", "{0}  {1}-{2}"),
            FText::FromName(assignment.handshapeId),
            FText::AsNumber(assignment.blendStartFrame),
            FText::AsNumber(assignment.blendEndFrame));
        FSlateDrawElement::MakeText(
            outDrawElements,
            layerId + 4,
            allottedGeometry.ToPaintGeometry(
                FVector2D(FMath::Max(1.0f, width - 12.0f), 18.0f),
                FSlateLayoutTransform(FVector2D(left + 6.0f, 38.0f))),
            label,
            FAppStyle::GetFontStyle(TEXT("SmallFont")),
            drawEffects,
            FLinearColor::White);

        const float fullStart = getFrameOffset(assignment.fullPoseStartFrame);
        const float fullEnd = getFrameOffset(assignment.fullPoseEndFrame, true);
        for (const float handleX : {fullStart, fullEnd})
        {
            FSlateDrawElement::MakeBox(
                outDrawElements,
                layerId + 5,
                allottedGeometry.ToPaintGeometry(
                    FVector2D(8.0f, 8.0f),
                    FSlateLayoutTransform(FVector2D(handleX - 4.0f, 0.0f))),
                brush,
                drawEffects,
                FLinearColor(0.86f, 0.86f, 0.9f, 1.0f));
        }
        for (const float edgeX : {left, right})
        {
            FSlateDrawElement::MakeBox(
                outDrawElements,
                layerId + 5,
                allottedGeometry.ToPaintGeometry(
                    FVector2D(2.0f, clipHeight),
                    FSlateLayoutTransform(FVector2D(edgeX - 1.0f, 0.0f))),
                brush,
                drawEffects,
                FLinearColor(0.8f, 0.8f, 0.84f, 1.0f));
        }
    }
    return layerId + 6;
}

FReply SHandposeAssignmentLane::OnMouseButtonDown(
    const FGeometry& myGeometry,
    const FPointerEvent& mouseEvent)
{
    if (mouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return FReply::Unhandled();
    }
    const FVector2D localPosition = myGeometry.AbsoluteToLocal(
        mouseEvent.GetScreenSpacePosition());
    const FHitResult hit = hitTest(localPosition.X, localPosition.Y);
    if (!assignments.IsValidIndex(hit.assignmentIndex))
    {
        return FReply::Unhandled();
    }

    selectedAssignmentIndex = hit.assignmentIndex;
    draggedAssignmentIndex = hit.assignmentIndex;
    dragTarget = hit.target;
    assignmentAtDragStart = assignments[hit.assignmentIndex];
    cursorFrameAtDragStart = FMath::RoundToInt(
        layout.getFrameAtOffset(localPosition.X));
    onAssignmentSelected.ExecuteIfBound(hit.assignmentIndex);
    Invalidate(EInvalidateWidgetReason::Paint);
    return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SHandposeAssignmentLane::OnMouseMove(
    const FGeometry& myGeometry,
    const FPointerEvent& mouseEvent)
{
    if (!HasMouseCapture() || !assignments.IsValidIndex(draggedAssignmentIndex))
    {
        return FReply::Unhandled();
    }
    updateDrag(
        myGeometry.AbsoluteToLocal(mouseEvent.GetScreenSpacePosition()).X,
        false);
    return FReply::Handled();
}

FReply SHandposeAssignmentLane::OnMouseButtonUp(
    const FGeometry& myGeometry,
    const FPointerEvent& mouseEvent)
{
    if (mouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !HasMouseCapture())
    {
        return FReply::Unhandled();
    }
    updateDrag(
        myGeometry.AbsoluteToLocal(mouseEvent.GetScreenSpacePosition()).X,
        true);
    draggedAssignmentIndex = INDEX_NONE;
    dragTarget = EDragTarget::None;
    return FReply::Handled().ReleaseMouseCapture();
}

SHandposeAssignmentLane::FHitResult SHandposeAssignmentLane::hitTest(
    const float localX,
    const float localY) const
{
    if (localY < 0.0f || localY > clipHeight)
    {
        return {};
    }
    FHitResult closestEdge;
    float closestEdgeDistance = edgeHitWidth + 1.0f;
    for (int32 assignmentIndex = 0; assignmentIndex < assignments.Num(); ++assignmentIndex)
    {
        const FHandposeAssignment& assignment = assignments[assignmentIndex];
        const float left = getFrameOffset(assignment.blendStartFrame);
        const float right = getFrameOffset(assignment.blendEndFrame, true);
        for (const TPair<float, EDragTarget>& edge : {
                 TPair<float, EDragTarget>(left, EDragTarget::BlendStart),
                 TPair<float, EDragTarget>(right, EDragTarget::BlendEnd)})
        {
            const float distance = FMath::Abs(localX - edge.Key);
            const bool winsTie = FMath::IsNearlyEqual(distance, closestEdgeDistance) &&
                assignmentIndex == selectedAssignmentIndex;
            if (distance <= edgeHitWidth &&
                (distance < closestEdgeDistance || winsTie))
            {
                closestEdge = {assignmentIndex, edge.Value};
                closestEdgeDistance = distance;
            }
        }
    }
    if (closestEdge.assignmentIndex != INDEX_NONE)
    {
        return closestEdge;
    }

    if (localY <= 18.0f)
    {
        FHitResult closestHandle;
        float closestHandleDistance = handleRadius + 1.0f;
        for (int32 assignmentIndex = 0; assignmentIndex < assignments.Num(); ++assignmentIndex)
        {
            const FHandposeAssignment& assignment = assignments[assignmentIndex];
            for (const TPair<float, EDragTarget>& handle : {
                     TPair<float, EDragTarget>(
                         getFrameOffset(assignment.fullPoseStartFrame),
                         EDragTarget::FullPoseStart),
                     TPair<float, EDragTarget>(
                         getFrameOffset(assignment.fullPoseEndFrame, true),
                         EDragTarget::FullPoseEnd)})
            {
                const float distance = FMath::Abs(localX - handle.Key);
                const bool winsTie = FMath::IsNearlyEqual(
                    distance,
                    closestHandleDistance) &&
                    assignmentIndex == selectedAssignmentIndex;
                if (distance <= handleRadius &&
                    (distance < closestHandleDistance || winsTie))
                {
                    closestHandle = {assignmentIndex, handle.Value};
                    closestHandleDistance = distance;
                }
            }
        }
        if (closestHandle.assignmentIndex != INDEX_NONE)
        {
            return closestHandle;
        }
    }

    for (int32 assignmentIndex = assignments.Num() - 1; assignmentIndex >= 0; --assignmentIndex)
    {
        const FHandposeAssignment& assignment = assignments[assignmentIndex];
        const float left = getFrameOffset(assignment.blendStartFrame);
        const float right = getFrameOffset(assignment.blendEndFrame, true);
        if (localX >= left && localX <= right)
        {
            return {assignmentIndex, EDragTarget::Move};
        }
    }
    return {};
}

void SHandposeAssignmentLane::updateDrag(const float localX, const bool isFinal)
{
    if (!assignments.IsValidIndex(draggedAssignmentIndex))
    {
        return;
    }
    const int32 cursorFrame = FMath::Clamp(
        FMath::RoundToInt(layout.getFrameAtOffset(localX)),
        0,
        layout.getLastFrame());
    FHandposeAssignment updated = assignmentAtDragStart;
    switch (dragTarget)
    {
    case EDragTarget::Move:
        updated.shiftBy(
            cursorFrame - cursorFrameAtDragStart,
            layout.getLastFrame());
        break;
    case EDragTarget::BlendStart:
        updated.blendStartFrame = FMath::Clamp(
            cursorFrame,
            0,
            updated.fullPoseStartFrame);
        break;
    case EDragTarget::FullPoseStart:
        updated.fullPoseStartFrame = FMath::Clamp(
            cursorFrame,
            updated.blendStartFrame,
            updated.fullPoseEndFrame);
        break;
    case EDragTarget::FullPoseEnd:
        updated.fullPoseEndFrame = FMath::Clamp(
            cursorFrame - 1,
            updated.fullPoseStartFrame,
            updated.blendEndFrame);
        break;
    case EDragTarget::BlendEnd:
        updated.blendEndFrame = FMath::Clamp(
            cursorFrame - 1,
            updated.fullPoseEndFrame,
            layout.getLastFrame());
        break;
    default:
        return;
    }
    assignments[draggedAssignmentIndex] = updated;
    onAssignmentChanged.ExecuteIfBound(
        draggedAssignmentIndex,
        updated,
        isFinal);
    Invalidate(EInvalidateWidgetReason::Paint);
}

float SHandposeAssignmentLane::getFrameOffset(
    const int32 frame,
    const bool useExclusiveEnd) const
{
    return layout.getOffsetAtFrame(
        useExclusiveEnd ? static_cast<double>(frame) + 1.0 : frame);
}

FLinearColor SHandposeAssignmentLane::getAssignmentColor(
    const int32 assignmentIndex) const
{
    static const TArray<FLinearColor> colors = {
        FLinearColor(0.29f, 0.25f, 0.39f, 1.0f),
        FLinearColor(0.19f, 0.31f, 0.38f, 1.0f)
    };
    return colors[assignmentIndex % colors.Num()];
}
