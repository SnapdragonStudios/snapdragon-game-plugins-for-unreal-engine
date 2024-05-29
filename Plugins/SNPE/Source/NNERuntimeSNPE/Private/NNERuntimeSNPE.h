//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "CoreMinimal.h"

#include "NNERuntime.h"
#include "NNERuntimeCPU.h"

THIRD_PARTY_INCLUDES_START

#include "DlSystem/DlEnums.hpp"

THIRD_PARTY_INCLUDES_END

#include "NNERuntimeSNPEStructures.h"
#include "NNERuntimeSNPEModel.h"

#include "NNERuntimeSNPE.generated.h"


static TAutoConsoleVariable<bool> CVarSNPECpuFallback(
	TEXT("snpe.CpuFallback"),

#if WITH_EDITOR
	true,
#else
	false,
#endif // WITH_EDITOR

	TEXT("NNERuntimeSNPE: allow CPU execution if DSP isn't available.\n")
	TEXT("Defaults to true in editor (to simplify PIE testing), false otherwise.\n"),
	ECVF_ReadOnly
);


UCLASS()
class UNNERuntimeSNPE : public UObject, public INNERuntime, public INNERuntimeCPU
{
	GENERATED_BODY()

public:
	UNNERuntimeSNPE()
	{
		RuntimeName = TEXT("NNERuntimeSNPE");
		RuntimeMetadata = {
			.Guid = FGuid((int32)'S', (int32)'N', (int32)'P', (int32)'E'),
			.Version = 1
		};
		TargetedSNPERuntimes = { zdl::DlSystem::Runtime_t::DSP_FIXED8_TF };
		if (CVarSNPECpuFallback.GetValueOnAnyThread())
		{
			TargetedSNPERuntimes.Add(zdl::DlSystem::Runtime_t::CPU);
		}
	}

public:
	// ~ Begin INNERuntime interface

	virtual inline FString GetRuntimeName() const override { return RuntimeName; }

	virtual ECanCreateModelDataStatus CanCreateModelData(
		const FString& FileType, TConstArrayView<uint8> FileData,
		const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData,
		const FGuid& FileId, const ITargetPlatform* TargetPlatform
	) const override;

	virtual TSharedPtr<UE::NNE::FSharedModelData> CreateModelData(
		const FString& FileType, TConstArrayView<uint8> FileData,
		const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData,
		const FGuid& FileId, const ITargetPlatform* TargetPlatform
	) override;

	virtual FString GetModelDataIdentifier(
		const FString& FileType, TConstArrayView<uint8> FileData,
		const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData,
		const FGuid& FileId, const ITargetPlatform* TargetPlatform
	) const override;

	// ~ End INNERuntime interface

public:
	// ~ Begin INNERuntimeCPU interface
	
	virtual ECanCreateModelCPUStatus CanCreateModelCPU(TObjectPtr<UNNEModelData> ModelData) const override;

	virtual TSharedPtr<UE::NNE::IModelCPU> CreateModelCPU(TObjectPtr<UNNEModelData> ModelData);
	
	// ~ End INNERuntimeCPU interface

private:
	FString RuntimeName;
	QCOM::NNERuntimeSNPE::FRuntimeMetadata RuntimeMetadata;
	TArray<zdl::DlSystem::Runtime_t> TargetedSNPERuntimes;
};
