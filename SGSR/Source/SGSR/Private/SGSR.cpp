//============================================================================================================
//
//
//                  Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "SGSR.h"
#include "SGSRTUViewExtension.h"
#include "SGSRSUViewExtension.h"

#include "Runtime/Launch/Resources/Version.h"

static_assert((ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION <= 8 && ENGINE_MINOR_VERSION >= 0)), "SGSR plugin only supports UE5.0 - UE5.8");

IMPLEMENT_MODULE(FSGSRModule, SGSR)

void FSGSRModule::StartupModule()
{
	//allow GSR to exist with other upscalers
	SGSRTUViewExtension = FSceneViewExtensions::NewExtension<FSGSRTUViewExtension>();
	SGSRSUViewExtension = FSceneViewExtensions::NewExtension<FSGSRSUViewExtension>();
}

void FSGSRModule::ShutdownModule()
{
	//smart pointer to release its memory
	SGSRTUViewExtension = nullptr;
	SGSRSUViewExtension = nullptr;
} 
