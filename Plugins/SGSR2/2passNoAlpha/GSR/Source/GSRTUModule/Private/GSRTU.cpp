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
// #include "PostProcess/PostProcessMitchellNetravali.h"

DECLARE_GPU_STAT(GSRPass)
DECLARE_GPU_STAT_NAMED(GSRDispatch, TEXT("GSR Dispatch"));

typedef enum GSRQualityMode
{
	GSR_QUALITYMODE_ULTRA_QUALITY = 0,
	GSR_QUALITYMODE_QUALITY = 1,
	GSR_QUALITYMODE_BALANCED = 2,
	GSR_QUALITYMODE_PERFORMANCE = 3,
} GSRQualityMode;

/// Quality mode definitions
static const GSRQualityMode DefaultQualitymode = GSR_QUALITYMODE_QUALITY;
static const GSRQualityMode MinResolutionQualitymode = GSR_QUALITYMODE_PERFORMANCE;
static const GSRQualityMode MaxResolutionQualitymode = GSR_QUALITYMODE_ULTRA_QUALITY;

/// console variables
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
	FGSRShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}

	FGSRShader()
	{
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowRealTypes);
	}
}; // class

class FGSRConvertCS : public FGSRShader
{
	DECLARE_GLOBAL_SHADER(FGSRConvertCS);
	SHADER_USE_PARAMETER_STRUCT(FGSRConvertCS, FGSRShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FGSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputVelocity)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputColor)
		SHADER_PARAMETER(float, Exposure_co_rcp)
		SHADER_PARAMETER(float, AngleVertical)
		SHADER_PARAMETER(FMatrix44f, ReClipToPrevClip)
		SHADER_PARAMETER_SAMPLER(SamplerState, PointClamp)
		SHADER_PARAMETER_SAMPLER(SamplerState, PointClamp_Velocity)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, YCoCgColor)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, MotionDepthClipAlphaBuffer)
	END_SHADER_PARAMETER_STRUCT()
};

class FGSRUpscaleCS : public FGSRShader
{
	DECLARE_GLOBAL_SHADER(FGSRUpscaleCS);
	SHADER_USE_PARAMETER_STRUCT(FGSRUpscaleCS, FGSRShader);
	class FSampleNumberDim : SHADER_PERMUTATION_BOOL("SAMPLE_NUMBER");
	using FPermutationDomain = TShaderPermutationDomain<FSampleNumberDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FGSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, YCoCgColor)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MotionDepthClipAlphaBuffer)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevHistoryOutput)
		SHADER_PARAMETER_SAMPLER(SamplerState, PointClamp)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp1)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp2)
		SHADER_PARAMETER(float, ValidReset)
		SHADER_PARAMETER(float, MinLerpContribution)
		SHADER_PARAMETER(float, Scalefactor)
		SHADER_PARAMETER(float, Biasmax_viewportXScale)
		SHADER_PARAMETER(float, Exposure_co_rcp)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, HistoryOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SceneColorOutput)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FGSRConvertCS, "/Plugin/GSR/Private/gsr_convert.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FGSRUpscaleCS, "/Plugin/GSR/Private/gsr_upscale.usf", "MainCS", SF_Compute);

/// TU implementation
FGSRTU::FGSRTU()
{
	FMemory::Memzero(PostInputs);
	FGSRTU* self = this;

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
		UpdateScreenPercentage();
	}
}

static FVector LastViewOrigin = FVector::ZeroVector;
static FVector LastViewDir = FVector::ZeroVector;
static float SameFrameNum = 1.0;
IGSRTemporalUpscaler::FOutputs FGSRTU::AddPasses(
	FRDGBuilder& GraphBuilder,
	const GSRView& SceneView,
	const IGSRTemporalUpscaler::FInputs& PassInputs) const
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
	float HistoryFactor = FMath::Clamp(CVarGSRHistorySize.GetValueOnRenderThread() / 100.0f, 1.0f, 2.0f);
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
	bool const bHasAutoExposure = bValidEyeAdaptation || CVarGSRExposure.GetValueOnRenderThread();

	check((View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale));
	{
		IGSRTemporalUpscaler::FOutputs Outputs;

		RDG_GPU_STAT_SCOPE(GraphBuilder, GSRPass);
		RDG_EVENT_SCOPE(GraphBuilder, "SGSR2Pass");

		const bool bWritePrevViewInfo = !View.bStatePrevViewInfoIsReadOnly && View.ViewState;

		bool bHistoryValid = HistoryColorRT.IsValid() && View.ViewState && !View.bCameraCut;

		GSRstateRef GSRState;
		const FTemporalAAHistory& InputHistory = View.PrevViewInfo.TemporalAAHistory;
		const TRefCountPtr<ICustomTemporalAAHistory> PrevCustomHistory = PassInputs.PrevHistory;

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

		bool bReset = !InputHistory.IsValid() || View.bCameraCut || !OutputHistory;

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

		FRDGTextureRef YCoCgColor;
		FRDGTextureRef MotionDepthClipAlphaBuffer;

		FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
			InputExtents,
			PF_FloatRGBA,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV);

		MotionDepthClipAlphaBuffer = GraphBuilder.CreateTexture(Desc, TEXT("GSR.MotionDepthClipAlphaBuffer"));

		Desc.Format = PF_R32_UINT;
		YCoCgColor = GraphBuilder.CreateTexture(Desc, TEXT("GSR.YCoCgColor"));

		/////////
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
			PassParameters->Exposure_co_rcp = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure; ////TODO later
			PassParameters->ReClipToPrevClip = FMatrix44f(InvViewProj * PrevViewProj);
			PassParameters->PointClamp = TStaticSamplerState<SF_Point>::GetRHI();
			PassParameters->PointClamp_Velocity = TStaticSamplerState<SF_Point>::GetRHI();
			PassParameters->YCoCgColor = GraphBuilder.CreateUAV(YCoCgColor);
			PassParameters->MotionDepthClipAlphaBuffer = GraphBuilder.CreateUAV(MotionDepthClipAlphaBuffer);
			PassParameters->AngleVertical = tan(View.ViewMatrices.ComputeHalfFieldOfViewPerAxis().Y) * InputExtents.X / InputExtents.Y;

			TShaderMapRef<FGSRConvertCS> ComputeShader(View.ShaderMap);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GSR Convert %dx%d", InputRect.Width(), InputRect.Height()),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(InputRect.Size(), 8));
		}

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
		History.Color = GraphBuilder.CreateTexture(HistoryDesc, TEXT("GSRstate color"));
		HistoryDesc.Extent = InputExtents;
		HistoryDesc.Format = PF_R32_UINT;

		static const auto CVarPostPropagateAlpha = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessing.PropagateAlpha"));
		const bool bSupportsAlpha = (CVarPostPropagateAlpha && CVarPostPropagateAlpha->GetValueOnRenderThread() != 0);
		FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(
			HistoryExtents,
			bSupportsAlpha || (CVarGSRHistory.GetValueOnRenderThread() == 0) || IsOpenGLPlatform(View.GetShaderPlatform()) ? PF_FloatRGBA : PF_FloatR11G11B10,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV);

		FRDGTextureRef ColorOutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("GSR.Output"));

		{
			FGSRUpscaleCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGSRUpscaleCS::FParameters>();
			PassParameters->CommonParameters = CommonParameters;
			PassParameters->YCoCgColor = YCoCgColor;
			PassParameters->MotionDepthClipAlphaBuffer = MotionDepthClipAlphaBuffer;

			PassParameters->PrevHistoryOutput = PrevHistory.Color;

			PassParameters->Exposure_co_rcp = View.PrevViewInfo.SceneColorPreExposure / View.PreExposure;
			PassParameters->ValidReset = bReset;
			PassParameters->MinLerpContribution = 0.0;
			if (bSameCamera)
			{
				SameFrameNum += 1.0;
				if (SameFrameNum > 5)
				{
					PassParameters->MinLerpContribution = 0.3;
				}
			}
			else
			{
				SameFrameNum = 0.0;
			}

			float scalefactor = FMath::Min(20.0f, (float)pow((CommonParameters.HistoryInfo.ViewportSize / CommonParameters.InputInfo.ViewportSize).X * (CommonParameters.HistoryInfo.ViewportSize / CommonParameters.InputInfo.ViewportSize).Y, 3.0f));
			PassParameters->Scalefactor = scalefactor;
			float biasmax_viewportXScale = FMath::Min((CommonParameters.HistoryInfo.ViewportSize / CommonParameters.InputInfo.ViewportSize).X, 1.99f);
			PassParameters->Biasmax_viewportXScale = biasmax_viewportXScale;
			PassParameters->PointClamp = TStaticSamplerState<SF_Point>::GetRHI();
			PassParameters->LinearClamp1 = TStaticSamplerState<SF_Bilinear>::GetRHI();
			PassParameters->LinearClamp2 = TStaticSamplerState<SF_Bilinear>::GetRHI();
			PassParameters->HistoryOutput = GraphBuilder.CreateUAV(History.Color);
			PassParameters->SceneColorOutput = GraphBuilder.CreateUAV(ColorOutputTexture);
			FGSRUpscaleCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FGSRUpscaleCS::FSampleNumberDim>(CVarGSRSample.GetValueOnRenderThread());
			
			TShaderMapRef<FGSRUpscaleCS> ComputeShader(View.ShaderMap, PermutationVector);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GSR Update %dx%d", HistoryRect.Width(), HistoryRect.Height()),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(HistoryRect.Size(), 8));
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

/// postprocessing inputs for separate translucenecy available to mask
void FGSRTU::SetPostProcessingInputs(FPostProcessingInputs const& NewInputs)
{
	PostInputs = NewInputs;
}

/// release retained resources
void FGSRTU::EndofFrame()
{
	PostInputs.SceneTextures = nullptr;
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