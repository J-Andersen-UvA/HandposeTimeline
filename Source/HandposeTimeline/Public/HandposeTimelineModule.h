#pragma once

#include "Modules/ModuleManager.h"

class FSpawnTabArgs;
class SDockTab;

class FHandposeTimelineModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void registerMenus();
    void openHandposeTimeline();
    TSharedRef<SDockTab> spawnHandposeTimelineTab(const FSpawnTabArgs& spawnTabArgs);
};
