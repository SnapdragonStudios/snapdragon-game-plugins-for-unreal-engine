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
#include "GSRTUHistory.h"

struct FPostProcessingInputs;

typedef enum GSRMsgtype {
	GSR_MESSAGE_TYPE_ERROR = 0,
	GSR_MESSAGE_TYPE_WARNING = 1,
	GSR_MESSAGE_TYPE_COUNT
} GSRMsgtype;

class FGSRTU final : public ITemporalUpscaler
{
	friend class FGSRFXSystem;
public:
	FGSRTU(); 
	/*FGSRTU(ITemporalUpscaler* TU);*/
	virtual ~FGSRTU();

	const TCHAR* GetDebugName() const override;
	void Releasestate(GSRstateRef state);
	static float GetRfraction(uint32 Mode);

#if DO_CHECK || DO_GUARD_SLOW || DO_ENSURE
	static void onGSRMessage(GSRMsgtype type, const wchar_t* message);
#endif
	static void ChangeGSREnabled(IConsoleVariable* Var);
	static void ChangeQualityMode(IConsoleVariable* Var);

	ITemporalUpscaler::FOutputs AddPasses(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		const ITemporalUpscaler::FPassInputs& PassInputs) const override;

	ITemporalUpscaler* Fork_GameThread(const class FSceneViewFamily& InViewFamily) const override;

	float GetMinUpsampleResolutionFraction() const override;
	float GetMaxUpsampleResolutionFraction() const override;

	void CopyOpaqueColor(FRDGBuilder& GraphBuilder, TConstArrayView<FViewInfo> Views,
		const class FShaderParametersMetadata* SceneTexturesUniformBufferStruct, FRHIUniformBuffer* SceneTexturesUniformBuffer);

	void SetPostProcessingInputs(FPostProcessingInputs const& Inputs);

	void EndofFrame();

private:
	void Cleanup() const;

	/*ITemporalUpscaler* TemporalUpscaler;*/
	mutable FPostProcessingInputs PostInputs;
	FDynamicResolutionStateInfos DynamicResolutionStateInfos;
	mutable TLockFreePointerListFIFO<FGSRState, PLATFORM_CACHE_LINE_SIZE> Availablestates;
	/*mutable class FRDGBuilder* CurrentGraphBuilder;*/
	mutable FRDGTextureRef ReflectionTexture;
	mutable FTexture2DRHIRef SceneColorpreAlpha;
	mutable TRefCountPtr<IPooledRenderTarget> SceneColorpreAlphaRT;
	static float SavedSP;  //ScreenPercentage
};

class FGSRTUFork final : public ITemporalUpscaler
{
public:
	FGSRTUFork(ITemporalUpscaler* TU);
	virtual ~FGSRTUFork();

	const TCHAR* GetDebugName() const override;

	ITemporalUpscaler::FOutputs AddPasses(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		const ITemporalUpscaler::FPassInputs& PassInputs) const override;

	ITemporalUpscaler* Fork_GameThread(const class FSceneViewFamily& InViewFamily) const override;

	float GetMinUpsampleResolutionFraction() const override;
	float GetMaxUpsampleResolutionFraction() const override;

private:
	ITemporalUpscaler* TemporalUpscaler;
};