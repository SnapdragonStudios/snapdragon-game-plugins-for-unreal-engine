//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "SNPE.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "SNPEAndroidObject.h"

#define LOCTEXT_NAMESPACE "FSNPEModule"

#include <stdlib.h>

void FSNPEModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
#if PLATFORM_ANDROID
	FString NativeLibraryPath = FSNPEAndroidObject::GetNativeLibraryDir();
	FString CDSP_Variable_Name = "ADSP_LIBRARY_PATH";
	FString CDSP_value = NativeLibraryPath + ";/system/lib/rfsa/adsp;/system/vendor/lib/rfsa/adsp;/dsp";
	UE_LOG(LogAndroid, Log, TEXT("SNPE: SETTING ADSP_LIBRARY_PATH"));
	if (setenv(TCHAR_TO_UTF8(*CDSP_Variable_Name), TCHAR_TO_UTF8(*CDSP_value), 1) != 0)
	{
		UE_LOG(LogAndroid, Warning, TEXT("SNPE: FAILED TO SET ADSP_LIBRARY_PATH ENVIRONMENT VARIABLE FOR DSP, AIP EXECUTION"));
	}
	
#endif
}

void FSNPEModule::ShutdownModule()
{
}



#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSNPEModule, SNPE)
