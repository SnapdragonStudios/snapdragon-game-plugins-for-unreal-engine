//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "GSRViewExtension.h"
#include "GSRTU.h"
#include "GSRTUModule.h"
#include "PostProcess/PostProcessing.h"

#include "ScenePrivate.h"
#include "EngineUtils.h"

static TAutoConsoleVariable<int32> CVarEnableGSR(
	TEXT("r.SGSR2.Enabled"),
	1,
	TEXT("Enable QCOM Game Super Resolution for Temporal Upsampling"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarGSRTuneMipBias(
	TEXT("r.SGSR2.TuneMipbias"),
	1,
	TEXT("Allow SGSR2 to adjust the minimum global texture mip bias(r.ViewTextureMipBias.Min&r.ViewTextureMipBias.Offset)"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarGSRForceVertexDeformationOutputsVelocity(
	TEXT("r.SGSR2.ForceVertexDeformationOutputsVelocity"),
	1,
	TEXT("Allow SGSR2 to enable r.VertexDeformationOutputsVelocity to ensure materials that use WPO render velocities."),
	ECVF_ReadOnly);

FGSRViewExtension::FGSRViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister)
{
	static IConsoleVariable* CVarMinAutoViewMipBiasMin = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ViewTextureMipBias.Min"));
	static IConsoleVariable* CVarMinAutoViewMipBiasOffset = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ViewTextureMipBias.Offset"));
	static IConsoleVariable* CVarVertexDeformationOutputsVelocity = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VertexDeformationOutputsVelocity"));
	static IConsoleVariable* CVarBasePassForceOutputsVelocity = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BasePassForceOutputsVelocity"));

	GGSRCustomStaticScreenPercentage = nullptr;
	PreviousGSRState = CVarEnableGSR.GetValueOnAnyThread();
	PreviousGSRStateRT = CVarEnableGSR.GetValueOnAnyThread();
	CurrentGSRStateRT = CVarEnableGSR.GetValueOnAnyThread();
	VertexDeformationOutputsVelocity = CVarVertexDeformationOutputsVelocity ? CVarVertexDeformationOutputsVelocity->GetInt() : 0;
	MinAutoViewMipBiasMin = CVarMinAutoViewMipBiasMin ? CVarMinAutoViewMipBiasMin->GetFloat() : 0;
	MinAutoViewMipBiasOffset = CVarMinAutoViewMipBiasOffset ? CVarMinAutoViewMipBiasOffset->GetFloat() : 0;
	BasePassForceOutputsVelocity = CVarBasePassForceOutputsVelocity ? CVarBasePassForceOutputsVelocity->GetInt() : 0; 

	FGSRTUModule& GSRModule = FModuleManager::GetModuleChecked<FGSRTUModule>(TEXT("GSRTUModule"));
	if(GSRModule.GetTU() == nullptr)
	{
		TSharedPtr<FGSRTU, ESPMode::ThreadSafe> GSRTU = MakeShared<FGSRTU, ESPMode::ThreadSafe>();
		GSRModule.SetTU(GSRTU);
	}

	if(CVarEnableGSR.GetValueOnAnyThread())
	{
		if(!GIsEditor)    
		{
			if(ensureMsgf(GCustomStaticScreenPercentage == nullptr, TEXT("GCustomStaticScreenPercentage already in use, ensure only one upscaling plugin at a time")))
			{
				GCustomStaticScreenPercentage = GSRModule.GetCustomSP();
			}

			if(CVarGSRTuneMipBias.GetValueOnAnyThread())
			{
				if(CVarMinAutoViewMipBiasMin != nullptr)
				{
					CVarMinAutoViewMipBiasMin->Set(float(0.f + log2(1.f/3.f) - 1.f + FLT_EPSILON), EConsoleVariableFlags::ECVF_SetByCode);
				}
				if(CVarMinAutoViewMipBiasOffset != nullptr)
				{
					CVarMinAutoViewMipBiasOffset->Set(float(-1.f + FLT_EPSILON), EConsoleVariableFlags::ECVF_SetByCode);
				}
			}

			if(CVarGSRForceVertexDeformationOutputsVelocity.GetValueOnAnyThread())
			{
				if(CVarVertexDeformationOutputsVelocity != nullptr)
				{
					CVarVertexDeformationOutputsVelocity->Set(1, EConsoleVariableFlags::ECVF_SetByCode);
				}
				if(CVarBasePassForceOutputsVelocity != nullptr)
				{
					CVarBasePassForceOutputsVelocity->Set(1, EConsoleVariableFlags::ECVF_SetByCode);
				}
			}
		}
		else
		{
			PreviousGSRState = false;
			PreviousGSRStateRT = false;
			CurrentGSRStateRT = false;
		}
	}
	else
	{
		PreviousGSRState = false;
		PreviousGSRStateRT = false;
		CurrentGSRStateRT = false;
		CVarEnableGSR->Set(0, EConsoleVariableFlags::ECVF_SetByCode);
	}
}

void FGSRViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	static IConsoleVariable* CVarVertexDeformationOutputsVelocity = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VertexDeformationOutputsVelocity"));
	FGSRTUModule& GSRModule = FModuleManager::GetModuleChecked<FGSRTUModule>(TEXT("GSRTUModule"));
	check(GSRModule.GetGSRU());
	int32 EnableGSR = CVarEnableGSR.GetValueOnAnyThread();
	//GSRModule.GetGSRU()->Initialize();

	if(EnableGSR)
	{
		if(CVarGSRForceVertexDeformationOutputsVelocity.GetValueOnGameThread() 
			&& CVarVertexDeformationOutputsVelocity != nullptr
			&& VertexDeformationOutputsVelocity == 0 
			&& CVarVertexDeformationOutputsVelocity->GetInt() == 0)
		{
			VertexDeformationOutputsVelocity = CVarVertexDeformationOutputsVelocity->GetInt();
			CVarVertexDeformationOutputsVelocity->Set(1, EConsoleVariableFlags::ECVF_SetByCode);
		}

	}

	if(PreviousGSRState != EnableGSR)
	{
		PreviousGSRState = EnableGSR;
		static IConsoleVariable* CVarMinAutoViewMipBiasMin = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ViewTextureMipBias.Min"));
		static IConsoleVariable* CVarMinAutoViewMipBiasOffset = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ViewTextureMipBias.Offset"));
		static IConsoleVariable* CVarBasePassForceOutputsVelocity = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BasePassForceOutputsVelocity"));
		
		if(EnableGSR)
		{
			if(GCustomStaticScreenPercentage != GSRModule.GetCustomSP())
			{
				GGSRCustomStaticScreenPercentage = GCustomStaticScreenPercentage;
				GCustomStaticScreenPercentage = GSRModule.GetCustomSP();
			}

			if(CVarGSRTuneMipBias.GetValueOnGameThread())
			{
				if(CVarMinAutoViewMipBiasMin != nullptr)
				{
					MinAutoViewMipBiasMin = CVarMinAutoViewMipBiasMin->GetFloat();
					CVarMinAutoViewMipBiasMin->Set(float(0.f + log2(1.f/3.f) - 1.f + FLT_EPSILON), EConsoleVariableFlags::ECVF_SetByCode);
				}
				if(CVarMinAutoViewMipBiasOffset != nullptr)
				{
					MinAutoViewMipBiasOffset = CVarMinAutoViewMipBiasOffset->GetFloat();
					CVarMinAutoViewMipBiasOffset->Set(float(-1.f + FLT_EPSILON), EConsoleVariableFlags::ECVF_SetByCode);
				}
			}

			if(CVarGSRForceVertexDeformationOutputsVelocity.GetValueOnGameThread())
			{
				if(CVarVertexDeformationOutputsVelocity != nullptr)
				{
					CVarVertexDeformationOutputsVelocity->Set(1, EConsoleVariableFlags::ECVF_SetByCode);
				}
				if(CVarBasePassForceOutputsVelocity != nullptr)
				{
					CVarBasePassForceOutputsVelocity->Set(1, EConsoleVariableFlags::ECVF_SetByCode);
				}
			}

		}
		else
		{
			if(GGSRCustomStaticScreenPercentage)
			{
				GCustomStaticScreenPercentage = GGSRCustomStaticScreenPercentage;
				GGSRCustomStaticScreenPercentage = nullptr;
			}

			if(CVarGSRTuneMipBias.GetValueOnGameThread())
			{
				if(CVarMinAutoViewMipBiasMin != nullptr)
				{
					CVarMinAutoViewMipBiasMin->Set(MinAutoViewMipBiasMin, EConsoleVariableFlags::ECVF_SetByCode);
				}
				if(CVarMinAutoViewMipBiasOffset != nullptr)
				{
					CVarMinAutoViewMipBiasOffset->Set(MinAutoViewMipBiasOffset, EConsoleVariableFlags::ECVF_SetByCode);
				}
			}

			if(CVarGSRForceVertexDeformationOutputsVelocity.GetValueOnGameThread())
			{
				if(CVarVertexDeformationOutputsVelocity != nullptr)
				{
					CVarVertexDeformationOutputsVelocity->Set(VertexDeformationOutputsVelocity, EConsoleVariableFlags::ECVF_SetByCode);
				}
				if(CVarBasePassForceOutputsVelocity != nullptr)
				{
					CVarBasePassForceOutputsVelocity->Set(BasePassForceOutputsVelocity, EConsoleVariableFlags::ECVF_SetByCode);
				}
			}

		}
	}
}

void FGSRViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	FGSRTUModule& GSRModule = FModuleManager::GetModuleChecked<FGSRTUModule>(TEXT("GSRTUModule"));
	FGSRTU* Upscaler = GSRModule.GetGSRU();
	bool isTUrequest = false;
	bool isGameview = !WITH_EDITOR;
	for(int i = 0; i < InViewFamily.Views.Num(); i++)
	{
		const FSceneView* InView = InViewFamily.Views[i];
		if(ensure(InView))
		{
			isGameview |= InView->bIsGameView;

			isTUrequest |= (InView->PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale);
		}
	}

	if(isTUrequest && CVarEnableGSR.GetValueOnAnyThread() && (InViewFamily.GetTemporalUpscalerInterface() == nullptr))
	{
		InViewFamily.SetTemporalUpscalerInterface(Upscaler);
	}
}

void FGSRViewExtension::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	CurrentGSRStateRT = CVarEnableGSR.GetValueOnRenderThread();
	if(PreviousGSRStateRT != CurrentGSRStateRT)
	{
		PreviousGSRStateRT = CurrentGSRStateRT;
		for(auto* SceneView : InViewFamily.Views)
		{
			if(SceneView->bIsViewInfo)
			{
				FViewInfo* View = (FViewInfo*)SceneView;
				View->PrevViewInfo.CustomTemporalAAHistory.SafeRelease();
				if(!View->bStatePrevViewInfoIsReadOnly && View->ViewState)
				{
					View->ViewState->PrevFrameViewInfo.CustomTemporalAAHistory.SafeRelease();
				}
			}
		}
	}
}

void FGSRViewExtension::PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	if(CVarEnableGSR.GetValueOnAnyThread())
	{
		FGSRTUModule& GSRModule = FModuleManager::GetModuleChecked<FGSRTUModule>(TEXT("GSRTUModule"));
		GSRModule.GetGSRU()->EndofFrame();
	}
}