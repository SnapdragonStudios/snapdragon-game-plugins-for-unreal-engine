//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SNPENeuralNetworkComponent.h"
#include "CoreMinimal.h"
#include "SNPEThreadManager.h"

// Sets default values for this component's properties
USNPENeuralNetworkComponent::USNPENeuralNetworkComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;

}


USNPENeuralNetworkComponent::~USNPENeuralNetworkComponent()
{
	if (NeuralNetwork.IsValid())
	{
		NeuralNetwork.Reset();
	}
}

void USNPENeuralNetworkComponent::BeginPlay()
{
	Super::BeginPlay();
	LoadNeuralNetwork();
}

void USNPENeuralNetworkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (NeuralNetwork.IsValid())
	{
		NeuralNetwork.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void USNPENeuralNetworkComponent::LoadNeuralNetwork()
{
	if (NeuralNetwork.IsValid())
		return;

	RuntimeMode mode = RTM_CPU;
	switch (Runtime)
	{
	case NPERuntimeMode::NPE_R_GPU:
		mode = RuntimeMode::RTM_GPU;
		break;
	case NPERuntimeMode::NPE_R_DSP:
		mode = RuntimeMode::RTM_DSP;
		break;
	default:
		break;
	}
	NeuralNetwork.Reset(new FSNPEObject(Model, false, mode, UDOFiles, false));

}

void USNPENeuralNetworkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{

}

void USNPENeuralNetworkComponent::InferenceResponseHandling(const FSNPE_Result& Result)
{
	ResultCallback.Broadcast(Result);
	ResultCallbackBP.Broadcast(Result);
	
	OnInferenceFinished(Result);
}

bool USNPENeuralNetworkComponent::RunInference(const TArray<float>& FlattenInputs, uint8 Batches, FSNPE_Result& Result)
{
	if ((int)(FlattenInputs.Num()) % Batches != 0)
		return false;
	TArray<TArray<float>> Batched;
	int CurrentIndex = 0;
	for (int i = 0; i < FlattenInputs.Num(); ++i)
	{
		if ((int)(FlattenInputs.Num()) % Batches != 0)
			CurrentIndex = Batched.Add(TArray<float>());
		Batched[CurrentIndex].Add(FlattenInputs[i]);
	}
	if (NeuralNetwork.IsValid() && NeuralNetwork->IsValid())
	{
		Result.Success = NeuralNetwork->RunInference(Batched, Result.Outputs, Result.OutputsDimensions);
		return true;
	}
	return false;
}



bool USNPENeuralNetworkComponent::RunInferenceAsync(const TArray<float>& Inputs, uint8 Batches)
{
	if ((int)(Inputs.Num()) % Batches != 0)
		return false;
	TArray<TArray<float>> Batched;
	int CurrentIndex = 0;
	for (int i = 0; i < Inputs.Num(); ++i)
	{
		if ((int)(Inputs.Num()) % Batches != 0)
			CurrentIndex = Batched.Add(TArray<float>());
		Batched[CurrentIndex].Add(Inputs[i]);
	}

	if (NeuralNetwork.IsValid() && NeuralNetwork->IsValid())
	{
		USNPEThreadManager::GetSNPEThreadManager()->CreateSNPEThreadProcess(NeuralNetwork.Get(), Cast<ISNPEInferenceRequestHandler>(this), Batched);
		return true;
	}
	return false;

}

bool USNPENeuralNetworkComponent::IsNeuralNetworkValid()
{
	return NeuralNetwork.IsValid() && NeuralNetwork->IsValid();
}

bool USNPENeuralNetworkComponent::GetModelInfo(uint8& inputsRank, TArray<uint8>& inputDimensions, TMap<FString, FString>& OutputsDimensions)
{
	if (NeuralNetwork.IsValid() && NeuralNetwork->IsValid())
	{
		NeuralNetwork->GetModelInfo(inputsRank,inputDimensions,OutputsDimensions);
		return true;
	}
	return false;
}

bool USNPENeuralNetworkComponent::RunInference(const TArray<TArray<float>>& Inputs, FSNPE_Result& Result)
{
	if (NeuralNetwork.IsValid() && NeuralNetwork->IsValid())
	{
		Result.Success = NeuralNetwork->RunInference(Inputs, Result.Outputs, Result.OutputsDimensions);
		return true;
	}
	return false;
}


bool USNPENeuralNetworkComponent::RunInferenceAsync(const TArray<TArray<float>>& Inputs)
{
	if (NeuralNetwork.IsValid() && NeuralNetwork->IsValid())
	{
		USNPEThreadManager::GetSNPEThreadManager()->CreateSNPEThreadProcess(NeuralNetwork.Get(), Cast<ISNPEInferenceRequestHandler>(this), Inputs);
		return true;
	}
	return false;

}

void USNPENeuralNetworkComponent::bindCallbackFunction(ISNPEInferenceRequestHandler* listener)
{
	ResultCallback.AddRaw(listener,&ISNPEInferenceRequestHandler::InferenceResponseHandling);
}

