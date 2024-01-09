//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once
#include "CoreMinimal.h"

UENUM(BlueprintType)
enum NPERuntimeMode
{
	NPE_R_CPU   UMETA(DisplayName = "CPU"),
	NPE_R_GPU   UMETA(DisplayName = "GPU"),
	NPE_R_DSP   UMETA(DisplayName = "DSP"),
};
