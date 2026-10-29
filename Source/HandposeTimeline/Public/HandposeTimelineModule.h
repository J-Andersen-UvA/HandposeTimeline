#pragma once

#include "Modules/ModuleManager.h"

class FSpawnTabArgs;
class SDockTab;
class SHandposeTimelineWidget;
class UAnimSequence;
class UAnimSequenceBase;

class IHandposeTimelineModule : public IModuleInterface
{
public:
    virtual bool openAnimation(UAnimSequenceBase* animation) = 0;
    virtual bool canBakeCurrentAnimation() const = 0;
    virtual bool hasAssignments() const = 0;
    virtual bool bakeCurrentAnimation(
        UAnimSequence*& outputAnimation,
        FString& outputPath,
        FString& errorMessage,
        bool& cancelled) = 0;
    virtual UAnimSequenceBase* getLoadedAnimation() const = 0;
};

class FHandposeTimelineModule : public IHandposeTimelineModule
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    virtual bool openAnimation(UAnimSequenceBase* animation) override;
    virtual bool canBakeCurrentAnimation() const override;
    virtual bool hasAssignments() const override;
    virtual bool bakeCurrentAnimation(
        UAnimSequence*& outputAnimation,
        FString& outputPath,
        FString& errorMessage,
        bool& cancelled) override;
    virtual UAnimSequenceBase* getLoadedAnimation() const override;

private:
    void registerMenus();
    void openHandposeTimeline();
    TSharedRef<SDockTab> spawnHandposeTimelineTab(const FSpawnTabArgs& spawnTabArgs);
    TWeakPtr<SHandposeTimelineWidget> handposeTimelineWidget;
};
