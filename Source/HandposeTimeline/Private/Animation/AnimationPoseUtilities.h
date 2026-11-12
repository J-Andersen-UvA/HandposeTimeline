#pragma once

#include "CoreMinimal.h"

class UAnimSequence;
class UAnimSequenceBase;
struct FAnimPose;

namespace HandposeTimeline::AnimationPoseUtilities
{
bool evaluatePoses(
    UAnimSequenceBase& animation,
    TArray<FAnimPose>& poses,
    FString& errorMessage);

UAnimSequence* duplicateTransientAnimation(
    UAnimSequence& sourceAnimation,
    FName baseName);
}
