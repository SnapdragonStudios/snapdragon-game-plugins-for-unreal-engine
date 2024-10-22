//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "SceneViewExtension.h"

class GSRTUMODULE_API FGSRViewExtension final : public FSceneViewExtensionBase
{
public:
	FGSRViewExtension(const FAutoRegister& AutoRegister);

	void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}

	void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;

private:
	int32 PreviousGSRState;
	int32 PreviousGSRStateRT;
	int32 CurrentGSRStateRT;
	float MinAutoViewMipBiasMin;
	float MinAutoViewMipBiasOffset;
	int32 VertexDeformationOutputsVelocity;
	/*int32 BasePassForceOutputsVelocity;*/
	int32 VelocityEnableGrass;
};
