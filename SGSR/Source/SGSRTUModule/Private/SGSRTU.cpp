//============================================================================================================
//
//
//                  Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "SGSRTU.h"
#include "SGSRTUModule.h"
#include "SGSRTUHistory.h"
#include "HAL/Platform.h"
#include "SceneTextureParameters.h"
#include "TranslucentRendering.h"
#include "ScenePrivate.h"
#include "LogSGSR.h"
#include "LegacyScreenPercentageDriver.h"
#include "PlanarReflectionSceneProxy.h"
#include "Serialization/MemoryImage.h"
#include "Serialization/MemoryLayout.h"
#include "FXSystem.h"
#include "PostProcess/SceneRenderTargets.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
#include "DataDrivenShaderPlatformInfo.h"
#endif
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 8
#include "SceneViewState.h"
#endif
#include "RenderGraphUtils.h"
#include "RenderGraphBuilder.h"
//#include "PostProcess/PostProcessMitchellNetravali.h"

DECLARE_GPU_STAT_NAMED(SGSRPass, TEXT("SGSRPass"));
DECLARE_GPU_STAT_NAMED(SGSRDispatch, TEXT("SGSR Dispatch"));

typedef enum SGSRQualityMode
{
	SGSR_QUALITYMODE_ULTRA_QUALITY = 0,
	SGSR_QUALITYMODE_QUALITY = 1,
	SGSR_QUALITYMODE_BALANCED = 2,
	SGSR_QUALITYMODE_PERFORMANCE = 3,
	SGSR_QUALITYMODE_CUSTOM = 4,
} SGSRQualityMode;

///Quality mode definitions
static const SGSRQualityMode DefaultQualitymode = SGSR_QUALITYMODE_QUALITY;
static const SGSRQualityMode MinResolutionQualitymode = SGSR_QUALITYMODE_CUSTOM;
static const SGSRQualityMode MaxResolutionQualitymode = SGSR_QUALITYMODE_ULTRA_QUALITY;

///console variables
static TAutoConsoleVariable<int32> CVarSGSRMethod(
	TEXT("r.SGSR.Method"),
	0,
	TEXT("Options for SGSR methods 0-2. Default is 0.\nSU: Spatial Upscaling, TU: Temporal Upscaling.\n")
	TEXT("0 - SU\n")
	TEXT("1 - TU 2Pass Fragment Shader\n")
	TEXT("2 - TU 3Pass Compute Shader"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSGSRExposure(
	TEXT("r.SGSR.Exposure"),
	0,
	TEXT("Default 0 to use engine's auto-exposure value, otherwise specific auto-exposure is used"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSGSRHistory(
	TEXT("r.SGSR.History"),
	0,
	TEXT("Bit-depth for History texture format. 0: PF_FloatRGBA, 1: PF_FloatR11G11B10. Default(0) has better quality but worse bandwidth"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSGSRQuality(
	TEXT("r.SGSR.Quality"),
	DefaultQualitymode,
	TEXT("Quality Mode 0-4. Higher values refer to better performance, lower values refer to superior images. Default is 1\n")
	TEXT("0 - Ultra Quality 1.25x, ScreenPercentage 80%\n")
	TEXT("1 - Quality 		1.5x,  ScreenPercentage 66.67% \n")
	TEXT("2 - Balanced 		1.7x,  ScreenPercentage 58.8% \n")
	TEXT("3 - Performance 	2.0x,  ScreenPercentage 50%\n")
	TEXT("4 - Custom 		Use custom screen percentage\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarSGSRCustomScreenPercentage(
	TEXT("r.SGSR.CustomScreenPercentage"),
	100.0f,
	TEXT("Custom screen percentage, only works when r.SGSR.Quality=4. Default is 100.0f, range[50.f, 100.f]"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarSGSRHistorySize(
	TEXT("r.SGSR.HistorySize"),
	100.0f,
	TEXT("Screen percentage of upscaler's history."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarSGSRSample(
	TEXT("r.SGSR.5Sample"), 
	1,
	TEXT("Controls the sample number of input color, false to choose 9 sample for better image quality, true to choose 5 sample for better performance, default is 1"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarSGSRSharpening(
	TEXT("r.SGSR.DoSharpening"),
	0,
	TEXT("Do sharpening, default is 0"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarSGSRSharpness(
	TEXT("r.SGSR.Sharpness"),
	1.12,
	TEXT("Sharpness, default is 1.12"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<bool> CVarSGSRPixelLock(
	TEXT("r.SGSR.PixelLock"),
	0,
	TEXT("Enable PixelLock, default is false"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<bool> CVarSGSRLanczos(
	TEXT("r.SGSR.LanczosOpt"),
	0,
	TEXT("Only applies to TU 2Pass Fragment Shader. Use a better but more complicated Lanczos sampler, enable to suppress flicker."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<bool> CVarSGSRThinFeature(
	TEXT("r.SGSR.ThinFeature"),
	0,
	TEXT("Only applies to TU 2Pass Fragment Shader. Detect and keep the thin feature, enable to suppress flicker."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<bool> CVarSGSRHalfPrecision(
	TEXT("r.SGSR.HalfPrecision"),
	1,
	TEXT("Enable Half Precision shader arithmetic (platform support dependent).  May improve shader performance."),
	ECVF_RenderThreadSafe);

float FSGSRTU::SavedScreenPercentage{ 100.0f };
static inline float GSR_GetResolutionFraction(SGSRQualityMode Qualitymode)
{
	switch (Qualitymode)
	{
		case SGSR_QUALITYMODE_ULTRA_QUALITY:
			return 1.0f / 1.25f;
		case SGSR_QUALITYMODE_QUALITY:
			return 1.0f / 1.5f;
		case SGSR_QUALITYMODE_BALANCED:
			return 1.0f / 1.7f;
		case SGSR_QUALITYMODE_PERFORMANCE:
			return 1.0f / 2.0f;
		case SGSR_QUALITYMODE_CUSTOM:
		{
			float CustomScreenPercentage = CVarSGSRCustomScreenPercentage.GetValueOnAnyThread();
			CustomScreenPercentage = FMath::Clamp(CustomScreenPercentage * 0.01f, 0.5f, 1.0f);
			return CustomScreenPercentage; // 50% ~ 100%
		}
		default:
			return 0.0f;
	}
}

////////COMMON PARAMETERS
BEGIN_SHADER_PARAMETER_STRUCT(FSGSRCommonParameters, )
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputInfo)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, DepthInfo)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, HistoryInfo)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutputInfo)

	SHADER_PARAMETER(FVector2f, InputJitter)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FSGSRStateTextures, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Color)
END_SHADER_PARAMETER_STRUCT()

class FSGSRShader : public FGlobalShader
{
public:
	FSGSRShader() = default;
	FSGSRShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer) {}

	class FConfigCompileFp16 : SHADER_PERMUTATION_BOOL("CONFIG_COMPILE_FP16");
	using FBasePermutationDomain = TShaderPermutationDomain<FConfigCompileFp16>;

	static bool ShouldCompilePermutationBase(const FGlobalShaderPermutationParameters& Parameters, const FBasePermutationDomain& PermutationVector)
	{
		#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
		const ERHIFeatureSupport RealTypeSupport = FDataDrivenShaderPlatformInfo::GetSupportsRealTypes(Parameters.Platform);
		bool bIs16BitVALUPermutation = PermutationVector.Get<FConfigCompileFp16>();

		if (bIs16BitVALUPermutation && (RealTypeSupport == ERHIFeatureSupport::Unsupported))
		{
			// 16bit unsupported, dont compile the 16bit permutation
			return false;
		}
		#endif
		// 16bit might be supported (or always supported).  Compile both fp16 and fp32 shaders.
		return true;
	}

	static void ModifyCompilationEnvironmentBase(const FGlobalShaderPermutationParameters& Parameters, const FBasePermutationDomain& PermutationVector, FShaderCompilerEnvironment& OutEnvironment)
	{
		if (PermutationVector.Get<FConfigCompileFp16>())
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_AllowRealTypes);
		}
	}
}; // class

//---------2Pass FS-------------------
//	1. Convert
//	2. Upscale
//-----------------------------------------
class FSGSRConvertPS : public FSGSRShader
{
	DECLARE_GLOBAL_SHADER(FSGSRConvertPS);
	SHADER_USE_PARAMETER_STRUCT(FSGSRConvertPS, FSGSRShader);
	using FPermutationDomain = TShaderPermutationDomain<FSGSRShader::FBasePermutationDomain>;
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSGSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputVelocity)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputDepth)
		SHADER_PARAMETER(float, Exposure_co_rcp)
		SHADER_PARAMETER(float, AngleVertical)
		SHADER_PARAMETER(FMatrix44f, ReClipToPrevClip)
		SHADER_PARAMETER_SAMPLER(SamplerState, PointClamp)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		return ShouldCompilePermutationBase(Parameters, PermutationVector.Get<FBasePermutationDomain>());
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		ModifyCompilationEnvironmentBase(Parameters, PermutationVector.Get<FBasePermutationDomain>(), OutEnvironment);
	}
};

class FSGSRUpscalePS : public FSGSRShader
{
	DECLARE_GLOBAL_SHADER(FSGSRUpscalePS);
	SHADER_USE_PARAMETER_STRUCT(FSGSRUpscalePS, FSGSRShader);
	class FSampleNumberDim : SHADER_PERMUTATION_BOOL("SAMPLE_NUMBER");
	class FLanczosOptDim : SHADER_PERMUTATION_BOOL("LANCZOS_OPT");
	class FThinFeatureDim : SHADER_PERMUTATION_BOOL("THIN_FEATURE");
	using FPermutationDomain = TShaderPermutationDomain<FSGSRShader::FBasePermutationDomain, FSampleNumberDim, FLanczosOptDim, FThinFeatureDim>;
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSGSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputColor)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MotionDepthClipAlphaBuffer)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevHistoryOutput)
		SHADER_PARAMETER_SAMPLER(SamplerState, PointClamp)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
		SHADER_PARAMETER(float, ValidReset)
		SHADER_PARAMETER(float, MinLerpContribution)
		SHADER_PARAMETER(float, Scalefactor)
		SHADER_PARAMETER(float, Biasmax_viewportXScale)
		SHADER_PARAMETER(float, Exposure_co_rcp)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		return ShouldCompilePermutationBase(Parameters, PermutationVector.Get<FBasePermutationDomain>());
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		ModifyCompilationEnvironmentBase(Parameters, PermutationVector.Get<FBasePermutationDomain>(), OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSGSRConvertPS, "/Plugin/SGSR/Private/2PassFS/sgsr_convert.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSGSRUpscalePS, "/Plugin/SGSR/Private/2PassFS/sgsr_upscale.usf", "MainPS", SF_Pixel);


//---------3Pass Compute Shader----------------
//	1. Convert
//	2. Activate
//	3. Upscale
//	4. Sharpen
//-----------------------------------------

class FSGSRConvertCS : public FSGSRShader
{
	DECLARE_GLOBAL_SHADER(FSGSRConvertCS);
	SHADER_USE_PARAMETER_STRUCT(FSGSRConvertCS, FSGSRShader);
	class FInvertedDepthDim : SHADER_PERMUTATION_BOOL("INVERTED_DEPTH");
	using FPermutationDomain = TShaderPermutationDomain<FSGSRShader::FBasePermutationDomain, FInvertedDepthDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSGSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputVelocity)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputColor)
		SHADER_PARAMETER(FMatrix44f, ReClipToPrevClip)
		SHADER_PARAMETER_SAMPLER(SamplerState, PointClamp)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, YCoCgLuma)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, DilatedMotionDepthLuma)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		return ShouldCompilePermutationBase(Parameters, PermutationVector.Get<FBasePermutationDomain>());
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		ModifyCompilationEnvironmentBase(Parameters, PermutationVector.Get<FBasePermutationDomain>(), OutEnvironment);
	}
};

class FSGSRActivateCS : public FSGSRShader
{
	DECLARE_GLOBAL_SHADER(FSGSRActivateCS);
	SHADER_USE_PARAMETER_STRUCT(FSGSRActivateCS, FSGSRShader);
	class FInvertedDepthDim : SHADER_PERMUTATION_BOOL("INVERTED_DEPTH");
	class FPixelLockDim : SHADER_PERMUTATION_BOOL("ENABLE_PIXEL_LOCK");
	using FPermutationDomain = TShaderPermutationDomain<FSGSRShader::FBasePermutationDomain, FInvertedDepthDim, FPixelLockDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSGSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DilatedMotionDepthLuma)

		SHADER_PARAMETER(FVector4f, DeviceToViewDepth)
		SHADER_PARAMETER_SAMPLER(SamplerState, PointClamp)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
		
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, ReactiveMask)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, NewLocks)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		return ShouldCompilePermutationBase(Parameters, PermutationVector.Get<FBasePermutationDomain>());
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		ModifyCompilationEnvironmentBase(Parameters, PermutationVector.Get<FBasePermutationDomain>(), OutEnvironment);
	}
};

class FSGSRUpscaleCS : public FSGSRShader
{
	DECLARE_GLOBAL_SHADER(FSGSRUpscaleCS);
	SHADER_USE_PARAMETER_STRUCT(FSGSRUpscaleCS, FSGSRShader);
	class FSampleNumberDim : SHADER_PERMUTATION_BOOL("SAMPLE_NUMBER");
	class FDoSharpeningDim : SHADER_PERMUTATION_BOOL("DO_SHARPENING");
	class FInvertedDepthDim : SHADER_PERMUTATION_BOOL("INVERTED_DEPTH");
	class FPixelLockDim : SHADER_PERMUTATION_BOOL("ENABLE_PIXEL_LOCK");
	using FPermutationDomain = TShaderPermutationDomain<FSGSRShader::FBasePermutationDomain, FSampleNumberDim, FDoSharpeningDim, FInvertedDepthDim, FPixelLockDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSGSRCommonParameters, CommonParameters)
		//YCoCg->RGB
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, YCoCgLuma)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DilatedMotionDepthLuma)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevHistoryOutput)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ReactiveMask)

		SHADER_PARAMETER_SAMPLER(SamplerState, PointClamp)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
		SHADER_PARAMETER(float, ValidReset)
		SHADER_PARAMETER(float, JitterSeqLength)
		SHADER_PARAMETER(float, Scalefactor)
		
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, HistoryOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SceneColorOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, NewLocks)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		return ShouldCompilePermutationBase(Parameters, PermutationVector.Get<FBasePermutationDomain>());
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		ModifyCompilationEnvironmentBase(Parameters, PermutationVector.Get<FBasePermutationDomain>(), OutEnvironment);
	}
};

class FSGSRSharpenCS : public FSGSRShader
{
	DECLARE_GLOBAL_SHADER(FSGSRSharpenCS);
	SHADER_USE_PARAMETER_STRUCT(FSGSRSharpenCS, FSGSRShader);
	using FPermutationDomain = TShaderPermutationDomain<FSGSRShader::FBasePermutationDomain>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSGSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Input)
		SHADER_PARAMETER(float, Sharpness)
		SHADER_PARAMETER_SAMPLER(SamplerState, PointClamp)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, UpscaledOutput)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		return ShouldCompilePermutationBase(Parameters, PermutationVector.Get<FBasePermutationDomain>());
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		ModifyCompilationEnvironmentBase(Parameters, PermutationVector.Get<FBasePermutationDomain>(), OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSGSRConvertCS, "/Plugin/SGSR/Private/3PassCS/sgsr_convert.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSGSRActivateCS, "/Plugin/SGSR/Private/3PassCS/sgsr_activate.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSGSRUpscaleCS, "/Plugin/SGSR/Private/3PassCS/sgsr_upscale.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSGSRSharpenCS, "/Plugin/SGSR/Private/3PassCS/sgsr_sharpen.usf", "MainCS", SF_Compute);

//---------------------------------------------------------------

struct FSGSRFX
{
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	RDG_TEXTURE_ACCESS(InputColor, ERHIAccess::CopySrc)
	RDG_TEXTURE_ACCESS(OutputColor, ERHIAccess::CopyDest)
	END_SHADER_PARAMETER_STRUCT()
};

class FSGSRFXSystem : public FFXSystemInterface
{
	FGPUSortManager* GPUSortManager;
	FSGSRTU* Upscaler;
	FRHIUniformBuffer* SceneTexturesUniformBuffer = nullptr;

public:
	static const FName FXName;

	FFXSystemInterface* GetInterface(const FName& InName) final
	{
		return InName == FSGSRFXSystem::FXName ? this : nullptr;
	}
	void Tick(UWorld*, float DeltaSeconds) final {}

#if WITH_EDITOR
	void Suspend() final
	{
	}
	void Resume() final {}
#endif

	void DrawDebug(FCanvas* Canvas) final
	{
	}
	void AddVectorField(UVectorFieldComponent* VectorFieldComponent) final {}
	void RemoveVectorField(UVectorFieldComponent* VectorFieldComponent) final {}
	void UpdateVectorField(UVectorFieldComponent* VectorFieldComponent) final {}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
	void PreInitViews(FRDGBuilder&, bool, const TArrayView<const FSceneViewFamily*>&, const FSceneViewFamily*) final {};
	void PostInitViews(FRDGBuilder&, TConstStridedView<FSceneView>, bool) final {};
#else
	void PreInitViews(FRDGBuilder&, bool) final {}
	void PostInitViews(FRDGBuilder&, TArrayView<const FViewInfo, int32>, bool) final {}
#endif

	bool UsesGlobalDistanceField() const final { return false; }
	bool UsesDepthBuffer() const final { return false; }
	bool RequiresEarlyViewUniformBuffer() const final { return false; }
	bool RequiresRayTracingScene() const final { return false; }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
	void PreRender(FRDGBuilder&, TConstStridedView<FSceneView>, FSceneUniformBuffer&, bool) final{};
	void PostRenderOpaque(FRDGBuilder& GraphBuilder, TConstStridedView<FSceneView> Views, FSceneUniformBuffer& SceneUniformBuffer, bool bAllowGPUParticleUpdate) final {}
#else
	void PreRender(FRDGBuilder&, TConstArrayView<FViewInfo>, bool) final {}
	void PostRenderOpaque(FRDGBuilder& GraphBuilder, TConstArrayView<FViewInfo> Views, bool bAllowGPUParticleUpdate) final
	{
		Upscaler->CopyOpaqueColor(GraphBuilder, Views, nullptr, this->SceneTexturesUniformBuffer);
	}
#endif
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
	void SetSceneTexturesUniformBuffer(const TUniformBufferRef<FSceneTextureUniformParameters>& InSceneTexturesUniformParams) final
#else
	void SetSceneTexturesUniformBuffer(FRHIUniformBuffer* InSceneTexturesUniformParams) final
#endif
		 { SceneTexturesUniformBuffer = InSceneTexturesUniformParams; }

	FGPUSortManager* GetGPUSortManager() const
	{
		return GPUSortManager;
	}

	FSGSRFXSystem(FSGSRTU* InUpscaler, FGPUSortManager* InGPUSortManager)
		: GPUSortManager(InGPUSortManager)
		, Upscaler(InUpscaler)
	{
		check(GPUSortManager && Upscaler);
	}
	~FSGSRFXSystem() {}
};
FName const FSGSRFXSystem::FXName(TEXT("SGSRFXSystem"));

///TU implementation
FSGSRTU::FSGSRTU()
	: ReflectionTexture(nullptr)
{
	FMemory::Memzero(PostInputs);
	FSGSRTU* self = this;
	FFXSystemInterface::RegisterCustomFXSystem(
		FSGSRFXSystem::FXName,
		FCreateCustomFXSystemDelegate::CreateLambda([self](ERHIFeatureLevel::Type InFeatureLevel,
		EShaderPlatform InShaderPlatform, FGPUSortManager* InGPUSortManager) -> FFXSystemInterface* {
			return new FSGSRFXSystem(self, InGPUSortManager);
		}));

	FConsoleVariableDelegate EnabledChangedDelegate = FConsoleVariableDelegate::CreateStatic(&FSGSRTU::ChangeSGSREnabled);
	IConsoleVariable* CVarEnabled = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SGSR.Enabled"));
	CVarEnabled->SetOnChangedCallback(EnabledChangedDelegate);

	FConsoleVariableDelegate QualityChangedDelegate = FConsoleVariableDelegate::CreateStatic(&FSGSRTU::ChangeQualityMode);
	IConsoleVariable* CVarQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SGSR.Quality"));
	CVarQuality->SetOnChangedCallback(QualityChangedDelegate);

	FConsoleVariableDelegate MethodChangedDelegate = FConsoleVariableDelegate::CreateStatic(&FSGSRTU::ChangeSGSRMethod);
	IConsoleVariable* CVarMethod = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SGSR.Method"));
	CVarMethod->SetOnChangedCallback(MethodChangedDelegate);
	
	FConsoleVariableDelegate CustomScreenPercentageChangedDelegate = FConsoleVariableDelegate::CreateStatic(&FSGSRTU::ChangeCustomScreenPercentage);
	IConsoleVariable* CVarCustomScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SGSR.CustomScreenPercentage"));
	CVarCustomScreenPercentage->SetOnChangedCallback(CustomScreenPercentageChangedDelegate);

	if (CVarEnabled->GetBool())
	{
		ChangeSGSRMethod(nullptr);
		SaveScreenPercentage();
		UpdateScreenPercentage();
	}
	GEngine->GetDynamicResolutionCurrentStateInfos(DynamicResolutionStateInfos);
}

FSGSRTU::~FSGSRTU()
{
	Cleanup();
	FFXSystemInterface::UnregisterCustomFXSystem(FSGSRFXSystem::FXName);
}

const TCHAR* FSGSRTU::GetDebugName() const
{
	return TEXT("FSGSRTU");
}

void FSGSRTU::Releasestate(SGSRstateRef state)
{
	FSGSRState* Ptr = state.GetReference();
	if (Ptr)
	{
		Ptr->AddRef();
		Availablestates.Push(Ptr);
	}
}

void FSGSRTU::Cleanup() const
{
	FSGSRState* Ptr = Availablestates.Pop();
	while (Ptr)
	{
		Ptr->Release();
		Ptr = Availablestates.Pop();
	}
}

float FSGSRTU::GetRfraction(uint32 Mode)
{
	SGSRQualityMode Qualitymode = FMath::Clamp<SGSRQualityMode>((SGSRQualityMode)Mode, MaxResolutionQualitymode, MinResolutionQualitymode);
	const float ResolutionFraction = GSR_GetResolutionFraction(Qualitymode);
	return ResolutionFraction;
}

void FSGSRTU::SaveScreenPercentage()
{
	SavedScreenPercentage = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage"))->GetValueOnGameThread();
}

void FSGSRTU::UpdateScreenPercentage()
{
	float RFraction = GetRfraction(CVarSGSRQuality.GetValueOnGameThread());
	static IConsoleVariable* ScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	ScreenPercentage->Set(RFraction * 100.00f);
}


void FSGSRTU::RestoreScreenPercentage()
{
	static IConsoleVariable* ScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	ScreenPercentage->Set(SavedScreenPercentage);
}

void FSGSRTU::ChangeSGSREnabled(IConsoleVariable* Var)
{
	static const auto CVarEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SGSR.Enabled"));
	if (CVarEnabled && CVarEnabled->GetValueOnGameThread())
	{
		SaveScreenPercentage();
		UpdateScreenPercentage();
	}
	else
	{
		RestoreScreenPercentage();
	}
}

void FSGSRTU::ChangeQualityMode(IConsoleVariable* Var)
{
	static const auto CVarEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SGSR.Enabled"));
	if (CVarEnabled && CVarEnabled->GetValueOnGameThread())
	{
		uint32 QualityMode = CVarSGSRQuality.GetValueOnGameThread();
		float RFraction = GetRfraction(CVarSGSRQuality.GetValueOnGameThread());
		static IConsoleVariable* ScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
		ScreenPercentage->Set(RFraction * 100.0f);
	}
}

void FSGSRTU::ChangeSGSRMethod(IConsoleVariable* Var)
{
	static const auto CVarEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SGSR.Enabled"));
	if (CVarEnabled && CVarEnabled->GetValueOnAnyThread())
	{
		int GSRMethod = CVarSGSRMethod.GetValueOnAnyThread();
		static IConsoleVariable* DefaultAAMethod = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));
		static IConsoleVariable* MobileAAMethod = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.AntiAliasing"));
		if (!GSRMethod)
		{
			DefaultAAMethod->Set(0);
			MobileAAMethod->Set(0);
		}
		else
		{
			DefaultAAMethod->Set(2);
			MobileAAMethod->Set(2);
		}
	}
}

void FSGSRTU::ChangeCustomScreenPercentage(IConsoleVariable* Var)
{
	static const auto CVarEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SGSR.Enabled"));
	static const auto CVarQuality = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SGSR.Quality"));
	if (CVarEnabled && CVarEnabled->GetValueOnAnyThread() && CVarQuality && CVarQuality->GetValueOnAnyThread() == SGSR_QUALITYMODE_CUSTOM)
	{
		UpdateScreenPercentage();
	}
}

bool FSGSRTU::IsAlphaSupported() const
{
#if ENGINE_MAJOR_VERSION <= 5 && ENGINE_MINOR_VERSION < 5 // before UE5.5
	static const auto CVarPostPropagateAlpha = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessing.PropagateAlpha"));
	const bool bSupportsAlpha = (CVarPostPropagateAlpha && CVarPostPropagateAlpha->GetValueOnRenderThread() != 0);
#else
	static const auto CVarPostPropagateAlpha = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessing.PropagateAlpha"));
	const bool bSupportsAlpha = (CVarPostPropagateAlpha && CVarPostPropagateAlpha->GetBool());
#endif
	return bSupportsAlpha;
}

SGSRPassOutput FSGSRTU::AddPasses(
	FRDGBuilder& GraphBuilder,
	const SGSRView& SceneView,
	const SGSRPassInput& PassInputs) const
{
	int32 GSRMethod = (int32)CVarSGSRMethod.GetValueOnAnyThread();
	if (GSRMethod == 1)
		return AddPasses_2PassFS(GraphBuilder, SceneView, PassInputs);
	else //if (GSRMethod == 2)
		return AddPasses_3PassCS(GraphBuilder, SceneView, PassInputs);
}


static FVector LastViewOrigin = FVector::ZeroVector;
static FVector LastViewDir = FVector::ZeroVector;
static float SameFrameNum = 1.0;

SGSRPassOutput FSGSRTU::AddPasses_2PassFS(FRDGBuilder& GraphBuilder, const SGSRView& SceneView, const SGSRPassInput& PassInputs) const
{
	const FViewInfo& View = (FViewInfo&)(SceneView);
	/// output extent
	FIntPoint InputExtents = View.ViewRect.Size();
	FIntRect InputRect = View.ViewRect;
	FIntPoint InputExtentsQuantized;
	FIntPoint OutputExtents;
	FIntPoint OutputExtentsQuantized;
	FIntRect OutputRect;
	FIntPoint HistoryExtents;
	FIntPoint HistoryExtentsQuantized;
	FIntRect HistoryRect;
	const FViewUniformShaderParameters& ViewUniformParams = *View.CachedViewUniformShaderParameters;
	FIntPoint DepthExtents = FIntPoint(ViewUniformParams.BufferSizeAndInvSize.X, ViewUniformParams.BufferSizeAndInvSize.Y);
	FIntRect DepthRect = FIntRect(FIntPoint(0, 0), DepthExtents);

	if (View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale)
	{
		OutputRect = FIntRect(FIntPoint(0, 0), View.GetSecondaryViewRectSize());
		OutputExtents = View.GetSecondaryViewRectSize();
		QuantizeSceneBufferSize(InputExtents, InputExtentsQuantized);
		QuantizeSceneBufferSize(OutputExtents, OutputExtentsQuantized);
		OutputExtents = FIntPoint(FMath::Max(InputExtents.X, OutputExtentsQuantized.X), FMath::Max(InputExtents.Y, OutputExtentsQuantized.Y));
	}
	else
	{
		OutputRect = FIntRect(FIntPoint(0, 0), View.ViewRect.Size());
		OutputExtents = InputExtents;
	}

	FIntPoint HistorySize;
	float HistoryFactor = FMath::Clamp(CVarSGSRHistorySize.GetValueOnRenderThread() / 100.0f, 1.0f, 2.0f);
	HistoryExtents = FIntPoint(FMath::CeilToInt(OutputExtents.X * HistoryFactor),
		FMath::CeilToInt(OutputExtents.Y * HistoryFactor));
	HistorySize = FIntPoint(FMath::CeilToInt(OutputExtents.X * HistoryFactor),
		FMath::CeilToInt(OutputExtents.Y * HistoryFactor));

	HistoryRect = FIntRect(FIntPoint(0, 0), HistorySize);
	// Quantize the buffers to match UE behavior
	QuantizeSceneBufferSize(HistoryRect.Max, HistoryExtentsQuantized);

	const FVector ViewOrigin = View.ViewMatrices.GetViewOrigin();
	const FVector ViewDir = View.GetViewDirection();
	bool bSameCamera = ViewOrigin == LastViewOrigin && ViewDir == LastViewDir;
	LastViewOrigin = ViewOrigin;
	LastViewDir = ViewDir;

	bool const bValidEyeAdaptation = View.HasValidEyeAdaptationBuffer();
	bool const bHasAutoExposure = bValidEyeAdaptation || CVarSGSRExposure.GetValueOnRenderThread();

	check((View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale));
	{
		ISGSRTemporalUpscaler::FOutputs Outputs;

#if ENGINE_MINOR_VERSION < 8
		RDG_GPU_STAT_SCOPE(GraphBuilder, SGSRPass);
#else
		RDG_EVENT_SCOPE_STAT(GraphBuilder, SGSRPass, "SGSRPass");
#endif
		RDG_EVENT_SCOPE(GraphBuilder, "SGSR Temporal Upscaler - 2Pass Fragment Shader");

		GEngine->AddOnScreenDebugMessage(0, 1.0f, FColor::Red, FString::Printf(TEXT("SGSR Temporal Upscaler - 2Pass Fragment Shader")));

		const bool bWritePrevViewInfo = !View.bStatePrevViewInfoIsReadOnly && View.ViewState;

		bool bHistoryValid = View.PrevViewInfo.TemporalAAHistory.IsValid() && View.ViewState && !View.bCameraCut;

		SGSRstateRef GSRState;
		const FTemporalAAHistory& InputHistory = View.PrevViewInfo.TemporalAAHistory;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
		const TRefCountPtr<ICustomTemporalAAHistory> PrevCustomHistory = PassInputs.PrevHistory;
#else
		const TRefCountPtr<ICustomTemporalAAHistory> PrevCustomHistory = View.PrevViewInfo.CustomTemporalAAHistory;
#endif

		// const TRefCountPtr<ICustomTemporalAAHistory> InputCustomHistory = View.PrevViewInfo.CustomTemporalAAHistory;
		FGSRTUHistory* CustomHistory = static_cast<FGSRTUHistory*>(PrevCustomHistory.GetReference());

		FTemporalAAHistory* OutputHistory = View.ViewState ? &(View.ViewState->PrevFrameViewInfo.TemporalAAHistory) : nullptr;

		/// reuse states to reduce memory churn caused by camera cuts
		bool HasValidContext = CustomHistory && CustomHistory->Getstate().IsValid();

		if (HasValidContext)
		{

			if (CustomHistory->Getstate()->LastUsedFrame == GFrameCounterRenderThread)
			{
				HasValidContext = false;
			}
			else
			{
				GSRState = CustomHistory->Getstate();
			}
		}

		if (!HasValidContext)
		{
			TLockFreePointerListFIFO<FSGSRState, PLATFORM_CACHE_LINE_SIZE> Reusablestates;
			FSGSRState* Ptr = Availablestates.Pop();
			while (Ptr)
			{

				if (Ptr->LastUsedFrame == GFrameCounterRenderThread && Ptr->ViewID != View.ViewState->UniqueID)
				{
					// These states can't be reused immediately but perhaps a future frame, otherwise we break split screen.
					Reusablestates.Push(Ptr);
				}

				else
				{
					GSRState = Ptr;
					Ptr->Release();
					HasValidContext = true;
					bHistoryValid = false;
					break;
				}
				Ptr = Availablestates.Pop();
			}

			Ptr = Reusablestates.Pop();
			while (Ptr)
			{
				Availablestates.Push(Ptr);
				Ptr = Reusablestates.Pop();
			}

			GSRState = new FSGSRState();
		}

		GSRState->LastUsedFrame = GFrameCounterRenderThread;
		GSRState->ViewID = View.ViewState->UniqueID;

		/////prepare the view to receive history data
		if (bWritePrevViewInfo)
		{
			// Releases the existing history texture inside the wrapper object, this doesn't release NewHistory itself
			View.ViewState->PrevFrameViewInfo.TemporalAAHistory.SafeRelease();

			View.ViewState->PrevFrameViewInfo.TemporalAAHistory.ViewportRect = FIntRect(FIntPoint(0, 0), HistoryRect.Size());
			View.ViewState->PrevFrameViewInfo.TemporalAAHistory.ReferenceBufferSize = HistoryExtents;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
			Outputs.NewHistory = new FGSRTUHistory(GSRState, const_cast<FSGSRTU*>(this));
#else
			if (!View.ViewState->PrevFrameViewInfo.CustomTemporalAAHistory.GetReference())
			{
				View.ViewState->PrevFrameViewInfo.CustomTemporalAAHistory = new FGSRTUHistory(GSRState, const_cast<FSGSRTU*>(this));
			}
#endif
		}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
		else
		{
			Outputs.NewHistory = PassInputs.PrevHistory;
		}
#endif

		bool bReset = !InputHistory.IsValid() || View.bCameraCut || !OutputHistory;

		OutputExtents = FIntPoint(FMath::CeilToInt(OutputExtents.X * 0.25) * 4,
			FMath::CeilToInt(OutputExtents.Y * 0.25) * 4);

		OutputRect = FIntRect(FIntPoint(0, 0), OutputExtents);

		// Determine if we want/can use the 16bit shader variants
		const EShaderPlatform ShaderPlatform = View.GetShaderPlatform();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
		auto realTypeSupport = FDataDrivenShaderPlatformInfo::GetSupportsRealTypes(ShaderPlatform);
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
		bool bUseFp16 = ((GRHIGlobals.SupportsNative16BitOps || ShaderPlatform == SP_VULKAN_SM5_ANDROID) && realTypeSupport == ERHIFeatureSupport::RuntimeDependent);
		bUseFp16 = bUseFp16 || (realTypeSupport == ERHIFeatureSupport::RuntimeGuaranteed);
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
		bool bUseFp16 = (ShaderPlatform == SP_VULKAN_SM5_ANDROID && realTypeSupport == ERHIFeatureSupport::RuntimeDependent);
		bUseFp16 = bUseFp16 || (realTypeSupport == ERHIFeatureSupport::RuntimeGuaranteed);
#else
		bool bUseFp16 = ShaderPlatform == SP_VULKAN_SM5_ANDROID;
#endif

		bUseFp16 = bUseFp16 && CVarSGSRHalfPrecision.GetValueOnRenderThread();

		// All shaders use the same base (with the same bUseFp16 setting)
		FSGSRShader::FBasePermutationDomain BasePermutationVector;
		BasePermutationVector.Set<FSGSRShader::FConfigCompileFp16>(bUseFp16);
		FSGSRCommonParameters CommonParameters;
		{
			CommonParameters.InputInfo = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(
				InputExtents, InputRect));
			CommonParameters.DepthInfo = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(
				DepthExtents, DepthRect));
			CommonParameters.HistoryInfo = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(
				HistoryExtents, HistoryRect));
			CommonParameters.OutputInfo = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(
				OutputExtents, OutputRect));
			CommonParameters.InputJitter = FVector2f(View.TemporalJitterPixels);
			CommonParameters.ViewUniformBuffer = View.ViewUniformBuffer;
		}

		FRDGTextureRef MotionDepthClipAlphaBuffer;

		FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
			InputExtents,
			PF_FloatRGBA,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);

		MotionDepthClipAlphaBuffer = GraphBuilder.CreateTexture(Desc, TEXT("GSR.MotionDepthClipAlphaBuffer"));

		FScreenPassRenderTarget Output1 = FScreenPassRenderTarget(MotionDepthClipAlphaBuffer, InputRect, ERenderTargetLoadAction::ENoAction);
		/////////
		{
			FSGSRConvertPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSGSRConvertPS::FParameters>();
			PassParameters->CommonParameters = CommonParameters;
	#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
			FRDGTextureDesc SceneColorDesc = FRDGTextureDesc::Create2D(InputExtents, PassInputs.SceneColor.Texture->Desc.Format,
	#else
			FRDGTextureDesc SceneColorDesc = FRDGTextureDesc::Create2D(InputExtents, PassInputs.SceneColorTexture->Desc.Format,
	#endif
				FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
			EPixelFormat SceneColorFormat = SceneColorDesc.Format;

			const FViewMatrices& ViewMatrices = View.ViewMatrices;
			const FViewMatrices& PrevViewMatrices = View.PrevViewInfo.ViewMatrices;

			FVector DeltaTranslation = PrevViewMatrices.GetPreViewTranslation() - ViewMatrices.GetPreViewTranslation();
			FMatrix InvViewProj = ViewMatrices.ComputeInvProjectionNoAAMatrix() * (ViewMatrices.GetTranslatedViewMatrix().RemoveTranslation().GetTransposed());
			FMatrix PrevViewProj = FTranslationMatrix(DeltaTranslation) * PrevViewMatrices.GetTranslatedViewMatrix() * PrevViewMatrices.ComputeProjectionNoAAMatrix();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
			PassParameters->InputDepth = PassInputs.SceneDepth.Texture;
			PassParameters->InputVelocity = PassInputs.SceneVelocity.Texture;
#else
			PassParameters->InputDepth = PassInputs.SceneDepthTexture;
			PassParameters->InputVelocity = PassInputs.SceneVelocityTexture;
	#endif
			PassParameters->Exposure_co_rcp = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure;
			PassParameters->ReClipToPrevClip = FMatrix44f(InvViewProj * PrevViewProj);
			PassParameters->PointClamp = TStaticSamplerState<SF_Point>::GetRHI();
			PassParameters->AngleVertical = tan(View.ViewMatrices.ComputeHalfFieldOfViewPerAxis().Y) * InputExtents.X / InputExtents.Y;
			PassParameters->RenderTargets[0] = FRenderTargetBinding(Output1.Texture, ERenderTargetLoadAction::ENoAction);

			FSGSRConvertPS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FSGSRConvertPS::FBasePermutationDomain>(BasePermutationVector);
			TShaderMapRef<FSGSRConvertPS> PixelShader(View.ShaderMap, PermutationVector);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
			const FScreenPassTextureViewport InputViewport(PassInputs.SceneColor);
#else
			const FScreenPassTextureViewport InputViewport(PassInputs.SceneColorTexture);
#endif
			AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("Qualcomm-SGSR2/Convert (PS)"), View, InputViewport, InputViewport, PixelShader, PassParameters);

			GEngine->AddOnScreenDebugMessage(1, 1.0f, FColor::Orange, FString::Printf(TEXT("SGSR - 1. Convert %dx%d -> %dx%d (PS)"), InputViewport.Rect.Width(), InputViewport.Rect.Height(), InputViewport.Rect.Width(), InputViewport.Rect.Height()));
		}

		FRDGTextureRef BlackDummy = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
		FSGSRStateTextures PrevHistory;
		if (!bHistoryValid)
		{
			PrevHistory.Color = BlackDummy;
		}
		else
		{
			PrevHistory.Color = InputHistory.RT[0] ? GraphBuilder.RegisterExternalTexture(InputHistory.RT[0]) : BlackDummy;
		}

		const bool bSupportsAlpha = IsAlphaSupported();
		FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(
			HistoryExtents,
			bSupportsAlpha || (CVarSGSRHistory.GetValueOnRenderThread() == 0) || IsOpenGLPlatform(View.GetShaderPlatform()) ? PF_FloatRGBA : PF_FloatR11G11B10,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);

		FRDGTextureRef ColorOutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("GSR.Output"));
		FScreenPassRenderTarget Output2 = FScreenPassRenderTarget(ColorOutputTexture, OutputRect, ERenderTargetLoadAction::ENoAction);

		{
			FSGSRUpscalePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSGSRUpscalePS::FParameters>();
			PassParameters->CommonParameters = CommonParameters;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
			PassParameters->InputColor = PassInputs.SceneColor.Texture;
#else
			PassParameters->InputColor = PassInputs.SceneColorTexture;
#endif
			PassParameters->MotionDepthClipAlphaBuffer = Output1.Texture;
			PassParameters->PrevHistoryOutput = PrevHistory.Color;
			PassParameters->Exposure_co_rcp = View.PrevViewInfo.SceneColorPreExposure / View.PreExposure;
			PassParameters->ValidReset = bReset;
			PassParameters->MinLerpContribution = 0.0f;
			if (bSameCamera)
			{
				SameFrameNum += 1.0f;
				if (SameFrameNum > 1)
				{
					PassParameters->MinLerpContribution = 0.3f;
				}
			}
			else
			{
				SameFrameNum = 0.0f;
			}

			float scalefactor = FMath::Min(20.0f, (float)pow((CommonParameters.HistoryInfo.ViewportSize / CommonParameters.InputInfo.ViewportSize).X * (CommonParameters.HistoryInfo.ViewportSize / CommonParameters.InputInfo.ViewportSize).Y, 3.0f));
			PassParameters->Scalefactor = scalefactor;
			float biasmax_viewportXScale = FMath::Min((CommonParameters.HistoryInfo.ViewportSize / CommonParameters.InputInfo.ViewportSize).X, 1.99f);
			PassParameters->Biasmax_viewportXScale = biasmax_viewportXScale;
			PassParameters->LinearClamp = TStaticSamplerState<SF_Bilinear>::GetRHI();

			PassParameters->RenderTargets[0] = FRenderTargetBinding(Output2.Texture, ERenderTargetLoadAction::ENoAction);

			FSGSRUpscalePS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FSGSRUpscalePS::FSampleNumberDim>(CVarSGSRSample.GetValueOnRenderThread());
			PermutationVector.Set<FSGSRUpscalePS::FLanczosOptDim>(CVarSGSRLanczos.GetValueOnRenderThread());
			PermutationVector.Set<FSGSRUpscalePS::FThinFeatureDim>(CVarSGSRThinFeature.GetValueOnRenderThread());
			PermutationVector.Set<FSGSRUpscalePS::FBasePermutationDomain>(BasePermutationVector);

			TShaderMapRef<FSGSRUpscalePS> PixelShader(View.ShaderMap, PermutationVector);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
			const FScreenPassTextureViewport InputViewport(PassInputs.SceneColor);
#else
			const FScreenPassTextureViewport InputViewport(PassInputs.SceneColorTexture);
#endif
			const FScreenPassTextureViewport OutputViewport(Output2.Texture);
			AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("Qualcomm-SGSR/Upscale (PS)"), View, OutputViewport, InputViewport, PixelShader, PassParameters);
			GEngine->AddOnScreenDebugMessage(2, 1.0f, FColor::Yellow, FString::Printf(TEXT("SGSR - 2. Upscale %dx%d -> %dx%d (PS)"), InputViewport.Rect.Width(), InputViewport.Rect.Height(), OutputViewport.Rect.Width(), OutputViewport.Rect.Height()));
		}

		if (bWritePrevViewInfo)
		{
			GraphBuilder.QueueTextureExtraction(Output2.Texture, &OutputHistory->RT[0]);
		}

		Outputs.FullRes.Texture = Output2.Texture;
		Outputs.FullRes.ViewRect = FIntRect(FIntPoint::ZeroValue, View.GetSecondaryViewRectSize());

		Cleanup();
		return Outputs;
	}
}

SGSRPassOutput FSGSRTU::AddPasses_3PassCS(FRDGBuilder& GraphBuilder, const SGSRView& SceneView, const SGSRPassInput& PassInputs) const
{
	const FViewInfo& View = (FViewInfo&)(SceneView);
	FIntPoint InputExtents = View.ViewRect.Size();
	FIntRect InputRect = View.ViewRect;
	FIntPoint InputExtentsQuantized;
	FIntPoint OutputExtents;
	FIntPoint OutputExtentsQuantized;
	FIntRect OutputRect;
	FIntPoint HistoryExtents;
	FIntPoint HistoryExtentsQuantized;
	FIntRect HistoryRect;
	const FViewUniformShaderParameters& ViewUniformParams = *View.CachedViewUniformShaderParameters;
	FIntPoint DepthExtents = FIntPoint(ViewUniformParams.BufferSizeAndInvSize.X, ViewUniformParams.BufferSizeAndInvSize.Y);
	FIntRect DepthRect = FIntRect(FIntPoint(0, 0), DepthExtents);

	if (View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale)
	{
		OutputRect = FIntRect(FIntPoint(0, 0), View.GetSecondaryViewRectSize());
		OutputExtents = View.GetSecondaryViewRectSize();
		QuantizeSceneBufferSize(InputExtents, InputExtentsQuantized);
		QuantizeSceneBufferSize(OutputExtents, OutputExtentsQuantized);
		OutputExtents = FIntPoint(FMath::Max(InputExtents.X, OutputExtentsQuantized.X), FMath::Max(InputExtents.Y, OutputExtentsQuantized.Y));
	}
	else
	{
		OutputRect = FIntRect(FIntPoint(0, 0), View.ViewRect.Size());
		OutputExtents = InputExtents;
	}

	FIntPoint HistorySize;
	float HistoryFactor = FMath::Clamp(CVarSGSRHistorySize.GetValueOnRenderThread() / 100.0f, 1.0f, 2.0f);
	HistoryExtents = FIntPoint(FMath::CeilToInt(OutputExtents.X * HistoryFactor),
		FMath::CeilToInt(OutputExtents.Y * HistoryFactor));
	HistorySize = FIntPoint(FMath::CeilToInt(OutputExtents.X * HistoryFactor),
		FMath::CeilToInt(OutputExtents.Y * HistoryFactor));

	HistoryRect = FIntRect(FIntPoint(0, 0), HistorySize);
	// Quantize the buffers to match UE behavior
	QuantizeSceneBufferSize(HistoryRect.Max, HistoryExtentsQuantized);

	bool const bValidEyeAdaptation = View.HasValidEyeAdaptationBuffer();
	bool const bHasAutoExposure = bValidEyeAdaptation || CVarSGSRExposure.GetValueOnRenderThread();

	check((View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale));
	{
		ISGSRTemporalUpscaler::FOutputs Outputs;
#if ENGINE_MINOR_VERSION < 8
		RDG_GPU_STAT_SCOPE(GraphBuilder, SGSRPass);
#else
		RDG_EVENT_SCOPE_STAT(GraphBuilder, SGSRPass, "SGSRPass");
#endif
		RDG_EVENT_SCOPE(GraphBuilder, "SGSR Temporal Upscaler - 3Pass Compute Shader");

		GEngine->AddOnScreenDebugMessage(0, 1.0f, FColor::Red, FString::Printf(TEXT("SGSR Temporal Upscaler - 3Pass Compute Shader")));

		const bool bWritePrevViewInfo = !View.bStatePrevViewInfoIsReadOnly && View.ViewState;

		bool bHistoryValid = HistoryColorRT.IsValid() && View.ViewState && !View.bCameraCut;

		SGSRstateRef GSRState;
		const FTemporalAAHistory& InputHistory = View.PrevViewInfo.TemporalAAHistory;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
		const TRefCountPtr<ICustomTemporalAAHistory> PrevCustomHistory = PassInputs.PrevHistory;
#else
		const TRefCountPtr<ICustomTemporalAAHistory> PrevCustomHistory = View.PrevViewInfo.CustomTemporalAAHistory;
#endif
		FGSRTUHistory* CustomHistory = static_cast<FGSRTUHistory*>(PrevCustomHistory.GetReference());

		FTemporalAAHistory* OutputHistory = View.ViewState ? &(View.ViewState->PrevFrameViewInfo.TemporalAAHistory) : nullptr;

		/// reuse states to reduce memory churn caused by camera cuts
		bool HasValidContext = CustomHistory && CustomHistory->Getstate().IsValid();

		if (HasValidContext)
		{

			if (CustomHistory->Getstate()->LastUsedFrame == GFrameCounterRenderThread)
			{
				HasValidContext = false;
			}
			else
			{
				GSRState = CustomHistory->Getstate();
			}
		}

		if (!HasValidContext)
		{
			TLockFreePointerListFIFO<FSGSRState, PLATFORM_CACHE_LINE_SIZE> Reusablestates;
			FSGSRState* Ptr = Availablestates.Pop();
			while (Ptr)
			{

				if (Ptr->LastUsedFrame == GFrameCounterRenderThread && Ptr->ViewID != View.ViewState->UniqueID)
				{
					// These states can't be reused immediately but perhaps a future frame, otherwise we break split screen.
					Reusablestates.Push(Ptr);
				}

				else
				{
					GSRState = Ptr;
					Ptr->Release();
					HasValidContext = true;
					bHistoryValid = false;
					break;
				}
				Ptr = Availablestates.Pop();
			}

			Ptr = Reusablestates.Pop();
			while (Ptr)
			{
				Availablestates.Push(Ptr);
				Ptr = Reusablestates.Pop();
			}

			GSRState = new FSGSRState();
		}

		GSRState->LastUsedFrame = GFrameCounterRenderThread;
		GSRState->ViewID = View.ViewState->UniqueID;

		/////prepare the view to receive history data
		if (bWritePrevViewInfo)
		{
			// Releases the existing history texture inside the wrapper object, this doesn't release NewHistory itself
			View.ViewState->PrevFrameViewInfo.TemporalAAHistory.SafeRelease();

			View.ViewState->PrevFrameViewInfo.TemporalAAHistory.ViewportRect = FIntRect(FIntPoint(0, 0), HistoryRect.Size());
			View.ViewState->PrevFrameViewInfo.TemporalAAHistory.ReferenceBufferSize = HistoryExtents;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
			Outputs.NewHistory = new FGSRTUHistory(GSRState, const_cast<FSGSRTU*>(this));
#else
			if (!View.ViewState->PrevFrameViewInfo.CustomTemporalAAHistory.GetReference())
			{
				View.ViewState->PrevFrameViewInfo.CustomTemporalAAHistory = new FGSRTUHistory(GSRState, const_cast<FSGSRTU*>(this));
			}
#endif
		}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
		else
		{
			Outputs.NewHistory = PassInputs.PrevHistory;
		}
#endif

		bool bReset = !bHistoryValid || View.bCameraCut || !OutputHistory;

		// Determine if we want/can use the 16bit shader variants
		const EShaderPlatform ShaderPlatform = View.GetShaderPlatform();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
		auto realTypeSupport = FDataDrivenShaderPlatformInfo::GetSupportsRealTypes(ShaderPlatform);
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
		bool bUseFp16 = ((GRHIGlobals.SupportsNative16BitOps || ShaderPlatform == SP_VULKAN_SM5_ANDROID) && realTypeSupport == ERHIFeatureSupport::RuntimeDependent);
		bUseFp16 = bUseFp16 || (realTypeSupport == ERHIFeatureSupport::RuntimeGuaranteed);
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
		bool bUseFp16 = (ShaderPlatform == SP_VULKAN_SM5_ANDROID && realTypeSupport == ERHIFeatureSupport::RuntimeDependent);
		bUseFp16 = bUseFp16 || (realTypeSupport == ERHIFeatureSupport::RuntimeGuaranteed);
#else
		bool bUseFp16 = ShaderPlatform == SP_VULKAN_SM5_ANDROID;
#endif
		
		bUseFp16 = bUseFp16 && CVarSGSRHalfPrecision.GetValueOnRenderThread();

		// All shaders use the same base (with the same bUseFp16 setting)
		FSGSRShader::FBasePermutationDomain BasePermutationVector;
		BasePermutationVector.Set<FSGSRShader::FConfigCompileFp16>(bUseFp16);

		FSGSRCommonParameters CommonParameters;
		{
			CommonParameters.InputInfo = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(
				InputExtents, InputRect));
			CommonParameters.DepthInfo = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(
				DepthExtents, DepthRect));
			CommonParameters.HistoryInfo = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(
				HistoryExtents, HistoryRect));
			CommonParameters.OutputInfo = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(
				OutputExtents, OutputRect));
			CommonParameters.InputJitter = FVector2f(View.TemporalJitterPixels);
			CommonParameters.ViewUniformBuffer = View.ViewUniformBuffer;
		}

		auto DebugUAV = [&](const FIntPoint& Extent, const TCHAR* DebugName) {
			FRDGTextureDesc DebugDesc = FRDGTextureDesc::Create2D(
				Extent,
				PF_FloatRGBA,
				FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_UAV);

			FRDGTextureRef DebugTexture = GraphBuilder.CreateTexture(DebugDesc, DebugName);

			return GraphBuilder.CreateUAV(DebugTexture);
		};

		FRDGTextureRef BlackDummy = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);

		FSGSRStateTextures PrevHistory;
		if (!bHistoryValid)
		{
			PrevHistory.Color = BlackDummy;
		}
		else
		{
			PrevHistory.Color = GraphBuilder.RegisterExternalTexture(HistoryColorRT);
		}

		FSGSRStateTextures History;
		FRDGTextureDesc HistoryDesc = FRDGTextureDesc::Create2D(
			HistoryExtents,
			PF_FloatRGBA,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV);

		History.Color = GraphBuilder.CreateTexture(HistoryDesc, TEXT("GSRstate Color"));

		HistoryDesc.Format = PF_FloatRGBA;
		HistoryDesc.Extent = InputExtents;
		FRDGTextureRef DilatedMotionDepthLuma = GraphBuilder.CreateTexture(HistoryDesc, TEXT("GSRstate DilatedMotionDepthLuma"));

		FRDGTextureRef YCoCgLuma;
		FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
			InputExtents,
			PF_FloatRGBA,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV);
		Desc.Format = PF_FloatRGBA;
		YCoCgLuma = GraphBuilder.CreateTexture(Desc, TEXT("GSR.YCoCgLuma"));

		{
			FSGSRConvertCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSGSRConvertCS::FParameters>();
			PassParameters->CommonParameters = CommonParameters;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
			PassParameters->InputColor = PassInputs.SceneColor.Texture;

			FRDGTextureDesc SceneColorDesc = FRDGTextureDesc::Create2D(InputExtents, PassInputs.SceneColor.Texture->Desc.Format,
#else 
			PassParameters->InputColor = PassInputs.SceneColorTexture;

			FRDGTextureDesc SceneColorDesc = FRDGTextureDesc::Create2D(InputExtents, PassInputs.SceneColorTexture->Desc.Format,
#endif
				FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
			EPixelFormat SceneColorFormat = SceneColorDesc.Format;

			const FViewMatrices& ViewMatrices = View.ViewMatrices;
			const FViewMatrices& PrevViewMatrices = View.PrevViewInfo.ViewMatrices;

			FVector DeltaTranslation = PrevViewMatrices.GetPreViewTranslation() - ViewMatrices.GetPreViewTranslation();
			FMatrix InvViewProj = ViewMatrices.ComputeInvProjectionNoAAMatrix() * (ViewMatrices.GetTranslatedViewMatrix().RemoveTranslation().GetTransposed());
			FMatrix PrevViewProj = FTranslationMatrix(DeltaTranslation) * PrevViewMatrices.GetTranslatedViewMatrix() * PrevViewMatrices.ComputeProjectionNoAAMatrix();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
			PassParameters->InputDepth = PassInputs.SceneDepth.Texture;
			PassParameters->InputVelocity = PassInputs.SceneVelocity.Texture;
#else
			PassParameters->InputDepth = PassInputs.SceneDepthTexture;
			PassParameters->InputVelocity = PassInputs.SceneVelocityTexture;
#endif
			PassParameters->ReClipToPrevClip = FMatrix44f(InvViewProj * PrevViewProj);
			PassParameters->PointClamp = TStaticSamplerState<SF_Point>::GetRHI();
			PassParameters->LinearClamp = TStaticSamplerState<SF_Bilinear>::GetRHI();

			PassParameters->YCoCgLuma = GraphBuilder.CreateUAV(YCoCgLuma);
			PassParameters->DilatedMotionDepthLuma = GraphBuilder.CreateUAV(DilatedMotionDepthLuma);

			FSGSRConvertCS::FPermutationDomain PermutationVector;
#if ENGINE_MINOR_VERSION < 8
			PermutationVector.Set<FSGSRConvertCS::FInvertedDepthDim>(bool(ERHIZBuffer::IsInverted));
#else
			PermutationVector.Set<FSGSRConvertCS::FInvertedDepthDim>(true);
#endif
			PermutationVector.Set<FSGSRConvertCS::FBasePermutationDomain>(BasePermutationVector);

			TShaderMapRef<FSGSRConvertCS> ComputeShader(View.ShaderMap, PermutationVector);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GSR Convert %dx%d", InputRect.Width(), InputRect.Height()),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(InputRect.Size(), FIntPoint(16, 8)));
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
			const FScreenPassTextureViewport InputViewport(PassInputs.SceneColor);
#else
			const FScreenPassTextureViewport InputViewport(PassInputs.SceneColorTexture);
#endif
			GEngine->AddOnScreenDebugMessage(1, 1.0f, FColor::Orange, FString::Printf(TEXT("SGSR - 1. Convert %dx%d -> %dx%d (CS)"), InputViewport.Rect.Width(), InputViewport.Rect.Height(), InputViewport.Rect.Width(), InputViewport.Rect.Height()));
		}

		const EPixelFormat maskFormat = IsOpenGLPlatform(GMaxRHIShaderPlatform) ? PF_R32_FLOAT : PF_R16F;
		FRDGTextureRef NewLocks;
		FRDGTextureDesc NewLocksDesc = FRDGTextureDesc::Create2D(
			OutputExtents,
			maskFormat,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV);

		NewLocks = GraphBuilder.CreateTexture(NewLocksDesc, TEXT("GSR.NewLocks"));

		FRDGTextureUAVRef NewLocksUAVRef = GraphBuilder.CreateUAV(NewLocks);

		FRDGTextureRef ReactiveMask;
		FRDGTextureDesc ReactiveMaskDesc = FRDGTextureDesc::Create2D(
			InputExtents,
			maskFormat,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV);

		ReactiveMask = GraphBuilder.CreateTexture(ReactiveMaskDesc, TEXT("GSR.ReactiveMask"));

		{
			FSGSRActivateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSGSRActivateCS::FParameters>();
			PassParameters->CommonParameters = CommonParameters;
			// PF_R32_UINT
			PassParameters->DilatedMotionDepthLuma = DilatedMotionDepthLuma;

#if ENGINE_MINOR_VERSION < 8
			const bool bInverted = bool(ERHIZBuffer::IsInverted);
#else
			const bool bInverted = true;
#endif
			const bool bInfinite = true;
			float fMax;
			float fMin;
			if (bInverted)
			{
				fMin = FLT_MAX;
				fMax = View.ViewMatrices.ComputeNearPlane();
			}
			else
			{
				fMin = View.ViewMatrices.ComputeNearPlane();
				fMax = FLT_MAX;
			}

			const float fQ = fMax / (fMin - fMax);
			const float d = -1.0f; // for clarity

			const float matrix_elem_c[2][2] = {
				{
					fQ,					// non reversed, non infinite
					-1.0f - FLT_EPSILON // non reversed, infinite
				},
				{
					fQ,				   // reversed, non infinite
					0.0f + FLT_EPSILON // reversed, infinite
				}
			};

			const float matrix_elem_e[2][2] = {
				{
					fQ * fMin,			// non reversed, non infinite
					-fMin - FLT_EPSILON // non reversed, infinite
				},
				{
					fQ * fMin, // reversed, non infinite
					fMax	   // reversed, infinite
				}
			};

			const float angleVertical = tan(View.ViewMatrices.ComputeHalfFieldOfViewPerAxis().Y) * 2.0;
			const float aspect = float(InputRect.Width()) / float(InputRect.Height());
			const float cotHalfFovY = cosf(0.5f * angleVertical) / sinf(0.5f * angleVertical);
			const float a = cotHalfFovY / aspect;
			const float b = cotHalfFovY;

			PassParameters->DeviceToViewDepth = FVector4f(d * matrix_elem_c[bInverted][bInfinite], matrix_elem_e[bInverted][bInfinite], (1.0f / a), (1.0f / b));

			PassParameters->PointClamp = TStaticSamplerState<SF_Point>::GetRHI();
			PassParameters->LinearClamp = TStaticSamplerState<SF_Bilinear>::GetRHI();

			PassParameters->ReactiveMask = GraphBuilder.CreateUAV(ReactiveMask);
			PassParameters->NewLocks = NewLocksUAVRef;

			FSGSRActivateCS::FPermutationDomain PermutationVector;
#if ENGINE_MINOR_VERSION < 8
			PermutationVector.Set<FSGSRActivateCS::FInvertedDepthDim>(bool(ERHIZBuffer::IsInverted));
#else
			PermutationVector.Set<FSGSRActivateCS::FInvertedDepthDim>(true);
#endif
			PermutationVector.Set<FSGSRActivateCS::FPixelLockDim>(CVarSGSRPixelLock.GetValueOnRenderThread());
			PermutationVector.Set<FSGSRActivateCS::FBasePermutationDomain>(BasePermutationVector);

			TShaderMapRef<FSGSRActivateCS> ComputeShader(View.ShaderMap, PermutationVector);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GSR Activate %dx%d", InputRect.Width(), InputRect.Height()),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(InputRect.Size(), FIntPoint(16, 8)));
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
			const FScreenPassTextureViewport InputViewport(PassInputs.SceneColor);
#else
			const FScreenPassTextureViewport InputViewport(PassInputs.SceneColorTexture);
#endif
			const FScreenPassTextureViewport OutputViewport(ReactiveMask);

			GEngine->AddOnScreenDebugMessage(2, 1.0f, FColor::Yellow, FString::Printf(TEXT("SGSR - 2. Activate %dx%d -> %dx%d (CS)"), InputViewport.Rect.Width(), InputViewport.Rect.Height(), OutputViewport.Rect.Width(), OutputViewport.Rect.Height()));
		}

		const bool bSupportsAlpha = IsAlphaSupported();

		FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(
			HistoryExtents,
			bSupportsAlpha || (CVarSGSRHistory.GetValueOnRenderThread() == 0) || IsOpenGLPlatform(View.GetShaderPlatform()) ? PF_FloatRGBA : PF_FloatR11G11B10,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV);

		FRDGTextureRef ColorOutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("GSR.Output"));

		{
			FSGSRUpscaleCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSGSRUpscaleCS::FParameters>();
			PassParameters->CommonParameters = CommonParameters;
			PassParameters->YCoCgLuma = YCoCgLuma;
			PassParameters->DilatedMotionDepthLuma = DilatedMotionDepthLuma;
			PassParameters->PrevHistoryOutput = PrevHistory.Color;
			PassParameters->ReactiveMask = ReactiveMask;
			float scalefactor = FMath::Min(20.0f, (float)pow((CommonParameters.HistoryInfo.ViewportSize / CommonParameters.InputInfo.ViewportSize).X * (CommonParameters.HistoryInfo.ViewportSize / CommonParameters.InputInfo.ViewportSize).Y, 3.0f));
			PassParameters->Scalefactor = scalefactor;
			PassParameters->ValidReset = bReset;
			PassParameters->JitterSeqLength = View.TemporalJitterSequenceLength;
			PassParameters->PointClamp = TStaticSamplerState<SF_Point>::GetRHI();
			PassParameters->LinearClamp = TStaticSamplerState<SF_Bilinear>::GetRHI();

			PassParameters->HistoryOutput = GraphBuilder.CreateUAV(History.Color);
			PassParameters->SceneColorOutput = GraphBuilder.CreateUAV(ColorOutputTexture);
			PassParameters->NewLocks = NewLocksUAVRef;

			FSGSRUpscaleCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FSGSRUpscaleCS::FSampleNumberDim>(CVarSGSRSample.GetValueOnRenderThread());
			PermutationVector.Set<FSGSRUpscaleCS::FDoSharpeningDim>(CVarSGSRSharpening.GetValueOnRenderThread());
#if ENGINE_MINOR_VERSION < 8
			PermutationVector.Set<FSGSRUpscaleCS::FInvertedDepthDim>(bool(ERHIZBuffer::IsInverted));
#else
			PermutationVector.Set<FSGSRUpscaleCS::FInvertedDepthDim>(true);
#endif
			PermutationVector.Set<FSGSRUpscaleCS::FPixelLockDim>(CVarSGSRPixelLock.GetValueOnRenderThread());
			PermutationVector.Set<FSGSRUpscaleCS::FBasePermutationDomain>(BasePermutationVector);
			TShaderMapRef<FSGSRUpscaleCS> ComputeShader(View.ShaderMap, PermutationVector);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GSR Update %dx%d", HistoryRect.Width(), HistoryRect.Height()),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(HistoryRect.Size(), FIntPoint(16, 8)));

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
			const FScreenPassTextureViewport InputViewport(PassInputs.SceneColor);
#else
			const FScreenPassTextureViewport InputViewport(PassInputs.SceneColorTexture);
#endif
			const FScreenPassTextureViewport OutputViewport(ColorOutputTexture);

			GEngine->AddOnScreenDebugMessage(3, 1.0f, FColor::Green, FString::Printf(TEXT("SGSR - 3. Update %dx%d -> %dx%d (CS)"), InputViewport.Rect.Width(), InputViewport.Rect.Height(), OutputViewport.Rect.Width(), OutputViewport.Rect.Height()));
		}

		bool sharpenEnable = bool(CVarSGSRSharpening.GetValueOnRenderThread());
		if (sharpenEnable)
		{
			FSGSRSharpenCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSGSRSharpenCS::FParameters>();
			FSGSRSharpenCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FSGSRSharpenCS::FBasePermutationDomain>(BasePermutationVector);
			TShaderMapRef<FSGSRSharpenCS> ComputeShader(View.ShaderMap, PermutationVector);
			PassParameters->Input = History.Color;
			PassParameters->Sharpness = (CVarSGSRSharpness.GetValueOnRenderThread());
			PassParameters->PointClamp = TStaticSamplerState<SF_Point>::GetRHI();
			PassParameters->LinearClamp = TStaticSamplerState<SF_Bilinear>::GetRHI();
			PassParameters->UpscaledOutput = GraphBuilder.CreateUAV(ColorOutputTexture);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GSR Sharpen %dx%d", HistoryRect.Width(), HistoryRect.Height()),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(HistoryRect.Size(), FIntPoint(16, 8)));

			const FScreenPassTextureViewport OutputViewport(ColorOutputTexture);
			GEngine->AddOnScreenDebugMessage(4, 1.0f, FColor::Blue, FString::Printf(TEXT("SGSR - 4. Sharpen %dx%d -> %dx%d (CS)"), OutputViewport.Rect.Width(), OutputViewport.Rect.Height(), OutputViewport.Rect.Width(), OutputViewport.Rect.Height()));
		}

		if (bWritePrevViewInfo)
		{
			GraphBuilder.QueueTextureExtraction(History.Color, &HistoryColorRT);
			if (CVarSGSRPixelLock.GetValueOnRenderThread())
			{
				GraphBuilder.QueueTextureExtraction(NewLocks, &NewLocksRT);
			}
		}

		Outputs.FullRes.Texture = ColorOutputTexture;
		Outputs.FullRes.ViewRect = FIntRect(FIntPoint::ZeroValue, View.GetSecondaryViewRectSize());

		Cleanup();
		return Outputs;
	}
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
ISGSRTemporalUpscaler* FSGSRTU::Fork_GameThread(const class FSceneViewFamily& InViewFamily) const
{
	static const FSGSRTUModule& SGSRModule = FModuleManager::GetModuleChecked<FSGSRTUModule>(TEXT("SGSRTUModule"));
	return new FSGSRTUFork(SGSRModule.GetTU());
}
#endif

float FSGSRTU::GetMinUpsampleResolutionFraction() const
{
	return GSR_GetResolutionFraction(MinResolutionQualitymode);
}

float FSGSRTU::GetMaxUpsampleResolutionFraction() const
{
	return GSR_GetResolutionFraction(MaxResolutionQualitymode);
}

//////used to pick out translucency data not captured in separate translucency
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
void FSGSRTU::CopyOpaqueColor(FRDGBuilder& GraphBuilder, TConstStridedView<FSceneView> Views, const class FShaderParametersMetadata* SceneTexturesUniformBufferStruct, FRHIUniformBuffer* SceneTexturesUniformBuffer)
#else
void FSGSRTU::CopyOpaqueColor(FRDGBuilder& GraphBuilder, TConstArrayView<FViewInfo> Views, const class FShaderParametersMetadata* SceneTexturesUniformBufferStruct, FRHIUniformBuffer* SceneTexturesUniformBuffer)
#endif
{
	static const auto CVarGSREnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SGSR.Enabled"));
	if (CVarGSREnabled && CVarGSREnabled->GetValueOnRenderThread() && Views.Num() > 0)
	{
		FRHIUniformBuffer* ViewUniformBuffer = Views[0].ViewUniformBuffer;

		const FSceneTextures* SceneTextures = nullptr;


		FIntPoint SceneColorSize = FIntPoint::ZeroValue;
		for (auto const& SceneView : Views)
		{
			if (SceneView.bIsViewInfo == false)
				continue;

			const FViewInfo& View = (FViewInfo&)(SceneView);
			if (!SceneTextures)
			{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
				SceneTextures = ((FViewFamilyInfo*)View.Family)->GetSceneTexturesChecked();
#else
				SceneTextures = &FSceneTextures::Get(GraphBuilder);
#endif
			}

			SceneColorSize.X = FMath::Max(SceneColorSize.X, View.ViewRect.Max.X);
			SceneColorSize.Y = FMath::Max(SceneColorSize.Y, View.ViewRect.Max.Y);
		}
		check(SceneColorSize.X > 0 && SceneColorSize.Y > 0);
		FIntPoint QuantizedSize;
		QuantizeSceneBufferSize(SceneColorSize, QuantizedSize);

		FRDGTextureMSAA Opaque = SceneTextures->Color;
		auto const& Config = SceneTextures->Config;
		EPixelFormat SceneColorFormat = Config.ColorFormat;
		uint32 NumSamples = Config.NumSamples;

		if (SceneColorpreAlpha.GetReference())
		{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
			if (SceneColorpreAlpha->GetSizeX() != QuantizedSize.X
				|| SceneColorpreAlpha->GetSizeY() != QuantizedSize.Y
#else
			if (SceneColorpreAlpha->GetSizeXYZ().X != QuantizedSize.X
				|| SceneColorpreAlpha->GetSizeXYZ().Y != QuantizedSize.Y
#endif
				|| SceneColorpreAlpha->GetFormat() != SceneColorFormat
				|| SceneColorpreAlpha->GetNumSamples() != NumSamples)
			{
				SceneColorpreAlpha.SafeRelease();
				SceneColorpreAlphaRT.SafeRelease();
			}
		}

		if (this->SceneColorpreAlpha.GetReference() == nullptr)
		{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
			FRHITextureCreateDesc SceneColorpreAlphaCreateDesc = FRHITextureCreateDesc::Create2D(TEXT("SGSR2OpaqueSceneColor"), QuantizedSize.X, QuantizedSize.Y, SceneColorFormat);
			SceneColorpreAlphaCreateDesc.SetNumMips(1);
			SceneColorpreAlphaCreateDesc.SetNumSamples(NumSamples);
			SceneColorpreAlphaCreateDesc.SetFlags((ETextureCreateFlags)(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource));
			SceneColorpreAlpha = RHICreateTexture(SceneColorpreAlphaCreateDesc);
#else
			FRHIResourceCreateInfo CreateInfo(TEXT("SGSR2OpaqueSceneColor"));
			SceneColorpreAlpha = RHICreateTexture2D(QuantizedSize.X, QuantizedSize.Y, SceneColorFormat, 1, NumSamples, (ETextureCreateFlags)(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource), CreateInfo);
#endif
			SceneColorpreAlphaRT = CreateRenderTarget(this->SceneColorpreAlpha.GetReference(), TEXT("SGSR2OpaqueSceneColor"));
		}

		FSGSRFX::FParameters* PassParameters = GraphBuilder.AllocParameters<FSGSRFX::FParameters>();
		FRDGTextureRef SceneColoropaqueRDG = GraphBuilder.RegisterExternalTexture(SceneColorpreAlphaRT);
		PassParameters->InputColor = Opaque.Target;
		PassParameters->OutputColor = SceneColoropaqueRDG;

		GraphBuilder.AddPass(RDG_EVENT_NAME("FSGSRFXSystem::PostRenderOpaque"), PassParameters, ERDGPassFlags::Copy,
			[this, PassParameters, ViewUniformBuffer, Opaque](FRHICommandListImmediate& RHICmdList)
			{
				FTextureRHIRef SceneColor;
				if (Opaque.Target)
				{
					SceneColor = Opaque.Target->GetRHI();
				}
				RHICmdList.Transition(FRHITransitionInfo(SceneColor, ERHIAccess::RTV, ERHIAccess::CopySrc));
				RHICmdList.Transition(FRHITransitionInfo(SceneColorpreAlpha, ERHIAccess::Unknown, ERHIAccess::CopyDest));
				PassParameters->InputColor->MarkResourceAsUsed();
				PassParameters->OutputColor->MarkResourceAsUsed();
				/*this->Opaque = Opaque;*/
				/*this->CopyOpaqueSceneColor(RHICmdList, ViewUniformBuffer, nullptr, this->SceneTexturesUniformParams);*/
				SCOPED_DRAW_EVENTF(RHICmdList, GSRTU_CopyOpaqueScenecolor, TEXT("GSRTU CopyOpaqueSceneColor"));

				FRHICopyTextureInfo Info;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
				Info.Size.X = FMath::Min(SceneColorpreAlpha->GetSizeX(), (uint32)SceneColor->GetSizeXYZ().X);
				Info.Size.Y = FMath::Min(SceneColorpreAlpha->GetSizeY(), (uint32)SceneColor->GetSizeXYZ().Y);
#else
				Info.Size.X = FMath::Min((uint32)SceneColorpreAlpha->GetSizeXYZ().X, (uint32)SceneColor->GetSizeXYZ().X);
				Info.Size.Y = FMath::Min((uint32)SceneColorpreAlpha->GetSizeXYZ().Y, (uint32)SceneColor->GetSizeXYZ().Y);
#endif
				RHICmdList.CopyTexture(SceneColor, SceneColorpreAlpha, Info);

			}
		);
	}
}

///postprocessing inputs for separate translucenecy available to mask
void FSGSRTU::SetPostProcessingInputs(FPostProcessingInputs const& NewInputs)
{
	PostInputs = NewInputs;
}

///release retained resources
void FSGSRTU::EndofFrame()
{
	PostInputs.SceneTextures = nullptr;
	ReflectionTexture = nullptr;
}

FSGSRTUFork::FSGSRTUFork(ISGSRTemporalUpscaler* TU)
	: TemporalUpscaler(TU)
{
}

FSGSRTUFork::~FSGSRTUFork()
{
}

const TCHAR* FSGSRTUFork::GetDebugName() const
{
	return TemporalUpscaler->GetDebugName();
}

ISGSRTemporalUpscaler::FOutputs FSGSRTUFork::AddPasses(
	FRDGBuilder& GraphBuilder,
	const SGSRView& View,
	const SGSRPassInput& PassInputs) const
{
	return TemporalUpscaler->AddPasses(GraphBuilder, View, PassInputs);
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
ISGSRTemporalUpscaler* FSGSRTUFork::Fork_GameThread(const class FSceneViewFamily& InViewFamily) const
{
	return new FSGSRTUFork(TemporalUpscaler);
}
#endif

float FSGSRTUFork::GetMinUpsampleResolutionFraction() const
{
	return TemporalUpscaler->GetMinUpsampleResolutionFraction();
}

float FSGSRTUFork::GetMaxUpsampleResolutionFraction() const
{
	return TemporalUpscaler->GetMinUpsampleResolutionFraction();
}
