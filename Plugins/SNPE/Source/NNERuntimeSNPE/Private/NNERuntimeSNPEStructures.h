//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "CoreMinimal.h"

#include "NNETypes.h"

THIRD_PARTY_INCLUDES_START

#include "DlSystem/RuntimeList.hpp"
#include "DlSystem/PlatformConfig.hpp"
#include "DlSystem/UserBufferMap.hpp"
#include "DlSystem/TensorShape.hpp"

#include "DlContainer/IDlContainer.hpp"

THIRD_PARTY_INCLUDES_END


namespace QCOM::NNERuntimeSNPE
{

	// Encapsulates metadata used to identify runtimes
	struct FRuntimeMetadata
	{
		FGuid Guid;
		int32 Version;
	};

	// Encapsulates the different parts of a model data buffer
	struct FModelDataParts
	{
		FRuntimeMetadata RuntimeMetadata;
		TConstArrayView<uint8> DLCDataView;

		// add any further parts of the data buffer here (possibly: UDO data, output layer names)...
	};

	// SoA struct encapsulating all relevant tensor attributes (for setting up tensor-based data like user buffers)
	// (SoA used for compatibility with NNE::FModelInstance APIs to get array views of tensor descs etc.)
	struct FTensorAttributes
	{
		TArray<UE::NNE::FTensorDesc> TensorDescs;
		TArray<UE::NNE::FTensorShape> TensorShapes;
		TArray<zdl::DlSystem::UserBufferEncoding*> UserBufferEncodings;

	public:
		// ** convenience wrappers across arrays **

		inline int32 Num() const { return TensorDescs.Num(); }

		// ! SetNum() not directly used because it creates errors with default construction for TensorDescs
		inline void SetNumZeroed(int32 NewNum)
		{
			TensorDescs.SetNumZeroed(NewNum);
			TensorShapes.SetNumZeroed(NewNum);
			UserBufferEncodings.SetNumZeroed(NewNum);
		}
	};

	// Encapsulates data required for using a SNPE user buffer map, including the map itself
	struct FUserBufferMapData
	{
		zdl::DlSystem::UserBufferMap UserBufferMap;
		TArray<std::unique_ptr<zdl::DlSystem::IUserBuffer>> UserBuffers;
		TArray<TArray<uint8>> BackingArrays;

	public:
		// ** convenience wrappers **

		inline int32 NumBuffers() const { return UserBuffers.Num(); }

		inline bool AreBuffersEmpty() const { return NumBuffers() == 0; }

		inline void SetNumBuffers(int32 NewNum)
		{
			UserBuffers.SetNum(NewNum);
			BackingArrays.SetNum(NewNum);
		}
	};

	// Holds all data created by a Model to share with its ModelInstances
	struct FSharedModelProperties
	{
		std::unique_ptr<zdl::DlContainer::IDlContainer> DlContainer;
		TArray<zdl::DlSystem::Runtime_t> RuntimeArray;

		FTensorAttributes InputTensorAttributes;
		FTensorAttributes OutputTensorAttributes;
	};

} // namespace QCOM::NNERuntimeSNPE
