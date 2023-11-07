//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "SGSRData.h"

#include "PostProcess/PostProcessUpscale.h"

class FSGSRSubpass
{
public:
	typedef ISpatialUpscaler::FInputs FInputs;

	inline void SetData(FSGSRData* InData)
	{
		Data = InData;
	};

	virtual void ParseEnvironment(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) {}
	virtual void CreateResources(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) {}
	virtual void Upscale(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) {}
	virtual void PostProcess(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) {}

protected:
	FSGSRData* Data;
};