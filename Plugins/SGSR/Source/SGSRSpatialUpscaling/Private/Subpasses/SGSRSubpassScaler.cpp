//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SGSRSubpassScaler.h"
#include "SGSRSettings.h"

//#SGSR_TARGET_Definitions
static TAutoConsoleVariable<int32> SGSR_TargetCVar(
    TEXT(SGSR_CVAR_NAME_TARGET),
    0,
    TEXT("Target: Each Target is a different shader, and (starting with UE5.0.0) each shader has a full-precision and half-precision variant.  Valid values: [0,2]\n")
    TEXT(" 0: SGSR_TARGET_MOBILE\n")
    TEXT(" 1: SGSR_TARGET_HIGH_QUALITY\n")
    TEXT(" 2: SGSR_TARGET_VR"));

static TAutoConsoleVariable<bool> SGSR_HalfPrecisionCVar(
    TEXT(SGSR_CVAR_NAME_HALF_PRECISION),
    1,//half-precision often speeds processing time with little or no additional noticeable artifacts
    TEXT("If 1, use 16-bit precision for many SGSR operations.  Available only as of UE5.0.0; UE4 does not support half-precision in shader code"),
    ECVF_RenderThreadSafe
);

///
/// SGSR PIXEL SHADER
///
class FSGSRPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSGSRPS);
	SHADER_USE_PARAMETER_STRUCT(FSGSRPS, FGlobalShader);

    class FSGSR_HalfPrecision : SHADER_PERMUTATION_BOOL("SGSR_USE_HALF_PRECISION");
    class FSGSR_Target : SHADER_PERMUTATION_SPARSE_INT("SGSR_TARGET", SGSR_TARGET_MOBILE, SGSR_TARGET_HIGH_QUALITY, SGSR_TARGET_VR);//#SGSR_TARGET_Definitions -- sync between shader and C++ code
    enum { SGSR_TARGET_VALID_MIN = SGSR_TARGET_MOBILE, SGSR_TARGET_VALID_MAX = SGSR_TARGET_VR, SGSR_TARGET_DEFAULT = SGSR_TARGET_VALID_MIN};//use integer values declared by shader permutation above; #SGSR_TARGET_Definitions
    using FPermutationDomain = TShaderPermutationDomain<FSGSR_HalfPrecision, FSGSR_Target>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSGSRPassParameters, SGSR)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
        //it doesn't make sense for this case, but we could prevent certain permutations from being cooked like so:
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5)|| IsMobilePlatform(Parameters.Platform);
	}

};

IMPLEMENT_GLOBAL_SHADER(FSGSRPS, "/Plugin/SGSR/Private/PostProcessSGSR.usf", "MainPS", SF_Pixel);
void FSGSRSubpassScaler::ParseEnvironment(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs)
{
}

void OnChange_SGSR_HalfPrecisionCVar(IConsoleVariable* Var)
{
}

static bool inline SgsrTargetValueValid(const int32 SgsrTargetValue)
{
    return SgsrTargetValue >= FSGSRPS::SGSR_TARGET_VALID_MIN && SgsrTargetValue <= FSGSRPS::SGSR_TARGET_VALID_MAX;
}
static int32 inline SgsrTargetValueDefault()
{
    return FSGSRPS::SGSR_TARGET_DEFAULT;
}
void OnChange_SGSR_TargetCVar(IConsoleVariable* Var)
{
#if !UE_BUILD_SHIPPING
    const int32 SgsrTargetValue = Var->GetInt();
    if (!SgsrTargetValueValid(SgsrTargetValue))
    {
        UE_LOG(
            LogSGSR,
            Log,
            TEXT("Invalid SGSR Target value %i specified; using %i instead (ERROR: doing this in UE_BUILD_SHIPPING is undefined)"), SgsrTargetValue, SgsrTargetValueDefault());
        Var->Set(SgsrTargetValueDefault(), ECVF_SetByConsole);
    }
#endif
}

void FSGSRSubpassScaler::CreateResources(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs)
{
    SGSR_TargetCVar.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnChange_SGSR_TargetCVar));
	SGSR_HalfPrecisionCVar.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnChange_SGSR_HalfPrecisionCVar));

	Data->UpscaleTexture = FScreenPassTexture(PassInputs.SceneColor);

	Data->UpscaleTexture.Texture = GraphBuilder.CreateTexture(Data->SGSROutputTextureDesc, TEXT("SGSR-Output"), ERDGTextureFlags::MultiFrame);
	Data->UpscaleTexture.ViewRect = View.UnscaledViewRect;
	
	Data->OutputViewport = FScreenPassTextureViewport(Data->UpscaleTexture);
	Data->InputViewport = FScreenPassTextureViewport(PassInputs.SceneColor);
	Data->bSGSREnabled = (Data->InputViewport.Rect != Data->OutputViewport.Rect);
}

void FSGSRSubpassScaler::Upscale(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs)
{
	//
	// Snapdragon Game Super Resolution (GSR) Pass
	//
	if (Data->bSGSREnabled)
	{
		SGSR_F1 const0[4];
		SGSR_F1 const1[4];

		SgsrCon(const0, const1,
			static_cast<SGSR_F1>(Data->InputViewport.Rect.Width())
			, static_cast<SGSR_F1>(Data->InputViewport.Rect.Height())
			, static_cast<SGSR_F1>(PassInputs.SceneColor.Texture->Desc.Extent.X)
			, static_cast<SGSR_F1>(PassInputs.SceneColor.Texture->Desc.Extent.Y)
			, static_cast<SGSR_F1>(Data->OutputViewport.Rect.Width())
			, static_cast<SGSR_F1>(Data->OutputViewport.Rect.Height())		
		);

		const bool bUseIntermediateRT = !PassInputs.OverrideOutput.IsValid();

		FScreenPassRenderTarget Output = bUseIntermediateRT
			? FScreenPassRenderTarget(Data->UpscaleTexture.Texture, Data->UpscaleTexture.ViewRect, ERenderTargetLoadAction::ENoAction)
			: PassInputs.OverrideOutput;

		// VS-PS
		FSGSRPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSGSRPS::FParameters>();
		for (int i = 0; i < 4; i++)
		{
			PassParameters->SGSR.Const0[i] = const0[i];
			PassParameters->SGSR.Const1[i] = const1[i];
		}
		PassParameters->SGSR.InputTexture = Data->CurrentInputTexture;
		PassParameters->SGSR.samLinearClamp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->RenderTargets[0] = FRenderTargetBinding(Output.Texture, ERenderTargetLoadAction::ENoAction);

        FSGSRPS::FPermutationDomain PermutationVector;
        PermutationVector.Set<FSGSRPS::FSGSR_HalfPrecision>(SGSR_HalfPrecisionCVar.GetValueOnAnyThread() != 0);
        PermutationVector.Set<FSGSRPS::FSGSR_Target>(SGSR_TargetCVar.GetValueOnAnyThread());

		TShaderMapRef<FSGSRPS> PixelShader(View.ShaderMap, PermutationVector);

		AddDrawScreenPass(GraphBuilder,
			RDG_EVENT_NAME("Qualcomm-SGSR/Upscale %dx%d -> %dx%d (PS)"
				, Data->InputViewport.Rect.Width(), Data->InputViewport.Rect.Height()
				, Data->OutputViewport.Rect.Width(), Data->OutputViewport.Rect.Height()
			),
			View, Data->OutputViewport, Data->InputViewport,
			PixelShader, PassParameters,
			EScreenPassDrawFlags::None
		);
		Data->FinalOutput = Output; 
		Data->CurrentInputTexture = Output.Texture;
		GEngine->AddOnScreenDebugMessage(0, 1.0f, FColor::Red, TEXT("SGSR Enabled!"));
	}
}