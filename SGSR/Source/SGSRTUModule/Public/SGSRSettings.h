//============================================================================================================
//
//
//                  Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/DeveloperSettings.h"

#include "SGSRSettings.generated.h"

///// SGSR Methods
UENUM()
enum class ESGSRMethod : int32
{
	SU UMETA(DisplayName = "Spatial Upscaling"),
	TU_2Pass_FS UMETA(DisplayName = "Temperal Upscaling 2 Pass Fragment Shader"),
	TU_3Pass_CS UMETA(DisplayName = "Temperal Upscaling 3 Pass Compute Shader"),
};

/////SGSR SU Target
UENUM()
enum class ESGSRSUTarget : int32
{
	Mobile UMETA(DisplayName = "Mobile"),
	High_Quality UMETA(DisplayName = "High_Quality"),
	VR UMETA(DisplayName = "VR"),
};

/////SGSR Quality Modes
UENUM()
enum class ESGSRQualityMode : int32
{
	UltraQuality UMETA(DisplayName = "Ultra Quality"),
	Quality UMETA(DisplayName = "Quality"),
	Balanced UMETA(DisplayName = "Balanced"),
	Performance UMETA(DisplayName = "Performance"),
	Custom UMETA(DisplayName = "Custom"),
};

/////Texture format support for history data
UENUM()
enum class ESGSRHistoryFormat : int32
{
	FloatRGBA UMETA(DisplayName = "PF_FloatRGBA"),
	FloatR11G11B10 UMETA(DisplayName = "PF_FloatR11G11B10"),
};

/////Settings exposed through the editor UI
UCLASS(Config = Engine, DefaultConfig, DisplayName = "Snapdragon Game Super Resolution")
class SGSRTUMODULE_API USGSRSettings : public UDeveloperSettings
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
	UPROPERTY(Config, EditAnywhere, Category = "General Settings", meta = (ConsoleVariable = "r.SGSR.Enabled", DisplayName = "Enabled"))
	bool bEnabled;

	UPROPERTY(Config, EditAnywhere, Category = "General Settings", meta = (ConsoleVariable = "r.SGSR.Method", DisplayName = "Upscaling Method", ToolTip = "Selects upscaling method to be used with SGSR."))
	ESGSRMethod UpscalingMethod;

	UPROPERTY(Config, EditAnywhere, Category = "General Settings", meta = (ConsoleVariable = "r.SGSR.Quality", DisplayName = "Quality Mode", ToolTip = "Selects the default quality mode to be used with SGSR."))
	ESGSRQualityMode QualityMode;

	UPROPERTY(Config, EditAnywhere, Category = "General Settings", meta = (ConsoleVariable = "r.SGSR.CustomScreenPercentage", DisplayName = "Custom Screen Percentage", EditCondition = "QualityMode == ESGSRQualityMode::Custom", ToolTip = "Input custom screen percentage to overwrite quality mode.", UIMin = "50.0", UIMax = "100.0"))
	float CustomScreenPercentage;

	UPROPERTY(Config, EditAnywhere, Category = "General Settings", meta = (ConsoleVariable = "r.SGSR.HalfPrecision", DisplayName = "Half Precision", ToolTip = "Use half precision (16bit) arithmetic when platform support is available."))
	bool Halfprecision;

	UPROPERTY(Config, EditAnywhere, Category = "SU Settings", meta = (ConsoleVariable = "r.SGSR.Target", DisplayName = "SU Target", EditCondition = "UpscalingMethod == ESGSRMethod::SU", ToolTip = "Selects target platform for SU."))
	ESGSRSUTarget SUTarget;

	UPROPERTY(Config, EditAnywhere, Category = "TU Settings", meta = (ConsoleVariable = "r.SGSR.Exposure", DisplayName = "Auto Exposure", EditCondition = "UpscalingMethod != ESGSRMethod::SU", ToolTip = "Default 0 to use engine's auto-exposure value, otherwise specific auto-exposure is used."))
	bool AutoExposure;

	UPROPERTY(Config, EditAnywhere, Category = "TU Settings", meta = (ConsoleVariable = "r.SGSR.History", DisplayName = "History Format", EditCondition = "UpscalingMethod != ESGSRMethod::SU", ToolTip = "Bit-depth for History texture format. 0: PF_FloatRGBA, 1: PF_FloatR11G11B10. Default(0) has better quality but worse bandwidth."))
	ESGSRHistoryFormat HistoryFormat;

	UPROPERTY(Config, EditAnywhere, Category = "TU Settings", meta = (ConsoleVariable = "r.SGSR.TuneMipbias", DisplayName = "Mip Bias and Offset", EditCondition = "UpscalingMethod != ESGSRMethod::SU", ToolTip = "Applies negative MipBias to material textures, improving results."))
	bool AdjustMipBias;

	UPROPERTY(Config, EditAnywhere, Category = "TU Settings", meta = (ConsoleVariable = "r.SGSR.ForceVertexDeformationOutputsVelocity", DisplayName = "Force Vertex Deformation To Output Velocity", EditCondition = "UpscalingMethod != ESGSRMethod::SU", ToolTip = "Force enables materials with World Position Offset and/or World Displacement to output velocities during velocity pass even when the actor has not moved."))
	bool ForceVertexDeformationOutputsVelocity;

	//------Settings for Pixel Lock mode only--------
	UPROPERTY(Config, EditAnywhere, Category = "TU Settings", meta = (ConsoleVariable = "r.SGSR.PixelLock", DisplayName = "Pixel Lock", EditCondition = "UpscalingMethod == ESGSRMethod::TU_3Pass_CS", ToolTip = "Enable pixel lock for better thin feature upscaling (small performance hit)."))
	bool PixelLock;
	//-----------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "TU Settings", meta = (ConsoleVariable = "r.SGSR.DoSharpening", DisplayName = "Sharpen Output", EditCondition = "UpscalingMethod == ESGSRMethod::TU_3Pass_CS", ToolTip = "Add a sharpening pass after upscaling."))
	bool DoSharpening;

	UPROPERTY(Config, EditAnywhere, Category = "TU Settings", meta = (ConsoleVariable = "r.SGSR.Sharpness", DisplayName = "Sharpness", EditCondition = "DoSharpening == true", ToolTip = "Adjust sharpening strength.", UIMin = "0.0", UIMax = "1.3"))
	float Sharpness;
};
