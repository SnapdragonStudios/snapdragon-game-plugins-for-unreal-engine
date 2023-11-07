//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SGSRSpatialUpscaling.h"
#include "LogSGSR.h"

#include "CoreMinimal.h"
#include "Interfaces/IPluginManager.h"

IMPLEMENT_MODULE(FSGSRSpatialUpscalingModule, SGSRSpatialUpscaling)

DEFINE_LOG_CATEGORY(LogSGSR);

void FSGSRSpatialUpscalingModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("SGSR"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/SGSR"), PluginShaderDir);

	UE_LOG(LogSGSR, Log, TEXT("SGSR Spatial Upscaling Module Started"));
}

void FSGSRSpatialUpscalingModule::ShutdownModule()
{
	UE_LOG(LogSGSR, Log, TEXT("SGSR Spatial Upscaling Module Shutdown"));
}
