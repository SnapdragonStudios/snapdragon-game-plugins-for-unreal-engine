//============================================================================================================
//
//
//                  Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "Modules/ModuleManager.h"

class FSGSRTUViewExtension;
class FSGSRSUViewExtension;

class FSGSRModule final : public IModuleInterface
{
public:
	void StartupModule() override;
	void ShutdownModule() override;

private:
	TSharedPtr<FSGSRTUViewExtension, ESPMode::ThreadSafe> SGSRTUViewExtension;
	TSharedPtr<FSGSRSUViewExtension, ESPMode::ThreadSafe> SGSRSUViewExtension;
};