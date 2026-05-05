// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "WhisperJava.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidJavaEnv.h"
#include <android/log.h>
#include <jni.h>
#include <string>

DECLARE_LOG_CATEGORY_EXTERN(LogVoiceAIASRAndroid, Log, All)

TUniquePtr<FWhisperJava> FWhisperJava::Wrap(jobject JavaObject)
{
    TUniquePtr<FWhisperJava> WhisperJavaObject = MakeUnique<FWhisperJava>();

    JNIEnv *Env = AndroidJavaEnv::GetJavaEnv();
    Env->DeleteGlobalRef(WhisperJavaObject->Object);
    WhisperJavaObject->Object = Env->NewGlobalRef(JavaObject);
    return WhisperJavaObject;
}

FWhisperJava::FWhisperJava()
    : FJavaClassObject("com/qualcomm/qti/voice/assist/whisper/sdk/Whisper", "()V"),
      InitializeMethodId(GetClassMethod("init", "(ILjava/lang/String;Ljava/lang/String;[Ljava/lang/String;)I")),
      DeInitializeMethodId(GetClassMethod("deInit", "()V")),
      StartRecognitionMethodId(GetClassMethod("start", "(Ljava/io/InputStream;)V")),
      StopRecognitionMethodId(GetClassMethod("stop", "()V")),
      GetVersionMethodId(GetClassMethod("getVersion", "()Ljava/lang/String;")),
      EnableDebugMethodId(GetClassMethod("enableDebug", "(Z)V")),
      SetDebugDirMethodId(GetClassMethod("setDebugDir", "(Ljava/io/File;)V"))
{
}

FWhisperJava::~FWhisperJava()
{
}

void FWhisperJava::EnableDebug(bool bFlag)
{
    JNIEnv *Env = AndroidJavaEnv::GetJavaEnv();
    CallMethod<void>(EnableDebugMethodId, bFlag);
    FString debugPath = "/sdcard/Android/data/com.qti.acg.speechsample/files/";
    FScopedJavaObject<jstring> jDebugPath = GetJString(debugPath);

    jclass fileClass = FJavaWrapper::FindClass(Env, "java/io/File", false);

    jmethodID constructorID = FJavaWrapper::FindMethod(Env, fileClass, "<init>", "(Ljava/lang/String;)V", false);

    jobject fileObject = Env->NewObject(fileClass, constructorID, *jDebugPath);

    CallMethod<void>(SetDebugDirMethodId, fileObject);
}

FString FWhisperJava::GetVersion()
{
    return CallMethod<FString>(GetVersionMethodId);
}

int FWhisperJava::Init(const FString &ModelDirPath)
{
    // create a jstring array and pass as the last param

    JNIEnv *Env = AndroidJavaEnv::GetJavaEnv();
    jmethodID GetNativeLibDirMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID,
                                                               "getNativeLibDir", "()Ljava/lang/String;", false);
    FString NativeLibDir = FJavaHelper::FStringFromLocalRef(
        Env, (jstring)FJavaWrapper::CallObjectMethod(Env, FJavaWrapper::GameActivityThis, GetNativeLibDirMethod));

    TArray<FString> StringPaths = {
        ModelDirPath / "vocab.bin",   //.................
        ModelDirPath / "encoder.bin", //.................
        ModelDirPath / "decoder.bin"  //.................
    };
    TArray<FStringView> Paths = {
        StringPaths[0],
        StringPaths[1], //.................
        StringPaths[2]  //.................
    };

    FScopedJavaObject<jstring> jADSPPath = GetJString(NativeLibDir);                       // jni
    FScopedJavaObject<jstring> jDNNModel = GetJString(NativeLibDir / "libnnvad_model.so"); // speech_float.eai

    UE_LOG(LogVoiceAIASRAndroid, Warning, TEXT("NativeLibDir = %s"), *NativeLibDir);

    UE_LOG(LogVoiceAIASRAndroid, Warning, TEXT("ADSPPath     = %s"), *NativeLibDir);
    UE_LOG(LogVoiceAIASRAndroid, Warning, TEXT("Path.vocab   = %s"), *StringPaths[0]);
    UE_LOG(LogVoiceAIASRAndroid, Warning, TEXT("Path.encoder = %s"), *StringPaths[1]);
    UE_LOG(LogVoiceAIASRAndroid, Warning, TEXT("Path.decoder = %s"), *StringPaths[2]);

    FScopedJavaObject<jobjectArray> Arg = FJavaHelper::ToJavaStringArray(Env, Paths);

    int Result = CallMethod<int>(InitializeMethodId, 0, *jADSPPath, *jDNNModel, *Arg);
    UE_LOG(LogVoiceAIASRAndroid, Warning, TEXT("INITIALIZE = %d"), Result);
    return (Result == 0);
}

void FWhisperJava::DeInit()
{
    CallMethod<void>(DeInitializeMethodId);
}

void FWhisperJava::Stop()
{
    CallMethod<void>(StopRecognitionMethodId);
}

void FWhisperJava::Start(FNativeInputStreamJava *InputStreamObj)
{
    CallMethod<void>(StartRecognitionMethodId, InputStreamObj->GetJObject());
}
