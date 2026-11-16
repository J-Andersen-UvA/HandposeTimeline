#include "Wording/WordingService.h"
#include "Widgets/SHandposeTimelineWidget.h"

#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Components/AudioComponent.h"
#include "Editor.h"
#include "Playback/HandposeTimelinePlaybackController.h"
#include "Sound/SoundWaveProcedural.h"
#include "Widgets/Text/STextBlock.h"

bool FWordingAudio::renderPreview(double speechDuration, double elapsed, double playRate, TArray<int16>& pcm) const
{
    pcm.Reset();
    if (sampleRate <= 0 || samples.IsEmpty() || !FMath::IsFinite(speechDuration) ||
        !FMath::IsFinite(elapsed) || !FMath::IsFinite(playRate) ||
        speechDuration <= 0 || elapsed < 0 || elapsed >= speechDuration || playRate <= 0)
    {
        return false;
    }

    const double count = (speechDuration - elapsed) * sampleRate / playRate;
    if (!FMath::IsFinite(count) || count >= MAX_int32 / sizeof(int16))
    {
        return false;
    }

    const int32 frames = FMath::CeilToInt(count);
    if (frames <= 0)
    {
        return false;
    }

    const double step = playRate * samples.Num() / (speechDuration * sampleRate);
    const double first = elapsed / speechDuration * samples.Num();
    pcm.SetNumUninitialized(frames);
    for (int32 index = 0; index < frames; ++index)
    {
        const double position = FMath::Min(first + index * step, static_cast<double>(samples.Num() - 1));
        const int32 left = FMath::FloorToInt(position);
        const int32 right = FMath::Min(left + 1, samples.Num() - 1);
        const double sample = FMath::Lerp(samples[left], samples[right], position - left);
        pcm[index] = static_cast<int16>(FMath::RoundToInt(FMath::Clamp(sample, -1.0, 1.0) * 32767.0));
    }
    return true;
}

void SHandposeTimelineWidget::stopWordingAudio()
{
    if (wordingAudioComponent.IsValid())
    {
        wordingAudioComponent->Stop();
        wordingAudioComponent.Reset();
    }
    wordingAudioBlockIndex = INDEX_NONE;
}

void SHandposeTimelineWidget::updateWordingAudio()
{
    if (!wordingEnabled || wordingBlocks.IsEmpty() || !playbackController->isPlaying())
    {
        stopWordingAudio();
        return;
    }

    const double current = playbackController->getCurrentTimeSeconds();
    const double rate = playbackController->getEffectivePlayRate();
    const uint64 revision = playbackController->getPlaybackRevision();
    const int32 index = wordingBlocks.IndexOfByPredicate([current](const FWordingBlock& block)
    {
        return block.audio.IsValid() && current >= block.start && current < block.start + block.duration;
    });
    if (index == wordingAudioBlockIndex &&
        (index == INDEX_NONE || (revision == wordingAudioPlaybackRevision && rate == wordingAudioPlayRate)))
    {
        return;
    }

    stopWordingAudio();
    if (index == INDEX_NONE || !FMath::IsFinite(rate) || rate <= 0)
    {
        return;
    }

    wordingAudioBlockIndex = index;
    wordingAudioPlaybackRevision = revision;
    wordingAudioPlayRate = rate;

    auto* manager = GEngine ? GEngine->GetAudioDeviceManager() : nullptr;
    const FAudioDeviceHandle device = manager ? manager->GetActiveAudioDevice() : FAudioDeviceHandle{};
    if (!GEditor || !GEditor->CanPlayEditorSound() || !device.IsValid())
    {
        statusText->SetText(FText::FromString(TEXT("Wording preview needs editor sounds enabled and an active audio device.")));
        return;
    }

    const auto& block = wordingBlocks[index];
    TArray<int16> pcm;
    if (!block.audio->renderPreview(block.duration, current - block.start, rate, pcm))
    {
        statusText->SetText(FText::FromString(TEXT("Could not prepare the retimed wording audio.")));
        return;
    }

    auto* wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
    wave->SetSampleRate(block.audio->sampleRate);
    wave->SetNumFrames(pcm.Num());
    wave->NumChannels = 1;
    wave->Duration = static_cast<float>(pcm.Num()) / block.audio->sampleRate;
    wave->QueueAudio(reinterpret_cast<const uint8*>(pcm.GetData()), pcm.Num() * sizeof(int16));

    FAudioDevice::FCreateComponentParams params(device.GetAudioDevice());
    UAudioComponent* component = FAudioDevice::CreateComponent(wave, params);
    if (!component)
    {
        statusText->SetText(FText::FromString(TEXT("Could not start wording audio preview.")));
        return;
    }
    component->bIsPreviewSound = true;
    component->bIsUISound = true;
    component->bAllowSpatialization = false;
    component->bReverb = false;
    wordingAudioComponent.Reset(component);
    component->Play();
}
