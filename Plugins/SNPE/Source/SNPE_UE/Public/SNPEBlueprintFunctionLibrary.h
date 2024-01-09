//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "SNPENeuralNetwork.h"
#include "SNPEBlueprintFunctionLibrary.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInferenceResponseDelegate, const FSNPE_Result&, Result);

UCLASS()
class SNPE_UE_API USNPERunInference : public UBlueprintAsyncActionBase, public ISNPEInferenceRequestHandler
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
		FInferenceResponseDelegate Success;

	UPROPERTY(BlueprintAssignable)
		FInferenceResponseDelegate Failure;

	// On success, returns a path from Start to End location. Both start and end must be inside the given Volume.
	// If start or end is unreachable (or time limit was exceeded) returns nothing.
	// SmoothingPasses - During a smoothing pass, every other node is potentially removed, as long as there is an empty space to the next one.
	// With SmoothingPasses=0, the path will be very jagged since the graph is Discrete.
	// With SmoothingPasses > 2 there is a potential loss of data, especially if a custom Cost function is used.
	//UFUNCTION(BlueprintCallable, Category = CPath, meta = (BlueprintInternalUseOnly = "true"))
	//	static UCPathAsyncFindPath* FindPathAsync(class ACPathVolume* Volume, FVector StartLocation, FVector EndLocation, int SmoothingPasses = 2, int32 UserData = 0, float TimeLimit = 0.2f);

	virtual void Activate() override;
	virtual void BeginDestroy() override;

	UFUNCTION()
	virtual void InferenceResponseHandling(const FSNPE_Result& Result) override;


	UFUNCTION(BlueprintCallable, Category = "Qualcomm|NPE", Meta = (BlueprintInternalUseOnly = "true"))
	static USNPERunInference* RunInferenceAsync(USNPENeuralNetwork* NeuralNetwork, const TArray<float>& Inputs, uint8 Batches);


private:
	USNPENeuralNetwork* RunningNeuralNetwork = nullptr;
	TArray<float> NetInputs;
	uint8 BatchNum = 1;
};


/**
 * 
 */
UCLASS()
class SNPE_UE_API USNPEBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintCallable, Category = "Qualcomm|NPE|InferenceResultHelpers")
	static TArray<FString> GetOutputNames(const FSNPE_Result& Result);

	UFUNCTION(BlueprintCallable, Category = "Qualcomm|NPE|InferenceResultHelpers")
	static bool GetOutputsDimensionsForNamedOutput(const FSNPE_Result& Result, const FString& OutputName, TArray<uint8>& Dimensions);

	UFUNCTION(BlueprintCallable, Category = "Qualcomm|NPE|InferenceResultHelpers")
	static bool GetFlattenValuesForNamedOutput(const FSNPE_Result& Result, const FString& OutputName, TArray<float>& Output, uint8& batches);

	UFUNCTION(BlueprintCallable, Category = "Qualcomm|NPE|InferenceResultHelpers")
	static bool GetOutputsDimensionsAt(const FSNPE_Result& Result, uint8 index, TArray<uint8>& Dimensions);

	UFUNCTION(BlueprintCallable, Category = "Qualcomm|NPE|InferenceResultHelpers")
	static bool GetFlattenValuesAt(const FSNPE_Result& Result, uint8 index, TArray<float>& Output, uint8& batches);

	static void SplitUDOFiles(const FString& UDOs, TArray<FString>& UDOFiles);

};
