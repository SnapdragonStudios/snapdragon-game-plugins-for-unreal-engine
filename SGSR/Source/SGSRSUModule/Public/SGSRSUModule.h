//============================================================================================================
//
//
//                  Copyright (c) 2023, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "Modules/ModuleManager.h"

class FSGSRSUModule final : public IModuleInterface
{
public:
	// IModuleInterface implementation
	void StartupModule() override;
	void ShutdownModule() override;
}; 