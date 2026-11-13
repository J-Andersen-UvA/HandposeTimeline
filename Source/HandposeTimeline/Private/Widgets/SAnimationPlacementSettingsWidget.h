#pragma once

#include "AnimationPlacementService.h"
#include "Widgets/SCompoundWidget.h"

class AActor;
class SEditableTextBox;
class STextBlock;
class SWindow;

struct FAnimationPlacementEditorSettings
{
    bool enabled = false;
    TWeakObjectPtr<AActor> groundPlaneActor;
    FAnimationPlacementLegChain leftLeg;
    FAnimationPlacementLegChain rightLeg;
};

DECLARE_DELEGATE_OneParam(
    FOnAnimationPlacementSettingsAccepted,
    const FAnimationPlacementEditorSettings&);

class SAnimationPlacementSettingsWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SAnimationPlacementSettingsWidget)
    {
    }
        SLATE_ARGUMENT(FAnimationPlacementEditorSettings, InitialSettings)
        SLATE_ARGUMENT(TWeakPtr<SWindow>, OwnerWindow)
        SLATE_EVENT(
            FOnAnimationPlacementSettingsAccepted,
            OnSettingsAccepted)
    SLATE_END_ARGS()

    void Construct(const FArguments& arguments);

private:
    FReply onUseSelectedGroundPlaneClicked();
    FReply onApplyClicked();
    FReply onCancelClicked();
    static TArray<FName> parseBoneNames(const FText& text);
    static FText formatBoneNames(const TArray<FName>& boneNames);

    FAnimationPlacementEditorSettings settings;
    TWeakPtr<SWindow> ownerWindow;
    FOnAnimationPlacementSettingsAccepted onSettingsAccepted;
    TSharedPtr<STextBlock> groundPlaneText;
    TSharedPtr<SEditableTextBox> leftChainText;
    TSharedPtr<SEditableTextBox> rightChainText;
    TSharedPtr<SEditableTextBox> leftToeText;
    TSharedPtr<SEditableTextBox> rightToeText;
};
