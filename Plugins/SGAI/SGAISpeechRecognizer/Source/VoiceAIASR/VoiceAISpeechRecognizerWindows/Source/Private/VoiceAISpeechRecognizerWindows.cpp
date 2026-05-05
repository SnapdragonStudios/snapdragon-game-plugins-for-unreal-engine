// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "VoiceAISpeechRecognizerWindows.h"
#include "CoreMinimal.h"
#include "PathUtils.h"
#include "QAIRTPlatformUtils.h"
#include "SpeechRecognizer.h"
#include "VoiceAIConstants.h"
#include "Whisper.h"

DEFINE_LOG_CATEGORY(LogVoiceAIASRWindows)

VoiceAISpeechRecognizerWindows::~VoiceAISpeechRecognizerWindows()
{
    ForceStop();
    Reset();
}

//
//
void VoiceAISpeechRecognizerWindows::SetConfiguration(const SpeechRecognizerConfigurationSettings &Configuration)
{

    if (WhisperObj != nullptr)
    {
        ESpeechRecognizerConfigurationSettings key;

        key = ESpeechRecognizerConfigurationSettings::ESRCS_CONTINUOUS;
        if (Configuration.Contains(key))
        {
            WhisperObj->setContinuousTranscriptionEnabled(FCString::Atoi(*Configuration[key]) != 0);
            UE_LOG(LogVoiceAIASRWindows, Verbose, TEXT("continuous = %d"), (FCString::Atoi(*Configuration[key]) != 0));
        }

        key = ESpeechRecognizerConfigurationSettings::ESRCS_VADHANGOVER;
        if (Configuration.Contains(key))
        {
            WhisperObj->setVadLenHangover(FCString::Atoi(*Configuration[key]));
            UE_LOG(LogVoiceAIASRWindows, Verbose, TEXT("vadlenhangover = %d"), (FCString::Atoi(*Configuration[key])));
        }

        key = ESpeechRecognizerConfigurationSettings::ESRCS_LANGUAGE;
        if (Configuration.Contains(key))
        {
            WhisperObj->setLanguageCode(TCHAR_TO_UTF8(*Configuration[key]));
            UE_LOG(LogVoiceAIASRWindows, Verbose, TEXT("language = %hs"), TCHAR_TO_UTF8(*Configuration[key]));
        }

        key = ESpeechRecognizerConfigurationSettings::ESRCS_PARTIAL;
        if (Configuration.Contains(key))
        {
            WhisperObj->setPartialTranscriptionsEnabled(FCString::Atoi(*Configuration[key]) != 0);
            UE_LOG(LogVoiceAIASRWindows, Verbose, TEXT("partial = %d"), (FCString::Atoi(*Configuration[key]) != 0));
        }

        key = ESpeechRecognizerConfigurationSettings::ESRCS_NOSTARTSPEECHTIMEOUT;
        if (Configuration.Contains(key))
        {
            WhisperObj->setNoStartSpeechTimeout(FCString::Atoi(*Configuration[key]));
            UE_LOG(LogVoiceAIASRWindows, Verbose, TEXT("nostarttimeout = %d"), (FCString::Atoi(*Configuration[key])));
        }

        key = ESpeechRecognizerConfigurationSettings::ESRCS_VADTHRESHOLD;
        if (Configuration.Contains(key))
        {
            WhisperObj->setVadThreshold((int)(FMath::Clamp(FCString::Atof(*Configuration[key]), 0, 1) * 1048576));
            UE_LOG(LogVoiceAIASRWindows, Verbose, TEXT("vadthreshold = %d"),
                   (int)(FMath::Clamp(FCString::Atof(*Configuration[key]), 0, 1) * 1048576));
        }

        key = ESpeechRecognizerConfigurationSettings::ESRCS_TRANSLATE;
        if (Configuration.Contains(key))
        {
            WhisperObj->setTranslationEnabled(FCString::Atoi(*Configuration[key]) != 0);
            UE_LOG(LogVoiceAIASRWindows, Verbose, TEXT("translate = %d"), (FCString::Atoi(*Configuration[key]) != 0));
        }

        key = ESpeechRecognizerConfigurationSettings::ESRCS_NONSPEECH;
        if (Configuration.Contains(key))
        {
            WhisperObj->setSuppressNonSpeech(FCString::Atoi(*Configuration[key]) == 0);
            UE_LOG(LogVoiceAIASRWindows, Verbose, TEXT("nonspeech = %d"), (FCString::Atoi(*Configuration[key])));
        }
    }
}

//
//
static FString GetModelsPath(FString OverridePath)
{
    FString DefaultPath = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(*(IPluginManager::Get().FindPlugin("SpeechRecognizer")->GetBaseDir()),
                            "Source/VoiceAI/ThirdParty/VoiceAIASRLib/models"));

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
bool VoiceAISpeechRecognizerWindows::Initialize(const FSpeechRecognizerSettings &Settings)
{
    if (WhisperObj == nullptr)
    {
        WhisperObj = new WhisperFunction::Whisper();
        if (WhisperObj == nullptr)
        {
            UE_LOG(LogVoiceAIASRWindows, Error, TEXT("Whisper Init Failed - WhisperObj failed to be created"));
            Reset();
            return false;
        }

        TMap<ESpeechRecognizerConfigurationSettings, FString> initialSettings;
        initialSettings.Add(ESpeechRecognizerConfigurationSettings::ESRCS_LANGUAGE, "en");
        initialSettings.Add(ESpeechRecognizerConfigurationSettings::ESRCS_NOSTARTSPEECHTIMEOUT, "1000");
        initialSettings.Add(ESpeechRecognizerConfigurationSettings::ESRCS_TRANSLATE, "0");
        initialSettings.Add(ESpeechRecognizerConfigurationSettings::ESRCS_VADTHRESHOLD, "0.85");
        initialSettings.Add(ESpeechRecognizerConfigurationSettings::ESRCS_PARTIAL, "0");
        initialSettings.Add(ESpeechRecognizerConfigurationSettings::ESRCS_NONSPEECH, "0");
        initialSettings.Add(ESpeechRecognizerConfigurationSettings::ESRCS_CONTINUOUS, "1");
        initialSettings.Add(ESpeechRecognizerConfigurationSettings::ESRCS_VADHANGOVER, "120");

        for (auto &Entry : Settings.Configuration)
        {
            initialSettings.Add(Entry.Key, Entry.Value);
        }
        SetConfiguration(initialSettings);

        WhisperListener = new FWhisperResponseListenerImpl(this);
        WhisperObj->registerListener(WhisperListener);

        FString ModelsPath = GetModelsPath(Settings.ModelPath);

        ////pass paths
        std::map<std::string, std::string> input_map;
        input_map[VoiceAIConstants::KEY__PATH_ADSP] = TCHAR_TO_UTF8(*UQAIRTPlatformUtils::GetBinariesPath());
        input_map[VoiceAIConstants::KEY__PATH_VAD_MODEL] =
            TCHAR_TO_UTF8(*FPaths::Combine(ModelsPath, VoiceAIConstants::SPEECH));
        input_map[VoiceAIConstants::KEY__MODEL_FILE_ENCODER] =
            TCHAR_TO_UTF8(*FPaths::Combine(ModelsPath, VoiceAIConstants::ENCODER_FILE));
        input_map[VoiceAIConstants::KEY__MODEL_FILE_DECODER] =
            TCHAR_TO_UTF8(*FPaths::Combine(ModelsPath, VoiceAIConstants::DECODER_FILE));
        input_map[VoiceAIConstants::KEY__VOCAB_FILE] =
            TCHAR_TO_UTF8(*FPaths::Combine(ModelsPath, VoiceAIConstants::VOCAB));

        for (const auto &pair : input_map)
        {
            UE_LOG(LogVoiceAIASRWindows, Verbose, TEXT("[%d] - [(%hs), (%hs)]"), FPlatformTLS::GetCurrentThreadId(),
                   pair.first.c_str(), pair.second.c_str());
        }

        bool success = WhisperObj->init(input_map);
        if (success == false)
        {
            UE_LOG(LogVoiceAIASRWindows, Error, TEXT("Whisper Init Failed"));
            Reset();
        }
        return success;
    }

    return false;
}

//
//
void VoiceAISpeechRecognizerWindows::Destroy()
{
    Reset();
}

void VoiceAISpeechRecognizerWindows::Reset()
{
    if (WhisperObj != nullptr)
    {
        WhisperObj->stop();
        WhisperObj->deInit();
        delete WhisperObj;
        WhisperObj = nullptr;
    }

    if (WhisperListener != nullptr)
    {
        delete WhisperListener;
        WhisperListener = nullptr;
    }
}

//
//
void VoiceAISpeechRecognizerWindows::Start(const SpeechToTextStartedCallbackFn &OnStarted,
                                           const SpeechToTextStoppedCallbackFn &OnStopped,
                                           const SpeechToTextErrorCallbackFn &OnError,
                                           const SpeechToTextEventCallbackFn &OnTranscription)
{
    if ((WhisperObj != nullptr) && (InputStream == nullptr))
    {
        ForceStop(); // TODO: is this reuqired.
        this->OnEventCallback = OnTranscription;
        this->OnStartedCallback = OnStarted;
        this->OnStoppedCallback = OnStopped;
        this->OnErrorCallback = OnError;

        InputStream = new WhisperFunction::InputStream();
        WhisperObj->start(this->InputStream);
    }
}

//
//
bool VoiceAISpeechRecognizerWindows::ProcessAudio(const uint8 *bytes, const uint32 numBytes)
{
    if (InputStream != nullptr)
    {
        InputStream->write((uint8 *)(bytes), 0, numBytes);
        return true;
    }
    return false;
}

void VoiceAISpeechRecognizerWindows::InjectSilence(float TimeInSeconds)
{
    // wait for stopped event for a sec only if started event has no closing stopped event. / error event
    if (bHasStarted == true)
    {
        TArray<uint8> Silence;
        Silence.SetNumZeroed(2560);
        float startTime = FPlatformTime::Seconds();

        for (int i = 0; i < 8; i++)
        {
            if (bHasStarted)
            {
                ProcessAudio(Silence.GetData(), Silence.Num());
                FPlatformProcess::Sleep(0.04f);
            }
        }

        if (bHasStarted)
        {
            while (true)
            {
                if (!bHasStarted)
                {
                    break;
                }
                float deltaTime = (FPlatformTime::Seconds() - startTime);
                FPlatformProcess::Sleep(0.125f);
                if ((deltaTime > TimeInSeconds) || !bHasStarted)
                {
                    // trigger error
                    break;
                }
            }
        }
    }
}

void VoiceAISpeechRecognizerWindows::ForceStop()
{
    bHasStarted = false;

    if (WhisperObj != nullptr)
    {
        WhisperObj->stop();
    }

    if (InputStream != nullptr)
    {
        delete InputStream;
        InputStream = nullptr;
    }
}

//
//
void VoiceAISpeechRecognizerWindows::Stop()
{
    InjectSilence(2.0f);

    ForceStop();
}

//
//
void VoiceAISpeechRecognizerWindows::OnTranscription(const std::map<std::string, std::string> &Results) const
{
    for (const auto &pair : Results)
    {
        UE_LOG(LogVoiceAIASRWindows, Verbose, TEXT("result[%hs] = %hs"), pair.first.c_str(), pair.second.c_str());
    }
    bool bIsFinal = false;
    if (Results.contains(VoiceAIConstants::KEY__IS_FINAL))
    {
        bIsFinal = (Results.at(VoiceAIConstants::KEY__IS_FINAL).compare(VoiceAIConstants::ONE) == 0);
    }

    FString Text = "";
    if (Results.contains(VoiceAIConstants::KEY__TRANSCRIPTION))
    {
        Text = FString(UTF8_TO_TCHAR(Results.at(VoiceAIConstants::KEY__TRANSCRIPTION).c_str()));
    }

    if (Text.Len() > 0)
    {
        this->OnEventCallback(bIsFinal, Text);
    }
}

//
//
void VoiceAISpeechRecognizerWindows::OnEvent(const int Event)
{
    UE_LOG(LogVoiceAIASRWindows, Verbose, TEXT("Event = %d"), Event);
    if (Event == VoiceAIConstants::EVENT_SPEECH_STARTED)
    {
        bHasStarted = true;
        this->OnStartedCallback();
    }
    else if (Event == VoiceAIConstants::EVENT_SPEECH_STOPPED)
    {
        bHasStarted = false;
        this->OnStoppedCallback();
    }
    else
    {
        bHasStarted = false;
    }
}

//
//
void VoiceAISpeechRecognizerWindows::OnError(const int Error)
{
    bHasStarted = false;
    this->OnErrorCallback(Error);
}

//
//
VoiceAISpeechRecognizerWindows::FWhisperResponseListenerImpl::FWhisperResponseListenerImpl(
    VoiceAISpeechRecognizerWindows *VoiceAI)
    : Parent(VoiceAI)
{
}

//
//
void VoiceAISpeechRecognizerWindows::FWhisperResponseListenerImpl::onTranscription(
    const std::map<std::string, std::string> &Results)
{
    if (Parent != nullptr)
    {
        Parent->OnTranscription(Results);
    }
}

//
//
void VoiceAISpeechRecognizerWindows::FWhisperResponseListenerImpl::onEvent(int Event)
{
    if (Parent != nullptr)
    {
        Parent->OnEvent(Event);
    }
}

//
//
void VoiceAISpeechRecognizerWindows::FWhisperResponseListenerImpl::onError(int Error)
{
    if (Parent != nullptr)
    {
        Parent->OnError(Error);
    }
}
