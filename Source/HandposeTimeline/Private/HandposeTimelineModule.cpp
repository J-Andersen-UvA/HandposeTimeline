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

TSharedRef<SDockTab> FHandposeTimelineModule::spawnHandposeTimelineTab(
    const FSpawnTabArgs& spawnTabArgs)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SHandposeTimelineWidget)
        ];
}

IMPLEMENT_MODULE(FHandposeTimelineModule, HandposeTimeline)

#undef LOCTEXT_NAMESPACE
