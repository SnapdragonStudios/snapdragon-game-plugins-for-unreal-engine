//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "GSR.h"
#include "GSRViewExtension.h"

#include "Runtime/Launch/Resources/Version.h"

static_assert((ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 2 && ENGINE_PATCH_VERSION >= 0), "GSR plugin requires UE5.2.x");

IMPLEMENT_MODULE(FGSRModule, GSR)

void FGSRModule::StartupModule()
{
	//allow GSR to exist with other upscalers
	GSRViewExtension = FSceneViewExtensions::NewExtension<FGSRViewExtension>();
}

void FGSRModule::ShutdownModule()
{
	//smart pointer to release its memory
	GSRViewExtension = nullptr;
} 