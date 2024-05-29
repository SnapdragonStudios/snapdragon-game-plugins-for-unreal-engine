//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "NNERuntimeSNPE.h"

#include "Interfaces/ITargetPlatform.h"
#include "Kismet/GameplayStatics.h"

#include "NNERuntimeSNPEUtils.h"


UNNERuntimeSNPE::ECanCreateModelDataStatus UNNERuntimeSNPE::CanCreateModelData(
	const FString& FileType, TConstArrayView<uint8> FileData,
	const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData,
	const FGuid& FileId, const ITargetPlatform* TargetPlatform
) const
{
	if (FileType.Compare("dlc", ESearchCase::IgnoreCase) != 0)
	{
		return ECanCreateModelDataStatus::FailFileIdNotSupported;
	}
	else if (FileData.IsEmpty())
	{
		return ECanCreateModelDataStatus::Fail;
	}
	return ECanCreateModelDataStatus::Ok;
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeSNPE::CreateModelData(
	const FString& FileType, TConstArrayView<uint8> FileData,
	const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData,
	const FGuid& FileId, const ITargetPlatform* TargetPlatform
)
{
	QCOM::NNERuntimeSNPE::FModelDataParts DataParts{
		.RuntimeMetadata = RuntimeMetadata,
		.DLCDataView = FileData
	};

	TArray<uint8> Result;
	QCOM::NNERuntimeSNPE::Utils::ComposeModelData(Result, DataParts);

	return MakeShared<UE::NNE::FSharedModelData>(MakeSharedBufferFromArray(MoveTemp(Result)), 0 /* : arbitrary memory alignment */);
}

FString UNNERuntimeSNPE::GetModelDataIdentifier(
	const FString& FileType, TConstArrayView<uint8> FileData,
	const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData,
	const FGuid& FileId, const ITargetPlatform* TargetPlatform
) const
{
	FString PlatformName = TargetPlatform ? TargetPlatform->IniPlatformName() : UGameplayStatics::GetPlatformName();

	return QCOM::NNERuntimeSNPE::Utils::MakeModelDataIdentifier(
		RuntimeName, RuntimeMetadata, FileId, PlatformName
	);
}

UNNERuntimeSNPE::ECanCreateModelCPUStatus UNNERuntimeSNPE::CanCreateModelCPU(TObjectPtr<UNNEModelData> ModelData) const
{
	// if any targeted runtime is available, it's possible to create a model
	// as SNPE will fall back to available runtimes as needed
	bool bNoRuntimesAvailable = true;
	for (const auto Runtime : TargetedSNPERuntimes)
	{
		if (QCOM::NNERuntimeSNPE::Utils::CheckRuntime(Runtime))
		{
			bNoRuntimesAvailable = false;
			break;
		}
	}
	if (bNoRuntimesAvailable)
	{
		UE_LOG(LogNNE, Error, TEXT("No SNPE target hardware runtimes are available for this device configuration!"));
		return ECanCreateModelCPUStatus::Fail;
	}

	if (ModelData == nullptr)
	{
		return ECanCreateModelCPUStatus::Fail;
	}

	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	if (!SharedData.IsValid())
	{
		return ECanCreateModelCPUStatus::Fail;
	}

	QCOM::NNERuntimeSNPE::FModelDataParts DataParts;
	if (!QCOM::NNERuntimeSNPE::Utils::DecomposeModelData(DataParts, SharedData->GetView()))
	{
		return ECanCreateModelCPUStatus::Fail;
	}

	if (DataParts.RuntimeMetadata.Guid != RuntimeMetadata.Guid ||
		DataParts.RuntimeMetadata.Version != RuntimeMetadata.Version)
	{
		return ECanCreateModelCPUStatus::Fail;
	}

	return ECanCreateModelCPUStatus::Ok;
}

TSharedPtr<UE::NNE::IModelCPU> UNNERuntimeSNPE::CreateModelCPU(TObjectPtr<UNNEModelData> ModelData)
{
	TSharedPtr<UE::NNE::FSharedModelData> SharedModelData = ModelData->GetModelData(RuntimeName);
	check(SharedModelData.IsValid());

	TSharedPtr<QCOM::NNERuntimeSNPE::FModel> Model = MakeShared<QCOM::NNERuntimeSNPE::FModel>();
	if (!Model->Init(SharedModelData, TargetedSNPERuntimes))
	{
		UE_LOG(LogNNE, Error, TEXT("Model init failed!"));
		Model.Reset();
		return {};
	}

	return StaticCastSharedPtr<UE::NNE::IModelCPU>(Model);
}
