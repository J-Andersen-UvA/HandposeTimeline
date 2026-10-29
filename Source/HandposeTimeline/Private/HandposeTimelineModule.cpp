#include "HandposeTimelineModule.h"

#include "Framework/Docking/TabManager.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SHandposeTimelineWidget.h"

#define LOCTEXT_NAMESPACE "HandposeTimelineModule"

namespace
{
const FName handposeTimelineTabName(TEXT("HandposeTimeline"));
}

void FHandposeTimelineModule::StartupModule()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        handposeTimelineTabName,
        FOnSpawnTab::CreateRaw(this, &FHandposeTimelineModule::spawnHandposeTimelineTab))
        .SetDisplayName(LOCTEXT("HandposeTimelineTabTitle", "HandposeTimeline"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);

    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(
            this,
            &FHandposeTimelineModule::registerMenus));
}

void FHandposeTimelineModule::ShutdownModule()
{
    handposeTimelineWidget.Reset();
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(handposeTimelineTabName);
}

void FHandposeTimelineModule::registerMenus()
{
    FToolMenuOwnerScoped owner(this);
    UToolMenu* toolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
    FToolMenuSection& section = toolsMenu->FindOrAddSection(TEXT("Tools"));
    section.AddMenuEntry(
        TEXT("HandposeTimeline"),
        LOCTEXT("HandposeTimelineMenuLabel", "HandposeTimeline"),
        LOCTEXT("HandposeTimelineMenuTooltip", "Open HandposeTimeline."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(this, &FHandposeTimelineModule::openHandposeTimeline)));
}

void FHandposeTimelineModule::openHandposeTimeline()
{
    FGlobalTabmanager::Get()->TryInvokeTab(handposeTimelineTabName);
}

bool FHandposeTimelineModule::openAnimation(UAnimSequenceBase* animation)
{
    if (animation == nullptr)
    {
        return false;
    }

    FGlobalTabmanager::Get()->TryInvokeTab(handposeTimelineTabName);
    const TSharedPtr<SHandposeTimelineWidget> widget = handposeTimelineWidget.Pin();
    if (!widget.IsValid())
    {
        return false;
    }

    widget->loadAnimation(animation);
    return true;
}

bool FHandposeTimelineModule::canBakeCurrentAnimation() const
{
    const TSharedPtr<SHandposeTimelineWidget> widget = handposeTimelineWidget.Pin();
    return widget.IsValid() && widget->canBakeCurrentAnimation();
}

bool FHandposeTimelineModule::hasAssignments() const
{
    const TSharedPtr<SHandposeTimelineWidget> widget = handposeTimelineWidget.Pin();
    return widget.IsValid() && widget->hasAssignments();
}

bool FHandposeTimelineModule::bakeCurrentAnimation(
    UAnimSequence*& outputAnimation,
    FString& outputPath,
    FString& errorMessage,
    bool& cancelled)
{
    const TSharedPtr<SHandposeTimelineWidget> widget = handposeTimelineWidget.Pin();
    return widget.IsValid() && widget->bakeCurrentAnimation(
        outputAnimation,
        outputPath,
        errorMessage,
        cancelled);
}

UAnimSequenceBase* FHandposeTimelineModule::getLoadedAnimation() const
{
    const TSharedPtr<SHandposeTimelineWidget> widget = handposeTimelineWidget.Pin();
    return widget.IsValid() ? widget->getLoadedAnimation() : nullptr;
}

TSharedRef<SDockTab> FHandposeTimelineModule::spawnHandposeTimelineTab(
    const FSpawnTabArgs& spawnTabArgs)
{
    const TSharedRef<SHandposeTimelineWidget> widget =
        SNew(SHandposeTimelineWidget);
    handposeTimelineWidget = widget;
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            widget
        ];
}

IMPLEMENT_MODULE(FHandposeTimelineModule, HandposeTimeline)

#undef LOCTEXT_NAMESPACE
