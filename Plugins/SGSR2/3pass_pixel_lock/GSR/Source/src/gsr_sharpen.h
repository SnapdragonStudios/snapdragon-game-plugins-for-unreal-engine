Texture2D<half4> Input;
RWTexture2D<half4> UpscaledOutput;
float PreExposure;
float Sharpness;

#define GSR_RCAS_LIMIT (0.25-(1.0/16.0))

void CurrFilter(uint2 pos)
{
    // Algorithm uses minimal 3x3 pixel neighborhood.
     //    b
     //  d e f
     //    h
    int2 sp = int2(pos);
	
    half3 b = PrepareRgb(Input[sp + int2(0, -1)].rgb, PreExposure);
	half3 f = PrepareRgb(Input[sp + int2(1, 0)].rgb, PreExposure);
	half3 min4YCoCg = min(b, f);
	half3 max4YCoCg = max(b, f);
	half3 h = PrepareRgb(Input[sp + int2(0, 1)].rgb, PreExposure);
	min4YCoCg = min(min4YCoCg, h);
	max4YCoCg = max(max4YCoCg, h);
	half3 d = PrepareRgb(Input[sp + int2(-1, 0)].rgb, PreExposure);
	min4YCoCg = min(min4YCoCg, d);
	max4YCoCg = max(max4YCoCg, d);
	
	half3 hitMinYCoCg = min4YCoCg * rcp(max4YCoCg * half3(4.0, 4.0, 4.0));
	half3 hitMaxYCoCg = (half3(1.0, 1.0, 1.0) - max4YCoCg) * rcp(half3(4.0, 4.0, 4.0) * min4YCoCg - half3(4.0, 4.0, 4.0));
	half3 lobeYCoCg = max(-hitMinYCoCg, hitMaxYCoCg);

	half lobe = max(half(-GSR_RCAS_LIMIT), min(max3(lobeYCoCg.x, lobeYCoCg.y, lobeYCoCg.z), half(0.0))) * Sharpness;

	half3 e = PrepareRgb(Input[sp].rgb, PreExposure);

	// Noise detection
	half lumax2_b = dot(b.xyz, half3(0.5, 1.0, 0.5));
	half lumax2_f = dot(f.xyz, half3(0.5, 1.0, 0.5));
	half lumax2_h = dot(h.xyz, half3(0.5, 1.0, 0.5));
	half lumax2_d = dot(d.xyz, half3(0.5, 1.0, 0.5));
	half lumaCenter = dot(e.xyz, half3(0.5, 1.0, 0.5));
	half4 luma4 = half4(lumax2_b, lumax2_f, lumax2_h, lumax2_d);

	half nz = dot(luma4, half4(0.25, 0.25,0.25, 0.25)) - lumaCenter;
	nz = saturate(abs(nz) * rcp(max3(max3(luma4.x, luma4.y, luma4.z), luma4.w, lumaCenter) - min3(min3(luma4.x, luma4.y, luma4.z), luma4.w, lumaCenter)));
	nz = half(-0.5) * nz + half(1.0);
	
    lobe *= nz;

    half rcpL  = rcp(half(4.0) * lobe + half(1.0));
	half pixY  = (dot(half4(b.x, d.x, h.x, f.x), half4(lobe, lobe, lobe, lobe)) + e.x) * rcpL;
	half pixCo = (dot(half4(b.y, d.y, h.y, f.y), half4(lobe, lobe, lobe, lobe)) + e.y) * rcpL;
	half pixCg = (dot(half4(b.z, d.z, h.z, f.z), half4(lobe, lobe, lobe, lobe)) + e.z) * rcpL;

	half3 c = UnprepareRgb(half3(pixY, pixCo, pixCg), PreExposure);
	UpscaledOutput[pos] = half4(c, 1.0);
}

void sharpen(uint2 DisThreadID) {
    CurrFilter(DisThreadID);
}