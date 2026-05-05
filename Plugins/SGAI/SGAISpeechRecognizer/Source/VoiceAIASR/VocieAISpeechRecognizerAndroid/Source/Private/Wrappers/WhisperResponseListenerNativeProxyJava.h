// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "Android/AndroidJNI.h"
#include "Android/AndroidJava.h"
#include "Android/AndroidJavaEnv.h"
#include <string>
//
//
class IWhisperResponseListener
{
  public:
    virtual void OnTranscription(const std::string &transcription, const std::string &language, bool finalized,
                                 int code) = 0;
    virtual void OnError(int errorCode) = 0;
    virtual void OnRecordingStopped() = 0;
    virtual void OnSpeechStart() = 0;
    virtual void OnSpeechEnd() = 0;
    virtual void OnFinished() = 0;
};

//
//
class FWhisperResponseListenerNativeProxy : public FJavaClassObject
{
  public:
    FWhisperResponseListenerNativeProxy(IWhisperResponseListener *);
    virtual ~FWhisperResponseListenerNativeProxy();
};
