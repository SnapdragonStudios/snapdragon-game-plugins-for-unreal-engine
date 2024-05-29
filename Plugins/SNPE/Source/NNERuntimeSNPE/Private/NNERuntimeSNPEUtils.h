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
#include "NNEModelData.h"
#include "NNERuntimeCPU.h"

THIRD_PARTY_INCLUDES_START

#include "DlSystem/IUserBuffer.hpp"
#include "DlContainer/IDlContainer.hpp"

#include "SNPE/SNPE.hpp"
#include "SNPE/SNPEFactory.hpp"

THIRD_PARTY_INCLUDES_END

#include "NNERuntimeSNPEStructures.h"

namespace QCOM::NNERuntimeSNPE::Utils
{
	// ** Model data buffer utils **

	FString MakeModelDataIdentifier(
		const FString& RuntimeName, const FRuntimeMetadata& RuntimeMetadata,
		const FGuid& FileId, const FString& PlatformName
	);

	void ComposeModelData(TArray<uint8>& OutData, FModelDataParts& InParts);
	bool DecomposeModelData(FModelDataParts& OutParts, TConstArrayView<uint8> InDataView);

} // namespace QCOM::NNERuntimeSNPE::Utils


namespace QCOM::NNERuntimeSNPE::Utils
{
	// ** SNPE <=> NNE conversion utils **

	ENNETensorDataType ToNNEDataType(zdl::DlSystem::UserBufferEncoding::ElementType_t DataTypeSNPE);

	UE::NNE::FSymbolicTensorShape ToNNESymbolicTensorShape(const zdl::DlSystem::TensorShape& TensorShapeSNPE);
	inline UE::NNE::FTensorShape ToNNETensorShape(const zdl::DlSystem::TensorShape& TensorShapeSNPE)
	{
		return UE::NNE::FTensorShape::MakeFromSymbolic(ToNNESymbolicTensorShape(TensorShapeSNPE));
	}

	zdl::DlSystem::TensorShape ToSNPETensorShape(const UE::NNE::FTensorShape& TensorShapeNNE);
	inline zdl::DlSystem::TensorShape ToSNPETensorShape(const UE::NNE::FSymbolicTensorShape& SymTensorShapeNNE)
	{
		checkf(SymTensorShapeNNE.IsConcrete(), TEXT("SNPE runtime doesn't support variable dimensions."));
		return ToSNPETensorShape(UE::NNE::FTensorShape::MakeFromSymbolic(SymTensorShapeNNE));
	}

} // namespace QCOM::NNERuntimeSNPE::Utils


namespace QCOM::NNERuntimeSNPE::Utils
{
	// ** Other SNPE utils **

	inline bool CheckRuntime(zdl::DlSystem::Runtime_t Runtime, bool bAllowUnsignedApps = true)
	{
		zdl::DlSystem::RuntimeCheckOption_t RuntimeCheckOption = (
			bAllowUnsignedApps ?
			zdl::DlSystem::RuntimeCheckOption_t::UNSIGNEDPD_CHECK :
			zdl::DlSystem::RuntimeCheckOption_t::NORMAL_CHECK
			);
		return zdl::SNPE::SNPEFactory::isRuntimeAvailable(Runtime, RuntimeCheckOption);
	}

	inline zdl::DlSystem::RuntimeList MakeRuntimeList(TConstArrayView<zdl::DlSystem::Runtime_t> RuntimeArray)
	{
		zdl::DlSystem::RuntimeList Result;
		for (const auto Runtime : RuntimeArray)
		{
			Result.add(Runtime);
		}
		return Result;
	}

	inline zdl::DlSystem::PlatformConfig MakePlatformConfig(bool bAllowUnsignedApps = true)
	{
		zdl::DlSystem::PlatformConfig Result;
		Result.setPlatformOptionValue("unsignedPD", bAllowUnsignedApps ? "ON" : "OFF");
		check(Result.isOptionsValid());
		return Result;
	}

	std::unique_ptr<zdl::SNPE::SNPE> BuildSNPEInstance(
		const std::unique_ptr<zdl::DlContainer::IDlContainer>& DlContainer,
		TConstArrayView<zdl::DlSystem::Runtime_t> RuntimeArray
	);

	zdl::DlSystem::TensorShape CalculateStrides(const zdl::DlSystem::TensorShape& TensorShapeSNPE, size_t ElementSize);
	std::unique_ptr<zdl::DlSystem::IUserBuffer> CreateUserBuffer(
		const UE::NNE::FTensorShape& TensorShapeNNE,
		TArray<uint8>& BackingArray,
		zdl::DlSystem::UserBufferEncoding* UserBufferEncoding
	);

	bool SetupTensorAttributes(
		FTensorAttributes& OutTensorAttributes,
		const std::unique_ptr<zdl::SNPE::SNPE>& SNPEInstance,
		const DlSystem::StringList& TensorNames
	);

	bool SetupUserBufferMapData(
		FUserBufferMapData& OutMapData,
		const FTensorAttributes& TensorAttributes
	);

	bool SetupSharedModelProperties(
		FSharedModelProperties& OutSharedModelProperties,
		const TSharedPtr<UE::NNE::FSharedModelData>& SharedModelData,
		TArray<zdl::DlSystem::Runtime_t> RuntimeArray
	);

} // namespace QCOM::NNERuntimeSNPE::Utils


namespace QCOM::NNERuntimeSNPE::Utils
{
	// ** Quantization/Dequantization utils **

	inline bool IsQuantizedElementType(zdl::DlSystem::UserBufferEncoding::ElementType_t ElementTypeSNPE)
	{
		return (
			ElementTypeSNPE == zdl::DlSystem::UserBufferEncoding::ElementType_t::TF8 ||
			ElementTypeSNPE == zdl::DlSystem::UserBufferEncoding::ElementType_t::TF16
			);
	}

	template <typename QuantizedElementType>
	inline QuantizedElementType Quantize(float RawValue, float EncodingMin, float EncodingMax, QuantizedElementType QuantizedMaxValue)
	{
		return QuantizedMaxValue * FMath::Clamp(
			(RawValue - EncodingMin) / (EncodingMax - EncodingMin),
			0.0f, 1.0f
		);
	}

	template <typename QuantizedElementType>
	inline float Dequantize(QuantizedElementType QuantizedValue, uint64 StepEquivalentToZero, float QuantizedStepSize)
	{
		return QuantizedStepSize * (QuantizedValue - StepEquivalentToZero);
	}

	template <typename QuantizedElementType>
	void QuantizeBindingToBuffer(
		const UE::NNE::FTensorBindingCPU& Binding,
		const std::unique_ptr<zdl::DlSystem::IUserBuffer>& UserBuffer,
		TArrayView<uint8> BackingArray,
		QuantizedElementType QuantizedMaxValue
	)
	{
		TArrayView<float> BindingDataView(
			static_cast<float*>(Binding.Data),
			Binding.SizeInBytes / sizeof(float)
		);
		TArrayView<QuantizedElementType> BufferDataView(
			reinterpret_cast<QuantizedElementType*>(BackingArray.GetData()),
			BackingArray.Num() / sizeof(QuantizedElementType)
		);
		check(BindingDataView.Num() >= BufferDataView.Num());

		// ! static downcast performed here as we assume the buffer is of a quantized type
		zdl::DlSystem::UserBufferEncodingTfN* QuantizedUserBufferEncoding = (
			static_cast<zdl::DlSystem::UserBufferEncodingTfN*>(&UserBuffer->getEncoding())
			);
		float EncodingMin = QuantizedUserBufferEncoding->getMin();
		float EncodingMax = QuantizedUserBufferEncoding->getMax();

		for (int32 ElementIndex = 0; ElementIndex < BufferDataView.Num(); ++ElementIndex)
		{
			BufferDataView[ElementIndex] = Quantize<QuantizedElementType>(
				BindingDataView[ElementIndex], EncodingMin, EncodingMax, QuantizedMaxValue
			);
		}
	}

	template <typename QuantizedElementType>
	void DequantizeBufferToBinding(
		const std::unique_ptr<zdl::DlSystem::IUserBuffer>& UserBuffer,
		const UE::NNE::FTensorBindingCPU& Binding,
		TArrayView<uint8> BackingArray
	)
	{
		TArrayView<float> BindingDataView(
			static_cast<float*>(Binding.Data),
			Binding.SizeInBytes / sizeof(float)
		);
		TArrayView<QuantizedElementType> BufferDataView(
			reinterpret_cast<QuantizedElementType*>(BackingArray.GetData()),
			BackingArray.Num() / sizeof(QuantizedElementType)
		);
		check(BindingDataView.Num() >= BufferDataView.Num());

		// ! static downcast performed here as we assume the buffer is of a quantized type
		zdl::DlSystem::UserBufferEncodingTfN* QuantizedUserBufferEncoding = (
			static_cast<zdl::DlSystem::UserBufferEncodingTfN*>(&UserBuffer->getEncoding())
			);
		uint64 StepEquivalentToZero = QuantizedUserBufferEncoding->getStepExactly0();
		float QuantizedStepSize = QuantizedUserBufferEncoding->getQuantizedStepSize();

		for (int32 ElementIndex = 0; ElementIndex < BufferDataView.Num(); ++ElementIndex)
		{
			BindingDataView[ElementIndex] = Dequantize<QuantizedElementType>(
				BufferDataView[ElementIndex], StepEquivalentToZero, QuantizedStepSize
			);
		}
	}

} // namespace QCOM::NNERuntimeSNPE::Utils
