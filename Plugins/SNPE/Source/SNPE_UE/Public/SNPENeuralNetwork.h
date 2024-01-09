//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SNPEObject.h"
#include "SNPETypes.h"
#include "SNPEInferenceRequestHandler.h"
#include "SNPENeuralNetwork.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSNPENeuralNetworkObjectBlueprintResult, const FSNPE_Result&, Result);
DECLARE_MULTICAST_DELEGATE_OneParam(FSNPENeuralNetworkObjecttResult, const FSNPE_Result&);

UCLASS(Blueprintable, BlueprintType)
class SNPE_UE_API USNPENeuralNetwork : public UObject, public ISNPEInferenceRequestHandler
{
	GENERATED_BODY()

	public:
		USNPENeuralNetwork();
		~USNPENeuralNetwork();

		void LoadNeuralNetwork();

		UFUNCTION(BlueprintCallable, Category = "Qualcomm|NPE|NeuralNetwork")
		bool IsNeuralNetworkValid();

		virtual void InferenceResponseHandling(const FSNPE_Result& Result) override;

		bool RunInference(const TArray<TArray<float>>& inputs, TMap<FString, TArray<TArray<float>>>& outputs, TMap<FString, TArray<uint8>>& outputsDimensions);
		bool RunInferenceAsync(const TArray<TArray<float>>& inputs, UObject* requester);

		UFUNCTION(BlueprintCallable, Category = "Qualcomm|NPE|NeuralNetwork")
		static USNPENeuralNetwork* CreateSNPENeuralNetwork(UObject* Owner, const FString& Model, const FString& UDOs, TEnumAsByte<NPERuntimeMode> Runtime);


		UFUNCTION(BlueprintCallable, Category="Qualcomm|NPE|NeuralNetwork")
		bool GetModelInfo(uint8& inputsRank, TArray<uint8>& inputDimensions, TMap<FString, FString>& OutputsDimensions);
		UFUNCTION(BlueprintCallable, Category = "Qualcomm|NPE|NeuralNetwork")
		bool RunInference(const TArray<float>& FlattenInputs, uint8 Batches, FSNPE_Result& Result);
		UFUNCTION(BlueprintCallable, Category = "Qualcomm|NPE|NeuralNetwork")
		bool RunInferenceAsync(const TArray<float>& FlattenInputs, uint8 Batches);

		UFUNCTION(BlueprintCallable, Category = "Qualcomm|NPE|NeuralNetwork")
		bool RunInferenceAsyncForRequester(UObject* Requester, const TArray<float>& FlattenInputs, uint8 Batches);

		void bindCallbackFunction(ISNPEInferenceRequestHandler* listener);

	protected:
		static void SplitUDOFiles(const FString& UDOs, TArray<FString>& UDOFiles);


	public:
		UPROPERTY(EditAnywhere, Category="Qualcomm|NPE|NeuralNetwork")
		FString Model;
		UPROPERTY(EditAnywhere, Category = "Qualcomm|NPE|NeuralNetwork")
		TArray<FString> UDOFiles;
		UPROPERTY(EditAnywhere, Category = "Qualcomm|NPE|ExecutionMode")
		TEnumAsByte<NPERuntimeMode> Runtime;

		UPROPERTY(BlueprintAssignable, Category = "Qualcomm|NPE|NeuralNetworkComponent")
		FSNPENeuralNetworkObjectBlueprintResult ResultCallbackBP;

		FSNPENeuralNetworkObjecttResult ResultCallback;

	protected:

		TUniquePtr<FSNPEObject> NeuralNetwork;

};
