//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "SNPE_UE.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FSNPE_UEModule"

#include <stdlib.h>

void FSNPE_UEModule::StartupModule()
{
}

void FSNPE_UEModule::ShutdownModule()
{
}



#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSNPE_UEModule, SNPE_UE)
