//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "Subpasses/SGSRData.h"

#include "PostProcess/PostProcessUpscale.h"

class FSGSRSubpass;

enum class ESGSRMode
{
	None,
	UpscalingOnly,
	PostProcessingOnly,
	Combined
};

class FSGSRSpatialUpscaler final : public ISpatialUpscaler
{
public:
	FSGSRSpatialUpscaler(ESGSRMode InMode, TArray<TSharedPtr<FSGSRData>> InViewData);

	// ISpatialUpscaler interface
	const TCHAR* GetDebugName() const override { return TEXT("FSGSRSpatialUpscaler"); }

	ISpatialUpscaler* Fork_GameThread(const class FSceneViewFamily& ViewFamily) const override;
	FScreenPassTexture AddPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) const override;

private:
	template <class T>
	T* RegisterSubpass()
	{
		T* Subpass = new T();
		SGSRSubpasses.Add(Subpass);
		return Subpass;
	}

	TSharedPtr<FSGSRData> GetDataForView(const FViewInfo& View) const;

	ESGSRMode Mode;
	TArray<TSharedPtr<FSGSRData>> ViewData;
	TArray<FSGSRSubpass*> SGSRSubpasses;
};
