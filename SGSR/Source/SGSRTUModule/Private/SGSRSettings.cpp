//============================================================================================================
//
//
//                  Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "SGSRSettings.h"
#define LOCTEXT_NAMESPACE "FSGSRModule"

USGSRSettings::USGSRSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FName USGSRSettings::GetContainerName() const
{
	static const FName ContainerName("Project");
	return ContainerName;
}

FName USGSRSettings::GetCategoryName() const
{
	static const FName EditorCategoryName("Plugins");
	return EditorCategoryName;
}

FName USGSRSettings::GetSectionName() const
{
	static const FName EditorSectionName("SGSR");
	return EditorSectionName;
}

void USGSRSettings::PostInitProperties()
{
	Super::PostInitProperties(); 

#if WITH_EDITOR
	if(IsTemplate())
	{
		ImportConsoleVariableValues();
	}
#endif
}

#if WITH_EDITOR
void USGSRSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(PropertyChangedEvent.Property)
	{
		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);
	}
}
#endif

#undef LOCTEXT_NAMESPACE