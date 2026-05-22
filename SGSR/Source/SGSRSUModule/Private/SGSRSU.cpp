//============================================================================================================
//
//
//                  Copyright (c) 2023, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SGSRSU.h"
#include "LogSGSR.h"
#include "SceneRendering.h"
#include "Subpasses/SGSRSubpassFirst.h"
#include "Subpasses/SGSRSubpassScaler.h"
#include "Subpasses/SGSRSubpassLast.h"

#define EXECUTE_STEP(step) \
	for (FSGSRSubpass* Subpass : SGSRSubpasses) \
	{ \
		Subpass->step(GraphBuilder, View, PassInputs); \
	}

DECLARE_GPU_STAT(SnapdragonSuperResolutionPass);

FSGSRSU::FSGSRSU(ESGSRMode InMode, TArray<TSharedPtr<FSGSRData>> InViewData)
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

ISpatialUpscaler* FSGSRSU::Fork_GameThread(const class FSceneViewFamily& ViewFamily) const
{
	return new FSGSRSU(Mode, ViewData);
}

FScreenPassTexture FSGSRSU::AddPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) const
{
	RDG_GPU_STAT_SCOPE(GraphBuilder, SnapdragonSuperResolutionPass);
	check(PassInputs.SceneColor.IsValid());

	if (PassInputs.Stage == EUpscaleStage::SecondaryToOutput)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(23246, 1.0f, FColor::Red, FString::Printf(TEXT("SGSR Spatial Upscaling doesn't support a secondary upscaler, please disable it on your project config or turn off SGSRSU")), true);
		}
		else UE_LOG(LogSGSR, Warning, TEXT("SGSR Spatial Upscaling doesn't support a secondary upscaler, please disable it on your project config or turn off SGSRSU"));

		EUpscaleMethod Method = View.Family && View.Family->SecondaryScreenPercentageMethod == ESecondaryScreenPercentageMethod::LowerPixelDensitySimulation
			? EUpscaleMethod::SmoothStep
			: EUpscaleMethod::Nearest;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 4
		if (View.GetShaderPlatform() == SP_VULKAN_ES3_1_ANDROID)
			Method = EUpscaleMethod::Nearest;
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		if (IsMobilePlatform(View.GetShaderPlatform()))
			Method = EUpscaleMethod::Nearest;
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 4
		return ISpatialUpscaler::AddDefaultUpscalePass(GraphBuilder, View, PassInputs, Method, FPaniniProjectionConfig());
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		return ISpatialUpscaler::AddDefaultUpscalePass(GraphBuilder, View, PassInputs, Method, FLensDistortionLUT());
#endif
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

TSharedPtr<FSGSRData> FSGSRSU::GetDataForView(const FViewInfo& View) const
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