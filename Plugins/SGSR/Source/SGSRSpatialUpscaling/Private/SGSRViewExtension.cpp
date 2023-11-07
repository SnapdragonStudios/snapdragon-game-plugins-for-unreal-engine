//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SGSRViewExtension.h"
#include "SGSRSpatialUpscaler.h"

static TAutoConsoleVariable<int32> CVarEnableSGSR(
	TEXT("r.Qualcomm.SGSR.Enabled"),
	1,
	TEXT("Enable SGSR for Primary Upscale"),
	ECVF_RenderThreadSafe);

static bool IsSGSRTheLastPass()
{
	return true;
}

void FSGSRViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (CVarEnableSGSR.GetValueOnAnyThread() > 0)
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
			InViewFamily.SetPrimarySpatialUpscalerInterface(new FSGSRSpatialUpscaler(ESGSRMode::UpscalingOnly, ViewData));
			if (!IsSGSRTheLastPass()){
				InViewFamily.SetSecondarySpatialUpscalerInterface(new FSGSRSpatialUpscaler(ESGSRMode::PostProcessingOnly, ViewData));
			}
		}
		else
		{
			InViewFamily.SetSecondarySpatialUpscalerInterface(new FSGSRSpatialUpscaler(ESGSRMode::Combined, ViewData));
		}
	}
}

void FSGSRViewExtension::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
}