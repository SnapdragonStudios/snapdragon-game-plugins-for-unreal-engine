//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "SGSRSubpass.h"

class FSGSRSubpassFirst : public FSGSRSubpass
{
public:
	void CreateResources(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) override;
	void Upscale(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) override;
	void PostProcess(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) override;
};