//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "NNERuntimeSNPEModule.h"

#include "Modules/ModuleManager.h"

#include "CoreMinimal.h"
#include "NNE.h"

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"


THIRD_PARTY_INCLUDES_START

#include "DlSystem/DlEnums.hpp"

#include "SNPE/SNPEFactory.hpp"

THIRD_PARTY_INCLUDES_END

#if PLATFORM_ANDROID

#if USE_ANDROID_JNI
#include "Android/AndroidJavaEnv.h"
#endif // USE_ANDROID_JNI


static void SetupEnvironmentForSNPE();
static FString GetAndroidNativeLibraryDir();

#endif // PLATFORM_ANDROID

void FNNERuntimeSNPEModule::StartupModule()
{
#if PLATFORM_ANDROID
	SetupEnvironmentForSNPE();
#endif // PLATFORM_ANDROID

	{// load library
		FString BaseDir = IPluginManager::Get().FindPlugin("NNERuntimeSNPE")->GetBaseDir();
		// Add on the relative location of the third party dll and load it
		FString LibraryPath;
		#if PLATFORM_WINDOWS
			#if defined (_M_ARM64)
				LibraryPath = FPaths::Combine(*BaseDir, TEXT("/Binaries/ThirdParty/SNPE/Arm64/SNPE.dll"));
			#else 
				LibraryPath = FPaths::Combine(*BaseDir, TEXT("/Binaries/ThirdParty/SNPE/x64/SNPE.dll"));
			#endif
			LibraryHandle = !LibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*LibraryPath) : nullptr;
			if (LibraryHandle == nullptr)
			{
				UE_LOG(LogNNE, Error, TEXT("Failed to load SNPE third party library"));
			}
		#endif
	}

	zdl::SNPE::SNPEFactory::initializeLogging(zdl::DlSystem::LogLevel_t::LOG_INFO);

	// Create Instance & Register
	NNERuntimeSNPE = NewObject<UNNERuntimeSNPE>();
	if (NNERuntimeSNPE.IsValid())
	{
		NNERuntimeSNPE->AddToRoot();
		UE::NNE::RegisterRuntime(NNERuntimeSNPE.Get());
	}
}

void FNNERuntimeSNPEModule::ShutdownModule()
{
	if (NNERuntimeSNPE.IsValid())
	{
		UE::NNE::UnregisterRuntime(NNERuntimeSNPE.Get());
		NNERuntimeSNPE->RemoveFromRoot();
		NNERuntimeSNPE.Reset();
	}

	// Free the dll handle
	if(LibraryHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(LibraryHandle);
		LibraryHandle = nullptr;
	}

}

#if PLATFORM_ANDROID

void SetupEnvironmentForSNPE()
{
	// ** set ADSP_LIBRARY_PATH according to SNPE docs **

	const FString EnvVariableName = "ADSP_LIBRARY_PATH";

	FString NativeLibraryPath = GetAndroidNativeLibraryDir();
	FString ADSPLibraryPaths = NativeLibraryPath + ";/system/lib/rfsa/adsp;/system/vendor/lib/rfsa/adsp;/dsp";

	UE_LOG(
		LogAndroid, Log,
		TEXT("SNPE: setting %s to %s"),
		*EnvVariableName, *ADSPLibraryPaths
	);
	if (setenv(TCHAR_TO_UTF8(*EnvVariableName), TCHAR_TO_UTF8(*ADSPLibraryPaths), 1) != 0)
	{
		UE_LOG(
			LogAndroid, Warning,
			TEXT("SNPE: Failed to set %s - DSP execution will NOT be supported!"),
			*EnvVariableName
		);
	}
	else
	{
		UE_LOG(
			LogAndroid, Log,
			TEXT("SNPE: set %s successfully, getenv() returns: %s"),
			*EnvVariableName, ANSI_TO_TCHAR(getenv(TCHAR_TO_UTF8(*EnvVariableName)))
		);
	}
}

FString GetAndroidNativeLibraryDir()
{
	FString Result = "";

#if USE_ANDROID_JNI
	// ** use the JNI to execute: `GameActivity.this.getApplication().getApplicationInfo().nativeLibraryDir` **

	JNIEnv* JEnv = AndroidJavaEnv::GetJavaEnv();
	check(JEnv != nullptr);

	jobject GameActivity = AndroidJavaEnv::GetGameActivityThis();
	jclass GameActivityClass = JEnv->GetObjectClass(GameActivity);
	check(GameActivityClass != nullptr);

	jmethodID GetApplicationMethod = JEnv->GetMethodID(GameActivityClass, "getApplication", "()Landroid/app/Application;");
	jobject Application = JEnv->CallObjectMethod(GameActivity, GetApplicationMethod);
	check(Application != nullptr);

	jclass ApplicationClass = JEnv->GetObjectClass(Application);
	check(ApplicationClass != nullptr);

	jmethodID GetApplicationInfoMethod = JEnv->GetMethodID(ApplicationClass, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
	jobject ApplicationInfo = JEnv->CallObjectMethod(Application, GetApplicationInfoMethod);
	check(ApplicationInfo != nullptr);

	jclass ApplicationInfoClass = JEnv->GetObjectClass(ApplicationInfo);
	check(ApplicationInfoClass != nullptr);

	jfieldID NativeLibraryDir = JEnv->GetFieldID(ApplicationInfoClass, "nativeLibraryDir", "Ljava/lang/String;");
	auto JavaString = (jstring)JEnv->GetObjectField(ApplicationInfo, NativeLibraryDir);
	const auto Chars = JEnv->GetStringUTFChars(JavaString, 0);
	Result = FString(UTF8_TO_TCHAR(Chars));

#endif // USE_ANDROID_JNI

	return Result;
}

#endif // PLATFORM_ANDROID



IMPLEMENT_MODULE(FNNERuntimeSNPEModule, NNERuntimeSNPE)
