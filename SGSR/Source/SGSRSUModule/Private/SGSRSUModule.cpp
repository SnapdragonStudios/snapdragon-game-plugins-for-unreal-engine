//============================================================================================================
//
//
//                  Copyright (c) 2023, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SGSRSUModule.h"
#include "LogSGSR.h"

#include "CoreMinimal.h"
#include "Interfaces/IPluginManager.h"

IMPLEMENT_MODULE(FSGSRSUModule, SGSRSUModule);


void FSGSRSUModule::StartupModule()
{
	UE_LOG(LogSGSR, Log, TEXT("SGSRSU Module Started"));
}

void FSGSRSUModule::ShutdownModule()
{
	UE_LOG(LogSGSR, Log, TEXT("SGSRSU Module Shutdown"));
}
