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
https://github.com/GPUOpen-Effects/FidelityFX-FSR2/blob/master/src/ffx-fsr2-api/shaders/ffx_fsr2_reconstruct_dilated_velocity_and_previous_depth.h
https://github.com/GPUOpen-Effects/FidelityFX-FSR/blob/master/ffx-fsr/ffx_fsr1.h
https://gpuopen.com/fidelityfx-superresolution-2/
under MIT license
*/

#define GSR_OPTION_INVERTED_DEPTH INVERTED_DEPTH
#define SIMPLIFIED_LUMA_CALCULATION (1)
#define GATHER_FINDNEARESTDEPTH (1)

Texture2D<half2> InputVelocity;
Texture2D<half> InputDepth;
Texture2D<half3> InputColor;
float PreExposure;
float4x4 ReClipToPrevClip;

SamplerState PointClamp;
SamplerState LinearClamp;
//Change encode 1 channel YCoCg directly to 3 channels YCoCg, it's hdr image and the values are not in [0,1]
RWTexture2D<half4> YCoCgLuma;
RWTexture2D<half4> DilatedMotionDepthLuma;


half ComputeLuma(half3 fRgb)
{
	half luma = dot(Tonemap(fRgb), half3(0.2126, 0.7152, 0.0722));
#if SIMPLIFIED_LUMA_CALCULATION
	return pow(luma, half(1.0 / 18.0));
#else
	half percievedLuma = 0.0;
	if (luma < half(216.0 / 24389.0))
	{
		percievedLuma = luma * half(24389.0 / 27.0);
	}
	else
	{
		percievedLuma = pow(luma, 1.0 / 3.0) * 116.0 - 16.0;
	}
	return pow(percievedLuma * 0.01, half(1.0 / 6.0));
#endif // SIMPLIFIED_LUMA_CALCULATION
}

half3 ComputePreparedInputColor(half3 fRgb, half fPreExposure)
{
	half3 fPreparedRgb = PrepareRgb(max(half3(0, 0, 0), fRgb), fPreExposure);
	half3 fPreparedYCoCg = RGBToYCoCg(fPreparedRgb);
	return fPreparedYCoCg;
}


#if GATHER_FINDNEARESTDEPTH

half FindNearestDepth(int2 iPxPos, int2 iPxSize, out int2 iNearestDepthCoord)
{
	float2 gatherCoord = (float2(iPxPos.x, iPxPos.y) + float2(0.5, 0.5) /* iPxPos is middle*/) * InputInfo_ViewportSizeInverse;
	half fDepths[9];
	half4 gDepths = InputDepth.GatherRed(PointClamp, gatherCoord, int16_t2(0,0));
	fDepths[0] = gDepths.x;
	fDepths[1] = gDepths.y;
	fDepths[2] = gDepths.z;
	fDepths[3] = gDepths.w;
	gDepths = InputDepth.GatherRed(PointClamp, gatherCoord, int16_t2(-1, -1));

	fDepths[4] = gDepths.x;
	//fDepths[3] = gDepths.y;	// would be same as fDepths[3] - ie center
	fDepths[5] = gDepths.z;
	fDepths[6] = gDepths.w;

	fDepths[7] = InputDepth.GatherRed(PointClamp, gatherCoord, int16_t2(0, -1)).z;

	fDepths[8] = InputDepth.GatherRed(PointClamp, gatherCoord, int16_t2(-1, 0)).x;

	half fNearestDepth;
	const int iSampleCount = 9;
	const int2 iSampleOffsets[iSampleCount] = {
		int2(0, 1),
		int2(1, 1),
		int2(1, 0),
		int2(0, 0),

		int2(-1, 0),
		//int2(0, 0),
		int2(0, -1),
		int2(-1, -1),

		int2(1, -1),	// top right
		int2(-1, 1),	// bottom left
	};

	int iNearestIndex = 0;

	fNearestDepth = fDepths[0];
	for (int iSampleIndex = 1; iSampleIndex < iSampleCount; ++iSampleIndex)
	{
		//if (all(iPos < iPxSize))
		{
			half fNdDepth = fDepths[iSampleIndex].x;
	#if GSR_OPTION_INVERTED_DEPTH
			if (fNdDepth > fNearestDepth)
	#else
			if (fNdDepth < fNearestDepth)
	#endif
			{
				fNearestDepth = fNdDepth;
				iNearestIndex = iSampleIndex;
			}
		}
	}

	iNearestDepthCoord = iPxPos + iSampleOffsets[iNearestIndex];

	return fNearestDepth;
}


#else // GATHER_FINDNEARESTDEPTH

half FindNearestDepth(uint16_t2 iPxPos, uint16_t2 iPxSize, out int16_t2 fNearestDepthCoord)
{
	half fNearestDepth;

	const int iSampleCount = 8;
	const int16_t2 iSampleOffsets[iSampleCount] = {
		int16_t2(+1, +0),
		int16_t2(+0, +1),
		int16_t2(+0, -1),
		int16_t2(-1, +0),
		int16_t2(-1, +1),
		int16_t2(+1, +1),
		int16_t2(-1, -1),
		int16_t2(+1, -1),
	};

	int iSampleIndex = 0;

	int16_t2 fNearestDepthOffset = int16_t2(0,0);
	fNearestDepth = InputDepth.Load(int16_t3(iPxPos,0)).x;
	[unroll] 
	for (iSampleIndex = 0; iSampleIndex < iSampleCount; ++iSampleIndex)
	{
		int16_t2 iOffset = iSampleOffsets[iSampleIndex];

		//int2 iPos = iPxPos + iSampleOffsets[iSampleIndex];
		//if (all(iPos < iPxSize))
		{
			float fNdDepth = InputDepth.Load(int16_t3(iPxPos, 0), iOffset).x;
		#if GSR_OPTION_INVERTED_DEPTH
			if (fNdDepth > fNearestDepth)
		#else
			if (fNdDepth < fNearestDepth)
		#endif
			{
				fNearestDepthOffset = iOffset;
				fNearestDepth = fNdDepth;
			}
		}
	}
	fNearestDepthCoord = clamp(int16_t2(iPxPos) + fNearestDepthOffset, 0, int16_t2(iPxSize) - 1);
	return fNearestDepth;
}

#endif // GATHER_FINDNEARESTDEPTH


void Convert(uint16_t2 DisThreadID)
{
	int2 fNearestDepthCoord;
	half NearestZ = FindNearestDepth(DisThreadID, uint16_t2(DepthInfo_ViewportSize), fNearestDepthCoord);
	#if COMPILER_GLSL_ES3_1
	uint4 EncodedVelocity;
	float2 gatherCoord = float2(fNearestDepthCoord) * InputInfo_ViewportSizeInverse;
	EncodedVelocity.x = InputVelocity.GatherRed(PointClamp, gatherCoord).y;
	EncodedVelocity.y = InputVelocity.GatherGreen(PointClamp, gatherCoord).y;
	#else
	half4 EncodedVelocity = InputVelocity[fNearestDepthCoord].xyxy;
	#endif

	float2 motion;
	if (EncodedVelocity.x > 0.0)
	{
		motion = DecodeVelocityFromTexture(EncodedVelocity).xy;
	}
	else
	{
		float2 ViewportUV = (float2(fNearestDepthCoord) + 0.5) * InputInfo_ViewportSizeInverse;
		float2 ScreenPos = float2(2 * ViewportUV.x - 1, 1 - 2 * ViewportUV.y);
		float3 Position = float3(ScreenPos, NearestZ);
		float4 CurClip = float4(Position, 1);
		float4 PreClip = mul(CurClip, ReClipToPrevClip);
		float2 PreScreen = PreClip.xy / PreClip.w;
		motion = Position.xy - PreScreen;
	}
	motion *= float2(-0.5, 0.5);
	half3 ColorPostAlpha = InputColor[DisThreadID].xyz;
	
	//Divided by PreExposure and convert to YCoCg, it's hdr image and the values are not in [0,1]
	half3 ycocg = ComputePreparedInputColor(ColorPostAlpha, half(PreExposure)).xyz;
	YCoCgLuma[DisThreadID] = half4(ycocg, 0.0);
	half3 fPreparedRgb = max(half3(0, 0, 0), ColorPostAlpha) / half(PreExposure);
	half fLuma = ComputeLuma(fPreparedRgb);
	DilatedMotionDepthLuma[DisThreadID] = half4(motion, NearestZ, fLuma);
}
