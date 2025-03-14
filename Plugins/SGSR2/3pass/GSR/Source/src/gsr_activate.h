//============================================================================================================
//
//
//                  Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
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

Texture2D<uint> YCoCgColor;
Texture2D<half4> MotionDepthAlphaBuffer;

float ValidReset;
float AngleVertical;
SamplerState PointClamp1;
SamplerState PointClamp2;
SamplerState PointClamp3;

Texture2D<uint> PrevLumaHistory;
RWTexture2D<uint> LumaHistory;

RWTexture2D<half4> MotionDepthClipAlphaBuffer;

#define EPSILON 1.19e-07f /*1.192092896e-07f*/

float DecodeColorY(uint sample32)
{
	uint x11 = sample32 >> 21;
	return ((float)x11 * (1.0 / 2047.5));
}

void Activate(uint2 DisThreadID)
{
	const int2 sampleOffset[4] = {
		int2(0, 0),
		int2(0, 1),
		int2(1, 0),
		int2(1, 1),
	};

	float2 ViewportUV = (float2(DisThreadID) + 0.5f) * InputInfo_ViewportSizeInverse;
	float2 gatherCoord = ViewportUV + 0.5f * InputInfo_ViewportSizeInverse;
	uint luma_reference32 = YCoCgColor.GatherRed(PointClamp1, gatherCoord).w;
	float luma_reference = DecodeColorY(luma_reference32);

	float4 mda = MotionDepthAlphaBuffer[DisThreadID].xyzw; //motion depth alpha
	float depth = frac(mda.z);
	float depth_base = mda.z - depth;
	float alphamask = mda.w;
	float2 motion = mda.xy;

	float2 PrevUV;
	PrevUV.x = -0.5f * motion.x + ViewportUV.x;
	PrevUV.y = 0.5f * motion.y + ViewportUV.y;

	float depthclip = 0.0;

	if (depth > 1.0e-05f)
	{

		float2 Prevf_sample = PrevUV * InputInfo_ViewportSize - 0.5f;
		float2 Prevfrac = Prevf_sample - floor(Prevf_sample);

		float OneMinusPrevfacx = (1 - Prevfrac.x);
		float Bilinweights[4] = {
			OneMinusPrevfacx - OneMinusPrevfacx * Prevfrac.y,
			(Prevfrac.x - Prevfrac.x * Prevfrac.y),
			OneMinusPrevfacx * (Prevfrac.y),
			(Prevfrac.x) * (Prevfrac.y)
		};
		float Wdepth = 0.f;
		float Wsum = 0.f;
		float Ksep = 1.37e-05f;
		float Kfov = AngleVertical; /** (InputInfo_ViewportSize.x / InputInfo_ViewportSize.y)*/
		float diagonal_length = length(float2(InputInfo_ViewportSize));
		float Ksep_Kfov_diagonal = Ksep * Kfov * diagonal_length;
		for (int index = 0; index < 4; index += 2)
		{
			float4 gPrevdepth = MotionDepthAlphaBuffer.GatherBlue(PointClamp2, PrevUV, sampleOffset[index]);
			float tdepth1 = max(frac(gPrevdepth.x), frac(gPrevdepth.y));
			float tdepth2 = max(frac(gPrevdepth.z), frac(gPrevdepth.w));
			float fPrevdepth = max(tdepth1, tdepth2);

			float Depthsep = Ksep_Kfov_diagonal * max(fPrevdepth, depth);
			float weight = Bilinweights[index];
			Wdepth += saturate(Depthsep / (abs(fPrevdepth - depth) + EPSILON)) * weight;

			float2 gPrevdepth2 = MotionDepthAlphaBuffer.GatherBlue(PointClamp2, PrevUV, sampleOffset[index + 1]).xy;
			fPrevdepth = max(max(frac(gPrevdepth2.x), frac(gPrevdepth2.y)), tdepth1);
			Depthsep = Ksep_Kfov_diagonal * max(fPrevdepth, depth);
			weight = Bilinweights[index + 1];
			Wdepth += saturate(Depthsep / (abs(fPrevdepth - depth) + EPSILON)) * weight;
		}
		depthclip = saturate(1.0f - Wdepth);
	}

	float2 current_luma_diff;
	uint prev_lumadiff_pack = PrevLumaHistory.GatherRed(PointClamp3, PrevUV).w;
	float2 prev_luma_diff;

	prev_luma_diff.x = f16tof32(prev_lumadiff_pack >> 16);
	prev_luma_diff.y = f16tof32(prev_lumadiff_pack & 0xffff);

	bool enable = false;
	if (depthclip + ValidReset < 0.1f)
	{
		enable = all(PrevUV >= 0.0f) && all(PrevUV <= 1.0f);
	}

	float luma_diff = luma_reference - prev_luma_diff.x;
	if (!enable)
	{
		current_luma_diff.x = 0.0f;
		current_luma_diff.y = 0.0f;
	}
	else
	{
		current_luma_diff.x = luma_reference;
		current_luma_diff.y = prev_luma_diff.y != 0.0f ? (sign(luma_diff) == sign(prev_luma_diff.y) ? sign(luma_diff) * min(abs(prev_luma_diff.y), abs(luma_diff)) : prev_luma_diff.y) : luma_diff;
	}

	alphamask = floor(alphamask) + 0.5f * (float)((current_luma_diff.x != 0.0f) && (abs(current_luma_diff.y) != abs(luma_diff)));

	uint pack = (f32tof16(current_luma_diff.x) << 16) | f32tof16(current_luma_diff.y);
	depthclip = depthclip + depth_base;
	LumaHistory[DisThreadID] = pack;
	MotionDepthClipAlphaBuffer[DisThreadID] = float4(motion, depthclip, alphamask);
}
