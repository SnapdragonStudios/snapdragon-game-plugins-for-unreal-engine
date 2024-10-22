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

Texture2D InputVelocity;
Texture2D InputDepth;
Texture2D InputColor;
float Exposure_co_rcp;
float AngleVertical;
SamplerState PointClamp;
SamplerState PointClamp_Velocity;

RWTexture2D<uint> YCoCgColor;
RWTexture2D<half4> MotionDepthClipAlphaBuffer;

#define EPSILON 1.19e-07f /*1.192092896e-07f*/
void Convert(uint2 DisThreadID)
{
	int2 PixelOffset;
	PixelOffset.x = 0; //store int2(1, 1) - int2(1, 1)
	float2 gatherCoord = float2(DisThreadID) * InputInfo_ViewportSizeInverse;
	float2 ViewportUV = gatherCoord + float2(0.5f, 0.5f) * InputInfo_ViewportSizeInverse;
	gatherCoord = float2(DisThreadID) * DepthInfo_ViewportSizeInverse;

	float4 topleft = InputDepth.GatherRed(PointClamp, gatherCoord);
	float4 topRight = InputDepth.GatherRed(PointClamp, gatherCoord + float2(InputInfo_ViewportSizeInverse.x * 2.0, 0.0)).xyzw;
	float4 bottomLeft = InputDepth.GatherRed(PointClamp, gatherCoord + float2(0.0, InputInfo_ViewportSizeInverse.y * 2.0)).xyzw;
	float4 bottomRight = InputDepth.GatherRed(PointClamp, gatherCoord + float2(InputInfo_ViewportSizeInverse.x * 2.0, InputInfo_ViewportSizeInverse.y * 2.0)).xyzw;

	float maxC = max(max(max(topleft.y, topRight.x), bottomLeft.z), bottomRight.w);
	float topleft4 = max(max(max(topleft.y, topleft.x), topleft.z), topleft.w);
	float topLeftMax9 = max(bottomLeft.w, max(max(maxC, topleft4), topRight.w));

	float depthclip = 0.0;
	if (maxC > 1.0e-05f)
	{
		float topRight4 = max(max(max(topRight.y, topRight.x), topRight.z), topRight.w);
		float bottomLeft4 = max(max(max(bottomLeft.y, bottomLeft.x), bottomLeft.z), bottomLeft.w);
		float bottomRight4 = max(max(max(bottomRight.y, bottomRight.x), bottomRight.z), bottomRight.w);

		float Wdepth = 0.f;
		float Ksep = 1.37e-05f;
		float Kfov = AngleVertical; /** (InputInfo_ViewportSize.x / InputInfo_ViewportSize.y)*/
		float diagonal_length = length(float2(InputInfo_ViewportSize));
		float Ksep_Kfov_diagonal = Ksep * Kfov * diagonal_length;

		float Depthsep = Ksep_Kfov_diagonal * maxC;
		Wdepth += saturate(Depthsep / (abs(maxC - topleft4) + EPSILON));
		Wdepth += saturate(Depthsep / (abs(maxC - topRight4) + EPSILON));
		Wdepth += saturate(Depthsep / (abs(maxC - bottomLeft4) + EPSILON));
		Wdepth += saturate(Depthsep / (abs(maxC - bottomRight4) + EPSILON));

		depthclip = saturate(1.0f - Wdepth * 0.25);
	}

#if COMPILER_GLSL_ES3_1
	uint4 EncodedVelocity;
	EncodedVelocity.x = InputVelocity.GatherRed(PointClamp_Velocity, gatherCoord).y;
	EncodedVelocity.y = InputVelocity.GatherGreen(PointClamp_Velocity, gatherCoord).y;
#else
	float4 EncodedVelocity = InputVelocity[DisThreadID];
#endif

	float2 motion;
	if (EncodedVelocity.x > 0.0f)
	{
		motion = DecodeVelocityFromTexture(EncodedVelocity).xy;
	}
	else
	{
		float2 ScreenPos = float2(2.0f * ViewportUV.x - 1.0f, 1.0f - 2.0f * ViewportUV.y);
		float3 Position = float3(ScreenPos, topLeftMax9);
		float4 CurClip = float4(Position, 1.0f);
		float4 PreClip = mul(CurClip, View.ClipToPrevClip);
		float2 PreScreen = PreClip.xy / PreClip.w;
		motion = Position.xy - PreScreen;
	}

	float3 Colorrgb = InputColor[DisThreadID].xyz;
	///simple tonemap
	float ColorMax = (max(max(Colorrgb.x, Colorrgb.y), Colorrgb.z) + Exposure_co_rcp).x;
	Colorrgb /= ColorMax.xxx;

	float3 Colorycocg;
	Colorycocg.x = 0.25f * (Colorrgb.x + 2.0f * Colorrgb.y + Colorrgb.z),
	Colorycocg.y = saturate(0.5f * Colorrgb.x + 0.5 - 0.5f * Colorrgb.z),
	Colorycocg.z = saturate(Colorycocg.x + Colorycocg.y - Colorrgb.x);

	//now color YCoCG all in the range of [0,1]
	uint x11 = Colorycocg.x * 2047.5f;
	uint y11 = Colorycocg.y * 2047.5f;
	uint z10 = Colorycocg.z * 1023.5f;

	YCoCgColor[DisThreadID] = (x11 << 21) | (y11 << 10) | z10;
	MotionDepthClipAlphaBuffer[DisThreadID] = half4(motion, depthclip, ColorMax);
}
