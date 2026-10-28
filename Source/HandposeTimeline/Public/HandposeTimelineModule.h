#pragma once

#include "Modules/ModuleManager.h"

class FHandposeTimelineModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
