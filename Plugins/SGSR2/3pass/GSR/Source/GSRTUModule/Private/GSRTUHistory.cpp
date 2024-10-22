//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "GSRTUHistory.h"
#include "GSRTU.h"

FGSRTUHistory::FGSRTUHistory(GSRstateRef state, FGSRTU* _upscaler)
{
	upscaler = _upscaler;
	Setstate(state);
}

FGSRTUHistory::~FGSRTUHistory()
{
	upscaler->Releasestate(GSR);
}

const TCHAR* FGSRTUHistory::GetDebugName() const
{
	// this has to match FFXFSR3TemporalUpscalerHistory::GetDebugName()
	return TEXT("FGSRTU");
}

uint64 FGSRTUHistory::GetGPUSizeBytes() const
{
	// 5.3 not done
	return 0;
}

void FGSRTUHistory::Setstate(GSRstateRef state)
{
	upscaler->Releasestate(GSR);
	GSR = state;
}
