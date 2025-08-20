//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "GSRTU.h"
#include "GSRTUModule.h"
#include "GSRTUHistory.h"
#include "HAL/Platform.h"
#include "SceneTextureParameters.h"
#include "TranslucentRendering.h"
#include "ScenePrivate.h"
#include "logGSR.h"
#include "LegacyScreenPercentageDriver.h"
#include "PlanarReflectionSceneProxy.h"
#include "Serialization/MemoryImage.h"
#include "Serialization/MemoryLayout.h"
#include "FXSystem.h"
#include "PostProcess/SceneRenderTargets.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RenderGraphUtils.h"
#include "RenderGraphBuilder.h"

DECLARE_GPU_STAT(GSRPass)
DECLARE_GPU_STAT_NAMED(GSRDispatch, TEXT("GSR Dispatch"));

typedef enum GSRQualityMode
{
	GSR_QUALITYMODE_ULTRA_QUALITY = 0,
	GSR_QUALITYMODE_QUALITY = 1,
	GSR_QUALITYMODE_BALANCED = 2,
	GSR_QUALITYMODE_PERFORMANCE = 3,
} GSRQualityMode;

///Quality mode definitions
static const GSRQualityMode DefaultQualitymode = GSR_QUALITYMODE_QUALITY;
static const GSRQualityMode MinResolutionQualitymode = GSR_QUALITYMODE_PERFORMANCE;
static const GSRQualityMode MaxResolutionQualitymode = GSR_QUALITYMODE_ULTRA_QUALITY;

///console variables
static TAutoConsoleVariable<int32> CVarGSRExposure(
	TEXT("r.SGSR2.Exposure"),
	0,
	TEXT("Default 0 to use engine's auto-exposure value, otherwise specific auto-exposure is used"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarGSRHistory(
	TEXT("r.SGSR2.History"),
	0,
	TEXT("Bit-depth for History texture format. 0: PF_FloatRGBA, 1: PF_FloatR11G11B10. Default(0) has better quality but worse bandwidth"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarGSRQuality(
	TEXT("r.SGSR2.Quality"),
	DefaultQualitymode,
	TEXT("Quality Mode 0-4. Higher values refer to better performance, lower values refer to superior images. Default is 1\n")
	TEXT("0 - Ultra Quality 	1.25x\n")
	TEXT("1 - Quality 		1.5x \n")
	TEXT("2 - Balanced 		1.7x \n")
	TEXT("3 - Performance 		2.0x \n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarGSRHistorySize(
	TEXT("r.SGSR2.HistorySize"),
	100.0f,
	TEXT("Screen percentage of upscaler's history."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarGSRSample(
	TEXT("r.SGSR2.5Sample"), 
	1,
	TEXT("Controls the sample number of input color, false to choose 9 sample for better image quality, true to choose 5 sample for better performance, default is 1"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarGSRSharpening(
	TEXT("r.SGSR2.DoSharpening"),
	0,
	TEXT("Do sharpening, default is 0"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarGSRSharpness(
	TEXT("r.SGSR2.Sharpness"),
	1.12,
	TEXT("Sharpness, default is 1.12"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<bool> CVarGSRPixelLock(
	TEXT("r.SGSR2.PixelLock"),
	1,
	TEXT("Enable PixelLock, default is true"),
	ECVF_RenderThreadSafe);


static TAutoConsoleVariable<bool> CVarGSRHalfPrecision(
	TEXT("r.SGSR2.HalfPrecision"),
	1,
	TEXT("Enable Half Precision shader arithmetic (platform support dependent).  May improve shader performance."),
	ECVF_RenderThreadSafe);

float FGSRTU::SavedScreenPercentage{ 100.0f };
static inline float GSR_GetResolutionFraction(GSRQualityMode Qualitymode)
{
	switch (Qualitymode)
	{
		case GSR_QUALITYMODE_ULTRA_QUALITY:
			return 1.0f / 1.25f;
		case GSR_QUALITYMODE_QUALITY:
			return 1.0f / 1.5f;
		case GSR_QUALITYMODE_BALANCED:
			return 1.0f / 1.7f;
		case GSR_QUALITYMODE_PERFORMANCE:
			return 1.0f / 2.0f;
		default:
			return 0.0f;
	}
}

////////COMMON PARAMETERS
BEGIN_SHADER_PARAMETER_STRUCT(FGSRCommonParameters, )
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputInfo)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, DepthInfo)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, HistoryInfo)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutputInfo)

	SHADER_PARAMETER(FVector2f, InputJitter)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FGSRStateTextures, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Color)
END_SHADER_PARAMETER_STRUCT()

class FGSRShader : public FGlobalShader
{
public:
	FGSRShader() = default;
	FGSRShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer) {}

	class FConfigCompileFp16 : SHADER_PERMUTATION_BOOL("CONFIG_COMPILE_FP16");
	using FBasePermutationDomain = TShaderPermutationDomain<FConfigCompileFp16>;

	static bool ShouldCompilePermutationBase(const FGlobalShaderPermutationParameters& Parameters, const FBasePermutationDomain& PermutationVector)
	{
		const ERHIFeatureSupport RealTypeSupport = FDataDrivenShaderPlatformInfo::GetSupportsRealTypes(Parameters.Platform);
		bool bIs16BitVALUPermutation = PermutationVector.Get<FConfigCompileFp16>();

		if (bIs16BitVALUPermutation && (RealTypeSupport == ERHIFeatureSupport::Unsupported))
		{
			// 16bit unsupported, dont compile the 16bit permutation
			return false;
		}
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
};

class FGSRConvertCS : public FGSRShader
{
	DECLARE_GLOBAL_SHADER(FGSRConvertCS);
	SHADER_USE_PARAMETER_STRUCT(FGSRConvertCS, FGSRShader);
	class FInvertedDepthDim : SHADER_PERMUTATION_BOOL("INVERTED_DEPTH");
	using FPermutationDomain = TShaderPermutationDomain<FGSRShader::FBasePermutationDomain, FInvertedDepthDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FGSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputVelocity)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputColor)
		SHADER_PARAMETER(float, PreExposure)
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

class FGSRActivateCS : public FGSRShader
{
	DECLARE_GLOBAL_SHADER(FGSRActivateCS);
	SHADER_USE_PARAMETER_STRUCT(FGSRActivateCS, FGSRShader);
	class FInvertedDepthDim : SHADER_PERMUTATION_BOOL("INVERTED_DEPTH");
	class FPixelLockDim : SHADER_PERMUTATION_BOOL("ENABLE_PIXEL_LOCK");
	using FPermutationDomain = TShaderPermutationDomain<FGSRShader::FBasePermutationDomain, FInvertedDepthDim, FPixelLockDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FGSRCommonParameters, CommonParameters)
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

class FGSRUpscaleCS : public FGSRShader
{
	DECLARE_GLOBAL_SHADER(FGSRUpscaleCS);
	SHADER_USE_PARAMETER_STRUCT(FGSRUpscaleCS, FGSRShader);
	class FSampleNumberDim : SHADER_PERMUTATION_BOOL("SAMPLE_NUMBER");
	class FDoSharpeningDim : SHADER_PERMUTATION_BOOL("DO_SHARPENING");
	class FInvertedDepthDim : SHADER_PERMUTATION_BOOL("INVERTED_DEPTH");
	class FPixelLockDim : SHADER_PERMUTATION_BOOL("ENABLE_PIXEL_LOCK");
	using FPermutationDomain = TShaderPermutationDomain<FGSRShader::FBasePermutationDomain, FSampleNumberDim, FDoSharpeningDim, FInvertedDepthDim, FPixelLockDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FGSRCommonParameters, CommonParameters)
		//YCoCg->RGB
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, YCoCgLuma)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DilatedMotionDepthLuma)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevHistoryOutput)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ReactiveMask)

		SHADER_PARAMETER_SAMPLER(SamplerState, PointClamp)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
		SHADER_PARAMETER(float, ValidReset)
		SHADER_PARAMETER(float, PreExposure)
		SHADER_PARAMETER(float, JitterSeqLength)
		
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

class FGSRSharpenCS : public FGSRShader
{
	DECLARE_GLOBAL_SHADER(FGSRSharpenCS);
	SHADER_USE_PARAMETER_STRUCT(FGSRSharpenCS, FGSRShader);
	using FPermutationDomain = TShaderPermutationDomain<FGSRShader::FBasePermutationDomain>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FGSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Input)
		SHADER_PARAMETER(float, PreExposure)
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

IMPLEMENT_GLOBAL_SHADER(FGSRConvertCS, "/Plugin/GSR/Private/gsr_convert.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FGSRActivateCS, "/Plugin/GSR/Private/gsr_activate.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FGSRUpscaleCS, "/Plugin/GSR/Private/gsr_upscale.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FGSRSharpenCS, "/Plugin/GSR/Private/gsr_sharpen.usf", "MainCS", SF_Compute);

struct FGSRFX
{
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	RDG_TEXTURE_ACCESS(InputColor, ERHIAccess::CopySrc)
	RDG_TEXTURE_ACCESS(OutputColor, ERHIAccess::CopyDest)
	END_SHADER_PARAMETER_STRUCT()
};

class FGSRFXSystem : public FFXSystemInterface
{
	FGPUSortManager* GPUSortManager;
	FGSRTU* Upscaler;
	FRHIUniformBuffer* SceneTexturesUniformBuffer = nullptr;

public:
	static const FName FXName;

	FFXSystemInterface* GetInterface(const FName& InName) final
	{
		return InName == FGSRFXSystem::FXName ? this : nullptr;
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

	void PreInitViews(FRDGBuilder&, bool, const TArrayView<const FSceneViewFamily*>&, const FSceneViewFamily*) final {};
	void PostInitViews(FRDGBuilder&, TConstStridedView<FSceneView>, bool) final {};

	bool UsesGlobalDistanceField() const final { return false; }
	bool UsesDepthBuffer() const final { return false; }
	bool RequiresEarlyViewUniformBuffer() const final { return false; }
	bool RequiresRayTracingScene() const final { return false; }

	void PreRender(FRDGBuilder&, TConstStridedView<FSceneView>, FSceneUniformBuffer&, bool) final{};
	void PostRenderOpaque(FRDGBuilder& GraphBuilder, TConstStridedView<FSceneView> Views, FSceneUniformBuffer& SceneUniformBuffer, bool bAllowGPUParticleUpdate) final {}
	void SetSceneTexturesUniformBuffer(const TUniformBufferRef<FSceneTextureUniformParameters>& InSceneTexturesUniformParams) 
		final { SceneTexturesUniformBuffer = InSceneTexturesUniformParams; }

	FGPUSortManager* GetGPUSortManager() const
	{
		return GPUSortManager;
	}

	FGSRFXSystem(FGSRTU* InUpscaler, FGPUSortManager* InGPUSortManager)
		: GPUSortManager(InGPUSortManager)
		, Upscaler(InUpscaler)
	{
		check(GPUSortManager && Upscaler);
	}
	~FGSRFXSystem() {}
};
FName const FGSRFXSystem::FXName(TEXT("GSRFXSystem"));

///TU implementation
FGSRTU::FGSRTU()
	: ReflectionTexture(nullptr)
{
	FMemory::Memzero(PostInputs);
	FGSRTU* self = this;
	FFXSystemInterface::RegisterCustomFXSystem(
		FGSRFXSystem::FXName,
		FCreateCustomFXSystemDelegate::CreateLambda([self](ERHIFeatureLevel::Type InFeatureLevel,
		EShaderPlatform InShaderPlatform, FGPUSortManager* InGPUSortManager) -> FFXSystemInterface* {
			return new FGSRFXSystem(self, InGPUSortManager);
		}));

	FConsoleVariableDelegate EnabledChangedDelegate = FConsoleVariableDelegate::CreateStatic(&FGSRTU::ChangeGSREnabled);
	IConsoleVariable* CVarEnabled = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SGSR2.Enabled"));
	CVarEnabled->SetOnChangedCallback(EnabledChangedDelegate);

	FConsoleVariableDelegate QualityChangedDelegate = FConsoleVariableDelegate::CreateStatic(&FGSRTU::ChangeQualityMode);
	CVarGSRQuality->SetOnChangedCallback(QualityChangedDelegate);
	
	if (CVarEnabled->GetBool())
	{
		SaveScreenPercentage();
		UpdateScreenPercentage();
	}
	GEngine->GetDynamicResolutionCurrentStateInfos(DynamicResolutionStateInfos);
}

FGSRTU::~FGSRTU()
{
	Cleanup();
	FFXSystemInterface::UnregisterCustomFXSystem(FGSRFXSystem::FXName);
}

const TCHAR* FGSRTU::GetDebugName() const
{
	return TEXT("FGSRTU");
}

void FGSRTU::Releasestate(GSRstateRef state)
{
	FGSRState* Ptr = state.GetReference();
	if (Ptr)
	{
		Ptr->AddRef();
		Availablestates.Push(Ptr);
	}
}

void FGSRTU::Cleanup() const
{
	FGSRState* Ptr = Availablestates.Pop();
	while (Ptr)
	{
		Ptr->Release();
		Ptr = Availablestates.Pop();
	}
}

float FGSRTU::GetRfraction(uint32 Mode)
{
	GSRQualityMode Qualitymode = FMath::Clamp<GSRQualityMode>((GSRQualityMode)Mode, MaxResolutionQualitymode, MinResolutionQualitymode);
	const float ResolutionFraction = GSR_GetResolutionFraction(Qualitymode);
	return ResolutionFraction;
}

void FGSRTU::SaveScreenPercentage()
{
	SavedScreenPercentage = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage"))->GetValueOnGameThread();
}

void FGSRTU::UpdateScreenPercentage()
{
	float RFraction = GetRfraction(CVarGSRQuality.GetValueOnGameThread());
	static IConsoleVariable* ScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	ScreenPercentage->Set(RFraction * 100.00f);
}


void FGSRTU::RestoreScreenPercentage()
{
	static IConsoleVariable* ScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	ScreenPercentage->Set(SavedScreenPercentage);
}


void FGSRTU::ChangeGSREnabled(IConsoleVariable* Var)
{
	static const auto CVarEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SGSR2.Enabled"));
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

void FGSRTU::ChangeQualityMode(IConsoleVariable* Var)
{
	static const auto CVarEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SGSR2.Enabled"));
	if (CVarEnabled && CVarEnabled->GetValueOnGameThread())
	{
		float RFraction = GetRfraction(CVarGSRQuality.GetValueOnGameThread());
		static IConsoleVariable* ScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
		ScreenPercentage->Set(RFraction * 100.0f);
	}
}

IGSRTemporalUpscaler::FOutputs FGSRTU::AddPasses(
	FRDGBuilder& GraphBuilder,
	const GSRView& SceneView,
	const IGSRTemporalUpscaler::FInputs& PassInputs) const
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
	float HistoryFactor = FMath::Clamp(CVarGSRHistorySize.GetValueOnRenderThread() / 100.0f, 1.0f, 2.0f);
	HistoryExtents = FIntPoint(FMath::CeilToInt(OutputExtents.X * HistoryFactor),
		FMath::CeilToInt(OutputExtents.Y * HistoryFactor));
	HistorySize = FIntPoint(FMath::CeilToInt(OutputExtents.X * HistoryFactor),
		FMath::CeilToInt(OutputExtents.Y * HistoryFactor));

	HistoryRect = FIntRect(FIntPoint(0, 0), HistorySize);
	// Quantize the buffers to match UE behavior
	QuantizeSceneBufferSize(HistoryRect.Max, HistoryExtentsQuantized);

	bool const bValidEyeAdaptation = View.HasValidEyeAdaptationBuffer();
	bool const bHasAutoExposure = bValidEyeAdaptation || CVarGSRExposure.GetValueOnRenderThread();

	check((View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale));
	{
		IGSRTemporalUpscaler::FOutputs Outputs;

		RDG_GPU_STAT_SCOPE(GraphBuilder, GSRPass);
		RDG_EVENT_SCOPE(GraphBuilder, "SGSR2.1");

		const bool bWritePrevViewInfo = !View.bStatePrevViewInfoIsReadOnly && View.ViewState;

		bool bHistoryValid = HistoryColorRT.IsValid() && View.ViewState && !View.bCameraCut;

		GSRstateRef GSRState;
		const FTemporalAAHistory& InputHistory = View.PrevViewInfo.TemporalAAHistory;
		const TRefCountPtr<ICustomTemporalAAHistory> PrevCustomHistory = PassInputs.PrevHistory;

		FGSRTUHistory* CustomHistory = static_cast<FGSRTUHistory*>(PrevCustomHistory.GetReference());

		FTemporalAAHistory* OutputHistory = View.ViewState ? &(View.ViewState->PrevFrameViewInfo.TemporalAAHistory) : nullptr;

		///reuse states to reduce memory churn caused by camera cuts
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
			TLockFreePointerListFIFO<FGSRState, PLATFORM_CACHE_LINE_SIZE> Reusablestates;
			FGSRState* Ptr = Availablestates.Pop();
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

			GSRState = new FGSRState();
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

			Outputs.NewHistory = new FGSRTUHistory(GSRState, const_cast<FGSRTU*>(this)); 
		}
		else
		{
			Outputs.NewHistory = PassInputs.PrevHistory;
		}

		bool bReset = !bHistoryValid || View.bCameraCut || !OutputHistory;


		// Determine if we want/can use the 16bit shader variants
		const EShaderPlatform ShaderPlatform = View.GetShaderPlatform();
		auto realTypeSupport = FDataDrivenShaderPlatformInfo::GetSupportsRealTypes(ShaderPlatform);
		bool bUseFp16 = ((GRHIGlobals.SupportsNative16BitOps || ShaderPlatform == SP_VULKAN_SM5_ANDROID) && realTypeSupport == ERHIFeatureSupport::RuntimeDependent);
		bUseFp16 = bUseFp16 || (realTypeSupport == ERHIFeatureSupport::RuntimeGuaranteed);
		bUseFp16 = bUseFp16 && CVarGSRHalfPrecision.GetValueOnRenderThread();

		// All shaders use the same base (with the same bUseFp16 setting)
		FGSRShader::FBasePermutationDomain BasePermutationVector;
		BasePermutationVector.Set<FGSRShader::FConfigCompileFp16>(bUseFp16);

		FGSRCommonParameters CommonParameters;
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
		
		FGSRStateTextures PrevHistory;
		if (!bHistoryValid)
		{
			PrevHistory.Color = BlackDummy;
		}
		else
		{
			PrevHistory.Color = GraphBuilder.RegisterExternalTexture(HistoryColorRT);
		}

		FGSRStateTextures History;
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
			FGSRConvertCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGSRConvertCS::FParameters>();
			PassParameters->CommonParameters = CommonParameters;
			PassParameters->InputColor = PassInputs.SceneColor.Texture;

			FRDGTextureDesc SceneColorDesc = FRDGTextureDesc::Create2D(InputExtents, PassInputs.SceneColor.Texture->Desc.Format,
				FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
			EPixelFormat SceneColorFormat = SceneColorDesc.Format;

			const FViewMatrices& ViewMatrices = View.ViewMatrices;
			const FViewMatrices& PrevViewMatrices = View.PrevViewInfo.ViewMatrices;

			FVector DeltaTranslation = PrevViewMatrices.GetPreViewTranslation() - ViewMatrices.GetPreViewTranslation();
			FMatrix InvViewProj = ViewMatrices.ComputeInvProjectionNoAAMatrix() * (ViewMatrices.GetTranslatedViewMatrix().RemoveTranslation().GetTransposed());
			FMatrix PrevViewProj = FTranslationMatrix(DeltaTranslation) * PrevViewMatrices.GetTranslatedViewMatrix() * PrevViewMatrices.ComputeProjectionNoAAMatrix();
			
			PassParameters->InputDepth = PassInputs.SceneDepth.Texture;
			PassParameters->InputVelocity = PassInputs.SceneVelocity.Texture;
			PassParameters->PreExposure = View.PreExposure;
			PassParameters->ReClipToPrevClip = FMatrix44f(InvViewProj * PrevViewProj);
			PassParameters->PointClamp = TStaticSamplerState<SF_Point>::GetRHI();
			PassParameters->LinearClamp = TStaticSamplerState<SF_Bilinear>::GetRHI();

			PassParameters->YCoCgLuma = GraphBuilder.CreateUAV(YCoCgLuma);
			PassParameters->DilatedMotionDepthLuma = GraphBuilder.CreateUAV(DilatedMotionDepthLuma);

			FGSRConvertCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FGSRConvertCS::FInvertedDepthDim>(bool(ERHIZBuffer::IsInverted));
			PermutationVector.Set<FGSRConvertCS::FBasePermutationDomain>(BasePermutationVector);

			TShaderMapRef<FGSRConvertCS> ComputeShader(View.ShaderMap, PermutationVector);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GSR Convert %dx%d", InputRect.Width(), InputRect.Height()),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(InputRect.Size(), FIntPoint(16, 8)));
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
			FGSRActivateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGSRActivateCS::FParameters>();
			PassParameters->CommonParameters = CommonParameters;
			PassParameters->DilatedMotionDepthLuma = DilatedMotionDepthLuma;

			const bool bInverted = bool(ERHIZBuffer::IsInverted);
			const bool bInfinite = true;
			float fMax;
			float fMin;
			if (bInverted){
				fMin = FLT_MAX;
				fMax = View.ViewMatrices.ComputeNearPlane();
			}else{
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

			FGSRActivateCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FGSRActivateCS::FInvertedDepthDim>(bool(ERHIZBuffer::IsInverted));
			PermutationVector.Set<FGSRActivateCS::FPixelLockDim>(CVarGSRPixelLock.GetValueOnRenderThread());
			PermutationVector.Set<FGSRActivateCS::FBasePermutationDomain>(BasePermutationVector);

			TShaderMapRef<FGSRActivateCS> ComputeShader(View.ShaderMap, PermutationVector);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GSR Activate %dx%d", InputRect.Width(), InputRect.Height()),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(InputRect.Size(), FIntPoint(16, 8)));
		}

#if ENGINE_MAJOR_VERSION <= 5 && ENGINE_MINOR_VERSION < 5	// before UE5.0
		static const auto CVarPostPropagateAlpha = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessing.PropagateAlpha"));
		const bool bSupportsAlpha = (CVarPostPropagateAlpha && CVarPostPropagateAlpha->GetValueOnRenderThread() != 0);
#else
		static const auto CVarPostPropagateAlpha = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessing.PropagateAlpha"));
		const bool bSupportsAlpha = (CVarPostPropagateAlpha && CVarPostPropagateAlpha->GetBool());
#endif
		FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(
			HistoryExtents,
			bSupportsAlpha || (CVarGSRHistory.GetValueOnRenderThread() == 0) || IsOpenGLPlatform(View.GetShaderPlatform()) ? PF_FloatRGBA : PF_FloatR11G11B10,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV);

		FRDGTextureRef ColorOutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("GSR.Output"));

		{
			FGSRUpscaleCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGSRUpscaleCS::FParameters>();
			PassParameters->CommonParameters = CommonParameters;
			PassParameters->YCoCgLuma = YCoCgLuma;
			PassParameters->DilatedMotionDepthLuma = DilatedMotionDepthLuma;
			PassParameters->NewLocks = NewLocksUAVRef;
			PassParameters->PrevHistoryOutput = PrevHistory.Color;
			PassParameters->ReactiveMask = ReactiveMask;
			PassParameters->PreExposure = View.PreExposure;
			PassParameters->ValidReset = bReset;
			PassParameters->JitterSeqLength = View.TemporalJitterSequenceLength;
			PassParameters->PointClamp = TStaticSamplerState<SF_Point>::GetRHI();
			PassParameters->LinearClamp = TStaticSamplerState<SF_Bilinear>::GetRHI();

			PassParameters->HistoryOutput = GraphBuilder.CreateUAV(History.Color);
			PassParameters->SceneColorOutput = GraphBuilder.CreateUAV(ColorOutputTexture);
			FGSRUpscaleCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FGSRUpscaleCS::FSampleNumberDim>(CVarGSRSample.GetValueOnRenderThread());
			PermutationVector.Set<FGSRUpscaleCS::FDoSharpeningDim>(CVarGSRSharpening.GetValueOnRenderThread());
			PermutationVector.Set<FGSRUpscaleCS::FInvertedDepthDim>(bool(ERHIZBuffer::IsInverted));
			PermutationVector.Set<FGSRUpscaleCS::FPixelLockDim>(CVarGSRPixelLock.GetValueOnRenderThread());
			TShaderMapRef<FGSRUpscaleCS> ComputeShader(View.ShaderMap, PermutationVector);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GSR Update %dx%d", HistoryRect.Width(), HistoryRect.Height()),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(HistoryRect.Size(), FIntPoint(16, 8)));
		}
		
		bool sharpenEnable = bool(CVarGSRSharpening.GetValueOnRenderThread());
		if (sharpenEnable){
			FGSRSharpenCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGSRSharpenCS::FParameters>();
			FGSRSharpenCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FGSRSharpenCS::FBasePermutationDomain>(BasePermutationVector);
			TShaderMapRef<FGSRSharpenCS> ComputeShader(View.ShaderMap, PermutationVector);
			PassParameters->Input = History.Color;
			PassParameters->PreExposure = View.PreExposure;
			PassParameters->Sharpness = (CVarGSRSharpness.GetValueOnRenderThread());
			PassParameters->PointClamp = TStaticSamplerState<SF_Point>::GetRHI();
			PassParameters->LinearClamp = TStaticSamplerState<SF_Bilinear>::GetRHI();
			PassParameters->UpscaledOutput = GraphBuilder.CreateUAV(ColorOutputTexture);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GSR Sharpen %dx%d", HistoryRect.Width(), HistoryRect.Height()),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(HistoryRect.Size(), FIntPoint(16, 8)));
		}

		if (bWritePrevViewInfo)
		{
			GraphBuilder.QueueTextureExtraction(History.Color, &HistoryColorRT);
		}

		Outputs.FullRes.Texture = ColorOutputTexture;
		Outputs.FullRes.ViewRect = FIntRect(FIntPoint::ZeroValue, View.GetSecondaryViewRectSize());

		Cleanup(); 
		return Outputs;
	}
}

IGSRTemporalUpscaler* FGSRTU::Fork_GameThread(const class FSceneViewFamily& InViewFamily) const
{
	static const FGSRTUModule& GSRModule = FModuleManager::GetModuleChecked<FGSRTUModule>(TEXT("GSRTUModule"));
	return new FGSRTUFork(GSRModule.GetTU());
}

float FGSRTU::GetMinUpsampleResolutionFraction() const
{
	return GSR_GetResolutionFraction(MinResolutionQualitymode);
}

float FGSRTU::GetMaxUpsampleResolutionFraction() const
{
	return GSR_GetResolutionFraction(MaxResolutionQualitymode);
}

//////used to pick out translucency data not captured in separate translucency
void FGSRTU::CopyOpaqueColor(FRDGBuilder& GraphBuilder, TConstStridedView<FSceneView> Views, const class FShaderParametersMetadata* SceneTexturesUniformBufferStruct, FRHIUniformBuffer* SceneTexturesUniformBuffer)
{
	static const auto CVarGSREnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SGSR2.Enabled"));
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
				SceneTextures = ((FViewFamilyInfo*)View.Family)->GetSceneTexturesChecked();
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
			if (SceneColorpreAlpha->GetSizeX() != QuantizedSize.X
				|| SceneColorpreAlpha->GetSizeY() != QuantizedSize.Y
				|| SceneColorpreAlpha->GetFormat() != SceneColorFormat
				|| SceneColorpreAlpha->GetNumSamples() != NumSamples)
			{
				SceneColorpreAlpha.SafeRelease();
				SceneColorpreAlphaRT.SafeRelease();
			}
		}

		if (this->SceneColorpreAlpha.GetReference() == nullptr)
		{
			FRHITextureCreateDesc SceneColorpreAlphaCreateDesc = FRHITextureCreateDesc::Create2D(TEXT("SGSR2OpaqueSceneColor"), QuantizedSize.X, QuantizedSize.Y, SceneColorFormat);
			SceneColorpreAlphaCreateDesc.SetNumMips(1);
			SceneColorpreAlphaCreateDesc.SetNumSamples(NumSamples);
			SceneColorpreAlphaCreateDesc.SetFlags((ETextureCreateFlags)(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource));
			SceneColorpreAlpha = RHICreateTexture(SceneColorpreAlphaCreateDesc);
			SceneColorpreAlphaRT = CreateRenderTarget(this->SceneColorpreAlpha.GetReference(), TEXT("SGSR2OpaqueSceneColor"));
		}

		FGSRFX::FParameters* PassParameters = GraphBuilder.AllocParameters<FGSRFX::FParameters>();
		FRDGTextureRef SceneColoropaqueRDG = GraphBuilder.RegisterExternalTexture(SceneColorpreAlphaRT);
		PassParameters->InputColor = Opaque.Target;
		PassParameters->OutputColor = SceneColoropaqueRDG;

		GraphBuilder.AddPass(RDG_EVENT_NAME("FGSRFXSystem::PostRenderOpaque"), PassParameters, ERDGPassFlags::Copy,
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
				SCOPED_DRAW_EVENTF(RHICmdList, GSRTU_CopyOpaqueScenecolor, TEXT("GSRTU CopyOpaqueSceneColor"));

				FRHICopyTextureInfo Info;
				Info.Size.X = FMath::Min(SceneColorpreAlpha->GetSizeX(), (uint32)SceneColor->GetSizeXYZ().X);
				Info.Size.Y = FMath::Min(SceneColorpreAlpha->GetSizeY(), (uint32)SceneColor->GetSizeXYZ().Y);
				RHICmdList.CopyTexture(SceneColor, SceneColorpreAlpha, Info);

			}
		);
	}
}

///postprocessing inputs for separate translucenecy available to mask
void FGSRTU::SetPostProcessingInputs(FPostProcessingInputs const& NewInputs)
{
	PostInputs = NewInputs;
}

///release retained resources
void FGSRTU::EndofFrame()
{
	PostInputs.SceneTextures = nullptr;
	ReflectionTexture = nullptr;
}

FGSRTUFork::FGSRTUFork(IGSRTemporalUpscaler* TU)
	: TemporalUpscaler(TU)
{
}

FGSRTUFork::~FGSRTUFork()
{
}

const TCHAR* FGSRTUFork::GetDebugName() const
{
	return TemporalUpscaler->GetDebugName();
}

IGSRTemporalUpscaler::FOutputs FGSRTUFork::AddPasses(
	FRDGBuilder& GraphBuilder,
	const GSRView& View,
	const GSRPassInput& PassInputs) const
{
	return TemporalUpscaler->AddPasses(GraphBuilder, View, PassInputs);
}

IGSRTemporalUpscaler* FGSRTUFork::Fork_GameThread(const class FSceneViewFamily& InViewFamily) const
{
	return new FGSRTUFork(TemporalUpscaler);
}

float FGSRTUFork::GetMinUpsampleResolutionFraction() const
{
	return TemporalUpscaler->GetMinUpsampleResolutionFraction();
}

float FGSRTUFork::GetMaxUpsampleResolutionFraction() const
{
	return TemporalUpscaler->GetMinUpsampleResolutionFraction();
}
