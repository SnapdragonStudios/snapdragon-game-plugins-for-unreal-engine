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

#include "TemporalUpscaler.h"
using IGSRTemporalUpscaler = UE::Renderer::Private::ITemporalUpscaler;
using GSRPassInput = UE::Renderer::Private::ITemporalUpscaler::FInputs;
using GSRView = FSceneView;
using ICustomTemporalAAHistory = UE::Renderer::Private::ITemporalUpscaler::IHistory;

// using IGSRTemporalUpscaler = ITemporalUpscaler;
// using GSRPassInput = ITemporalUpscaler::FPassInputs;

typedef enum GSRMsgtype
{
	GSR_MESSAGE_TYPE_ERROR = 0,
	GSR_MESSAGE_TYPE_WARNING = 1,
	GSR_MESSAGE_TYPE_COUNT
} GSRMsgtype;

class FGSRTU final : public IGSRTemporalUpscaler
{
	friend class FGSRFXSystem;

public:
	FGSRTU();
	/*FGSRTU(IGSRTemporalUpscaler* TU);*/
	virtual ~FGSRTU();

	const TCHAR* GetDebugName() const override;
	void Releasestate(GSRstateRef state);
	static float GetRfraction(uint32 Mode);

#if DO_CHECK || DO_GUARD_SLOW || DO_ENSURE
	static void onGSRMessage(GSRMsgtype type, const wchar_t* message);
#endif
	static void SaveScreenPercentage();
	static void UpdateScreenPercentage();
	static void RestoreScreenPercentage();

	static void ChangeGSREnabled(IConsoleVariable* Var);
	static void ChangeQualityMode(IConsoleVariable* Var);

	IGSRTemporalUpscaler::FOutputs AddPasses(
		FRDGBuilder& GraphBuilder,
		const GSRView& View,
		const IGSRTemporalUpscaler::FInputs& PassInputs) const override;

	IGSRTemporalUpscaler* Fork_GameThread(const class FSceneViewFamily& InViewFamily) const override;

	float GetMinUpsampleResolutionFraction() const override;
	float GetMaxUpsampleResolutionFraction() const override;

	void SetPostProcessingInputs(FPostProcessingInputs const& Inputs);

	void EndofFrame();

private:
	void Cleanup() const;

	/*IGSRTemporalUpscaler* TemporalUpscaler;*/
	mutable FPostProcessingInputs PostInputs;
	FDynamicResolutionStateInfos DynamicResolutionStateInfos;
	mutable TLockFreePointerListFIFO<FGSRState, PLATFORM_CACHE_LINE_SIZE> Availablestates;
	mutable TRefCountPtr<IPooledRenderTarget> HistoryColorRT;
	/*mutable class FRDGBuilder* CurrentGraphBuilder;*/
	static float SavedScreenPercentage;
};

class FGSRTUFork final : public IGSRTemporalUpscaler
{
public:
	FGSRTUFork(IGSRTemporalUpscaler* TU);
	virtual ~FGSRTUFork();

	const TCHAR* GetDebugName() const override;

	IGSRTemporalUpscaler::FOutputs AddPasses(
		FRDGBuilder& GraphBuilder,
		const GSRView& View,
		const GSRPassInput& PassInputs) const override;

	IGSRTemporalUpscaler* Fork_GameThread(const class FSceneViewFamily& InViewFamily) const override;

	float GetMinUpsampleResolutionFraction() const override;
	float GetMaxUpsampleResolutionFraction() const override;

private:
	IGSRTemporalUpscaler* TemporalUpscaler;
};