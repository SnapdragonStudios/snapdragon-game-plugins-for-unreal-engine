//============================================================================================================
//
//
//                  Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "SGSRTUViewExtension.h"
#include "SGSRTU.h"
#include "SGSRTUModule.h"
#include "PostProcess/PostProcessing.h"

#include "ScenePrivate.h"
#include "EngineUtils.h"

static TAutoConsoleVariable<int32> CVarEnableSGSR(
	TEXT("r.SGSR.Enabled"),
	1,
	TEXT("Enable QCOM Game Super Resolution"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSGSRTuneMipBias(
	TEXT("r.SGSR.TuneMipbias"),
	1,
	TEXT("Allow SGSR TU to adjust the minimum global texture mip bias(r.ViewTextureMipBias.Min&r.ViewTextureMipBias.Offset)"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarSGSRForceVertexDeformationOutputsVelocity(
	TEXT("r.SGSR.ForceVertexDeformationOutputsVelocity"),
	1,
	TEXT("Allow SGSR TU to enable r.Velocity.EnableVertexDeformation to ensure materials that use WPO render velocities."),
	ECVF_ReadOnly);

FSGSRTUViewExtension::FSGSRTUViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
	static IConsoleVariable* CVarMinAutoViewMipBiasMin = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ViewTextureMipBias.Min"));
	static IConsoleVariable* CVarMinAutoViewMipBiasOffset = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ViewTextureMipBias.Offset"));
	static IConsoleVariable* CVarVertexDeformationOutputsVelocity = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Velocity.EnableVertexDeformation"));
	/*static IConsoleVariable* CVarBasePassForceOutputsVelocity = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BasePassForceOutputsVelocity"));*/
	static IConsoleVariable* CVarVelocityEnableGrass = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Velocity.EnableLandscapeGrass"));

	PreviousSGSRState = CVarEnableSGSR.GetValueOnAnyThread();
	PreviousSGSRStateRT = CVarEnableSGSR.GetValueOnAnyThread();
	CurrentSGSRStateRT = CVarEnableSGSR.GetValueOnAnyThread();
	VertexDeformationOutputsVelocity = CVarVertexDeformationOutputsVelocity ? CVarVertexDeformationOutputsVelocity->GetInt() : 0;
	MinAutoViewMipBiasMin = CVarMinAutoViewMipBiasMin ? CVarMinAutoViewMipBiasMin->GetFloat() : 0;
	MinAutoViewMipBiasOffset = CVarMinAutoViewMipBiasOffset ? CVarMinAutoViewMipBiasOffset->GetFloat() : 0;
	/*BasePassForceOutputsVelocity = CVarBasePassForceOutputsVelocity ? CVarBasePassForceOutputsVelocity->GetInt() : 0; */
	VelocityEnableGrass = CVarVelocityEnableGrass ? CVarVelocityEnableGrass->GetInt() : 0;

	FSGSRTUModule& SGSRModule = FModuleManager::GetModuleChecked<FSGSRTUModule>(TEXT("SGSRTUModule"));
	if (SGSRModule.GetTU() == nullptr)
	{
		TSharedPtr<FSGSRTU, ESPMode::ThreadSafe> GSRTU = MakeShared<FSGSRTU, ESPMode::ThreadSafe>();
		SGSRModule.SetTU(GSRTU);
	}

	if (CVarEnableSGSR.GetValueOnAnyThread())
	{
		if (!GIsEditor)
		{
			if (CVarSGSRTuneMipBias.GetValueOnAnyThread())
			{
				if (CVarMinAutoViewMipBiasMin != nullptr)
				{
					CVarMinAutoViewMipBiasMin->Set(float(0.f + log2(1.f / 3.f) - 1.f + FLT_EPSILON), EConsoleVariableFlags::ECVF_SetByCode);
				}
				if (CVarMinAutoViewMipBiasOffset != nullptr)
				{
					CVarMinAutoViewMipBiasOffset->Set(float(-1.f + FLT_EPSILON), EConsoleVariableFlags::ECVF_SetByCode);
				}
			}

			if (CVarSGSRForceVertexDeformationOutputsVelocity.GetValueOnAnyThread())
			{
				if (CVarVertexDeformationOutputsVelocity != nullptr)
				{
					CVarVertexDeformationOutputsVelocity->Set(1, EConsoleVariableFlags::ECVF_SetByCode);
				}
				if (CVarVelocityEnableGrass != nullptr)
				{
					CVarVelocityEnableGrass->Set(1, EConsoleVariableFlags::ECVF_SetByCode);
				}
			}
		}
		else
		{
			PreviousSGSRState = false;
			PreviousSGSRStateRT = false;
			CurrentSGSRStateRT = false;
		}
	}
	else
	{
		PreviousSGSRState = false;
		PreviousSGSRStateRT = false;
		CurrentSGSRStateRT = false;
		CVarEnableSGSR->Set(0, EConsoleVariableFlags::ECVF_SetByCode);
	}
}

void FSGSRTUViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	static IConsoleVariable* CVarVertexDeformationOutputsVelocity = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Velocity.EnableVertexDeformation"));
	static IConsoleVariable* CVarVelocityEnableGrass = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Velocity.EnableLandscapeGrass"));
	FSGSRTUModule& SGSRModule = FModuleManager::GetModuleChecked<FSGSRTUModule>(TEXT("SGSRTUModule"));
	check(SGSRModule.GetGSRU());
	int32 EnableGSR = CVarEnableSGSR.GetValueOnAnyThread();
	// GSRModule.GetGSRU()->Initialize();

	if (EnableGSR)
	{
		if (CVarSGSRForceVertexDeformationOutputsVelocity.GetValueOnGameThread()
			&& CVarVertexDeformationOutputsVelocity != nullptr
			&& VertexDeformationOutputsVelocity == 0
			&& CVarVertexDeformationOutputsVelocity->GetInt() == 0)
		{
			VertexDeformationOutputsVelocity = CVarVertexDeformationOutputsVelocity->GetInt();
			CVarVertexDeformationOutputsVelocity->Set(1, EConsoleVariableFlags::ECVF_SetByCode);
		}

		if (CVarSGSRForceVertexDeformationOutputsVelocity.GetValueOnGameThread()
			&& CVarVelocityEnableGrass != nullptr
			&& VelocityEnableGrass == 0
			&& CVarVelocityEnableGrass->GetInt() == 0)
		{
			VelocityEnableGrass = CVarVelocityEnableGrass->GetInt();
			CVarVelocityEnableGrass->Set(1, EConsoleVariableFlags::ECVF_SetByCode);
		}
	}

	if (PreviousSGSRState != EnableGSR)
	{
		PreviousSGSRState = EnableGSR;
		static IConsoleVariable* CVarMinAutoViewMipBiasMin = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ViewTextureMipBias.Min"));
		static IConsoleVariable* CVarMinAutoViewMipBiasOffset = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ViewTextureMipBias.Offset"));
		/*static IConsoleVariable* CVarBasePassForceOutputsVelocity = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BasePassForceOutputsVelocity"));*/

		if (EnableGSR)
		{
			if (CVarSGSRTuneMipBias.GetValueOnGameThread())
			{
				if (CVarMinAutoViewMipBiasMin != nullptr)
				{
					MinAutoViewMipBiasMin = CVarMinAutoViewMipBiasMin->GetFloat();
					CVarMinAutoViewMipBiasMin->Set(float(0.f + log2(1.f / 3.f) - 1.f + FLT_EPSILON), EConsoleVariableFlags::ECVF_SetByCode);
				}
				if (CVarMinAutoViewMipBiasOffset != nullptr)
				{
					MinAutoViewMipBiasOffset = CVarMinAutoViewMipBiasOffset->GetFloat();
					CVarMinAutoViewMipBiasOffset->Set(float(-1.f + FLT_EPSILON), EConsoleVariableFlags::ECVF_SetByCode);
				}
			}

			if (CVarSGSRForceVertexDeformationOutputsVelocity.GetValueOnGameThread())
			{
				if (CVarVertexDeformationOutputsVelocity != nullptr)
				{
					CVarVertexDeformationOutputsVelocity->Set(1, EConsoleVariableFlags::ECVF_SetByCode);
				}
				if (CVarVelocityEnableGrass != nullptr)
				{
					CVarVelocityEnableGrass->Set(1, EConsoleVariableFlags::ECVF_SetByCode);
				}
			}
		}
		else
		{
			if (CVarSGSRTuneMipBias.GetValueOnGameThread())
			{
				if (CVarMinAutoViewMipBiasMin != nullptr)
				{
					CVarMinAutoViewMipBiasMin->Set(MinAutoViewMipBiasMin, EConsoleVariableFlags::ECVF_SetByCode);
				}
				if (CVarMinAutoViewMipBiasOffset != nullptr)
				{
					CVarMinAutoViewMipBiasOffset->Set(MinAutoViewMipBiasOffset, EConsoleVariableFlags::ECVF_SetByCode);
				}
			}

			if (CVarSGSRForceVertexDeformationOutputsVelocity.GetValueOnGameThread())
			{
				if (CVarVertexDeformationOutputsVelocity != nullptr)
				{
					CVarVertexDeformationOutputsVelocity->Set(VertexDeformationOutputsVelocity, EConsoleVariableFlags::ECVF_SetByCode);
				}
				if (CVarVelocityEnableGrass != nullptr)
				{
					CVarVelocityEnableGrass->Set(VelocityEnableGrass, EConsoleVariableFlags::ECVF_SetByCode);
				}
			}
		}
	}
}

void FSGSRTUViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	FSGSRTUModule& SGSRModule = FModuleManager::GetModuleChecked<FSGSRTUModule>(TEXT("SGSRTUModule"));
	FSGSRTU* Upscaler = SGSRModule.GetGSRU();
	bool isTUrequest = false;
	bool isGameview = !WITH_EDITOR;
	for (int i = 0; i < InViewFamily.Views.Num(); i++)
	{
		const FSceneView* InView = InViewFamily.Views[i];
		if (ensure(InView))
		{
			isGameview |= InView->bIsGameView;

			isTUrequest |= (InView->PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale);
		}
	}

	if (isTUrequest && CVarEnableSGSR.GetValueOnAnyThread() && (InViewFamily.GetTemporalUpscalerInterface() == nullptr) && isGameview)
	{
		InViewFamily.SetTemporalUpscalerInterface(new FSGSRTUFork(Upscaler));
	}
}

void FSGSRTUViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	CurrentSGSRStateRT = CVarEnableSGSR.GetValueOnRenderThread();
	if (PreviousSGSRStateRT != CurrentSGSRStateRT)
	{
		PreviousSGSRStateRT = CurrentSGSRStateRT;
#if 0
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
#endif
	}
}

void FSGSRTUViewExtension::PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	if (CVarEnableSGSR.GetValueOnAnyThread())
	{
		FSGSRTUModule& SGSRModule = FModuleManager::GetModuleChecked<FSGSRTUModule>(TEXT("SGSRTUModule"));
		SGSRModule.GetGSRU()->EndofFrame();
	}
}