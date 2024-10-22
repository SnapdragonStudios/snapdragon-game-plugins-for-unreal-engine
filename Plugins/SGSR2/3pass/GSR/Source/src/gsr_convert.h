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

Texture2D InputDepth;

// #if COMPILER_GLSL_ES3_1
Texture2D InputVelocity;
//SamplerState PointClamp_Velocity;
//#else
Texture2D<uint4> InputVelocitySRV;
//#endif
// Texture2D InputVelocity;

Texture2D InputColor;
Texture2D InputOpaqueColor;
half Exposure_co_rcp;

SamplerState PointClamp;
SamplerState PointClamp_Velocity;

RWTexture2D<uint> YCoCgColor;
RWTexture2D<half4> MotionDepthAlphaBuffer;

void Convert(uint2 DisThreadID)
{
	int2 PixelOffset;
	PixelOffset.x = 0; //store int2(1, 1) - int2(1, 1)
	float2 gatherCoord = float2(DisThreadID) * InputInfo_ViewportSizeInverse;
	float2 ViewportUV = gatherCoord + float2(0.5f, 0.5f) * InputInfo_ViewportSizeInverse;
	gatherCoord = float2(DisThreadID) * DepthInfo_ViewportSizeInverse;

	int2 InputPosBtmRight = int2(1, 1) + DisThreadID;
	float NearestZ = InputDepth[InputPosBtmRight].x; //bottom right z
	float4 topleft = InputDepth.GatherRed(PointClamp, gatherCoord);
	NearestZ = max(topleft.x, NearestZ);
	NearestZ = max(topleft.y, NearestZ);
	NearestZ = max(topleft.z, NearestZ);
	NearestZ = max(topleft.w, NearestZ);

	float2 topRight = InputDepth.GatherRed(PointClamp, gatherCoord + float2(InputInfo_ViewportSizeInverse.x, 0.0)).yz;
	NearestZ = max(topRight.x, NearestZ);
	NearestZ = max(topRight.y, NearestZ);

	float2 bottomLeft = InputDepth.GatherRed(PointClamp, gatherCoord + float2(0.0, InputInfo_ViewportSizeInverse.y)).xy;
	NearestZ = max(bottomLeft.x, NearestZ);
	NearestZ = max(bottomLeft.y, NearestZ);

// #if COMPILER_GLSL_ES3_1
// 	int2 Coord = int2(BufferUV * InputSceneColorSize.xy) + PixelOffset;
// 	return GBufferVelocityTextureSRV.Load(int3(Coord, 0));
// #else
// 	return GBufferVelocityTexture.SampleLevel(GBufferVelocityTextureSampler, BufferUV, 0, PixelOffset);
// #endif
//#if COMPILER_GLSL_ES3_1
    //uint4 EncodedVelocity = InputVelocitySRV.Load(int3(DisThreadID, 0));
	//EncodedVelocity.x = InputVelocitySRV.GatherRed(PointClamp_Velocity, gatherCoord).y;
	//EncodedVelocity.y = InputVelocitySRV.GatherGreen(PointClamp_Velocity, gatherCoord).y;
//	EncodedVelocity.zw = uint2(0, 0);



	//float4 EncodedVelocity = InputVelocity[DisThreadID];
#if COMPILER_GLSL_ES3_1
	uint4 EncodedVelocity = InputVelocitySRV.Load(int3(DisThreadID, 0));
#else
	float4 EncodedVelocity = InputVelocity.SampleLevel(PointClamp_Velocity, gatherCoord, 0, int2(0, 0)); 
#endif

	float2 motion;
	if (EncodedVelocity.x > 0.0)
	{
		motion = DecodeVelocityFromTexture(EncodedVelocity).xy;
	}
	else
	{
		float2 ScreenPos = float2(2 * ViewportUV.x - 1, 1 - 2 * ViewportUV.y);
		float3 Position = float3(ScreenPos, NearestZ);
		float4 CurClip = float4(Position, 1);
		float4 PreClip = mul(CurClip, View.ClipToPrevClip);
		float2 PreScreen = PreClip.xy / PreClip.w;
		motion = Position.xy - PreScreen;
	}

	float3 Colorrgb = InputColor[DisThreadID].xyz;
	///simple tonemap
	float ColorMax = (max(max(Colorrgb.x, Colorrgb.y), Colorrgb.z) + Exposure_co_rcp).x;
	Colorrgb /= ColorMax.xxx;

	float depth_bright = floor(ColorMax * 0.001f) + NearestZ;

	float3 Colorycocg;
	Colorycocg.x = 0.25f * (Colorrgb.x + 2.0f * Colorrgb.y + Colorrgb.z),
	Colorycocg.y = saturate(0.5f * Colorrgb.x + 0.5 - 0.5f * Colorrgb.z),
	Colorycocg.z = saturate(Colorycocg.x + Colorycocg.y - Colorrgb.x);

	//now color YCoCG all in the range of [0,1]
	uint x11 = Colorycocg.x * 2047.5;
	uint y11 = Colorycocg.y * 2047.5;
	uint z10 = Colorycocg.z * 1023.5;

	float3 Colorprergb = InputOpaqueColor[DisThreadID].xyz;
	///simple tonemap
	Colorprergb /= (max(max(Colorprergb.x, Colorprergb.y), Colorprergb.z) + Exposure_co_rcp).xxx;
	float3 delta = abs(Colorrgb - Colorprergb);
	float alpha_mask = max(delta.x, max(delta.y, delta.z));
	alpha_mask = (0.35f * 1000.0f) * alpha_mask;

	YCoCgColor[DisThreadID] = (x11 << 21) | (y11 << 10) | z10;
	MotionDepthAlphaBuffer[DisThreadID] = half4(motion, depth_bright, alpha_mask);
}
