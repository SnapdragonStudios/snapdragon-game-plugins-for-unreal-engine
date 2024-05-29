//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HandTrackingSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class UHandTrackingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
private:
	const int kImageSize = 256;
	const int kNumTrackers = 21;
	TArray<UE::NNE::FTensorBindingCPU> InputBinding;
	TArray<TArray<float>> outputs;
	TArray<UE::NNE::FTensorBindingCPU> OutputBinding;

	TArray<FColor> tmpColorData;
	TArray<float> rawPixels;

	TWeakInterfacePtr<INNERuntimeCPU> runtime;
	TSharedPtr<UE::NNE::IModelCPU> Model;
	TSharedPtr<UE::NNE::IModelInstanceCPU> ModelInstance;
public:
	UPROPERTY(BlueprintReadOnly)
	float accuracy;
	UPROPERTY(BlueprintReadOnly)
	TArray<FVector> trackers;
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UNNEModelData> modelData;
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	UFUNCTION(BlueprintCallable)
	void Process(UTextureRenderTarget2D* renderTarget2D);
};
