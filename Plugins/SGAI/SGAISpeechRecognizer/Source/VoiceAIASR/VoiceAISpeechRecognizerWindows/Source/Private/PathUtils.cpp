// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "PathUtils.h"
#include "CoreMinimal.h"
#include "Interfaces/IPluginManager.h"
#include "QAIRTPlatformUtils.h"

namespace VoiceAIASR
{
//
//
FString GetBinariesPath()
{
    FString platform_sub_dir;
#if PLATFORM_WINDOWS
#if defined(_M_ARM64)
    platform_sub_dir = "aarch64-windows-msvc";
#else
    platform_sub_dir = UQAIRTPlatformUtils::IsARM64Host() ? "arm64ec-windows-msvc" : "x86_64-windows-msvc";
#endif
#endif
    return FPaths::ConvertRelativePathToFull(
        FPaths::Combine(*(IPluginManager::Get().FindPlugin("SpeechRecognizer")->GetBaseDir()),
                        "Source/VoiceAI/ThirdParty/VoiceAIASRLib/lib", *platform_sub_dir));
}
} // namespace VoiceAIASR
