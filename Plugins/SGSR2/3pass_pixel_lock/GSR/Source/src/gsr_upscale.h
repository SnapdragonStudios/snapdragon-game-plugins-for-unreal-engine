//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
/*
**
** reference from
Lanczos resampling, https://en.wikipedia.org/wiki/Lanczos_resampling
YCoCg Color Space, https://en.wikipedia.org/wiki/YCoCg
https://developer.download.nvidia.com/gameworks/events/GDC2016/msalvi_temporal_supersampling.pdf
https://github.com/GPUOpen-Effects/FidelityFX-FSR2/blob/master/src/ffx-fsr2-api/shaders/ffx_fsr2_upsample.h
under MIT license
*/


#define five_sample_use SAMPLE_NUMBER
#define GSR_OPTION_APPLY_SHARPENING DO_SHARPENING
#define GSR_OPTION_INVERTED_DEPTH INVERTED_DEPTH
#define GSR_OPTION_PIXEL_LOCK ENABLE_PIXEL_LOCK
#define IGNORE_EDGE_CLAMP (1) 

Texture2D<half4> YCoCgLuma;
Texture2D<half4> DilatedMotionDepthLuma;
Texture2D<half4> PrevHistoryOutput;
Texture2D<half> ReactiveMask;

SamplerState PointClamp;
SamplerState LinearClamp;
float PreExposure;
float ValidReset;
float JitterSeqLength;

RWTexture2D<half4> HistoryOutput;
RWTexture2D<half3> SceneColorOutput;
RWTexture2D<half> NewLocks;

half3 ComputeBaseAccumutationWeight(half fThisFrameReactive, half fDepthClipFactor, bool bIsExistingSample, bool bInMotionLastFrame, half fUpsampledWeight, half fHrVelocity)
{
	half fBaseAccumulation = half(bIsExistingSample) * (1.0 - fThisFrameReactive) * (1.0 - fDepthClipFactor); // bInMotionLastFrame = (History.w<0.0f);
	fBaseAccumulation = min(fBaseAccumulation, lerp(fBaseAccumulation, fUpsampledWeight * 10.0, max(half(bInMotionLastFrame), saturate(fHrVelocity * 10.0))));
	fBaseAccumulation = min(fBaseAccumulation, lerp(fBaseAccumulation, fUpsampledWeight, saturate(fHrVelocity / 20.0)));
	return fBaseAccumulation.xxx;
}

half ComputeTemporalReactiveFactor(half fTemporalReactiveFactor, half fDepthClipFactor, half fHrVelocity, bool bIsNewSample)
{
	half fNewFactor = min(0.99, fTemporalReactiveFactor);
	fNewFactor = max(fNewFactor, lerp(fNewFactor, 0.4, saturate(fHrVelocity)));
	fNewFactor = max(fNewFactor * fNewFactor, max(fDepthClipFactor * 0.1, 0.0));
	fNewFactor = bIsNewSample ? 1.0 : fNewFactor;

	if (saturate(fHrVelocity * 10.0) >= 1.0)
	{
		fNewFactor = -max(1e-03, fNewFactor);
	}
	return fNewFactor;
}

half FastLanczos(half x2)
{
	//x2 = min(x2, 4.0f);  TODO
	half a = (2.0 / 5.0) * x2 - 1.0;
	half b = (1.0 / 4.0) * x2 - 1.0;
	return ((25.0 / 16.0) * a * a - (9.0 / 16.0)) * (b * b);
}

bool ReprojectHistoryLockStatus(float2 fReprojectedHrUv, int2 iPxHrPos, inout half fReprojectedLockStatus)
{
	const half fNewLockIntensity = NewLocks[iPxHrPos];
	bool bNewLock = fNewLockIntensity > 1.999;
	fReprojectedLockStatus = fNewLockIntensity;
	return bNewLock;
}

void UpdateLockStatus(half fDepthClipFactor, half fAccumulationMask,
	inout half fReactiveFactor, bool bNewLock,
	inout half fLockStatus,
	out half fLockContributionThisFrame,
	half fLuminanceDiff)
{
	if (fLockStatus < 1.9999)  //if not a new lock
	{
		fLockStatus *= half(fLuminanceDiff > 0.4);
	}

	fLockStatus *= half(fReactiveFactor + half(FLT_FP16_MIN) > 0 && fReactiveFactor - half(FLT_FP16_MIN) < 0);

	fLockStatus *= (1.0 - fReactiveFactor);
	fLockStatus *= half(fDepthClipFactor < 0.1);

	// Compute this frame lock contribution
	const half fLifetimeContribution = saturate(fLockStatus - 1.0);

	fLockContributionThisFrame = saturate(saturate(fLifetimeContribution * 4.0));
}

void FinalizeLockStatus(float2 fHrUv, float2 fMotionVector, int2 iPxHrPos, half fLockStatus, half fUpsampledWeight, half hrVelocity)
{
	float2 fEstimatedUvNextFrame = fHrUv - fMotionVector;
	if (IsUvInside(fEstimatedUvNextFrame) == false)
	{
		fLockStatus = 0.0;
	}
	else
	{
		// Decrease lock lifetime
#ifdef GSR_OPTION_APPLY_5SAMPLE
		half fUpsampleLanczosWeightScale = 1.0 / 5.0;
#else
		half fUpsampleLanczosWeightScale = 1.0 / 9.0;
#endif
		half fAverageLanczosWeightPerFrame = 0.74 * fUpsampleLanczosWeightScale;						  // Average lanczos weight for jitter accumulated samples
		const half fLifetimeDecreaseLanczosMax = half(JitterSeqLength) * fAverageLanczosWeightPerFrame; // 2.631
		const half fLifetimeDecrease = half(fUpsampledWeight / fLifetimeDecreaseLanczosMax);
		fLockStatus = max(half(0.0), fLockStatus - fLifetimeDecrease);
	}
	NewLocks[iPxHrPos] = fLockStatus;
}

void Update(uint2 DisThreadID)
{
	float2 Hruv = (DisThreadID + 0.5) * HistoryInfo_ViewportSizeInverse;
	float2 Jitteruv;
	Jitteruv.x = clamp(Hruv.x + InputJitter.x * InputInfo_ViewportSizeInverse.x, 0.0f, 1.0f);
	Jitteruv.y = clamp(Hruv.y + InputJitter.y * InputInfo_ViewportSizeInverse.y, 0.0f, 1.0f);
	int2 InputPos = Jitteruv * InputInfo_ViewportSize;

	float2 Motion = DilatedMotionDepthLuma.SampleLevel(LinearClamp, Hruv, 0).xy;
	half fReactiveMasks = ReactiveMask.SampleLevel(LinearClamp, Jitteruv, 0).x;
	float2 PrevUV;

	PrevUV.x = Motion.x + Hruv.x;
	PrevUV.y = Motion.y + Hruv.y;

	half fDepthFactor = fReactiveMasks.x;
	half fAccumulationMask = 0.0;
	half4 History = half4(0.0, 0.0, 0.0, 0.0);

	half fTemporalReactiveFactor = 0.0;
	bool bInMotionLastFrame = false;
	bool bIsExistingSample = IsUvInside(PrevUV);
	bool bIsResetFrame = (ValidReset > FLT_EPSILON);
	bool bIsNewSample = (!bIsExistingSample || bIsResetFrame);

	half fReprojectedLockStatus = 0.0;
	bool bNewLock = false;
	if (bIsExistingSample && !bIsResetFrame)
	{
		History = PrevHistoryOutput.SampleLevel(LinearClamp, PrevUV, 0);
		fTemporalReactiveFactor = saturate(abs(History.w));
		bInMotionLastFrame = (History.w < 0.0);
#if GSR_OPTION_PIXEL_LOCK
		bNewLock = ReprojectHistoryLockStatus(PrevUV, int2(DisThreadID), fReprojectedLockStatus);
#endif
	}
	half fThisFrameReactiveFactor = max(0.0, fTemporalReactiveFactor);

	History.xyz = PrepareRgb(History.xyz, half(PreExposure));
	History.xyz = RGBToYCoCg(History.xyz);

	half3 HistoryColor = History.xyz;

	half fLockContributionThisFrame = 0.0;
	half fLuminanceDiff = 0.0;
#if GSR_OPTION_PIXEL_LOCK
	half curLuma = YCoCgLuma.SampleLevel(PointClamp, Jitteruv, 0).x;
	half prevLuma = History.x;
	half maxLuma = max(curLuma, prevLuma);
	half fLuminanceDiff1 = maxLuma > half(FLT_FP16_MIN) ? min(curLuma, prevLuma) / maxLuma : 0.0;

	curLuma = YCoCgLuma.SampleLevel(PointClamp, Jitteruv, 0, int2(1, 0)).x;
	maxLuma = max(curLuma, prevLuma);
	half fLuminanceDiff2 = maxLuma > half(FLT_FP16_MIN) ? min(curLuma, prevLuma) / maxLuma : 0.0;

	curLuma = YCoCgLuma.SampleLevel(PointClamp, Jitteruv, 0, int2(0, 1)).x;
	maxLuma = max(curLuma, prevLuma);
	half fLuminanceDiff3 = maxLuma > half(FLT_FP16_MIN) ? min(curLuma, prevLuma) / maxLuma : 0.0;

	curLuma = YCoCgLuma.SampleLevel(PointClamp, Jitteruv, 0, int2(-1, 0)).x;
	maxLuma = max(curLuma, prevLuma);
	half fLuminanceDiff4 = maxLuma > half(FLT_FP16_MIN) ? min(curLuma, prevLuma) / maxLuma : 0.0;

	curLuma = YCoCgLuma.SampleLevel(PointClamp, Jitteruv, 0, int2(0, -1)).x;
	maxLuma = max(curLuma, prevLuma);
	half fLuminanceDiff5 = maxLuma > half(FLT_FP16_MIN) ? min(curLuma, prevLuma) / maxLuma : 0.0;

	fLuminanceDiff = fLuminanceDiff1 + fLuminanceDiff2 + fLuminanceDiff3 + fLuminanceDiff4 + fLuminanceDiff5; 

	UpdateLockStatus(fDepthFactor, fAccumulationMask, fThisFrameReactiveFactor, bNewLock, fReprojectedLockStatus, fLockContributionThisFrame, fLuminanceDiff); // TODO: use fLuminanceDiff to ComputeLumaInstabilityFactor
#endif
	/////upsample and compute box
	half fKernelReactiveFacor = max(fThisFrameReactiveFactor, half(bIsNewSample));
	half fKernelWeight = half(1.0 + (HistoryInfo_ViewportSize.x * InputInfo_ViewportSizeInverse.x - 1.0) /* * 1.0*/);
	half fKernelBiasMax = max(1.99, fKernelWeight) * (1.0 - fKernelReactiveFacor);
	half fKernelBiasMin = max(1.0, (1.0 + fKernelBiasMax) * 0.3);
	half fKernelBiasFactor = max(0.0, max(0.25 * fDepthFactor, fKernelReactiveFacor));
	half fKernelBias = lerp(fKernelBiasMax, fKernelBiasMin, fKernelBiasFactor);
	
	fKernelBias *= 0.7;
	
	half4 UpsampledColorAndWeight = half4(0.0, 0.0, 0.0, 0.0);

	half fHrVelocity = half(length(Motion * HistoryInfo_ViewportSize));
	half curvebias = lerp(-2.0, -3.0, saturate(fHrVelocity * 0.02));

	half3 rectboxcenter = half3(0.0, 0.0, 0.0);
	half3 rectboxvar = half3(0.0, 0.0, 0.0);
	half rectboxweight = 0.0;

	float2 srcpos = InputPos + float2(0.5, 0.5) - InputJitter; 
	float2 srcOutputPos = Hruv * InputInfo_ViewportSize;		

	half2 srcpos_srcOutputPos = half2(srcpos - srcOutputPos); 
														
	half3 rectboxmin = half3(FLT_FP16_MAX, FLT_FP16_MAX, FLT_FP16_MAX);
	half3 rectboxmax = half3(0.0, 0.0, 0.0);
#if five_sample_use
	const int16_t2 sampleOffset[5] = {
		int16_t2(1, 0),
		int16_t2(-1, 0),
		int16_t2(0, 0),
		int16_t2(0, 1),
		int16_t2(0, -1),
	};
	const int sampleNums = 5;
#else
	const int16_t2 sampleOffset[9] = {
		int16_t2(1, 0),
		int16_t2(-1, 0),
		int16_t2(0, 0),
		int16_t2(0, 1),
		int16_t2(0, -1),
		int16_t2(1, 1),
		int16_t2(-1, -1),
		int16_t2(1, -1),
		int16_t2(-1, 1),

	};
	const int sampleNums = 9;
#endif
	[unroll] for (int i = 0; i < sampleNums; i++)
	{
		int16_t2 offset = sampleOffset[i];
#if IGNORE_EDGE_CLAMP
		half3 samplecolor = YCoCgLuma.SampleLevel(PointClamp, Jitteruv, 0, offset).xyz;
#else
		half3 samplecolor = YCoCgLuma[ClampLoad(InputPos, offset, InputInfo_ViewportSize)].xyz;
#endif

		half2 baseoffset = (srcpos_srcOutputPos + half2(offset)) * fKernelBias;
		half baseoffset_dot = dot(baseoffset, baseoffset);
		half base = min(baseoffset_dot, 4.0);
#if IGNORE_EDGE_CLAMP
		half weight = FastLanczos(base);
#else
		half weight = FastLanczos(base) * half(all((InputPos + offset) < InputInfo_ViewportSize));
#endif

		UpsampledColorAndWeight += half4(samplecolor * weight, weight);
		half boxweight = exp(baseoffset_dot * curvebias);
		rectboxmin = min(rectboxmin, samplecolor);
		rectboxmax = max(rectboxmax, samplecolor);
		half3 wsample = samplecolor * boxweight;
		rectboxcenter += wsample;
		rectboxvar += (samplecolor * wsample); 
		rectboxweight += boxweight;			  
	}

	half fBoxCenterWeight = abs(rectboxweight) > 0.001 ? rectboxweight : 1.0;
	rectboxweight = 1.0 / fBoxCenterWeight;
	rectboxcenter *= rectboxweight;
	rectboxvar *= rectboxweight;

	rectboxvar = sqrt(abs(rectboxvar - rectboxcenter * rectboxcenter));

	//UpsampledColorAndWeight.w *= half(UpsampledColorAndWeight.w > 0.001);
	if (UpsampledColorAndWeight.w > 0.001)
	{
		UpsampledColorAndWeight.xyz = UpsampledColorAndWeight.xyz / UpsampledColorAndWeight.w;
		UpsampledColorAndWeight.w = UpsampledColorAndWeight.w * (1.0 / half(sampleNums));
		UpsampledColorAndWeight.xyz = clamp(UpsampledColorAndWeight.xyz, rectboxmin, rectboxmax);
	}

	half3 fAccumulation = ComputeBaseAccumutationWeight(fThisFrameReactiveFactor, fDepthFactor, bIsExistingSample, bInMotionLastFrame, UpsampledColorAndWeight.w, fHrVelocity);
	half BaseAccumulation = fAccumulation.x;
	if (bIsNewSample)
	{
		HistoryColor = YCoCgToRGB(UpsampledColorAndWeight.xyz);
	}
	else
	{
		// Rectify history
		half fScaleFactorInfluence = min(20.0, pow(half(1.0 / length((InputInfo_ViewportSize.x / OutputInfo_ViewportSize.x) * (InputInfo_ViewportSize.y / OutputInfo_ViewportSize.y))), 3.0));
		half fVelocityFactor = saturate(fHrVelocity * 0.05);
		half boxscale = max(fDepthFactor, fVelocityFactor);
		half boxsize = lerp(fScaleFactorInfluence, 1.0, boxscale);

		half3 sboxvar = rectboxvar * boxsize;
		half3 boxmin = rectboxcenter - sboxvar;
		half3 boxmax = rectboxcenter + sboxvar;
		rectboxmax = min(rectboxmax, boxmax);
		rectboxmin = max(rectboxmin, boxmin);
		half3 clampedHistoryColor = clamp(HistoryColor, rectboxmin, rectboxmax);

		half fLumaInstabilityFactor = 0.0; /// TODO:need to implement fLumaInstabilityFactor?
		half lerpcontribution = 0.0;
		if (any(rectboxmin > HistoryColor) || any(HistoryColor > rectboxmax)) 
		{
			lerpcontribution = max(fLumaInstabilityFactor, fLockContributionThisFrame);
			HistoryColor = lerp(clampedHistoryColor, HistoryColor, saturate(lerpcontribution));
			half3 fAccumulationMin = min(fAccumulation, half3(0.1, 0.1, 0.1));
			fAccumulation = lerp(fAccumulationMin, fAccumulation, saturate(lerpcontribution));
		}

		////blend color
		fAccumulation = max(half3(1e-03, 1e-03, 1e-03), fAccumulation + UpsampledColorAndWeight.www);

		UpsampledColorAndWeight.xyz = RGBToYCoCg(Tonemap(YCoCgToRGB(UpsampledColorAndWeight.xyz)));
		HistoryColor = RGBToYCoCg(Tonemap(YCoCgToRGB(HistoryColor)));

		half3 fAlpha = UpsampledColorAndWeight.www / fAccumulation;
		HistoryColor = lerp(HistoryColor, UpsampledColorAndWeight.xyz, fAlpha);

		HistoryColor = YCoCgToRGB(HistoryColor);

		HistoryColor = InverseTonemap(HistoryColor);
	}

	HistoryColor = UnprepareRgb(HistoryColor, half(PreExposure));
#if GSR_OPTION_PIXEL_LOCK
	FinalizeLockStatus(PrevUV, Motion, int2(DisThreadID), fReprojectedLockStatus, UpsampledColorAndWeight.w, fHrVelocity);
#endif
	fTemporalReactiveFactor = ComputeTemporalReactiveFactor(fThisFrameReactiveFactor, fDepthFactor, fHrVelocity, bIsNewSample);
	HistoryOutput[DisThreadID] = half4(HistoryColor, fTemporalReactiveFactor);	

#if GSR_OPTION_APPLY_SHARPENING == 0
	SceneColorOutput[DisThreadID] = HistoryColor;
#endif
}
