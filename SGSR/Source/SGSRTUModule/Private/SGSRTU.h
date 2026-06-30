//============================================================================================================
//
//
//                  Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "Engine/Engine.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessUpscale.h"
#include "PostProcess/TemporalAA.h"
#include "Containers/LockFreeList.h"
#include "SGSRTUHistory.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 8
#include "PostProcess/PostProcessInputs.h"
#endif

struct FPostProcessingInputs;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
#include "TemporalUpscaler.h"
using ISGSRTemporalUpscaler = UE::Renderer::Private::ITemporalUpscaler;
using SGSRPassInput = UE::Renderer::Private::ITemporalUpscaler::FInputs;
using SGSRPassOutput = UE::Renderer::Private::ITemporalUpscaler::FOutputs;
using ICustomTemporalAAHistory = UE::Renderer::Private::ITemporalUpscaler::IHistory;
using SGSRView = FSceneView;
#else
//typedef ITemporalUpscaler IGSRTemporalUpscaler;
using ISGSRTemporalUpscaler = ITemporalUpscaler;
using SGSRPassInput = ITemporalUpscaler::FPassInputs;
using SGSRPassOutput = ITemporalUpscaler::FOutputs;
using SGSRView = FViewInfo;
#endif


// using IGSRTemporalUpscaler = ITemporalUpscaler;
// using GSRPassInput = ITemporalUpscaler::FPassInputs;

typedef enum SGSRMsgtype
{
	SGSR_MESSAGE_TYPE_ERROR = 0,
	SGSR_MESSAGE_TYPE_WARNING = 1,
	SGSR_MESSAGE_TYPE_COUNT
} SGSRMsgtype;

class FSGSRTU final : public ISGSRTemporalUpscaler
{
	friend class FSGSRFXSystem;

public:
	FSGSRTU();
	/*FGSRTU(IGSRTemporalUpscaler* TU);*/
	virtual ~FSGSRTU();

	const TCHAR* GetDebugName() const override;
	void Releasestate(SGSRstateRef state);
	static float GetRfraction(uint32 Mode);

	static void SaveScreenPercentage();
	static void UpdateScreenPercentage();
	static void RestoreScreenPercentage();

	static void ChangeSGSREnabled(IConsoleVariable* Var);
	static void ChangeQualityMode(IConsoleVariable* Var); 
	static void ChangeSGSRMethod(IConsoleVariable* Var);
	static void ChangeCustomScreenPercentage(IConsoleVariable* Var);

	bool IsAlphaSupported() const;

	SGSRPassOutput AddPasses(
		FRDGBuilder& GraphBuilder,
		const SGSRView& View,
		const SGSRPassInput& PassInputs) const override;

	SGSRPassOutput AddPasses_2PassFS(
		FRDGBuilder& GraphBuilder,
		const SGSRView& View,
		const SGSRPassInput& PassInputs) const;

	SGSRPassOutput AddPasses_3PassCS(
		FRDGBuilder& GraphBuilder,
		const SGSRView& View,
		const SGSRPassInput& PassInputs) const;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
	ISGSRTemporalUpscaler* Fork_GameThread(const class FSceneViewFamily& InViewFamily) const override;
#endif

	float GetMinUpsampleResolutionFraction() const override;
	float GetMaxUpsampleResolutionFraction() const override;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
	void CopyOpaqueColor(FRDGBuilder& GraphBuilder, TConstStridedView<FSceneView> Views,
#else
	void CopyOpaqueColor(FRDGBuilder& GraphBuilder, TConstArrayView<FViewInfo> Views,
#endif
		const class FShaderParametersMetadata* SceneTexturesUniformBufferStruct, FRHIUniformBuffer* SceneTexturesUniformBuffer);

	void SetPostProcessingInputs(FPostProcessingInputs const& Inputs);

	void EndofFrame();

private:
	void Cleanup() const;

	/*IGSRTemporalUpscaler* TemporalUpscaler;*/
	mutable FPostProcessingInputs PostInputs;
	FDynamicResolutionStateInfos DynamicResolutionStateInfos;
	mutable TLockFreePointerListFIFO<FSGSRState, PLATFORM_CACHE_LINE_SIZE> Availablestates;
	/*mutable class FRDGBuilder* CurrentGraphBuilder;*/
	mutable FRDGTextureRef ReflectionTexture;
	mutable FTextureRHIRef SceneColorpreAlpha;
	mutable TRefCountPtr<IPooledRenderTarget> SceneColorpreAlphaRT;
	mutable TRefCountPtr<IPooledRenderTarget> HistoryColorRT;
	mutable TRefCountPtr<IPooledRenderTarget> NewLocksRT;

	static float SavedScreenPercentage; 
};

class FSGSRTUFork final : public ISGSRTemporalUpscaler 
{
public:
	FSGSRTUFork(ISGSRTemporalUpscaler* TU);
	virtual ~FSGSRTUFork();

	const TCHAR* GetDebugName() const override;

	ISGSRTemporalUpscaler::FOutputs AddPasses(
		FRDGBuilder& GraphBuilder,
		const SGSRView& View,
		const SGSRPassInput& PassInputs) const override;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
	ISGSRTemporalUpscaler* Fork_GameThread(const class FSceneViewFamily& InViewFamily) const override;
#endif

	float GetMinUpsampleResolutionFraction() const override;
	float GetMaxUpsampleResolutionFraction() const override;

private:
	ISGSRTemporalUpscaler* TemporalUpscaler;
};
