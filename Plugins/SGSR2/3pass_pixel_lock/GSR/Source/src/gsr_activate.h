//============================================================================================================
//
//
//                  Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
/*
**
** reference from
Kurt Akeley and Jonathan Su, "Minimum Triangle Separation for Correct Z-Buffer Occlusion", 
http://www.cs.cmu.edu/afs/cs/academic/class/15869-f11/www/readings/akeley06_triseparation.pdf
https://github.com/GPUOpen-Effects/FidelityFX-FSR2/blob/master/src/ffx-fsr2-api/shaders/ffx_fsr2_depth_clip.h
under MIT license
*/

#define GSR_OPTION_INVERTED_DEPTH INVERTED_DEPTH
#define GSR_OPTION_PIXEL_LOCK ENABLE_PIXEL_LOCK

Texture2D<half4> DilatedMotionDepthLuma;

float4 DeviceToViewDepth;
SamplerState PointClamp;
SamplerState LinearClamp;

RWTexture2D<half> ReactiveMask;
RWTexture2D<half> NewLocks;

float GetViewSpaceDepth(float fDeviceDepth)
{
	return DeviceToViewDepth[1]/(fDeviceDepth-DeviceToViewDepth[0]);
}

float3 GetViewSpacePosition(float2 iViewportPos, int2 iViewportSize, float fDeviceDepth)
{
	float Z = GetViewSpaceDepth(fDeviceDepth);
	//ComputeNdc
	float2 fNdcPos = iViewportPos/float2(iViewportSize)*float2(2.0,-2.0)+float2(-1.0,1.0);
	float X = DeviceToViewDepth[2]*fNdcPos.x*Z;
	float Y = DeviceToViewDepth[3]*fNdcPos.y*Z;
	return float3(X,Y,Z);
}

float EvaluateSurface(int2 iPxPos, float2 fMotionVector)
{
	float d0 = GetViewSpaceDepth((DilatedMotionDepthLuma[iPxPos + int2(0, -1)].z));
	float d1 = GetViewSpaceDepth((DilatedMotionDepthLuma[iPxPos + int2(0, 0)].z));
	float d2 = GetViewSpaceDepth((DilatedMotionDepthLuma[iPxPos + int2(0, 1)].z));

	return 1.0 - float(((d0 - d1) > (d1 * 0.01)) && ((d1 - d2) > (d2 * 0.01)));
}

float ComputeDepthClip(float2 fUvSample,float fCurrentDepthSample)
{
	float fCurrentDepthViewSpace = GetViewSpaceDepth(fCurrentDepthSample);
	int2 iOffsets[4] = { int2(0, 0), int2(1, 0), int2(0, 1), int2(1, 1) };
	float fWeights[4];

	float2 fPxSample = (fUvSample * InputInfo_ViewportSize) - float2(0.5, 0.5);
	int2 iBasePos = int2(floor(fPxSample));
	float2 fPxFrac = frac(fPxSample);

	fWeights[0] = (1 - fPxFrac.x) * (1 - fPxFrac.y);
	fWeights[1] = (fPxFrac.x) * (1 - fPxFrac.y);
	fWeights[2] = (1 - fPxFrac.x) * (fPxFrac.y);
	fWeights[3] = (fPxFrac.x) * (fPxFrac.y);
	
	float fDilatedSum = 0.0;
	float fDepth = 0.0;
	float fWeightSum = 0.0;
	[unroll] 
	for (int iSampleIndex = 0; iSampleIndex < 4; iSampleIndex++)
	{
		const int2 iOffset = iOffsets[iSampleIndex];
		const int2 iSamplePos = iBasePos + iOffset;

		if (all(iSamplePos < InputInfo_ViewportSize))
		{
			const float fWeight = fWeights[iSampleIndex];
			if (fWeight > 0.01)
			{
				const float fPrevDepthSample = DilatedMotionDepthLuma[iSamplePos].z;
				const float fPrevNearestDepthViewSpace = GetViewSpaceDepth(fPrevDepthSample);

				const float fDepthDiff = fCurrentDepthViewSpace - fPrevNearestDepthViewSpace;

				if (fDepthDiff > 0.0)
				{
				#if GSR_OPTION_INVERTED_DEPTH
					const float fPlaneDepth = min(fPrevDepthSample, fCurrentDepthSample);
				#else
					const float fPlaneDepth = max(fPrevDepthSample, fCurrentDepthSample);
				#endif

					const float3 fCenter = GetViewSpacePosition(int2(InputInfo_ViewportSize * 0.5), InputInfo_ViewportSize, fPlaneDepth);
					const float3 fCorner = GetViewSpacePosition(int2(0, 0), InputInfo_ViewportSize, fPlaneDepth);

					const float fHalfViewportWidth = length(float2(InputInfo_ViewportSize));
					const float fDepthThreshold = max(fCurrentDepthViewSpace, fPrevNearestDepthViewSpace);

					const float Ksep = 1.37e-05f;
					const float Kfov = length(fCorner) / length(fCenter);
					const float fRequiredDepthSeparation = Ksep * Kfov * fHalfViewportWidth * fDepthThreshold;

					const float fResolutionFactor = saturate(length(float2(InputInfo_ViewportSize)) / length(float2(1920.0f, 1080.0f)));
					const float fPower = lerp(1.0f, 3.0f, fResolutionFactor);
					fDepth += pow(saturate(float(fRequiredDepthSeparation / fDepthDiff)), fPower) * fWeight;
					fWeightSum += fWeight;
				}
			}
		}
	}
	return (fWeightSum > 0) ? saturate(1.0f - fDepth / fWeightSum) : 0.0f;
}


int2 ComputeHrPosFromLrPos(int2 iPxLrPos, float2 jitter)
{
	float2 fSrcJitteredPos = float2(iPxLrPos) + 0.5f - jitter;
	float2 fLrPosInHr = (fSrcJitteredPos / InputInfo_ViewportSize) * HistoryInfo_ViewportSize;
	int2 iPxHrPos = int2(floor(fLrPosInHr));
	return iPxHrPos;
}

bool ComputeThinFeatureConfidence(int2 pos)
{
	float2 gatherCoord = float2(pos) / float2(InputInfo_ViewportSize);
	half4 topeLeftLumas = DilatedMotionDepthLuma.GatherAlpha(PointClamp, gatherCoord);
	half fNucleus = topeLeftLumas.y;

	/*  0 1 2
		3 4 5
		6 7 8  */

#define SETBIT(x) (1U << x)
	uint mask = SETBIT(4); // flag fNucleus as similar

	const uint uNumRejectionMasks = 4;
	const uint uRejectionMasks[uNumRejectionMasks] = {
		SETBIT(0) | SETBIT(1) | SETBIT(3) | SETBIT(4), // Upper left
		SETBIT(1) | SETBIT(2) | SETBIT(4) | SETBIT(5), // Upper right
		SETBIT(3) | SETBIT(4) | SETBIT(6) | SETBIT(7), // Lower left
		SETBIT(4) | SETBIT(5) | SETBIT(7) | SETBIT(8), // Lower right
	};

	half similar_threshold = 1.05;
	half dissimilarLumaMin = half(FLT_FP16_MAX);
	half dissimilarLumaMax = 0;

	/////// luma at position 0  /////
	int idx = 0;
	half sampleLuma = topeLeftLumas.w;
	half difference = max(sampleLuma, fNucleus) / min(sampleLuma, fNucleus);
	if (difference > 0 && (difference < similar_threshold))
	{
		mask |= SETBIT(idx);
	}
	else
	{
		dissimilarLumaMin = min(dissimilarLumaMin, sampleLuma);
		dissimilarLumaMax = max(dissimilarLumaMax, sampleLuma);
	}

	/////// luma at position 1  /////
	idx = 1;
	sampleLuma = topeLeftLumas.z;
	difference = max(sampleLuma, fNucleus) / min(sampleLuma, fNucleus);
	if (difference > 0 && (difference < similar_threshold))
	{
		mask |= SETBIT(idx);
	}
	else
	{
		dissimilarLumaMin = min(dissimilarLumaMin, sampleLuma);
		dissimilarLumaMax = max(dissimilarLumaMax, sampleLuma);
	}

	/////// luma at position 3  /////
	idx = 3;
	sampleLuma = topeLeftLumas.x;
	difference = max(sampleLuma, fNucleus) / min(sampleLuma, fNucleus);
	if (difference > 0 && (difference < similar_threshold))
	{
		mask |= SETBIT(idx);
	}
	else
	{
		dissimilarLumaMin = min(dissimilarLumaMin, sampleLuma);
		dissimilarLumaMax = max(dissimilarLumaMax, sampleLuma);
	}

	/////// early skip after topleft  /////
	if ((mask & uRejectionMasks[0]) == uRejectionMasks[0])
	{
		return false;
	}

	gatherCoord = float2(pos) / float2(InputInfo_ViewportSize);
	half4 bottomRightLumas = DilatedMotionDepthLuma.GatherAlpha(PointClamp, gatherCoord, int2(1, 1));
	/////// luma at position 5  /////
	idx = 5;
	sampleLuma = bottomRightLumas.z;
	difference = max(sampleLuma, fNucleus) / min(sampleLuma, fNucleus);
	if (difference > 0 && (difference < similar_threshold))
	{
		mask |= SETBIT(idx);
	}
	else
	{
		dissimilarLumaMin = min(dissimilarLumaMin, sampleLuma);
		dissimilarLumaMax = max(dissimilarLumaMax, sampleLuma);
	}
	/////// luma at position 7  /////
	idx = 7;
	sampleLuma = bottomRightLumas.x;
	difference = max(sampleLuma, fNucleus) / min(sampleLuma, fNucleus);
	if (difference > 0 && (difference < similar_threshold))
	{
		mask |= SETBIT(idx);
	}
	else
	{
		dissimilarLumaMin = min(dissimilarLumaMin, sampleLuma);
		dissimilarLumaMax = max(dissimilarLumaMax, sampleLuma);
	}
	/////// luma at position 8  /////
	idx = 8;
	sampleLuma = bottomRightLumas.y;
	difference = max(sampleLuma, fNucleus) / min(sampleLuma, fNucleus);
	if (difference > 0 && (difference < similar_threshold))
	{
		mask |= SETBIT(idx);
	}
	else
	{
		dissimilarLumaMin = min(dissimilarLumaMin, sampleLuma);
		dissimilarLumaMax = max(dissimilarLumaMax, sampleLuma);
	}

	/////// early skip after bottom right  /////
	if ((mask & uRejectionMasks[3]) == uRejectionMasks[3])
	{
		return false;
	}

	/////// luma at position 2  /////
	idx = 2;
	sampleLuma = DilatedMotionDepthLuma[pos + int2(1, -1)].w;
	difference = max(sampleLuma, fNucleus) / min(sampleLuma, fNucleus);
	if (difference > 0 && (difference < similar_threshold))
	{
		mask |= SETBIT(idx);
	}
	else
	{
		dissimilarLumaMin = min(dissimilarLumaMin, sampleLuma);
		dissimilarLumaMax = max(dissimilarLumaMax, sampleLuma);
	}
	/////// early skip after top right  /////
	if ((mask & uRejectionMasks[1]) == uRejectionMasks[1])
	{
		return false;
	}

	/////// luma at position 6  /////
	idx = 6;
	sampleLuma = DilatedMotionDepthLuma[pos + int2(-1, 1)].w;
	difference = max(sampleLuma, fNucleus) / min(sampleLuma, fNucleus);
	if (difference > 0 && (difference < similar_threshold))
	{
		mask |= SETBIT(idx);
	}
	else
	{
		dissimilarLumaMin = min(dissimilarLumaMin, sampleLuma);
		dissimilarLumaMax = max(dissimilarLumaMax, sampleLuma);
	}
	///////bottom left & ridge /////
	bool isRidge = (fNucleus > dissimilarLumaMax) || (fNucleus < dissimilarLumaMin);
	if (!isRidge || ((mask & uRejectionMasks[2]) == uRejectionMasks[2]))
	{
		return false;
	}

	return true;
}

void Activate(uint2 DisThreadID)
{
	float2 ViewportUV = (float2(DisThreadID) + 0.5f) * InputInfo_ViewportSizeInverse;

	float3 motionDepth = DilatedMotionDepthLuma[DisThreadID].xyz;
	float2 motion = motionDepth.xy;
	float depth = motionDepth.z;

	float2 motionClamped = motion * float(length(motion * HistoryInfo_ViewportSize) > 0.01f);
	float2 PrevUV;
	
	PrevUV.x = motionClamped.x + ViewportUV.x;
	PrevUV.y = motionClamped.y + ViewportUV.y;
	
	float depthclip = depth > FLT_FP16_MIN ? ComputeDepthClip(PrevUV, depth) * EvaluateSurface(ClampLoad(int2(PrevUV * DepthInfo_ViewportSize), int2(0, 0), int2(DepthInfo_ViewportSize)), motionClamped) : 0.0f; // ignore minor depth for half precision
	ReactiveMask[DisThreadID] = depthclip;
#if GSR_OPTION_PIXEL_LOCK
	if (ComputeThinFeatureConfidence(DisThreadID))
	{
		NewLocks[ComputeHrPosFromLrPos(DisThreadID, InputJitter)] = 2.0;
	}
#endif
}
