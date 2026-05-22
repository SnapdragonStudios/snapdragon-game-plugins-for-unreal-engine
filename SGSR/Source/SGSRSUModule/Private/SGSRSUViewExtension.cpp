//============================================================================================================
//
//
//                  Copyright (c) 2023, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SGSRSUViewExtension.h"
#include "SGSRSU.h"

static bool IsSGSRTheLastPass()
{
	return true;
}

void FSGSRSUViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	static const auto CVarEnableSGSR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SGSR.Enabled"));
	if (CVarEnableSGSR->GetValueOnAnyThread() > 0)
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			static auto CVarEnableEditorScreenPercentageOverride = IConsoleManager::Get().FindConsoleVariable(TEXT("Editor.OverrideDPIBasedEditorViewportScaling"));
			if (CVarEnableEditorScreenPercentageOverride)
            {
                CVarEnableEditorScreenPercentageOverride->Set(1);
            }

		}
#endif

		TArray<TSharedPtr<FSGSRData>> ViewData;

		bool IsTemporalUpscalingRequested = false;
		for (int i = 0; i < InViewFamily.Views.Num(); i++)
		{
			const FSceneView* InView = InViewFamily.Views[i];
			if (ensure(InView))
			{
				IsTemporalUpscalingRequested |= (InView->PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale);

				FSGSRData* Data = new FSGSRData();
				ViewData.Add(TSharedPtr<FSGSRData>(Data));
			}
		}

		if (!IsTemporalUpscalingRequested)
		{
			InViewFamily.SetPrimarySpatialUpscalerInterface(new FSGSRSU(ESGSRMode::UpscalingOnly, ViewData));
			if (!IsSGSRTheLastPass()){
				InViewFamily.SetSecondarySpatialUpscalerInterface(new FSGSRSU(ESGSRMode::PostProcessingOnly, ViewData));
			}
		}
	}
}
