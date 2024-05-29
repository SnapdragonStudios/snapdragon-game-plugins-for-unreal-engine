//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "NNERuntimeSNPEUtils.h"

THIRD_PARTY_INCLUDES_START

#include "SNPE/SNPEBuilder.hpp"

#include "DlSystem/IUserBufferFactory.hpp"
#include "DlSystem/DlEnums.hpp"

THIRD_PARTY_INCLUDES_END


namespace QCOM::NNERuntimeSNPE::Utils
{
	// ** Model data buffer utils **

	FString MakeModelDataIdentifier(
		const FString& RuntimeName, const FRuntimeMetadata& RuntimeMetadata,
		const FGuid& FileId, const FString& PlatformName
	)
	{
		return (
			RuntimeName + "-" +
			RuntimeMetadata.Guid.ToString(EGuidFormats::Digits) + "-" +
			FString::FromInt(RuntimeMetadata.Version) + "-" +
			FileId.ToString(EGuidFormats::Digits) + "-" +
			PlatformName
			);
	}

	void ComposeModelData(TArray<uint8>& OutData, FModelDataParts& InParts)
	{
		FMemoryWriter Writer(OutData);

		Writer << InParts.RuntimeMetadata.Guid;
		Writer << InParts.RuntimeMetadata.Version;

		int32 DLCSize = InParts.DLCDataView.Num();
		Writer << DLCSize;
		Writer.Serialize(const_cast<uint8*>(InParts.DLCDataView.GetData()), InParts.DLCDataView.Num());
	}

	bool DecomposeModelData(FModelDataParts& OutParts, TConstArrayView<uint8> InDataView)
	{
		FMemoryReaderView Reader(InDataView);

		if (InDataView.Num() < sizeof(FRuntimeMetadata))
		{
			return false;
		}

		Reader << OutParts.RuntimeMetadata.Guid;
		Reader << OutParts.RuntimeMetadata.Version;

		if (InDataView.Num() - Reader.Tell() < sizeof(int32))
		{
			return false;
		}
		int32 DLCSize;
		Reader << DLCSize;

		if (InDataView.Num() - Reader.Tell() < DLCSize)
		{
			return false;
		}
		OutParts.DLCDataView = TConstArrayView<uint8>(InDataView.GetData() + Reader.Tell(), DLCSize);

		return true;
	}

} // namespace QCOM::NNERuntimeSNPE::Utils


namespace QCOM::NNERuntimeSNPE::Utils
{
	// ** SNPE <=> NNE conversion utils **

	ENNETensorDataType ToNNEDataType(zdl::DlSystem::UserBufferEncoding::ElementType_t DataTypeSNPE)
	{
		using SNPEUserBufferElementType = zdl::DlSystem::UserBufferEncoding::ElementType_t;

		switch (DataTypeSNPE)
		{
		case SNPEUserBufferElementType::BOOL8:
			return ENNETensorDataType::Boolean;

		case SNPEUserBufferElementType::FLOAT:
			return ENNETensorDataType::Float;

		case SNPEUserBufferElementType::FLOAT16:
			return ENNETensorDataType::Half;

		case SNPEUserBufferElementType::INT8:
			return ENNETensorDataType::Int8;

		case SNPEUserBufferElementType::INT16:
			return ENNETensorDataType::Int16;

		case SNPEUserBufferElementType::INT32:
			return ENNETensorDataType::Int32;

		case SNPEUserBufferElementType::TF16:
			return ENNETensorDataType::Float;	// The plugin performs TF16 quantization on float inputs from the user

		case SNPEUserBufferElementType::TF8:
			return ENNETensorDataType::Float;	// The plugin performs TF8 quantization on float inputs from the user

		case SNPEUserBufferElementType::UINT8:
			return ENNETensorDataType::UInt8;

		case SNPEUserBufferElementType::UNSIGNED8BIT:
			return ENNETensorDataType::UInt8;

		case SNPEUserBufferElementType::UINT16:
			return ENNETensorDataType::UInt16;

		case SNPEUserBufferElementType::UINT32:
			return ENNETensorDataType::UInt32;

		case SNPEUserBufferElementType::UNKNOWN:
			return ENNETensorDataType::None;

		default:
			return ENNETensorDataType::None;
		}
	}

	UE::NNE::FSymbolicTensorShape ToNNESymbolicTensorShape(const zdl::DlSystem::TensorShape& TensorShapeSNPE)
	{
		TArray<int32> TensorShapeAsArray;
		for (int32 DimIndex = 0; DimIndex < TensorShapeSNPE.rank(); ++DimIndex)
		{
			TensorShapeAsArray.Add(TensorShapeSNPE.getDimensions()[DimIndex]);
		}

		return UE::NNE::FSymbolicTensorShape::Make(TensorShapeAsArray);
	}

	zdl::DlSystem::TensorShape ToSNPETensorShape(const UE::NNE::FTensorShape& TensorShapeNNE)
	{
		TArray<size_t> TensorShapeAsArray;
		for (int32 DimIndex = 0; DimIndex < TensorShapeNNE.Rank(); ++DimIndex)
		{
			TensorShapeAsArray.Add(TensorShapeNNE.GetData()[DimIndex]);
		}

		return zdl::DlSystem::TensorShape(TensorShapeAsArray.GetData(), TensorShapeAsArray.Num());
	}
} // namespace QCOM::NNERuntimeSNPE::Utils


namespace QCOM::NNERuntimeSNPE::Utils
{
	// ** Other SNPE utils **

	std::unique_ptr<zdl::SNPE::SNPE> BuildSNPEInstance(
		const std::unique_ptr<zdl::DlContainer::IDlContainer>& DlContainer,
		TConstArrayView<zdl::DlSystem::Runtime_t> RuntimeArray
	)
	{
		zdl::SNPE::SNPEBuilder SNPEBuilder(DlContainer.get());
		return (
			SNPEBuilder
			.setRuntimeProcessorOrder(MakeRuntimeList(RuntimeArray))
			.setPlatformConfig(MakePlatformConfig())
			.setUnconsumedTensorsAsOutputs(true)
			.setUseUserSuppliedBuffers(true)
			.setInitCacheMode(false)
			.build()
			);
	}

	zdl::DlSystem::TensorShape CalculateStrides(const zdl::DlSystem::TensorShape& TensorShapeSNPE, size_t ElementSize)
	{
		// ! example: tensor shape [4, 3, 2] with element size 4 => strides of [24, 8, 4] i.e. [3 * (2 * (4)), 2 * (4), 4]

		size_t Rank = TensorShapeSNPE.rank();

		if (Rank == 0)
		{
			// trivial (scalar) tensor; no strides
			return {};
		}

		zdl::DlSystem::TensorShape Strides = TensorShapeSNPE;
		Strides[Rank - 1] = ElementSize;
		for (int Dim = Rank - 2; Dim >= 0; --Dim)
		{
			Strides[Dim] = TensorShapeSNPE[Dim + 1] * Strides[Dim + 1];
		}

		return Strides;
	}

	std::unique_ptr<zdl::DlSystem::IUserBuffer> CreateUserBuffer(
		const UE::NNE::FTensorShape& TensorShapeNNE,
		TArray<uint8>& BackingArray,
		zdl::DlSystem::UserBufferEncoding* UserBufferEncoding
	)
	{
		auto ElementSize = UserBufferEncoding->getElementSize();
		auto TensorSizeInBytes = TensorShapeNNE.Volume() * ElementSize;

		BackingArray.SetNumZeroed(TensorSizeInBytes);

		const auto& TensorShapeSNPE = ToSNPETensorShape(TensorShapeNNE);

		auto UserBuffer = zdl::SNPE::SNPEFactory::getUserBufferFactory().createUserBuffer(
			static_cast<void*>(BackingArray.GetData()),
			TensorSizeInBytes,
			CalculateStrides(TensorShapeSNPE, ElementSize),
			UserBufferEncoding
		);
		return UserBuffer;
	}

	bool SetupTensorAttributes(
		FTensorAttributes& OutTensorAttributes,
		const std::unique_ptr<zdl::SNPE::SNPE>& SNPEInstance,
		const DlSystem::StringList& TensorNames
	)
	{
		const auto NumTensors = TensorNames.size();
		OutTensorAttributes.SetNumZeroed(NumTensors);

		for (int TensorIndex = 0; TensorIndex < TensorNames.size(); ++TensorIndex)
		{
			const char* TensorName = TensorNames.at(TensorIndex);
			auto TensorBufferAttributesOptional = SNPEInstance->getInputOutputBufferAttributes(TensorName);
			if (!TensorBufferAttributesOptional)
			{
				return false;
			}

			// ! Ensures the attributes held aren't owned by this Optional object.
			// ! Without this call, the UserBufferEncoding being read will not be valid after this scope.
			TensorBufferAttributesOptional.release();

			const auto& TensorBufferAttributes = *(TensorBufferAttributesOptional);

			const auto TensorShapeSNPE = TensorBufferAttributes->getDims();
			UE::NNE::FSymbolicTensorShape SymTensorShapeNNE = ToNNESymbolicTensorShape(TensorShapeSNPE);

			const auto TensorDataTypeSNPE = TensorBufferAttributes->getEncodingType();
			ENNETensorDataType TensorDataTypeNNE = ToNNEDataType(TensorDataTypeSNPE);

			OutTensorAttributes.TensorDescs[TensorIndex] = UE::NNE::FTensorDesc::Make(TensorName, SymTensorShapeNNE, TensorDataTypeNNE);
			OutTensorAttributes.UserBufferEncodings[TensorIndex] = TensorBufferAttributes->getEncoding();

			// ! no support for variable dimensions => assume shapes are concrete and load them directly
			check(SymTensorShapeNNE.IsConcrete());
			OutTensorAttributes.TensorShapes[TensorIndex] = UE::NNE::FTensorShape::MakeFromSymbolic(SymTensorShapeNNE);
		}

		return true;
	}

	bool SetupUserBufferMapData(
		FUserBufferMapData& OutMapData,
		const FTensorAttributes& TensorAttributes
	)
	{
		int32 NumTensors = TensorAttributes.Num();
		OutMapData.SetNumBuffers(NumTensors);

		OutMapData.UserBufferMap.clear();

		for (int32 TensorIndex = 0; TensorIndex < NumTensors; ++TensorIndex)
		{
			const auto& TensorShapeNNE = TensorAttributes.TensorShapes[TensorIndex];
			const auto& TensorDescNNE = TensorAttributes.TensorDescs[TensorIndex];
			auto* UserBufferEncoding = TensorAttributes.UserBufferEncodings[TensorIndex];

			auto& BackingArray = OutMapData.BackingArrays[TensorIndex];
			BackingArray.Empty();

			const auto TensorNameIntermediate = StringCast<ANSICHAR>(*(TensorDescNNE.GetName()));
			const char* TensorName = TensorNameIntermediate.Get();

			std::unique_ptr<zdl::DlSystem::IUserBuffer> UserBuffer = (
				CreateUserBuffer(TensorShapeNNE, BackingArray, UserBufferEncoding)
				);
			if (!UserBuffer.get())
			{
				return false;
			}

			OutMapData.UserBufferMap.add(TensorName, UserBuffer.get());
			OutMapData.UserBuffers[TensorIndex] = std::move(UserBuffer);
		}

		return true;
	}

	bool SetupSharedModelProperties(
		FSharedModelProperties& OutSharedModelProperties,
		const TSharedPtr<UE::NNE::FSharedModelData>& SharedModelData,
		TArray<zdl::DlSystem::Runtime_t> RuntimeArray
	)
	{
		FModelDataParts DataParts;
		if (!DecomposeModelData(DataParts, SharedModelData->GetView()))
		{
			return false;
		}

		OutSharedModelProperties.DlContainer = zdl::DlContainer::IDlContainer::open(
			DataParts.DLCDataView.GetData(), DataParts.DLCDataView.Num()
		);
		if (OutSharedModelProperties.DlContainer.get() == nullptr)
		{
			return false;
		}

		OutSharedModelProperties.RuntimeArray = RuntimeArray;

		// build a sample SNPE instance to read common attributes from
		const auto& SampleSNPEInstance = BuildSNPEInstance(
			OutSharedModelProperties.DlContainer,
			OutSharedModelProperties.RuntimeArray
		);
		if (!SampleSNPEInstance.get())
		{
			return false;
		}

		const auto& InputTensorNamesOptional = SampleSNPEInstance->getInputTensorNames();
		const auto& OutputTensorNamesOptional = SampleSNPEInstance->getOutputTensorNames();
		if (!InputTensorNamesOptional || !OutputTensorNamesOptional)
		{
			return false;
		}

		if (
			!SetupTensorAttributes(OutSharedModelProperties.InputTensorAttributes, SampleSNPEInstance, *InputTensorNamesOptional) ||
			!SetupTensorAttributes(OutSharedModelProperties.OutputTensorAttributes, SampleSNPEInstance, *OutputTensorNamesOptional)
			)
		{
			return false;
		}

		return true;
	}

} // namespace QCOM::NNERuntimeSNPE::Utils