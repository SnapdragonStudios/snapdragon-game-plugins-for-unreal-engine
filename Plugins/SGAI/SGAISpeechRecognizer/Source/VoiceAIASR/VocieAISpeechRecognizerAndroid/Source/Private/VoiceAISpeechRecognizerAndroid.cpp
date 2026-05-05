// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "VoiceAISpeechRecognizerAndroid.h"

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/Paths.h"
#include "Templates/UniquePtr.h"
#include "VoiceAIConstants.h"
#include "Wrappers/WhisperBuilderJava.h"

DEFINE_LOG_CATEGORY(LogVoiceAIASRAndroid);

VoiceAISpeechRecognizerAndroid::~VoiceAISpeechRecognizerAndroid()
{
    ForceStop();
    Reset();
}

static TUniquePtr<FWhisperBuilder> GetWhisperBuilder(const SpeechRecognizerConfigurationSettings &Configuration)
{
    TUniquePtr<FWhisperBuilder> WhisperBuilder = MakeUnique<FWhisperBuilder>();
    ESpeechRecognizerConfigurationSettings key;

    key = ESpeechRecognizerConfigurationSettings::ESRCS_CONTINUOUS;
    if (Configuration.Contains(key))
    {
        WhisperBuilder->EnableContinuousTranscription(FCString::Atoi(*Configuration[key]) != 0);
        UE_LOG(LogVoiceAIASRAndroid, Verbose, TEXT("continuous = %d"), (FCString::Atoi(*Configuration[key]) != 0));
    }

    key = ESpeechRecognizerConfigurationSettings::ESRCS_VADHANGOVER;
    if (Configuration.Contains(key))
    {
        WhisperBuilder->SetVadLenHangover(FCString::Atoi(*Configuration[key]));
        UE_LOG(LogVoiceAIASRAndroid, Verbose, TEXT("vadlenhangover = %d"), (FCString::Atoi(*Configuration[key])));
    }

    key = ESpeechRecognizerConfigurationSettings::ESRCS_LANGUAGE;
    if (Configuration.Contains(key))
    {
        WhisperBuilder->SetLanguageCode(TCHAR_TO_UTF8(*Configuration[key]));
        UE_LOG(LogVoiceAIASRAndroid, Verbose, TEXT("language = %hs"), TCHAR_TO_UTF8(*Configuration[key]));
    }

    key = ESpeechRecognizerConfigurationSettings::ESRCS_PARTIAL;
    if (Configuration.Contains(key))
    {
        WhisperBuilder->EnablePartialTranscriptions(FCString::Atoi(*Configuration[key]) != 0);
        UE_LOG(LogVoiceAIASRAndroid, Verbose, TEXT("partial = %d"), (FCString::Atoi(*Configuration[key]) != 0));
    }

    key = ESpeechRecognizerConfigurationSettings::ESRCS_NOSTARTSPEECHTIMEOUT;
    if (Configuration.Contains(key))
    {
        WhisperBuilder->SetNoStartSpeechTimeout(FCString::Atoi(*Configuration[key]));
        UE_LOG(LogVoiceAIASRAndroid, Verbose, TEXT("nostarttimeout = %d"), (FCString::Atoi(*Configuration[key])));
    }

    key = ESpeechRecognizerConfigurationSettings::ESRCS_VADTHRESHOLD;
    if (Configuration.Contains(key))
    {
        WhisperBuilder->SetVadThreshold((int)(FMath::Clamp(FCString::Atof(*Configuration[key]), 0, 1) * 1048576));
        UE_LOG(LogVoiceAIASRAndroid, Verbose, TEXT("vadthreshold = %d"),
               (int)(FMath::Clamp(FCString::Atof(*Configuration[key]), 0, 1) * 1048576));
    }

    key = ESpeechRecognizerConfigurationSettings::ESRCS_TRANSLATE;
    if (Configuration.Contains(key))
    {
        WhisperBuilder->SetTranslationEnabled(FCString::Atoi(*Configuration[key]) != 0);
        UE_LOG(LogVoiceAIASRAndroid, Verbose, TEXT("translate = %d"), (FCString::Atoi(*Configuration[key]) != 0));
    }

    key = ESpeechRecognizerConfigurationSettings::ESRCS_NONSPEECH;
    if (Configuration.Contains(key))
    {
        WhisperBuilder->SuppressNonSpeech(FCString::Atoi(*Configuration[key]) == 0);
        UE_LOG(LogVoiceAIASRAndroid, Verbose, TEXT("nonspeech = %d"), (FCString::Atoi(*Configuration[key])));
    }

#if 0
    WhisperBuilder->SetAILogLevel(0);
#endif

    return WhisperBuilder;
}

//
//
static FString GetModelsPath(FString OverridePath)
{
    FString DefaultPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*(FPaths::ProjectDir() / "models"));

    FString ModelsPath = OverridePath;
    if (OverridePath.IsEmpty())
    {
        ModelsPath = DefaultPath;
    }
    else
    {
        if (FPaths::IsRelative(OverridePath))
        {
            ModelsPath = FPaths::Combine(DefaultPath, ModelsPath);
        }
    }

    return ModelsPath;
}

//
//
bool VoiceAISpeechRecognizerAndroid::Initialize(const FSpeechRecognizerSettings &Settings)
{
    Reset();

    if (WhisperObj == nullptr)
    {

        WhisperResponseListenerObj = MakeUnique<FWhisperResponseListenerNativeProxy>(this);
        InputStreamObj = MakeUnique<FNativeInputStreamJava>(this);
        AudioBufferQueue = MakeUnique<FAudioBufferQueue>(32000);

        TMap<ESpeechRecognizerConfigurationSettings, FString> initialSettings;
        initialSettings.Add(ESpeechRecognizerConfigurationSettings::ESRCS_LANGUAGE, "en");
        initialSettings.Add(ESpeechRecognizerConfigurationSettings::ESRCS_NOSTARTSPEECHTIMEOUT, "1000");
        initialSettings.Add(ESpeechRecognizerConfigurationSettings::ESRCS_TRANSLATE, "0");
        initialSettings.Add(ESpeechRecognizerConfigurationSettings::ESRCS_VADTHRESHOLD, "0.85");
        initialSettings.Add(ESpeechRecognizerConfigurationSettings::ESRCS_PARTIAL, "0");
        initialSettings.Add(ESpeechRecognizerConfigurationSettings::ESRCS_NONSPEECH, "0");
        initialSettings.Add(ESpeechRecognizerConfigurationSettings::ESRCS_CONTINUOUS, "0");
        initialSettings.Add(ESpeechRecognizerConfigurationSettings::ESRCS_VADHANGOVER, "120");
        for (auto &Entry : Settings.Configuration)
        {
            initialSettings.Add(Entry.Key, Entry.Value);
        }

        TUniquePtr<FWhisperBuilder> WhisperBuilder = GetWhisperBuilder(initialSettings);
        WhisperBuilder->SetListener(WhisperResponseListenerObj.Get());

        WhisperObj = WhisperBuilder->Build();
    }

    bool success = WhisperObj->Init(GetModelsPath(Settings.ModelPath));

    if (success == false)
    {
        UE_LOG(LogVoiceAIASRAndroid, Error, TEXT("Whisper Init Failed"));
        Reset();
    }

#if 0
    WhisperObj->EnableDebug(true);
#endif

    return success;
}

void VoiceAISpeechRecognizerAndroid::Destroy()
{
    Reset();
}

void VoiceAISpeechRecognizerAndroid::Reset()
{
    if (WhisperObj != nullptr)
    {
        WhisperObj->Stop();
        WhisperObj->DeInit();
        WhisperObj.Reset();
    }

    if (WhisperResponseListenerObj != nullptr)
    {
        WhisperResponseListenerObj.Reset(nullptr);
    }

    if (AudioBufferQueue != nullptr)
    {
        AudioBufferQueue->reset();
    }
}

void VoiceAISpeechRecognizerAndroid::OnError(int errorCode)
{
    if (OnErrorCallback)
    {
        OnErrorCallback(errorCode);
    }
}

void VoiceAISpeechRecognizerAndroid::OnRecordingStopped()
{
    if (OnStoppedCallback)
    {
        OnStoppedCallback();
    }
}

void VoiceAISpeechRecognizerAndroid::OnFinished()
{
    if (WhisperObj != nullptr) {
        WhisperObj->Stop();
    }
}

void VoiceAISpeechRecognizerAndroid::OnSpeechStart()
{
    if (OnStartedCallback)
    {
        OnStartedCallback();
    }
}

void VoiceAISpeechRecognizerAndroid::OnSpeechEnd()
{
}

int VoiceAISpeechRecognizerAndroid::Read(uint8 *arr, int offset, int length)
{
    if (AudioBufferQueue != nullptr)
    {
        AudioBufferQueue->read(arr, length);
    }

    return length;
}

//
//
void VoiceAISpeechRecognizerAndroid::Start(const SpeechToTextStartedCallbackFn &OnStartedCallbackFn,
                                           const SpeechToTextStoppedCallbackFn &OnStoppedCallbackFn,
                                           const SpeechToTextErrorCallbackFn &OnErrorCallbackFn,
                                           const SpeechToTextEventCallbackFn &OnTranscriptionCallbackFn)
{
    if (WhisperObj != nullptr)
    {
        ForceStop();
        if (AudioBufferQueue != nullptr)
        {
            AudioBufferQueue->reset();
        }
        this->OnTranscriptionCallback = OnTranscriptionCallbackFn;
        this->OnStartedCallback = OnStartedCallbackFn;
        this->OnStoppedCallback = OnStoppedCallbackFn;
        this->OnErrorCallback = OnErrorCallbackFn;

        WhisperObj->Start(InputStreamObj.Get());
    }
    else
    {
        UE_LOG(LogVoiceAIASRAndroid, Warning, TEXT("VoiceAI::Start called without Connecting. OR without Stopping."));
    }
}

//
//
bool VoiceAISpeechRecognizerAndroid::ProcessAudio(const uint8 *bytes, const uint32 numBytes)
{
    if (AudioBufferQueue != nullptr)
    {
        AudioBufferQueue->ProcessAudio(bytes, numBytes);
    }

    //
    return false;
}

void VoiceAISpeechRecognizerAndroid::ForceStop()
{
    if (WhisperObj != nullptr)
    {
        WhisperObj->Stop();
    }
}

//
//
void VoiceAISpeechRecognizerAndroid::Stop()
{
    // InjectSilence(2.0f);
    //
    ForceStop();
}

//
//
void VoiceAISpeechRecognizerAndroid::OnTranscription(const std::string &transcription, const std::string &language,
                                                     bool finalized, int code)
{
    if (transcription.length() > 0)
    {
        this->OnTranscriptionCallback(finalized, transcription.c_str());
    }
}
