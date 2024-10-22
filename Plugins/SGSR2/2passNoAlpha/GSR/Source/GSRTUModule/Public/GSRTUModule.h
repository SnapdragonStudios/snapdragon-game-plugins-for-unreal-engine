//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "Modules/ModuleManager.h"
#include "RHIDefinitions.h"

class FGSRTU;
class ITemporalUpscaler;

class FGSRTUModule final : public IModuleInterface
{
public:
	void StartupModule() override;
	void ShutdownModule() override;

	void SetTU(TSharedPtr<FGSRTU, ESPMode::ThreadSafe> Upscaler);

	FGSRTU* GetGSRU() const;
	ITemporalUpscaler* GetTU() const;
	float GetRfraction(uint32 Mode) const;
private:
	TSharedPtr<FGSRTU, ESPMode::ThreadSafe> TemporalUpscaler;
};



