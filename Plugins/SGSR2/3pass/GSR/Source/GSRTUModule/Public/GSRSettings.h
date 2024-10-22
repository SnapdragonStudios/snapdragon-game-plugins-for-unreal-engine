//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/DeveloperSettings.h"

#include "GSRSettings.generated.h"

/////GSR Quality Modes
UENUM()
enum class EGSRQualityMode : int32
{
	Unused UMETA(Hidden),
	Quality UMETA(DisplayName = "Quality"),
	Balanced UMETA(DisplayName = "Balanced"),
	Performance UMETA(DisplayName = "Performance"),
	UltraQuality UMETA(DisplayName = "Ultra Quality"),
};

/////Texture format support for history data
UENUM()
enum class EGSRHistoryFormat : int32
{
	FloatRGBA UMETA(DisplayName = "PF_FloatRGBA"),
	FloatR11G11B10 UMETA(DisplayName = "PF_FloatR11G11B10"),
};

/////Settings exposed through the editor UI
UCLASS(Config = Engine, DefaultConfig, DisplayName = "Snapdragon Game Super Resolution 2")
class GSRTUMODULE_API UGSRSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()
public:
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	UPROPERTY(Config, EditAnywhere, Category = "General Settings", meta = (ConsoleVariable = "r.SGSR2.Enabled", DisplayName = "Enabled"))
		bool bEnabled;

	UPROPERTY(Config, EditAnywhere, Category = "General Settings", meta = (ConsoleVariable = "r.SGSR2.Exposure", DisplayName = "Auto Exposure", ToolTip = "Default 0 to use engine's auto-exposure value, otherwise specific auto-exposure is used."))
		bool AutoExposure;

	UPROPERTY(Config, EditAnywhere, Category = "Quality Settings", meta = (ConsoleVariable = "r.SGSR2.Quality", DisplayName = "Quality Mode", ToolTip = "Selects the default quality mode to be used with SGSR2."))
		EGSRQualityMode QualityMode;

	UPROPERTY(Config, EditAnywhere, Category = "Quality Settings", meta = (ConsoleVariable = "r.SGSR2.History", DisplayName = "History Format", ToolTip = "Bit-depth for History texture format. 0: PF_FloatRGBA, 1: PF_FloatR11G11B10. Default(0) has better quality but worse bandwidth."))
		EGSRHistoryFormat HistoryFormat;

	UPROPERTY(Config, EditAnywhere, Category = "Quality Settings", meta = (ConsoleVariable = "r.SGSR2.TuneMipbias", DisplayName = "Mip Bias and Offset", ToolTip = "Applies negative MipBias to material textures, improving results."))
		bool AdjustMipBias;

	UPROPERTY(Config, EditAnywhere, Category = "Quality Settings", meta = (ConsoleVariable = "r.SGSR2.ForceVertexDeformationOutputsVelocity", DisplayName = "Force Vertex Deformation To Output Velocity", ToolTip = "Force enables materials with World Position Offset and/or World Displacement to output velocities during velocity pass even when the actor has not moved."))
		bool ForceVertexDeformationOutputsVelocity;
};
