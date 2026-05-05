// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "CoreMinimal.h"
#include "SpeechRecognizerConfigurationSettings.generated.h"

/**
 * Enum for Configuration Settings
 */
UENUM(BlueprintType)
enum class ESpeechRecognizerConfigurationSettings : uint8
{
    ESRCS_LANGUAGE UMETA(DisplayName = "language"),
    ESRCS_NOSTARTSPEECHTIMEOUT UMETA(DisplayName = "nostartspeechtimeout"),
    ESRCS_TRANSLATE UMETA(DisplayName = "translate"),
    ESRCS_VADTHRESHOLD UMETA(DisplayName = "vadthreshold"),
    ESRCS_PARTIAL UMETA(DisplayName = "partial"),
    ESRCS_NONSPEECH UMETA(DisplayName = "nonspeech"),
    ESRCS_CONTINUOUS UMETA(DisplayName = "continuous"),
    ESRCS_VADHANGOVER UMETA(DisplayName = "vadhangover")
};

USTRUCT(BlueprintType)
struct SGAISPEECHRECOGNIZER_API FSpeechRecognizerSettings
{
    GENERATED_BODY()

    //
    //
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpeechRecognizer")
    FString ModelPath;

    //
    //
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpeechRecognizer")
    TMap<ESpeechRecognizerConfigurationSettings, FString> Configuration;
};
