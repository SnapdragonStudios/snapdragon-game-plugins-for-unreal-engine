//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SGSRSettings.h"
#include "LogSGSR.h"
#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1 && ENGINE_PATCH_VERSION >= 1)
#include "Misc/ConfigUtilities.h"
#endif

void HalfPrecisionCVarSetToFalse(IConsoleVariable* const Var)
{
	check(Var);
	if (Var)
	{
		const bool CVarIsBool = Var->IsVariableBool();
		check(CVarIsBool);
		if (CVarIsBool && Var->GetBool())
		{
			const bool False = false;
			UE_LOG(
				LogSGSR,
				Log,
				TEXT("SGSR Half Precision is not supported in any Unreal version less than 5.0; using %i instead"), False);
			Var->Set(False, ECVF_SetByConsole);
		}
	}
}

USGSR_Settings::USGSR_Settings(const FObjectInitializer& obj)
{
    //CVar's that share the same ConsoleVariable string as these Settings have default values if ini-initialization is missing
}
/*virtual*/ void USGSR_Settings::PostInitProperties() /*override final*/
{
    Super::PostInitProperties();

    const TCHAR* const iniSettingsString = TEXT("/Script/SGSRSpatialUpscaling.SGSR_Settings");
#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1 && ENGINE_PATCH_VERSION >= 1)
    UE::ConfigUtilities::
#endif
	ApplyCVarSettingsFromIni(iniSettingsString, *GEngineIni, ECVF_SetByProjectSetting);

#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1 && ENGINE_PATCH_VERSION >= 1)
	UE::ConfigUtilities::
#endif
	ApplyCVarSettingsFromIni(iniSettingsString, *GGameIni, ECVF_SetByProjectSetting);

#if !defined(SGSR_HALF_PRECISION_SUPPORTED)
	HalfPrecisionCVarSetToFalse(IConsoleManager::Get().FindConsoleVariable(TEXT(SGSR_CVAR_NAME_HALF_PRECISION)));
#endif
}

#if WITH_EDITOR
/*virtual*/ void USGSR_Settings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) /*override final*/
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const auto& PropertyThatChanged = PropertyChangedEvent.Property;
    if (PropertyThatChanged)
    {
        const FString& CVarName = PropertyThatChanged->GetMetaData(TEXT("ConsoleVariable"));
        const bool CVarNameNotEmpty = !CVarName.IsEmpty();
        check(CVarNameNotEmpty);
        if (CVarNameNotEmpty)
        {
            if (CVarName.Compare(FString(TEXT(SGSR_CVAR_NAME_HALF_PRECISION))) == 0)
            {
                IConsoleVariable*const CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
                check(CVar);
                if (CVar)
                {
                    FBoolProperty*const BoolProperty = CastField<FBoolProperty>(PropertyThatChanged);
                    check(BoolProperty);
                    if (BoolProperty)
                    {
                        CVar->Set(static_cast<int32>(BoolProperty->GetPropertyValue_InContainer(this)), ECVF_SetByProjectSetting);
                    }
                }
            }
            else if (CVarName.Compare(FString(TEXT(SGSR_CVAR_NAME_TARGET))) == 0)
            {
                IConsoleVariable*const CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
                check(CVar);
                if (CVar)
                {
                    FByteProperty*const ByteProperty = CastField<FByteProperty>(PropertyThatChanged);
                    check(ByteProperty);
                    if (ByteProperty)
                    {
                        CVar->Set(static_cast<int32>(ByteProperty->GetPropertyValue_InContainer(this)), ECVF_SetByProjectSetting);
                    }
                }
            }
            else
            {
                check(false);
            }
        }
    }
}
#endif