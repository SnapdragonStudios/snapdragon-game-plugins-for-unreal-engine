//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "SceneViewExtension.h"

class FSGSRViewExtension final : public FSceneViewExtensionBase
{
public:
	FSGSRViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister) {}

	// ISceneViewExtension interface
	void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override {}

	SGSRSPATIALUPSCALING_API void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	SGSRSPATIALUPSCALING_API void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
};