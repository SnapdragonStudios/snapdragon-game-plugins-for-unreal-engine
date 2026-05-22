//============================================================================================================
//
//
//                  Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "SceneViewExtension.h"

class SGSRTUMODULE_API FSGSRTUViewExtension final : public FSceneViewExtensionBase
{
public:
	FSGSRTUViewExtension(const FAutoRegister& AutoRegister);

	void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}

	void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;

private:
	int32 PreviousSGSRState;
	int32 PreviousSGSRStateRT;
	int32 CurrentSGSRStateRT;
	float MinAutoViewMipBiasMin;
	float MinAutoViewMipBiasOffset;
	int32 VertexDeformationOutputsVelocity;
	/*int32 BasePassForceOutputsVelocity;*/
	int32 VelocityEnableGrass;
};

