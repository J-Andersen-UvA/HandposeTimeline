#pragma once

#include "CoreMinimal.h"

class UAnimSequence;
class UDataTable;

struct FWordingClip
{
    TArray<FName> names;
    TArray<double> times;
    TArray<TArray<float>> weights;
    double duration = 0.0;
};

struct FVisemeMarker
{
    double position = 0.0;
    double multiplier = 2.0;
    double attack = 0.05;
    double hold = 0.07;
    double release = 0.06;
};

struct FWordingAudio
{
    TArray<float> samples;
    int32 sampleRate = 0;

    bool renderPreview(double speechDuration, double elapsed, double playRate, TArray<int16>& pcm) const;
};

struct FWordingBlock
{
    FString text;
    FWordingClip clip;
    TSharedPtr<const FWordingAudio> audio;
    TArray<FVisemeMarker> markers;
    double start = 0.0;
    double duration = 0.0;
    double leadIn = 0.08;
    double leadOut = 0.08;
};

namespace Wording
{
    UClass* voiceClass();
    bool available();
    UObject* generate(const FString& text, UObject* profile, TSharedPtr<const FWordingAudio>& audio, FString& error);
    bool readResult(const UObject& job, FWordingClip& clip, FString& error);
    bool defaultChannel(FName name);
    UAnimSequence* apply(UAnimSequence& source, const TArray<FWordingBlock>& blocks,
        const UDataTable* faceMap, const UDataTable* tongueMap,
        const TMap<FName, bool>& channelOverrides, bool overwriteAll, FString& error);
}
