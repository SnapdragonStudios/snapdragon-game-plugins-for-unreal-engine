//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SGSR.h"
#include "SGSRViewExtension.h"
#include "SGSRSettings.h"

#include "CoreMinimal.h"
#include "ISettingsModule.h"
#include "Runtime/Launch/Resources/Version.h"


// DUPLICATED FROM UNREALENGINE.CPP
/** Special function that loads an engine texture and adds it to root set in cooked builds.
  * This is to prevent the textures from being added to GC clusters (root set objects are not clustered)
	* because otherwise since they're always going to be referenced by UUnrealEngine object which will
	* prevent the clusters from being GC'd. */
	template <typename TextureType>
static void LoadEngineTexture(TextureType*& InOutTexture, const TCHAR* InName)
{
	if (!InOutTexture)
	{
		InOutTexture = LoadObject<TextureType>(nullptr, InName, nullptr, LOAD_None, nullptr);
	}
	if (FPlatformProperties::RequiresCookedData() && InOutTexture)
	{
		InOutTexture->AddToRoot();
	}
}
// END OF DUPLICATED FROM UNREALENGINE.CPP

#define SGSR_PROJECT_CSTRING ("Project")
#define SGSR_PLUGINS_CSTRING ("Plugins")

#define LOCTEXT_NAMESPACE "SGSR"
void FSGSRModule::StartupModule()
{
	SGSRViewExtension = FSceneViewExtensions::NewExtension<FSGSRViewExtension>();

    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        SettingsModule->RegisterSettings(
            SGSR_PROJECT_CSTRING,
            SGSR_PLUGINS_CSTRING,
            "Snapdragon GSR Settings",
            LOCTEXT("RuntimeSettingsName", "Snapdragon GSR Settings"),
            LOCTEXT("RuntimeSettingsDescription", "Configure which shader variants to compile and execute"),
            GetMutableDefault<USGSR_Settings>());
    }
}

void FSGSRModule::ShutdownModule()
{
    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        SettingsModule->UnregisterSettings(SGSR_PROJECT_CSTRING, SGSR_PLUGINS_CSTRING, "Snapdragon GSR Settings");
    }

	SGSRViewExtension = nullptr;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSGSRModule, SGSR)