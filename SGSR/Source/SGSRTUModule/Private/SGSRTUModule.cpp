//============================================================================================================
//
//
//                  Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "SGSRTUModule.h"
#include "SGSRTU.h"
#include "SGSRSettings.h"
#include "LogSGSR.h"

#include "CoreMinimal.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
#include "Misc/ConfigUtilities.h"
#endif

IMPLEMENT_MODULE(FSGSRTUModule, SGSRTUModule)

#define LOCTEXT_NAMESPACE "GSR"

DEFINE_LOG_CATEGORY(LogSGSR);

#if DO_CHECK || DO_GUARD_SLOW || DO_ENSURE
DEFINE_LOG_CATEGORY(LogSGSRAPI);
#endif

void FSGSRTUModule::StartupModule()
{
	FString PluginGshaderdir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("SGSR"))->GetBaseDir(), TEXT("Source/src"));
	AddShaderSourceDirectoryMapping(TEXT("/ThirdParty/SGSR"), PluginGshaderdir);
	FString PluginUshaderdir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("SGSR"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/SGSR"), PluginUshaderdir);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
	UE::ConfigUtilities::ApplyCVarSettingsFromIni(TEXT("/Script/SGSRTUModule.GSRSettings"), *GEngineIni, ECVF_SetByProjectSetting);
#else 
	ApplyCVarSettingsFromIni(TEXT("/Script/SGSRTUModule.GSRSettings"), *GEngineIni, ECVF_SetByProjectSetting);
#endif
	UE_LOG(LogSGSR, Log, TEXT("SGSRTUModule Started"));
}

void FSGSRTUModule::ShutdownModule()
{
	UE_LOG(LogSGSR, Log, TEXT("SGSRTUModule Shutdown"));
}

void FSGSRTUModule::SetTU(TSharedPtr<FSGSRTU, ESPMode::ThreadSafe> Upscaler)
{
	TemporalUpscaler = Upscaler;
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
ISGSRTemporalUpscaler* FSGSRTUModule::GetTU() const
#else
ITemporalUpscaler* FSGSRTUModule::GetTU() const
#endif
{
	return TemporalUpscaler.Get();
}

FSGSRTU* FSGSRTUModule::GetGSRU() const
{
	return TemporalUpscaler.Get();
}

float FSGSRTUModule::GetRfraction(uint32 Mode) const
{
	return TemporalUpscaler->GetRfraction(Mode);
}

#undef LOCTEXT_NAMESPACE