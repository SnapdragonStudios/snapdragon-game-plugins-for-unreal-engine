//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(logGSR, Verbose, All);

#if DO_CHECK || DO_GUARD_SLOW || DO_ENSURE
DECLARE_LOG_CATEGORY_EXTERN(logGSRAPI, Verbose, All);
#endif
