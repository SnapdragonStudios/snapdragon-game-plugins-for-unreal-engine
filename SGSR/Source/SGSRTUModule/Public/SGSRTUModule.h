//============================================================================================================
//
//
//                  Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "Modules/ModuleManager.h"
#include "RHIDefinitions.h"

class FSGSRTU;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
using ISGSRTemporalUpscaler = UE::Renderer::Private::ITemporalUpscaler;
#else
class ITemporalUpscaler;
#endif

class FSGSRTUModule final : public IModuleInterface
{
public:
	void StartupModule() override;
	void ShutdownModule() override;

	void SetTU(TSharedPtr<FSGSRTU, ESPMode::ThreadSafe> Upscaler);

	FSGSRTU* GetGSRU() const;
	
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
	ISGSRTemporalUpscaler* GetTU() const;
#else
	ITemporalUpscaler* GetTU() const;
#endif

	float GetRfraction(uint32 Mode) const;

private:
	TSharedPtr<FSGSRTU, ESPMode::ThreadSafe> TemporalUpscaler;
};
