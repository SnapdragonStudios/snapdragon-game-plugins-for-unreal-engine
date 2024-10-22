//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "Engine/Engine.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessUpscale.h"
#include "PostProcess/TemporalAA.h"
#include "Containers/LockFreeList.h"
#include "CustomStaticScreenPercentage.h"
//#include "ScreenSpaceDenoise.h"
#include "GSRTUHistory.h"

struct FPostProcessingInputs;

typedef enum GSRMsgtype {
	GSR_MESSAGE_TYPE_ERROR = 0,
	GSR_MESSAGE_TYPE_WARNING = 1,
	GSR_MESSAGE_TYPE_COUNT
} GSRMsgtype;

//////TODO: SSD support later
class FGSRTU final : public ITemporalUpscaler, public ICustomStaticScreenPercentage//, public IScreenSpaceDenoiser
{
	friend class FGSRFXSystem;
public:
	FGSRTU();
	virtual ~FGSRTU();

	//void Initialize() const;

	const TCHAR* GetDebugName() const override;

	void Releasestate(GSRstateRef state);

	static float GetRfraction(uint32 Mode);

#if DO_CHECK || DO_GUARD_SLOW || DO_ENSURE
	static void onGSRMessage(GSRMsgtype type, const wchar_t* message);
#endif

	//class FGDGBuilder* GetGraphBuilder();

	void AddPasses(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		const ITemporalUpscaler::FPassInputs& PassInputs,
		FRDGTextureRef* OutSceneColorTexture,
		FIntRect* OutSceneColorViewRect,
		FRDGTextureRef* OutSceneColorHalfResTexture,
		FIntRect* OutSceneColorHalfResViewRect) const override;

	void SetupMainGameViewFamily(FSceneViewFamily& InViewFamily) override;

	float GetMinUpsampleResolutionFraction() const override;
	float GetMaxUpsampleResolutionFraction() const override;

	void CopyOpaqueColor(FRHICommandListImmediate& RHICmdList, FRHIUniformBuffer* ViewUniformBuffer,
		const class FShaderParametersMetadata* SceneTexturesUniformBufferStruct, FRHIUniformBuffer* SceneTexturesUniformBuffer);

	void SetPostProcessingInputs(FPostProcessingInputs const& Inputs);

	void EndofFrame();

private:
	void Cleanup() const;

	mutable FPostProcessingInputs PostInputs;
	FDynamicResolutionStateInfos DynamicResolutionStateInfos;
	mutable TLockFreePointerListFIFO<FGSRState, PLATFORM_CACHE_LINE_SIZE> Availablestates;
	mutable class FRDGBuilder* CurrentGraphBuilder;
};