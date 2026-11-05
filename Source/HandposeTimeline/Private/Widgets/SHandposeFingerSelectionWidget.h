#pragma once

#include "HandposeFinger.h"
#include "Widgets/SCompoundWidget.h"

class SWindow;

DECLARE_DELEGATE_OneParam(
    FOnSkippedFingersChosen,
    const TArray<EHandposeFinger>&)

class SHandposeFingerSelectionWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHandposeFingerSelectionWidget)
    {
    }
        SLATE_ARGUMENT(TArray<EHandposeFinger>, InitialSkippedFingers)
        SLATE_EVENT(FOnSkippedFingersChosen, OnSkippedFingersChosen)
    SLATE_END_ARGS()

    void Construct(const FArguments& arguments);
    void setOwnerWindow(const TSharedRef<SWindow>& window);

private:
    TSharedRef<SWidget> createFingerOption(
        EHandposeFinger finger,
        const FText& label);
    ECheckBoxState getFingerState(EHandposeFinger finger) const;
    void onFingerStateChanged(ECheckBoxState state, EHandposeFinger finger);
    FReply onApplyClicked();
    FReply onCancelClicked();

    TArray<EHandposeFinger> skippedFingers;
    FOnSkippedFingersChosen onSkippedFingersChosen;
    TWeakPtr<SWindow> ownerWindow;
};
