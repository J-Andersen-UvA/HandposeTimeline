#include "Widgets/SAnimationPlacementSettingsWidget.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SAnimationPlacementSettingsWidget"

namespace
{
TSharedRef<SWidget> makeLabel(const FText& text)
{
    return SNew(STextBlock)
        .Text(text)
        .MinDesiredWidth(110.0f);
}
}

void SAnimationPlacementSettingsWidget::Construct(const FArguments& arguments)
{
    settings = arguments._InitialSettings;
    ownerWindow = arguments._OwnerWindow;
    onSettingsAccepted = arguments._OnSettingsAccepted;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
        .Padding(12.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SCheckBox)
                .IsChecked(settings.enabled
                    ? ECheckBoxState::Checked
                    : ECheckBoxState::Unchecked)
                .OnCheckStateChanged_Lambda(
                    [this](const ECheckBoxState state)
                    {
                        settings.enabled = state == ECheckBoxState::Checked;
                    })
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("EnableGroundPlacement", "Enable ground placement"))
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 10.0f, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    makeLabel(LOCTEXT("GroundPlane", "Ground Plane"))
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SAssignNew(groundPlaneText, STextBlock)
                    .Text(settings.groundPlaneActor.IsValid()
                        ? FText::FromString(settings.groundPlaneActor->GetActorLabel())
                        : LOCTEXT("NoGroundPlane", "No actor selected"))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("UseSelectedGroundPlane", "Use Selected Actor"))
                    .OnClicked(this, &SAnimationPlacementSettingsWidget::onUseSelectedGroundPlaneClicked)
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 12.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("LegChainHelp", "Chains must be ordered from upper leg to foot."))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 3.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [makeLabel(LOCTEXT("LeftChain", "Left IK Chain"))]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SAssignNew(leftChainText, SEditableTextBox)
                    .Text(formatBoneNames(settings.leftLeg.bones))
                    .HintText(LOCTEXT("LeftChainHint", "thigh_l, calf_l, foot_l"))
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 3.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [makeLabel(LOCTEXT("LeftToe", "Left Toe Bone"))]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SAssignNew(leftToeText, SEditableTextBox)
                    .Text(FText::FromName(settings.leftLeg.toeBone))
                    .HintText(LOCTEXT("LeftToeHint", "ball_l"))
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 3.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [makeLabel(LOCTEXT("RightChain", "Right IK Chain"))]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SAssignNew(rightChainText, SEditableTextBox)
                    .Text(formatBoneNames(settings.rightLeg.bones))
                    .HintText(LOCTEXT("RightChainHint", "thigh_r, calf_r, foot_r"))
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 3.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [makeLabel(LOCTEXT("RightToe", "Right Toe Bone"))]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SAssignNew(rightToeText, SEditableTextBox)
                    .Text(FText::FromName(settings.rightLeg.toeBone))
                    .HintText(LOCTEXT("RightToeHint", "ball_r"))
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Right)
            .Padding(0.0f, 14.0f, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Cancel", "Cancel"))
                    .OnClicked(this, &SAnimationPlacementSettingsWidget::onCancelClicked)
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Apply", "Apply"))
                    .OnClicked(this, &SAnimationPlacementSettingsWidget::onApplyClicked)
                ]
            ]
        ]
    ];
}

FReply SAnimationPlacementSettingsWidget::onUseSelectedGroundPlaneClicked()
{
    AActor* actor = GEditor != nullptr
        ? GEditor->GetSelectedActors()->GetTop<AActor>()
        : nullptr;
    settings.groundPlaneActor = actor;
    groundPlaneText->SetText(actor != nullptr
        ? FText::FromString(actor->GetActorLabel())
        : LOCTEXT("NoSelectedGroundPlane", "No actor selected"));
    return FReply::Handled();
}

FReply SAnimationPlacementSettingsWidget::onApplyClicked()
{
    settings.leftLeg.bones = parseBoneNames(leftChainText->GetText());
    settings.rightLeg.bones = parseBoneNames(rightChainText->GetText());
    settings.leftLeg.toeBone = FName(*leftToeText->GetText().ToString().TrimStartAndEnd());
    settings.rightLeg.toeBone = FName(*rightToeText->GetText().ToString().TrimStartAndEnd());
    onSettingsAccepted.ExecuteIfBound(settings);
    if (const TSharedPtr<SWindow> window = ownerWindow.Pin())
    {
        window->RequestDestroyWindow();
    }
    return FReply::Handled();
}

FReply SAnimationPlacementSettingsWidget::onCancelClicked()
{
    if (const TSharedPtr<SWindow> window = ownerWindow.Pin())
    {
        window->RequestDestroyWindow();
    }
    return FReply::Handled();
}

TArray<FName> SAnimationPlacementSettingsWidget::parseBoneNames(const FText& text)
{
    TArray<FString> values;
    text.ToString().ParseIntoArray(values, TEXT(","), true);
    TArray<FName> names;
    for (FString& value : values)
    {
        value.TrimStartAndEndInline();
        if (!value.IsEmpty())
        {
            names.Add(FName(*value));
        }
    }
    return names;
}

FText SAnimationPlacementSettingsWidget::formatBoneNames(
    const TArray<FName>& boneNames)
{
    TArray<FString> values;
    for (const FName boneName : boneNames)
    {
        values.Add(boneName.ToString());
    }
    return FText::FromString(FString::Join(values, TEXT(", ")));
}

#undef LOCTEXT_NAMESPACE
