#include "Widgets/SHandposeFingerSelectionWidget.h"

#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHandposeFingerSelectionWidget"

void SHandposeFingerSelectionWidget::Construct(const FArguments& arguments)
{
    skippedFingers = arguments._InitialSkippedFingers;
    onSkippedFingersChosen = arguments._OnSkippedFingersChosen;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
        .Padding(14.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(LOCTEXT(
                    "SkipFingerPrompt",
                    "Choose the fingers that should keep the original animation."))
                .AutoWrapText(true)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 12.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    createFingerOption(EHandposeFinger::Thumb, LOCTEXT("Thumb", "Skip Thumb"))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    createFingerOption(EHandposeFinger::Index, LOCTEXT("Index", "Skip Index"))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    createFingerOption(EHandposeFinger::Middle, LOCTEXT("Middle", "Skip Middle"))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    createFingerOption(EHandposeFinger::Ring, LOCTEXT("Ring", "Skip Ring"))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    createFingerOption(EHandposeFinger::Pinky, LOCTEXT("Pinky", "Skip Pinky"))
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Right)
            [
                SNew(SUniformGridPanel)
                .SlotPadding(FMargin(4.0f, 0.0f))
                + SUniformGridPanel::Slot(0, 0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Cancel", "Cancel"))
                    .OnClicked(this, &SHandposeFingerSelectionWidget::onCancelClicked)
                ]
                + SUniformGridPanel::Slot(1, 0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Apply", "Apply Handshape"))
                    .OnClicked(this, &SHandposeFingerSelectionWidget::onApplyClicked)
                ]
            ]
        ]
    ];
}

void SHandposeFingerSelectionWidget::setOwnerWindow(
    const TSharedRef<SWindow>& window)
{
    ownerWindow = window;
}

TSharedRef<SWidget> SHandposeFingerSelectionWidget::createFingerOption(
    const EHandposeFinger finger,
    const FText& label)
{
    return SNew(SCheckBox)
        .IsChecked(this, &SHandposeFingerSelectionWidget::getFingerState, finger)
        .OnCheckStateChanged(
            this,
            &SHandposeFingerSelectionWidget::onFingerStateChanged,
            finger)
        [
            SNew(STextBlock)
            .Text(label)
        ];
}

ECheckBoxState SHandposeFingerSelectionWidget::getFingerState(
    const EHandposeFinger finger) const
{
    return skippedFingers.Contains(finger)
        ? ECheckBoxState::Checked
        : ECheckBoxState::Unchecked;
}

void SHandposeFingerSelectionWidget::onFingerStateChanged(
    const ECheckBoxState state,
    const EHandposeFinger finger)
{
    if (state == ECheckBoxState::Checked)
    {
        skippedFingers.AddUnique(finger);
    }
    else
    {
        skippedFingers.Remove(finger);
    }
}

FReply SHandposeFingerSelectionWidget::onApplyClicked()
{
    onSkippedFingersChosen.ExecuteIfBound(skippedFingers);
    if (const TSharedPtr<SWindow> window = ownerWindow.Pin())
    {
        window->RequestDestroyWindow();
    }
    return FReply::Handled();
}

FReply SHandposeFingerSelectionWidget::onCancelClicked()
{
    if (const TSharedPtr<SWindow> window = ownerWindow.Pin())
    {
        window->RequestDestroyWindow();
    }
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
