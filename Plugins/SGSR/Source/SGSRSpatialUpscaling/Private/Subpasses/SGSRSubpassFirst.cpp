//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SGSRSubpassFirst.h"

void FSGSRSubpassFirst::CreateResources(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs)
{
	Data->SGSROutputTextureDesc = PassInputs.SceneColor.Texture->Desc;
	Data->SGSROutputTextureDesc.Reset();
	Data->SGSROutputTextureDesc.Extent = View.UnscaledViewRect.Max;
	Data->SGSROutputTextureDesc.ClearValue = FClearValueBinding::Black;
	Data->SGSROutputTextureDesc.Flags = TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable;
}

void FSGSRSubpassFirst::Upscale(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs)
{
	Data->FinalOutput = PassInputs.SceneColor;
}

void FSGSRSubpassFirst::PostProcess(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs)
{
	if (!Data->FinalOutput.IsValid())
	{
		check(PassInputs.OverrideOutput.IsValid());
		Data->FinalOutput = PassInputs.OverrideOutput;
	}
}