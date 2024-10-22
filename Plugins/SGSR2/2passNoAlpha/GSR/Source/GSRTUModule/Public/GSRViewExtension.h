//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "SceneViewExtension.h"
#include "CustomStaticScreenPercentage.h"

class GSRTUMODULE_API FGSRViewExtension final : public FSceneViewExtensionBase
{
public:
	FGSRViewExtension(const FAutoRegister& AutoRegister);

	void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}

	void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override {}
	void PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;

private:
	class ICustomStaticScreenPercentage* GGSRCustomStaticScreenPercentage;
	int32 PreviousGSRState;
	int32 PreviousGSRStateRT;
	int32 CurrentGSRStateRT;
	float MinAutoViewMipBiasMin;
	float MinAutoViewMipBiasOffset;
	int32 VertexDeformationOutputsVelocity;
	int32 BasePassForceOutputsVelocity;
};

