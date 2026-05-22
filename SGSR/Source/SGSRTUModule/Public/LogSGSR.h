//============================================================================================================
//
//
//                  Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "CoreMinimal.h"

SGSRTUMODULE_API DECLARE_LOG_CATEGORY_EXTERN(LogSGSR, Verbose, All);

#if DO_CHECK || DO_GUARD_SLOW || DO_ENSURE
SGSRTUMODULE_API DECLARE_LOG_CATEGORY_EXTERN(LogSGSRAPI, Verbose, All);
#endif
