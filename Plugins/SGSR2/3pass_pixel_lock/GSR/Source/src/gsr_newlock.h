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
Kurt Akeley and Jonathan Su, "Minimum Triangle Separation for Correct Z-Buffer Occlusion", 
http://www.cs.cmu.edu/afs/cs/academic/class/15869-f11/www/readings/akeley06_triseparation.pdf
https://github.com/GPUOpen-Effects/FidelityFX-FSR2/blob/master/src/ffx-fsr2-api/shaders/ffx_fsr2_depth_clip.h
under MIT license
*/

#define GSR_OPTION_INVERTED_DEPTH INVERTED_DEPTH

Texture2D<half4> DilatedMotionDepthLuma;
SamplerState PointClamp;
RWTexture2D<half> NewLocks;
RWTexture2D<uint> ReconstructedPreviousNearestDepth;
//RWTexture2D<half4> DebugOutput;


int2 ComputeHrPosFromLrPos(int2 iPxLrPos, float2 jitter)
{
	float2 fSrcJitteredPos = float2(iPxLrPos) + 0.5f - jitter;
	float2 fLrPosInHr = (fSrcJitteredPos / InputInfo_ViewportSize) * HistoryInfo_ViewportSize;
	int2 iPxHrPos = int2(floor(fLrPosInHr));
	return iPxHrPos;
}
 
void StoreNewLocks(uint2 iPxPos, half newLock)
{
	NewLocks[iPxPos] = newLock;
}

bool ComputeThinFeatureConfidence(int2 pos){
	float2 gatherCoord = float2(pos) / float2(InputInfo_ViewportSize);
	float4 topeLeftLumas = DilatedMotionDepthLuma.GatherAlpha(PointClamp, gatherCoord);
	half fNucleus = topeLeftLumas.y;

	
    /*  0 1 2
		3 4 5
		6 7 8  */

    #define SETBIT(x) (1U << x)
	uint mask = SETBIT(4); // flag fNucleus as similar
	
	const uint uNumRejectionMasks = 4;
	const uint uRejectionMasks[uNumRejectionMasks] = {
		SETBIT(0) | SETBIT(1) | SETBIT(3) | SETBIT(4), //Upper left
		SETBIT(1) | SETBIT(2) | SETBIT(4) | SETBIT(5), //Upper right
		SETBIT(3) | SETBIT(4) | SETBIT(6) | SETBIT(7), //Lower left
		SETBIT(4) | SETBIT(5) | SETBIT(7) | SETBIT(8), //Lower right
	};

	half similar_threshold = 1.05f;
	half dissimilarLumaMin = FLT_MAX;
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


	gatherCoord = float2(pos + int2(1, 1)) / float2(InputInfo_ViewportSize);
	float4 bottomRightLumas = DilatedMotionDepthLuma.GatherAlpha(PointClamp, gatherCoord);
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
	if ( !isRidge || ((mask & uRejectionMasks[2]) == uRejectionMasks[2]))
	{
		return false;
	}
	
	return true;
}

void NewLock(uint2 DisThreadID)
{
	if (ComputeThinFeatureConfidence(DisThreadID))
	{
		NewLocks[DisThreadID] = 2.0f;		
	}
	#if GSR_OPTION_INVERTED_DEPTH
	ReconstructedPreviousNearestDepth[DisThreadID] = 0x0;
	#else
	ReconstructedPreviousNearestDepth[DisThreadID] = 0x3f800000;
	#endif
}
