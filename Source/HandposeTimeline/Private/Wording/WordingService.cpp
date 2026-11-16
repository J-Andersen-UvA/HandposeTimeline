#include "WordingService.h"
#include "Algo/BinarySearch.h"

#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
UFunction* function(const TCHAR* classPath, const TCHAR* name)
{
    UClass* type = FindObject<UClass>(nullptr, classPath);
    return type ? type->FindFunctionByName(name) : nullptr;
}

UFunction* piperFunction()
{
    return function(TEXT("/Script/PiperTTS.PiperBlueprintLibrary"), TEXT("synthesizeSpeech"));
}

UFunction* solverFunction()
{
    return function(TEXT("/Script/Audio2Blendshapes.GenerateAudioBlendshapes"), TEXT("GenerateBlendshapesFromPCM"));
}

template<typename T>
const typename T::TCppType* value(const UStruct* type, const void* container, FName name)
{
    const T* property = FindFProperty<T>(type, name);
    return property ? property->template ContainerPtrToValuePtr<typename T::TCppType>(container) : nullptr;
}

template<typename T, typename Item>
bool copyArray(const UStruct* type, const void* container, FName name, TArray<Item>& out)
{
    const auto* array = FindFProperty<FArrayProperty>(type, name);
    if (!array || !CastField<T>(array->Inner))
    {
        return false;
    }
    FScriptArrayHelper helper(array, array->ContainerPtrToValuePtr<void>(container));
    out.Reserve(helper.Num());
    for (int32 index = 0; index < helper.Num(); ++index)
    {
        out.Add(*reinterpret_cast<const Item*>(helper.GetRawPtr(index)));
    }
    return true;
}

void collectTargets(const UDataTable* table, FName source, TSet<FName>& targets)
{
    if (!table || !table->GetRowStruct())
    {
        return;
    }
    const uint8* const* row = table->GetRowMap().Find(source);
    if (row)
    {
        if (const FString* fields = value<FStrProperty>(table->GetRowStruct(), *row, TEXT("Targets")))
        {
            TArray<FString> names;
            fields->ParseIntoArray(names, TEXT("|"), true);
            for (FString& name : names)
            {
                name.TrimStartAndEndInline();
                if (!name.IsEmpty())
                {
                    targets.Add(FName(*name));
                }
            }
        }
    }
}

float sample(const FWordingClip& clip, double time, int32 channel)
{
    const int32 next = Algo::UpperBound(clip.times, time);
    if (next == 0)
    {
        return clip.weights[0][channel];
    }
    if (next >= clip.times.Num())
    {
        return clip.weights.Last()[channel];
    }
    const int32 previous = next - 1;
    const double alpha = (time - clip.times[previous]) / (clip.times[next] - clip.times[previous]);
    return FMath::Lerp(clip.weights[previous][channel], clip.weights[next][channel], alpha);
}
}

UClass* Wording::voiceClass()
{
    return FindObject<UClass>(nullptr, TEXT("/Script/PiperTTS.PiperVoiceProfile"));
}

bool Wording::available()
{
    UFunction* solver = solverFunction();
    return voiceClass() && piperFunction() && solver &&
        FindFProperty<FBoolProperty>(solver->GetOuterUClass(), TEXT("bResultReady"));
}

UObject* Wording::generate(const FString& text, UObject* profile, TSharedPtr<const FWordingAudio>& previewAudio, FString& error)
{
    error = TEXT("The optional Piper or Audio2Blendshapes interface is unavailable.");
    UFunction* piper = piperFunction();
    UFunction* solver = solverFunction();
    if (!piper || !solver || !profile || !profile->IsA(voiceClass()))
    {
        return nullptr;
    }
    FStructOnScope piperParams(piper);
    void* params = piperParams.GetStructMemory();
    const auto* textProperty = FindFProperty<FStrProperty>(piper, TEXT("text"));
    const auto* profileProperty = FindFProperty<FObjectPropertyBase>(piper, TEXT("profile"));
    const auto* resultProperty = CastField<FStructProperty>(piper->GetReturnProperty());
    if (!textProperty || !profileProperty || !resultProperty)
    {
        return nullptr;
    }
    textProperty->SetPropertyValue_InContainer(params, text);
    profileProperty->SetObjectPropertyValue_InContainer(params, profile);
    piper->GetOuterUClass()->GetDefaultObject()->ProcessEvent(piper, params);
    const void* result = resultProperty->ContainerPtrToValuePtr<void>(params);
    const auto* success = FindFProperty<FBoolProperty>(resultProperty->Struct, TEXT("success"));
    if (!success || !success->GetPropertyValue_InContainer(result))
    {
        const FString* message = value<FStrProperty>(resultProperty->Struct, result, TEXT("errorMessage"));
        error = message && !message->IsEmpty() ? *message : TEXT("Piper did not produce audio.");
        return nullptr;
    }

    const auto* audioProperty = FindFProperty<FStructProperty>(resultProperty->Struct, TEXT("audio"));
    if (!audioProperty)
    {
        return nullptr;
    }
    const void* audio = audioProperty->ContainerPtrToValuePtr<void>(result);
    const int32* rate = value<FIntProperty>(audioProperty->Struct, audio, TEXT("sampleRate"));
    TArray<float> samples;
    if (!rate || *rate <= 0 || !copyArray<FFloatProperty>(audioProperty->Struct, audio, TEXT("samples"), samples) ||
        samples.IsEmpty())
    {
        return nullptr;
    }

    FStructOnScope solverParams(solver);
    params = solverParams.GetStructMemory();
    const auto* pcm = FindFProperty<FArrayProperty>(solver, TEXT("Samples"));
    const auto* sampleRate = FindFProperty<FIntProperty>(solver, TEXT("SampleRate"));
    const auto* channels = FindFProperty<FIntProperty>(solver, TEXT("Channels"));
    const auto* jobProperty = CastField<FObjectPropertyBase>(solver->GetReturnProperty());
    if (!pcm || !CastField<FFloatProperty>(pcm->Inner) || !sampleRate || !channels || !jobProperty)
    {
        return nullptr;
    }
    FScriptArrayHelper input(pcm, pcm->ContainerPtrToValuePtr<void>(params));
    input.Resize(samples.Num());
    if (!samples.IsEmpty())
    {
        FMemory::Memcpy(input.GetRawPtr(), samples.GetData(), samples.Num() * sizeof(float));
    }
    sampleRate->SetPropertyValue_InContainer(params, *rate);
    channels->SetPropertyValue_InContainer(params, 1);
    solver->GetOuterUClass()->GetDefaultObject()->ProcessEvent(solver, params);
    auto* job = Cast<UBlueprintAsyncActionBase>(jobProperty->GetObjectPropertyValue_InContainer(params));
    if (job)
    {
        auto savedAudio = MakeShared<FWordingAudio>();
        savedAudio->samples = MoveTemp(samples);
        savedAudio->sampleRate = *rate;
        previewAudio = MoveTemp(savedAudio);
        job->Activate();
        error.Reset();
    }
    return job;
}

bool Wording::readResult(const UObject& job, FWordingClip& clip, FString& error)
{
    const UClass* type = job.GetClass();
    const auto* ready = FindFProperty<FBoolProperty>(type, TEXT("bResultReady"));
    if (ready && !ready->GetPropertyValue_InContainer(&job))
    {
        return false;
    }
    const FString* message = value<FStrProperty>(type, &job, TEXT("ResultError"));
    if (message && !message->IsEmpty())
    {
        error = *message;
        return true;
    }
    const auto* clipProperty = FindFProperty<FStructProperty>(type, TEXT("ResultClip"));
    if (clipProperty)
    {
        const auto* data = clipProperty->ContainerPtrToValuePtr<void>(&job);
        const UStruct* clipType = clipProperty->Struct;
        const auto* frames = FindFProperty<FArrayProperty>(clipType, TEXT("Frames"));
        const auto* frame = frames ? CastField<FStructProperty>(frames->Inner) : nullptr;
        const double* duration = value<FDoubleProperty>(clipType, data, TEXT("AudioDurationSeconds"));
        if (frame && duration && *duration > 0 && copyArray<FNameProperty>(clipType, data, TEXT("CurveNames"), clip.names))
        {
            FScriptArrayHelper list(frames, frames->ContainerPtrToValuePtr<void>(data));
            clip.duration = *duration;
            for (int32 index = 0; index < list.Num(); ++index)
            {
                const void* entry = list.GetRawPtr(index);
                const double* time = value<FDoubleProperty>(frame->Struct, entry, TEXT("TimeSeconds"));
                auto& weights = clip.weights.AddDefaulted_GetRef();
                if (!time || !copyArray<FFloatProperty>(frame->Struct, entry, TEXT("Weights"), weights) ||
                    weights.Num() != clip.names.Num())
                {
                    break;
                }
                clip.times.Add(*time);
            }
            if (!clip.times.IsEmpty() && clip.times.Num() == list.Num())
            {
                return true;
            }
        }
    }
    error = TEXT("Audio2Blendshapes returned an invalid clip.");
    return true;
}

bool Wording::defaultChannel(FName name)
{
    const FString channel = name.ToString();
    return channel.StartsWith(TEXT("mouth")) || channel.StartsWith(TEXT("jaw"));
}

UAnimSequence* Wording::apply(UAnimSequence& source, const TArray<FWordingBlock>& blocks,
    const UDataTable* faceMap, const UDataTable* tongueMap, const TMap<FName, bool>& overrides, bool overwriteAll, FString& error)
{
    struct FPoseWindow
    {
        double in;
        double peak;
        double holdEnd;
        double out;
    };
    const auto windowFor = [](const FWordingBlock& block, const FVisemeMarker& marker)
    {
        const double end = block.start + block.duration;
        const double peak = block.start + marker.position * block.duration;
        const double available = FMath::Max(0.0, end - peak);
        const double release = FMath::Min(marker.release, available);
        const double held = peak + FMath::Min(marker.hold, available - release);
        return FPoseWindow{FMath::Max(block.start, peak - marker.attack), peak, held, held + release};
    };

    struct FLayer
    {
        const FWordingBlock* block = nullptr;
        TArray<int32> channels;
    };
    TMap<FName, TArray<FLayer>> layers;
    for (const auto& block : blocks)
    {
        TMap<FName, FLayer> mapping;
        for (int32 index = 0; index < block.clip.names.Num(); ++index)
        {
            FName name = block.clip.names[index];
            const bool* enabled = overrides.Find(name);
            if (!overwriteAll && !(enabled ? *enabled : defaultChannel(name)))
            {
                continue;
            }
            TSet<FName> targets;
            collectTargets(faceMap, name, targets);
            collectTargets(tongueMap, name, targets);
            for (FName target : targets)
            {
                mapping.FindOrAdd(target).channels.Add(index);
            }
        }
        for (auto& pair : mapping)
        {
            pair.Value.block = &block;
            layers.FindOrAdd(pair.Key).Add(MoveTemp(pair.Value));
        }
    }
    if (layers.IsEmpty())
    {
        return &source;
    }

    UAnimSequence* output = DuplicateObject<UAnimSequence>(&source, GetTransientPackage());
    output->SetFlags(RF_Transient);
    auto& controller = output->GetController();
    const auto* data = source.GetDataModel();
    const double length = source.GetPlayLength();
    const double fps = source.GetSamplingFrameRate().AsDecimal();
    controller.OpenBracket(FText::FromString(TEXT("Apply wording")), false);
    for (const auto& pair : layers)
    {
        const FAnimationCurveIdentifier id(pair.Key, ERawCurveTrackTypes::RCT_Float);
        const FFloatCurve* original = data->FindFloatCurve(id);
        TArray<FRichCurveKey> keys = original ? original->FloatCurve.GetCopyOfKeys() : TArray<FRichCurveKey>{};
        for (const FLayer& layer : pair.Value)
        {
            const auto& block = *layer.block;
            const double speechEnd = block.start + block.duration;
            const double begin = FMath::Max(0.0, block.start - block.leadIn);
            const double end = FMath::Min(length, speechEnd + block.leadOut);
            keys.RemoveAll([&](const FRichCurveKey& key) { return key.Time >= begin && key.Time <= end; });
            auto add = [&](double time)
            {
                const float base = original ? original->FloatCurve.Eval(time) : 0.0f;
                double result = 0;
                if (time < block.start)
                {
                    result = base * (1.0 - FMath::Clamp((time - begin) / (block.start - begin), 0.0, 1.0));
                }
                else if (time > speechEnd)
                {
                    result = base * FMath::Clamp((time - speechEnd) / (end - speechEnd), 0.0, 1.0);
                }
                else
                {
                    const double sourceTime = (time - block.start) / block.duration * block.clip.duration;
                    for (int32 channel : layer.channels)
                    {
                        result += sample(block.clip, sourceTime, channel);
                    }
                    for (const FVisemeMarker& marker : block.markers)
                    {
                        const auto window = windowFor(block, marker);
                        if (time < window.in || time > window.out)
                        {
                            continue;
                        }
                        const double alpha = time < window.peak ? (time - window.in) / (window.peak - window.in)
                            : time > window.holdEnd ? (window.out - time) / (window.out - window.holdEnd) : 1.0;
                        const double smooth = alpha * alpha * (3.0 - 2.0 * alpha);
                        const double gain = FMath::Lerp(1.0, marker.multiplier, smooth);
                        result *= gain;
                    }
                    result = FMath::Clamp(result, -1.0, 1.0);
                }
                FRichCurveKey key(static_cast<float>(time), static_cast<float>(result));
                key.InterpMode = RCIM_Linear;
                keys.Add(key);
            };
            const double frame = 1.0 / fps;
            auto anchor = [&](double time)
            {
                if (!keys.ContainsByPredicate([&](const FRichCurveKey& key) { return FMath::IsNearlyEqual(key.Time, static_cast<float>(time)); }))
                {
                    FRichCurveKey key(static_cast<float>(time), original ? original->FloatCurve.Eval(time) : 0.0f);
                    key.InterpMode = RCIM_Linear;
                    keys.Add(key);
                }
            };
            if (begin > 0.0) { anchor(FMath::Max(0.0, begin - frame)); }
            if (end < length) { anchor(FMath::Min(length, end + frame)); }
            add(begin);
            for (int32 index = FMath::FloorToInt(begin * fps) + 1; index / fps < end; ++index)
            {
                add(index / fps);
            }
            if (end > begin) { add(end); }
            for (double neutralEdge : {block.start - frame * 0.01, speechEnd + frame * 0.01})
            {
                if (neutralEdge > begin && neutralEdge < end)
                {
                    keys.RemoveAll([&](const FRichCurveKey& key) { return key.Time == static_cast<float>(neutralEdge); });
                    FRichCurveKey key(static_cast<float>(neutralEdge), 0.0f);
                    key.InterpMode = RCIM_Linear;
                    keys.Add(key);
                }
            }
            TArray<double> boundaries = {block.start, speechEnd};
            for (const FVisemeMarker& marker : block.markers)
            {
                const auto window = windowFor(block, marker);
                boundaries.Append({window.in, (window.in + window.peak) * 0.5, window.peak,
                    window.holdEnd, (window.holdEnd + window.out) * 0.5, window.out});
                const double step = frame * 0.01;
                if (window.in == window.peak && window.in - step >= block.start)
                {
                    boundaries.Add(window.in - step);
                }
                if (window.holdEnd == window.out && window.out + step <= speechEnd)
                {
                    boundaries.Add(window.out + step);
                }
            }
            for (double boundary : boundaries)
            {
                keys.RemoveAll([&](const FRichCurveKey& key) { return key.Time == static_cast<float>(boundary); });
                add(boundary);
            }
        }
        keys.Sort([](const FRichCurveKey& a, const FRichCurveKey& b) { return a.Time < b.Time; });
        if ((!original && !controller.AddCurve(id, AACF_DefaultCurve, false)) ||
            !controller.SetCurveKeys(id, keys, false))
        {
            controller.CloseBracket(false);
            error = FString::Printf(TEXT("Could not write wording curve %s."), *pair.Key.ToString());
            return nullptr;
        }
    }
    controller.CloseBracket(false);
    return output;
}



