//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "NNERuntimeSNPEModel.h"

#include "NNERuntimeSNPEUtils.h"

#include "Math/UnrealMathUtility.h"


// *** FModel implementation ***
namespace QCOM::NNERuntimeSNPE
{
	bool FModel::Init(
		const TSharedPtr<UE::NNE::FSharedModelData>& InSharedModelData,
		TArray<zdl::DlSystem::Runtime_t> RuntimeArray
	)
	{
		check(InSharedModelData.IsValid());

		SharedModelProperties = MakeShared<FSharedModelProperties>();
		if (!Utils::SetupSharedModelProperties(*SharedModelProperties, InSharedModelData, RuntimeArray))
		{
			return false;
		}

		return true;
	}

	TSharedPtr<UE::NNE::IModelInstanceCPU> FModel::CreateModelInstanceCPU()
	{
		TSharedPtr<FModelInstance> ModelInstance = MakeShared<FModelInstance>();
		if (!ModelInstance->Init(SharedModelProperties))
		{
			ModelInstance.Reset();
			return {};
		}

		return StaticCastSharedPtr<UE::NNE::IModelInstanceCPU>(ModelInstance);
	}

} // namespace QCOM::NNERuntimeSNPE


// *** FModelInstance implementation ***
namespace QCOM::NNERuntimeSNPE
{
	bool FModelInstance::Init(const TSharedPtr<FSharedModelProperties>& InSharedModelProperties)
	{
		SharedModelProperties = InSharedModelProperties;
		SNPEInstance = Utils::BuildSNPEInstance(SharedModelProperties->DlContainer, SharedModelProperties->RuntimeArray);

		return (SNPEInstance != nullptr);
	}

	FModelInstance::ESetInputTensorShapesStatus FModelInstance::SetInputTensorShapes(
		TConstArrayView<UE::NNE::FTensorShape> InInputShapes
	)
	{
		// ** validate input shapes **

		// ! only one set of input shapes is valid: the ones already present in the Model that created this instance
		// ! (variable dimensions / network resizing not supported)

		const auto& ExpectedInputTensorShapes = GetInputTensorShapes();
		if (InInputShapes.Num() != ExpectedInputTensorShapes.Num())
		{
			return ESetInputTensorShapesStatus::Fail;
		}
		for (int InputTensorIndex = 0; InputTensorIndex < InInputShapes.Num(); ++InputTensorIndex)
		{
			if (InInputShapes[InputTensorIndex] != ExpectedInputTensorShapes[InputTensorIndex])
			{
				return ESetInputTensorShapesStatus::Fail;
			}
		}

		// ** setup input & output buffers **
		if (
			!Utils::SetupUserBufferMapData(InputMapData, SharedModelProperties->InputTensorAttributes) ||
			!Utils::SetupUserBufferMapData(OutputMapData, SharedModelProperties->OutputTensorAttributes)
			)
		{
			return ESetInputTensorShapesStatus::Fail;
		}

		return ESetInputTensorShapesStatus::Ok;
	}

	FModelInstance::ERunSyncStatus FModelInstance::RunSync(
		TConstArrayView<UE::NNE::FTensorBindingCPU> InInputBindings,
		TConstArrayView<UE::NNE::FTensorBindingCPU> InOutputBindings
	)
	{
		if (InputMapData.AreBuffersEmpty() || OutputMapData.AreBuffersEmpty())
		{
			return ERunSyncStatus::Fail;
		}

		if (
			!ValidateBindings(InInputBindings, SharedModelProperties->InputTensorAttributes) ||
			!ValidateBindings(InOutputBindings, SharedModelProperties->OutputTensorAttributes)
			)
		{
			return ERunSyncStatus::Fail;
		}

		if (!PreProcessInputs(InInputBindings))
		{
			return ERunSyncStatus::Fail;
		}

		bool ExecutionSuccess = SNPEInstance->execute(InputMapData.UserBufferMap, OutputMapData.UserBufferMap);
		if (!ExecutionSuccess)
		{
			return ERunSyncStatus::Fail;
		}

		if (!PostProcessOutputs(InOutputBindings))
		{
			return ERunSyncStatus::Fail;
		}

		return ERunSyncStatus::Ok;
	}

	bool FModelInstance::ValidateBindings(
		TConstArrayView<UE::NNE::FTensorBindingCPU> Bindings,
		const FTensorAttributes& TensorAttributes
	)
	{
		if (Bindings.Num() != TensorAttributes.Num())
		{
			return false;
		}

		for (int32 BindingIndex = 0; BindingIndex < Bindings.Num(); ++BindingIndex)
		{
			const auto& Binding = Bindings[BindingIndex];

			const auto& TensorShape = TensorAttributes.TensorShapes[BindingIndex];
			const auto& ElementSize = TensorAttributes.UserBufferEncodings[BindingIndex]->getElementSize();

			if (Binding.Data == nullptr)
			{
				return false;
			}
			else if (Binding.SizeInBytes < TensorShape.Volume() * ElementSize)	// ! note: it's okay to have a bigger binding than needed
			{
				return false;
			}
		}

		return true;
	}

	bool FModelInstance::PreProcessInputs(TConstArrayView<UE::NNE::FTensorBindingCPU> InInputBindings)
	{
		for (int32 InputTensorIndex = 0; InputTensorIndex < InInputBindings.Num(); ++InputTensorIndex)
		{
			const auto& Binding = InInputBindings[InputTensorIndex];

			const auto& UserBuffer = InputMapData.UserBuffers[InputTensorIndex];
			auto& BackingArray = InputMapData.BackingArrays[InputTensorIndex];

			const auto& ElementType = UserBuffer->getEncoding().getElementType();

			if (!Utils::IsQuantizedElementType(ElementType))
			{
				std::memcpy(
					static_cast<void*>(BackingArray.GetData()),
					const_cast<const void*>(Binding.Data),
					BackingArray.Num()
				);
			}
			else if (ElementType == zdl::DlSystem::UserBufferEncoding::ElementType_t::TF8)
			{
				Utils::QuantizeBindingToBuffer<uint8>(Binding, UserBuffer, BackingArray, UINT8_MAX);
			}
			else if (ElementType == zdl::DlSystem::UserBufferEncoding::ElementType_t::TF16)
			{
				Utils::QuantizeBindingToBuffer<uint16>(Binding, UserBuffer, BackingArray, UINT16_MAX);
			}
			else
			{
				return false;
			}
		}

		return true;
	}

	bool FModelInstance::PostProcessOutputs(TConstArrayView<UE::NNE::FTensorBindingCPU> InOutputBindings)
	{
		for (int32 OutputTensorIndex = 0; OutputTensorIndex < InOutputBindings.Num(); ++OutputTensorIndex)
		{
			const auto& Binding = InOutputBindings[OutputTensorIndex];

			const auto& UserBuffer = OutputMapData.UserBuffers[OutputTensorIndex];
			auto& BackingArray = OutputMapData.BackingArrays[OutputTensorIndex];

			const auto& ElementType = UserBuffer->getEncoding().getElementType();

			if (!Utils::IsQuantizedElementType(ElementType))
			{
				std::memcpy(
					Binding.Data,
					const_cast<const void*>(static_cast<void*>(BackingArray.GetData())),
					BackingArray.Num()
				);
			}
			else if (ElementType == zdl::DlSystem::UserBufferEncoding::ElementType_t::TF8)
			{
				Utils::DequantizeBufferToBinding<uint8>(UserBuffer, Binding, BackingArray);
			}
			else if (ElementType == zdl::DlSystem::UserBufferEncoding::ElementType_t::TF16)
			{
				Utils::DequantizeBufferToBinding<uint16>(UserBuffer, Binding, BackingArray);
			}
			else
			{
				return false;
			}
		}

		return true;
	}

} // namespace QCOM::NNERuntimeSNPE
