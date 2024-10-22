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

Texture2D<uint> YCoCgColor;
Texture2D<float4> MotionDepthClipAlphaBuffer;
Texture2D<float4> PrevHistoryOutput;

SamplerState PointClamp;
SamplerState LinearClamp1;
SamplerState LinearClamp2;
float Exposure_co_rcp;
float ValidReset;
float MinLerpContribution; // 0.0 or 0.3, when camera move 0.0, camera freeze 0.3
float Scalefactor;
float Biasmax_viewportXScale;

RWTexture2D<float4> HistoryOutput;
RWTexture2D<float3> SceneColorOutput;

float FastLanczos(float base)
{
	float y = base - 1.0f;
	float y2 = y * y;
	float y_temp = 0.75f * y + y2;
	return y_temp * y2;
}

float3 DecodeColor(uint sample32)
{
	uint x11 = sample32 >> 21;
	uint y11 = sample32 & (2047 << 10);
	uint z10 = sample32 & 1023;
	float3 samplecolor;
	samplecolor.x = ((float)x11 * (1.0f / 2047.5f));
	samplecolor.y = ((float)y11 * (4.76953602e-7f)) - 0.5f;
	samplecolor.z = ((float)z10 * (1.0f / 1023.5f)) - 0.5f;
	return samplecolor;
}

void Update(uint2 DisThreadID)
{
	float2 Hruv = (DisThreadID + 0.5f) * HistoryInfo_ViewportSizeInverse;
	float2 srcOutputPos = Hruv * InputInfo_ViewportSize;
	int2 InputPos = srcOutputPos;
	float2 Jitteruv;
	Jitteruv.x = saturate(Hruv.x + InputJitter.x * OutputInfo_ViewportSizeInverse.x);
	Jitteruv.y = saturate(Hruv.y + InputJitter.y * OutputInfo_ViewportSizeInverse.y);

	float4 mda = MotionDepthClipAlphaBuffer.SampleLevel(LinearClamp1, Jitteruv, 0).xyzw;

	float2 PrevUV;
	PrevUV.x = -0.5f * mda.x + Hruv.x;
	PrevUV.y = 0.5f * mda.y + Hruv.y;

	PrevUV.x = clamp(PrevUV.x, 0.0f, 1.0f);
	PrevUV.y = clamp(PrevUV.y, 0.0f, 1.0f);

	float depthclip = mda.z;
	float ColorMax = mda.w;

	float3 HistoryColor = PrevHistoryOutput.SampleLevel(LinearClamp2, PrevUV, 0).xyz;

	/////upsample and compute box
	float4 Upsampledcw = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float kernelfactor = ValidReset;
	float biasmax = Biasmax_viewportXScale - Biasmax_viewportXScale * kernelfactor;
	float biasmin = max(1.0f, 0.3f + 0.3f * biasmax);
	float biasfactor = max(0.25f * depthclip, kernelfactor);
	float kernelbias = lerp(biasmax, biasmin, biasfactor);
	float motion_viewport_len = length(mda.xy * HistoryInfo_ViewportSize);
	float curvebias = lerp(-2.0f, -3.0f, saturate(motion_viewport_len * 0.02f));

	float3 rectboxcenter = float3(0.0f, 0.0f, 0.0f);
	float3 rectboxvar = float3(0.0f, 0.0f, 0.0f);
	float rectboxweight = 0.0f;

	float2 InputPosf = float2(InputPos);
	float2 srcpos = InputPosf + float2(0.5f, 0.5f) - InputJitter; //fSrcUnjitteredPos

	float kernelbias2 = kernelbias * kernelbias * 0.25f;
	float2 srcpos_srcOutputPos = srcpos - srcOutputPos;

	int2 InputPosBtmRight = int2(1, 1) + InputPos;
	float2 gatherCoord = InputPosf * InputInfo_ViewportSizeInverse;
	uint4 topleft = YCoCgColor.GatherRed(PointClamp, gatherCoord);
	uint2 topRight;
	uint2 bottomLeft;

#if five_sample_use
	uint2 btmRight = YCoCgColor.GatherRed(PointClamp, gatherCoord + float2(InputInfo_ViewportSizeInverse.x, InputInfo_ViewportSizeInverse.y)).xz;
	bottomLeft.y = btmRight.x;
	topRight.x = btmRight.y;
#else
	topRight = YCoCgColor.GatherRed(PointClamp, gatherCoord + float2(InputInfo_ViewportSizeInverse.x, 0.0f)).yz;
	bottomLeft = YCoCgColor.GatherRed(PointClamp, gatherCoord + float2(0.0f, InputInfo_ViewportSizeInverse.y)).xy;
#endif

	float3 rectboxmin;
	float3 rectboxmax;
	{
		float3 samplecolor = DecodeColor(bottomLeft.y);
		float2 baseoffset = srcpos_srcOutputPos + float2(0.0f, 1.0f);
		float baseoffset_dot = dot(baseoffset, baseoffset);
		float base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
		float weight = FastLanczos(base);
		Upsampledcw = float4(samplecolor * weight, weight);
		float boxweight = exp(baseoffset_dot * curvebias);
		rectboxmin = samplecolor;
		rectboxmax = samplecolor;
		float3 wsample = samplecolor * boxweight;
		rectboxcenter = wsample;
		rectboxvar = (samplecolor * wsample);
		rectboxweight = boxweight;
	}
	{
		float3 samplecolor = DecodeColor(topRight.x);
		float2 baseoffset = srcpos_srcOutputPos + float2(1.0f, 0.0f);
		float baseoffset_dot = dot(baseoffset, baseoffset);
		float base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
		float weight = FastLanczos(base);
		Upsampledcw += float4(samplecolor * weight, weight);
		float boxweight = exp(baseoffset_dot * curvebias);
		rectboxmin = min(rectboxmin, samplecolor);
		rectboxmax = max(rectboxmax, samplecolor);
		float3 wsample = samplecolor * boxweight;
		rectboxcenter += wsample;
		rectboxvar += (samplecolor * wsample);
		rectboxweight += boxweight;
	}
	{
		float3 samplecolor = DecodeColor(topleft.x);
		float2 baseoffset = srcpos_srcOutputPos + float2(-1.0f, 0.0f);
		float baseoffset_dot = dot(baseoffset, baseoffset);
		float base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
		float weight = FastLanczos(base);
		Upsampledcw += float4(samplecolor * weight, weight);
		float boxweight = exp(baseoffset_dot * curvebias);
		rectboxmin = min(rectboxmin, samplecolor);
		rectboxmax = max(rectboxmax, samplecolor);
		float3 wsample = samplecolor * boxweight;
		rectboxcenter += wsample;
		rectboxvar += (samplecolor * wsample);
		rectboxweight += boxweight;
	}
	{
		float3 samplecolor = DecodeColor(topleft.y);
		float2 baseoffset = srcpos_srcOutputPos;
		float baseoffset_dot = dot(baseoffset, baseoffset);
		float base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
		float weight = FastLanczos(base);
		Upsampledcw += float4(samplecolor * weight, weight);
		float boxweight = exp(baseoffset_dot * curvebias);
		rectboxmin = min(rectboxmin, samplecolor);
		rectboxmax = max(rectboxmax, samplecolor);
		float3 wsample = samplecolor * boxweight;
		rectboxcenter += wsample;
		rectboxvar += (samplecolor * wsample);
		rectboxweight += boxweight;
	}
	{
		float3 samplecolor = DecodeColor(topleft.z);
		float2 baseoffset = srcpos_srcOutputPos + float2(0.0f, -1.0f);
		float baseoffset_dot = dot(baseoffset, baseoffset);
		float base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
		float weight = FastLanczos(base);
		Upsampledcw += float4(samplecolor * weight, weight);
		float boxweight = exp(baseoffset_dot * curvebias);
		rectboxmin = min(rectboxmin, samplecolor);
		rectboxmax = max(rectboxmax, samplecolor);
		float3 wsample = samplecolor * boxweight;
		rectboxcenter += wsample;
		rectboxvar += (samplecolor * wsample);
		rectboxweight += boxweight;
	}

#if !five_sample_use
	{
		uint btmRight = YCoCgColor[InputPosBtmRight].x;
		float3 samplecolor = DecodeColor(btmRight);
		float2 baseoffset = srcpos_srcOutputPos + float2(1.0f, 1.0f);
		float baseoffset_dot = dot(baseoffset, baseoffset);
		float base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
		float weight = FastLanczos(base);
		Upsampledcw += float4(samplecolor * weight, weight);
		float boxweight = exp(baseoffset_dot * curvebias);
		rectboxmin = min(rectboxmin, samplecolor);
		rectboxmax = max(rectboxmax, samplecolor);
		float3 wsample = samplecolor * boxweight;
		rectboxcenter += wsample;
		rectboxvar += (samplecolor * wsample);
		rectboxweight += boxweight;
	}
	{
		float3 samplecolor = DecodeColor(bottomLeft.x);
		float2 baseoffset = srcpos_srcOutputPos + float2(-1.0f, 1.0f);
		float baseoffset_dot = dot(baseoffset, baseoffset);
		float base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
		float weight = FastLanczos(base);
		Upsampledcw += float4(samplecolor * weight, weight);
		float boxweight = exp(baseoffset_dot * curvebias);
		rectboxmin = min(rectboxmin, samplecolor);
		rectboxmax = max(rectboxmax, samplecolor);
		float3 wsample = samplecolor * boxweight;
		rectboxcenter += wsample;
		rectboxvar += (samplecolor * wsample);
		rectboxweight += boxweight;
	}
	{
		float3 samplecolor = DecodeColor(topRight.y);
		float2 baseoffset = srcpos_srcOutputPos + float2(1.0f, -1.0f);
		float baseoffset_dot = dot(baseoffset, baseoffset);
		float base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
		float weight = FastLanczos(base);
		Upsampledcw += float4(samplecolor * weight, weight);
		float boxweight = exp(baseoffset_dot * curvebias);
		rectboxmin = min(rectboxmin, samplecolor);
		rectboxmax = max(rectboxmax, samplecolor);
		float3 wsample = samplecolor * boxweight;
		rectboxcenter += wsample;
		rectboxvar += (samplecolor * wsample);
		rectboxweight += boxweight;
	}
	{
		float3 samplecolor = DecodeColor(topleft.w);
		float2 baseoffset = srcpos_srcOutputPos + float2(-1.0f, -1.0f);
		float baseoffset_dot = dot(baseoffset, baseoffset);
		float base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
		float weight = FastLanczos(base);
		Upsampledcw += float4(samplecolor * weight, weight);
		float boxweight = exp(baseoffset_dot * curvebias);
		rectboxmin = min(rectboxmin, samplecolor);
		rectboxmax = max(rectboxmax, samplecolor);
		float3 wsample = samplecolor * boxweight;
		rectboxcenter += wsample;
		rectboxvar += (samplecolor * wsample);
		rectboxweight += boxweight;
	}
#endif
	
	rectboxweight = 1.0f / rectboxweight;
	rectboxcenter *= rectboxweight;
	rectboxvar *= rectboxweight;
	rectboxvar = sqrt(abs(rectboxvar - rectboxcenter * rectboxcenter));
	////Upsampledcw.xyz = (Upsampledcw.w > 0.00025f) ? clamp(Upsampledcw.xyz / Upsampledcw.w, rectboxmin, rectboxmax) : Upsampledcw.xyz;
	////Upsampledcw.w = (Upsampledcw.w > 0.00025f) ? Upsampledcw.w * (1.0f / 3.0f) : 0.0f;

	float3 bias = float3(0.05f, 0.05f, 0.05f);
	Upsampledcw.xyz = clamp(Upsampledcw.xyz / Upsampledcw.w, rectboxmin - bias, rectboxmax + bias);
	Upsampledcw.w = Upsampledcw.w * (1.0f / 3.0f);

	float baseupdate = 1.0 - depthclip;
	baseupdate = min(baseupdate, lerp(baseupdate, Upsampledcw.w * 10.0f, saturate(10.0f * motion_viewport_len)));
	baseupdate = min(baseupdate, lerp(baseupdate, Upsampledcw.w, saturate(motion_viewport_len * 0.05f)));
	float basealpha = baseupdate;

	const float EPSILON = 1.192e-07f /*1.192092896e-07f*/;
	float boxscale = max(depthclip, saturate(motion_viewport_len * 0.05f));
	float boxsize = lerp(Scalefactor, 1.0f, boxscale);

	float3 sboxvar = rectboxvar * boxsize;
	float3 boxmin = rectboxcenter - sboxvar;
	float3 boxmax = rectboxcenter + sboxvar;
	rectboxmax = min(rectboxmax, boxmax);
	rectboxmin = max(rectboxmin, boxmin);

	float3 clampedcolor = clamp(HistoryColor, rectboxmin, rectboxmax);
	float startLerpValue = MinLerpContribution;
	if ((abs(mda.x) + abs(mda.y)) > 0.000001f)
		startLerpValue = 0.0f;
	float lerpcontribution = (any(rectboxmin > HistoryColor) || any(HistoryColor > rectboxmax)) ? startLerpValue : 1.0f;

	HistoryColor = lerp(clampedcolor, HistoryColor, saturate(lerpcontribution));
	float basemin = min(basealpha, 0.1f);
	basealpha = lerp(basemin, basealpha, saturate(lerpcontribution));

	////blend color
	float alphasum = max(EPSILON, basealpha + Upsampledcw.w);
	float alpha = saturate(Upsampledcw.w / alphasum + ValidReset);

	Upsampledcw.xyz = lerp(HistoryColor, Upsampledcw.xyz, alpha.xxx);
	HistoryOutput[DisThreadID] = float4(Upsampledcw.xyz, 0.0f);
	////ycocg to grb
	float x_z = Upsampledcw.x - Upsampledcw.z;
	Upsampledcw.xyz = float3(
		saturate(x_z + Upsampledcw.y),
		saturate(Upsampledcw.x + Upsampledcw.z),
		saturate(x_z - Upsampledcw.y));

	float compMax = max(Upsampledcw.x, Upsampledcw.y);
	compMax = clamp(max(compMax, Upsampledcw.z), 0.0f, 1.0f);
	float scale = min(Exposure_co_rcp / ((1.0f + 1.0f / 65504.0f) - compMax), ColorMax);

	if (ColorMax > 4000.0f) scale = ColorMax;
	Upsampledcw.xyz = Upsampledcw.xyz * scale.xxx;

	SceneColorOutput[DisThreadID] = Upsampledcw.xyz;
}
