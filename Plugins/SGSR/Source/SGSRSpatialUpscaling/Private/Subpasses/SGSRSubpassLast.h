//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "SGSRSubpass.h"

class FSGSRSubpassLast : public FSGSRSubpass
{
public:
	void ParseEnvironment(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) override;
	void CreateResources(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) override;
};