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
float Scalefactor;
float Biasmax_viewportXScale;

RWTexture2D<float4> HistoryOutput;
RWTexture2D<float3> SceneColorOutput;

float FastLanczos(float base)
{
	float y = base - 1;
	float y2 = y * y;
	float y_temp = 0.75 * y + y2;
	return y_temp * y2;
}

float3 DecodeColor(uint sample32)
{
	uint x11 = sample32 >> 21;
	uint y11 = sample32 & (2047 << 10);
	uint z10 = sample32 & 1023;
	float3 samplecolor;
	samplecolor.x = ((float)x11 * (1.0 / 2047.5));
	samplecolor.y = ((float)y11 * (4.76953602e-7)) - 0.5;
	samplecolor.z = ((float)z10 * (1.0 / 1023.5)) - 0.5;
	return samplecolor;
}

void Update(uint2 DisThreadID)
{
	float2 Hruv = (DisThreadID + 0.5f) * HistoryInfo_ViewportSizeInverse;
	float2 Jitteruv;
	Jitteruv.x = clamp(Hruv.x + InputJitter.x * InputInfo_ViewportSizeInverse.x, 0.0f, 1.0f);
	Jitteruv.y = clamp(Hruv.y + InputJitter.y * InputInfo_ViewportSizeInverse.y, 0.0f, 1.0f);
	int2 InputPos = Jitteruv * InputInfo_ViewportSize;

	float4 mda = MotionDepthClipAlphaBuffer.SampleLevel(LinearClamp1, Jitteruv, 0).xyzw;
	float2 Motion = mda.xy;
	///ScreenPosToViewportScale&Bias
	float2 PrevUV;
	PrevUV.x = -0.5f * Motion.x + Hruv.x;
	PrevUV.y = 0.5f * Motion.y + Hruv.y;

	PrevUV.x = clamp(PrevUV.x, 0.0f, 1.0f);
	PrevUV.y = clamp(PrevUV.y, 0.0f, 1.0f);

	float depthfactor = frac(mda.z);
	float bright = (mda.z - depthfactor) * 1000.0f;
	float history_value = frac(mda.w);
	float alphamask = (mda.w - history_value) * 0.001f;
	history_value *= 2;

	float4 History = PrevHistoryOutput.SampleLevel(LinearClamp2, PrevUV, 0);
	float3 HistoryColor = History.xyz;
	float Historyw = History.w;
	float Wfactor = max(saturate(abs(Historyw)), alphamask);

	/////upsample and compute box
	float4 Upsampledcw = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float kernelfactor = saturate(Wfactor + ValidReset);
	float biasmax = Biasmax_viewportXScale - Biasmax_viewportXScale * kernelfactor;
	float biasmin = max(1.0f, 0.3f + 0.3f * biasmax);
	float biasfactor = max(0.25f * depthfactor, kernelfactor);
	float kernelbias = lerp(biasmax, biasmin, biasfactor);
	float motion_viewport_len = length(Motion * HistoryInfo_ViewportSize);
	float curvebias = lerp(-2.0f, -3.0f, saturate(motion_viewport_len * 0.02f));

	float3 rectboxcenter = float3(0.0f, 0.0f, 0.0f);
	float3 rectboxvar = float3(0.0f, 0.0f, 0.0f);
	float rectboxweight = 0.0f;

	float2 srcpos = InputPos + float2(0.5f, 0.5f) - InputJitter;
	float2 srcOutputPos = Hruv * InputInfo_ViewportSize;

	kernelbias *= 0.5f;
	float kernelbias2 = kernelbias * kernelbias;
	float2 srcpos_srcOutputPos = srcpos - srcOutputPos;

	int2 InputPosBtmRight = int2(1, 1) + InputPos;
	float2 gatherCoord = float2(InputPos) * InputInfo_ViewportSizeInverse;
	uint4 topleft = YCoCgColor.GatherRed(PointClamp, gatherCoord);
	uint2 topRight;
	uint2 bottomLeft;
	
#if five_sample_use
	uint2 btmRight = YCoCgColor.GatherRed(PointClamp, gatherCoord + float2(InputInfo_ViewportSizeInverse.x, InputInfo_ViewportSizeInverse.y)).xz;
	bottomLeft.y = btmRight.x;
	topRight.x = btmRight.y;
#else
	topRight = YCoCgColor.GatherRed(PointClamp, gatherCoord + float2(InputInfo_ViewportSizeInverse.x, 0.0)).yz;
	bottomLeft = YCoCgColor.GatherRed(PointClamp, gatherCoord + float2(0.0, InputInfo_ViewportSizeInverse.y)).xy;
#endif

	float3 rectboxmin;
	float3 rectboxmax;

	{
		float3 samplecolor = DecodeColor(bottomLeft.y);
		float2 baseoffset = srcpos_srcOutputPos + float2(0.0, 1.0);
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
		float2 baseoffset = srcpos_srcOutputPos + float2(1.0, 0.0);
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
		float2 baseoffset = srcpos_srcOutputPos + float2(-1.0, 0.0);
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
		float2 baseoffset = srcpos_srcOutputPos + float2(0.0, -1.0);
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
		float2 baseoffset = srcpos_srcOutputPos + float2(1.0, 1.0);
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
		float2 baseoffset = srcpos_srcOutputPos + float2(-1.0, 1.0);
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
		float2 baseoffset = srcpos_srcOutputPos + float2(1.0, -1.0);
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
		float2 baseoffset = srcpos_srcOutputPos + float2(-1.0, -1.0);
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

	rectboxweight = 1.0 / rectboxweight;
	rectboxcenter *= rectboxweight;
	rectboxvar *= rectboxweight;
	rectboxvar = sqrt(abs(rectboxvar - rectboxcenter * rectboxcenter));

	float3 bias = float3(0.05f, 0.05f, 0.05f);
	Upsampledcw.xyz = clamp(Upsampledcw.xyz / Upsampledcw.w, rectboxmin - bias, rectboxmax + bias);
	Upsampledcw.w = Upsampledcw.w * (1.0f / 3.0f);

	float tcontribute = history_value * saturate(rectboxvar.x * 10.0f);
	float OneMinusWfactor = 1.0f - Wfactor;
	tcontribute = tcontribute * OneMinusWfactor;

	float baseupdate = OneMinusWfactor - OneMinusWfactor * depthfactor;
	baseupdate = min(baseupdate, lerp(baseupdate, Upsampledcw.w * 10.0f, saturate(10.0f * motion_viewport_len)));
	baseupdate = min(baseupdate, lerp(baseupdate, Upsampledcw.w, saturate(motion_viewport_len * 0.05f)));
	float basealpha = baseupdate;

	const float EPSILON = 1.192e-07f;
	float boxscale = max(depthfactor, saturate(motion_viewport_len * 0.05f));
	float boxsize = lerp(Scalefactor, 1.0f, boxscale);

	float3 sboxvar = rectboxvar * boxsize;
	float3 boxmin = rectboxcenter - sboxvar;
	float3 boxmax = rectboxcenter + sboxvar;
	rectboxmax = min(rectboxmax, boxmax);
	rectboxmin = max(rectboxmin, boxmin);

	float3 clampedcolor = clamp(HistoryColor, rectboxmin, rectboxmax);
	float lerpcontribution = (any(rectboxmin > HistoryColor) || any(HistoryColor > rectboxmax)) ? tcontribute : 1.0f;
	lerpcontribution = lerpcontribution - lerpcontribution * sqrt(alphamask);

	HistoryColor = lerp(clampedcolor, HistoryColor, saturate(lerpcontribution));
	float basemin = min(basealpha, 0.1f);
	basealpha = lerp(basemin, basealpha, saturate(lerpcontribution));

	////blend color
	float alphasum = max(EPSILON, basealpha + Upsampledcw.w);
	float alpha = saturate(Upsampledcw.w / alphasum + ValidReset);
	Upsampledcw.xyz = lerp(HistoryColor, Upsampledcw.xyz, alpha.xxx);

	HistoryOutput[DisThreadID] = float4(Upsampledcw.xyz, Wfactor);
	////ycocg to grb
	float x_z = Upsampledcw.x - Upsampledcw.z;
	Upsampledcw.xyz = float3(
		x_z + Upsampledcw.y,
		Upsampledcw.x + Upsampledcw.z,
		x_z - Upsampledcw.y);

	float compMax = max(Upsampledcw.x, Upsampledcw.y);
	float scale;
	if (bright > 1000.0f)
	{
		compMax = clamp(max(compMax, Upsampledcw.z), 0.0f, 1.0f);
		scale = bright > 4000.0f ? bright : min(Exposure_co_rcp / ((1.0f + 1.0f / 65504.0f) - compMax), bright);
	}
	else
	{
		compMax = clamp(max(compMax, Upsampledcw.z), 0.0f, 254.0f / 255.0f);
		scale = Exposure_co_rcp / ((1.0f + 1.0f / 65504.0f) - compMax);
	}
	Upsampledcw.xyz = Upsampledcw.xyz * scale.xxx;

	SceneColorOutput[DisThreadID] = Upsampledcw.xyz;
}
