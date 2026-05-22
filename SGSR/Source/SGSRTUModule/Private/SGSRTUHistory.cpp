//============================================================================================================
//
//
//                  Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "SGSRTUHistory.h"
#include "SGSRTU.h"

FGSRTUHistory::FGSRTUHistory(SGSRstateRef state, FSGSRTU* _upscaler)
{
	upscaler = _upscaler;
	Setstate(state);
}

FGSRTUHistory::~FGSRTUHistory()
{
	upscaler->Releasestate(GSR);
}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2

const TCHAR* FGSRTUHistory::GetDebugName() const
{
	// this has to match FFXFSR3TemporalUpscalerHistory::GetDebugName()
	return TEXT("FSGSRTU");
}

uint64 FGSRTUHistory::GetGPUSizeBytes() const
{
	// 5.3 not done
	return 0;
}
#endif
void FGSRTUHistory::Setstate(SGSRstateRef state)
{
	upscaler->Releasestate(GSR);
	GSR = state;
}
