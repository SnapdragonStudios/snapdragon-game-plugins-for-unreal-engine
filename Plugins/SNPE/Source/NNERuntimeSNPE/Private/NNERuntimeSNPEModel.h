//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "CoreMinimal.h"
#include "NNE.h"
#include "NNEModelBase.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h"
#include "NNETensor.h"
#include "NNETypes.h"

THIRD_PARTY_INCLUDES_START

#include "SNPE/SNPE.hpp"

#include "DlSystem/DlEnums.hpp"

THIRD_PARTY_INCLUDES_END

#include "NNERuntimeSNPEStructures.h"


// *** model ***
namespace QCOM::NNERuntimeSNPE
{
	class FModel : public UE::NNE::IModelCPU
	{
	public:
		// ** init **

		FModel() {};
		virtual ~FModel() {};

		bool Init(
			const TSharedPtr<UE::NNE::FSharedModelData>& InSharedModelData,
			TArray<zdl::DlSystem::Runtime_t> RuntimeArray
		);

	public:
		//~ begin NNE::IModelCPU overrides

		virtual TSharedPtr<UE::NNE::IModelInstanceCPU> CreateModelInstanceCPU() override;

		//~ end NNE::IModelCPU overrides

	private:
		TSharedPtr<FSharedModelProperties> SharedModelProperties;
	};
} // namespace QCOM::NNERuntimeSNPE


// *** model instance ***
namespace QCOM::NNERuntimeSNPE
{
	class FModelInstance : public UE::NNE::IModelInstanceCPU
	{

	public:
		// ** init **

		FModelInstance() {};
		virtual ~FModelInstance() {};

		bool Init(const TSharedPtr<FSharedModelProperties>& InSharedModelProperties);

	public:
		//~ begin NNE::IModelInstanceCPU overrides

		virtual inline TConstArrayView<UE::NNE::FTensorDesc> GetInputTensorDescs() const override
		{
			return SharedModelProperties->InputTensorAttributes.TensorDescs;
		}

		virtual inline TConstArrayView<UE::NNE::FTensorDesc> GetOutputTensorDescs() const override
		{
			return SharedModelProperties->OutputTensorAttributes.TensorDescs;
		}

		virtual inline TConstArrayView<UE::NNE::FTensorShape> GetInputTensorShapes() const override
		{
			return SharedModelProperties->InputTensorAttributes.TensorShapes;
		}

		virtual inline TConstArrayView<UE::NNE::FTensorShape> GetOutputTensorShapes() const override
		{
			return SharedModelProperties->OutputTensorAttributes.TensorShapes;
		}

		virtual ESetInputTensorShapesStatus SetInputTensorShapes(
			TConstArrayView<UE::NNE::FTensorShape> InInputShapes
		) override;

		virtual ERunSyncStatus RunSync(
			TConstArrayView<UE::NNE::FTensorBindingCPU> InInputBindings,
			TConstArrayView<UE::NNE::FTensorBindingCPU> InOutputBindings
		) override;

		//~ end NNE::IModelInstanceCPU overrides

	private:
		// ** intermediate steps **

		bool ValidateBindings(
			TConstArrayView<UE::NNE::FTensorBindingCPU> Bindings,
			const FTensorAttributes& TensorAttributes
		);
		bool PreProcessInputs(TConstArrayView<UE::NNE::FTensorBindingCPU> InInputBindings);
		bool PostProcessOutputs(TConstArrayView<UE::NNE::FTensorBindingCPU> InOutputBindings);

	private:
		FUserBufferMapData InputMapData;
		FUserBufferMapData OutputMapData;

		TSharedPtr<FSharedModelProperties> SharedModelProperties;

		std::unique_ptr<zdl::SNPE::SNPE> SNPEInstance;
	};
} // namespace QCOM::NNERuntimeSNPE