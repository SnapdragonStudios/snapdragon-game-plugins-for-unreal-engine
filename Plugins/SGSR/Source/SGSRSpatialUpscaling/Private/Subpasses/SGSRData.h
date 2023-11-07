//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "SGSRSubpassSharedTypes.h"
#include "PostProcess/PostProcessTonemap.h"

struct FSGSRData
{
	bool bInitialized = false;

	bool bSGSREnabled;
	bool bSGSRIsTheLastPass;

	FRDGTextureDesc SGSROutputTextureDesc;
	FScreenPassTexture UpscaleTexture;

	FScreenPassTextureViewport InputViewport;
	FScreenPassTextureViewport OutputViewport;

	FRDGTextureRef CurrentInputTexture;
	FScreenPassTexture FinalOutput;
};