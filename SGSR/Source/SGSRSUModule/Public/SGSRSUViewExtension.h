//============================================================================================================
//
//
//                  Copyright (c) 2023, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "SceneViewExtension.h"

class FSGSRSUViewExtension final : public FSceneViewExtensionBase
{
public:
	FSGSRSUViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister) {}

	// ISceneViewExtension interface
	void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 6
	void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override {}
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 5
	void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override {}
#endif

	SGSRSUMODULE_API void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
};