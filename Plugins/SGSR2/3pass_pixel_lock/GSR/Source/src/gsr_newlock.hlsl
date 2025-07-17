//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "gsr_newlock.h"

#define TILE_SIZE_X 8
#define TILE_SIZE_Y 8

[numthreads(TILE_SIZE_X, TILE_SIZE_Y, 1)]
void MainCS(uint2 uGroupId : SV_GroupID,
            uint2 uGroupThreadId : SV_GroupThreadID,
            uint2 uDispatchThreadId : SV_DispatchThreadID)
{
    NewLock(uDispatchThreadId);
}
