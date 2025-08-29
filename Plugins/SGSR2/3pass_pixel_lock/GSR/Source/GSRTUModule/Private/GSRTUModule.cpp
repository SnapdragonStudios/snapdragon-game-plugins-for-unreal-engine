//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "GSRTUModule.h"
#include "GSRTU.h"
#include "GSRSettings.h"
#include "logGSR.h"

#include "CoreMinimal.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigUtilities.h"

IMPLEMENT_MODULE(FGSRTUModule, GSRTUModule)

#define LOCTEXT_NAMESPACE "GSR"

DEFINE_LOG_CATEGORY(logGSR);

#if DO_CHECK || DO_GUARD_SLOW || DO_ENSURE
DEFINE_LOG_CATEGORY(logGSRAPI);
#endif

void FGSRTUModule::StartupModule()
{
	FString PluginGshaderdir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("GSR"))->GetBaseDir(), TEXT("Source/src"));
	AddShaderSourceDirectoryMapping(TEXT("/ThirdParty/GSR"), PluginGshaderdir);
	FString PluginUshaderdir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("GSR"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/GSR"), PluginUshaderdir);
	UE::ConfigUtilities::ApplyCVarSettingsFromIni(TEXT("/Script/GSRTUModule.GSRSettings"), *GEngineIni, ECVF_SetByProjectSetting);
	UE_LOG(logGSR, Log, TEXT("GSRTUModule Started"));
}

void FGSRTUModule::ShutdownModule()
{
	UE_LOG(logGSR, Log, TEXT("GSRTUModule Shutdown"));
}

void FGSRTUModule::SetTU(TSharedPtr<FGSRTU, ESPMode::ThreadSafe> Upscaler)
{
	TemporalUpscaler = Upscaler;
}

IGSRTemporalUpscaler* FGSRTUModule::GetTU() const
{
	return TemporalUpscaler.Get();
}

FGSRTU* FGSRTUModule::GetGSRU() const
{
	return TemporalUpscaler.Get();
}

float FGSRTUModule::GetRfraction(uint32 Mode) const
{
	return TemporalUpscaler->GetRfraction(Mode);
}

#undef LOCTEXT_NAMESPACE