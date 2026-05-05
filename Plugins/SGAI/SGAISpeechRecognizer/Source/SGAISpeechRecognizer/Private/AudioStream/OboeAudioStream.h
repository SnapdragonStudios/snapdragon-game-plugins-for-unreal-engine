// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// All rights reserved.

#pragma once

#if PLATFORM_ANDROID

#include "AudioCaptureCore.h"
#include "IAudioStream.h"
#include "oboe/Oboe.h"

namespace SGAISpeechRecognizer
{

class FOboeAudioStream : public IAudioStream, public oboe::AudioStreamCallback
{
  public:
    FOboeAudioStream() = default;

    virtual bool OpenStream() override;
    virtual void CloseStream() override;
    virtual bool StopCapture() override;
    virtual bool StartCapture(const IAudioStream::AudioStreamCallback &func) override;
    virtual void OnAudioCapture(void *InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow);

  private:
    virtual oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32 numFrames);

  private:
    IAudioStream::AudioStreamCallback OnCapture;
    int32 NumChannels = 1;
    int32 SampleRate = 16000;

    TUniquePtr<oboe::AudioStream> InputOboeStream;
};

} // namespace SGAISpeechRecognizer

#endif
