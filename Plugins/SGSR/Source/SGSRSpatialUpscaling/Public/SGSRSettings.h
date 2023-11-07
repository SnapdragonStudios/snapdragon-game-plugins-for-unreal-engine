//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once
#include "SGSRSettings.generated.h"


//#SGSR_TARGET_Definitions
UENUM()
enum ESGSRTarget
{
    SGSR_TARGET_MOBILE UMETA(DisplayName = "Mobile"),
    SGSR_TARGET_HIGH_QUALITY UMETA(DisplayName = "High Quality"),
    SGSR_TARGET_VR UMETA(DisplayName = "Virtual Reality"),
    SGSR_MAX,
};

//#HalfPrecisionNotSupported
#define SGSR_CVAR_NAME_HALF_PRECISION "r.Qualcomm.SGSR.HalfPrecision"

#define SGSR_CVAR_NAME_TARGET "r.Qualcomm.SGSR.Target"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Rendering"))
class SGSRSPATIALUPSCALING_API USGSR_Settings : public UObject
{
    GENERATED_BODY()

public:
    USGSR_Settings(const FObjectInitializer& obj);

    UPROPERTY(config, EditAnywhere, Category = "Snapdragon Game Super Resolution Settings", meta = (
        ConsoleVariable = "r.Qualcomm.SGSR.HalfPrecision", DisplayName = "Half-Precision",
        ToolTip = "Whether to use 16bit precision for many floating point operations."))
        uint32 bHalfPrecision : 1;

    UPROPERTY(config, EditAnywhere, Category = "Snapdragon Game Super Resolution Settings", meta = (
        ConsoleVariable = "r.Qualcomm.SGSR.Target", DisplayName = "Target Algorithm Variant",
        ToolTip = "Selects shader variant; different variants are intended for different kinds of games."))
        TEnumAsByte<ESGSRTarget> Target;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override final;
#endif
    virtual void PostInitProperties() override final;
};

void HalfPrecisionCVarSetToFalse(IConsoleVariable* const Var);