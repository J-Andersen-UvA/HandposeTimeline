#include "Widgets/SHandposeTimelineWidget.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "AnimationPlacementService.h"
#include "AnimPose.h"
#include "AssetRegistry/AssetData.h"
#include "Baking/HandposeBakeService.h"
#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "FaceTouchCorrectionEditorModule.h"
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
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "SemanticInteractionPointComponent.h"
#include "Playback/HandposeTimelinePlaybackController.h"
#include "Preview/HandposePreviewBuilder.h"
#include "FaceTouch/FaceTouchPreviewBuilder.h"
#include "Handposes/HandposeMirrorService.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SHandshapePickerWidget.h"
#include "Widgets/SAnimationPlacementSettingsWidget.h"
#include "Widgets/SHandposeFingerSelectionWidget.h"
#include "Widgets/SHandposeTimelineTrack.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHandposeTimelineWidget"

DEFINE_LOG_CATEGORY_STATIC(LogHandposeTimelineRecommendations, Log, All);

namespace
{
constexpr float minimumTimelineZoom = 1.0f;
constexpr float maximumTimelineZoom = 2.5f;
const TCHAR* configPersistenceSection = TEXT("HandposeTimeline");
const TCHAR* configPersistenceKey = TEXT("LastConfigAsset");
const TCHAR* rotate180PersistenceKey = TEXT("Rotate180Enabled");
const TCHAR* actorPersistenceKey = TEXT("LastPreviewActor");
const TCHAR* placementEnabledPersistenceKey = TEXT("GroundPlacementEnabled");
const TCHAR* groundPlanePersistenceKey = TEXT("GroundPlaneActor");
const TCHAR* leftLegChainPersistenceKey = TEXT("LeftLegIkChain");
const TCHAR* rightLegChainPersistenceKey = TEXT("RightLegIkChain");
const TCHAR* leftToePersistenceKey = TEXT("LeftToeBone");
const TCHAR* rightToePersistenceKey = TEXT("RightToeBone");

FString formatBoneNames(const TArray<FName>& boneNames)
{
    TArray<FString> values;
    for (const FName boneName : boneNames)
    {
        values.Add(boneName.ToString());
    }
    return FString::Join(values, TEXT(","));
}

TArray<FName> parseBoneNames(const FString& value)
{
    TArray<FString> values;
    value.ParseIntoArray(values, TEXT(","), true);
    TArray<FName> names;
    for (FString& boneName : values)
    {
        boneName.TrimStartAndEndInline();
        if (!boneName.IsEmpty())
        {
            names.Add(FName(*boneName));
        }
    }
    return names;
}

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

FString descriptorToString(const TArray<double>& descriptor)
{
    TArray<FString> values;
    values.Reserve(descriptor.Num());
    for (const double value : descriptor)
    {
        values.Add(FString::Printf(TEXT("%.12f"), value));
    }
    return FString::Join(values, TEXT(","));
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

DECLARE_DELEGATE_OneParam(
    FOnFaceTouchPointsChosen,
    const TArray<FName>&)

class SFaceTouchPointPicker : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SFaceTouchPointPicker)
    {
    }
        SLATE_ARGUMENT(TArray<FName>, PointIds)
        SLATE_EVENT(FOnFaceTouchPointsChosen, OnPointsChosen)
    SLATE_END_ARGS()

    void Construct(const FArguments& arguments)
    {
        pointIds = arguments._PointIds;
        onPointsChosen = arguments._OnPointsChosen;

        TSharedRef<SVerticalBox> pointList = SNew(SVerticalBox);
        for (const FName pointId : pointIds)
        {
            pointList->AddSlot()
            .AutoHeight()
            .Padding(4.0f)
            [
                SNew(SCheckBox)
                .OnCheckStateChanged_Lambda(
                    [this, pointId](const ECheckBoxState state)
                    {
                        if (state == ECheckBoxState::Checked)
                        {
                            selectedPointIds.Add(pointId);
                        }
                        else
                        {
                            selectedPointIds.Remove(pointId);
                        }
                    })
                [
                    SNew(STextBlock)
                    .Text(FText::FromName(pointId))
                ]
            ];
        }

        ChildSlot
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(8.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT(
                    "ChooseFaceTouchPoints",
                    "Which finger or fingers should reach this location?"))
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(8.0f, 0.0f)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()
                [
                    pointList
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Right)
            .Padding(8.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CancelFaceTouch", "Cancel"))
                    .OnClicked_Lambda(
                        [this]()
                        {
                            if (ownerWindow.IsValid())
                            {
                                ownerWindow.Pin()->RequestDestroyWindow();
                            }
                            return FReply::Handled();
                        })
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(8.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CreateFaceTouch", "Create Correction"))
                    .IsEnabled_Lambda([this]() { return !selectedPointIds.IsEmpty(); })
                    .OnClicked_Lambda(
                        [this]()
                        {
                            TArray<FName> selected = selectedPointIds.Array();
                            selected.Sort(FNameLexicalLess());
                            onPointsChosen.ExecuteIfBound(selected);
                            if (ownerWindow.IsValid())
                            {
                                ownerWindow.Pin()->RequestDestroyWindow();
                            }
                            return FReply::Handled();
                        })
                ]
            ]
        ];
    }

    void setOwnerWindow(const TSharedRef<SWindow>& window)
    {
        ownerWindow = window;
    }

private:
    TArray<FName> pointIds;
    TSet<FName> selectedPointIds;
    FOnFaceTouchPointsChosen onPointsChosen;
    TWeakPtr<SWindow> ownerWindow;
};

SHandposeTimelineWidget::~SHandposeTimelineWidget()
{
    if (!IsEngineExitRequested() &&
        FModuleManager::Get().IsModuleLoaded(TEXT("FaceTouchCorrectionEditor")))
    {
        IFaceTouchCorrectionEditorModule::get().endFacePicking();
    }
}

void SHandposeTimelineWidget::Construct(const FArguments& arguments)
{
    playbackController = MakeShared<FHandposeTimelinePlaybackController>();
    GConfig->GetBool(
        configPersistenceSection,
        rotate180PersistenceKey,
        rotate180Enabled,
        GEditorPerProjectIni);
    restoreAnimationPlacementSettings();
    TSharedPtr<SHandposeTimelineVerticalScrollBox> verticalTimelineScrollBox;

    const auto makeToolbarButton = [](
        const FName iconName,
        const TAttribute<FText>& label,
        const FText& tooltip,
        const FOnClicked& onClicked,
        const TAttribute<FSlateColor>& buttonColor,
        const bool enabled = true)
    {
        return SNew(SButton)
            .ToolTipText(tooltip)
            .OnClicked(onClicked)
            .ButtonColorAndOpacity(buttonColor)
            .IsEnabled(enabled)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SImage)
                    .Image(FAppStyle::GetBrush(iconName))
                    .ColorAndOpacity(FSlateColor::UseForeground())
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(5.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(label)
                ]
            ];
    };

    const auto makeGroupLabel = [](const FText& label)
    {
        return SNew(STextBlock)
            .Text(label)
            .Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground());
    };

    const TAttribute<FSlateColor> defaultButtonColor =
        FSlateColor(FLinearColor::White);
    const TAttribute<FSlateColor> faceTouchButtonColor =
        TAttribute<FSlateColor>::CreateLambda(
            [this]()
            {
                return FSlateColor(
                    faceTouchCorrectionModeEnabled
                        ? FLinearColor(0.05f, 0.4f, 0.75f, 1.0f)
                        : FLinearColor::White);
            });
    const TAttribute<FSlateColor> rotate180ButtonColor =
        TAttribute<FSlateColor>::CreateLambda(
            [this]()
            {
                return FSlateColor(
                    rotate180Enabled
                        ? FLinearColor(0.05f, 0.4f, 0.75f, 1.0f)
                        : FLinearColor::White);
            });
    const TAttribute<FSlateColor> animationPlacementButtonColor =
        TAttribute<FSlateColor>::CreateLambda(
            [this]()
            {
                return FSlateColor(
                    animationPlacementEnabled
                        ? FLinearColor(0.05f, 0.4f, 0.75f, 1.0f)
                        : FLinearColor::White);
            });
    const TAttribute<FSlateColor> actorButtonColor =
        TAttribute<FSlateColor>::CreateLambda(
            [this]()
            {
                return FSlateColor(
                    playbackController->getActor() != nullptr
                        ? FLinearColor(0.2f, 0.5f, 0.25f, 1.0f)
                        : FLinearColor(0.55f, 0.18f, 0.18f, 1.0f));
            });
    const TAttribute<FSlateColor> loopAllButtonColor =
        TAttribute<FSlateColor>::CreateLambda(
            [this]()
            {
                return FSlateColor(
                    playbackController->shouldLoopAll()
                        ? FLinearColor(0.05f, 0.4f, 0.75f, 1.0f)
                        : FLinearColor::White);
            });

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.0f, 8.0f, 8.0f, 4.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
                .Padding(6.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 4.0f)
                    [
                        makeGroupLabel(LOCTEXT("SetupGroup", "SETUP"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SUniformGridPanel)
                        .SlotPadding(FMargin(2.0f))
                        + SUniformGridPanel::Slot(0, 0)
                        [
                            makeToolbarButton(
                                TEXT("Icons.Settings"),
                                LOCTEXT("LoadConfigCompact", "Config"),
                                LOCTEXT(
                                    "LoadConfigTooltip",
                                    "Choose the segmentation and handshape source configuration."),
                                FOnClicked::CreateSP(
                                    this,
                                    &SHandposeTimelineWidget::onLoadConfigClicked),
                                defaultButtonColor)
                        ]
                        + SUniformGridPanel::Slot(1, 0)
                        [
                            makeToolbarButton(
                                TEXT("Icons.FolderOpen"),
                                LOCTEXT("LoadAnimationCompact", "Animation"),
                                LOCTEXT(
                                    "LoadAnimationTooltip",
                                    "Choose an animation and generate its handpose segments."),
                                FOnClicked::CreateSP(
                                    this,
                                    &SHandposeTimelineWidget::onLoadAnimationClicked),
                                defaultButtonColor)
                        ]
                        + SUniformGridPanel::Slot(0, 1)
                        [
                            makeToolbarButton(
                                TEXT("Icons.SelectInViewport"),
                                LOCTEXT("UseSelectedActorCompact", "Actor"),
                                LOCTEXT(
                                    "UseSelectedActorTooltip",
                                    "Use the selected scene actor and its skeletal mesh for segment previews."),
                                FOnClicked::CreateSP(
                                    this,
                                    &SHandposeTimelineWidget::onUseSelectedActorClicked),
                                actorButtonColor)
                        ]
                    ]
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4.0f, 0.0f)
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
                .Padding(6.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 4.0f)
                    [
                        makeGroupLabel(LOCTEXT("FunctionsGroup", "FUNCTIONS"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            makeToolbarButton(
                                TEXT("Icons.Edit"),
                                LOCTEXT("FaceTouchCorrectionMode", "Face Touch"),
                                LOCTEXT(
                                    "FaceTouchCorrectionModeTooltip",
                                    "Show face-touch correction lanes and enable face-touch authoring."),
                                FOnClicked::CreateSP(
                                    this,
                                    &SHandposeTimelineWidget::onFaceTouchCorrectionModeClicked),
                                faceTouchButtonColor)
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                        [
                            makeToolbarButton(
                                TEXT("Icons.Transform"),
                                LOCTEXT("Rotate180", "Rotate 180"),
                                LOCTEXT(
                                    "Rotate180Tooltip",
                                    "Rotate the animation and its root motion 180 degrees."),
                                FOnClicked::CreateSP(
                                    this,
                                    &SHandposeTimelineWidget::onRotate180Clicked),
                                rotate180ButtonColor)
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                        [
                            makeToolbarButton(
                                TEXT("Icons.Transform"),
                                LOCTEXT("AnimationPlacement", "Place Feet"),
                                LOCTEXT(
                                    "AnimationPlacementTooltip",
                                    "Configure trim-start leg IK against a persistent ground plane."),
                                FOnClicked::CreateSP(
                                    this,
                                    &SHandposeTimelineWidget::onAnimationPlacementClicked),
                                animationPlacementButtonColor)
                        ]
                    ]
                ]
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
                .Padding(6.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 4.0f)
                    [
                        makeGroupLabel(LOCTEXT("PlaybackGroup", "PLAYBACK"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SButton)
                            .ToolTipText(LOCTEXT(
                                "PlayPauseTimelineTooltip",
                                "Play the full animation from the playhead or pause active playback."))
                            .OnClicked(this, &SHandposeTimelineWidget::onPlayPauseClicked)
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                [
                                    SNew(SBox)
                                    .WidthOverride(16.0f)
                                    .HeightOverride(16.0f)
                                    [
                                        SNew(SImage)
                                        .Image_Lambda(
                                            [this]()
                                            {
                                                return FAppStyle::GetBrush(
                                                    playbackController->isPlaying()
                                                        ? TEXT("Icons.Toolbar.Pause")
                                                        : TEXT("Icons.Play"));
                                            })
                                        .ColorAndOpacity(FSlateColor::UseForeground())
                                    ]
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                .Padding(5.0f, 0.0f, 0.0f, 0.0f)
                                [
                                    SNew(STextBlock)
                                    .Text(this, &SHandposeTimelineWidget::getPlayPauseButtonText)
                                ]
                            ]
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(8.0f, 0.0f)
                        [
                            SNew(STextBlock)
                            .Text(this, &SHandposeTimelineWidget::getTimecodeText)
                            .Font(FAppStyle::GetFontStyle(TEXT("MonospacedText")))
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            makeToolbarButton(
                                TEXT("Icons.Refresh"),
                                LOCTEXT("LoopAll", "Loop All"),
                                LOCTEXT(
                                    "LoopAllTooltip",
                                    "Loop the full timeline when using Play."),
                                FOnClicked::CreateSP(
                                    this,
                                    &SHandposeTimelineWidget::onLoopAllClicked),
                                loopAllButtonColor)
                        ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SButton)
                                .ToolTipText(LOCTEXT(
                                    "PreviewSpeedTooltip",
                                    "Cycle the segment preview speed between 0.25x, 0.5x, and 1x."))
                                .OnClicked(this, &SHandposeTimelineWidget::onSpeedClicked)
                                [
                                    SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SImage)
                                        .Image(FAppStyle::GetBrush(TEXT("Icons.Adjust")))
                                        .ColorAndOpacity(FSlateColor::UseForeground())
                                    ]
                                    + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    .Padding(5.0f, 0.0f, 0.0f, 0.0f)
                                    [
                                        SAssignNew(speedText, STextBlock)
                                        .Text(LOCTEXT("DefaultPreviewSpeed", "Speed: 1x"))
                                    ]
                                ]
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(8.0f, 0.0f, 4.0f, 0.0f)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("TimelineZoom", "Zoom"))
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            [
                                SNew(SBox)
                                .WidthOverride(100.0f)
                                [
                                    SNew(SSlider)
                                    .Value_Lambda(
                                        [this]()
                                        {
                                            return (timelineZoom - minimumTimelineZoom) /
                                                (maximumTimelineZoom - minimumTimelineZoom);
                                        })
                                    .OnValueChanged(
                                        this,
                                        &SHandposeTimelineWidget::onZoomChanged)
                                ]
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(4.0f, 0.0f)
                            [
                                SAssignNew(zoomText, STextBlock)
                                .Text(LOCTEXT("DefaultZoom", "100%"))
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                makeToolbarButton(
                                    TEXT("Icons.Help"),
                                    FText::GetEmpty(),
                                    LOCTEXT(
                                        "ShortcutsHelpTooltip",
                                        "Show HandposeTimeline shortcuts."),
                                    FOnClicked::CreateSP(
                                        this,
                                        &SHandposeTimelineWidget::onShortcutsHelpClicked),
                                    defaultButtonColor)
                            ]
                        ]
                    ]
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
                .Padding(6.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 4.0f)
                    [
                        makeGroupLabel(LOCTEXT("FinishingGroup", "FINISHING"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SComboButton)
                            .ToolTipText(LOCTEXT(
                                "DecisionDataTooltip",
                                "Save or import HandposeTimeline decision data."))
                            .OnGetMenuContent_Lambda(
                                [this]()
                                {
                                    FMenuBuilder menuBuilder(true, nullptr);
                                    menuBuilder.AddMenuEntry(
                                        LOCTEXT("SaveDecisionData", "Save"),
                                        LOCTEXT(
                                            "SaveDecisionDataTooltip",
                                            "Capture the final timeline decisions and immediately write the Data Asset and JSON."),
                                        FSlateIcon(
                                            FAppStyle::GetAppStyleSetName(),
                                            TEXT("Icons.Save")),
                                        FUIAction(FExecuteAction::CreateLambda(
                                            [this]()
                                            {
                                                onSaveDecisionDataClicked();
                                            })));
                                    menuBuilder.AddMenuEntry(
                                        LOCTEXT("ImportDecisionData", "Import"),
                                        LOCTEXT(
                                            "ImportDecisionDataTooltip",
                                            "Import decision-data JSON and create or update its Data Asset."),
                                        FSlateIcon(
                                            FAppStyle::GetAppStyleSetName(),
                                            TEXT("Icons.Import")),
                                        FUIAction(FExecuteAction::CreateLambda(
                                            [this]()
                                            {
                                                onImportDecisionDataClicked();
                                            })));
                                    return menuBuilder.MakeWidget();
                                })
                            .ButtonContent()
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                [
                                    SNew(SImage)
                                    .Image(FAppStyle::GetBrush(TEXT("Icons.Save")))
                                    .ColorAndOpacity(FSlateColor::UseForeground())
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                .Padding(5.0f, 0.0f, 0.0f, 0.0f)
                                [
                                    SNew(STextBlock)
                                    .Text(LOCTEXT("DecisionData", "Decision Data"))
                                ]
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                        [
                            makeToolbarButton(
                                TEXT("Icons.Package"),
                                LOCTEXT("Bake", "Bake"),
                                LOCTEXT(
                                    "BakeTooltip",
                                    "Bake the current handshape preview to the configured output folder."),
                                FOnClicked::CreateSP(
                                    this,
                                    &SHandposeTimelineWidget::onBakeClicked),
                                defaultButtonColor)
                        ]
                    ]
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.0f, 0.0f, 8.0f, 6.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ConfigStatusLabel", "Config:"))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(4.0f, 0.0f, 12.0f, 0.0f)
            [
                SAssignNew(configNameText, STextBlock)
                .Text(LOCTEXT("NoConfig", "Default settings"))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("AnimationStatusLabel", "Animation:"))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(4.0f, 0.0f, 12.0f, 0.0f)
            [
                SAssignNew(animationNameText, STextBlock)
                .Text(LOCTEXT("NoAnimation", "No animation loaded"))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ActorStatusLabel", "Actor:"))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(4.0f, 0.0f, 12.0f, 0.0f)
            [
                SAssignNew(actorNameText, STextBlock)
                .Text(LOCTEXT("NoActor", "No actor selected"))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 12.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(this, &SHandposeTimelineWidget::getTrimRangeText)
                .ToolTipText(LOCTEXT(
                    "TrimRangeTooltip",
                    "Press [ to set the trim start and ] to set the trim end at the playhead."))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SAssignNew(statusText, STextBlock)
                .Text(LOCTEXT("LoadPrompt", "Load an animation to generate segments."))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
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
                    .OnFaceTouchCorrectionSelected(
                        this,
                        &SHandposeTimelineWidget::onFaceTouchCorrectionSelected)
                    .OnFaceTouchCorrectionChanged(
                        this,
                        &SHandposeTimelineWidget::onFaceTouchCorrectionChanged)
                    .OnFaceTouchCorrectionEditRequested(
                        this,
                        &SHandposeTimelineWidget::onFaceTouchCorrectionEditRequested)
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
                    .OnFaceTouchCorrectionSelected(
                        this,
                        &SHandposeTimelineWidget::onFaceTouchCorrectionSelected)
                    .OnFaceTouchCorrectionChanged(
                        this,
                        &SHandposeTimelineWidget::onFaceTouchCorrectionChanged)
                    .OnFaceTouchCorrectionEditRequested(
                        this,
                        &SHandposeTimelineWidget::onFaceTouchCorrectionEditRequested)
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
    restoreLastActor();
    if (animationPlacementEnabled && loadedAnimation.IsValid())
    {
        rebuildPreviewAnimation(false);
    }
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
        const TArray<FFaceTouchCorrectionBlock>& corrections =
            getFaceTouchCorrections(selectedHandSide);
        const int32 selectedCorrectionIndex =
            getSelectedFaceTouchCorrectionIndex(selectedHandSide);
        if (corrections.IsValidIndex(selectedCorrectionIndex))
        {
            removeSelectedFaceTouchCorrection();
        }
        else
        {
            removeSelectedAssignment();
        }
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
    if (keyEvent.GetKey() == EKeys::LeftBracket && !keyEvent.IsRepeat())
    {
        setTrimStartAtPlayhead();
        return FReply::Handled();
    }
    if (keyEvent.GetKey() == EKeys::RightBracket && !keyEvent.IsRepeat())
    {
        setTrimEndAtPlayhead();
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
        saveLastActor(*selectedActor);
        actorNameText->SetText(FText::FromString(selectedActor->GetActorLabel()));
        statusText->SetText(FText::Format(
            LOCTEXT("PreviewActorSelected", "Using {0} for segment previews."),
            FText::FromString(selectedActor->GetActorLabel())));
        if (animationPlacementEnabled && loadedAnimation.IsValid())
        {
            rebuildPreviewAnimation(true);
        }
        if (faceTouchCorrectionModeEnabled)
        {
            IFaceTouchCorrectionEditorModule::get().beginFacePicking(
                selectedActor,
                FOnFaceTouchSurfaceClicked::CreateSP(
                    this,
                    &SHandposeTimelineWidget::onFaceSurfaceClicked));
        }
    }
    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onFaceTouchCorrectionModeClicked()
{
    faceTouchCorrectionModeEnabled = !faceTouchCorrectionModeEnabled;
    if (faceTouchCorrectionModeEnabled)
    {
        IFaceTouchCorrectionEditorModule::get().beginFacePicking(
            playbackController->getActor(),
            FOnFaceTouchSurfaceClicked::CreateSP(
                this,
                &SHandposeTimelineWidget::onFaceSurfaceClicked));
    }
    else
    {
        IFaceTouchCorrectionEditorModule::get().endFacePicking();
    }
    statusText->SetText(faceTouchCorrectionModeEnabled
        ? LOCTEXT(
            "FaceTouchCorrectionModeEnabled",
            "Face touch correction mode is active. Select a frame, then click the target face.")
        : LOCTEXT(
            "FaceTouchCorrectionModeDisabled",
            "Face touch correction mode is inactive."));
    refreshTimeline();
    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onRotate180Clicked()
{
    if (loadedAnimation.Get() == nullptr)
    {
        statusText->SetText(LOCTEXT(
            "Rotate180RequiresAnimation",
            "Load an animation before enabling Rotate 180."));
        return FReply::Handled();
    }

    rotate180Enabled = !rotate180Enabled;
    GConfig->SetBool(
        configPersistenceSection,
        rotate180PersistenceKey,
        rotate180Enabled,
        GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
    rebuildPreviewAnimation(true);
    statusText->SetText(rotate180Enabled
        ? LOCTEXT("Rotate180Enabled", "Rotate 180 is enabled.")
        : LOCTEXT("Rotate180Disabled", "Rotate 180 is disabled."));
    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onAnimationPlacementClicked()
{
    if (const TSharedPtr<SWindow> existingWindow = animationPlacementWindow.Pin())
    {
        existingWindow->BringToFront(true);
        return FReply::Handled();
    }

    FAnimationPlacementEditorSettings initialSettings;
    initialSettings.enabled = animationPlacementEnabled;
    initialSettings.groundPlaneActor = groundPlaneActor;
    initialSettings.leftLeg = leftLegPlacement;
    initialSettings.rightLeg = rightLegPlacement;

    const TSharedRef<SWindow> window = SNew(SWindow)
        .Title(LOCTEXT("AnimationPlacementTitle", "Animation Placement"))
        .ClientSize(FVector2D(560.0f, 285.0f))
        .SupportsMaximize(false)
        .SupportsMinimize(false);
    animationPlacementWindow = window;
    window->SetContent(
        SNew(SAnimationPlacementSettingsWidget)
        .InitialSettings(initialSettings)
        .OwnerWindow(window)
        .OnSettingsAccepted(
            FOnAnimationPlacementSettingsAccepted::CreateSP(
                this,
                &SHandposeTimelineWidget::applyAnimationPlacementSettings)));
    window->SetOnWindowClosed(FOnWindowClosed::CreateLambda(
        [this](const TSharedRef<SWindow>&)
        {
            animationPlacementWindow.Reset();
        }));
    FSlateApplication::Get().AddWindow(window);
    return FReply::Handled();
}

FReply SHandposeTimelineWidget::onLoopAllClicked()
{
    playbackController->toggleLoopAll();
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

    const EHandposeTimelinePlaybackResult result =
        playbackController->playTimeline();
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

FReply SHandposeTimelineWidget::onShortcutsHelpClicked()
{
    FMessageDialog::Open(
        EAppMsgType::Ok,
        LOCTEXT(
            "ShortcutsHelpText",
            "Keyboard\n"
            "Space    Play or pause\n"
            "Left / Right    Step one frame\n"
            "[    Set trim start at the playhead\n"
            "]    Set trim end at the playhead\n"
            "Delete    Remove the selected assigned handshape\n\n"
            "Mouse\n"
            "Ctrl + Wheel    Zoom the timeline\n"
            "Shift + Wheel    Scroll horizontally\n"
            "Drag the frame ruler    Scrub the animation\n"
            "Click a segment    Preview it and open the handshape picker\n"
            "Right-click an assignment    Edit skipped fingers\n"
            "Drag assignment edges or handles    Adjust its range and blending"),
        LOCTEXT("ShortcutsHelpTitle", "HandposeTimeline Shortcuts"));
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
        hasPendingChanges();
}

bool SHandposeTimelineWidget::hasPendingChanges() const
{
    return hasAssignments() ||
        !rightFaceTouchCorrections.IsEmpty() ||
        !leftFaceTouchCorrections.IsEmpty() ||
        rotate180Enabled ||
        animationPlacementEnabled ||
        hasCustomPlaybackRange();
}

bool SHandposeTimelineWidget::hasAssignments() const
{
    return !rightAssignments.IsEmpty() || !leftAssignments.IsEmpty();
}

bool SHandposeTimelineWidget::hasCustomPlaybackRange() const
{
    const UAnimSequenceBase* animation = loadedAnimation.Get();
    return animation != nullptr &&
        (trimStartFrame > 0 ||
            trimEndFrame < FMath::Max(0, animation->GetNumberOfSampledKeys() - 1));
}

void SHandposeTimelineWidget::getPlaybackRange(
    int32& startFrame,
    int32& endFrame) const
{
    startFrame = trimStartFrame;
    endFrame = trimEndFrame;
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
    const bool hasFaceTouchCorrections =
        !rightFaceTouchCorrections.IsEmpty() ||
        !leftFaceTouchCorrections.IsEmpty();
    if (!hasPendingChanges())
    {
        errorMessage = TEXT("Make a timeline change before baking.");
        return false;
    }
    if ((hasAssignments() || hasFaceTouchCorrections || rotate180Enabled ||
            animationPlacementEnabled) &&
        !previewAnimation.IsValid())
    {
        rebuildPreviewAnimation(false);
    }
    if ((hasAssignments() || hasFaceTouchCorrections || rotate180Enabled ||
            animationPlacementEnabled) &&
        !previewAnimation.IsValid())
    {
        errorMessage = TEXT("The preview could not be built. Resolve the timeline status error before baking.");
        return false;
    }

    const FHandposeBakeResult result = FHandposeBakeService::bake(
        previewAnimation.IsValid() ? previewAnimation.Get() : sourceAnimation,
        sourceAnimation,
        config->getBakedAnimationOutputPath(),
        trimStartFrame,
        trimEndFrame);
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
    const FHandshapeCandidate& candidate,
    const bool alsoPasteForOtherHand)
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
    assignment.mirrorToOppositeHand = candidate.mirrorToOppositeHand;
    assignment.skippedFingers = candidate.skippedFingers;
    linkOverlappingAssignments(handSide);
    updateAssignmentCoverage(handSide);

    bool pastedForOtherHand = false;
    if (alsoPasteForOtherHand)
    {
        const FHandposeAssignment sourceAssignment =
            assignments[selectedAssignmentIndex];
        const EHandposeSide otherHandSide = handSide == EHandposeSide::Right
            ? EHandposeSide::Left
            : EHandposeSide::Right;
        TArray<FHandposeTimelineSegment>& otherSegments = getSegments(otherHandSide);
        TArray<FHandposeAssignment>& otherAssignments = getAssignments(otherHandSide);

        int32 otherSegmentIndex = INDEX_NONE;
        int32 bestOverlap = 0;
        double bestCenterDistance = TNumericLimits<double>::Max();
        const double sourceCenter =
            (sourceAssignment.fullPoseStartFrame + sourceAssignment.fullPoseEndFrame) * 0.5;
        for (int32 otherIndex = 0; otherIndex < otherSegments.Num(); ++otherIndex)
        {
            const FHandposeTimelineSegment& otherSegment = otherSegments[otherIndex];
            const int32 overlap = FMath::Max(
                0,
                FMath::Min(
                    sourceAssignment.fullPoseEndFrame,
                    otherSegment.endFrame) -
                    FMath::Max(
                        sourceAssignment.fullPoseStartFrame,
                        otherSegment.startFrame) + 1);
            const double centerDistance = FMath::Abs(
                sourceCenter -
                (otherSegment.startFrame + otherSegment.endFrame) * 0.5);
            if (overlap > bestOverlap ||
                (overlap == bestOverlap && centerDistance < bestCenterDistance))
            {
                otherSegmentIndex = otherIndex;
                bestOverlap = overlap;
                bestCenterDistance = centerDistance;
            }
        }

        int32 otherAssignmentIndex = otherAssignments.IndexOfByPredicate(
            [otherSegmentIndex, sourceCenter](const FHandposeAssignment& otherAssignment)
            {
                return (otherSegmentIndex != INDEX_NONE &&
                        (otherAssignment.sourceSegmentIndex == otherSegmentIndex ||
                            otherAssignment.coveredSegmentIndices.Contains(
                                otherSegmentIndex))) ||
                    (sourceCenter >= otherAssignment.blendStartFrame &&
                        sourceCenter <= otherAssignment.blendEndFrame);
            });
        const FGuid assignmentId = otherAssignments.IsValidIndex(otherAssignmentIndex)
            ? otherAssignments[otherAssignmentIndex].assignmentId
            : FGuid::NewGuid();
        if (otherAssignmentIndex == INDEX_NONE)
        {
            otherAssignmentIndex = otherAssignments.AddDefaulted();
        }

        FHandposeAssignment otherAssignment = sourceAssignment;
        otherAssignment.assignmentId = assignmentId;
        otherAssignment.sourceSegmentIndex = otherSegmentIndex;
        otherAssignment.handSide = otherHandSide;
        otherAssignment.mirrorToOppositeHand =
            !sourceAssignment.mirrorToOppositeHand;
        otherAssignment.coveredSegmentIndices.Reset();
        otherAssignments[otherAssignmentIndex] = MoveTemp(otherAssignment);
        linkOverlappingAssignments(otherHandSide);
        updateAssignmentCoverage(otherHandSide);
        pastedForOtherHand = true;
    }

    statusText->SetText(FText::Format(
        pastedForOtherHand
            ? LOCTEXT(
                "HandshapeAssignedBothHands",
                "Assigned {0} to Segment {1} and the opposite-hand lane.")
            : LOCTEXT(
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
    selectedRightFaceTouchCorrectionIndex = INDEX_NONE;
    selectedLeftFaceTouchCorrectionIndex = INDEX_NONE;
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

void SHandposeTimelineWidget::onFaceTouchCorrectionSelected(
    const EHandposeSide handSide,
    const int32 correctionIndex)
{
    TArray<FFaceTouchCorrectionBlock>& corrections = getFaceTouchCorrections(handSide);
    if (!corrections.IsValidIndex(correctionIndex))
    {
        return;
    }

    selectedHandSide = handSide;
    selectedRightAssignmentIndex = INDEX_NONE;
    selectedLeftAssignmentIndex = INDEX_NONE;
    getSelectedFaceTouchCorrectionIndex(
        handSide == EHandposeSide::Left
            ? EHandposeSide::Right
            : EHandposeSide::Left) = INDEX_NONE;
    getSelectedFaceTouchCorrectionIndex(handSide) = correctionIndex;
    const FFaceTouchCorrectionBlock& correction = corrections[correctionIndex];
    statusText->SetText(FText::Format(
        LOCTEXT(
            "FaceTouchCorrectionSelected",
            "{0}: zero {1}, touch {2}-{3}, zero {4}."),
        correction.getDisplayName(),
        FText::AsNumber(correction.approachZeroFrame),
        FText::AsNumber(correction.touchStartFrame),
        FText::AsNumber(correction.touchEndFrame),
        FText::AsNumber(correction.recoveryZeroFrame)));
    refreshTimeline();
}

void SHandposeTimelineWidget::onFaceTouchCorrectionChanged(
    const EHandposeSide handSide,
    const int32 correctionIndex,
    const FFaceTouchCorrectionBlock& correction,
    const bool isFinal)
{
    TArray<FFaceTouchCorrectionBlock>& corrections = getFaceTouchCorrections(handSide);
    if (!corrections.IsValidIndex(correctionIndex))
    {
        return;
    }

    corrections[correctionIndex] = correction;
    corrections[correctionIndex].handSide = handSide == EHandposeSide::Left
        ? EFaceTouchHandSide::Left
        : EFaceTouchHandSide::Right;
    getSelectedFaceTouchCorrectionIndex(handSide) = correctionIndex;
    selectedHandSide = handSide;
    if (isFinal)
    {
        rebuildPreviewAnimation(true);
        refreshTimeline();
    }
}

void SHandposeTimelineWidget::onFaceTouchCorrectionEditRequested(
    const EHandposeSide handSide,
    const int32 correctionIndex)
{
    const TArray<FFaceTouchCorrectionBlock>& corrections =
        getFaceTouchCorrections(handSide);
    if (!corrections.IsValidIndex(correctionIndex))
    {
        return;
    }

    const TSharedRef<SWidget> popup =
        SNew(SBorder)
        .Padding(4.0f)
        .BorderImage(FAppStyle::GetBrush("Menu.Background"))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("MoveTouchCloser", "+"))
                .ToolTipText(LOCTEXT(
                    "MoveTouchCloserTooltip",
                    "Move the selected interaction point 0.1 cm closer to the surface."))
                .OnClicked(
                    this,
                    &SHandposeTimelineWidget::adjustFaceTouchNormalOffset,
                    handSide,
                    correctionIndex,
                    0.1f)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("MoveTouchFurther", "−"))
                .ToolTipText(LOCTEXT(
                    "MoveTouchFurtherTooltip",
                    "Move the selected interaction point 0.1 cm further from the surface."))
                .OnClicked(
                    this,
                    &SHandposeTimelineWidget::adjustFaceTouchNormalOffset,
                    handSide,
                    correctionIndex,
                    -0.1f)
            ]
        ];
    FSlateApplication::Get().PushMenu(
        SharedThis(this),
        FWidgetPath(),
        popup,
        FSlateApplication::Get().GetCursorPos(),
        FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}

FReply SHandposeTimelineWidget::adjustFaceTouchNormalOffset(
    const EHandposeSide handSide,
    const int32 correctionIndex,
    const float offsetDelta)
{
    TArray<FFaceTouchCorrectionBlock>& corrections =
        getFaceTouchCorrections(handSide);
    if (!corrections.IsValidIndex(correctionIndex))
    {
        return FReply::Handled();
    }

    FFaceTouchCorrectionBlock& correction = corrections[correctionIndex];
    correction.surfaceLocation.normalOffset = FMath::Clamp(
        correction.surfaceLocation.normalOffset + offsetDelta,
        -5.0f,
        5.0f);
    selectedHandSide = handSide;
    getSelectedFaceTouchCorrectionIndex(handSide) = correctionIndex;
    statusText->SetText(FText::Format(
        LOCTEXT(
            "FaceTouchNormalOffsetChanged",
            "{0}: surface offset {1} cm."),
        correction.getDisplayName(),
        FText::AsNumber(correction.surfaceLocation.normalOffset)));
    FSlateApplication::Get().DismissAllMenus();
    rebuildPreviewAnimation(true);
    refreshTimeline();
    return FReply::Handled();
}

void SHandposeTimelineWidget::onFaceSurfaceClicked(
    const FFaceTouchSurfaceLocation& surfaceLocation)
{
    pendingFaceTouchSurfaceLocation = surfaceLocation;
    pendingFaceTouchFrame = getCurrentFrame();

    const TArray<FName> pointIds = getFaceTouchPointIds();
    if (pointIds.IsEmpty())
    {
        statusText->SetText(LOCTEXT(
            "NoFaceTouchPoints",
            "The selected actor has no left- or right-hand interaction points."));
        return;
    }

    if (const TSharedPtr<SWindow> existingWindow = faceTouchPointPickerWindow.Pin())
    {
        existingWindow->RequestDestroyWindow();
    }

    const TSharedRef<SWindow> pickerWindow = SNew(SWindow)
        .Title(LOCTEXT("FaceTouchPointPickerTitle", "Select Touching Fingers"))
        .ClientSize(FVector2D(390.0f, 360.0f))
        .SupportsMaximize(false)
        .SupportsMinimize(false);
    const TSharedRef<SFaceTouchPointPicker> picker = SNew(SFaceTouchPointPicker)
        .PointIds(pointIds)
        .OnPointsChosen(
            this,
            &SHandposeTimelineWidget::onFaceTouchPointsChosen);
    picker->setOwnerWindow(pickerWindow);
    pickerWindow->SetContent(picker);
    faceTouchPointPickerWindow = pickerWindow;

    const TSharedPtr<SWindow> parentWindow =
        FSlateApplication::Get().FindWidgetWindow(SharedThis(this));
    if (parentWindow.IsValid())
    {
        FSlateApplication::Get().AddWindowAsNativeChild(
            pickerWindow,
            parentWindow.ToSharedRef());
    }
    else
    {
        FSlateApplication::Get().AddWindow(pickerWindow);
    }
}

void SHandposeTimelineWidget::onFaceTouchPointsChosen(
    const TArray<FName>& pointIds)
{
    UAnimSequenceBase* animation = loadedAnimation.Get();
    if (animation == nullptr || pendingFaceTouchFrame == INDEX_NONE || pointIds.IsEmpty())
    {
        return;
    }

    TOptional<EHandposeSide> pointHandSide;
    for (const FName pointId : pointIds)
    {
        const FString pointName = pointId.ToString();
        const TOptional<EHandposeSide> currentSide =
            pointName.StartsWith(TEXT("left"), ESearchCase::IgnoreCase)
                ? TOptional<EHandposeSide>(EHandposeSide::Left)
                : pointName.StartsWith(TEXT("right"), ESearchCase::IgnoreCase)
                    ? TOptional<EHandposeSide>(EHandposeSide::Right)
                    : TOptional<EHandposeSide>();
        if (!currentSide.IsSet())
        {
            continue;
        }
        if (pointHandSide.IsSet() && pointHandSide.GetValue() != currentSide.GetValue())
        {
            statusText->SetText(LOCTEXT(
                "MixedFaceTouchHands",
                "Create separate face-touch corrections for the left and right hands."));
            pendingFaceTouchFrame = INDEX_NONE;
            return;
        }
        pointHandSide = currentSide;
    }
    if (!pointHandSide.IsSet())
    {
        statusText->SetText(LOCTEXT(
            "UnknownFaceTouchHand",
            "The selected interaction points do not identify a left or right hand."));
        pendingFaceTouchFrame = INDEX_NONE;
        return;
    }
    pendingFaceTouchHandSide = pointHandSide.GetValue();

    const int32 lastFrame = FMath::Max(0, animation->GetNumberOfSampledKeys() - 1);
    int32 approachZeroFrame = FMath::Max(0, pendingFaceTouchFrame - 10);
    int32 recoveryZeroFrame = FMath::Min(lastFrame, pendingFaceTouchFrame + 10);
    const TArray<FHandposeTimelineSegment>& segments =
        getSegments(pendingFaceTouchHandSide);
    const int32 selectedSegmentIndex =
        getSelectedSegmentIndex(pendingFaceTouchHandSide);
    if (segments.IsValidIndex(selectedSegmentIndex))
    {
        approachZeroFrame = FMath::Min(
            pendingFaceTouchFrame,
            segments[selectedSegmentIndex].startFrame);
        recoveryZeroFrame = FMath::Max(
            pendingFaceTouchFrame,
            segments[selectedSegmentIndex].endFrame);
    }

    FFaceTouchCorrectionBlock correction;
    correction.handSide = pendingFaceTouchHandSide == EHandposeSide::Left
        ? EFaceTouchHandSide::Left
        : EFaceTouchHandSide::Right;
    correction.interactionPointIds = pointIds;
    correction.solveFrame = pendingFaceTouchFrame;
    correction.approachZeroFrame = approachZeroFrame;
    correction.touchStartFrame = pendingFaceTouchFrame;
    correction.touchEndFrame = FMath::Min(
        recoveryZeroFrame,
        pendingFaceTouchFrame + 1);
    correction.recoveryZeroFrame = recoveryZeroFrame;
    correction.surfaceLocation = pendingFaceTouchSurfaceLocation;

    TArray<FFaceTouchCorrectionBlock>& corrections =
        getFaceTouchCorrections(pendingFaceTouchHandSide);
    const int32 correctionIndex = corrections.Add(correction);
    getSelectedFaceTouchCorrectionIndex(pendingFaceTouchHandSide) = correctionIndex;
    getSelectedFaceTouchCorrectionIndex(
        pendingFaceTouchHandSide == EHandposeSide::Left
            ? EHandposeSide::Right
            : EHandposeSide::Left) = INDEX_NONE;
    selectedRightAssignmentIndex = INDEX_NONE;
    selectedLeftAssignmentIndex = INDEX_NONE;
    selectedHandSide = pendingFaceTouchHandSide;
    pendingFaceTouchFrame = INDEX_NONE;

    statusText->SetText(FText::Format(
        LOCTEXT(
            "FaceTouchCorrectionCreated",
            "Created {0} at frame {1}. Drag the block edges and handles to adjust timing."),
        correction.getDisplayName(),
        FText::AsNumber(correction.touchStartFrame)));
    rebuildPreviewAnimation(true);
    refreshTimeline();
}

TArray<FName> SHandposeTimelineWidget::getFaceTouchPointIds() const
{
    TArray<FName> pointIds;
    AActor* actor = playbackController->getActor();
    if (actor == nullptr)
    {
        return pointIds;
    }

    TArray<USemanticInteractionPointComponent*> pointComponents;
    actor->GetComponents(pointComponents);
    for (const USemanticInteractionPointComponent* point : pointComponents)
    {
        if (point == nullptr)
        {
            continue;
        }
        const FString pointName = point->pointId.ToString();
        if (pointName.StartsWith(TEXT("left"), ESearchCase::IgnoreCase) ||
            pointName.StartsWith(TEXT("right"), ESearchCase::IgnoreCase))
        {
            pointIds.AddUnique(point->pointId);
        }
    }
    pointIds.Sort(FNameLexicalLess());
    return pointIds;
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

void SHandposeTimelineWidget::removeSelectedFaceTouchCorrection()
{
    TArray<FFaceTouchCorrectionBlock>& corrections =
        getFaceTouchCorrections(selectedHandSide);
    int32& selectedCorrectionIndex =
        getSelectedFaceTouchCorrectionIndex(selectedHandSide);
    if (!corrections.IsValidIndex(selectedCorrectionIndex))
    {
        return;
    }

    const FText correctionName =
        corrections[selectedCorrectionIndex].getDisplayName();
    corrections.RemoveAt(selectedCorrectionIndex);
    selectedCorrectionIndex = INDEX_NONE;

    statusText->SetText(FText::Format(
        LOCTEXT("FaceTouchCorrectionRemoved", "Removed {0}."),
        correctionName));
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

void SHandposeTimelineWidget::setTrimStartAtPlayhead()
{
    if (!loadedAnimation.IsValid())
    {
        return;
    }
    trimStartFrame = getCurrentFrame();
    trimEndFrame = FMath::Max(trimEndFrame, trimStartFrame);
    applyPlaybackRange();
    statusText->SetText(FText::Format(
        LOCTEXT("TrimStartSet", "Trim start set to frame {0}."),
        FText::AsNumber(trimStartFrame)));
    refreshTimeline();
    if (animationPlacementEnabled)
    {
        rebuildPreviewAnimation(true);
    }
}

void SHandposeTimelineWidget::setTrimEndAtPlayhead()
{
    if (!loadedAnimation.IsValid())
    {
        return;
    }
    trimEndFrame = getCurrentFrame();
    trimStartFrame = FMath::Min(trimStartFrame, trimEndFrame);
    applyPlaybackRange();
    statusText->SetText(FText::Format(
        LOCTEXT("TrimEndSet", "Trim end set to frame {0}."),
        FText::AsNumber(trimEndFrame)));
    refreshTimeline();
}

void SHandposeTimelineWidget::applyPlaybackRange()
{
    playbackController->setPlaybackRange(trimStartFrame, trimEndFrame);
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
    rightFaceTouchCorrections.Reset();
    leftFaceTouchCorrections.Reset();
    selectedRightSegmentIndex = INDEX_NONE;
    selectedLeftSegmentIndex = INDEX_NONE;
    selectedRightAssignmentIndex = INDEX_NONE;
    selectedLeftAssignmentIndex = INDEX_NONE;
    selectedRightFaceTouchCorrectionIndex = INDEX_NONE;
    selectedLeftFaceTouchCorrectionIndex = INDEX_NONE;
    selectedHandSide = EHandposeSide::Right;
    trimStartFrame = 0;
    trimEndFrame = animation != nullptr
        ? FMath::Max(0, animation->GetNumberOfSampledKeys() - 1)
        : 0;
    applyPlaybackRange();

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
    if (rotate180Enabled || animationPlacementEnabled)
    {
        rebuildPreviewAnimation(false);
    }
    refreshTimeline();
}

void SHandposeTimelineWidget::resetSession()
{
    if (const TSharedPtr<SWindow> pickerWindow = handshapePickerWindow.Pin())
    {
        pickerWindow->RequestDestroyWindow();
    }
    handshapePickerWindow.Reset();
    if (const TSharedPtr<SWindow> pickerWindow = faceTouchPointPickerWindow.Pin())
    {
        pickerWindow->RequestDestroyWindow();
    }
    faceTouchPointPickerWindow.Reset();
    IFaceTouchCorrectionEditorModule::get().endFacePicking();

    AActor* retainedActor = playbackController->getActor();
    playbackController->pause();
    playbackController = MakeShared<FHandposeTimelinePlaybackController>();
    if (retainedActor != nullptr)
    {
        playbackController->setActor(retainedActor);
    }
    loadedAnimation.Reset();
    previewAnimation.Reset();
    rightSegments.Reset();
    leftSegments.Reset();
    rightAssignments.Reset();
    leftAssignments.Reset();
    rightFaceTouchCorrections.Reset();
    leftFaceTouchCorrections.Reset();
    selectedRightSegmentIndex = INDEX_NONE;
    selectedLeftSegmentIndex = INDEX_NONE;
    selectedRightAssignmentIndex = INDEX_NONE;
    selectedLeftAssignmentIndex = INDEX_NONE;
    selectedRightFaceTouchCorrectionIndex = INDEX_NONE;
    selectedLeftFaceTouchCorrectionIndex = INDEX_NONE;
    selectedHandSide = EHandposeSide::Right;
    timelineZoom = minimumTimelineZoom;
    trimStartFrame = 0;
    trimEndFrame = 0;
    faceTouchCorrectionModeEnabled = false;
    pendingFaceTouchFrame = INDEX_NONE;

    if (horizontalTimelineScrollBox.IsValid())
    {
        horizontalTimelineScrollBox->SetScrollOffset(0.0f);
    }
    animationNameText->SetText(LOCTEXT("NoAnimationAfterReset", "No animation loaded"));
    actorNameText->SetText(retainedActor != nullptr
        ? FText::FromString(retainedActor->GetActorLabel())
        : LOCTEXT("NoActorAfterReset", "No actor selected"));
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

void SHandposeTimelineWidget::restoreLastActor()
{
    if (GConfig == nullptr)
    {
        return;
    }

    FString actorPath;
    if (!GConfig->GetString(
            configPersistenceSection,
            actorPersistenceKey,
            actorPath,
            GEditorPerProjectIni) ||
        actorPath.IsEmpty())
    {
        return;
    }

    AActor* actor = Cast<AActor>(FSoftObjectPath(actorPath).ResolveObject());
    if (actor == nullptr ||
        playbackController->setActor(actor) != EHandposeTimelineActorResult::Success)
    {
        return;
    }

    actorNameText->SetText(FText::FromString(actor->GetActorLabel()));
}

void SHandposeTimelineWidget::saveLastActor(const AActor& actor) const
{
    if (GConfig == nullptr)
    {
        return;
    }

    GConfig->SetString(
        configPersistenceSection,
        actorPersistenceKey,
        *actor.GetPathName(),
        GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

void SHandposeTimelineWidget::restoreAnimationPlacementSettings()
{
    if (GConfig == nullptr)
    {
        return;
    }
    GConfig->GetBool(
        configPersistenceSection,
        placementEnabledPersistenceKey,
        animationPlacementEnabled,
        GEditorPerProjectIni);

    FString value;
    if (GConfig->GetString(
            configPersistenceSection,
            groundPlanePersistenceKey,
            value,
            GEditorPerProjectIni))
    {
        groundPlaneActor = Cast<AActor>(FSoftObjectPath(value).ResolveObject());
    }
    if (GConfig->GetString(
            configPersistenceSection,
            leftLegChainPersistenceKey,
            value,
            GEditorPerProjectIni))
    {
        leftLegPlacement.bones = parseBoneNames(value);
    }
    if (GConfig->GetString(
            configPersistenceSection,
            rightLegChainPersistenceKey,
            value,
            GEditorPerProjectIni))
    {
        rightLegPlacement.bones = parseBoneNames(value);
    }
    if (leftLegPlacement.bones.IsEmpty())
    {
        leftLegPlacement.bones = {TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l")};
    }
    if (rightLegPlacement.bones.IsEmpty())
    {
        rightLegPlacement.bones = {TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r")};
    }
    if (GConfig->GetString(
            configPersistenceSection,
            leftToePersistenceKey,
            value,
            GEditorPerProjectIni))
    {
        leftLegPlacement.toeBone = FName(*value);
    }
    if (GConfig->GetString(
            configPersistenceSection,
            rightToePersistenceKey,
            value,
            GEditorPerProjectIni))
    {
        rightLegPlacement.toeBone = FName(*value);
    }
    if (leftLegPlacement.toeBone.IsNone())
    {
        leftLegPlacement.toeBone = TEXT("ball_l");
    }
    if (rightLegPlacement.toeBone.IsNone())
    {
        rightLegPlacement.toeBone = TEXT("ball_r");
    }
}

void SHandposeTimelineWidget::saveAnimationPlacementSettings() const
{
    if (GConfig == nullptr)
    {
        return;
    }
    GConfig->SetBool(
        configPersistenceSection,
        placementEnabledPersistenceKey,
        animationPlacementEnabled,
        GEditorPerProjectIni);
    GConfig->SetString(
        configPersistenceSection,
        groundPlanePersistenceKey,
        groundPlaneActor.IsValid() ? *groundPlaneActor->GetPathName() : TEXT(""),
        GEditorPerProjectIni);
    GConfig->SetString(
        configPersistenceSection,
        leftLegChainPersistenceKey,
        *formatBoneNames(leftLegPlacement.bones),
        GEditorPerProjectIni);
    GConfig->SetString(
        configPersistenceSection,
        rightLegChainPersistenceKey,
        *formatBoneNames(rightLegPlacement.bones),
        GEditorPerProjectIni);
    GConfig->SetString(
        configPersistenceSection,
        leftToePersistenceKey,
        *leftLegPlacement.toeBone.ToString(),
        GEditorPerProjectIni);
    GConfig->SetString(
        configPersistenceSection,
        rightToePersistenceKey,
        *rightLegPlacement.toeBone.ToString(),
        GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

void SHandposeTimelineWidget::applyAnimationPlacementSettings(
    const FAnimationPlacementEditorSettings& settings)
{
    animationPlacementEnabled = settings.enabled;
    groundPlaneActor = settings.groundPlaneActor;
    leftLegPlacement = settings.leftLeg;
    rightLegPlacement = settings.rightLeg;
    saveAnimationPlacementSettings();
    if (loadedAnimation.IsValid())
    {
        rebuildPreviewAnimation(true);
    }
    statusText->SetText(animationPlacementEnabled
        ? LOCTEXT("AnimationPlacementEnabled", "Ground placement is enabled.")
        : LOCTEXT("AnimationPlacementDisabled", "Ground placement is disabled."));
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

    const bool mirrorRightAssetsToLeft = handSide == EHandposeSide::Left;
    const EHandposeSide matchingSide = mirrorRightAssetsToLeft
        ? EHandposeSide::Right
        : handSide;
    UAnimSequenceBase* matchingAnimation = animation;
    TStrongObjectPtr<UAnimSequence> remappedMatchingAnimation;
    if (mirrorRightAssetsToLeft)
    {
        const FHandposeMirrorResult remapResult =
            FHandposeMirrorService::remapAnimationToOppositeHand(
                Cast<UAnimSequence>(animation),
                config->getBoneMapping(EHandposeSide::Left),
                config->getBoneMapping(EHandposeSide::Right),
                config->getHandposeMirrorFlipAxis());
        if (!remapResult.success || remapResult.animation == nullptr)
        {
            errorMessage = remapResult.errorMessage;
            return candidates;
        }
        remappedMatchingAnimation.Reset(remapResult.animation);
        matchingAnimation = remapResult.animation;
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
        if (handposeAsset->handSide != matchingSide)
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
        if (matchingSide != EHandposeSide::Right)
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
            matchingAnimation,
            segment.startFrame,
            segment.endFrame,
            handshapeAnimations,
            FAnimPoseEvaluationOptions(),
            config->getSegmentationSettings(matchingSide),
            config->matchingMethod,
            config->descriptorMethod,
            config->getBoneMapping(matchingSide),
            config->useVirtualLeafBones,
            config->virtualLeafBoneExtensionCm);
    if (!matchingResult.success)
    {
        errorMessage = matchingResult.errorMessage;
        return candidates;
    }

    const FString descriptorMethod =
        StaticEnum<EHandshapeDescriptorMethod>()->GetNameStringByValue(
            static_cast<int64>(config->descriptorMethod));
    const FString matchingMethod =
        StaticEnum<EHandshapeMatchingMethod>()->GetNameStringByValue(
            static_cast<int64>(config->matchingMethod));
    UE_LOG(
        LogHandposeTimelineRecommendations,
        Display,
        TEXT("Recommendation input: animation=%s hand=%s frames=%d-%d fps=%.6f totalFrames=%d descriptor=%s distance=%s candidates=%d virtualTips=%s extensionCm=%.3f"),
        *animation->GetPathName(),
        *handSideToString(handSide),
        segment.startFrame,
        segment.endFrame,
        animation->GetSamplingFrameRate().AsDecimal(),
        animation->GetNumberOfSampledKeys(),
        *descriptorMethod,
        *matchingMethod,
        matchingResult.matches.Num(),
        config->useVirtualLeafBones ? TEXT("true") : TEXT("false"),
        config->virtualLeafBoneExtensionCm);
    if (config->descriptorMethod == EHandshapeDescriptorMethod::HandSkeleton)
    {
        const FHandshapeBoneMapping& mapping = config->getBoneMapping(matchingSide);
        UE_LOG(
            LogHandposeTimelineRecommendations,
            Display,
            TEXT("Recommendation bones: thumb=(%s,%s,%s,%s) index=(%s,%s,%s,%s,%s) middle=(%s,%s,%s,%s,%s) ring=(%s,%s,%s,%s,%s) pinky=(%s,%s,%s,%s,%s)"),
            *mapping.thumb.metacarpal.ToString(),
            *mapping.thumb.proximal.ToString(),
            *mapping.thumb.distal.ToString(),
            *mapping.thumb.tip.ToString(),
            *mapping.index.metacarpal.ToString(),
            *mapping.index.proximal.ToString(),
            *mapping.index.middle.ToString(),
            *mapping.index.distal.ToString(),
            *mapping.index.tip.ToString(),
            *mapping.middle.metacarpal.ToString(),
            *mapping.middle.proximal.ToString(),
            *mapping.middle.middle.ToString(),
            *mapping.middle.distal.ToString(),
            *mapping.middle.tip.ToString(),
            *mapping.ring.metacarpal.ToString(),
            *mapping.ring.proximal.ToString(),
            *mapping.ring.middle.ToString(),
            *mapping.ring.distal.ToString(),
            *mapping.ring.tip.ToString(),
            *mapping.pinky.metacarpal.ToString(),
            *mapping.pinky.proximal.ToString(),
            *mapping.pinky.middle.ToString(),
            *mapping.pinky.distal.ToString(),
            *mapping.pinky.tip.ToString());
        UE_LOG(
            LogHandposeTimelineRecommendations,
            Display,
            TEXT("Recommendation inputDescriptor=[%s]"),
            *descriptorToString(matchingResult.inputDescriptor));
    }
    const int32 loggedMatchCount = FMath::Min(10, matchingResult.matches.Num());
    for (int32 matchIndex = 0; matchIndex < loggedMatchCount; ++matchIndex)
    {
        const FHandshapeMatch& match = matchingResult.matches[matchIndex];
        const UAnimSequenceBase* candidateAnimation = match.handshapeAnimation;
        UE_LOG(
            LogHandposeTimelineRecommendations,
            Display,
            TEXT("Recommendation rank=%d handshape=%s distance=%.12f asset=%s frames=%d fps=%.6f duration=%.6f"),
            matchIndex + 1,
            *match.handshapeId.ToString(),
            match.distance,
            candidateAnimation != nullptr ? *candidateAnimation->GetPathName() : TEXT("None"),
            candidateAnimation != nullptr ? candidateAnimation->GetNumberOfSampledKeys() : 0,
            candidateAnimation != nullptr ? candidateAnimation->GetSamplingFrameRate().AsDecimal() : 0.0,
            candidateAnimation != nullptr ? candidateAnimation->GetPlayLength() : 0.0);
        if (!match.descriptor.IsEmpty())
        {
            UE_LOG(
                LogHandposeTimelineRecommendations,
                Display,
                TEXT("Recommendation rank=%d descriptor=[%s]"),
                matchIndex + 1,
                *descriptorToString(match.descriptor));
        }
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
            candidate.mirrorToOppositeHand = mirrorRightAssetsToLeft;
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
            candidate.mirrorToOppositeHand = mirrorRightAssetsToLeft;
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
    TArray<FFaceTouchCorrectionBlock> faceTouchCorrections =
        rightFaceTouchCorrections;
    faceTouchCorrections.Append(leftFaceTouchCorrections);
    if (sourceAnimation == nullptr ||
        (assignments.IsEmpty() &&
            faceTouchCorrections.IsEmpty() &&
            !rotate180Enabled &&
            !animationPlacementEnabled))
    {
        previewAnimation.Reset();
        playbackController->setAnimation(loadedAnimation.Get());
        applyPlaybackRange();
        return;
    }

    const float previousTime = preserveTime ? getCurrentTimeSeconds() : 0.0f;
    previewAnimation.Reset();
    playbackController->setAnimation(loadedAnimation.Get());
    applyPlaybackRange();
    const TArray<FName> rightBoneNames = loadedConfig.IsValid()
        ? loadedConfig->getApplyBoneNames(EHandposeSide::Right)
        : getSegmentationBoneNames(*sourceAnimation, EHandposeSide::Right);
    const TArray<FName> leftBoneNames = loadedConfig.IsValid()
        ? loadedConfig->getApplyBoneNames(EHandposeSide::Left)
        : getSegmentationBoneNames(*sourceAnimation, EHandposeSide::Left);
    UAnimSequence* previewSource = sourceAnimation;
    if (rotate180Enabled)
    {
        const FAnimationPlacementResult rotationResult =
            FAnimationPlacementService::rotate180(previewSource);
        if (!rotationResult.success || rotationResult.animation == nullptr)
        {
            statusText->SetText(FText::Format(
                LOCTEXT(
                    "Rotate180PreviewBuildFailed",
                    "Could not rotate the preview animation: {0}"),
                FText::FromString(rotationResult.errorMessage)));
            return;
        }
        previewSource = rotationResult.animation;
    }
    if (animationPlacementEnabled)
    {
        AActor* previewActor = playbackController->getActor();
        USkeletalMeshComponent* skeletalMeshComponent = previewActor != nullptr
            ? previewActor->FindComponentByClass<USkeletalMeshComponent>()
            : nullptr;
        AActor* planeActor = groundPlaneActor.Get();
        if (skeletalMeshComponent == nullptr ||
            skeletalMeshComponent->GetSkeletalMeshAsset() == nullptr ||
            planeActor == nullptr)
        {
            statusText->SetText(LOCTEXT(
                "AnimationPlacementMissingSetup",
                "Ground placement needs a preview actor with a skeletal mesh and a ground-plane actor."));
            return;
        }
        FAnimationGroundSettings placementSettings;
        placementSettings.solveFrame = trimStartFrame;
        placementSettings.skeletalMeshComponentTransform =
            skeletalMeshComponent->GetComponentTransform();
        placementSettings.planeWorldPoint = planeActor->GetActorLocation();
        placementSettings.planeWorldNormal = planeActor->GetActorUpVector();
        placementSettings.leftLeg = leftLegPlacement;
        placementSettings.rightLeg = rightLegPlacement;
        const FAnimationPlacementResult placementResult =
            FAnimationPlacementService::placeFeetOnGround(
                previewSource,
                skeletalMeshComponent->GetSkeletalMeshAsset(),
                placementSettings);
        if (!placementResult.success || placementResult.animation == nullptr)
        {
            statusText->SetText(FText::Format(
                LOCTEXT(
                    "AnimationPlacementBuildFailed",
                    "Could not place the animation on the ground: {0}"),
                FText::FromString(placementResult.errorMessage)));
            return;
        }
        previewSource = placementResult.animation;
    }
    if (!assignments.IsEmpty())
    {
        const FHandposePreviewBuildResult handposeResult =
            FHandposePreviewBuilder::build(
                previewSource,
                assignments,
                rightBoneNames,
                leftBoneNames,
                loadedConfig->getBoneMapping(EHandposeSide::Right),
                loadedConfig->getBoneMapping(EHandposeSide::Left),
                loadedConfig->getHandposeMirrorFlipAxis());
        if (!handposeResult.success || handposeResult.animation == nullptr)
        {
            statusText->SetText(FText::Format(
                LOCTEXT("PreviewBuildFailed", "Could not build handpose preview: {0}"),
                FText::FromString(handposeResult.errorMessage)));
            return;
        }
        previewSource = handposeResult.animation;
    }

    if (!faceTouchCorrections.IsEmpty())
    {
        const FHandposePreviewBuildResult faceTouchResult =
            FFaceTouchPreviewBuilder::applyCorrections(
                previewSource,
                playbackController->getActor(),
                faceTouchCorrections);
        if (!faceTouchResult.success || faceTouchResult.animation == nullptr)
        {
            statusText->SetText(FText::Format(
                LOCTEXT(
                    "FaceTouchPreviewBuildFailed",
                    "Could not build face-touch preview: {0}"),
                FText::FromString(faceTouchResult.errorMessage)));
            return;
        }
        previewSource = faceTouchResult.animation;
    }

    previewAnimation.Reset(previewSource);
    playbackController->setAnimation(previewAnimation.Get());
    applyPlaybackRange();
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
            timelineZoom,
            trimStartFrame,
            trimEndFrame,
            faceTouchCorrectionModeEnabled,
            rightFaceTouchCorrections,
            selectedRightFaceTouchCorrectionIndex);
    }
    if (leftTimelineTrack.IsValid())
    {
        leftTimelineTrack->setTimeline(
            loadedAnimation.Get(),
            leftSegments,
            leftAssignments,
            selectedLeftSegmentIndex,
            selectedLeftAssignmentIndex,
            timelineZoom,
            trimStartFrame,
            trimEndFrame,
            faceTouchCorrectionModeEnabled,
            leftFaceTouchCorrections,
            selectedLeftFaceTouchCorrectionIndex);
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

TArray<FFaceTouchCorrectionBlock>& SHandposeTimelineWidget::getFaceTouchCorrections(
    const EHandposeSide handSide)
{
    return handSide == EHandposeSide::Left
        ? leftFaceTouchCorrections
        : rightFaceTouchCorrections;
}

const TArray<FFaceTouchCorrectionBlock>& SHandposeTimelineWidget::getFaceTouchCorrections(
    const EHandposeSide handSide) const
{
    return handSide == EHandposeSide::Left
        ? leftFaceTouchCorrections
        : rightFaceTouchCorrections;
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

int32& SHandposeTimelineWidget::getSelectedFaceTouchCorrectionIndex(
    const EHandposeSide handSide)
{
    return handSide == EHandposeSide::Left
        ? selectedLeftFaceTouchCorrectionIndex
        : selectedRightFaceTouchCorrectionIndex;
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
    sessionData.config.descriptorMethod = StaticEnum<EHandshapeDescriptorMethod>()->GetNameStringByValue(
        static_cast<int64>(config->descriptorMethod));
    sessionData.config.useVirtualLeafBones = config->useVirtualLeafBones;
    sessionData.config.virtualLeafBoneExtensionCm =
        config->virtualLeafBoneExtensionCm;
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
    for (const FName boneName : config->getLeafBoneNames(EHandposeSide::Right))
    {
        sessionData.config.rightHandLeafBoneNames.Add(boneName.ToString());
    }
    for (const FName boneName : config->getLeafBoneNames(EHandposeSide::Left))
    {
        sessionData.config.leftHandLeafBoneNames.Add(boneName.ToString());
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

FText SHandposeTimelineWidget::getTrimRangeText() const
{
    return loadedAnimation.IsValid()
        ? FText::Format(
            LOCTEXT("TrimRange", "Trim: {0}-{1}  ([ / ])"),
            FText::AsNumber(trimStartFrame),
            FText::AsNumber(trimEndFrame))
        : LOCTEXT("NoTrimRange", "Trim: --");
}

int32 SHandposeTimelineWidget::getCurrentFrame() const
{
    const UAnimSequenceBase* animation = loadedAnimation.Get();
    if (animation == nullptr || !animation->GetSamplingFrameRate().IsValid())
    {
        return 0;
    }
    return FMath::Clamp(
        FMath::RoundToInt(
            getCurrentTimeSeconds() * animation->GetSamplingFrameRate().AsDecimal()),
        0,
        FMath::Max(0, animation->GetNumberOfSampledKeys() - 1));
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
