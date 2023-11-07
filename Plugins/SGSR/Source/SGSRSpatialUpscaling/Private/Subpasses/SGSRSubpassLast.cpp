//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SGSRSubpassLast.h"

void FSGSRSubpassLast::ParseEnvironment(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs)
{
	Data->bSGSRIsTheLastPass = true;
}

void FSGSRSubpassLast::CreateResources(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs)
{
	Data->CurrentInputTexture = PassInputs.SceneColor.Texture;
	Data->bInitialized = true;
}