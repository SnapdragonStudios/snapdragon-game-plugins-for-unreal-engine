//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SGSRSpatialUpscaler.h"

#include "Subpasses/SGSRSubpassFirst.h"
#include "Subpasses/SGSRSubpassScaler.h"
#include "Subpasses/SGSRSubpassLast.h"

#define EXECUTE_STEP(step) \
	for (FSGSRSubpass* Subpass : SGSRSubpasses) \
	{ \
		Subpass->step(GraphBuilder, View, PassInputs); \
	}

DECLARE_GPU_STAT(SnapdragonSuperResolutionPass)

FSGSRSpatialUpscaler::FSGSRSpatialUpscaler(ESGSRMode InMode, TArray<TSharedPtr<FSGSRData>> InViewData)
	: Mode(InMode)
	, ViewData(InViewData)
{
	if (Mode != ESGSRMode::None)
	{
		RegisterSubpass<FSGSRSubpassFirst>();
		RegisterSubpass<FSGSRSubpassScaler>();
		RegisterSubpass<FSGSRSubpassLast>();
	}
}

ISpatialUpscaler* FSGSRSpatialUpscaler::Fork_GameThread(const class FSceneViewFamily& ViewFamily) const
{
	return new FSGSRSpatialUpscaler(Mode, ViewData);
}

FScreenPassTexture FSGSRSpatialUpscaler::AddPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) const
{
	RDG_GPU_STAT_SCOPE(GraphBuilder, SnapdragonSuperResolutionPass);
	check(PassInputs.SceneColor.IsValid());

	if (PassInputs.Stage == EUpscaleStage::SecondaryToOutput)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(23246, 1.0f, FColor::Red, FString::Printf(TEXT("SGSR doesn't support a secondary upscaler, please disable it on your project config or turn off SGSR")), true);
		}

		EUpscaleMethod Method = View.Family && View.Family->SecondaryScreenPercentageMethod == ESecondaryScreenPercentageMethod::LowerPixelDensitySimulation
			? EUpscaleMethod::SmoothStep
			: EUpscaleMethod::Nearest;

		return ISpatialUpscaler::AddDefaultUpscalePass(GraphBuilder, View, PassInputs, Method, FPaniniProjectionConfig());
	}

	TSharedPtr<FSGSRData> Data = GetDataForView(View);
	for (FSGSRSubpass* Subpass : SGSRSubpasses)
	{
		Subpass->SetData(Data.Get());
	}

	if (!Data->bInitialized)
	{
		EXECUTE_STEP(ParseEnvironment);
		EXECUTE_STEP(CreateResources);
	}

	if (Mode == ESGSRMode::UpscalingOnly || Mode == ESGSRMode::Combined)
	{
		EXECUTE_STEP(Upscale);
	}

	if (Mode == ESGSRMode::PostProcessingOnly || Mode == ESGSRMode::Combined)
	{
		EXECUTE_STEP(PostProcess);
	}

	FScreenPassTexture FinalOutput = Data->FinalOutput;
	return MoveTemp(FinalOutput);
}

TSharedPtr<FSGSRData> FSGSRSpatialUpscaler::GetDataForView(const FViewInfo& View) const
{
	for (int i = 0; i < View.Family->Views.Num(); i++)
	{
		if (View.Family->Views[i] == &View)
		{
			return ViewData[i];
		}
	}
	return nullptr;
}