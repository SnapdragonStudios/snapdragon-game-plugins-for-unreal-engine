//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================


#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SNPETypes.h"
#include "SNPEObject.h"
#include "SNPEInferenceRequestHandler.h"
#include "SNPENeuralNetworkComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSNPENeuralNetworkComponentBlueprintResult,const FSNPE_Result&, Result);
DECLARE_MULTICAST_DELEGATE_OneParam(FSNPENeuralNetworkComponentResult, const FSNPE_Result&);

UCLASS(Blueprintable, BlueprintType, meta=(BlueprintSpawnableComponent) )
class SNPE_UE_API USNPENeuralNetworkComponent : public UActorComponent, public ISNPEInferenceRequestHandler
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	USNPENeuralNetworkComponent();
	~USNPENeuralNetworkComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void LoadNeuralNetwork();

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void InferenceResponseHandling(const FSNPE_Result& Result) override;

	UFUNCTION(BlueprintCallable, Category = "Qualcomm|NPE|NeuralNetworkComponent")
	bool RunInference(const TArray<float>& FlattenInputs, uint8 Batches, FSNPE_Result& Result);

	UFUNCTION(BlueprintCallable, Category = "Qualcomm|NPE|NeuralNetworkComponent")
	bool RunInferenceAsync(const TArray<float>& Inputs, uint8 Batches);

	UFUNCTION(BlueprintCallable, Category = "Qualcomm|NPE|NeuralNetworkComponent")
	bool IsNeuralNetworkValid();

	UFUNCTION(BlueprintCallable, Category = "Qualcomm|NPE|NeuralNetworkComponent")
	bool GetModelInfo(uint8& inputsRank, TArray<uint8>& inputDimensions, TMap<FString, FString>& OutputsDimensions);

	bool RunInference(const TArray<TArray<float>>& Inputs, FSNPE_Result& Result);

	bool RunInferenceAsync(const TArray<TArray<float>>& Inputs);

	void bindCallbackFunction(ISNPEInferenceRequestHandler* listener);

	UFUNCTION(BlueprintImplementableEvent, Category = "Qualcomm|NPE|NeuralNetworkComponent")
	void OnInferenceFinished(const FSNPE_Result& Result);

public:
	UPROPERTY(EditAnywhere, Category="Qualcomm|NPE|NeuralNetworkComponent")
	FString Model; 
	UPROPERTY(EditAnywhere, Category = "Qualcomm|NPE|NeuralNetworkComponent")
	TArray<FString> UDOFiles;
	UPROPERTY(EditAnywhere, Category = "Qualcomm|NPE|NeuralNetworkComponent")
	TEnumAsByte<NPERuntimeMode> Runtime;

	UPROPERTY(BlueprintAssignable, Category = "Qualcomm|NPE|NeuralNetworkComponent")
	FSNPENeuralNetworkComponentBlueprintResult ResultCallbackBP;

	FSNPENeuralNetworkComponentResult ResultCallback;
protected:

	TUniquePtr<FSNPEObject> NeuralNetwork;
};
