//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once
#include "SNPENeuralNetwork.h"
#include "SNPEThreadManager.h"

USNPENeuralNetwork* USNPENeuralNetwork::CreateSNPENeuralNetwork(UObject* Owner, const FString& Model, const FString& UDOs, TEnumAsByte<NPERuntimeMode> Runtime)
{
	USNPENeuralNetwork* NN = NewObject<USNPENeuralNetwork>(Owner);
	NN->Model = Model;
	TArray<FString> UDOFiles;
	SplitUDOFiles(UDOs, UDOFiles);
	NN->UDOFiles = UDOFiles;
	NN->Runtime = Runtime;
	NN->LoadNeuralNetwork();
	return NN;

}

void USNPENeuralNetwork::SplitUDOFiles(const FString& UDOs, TArray<FString>& UDOFiles)
{
	FString left, right;
	while (UDOs.Split(FString(","), &left, &right))
	{
		UDOFiles.Add(left);
		SplitUDOFiles(right, UDOFiles);
	}
}


USNPENeuralNetwork::USNPENeuralNetwork()
{
}

USNPENeuralNetwork::~USNPENeuralNetwork()
{
	if (NeuralNetwork.IsValid())
	{
		NeuralNetwork.Reset();
	}
}

void USNPENeuralNetwork::LoadNeuralNetwork()
{
	if(NeuralNetwork.IsValid())
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

bool USNPENeuralNetwork::IsNeuralNetworkValid()
{
	return NeuralNetwork.IsValid() && NeuralNetwork->IsValid();
}

void USNPENeuralNetwork::InferenceResponseHandling(const FSNPE_Result& Result)
{
	ResultCallback.Broadcast(Result);
	ResultCallbackBP.Broadcast(Result);
}

void USNPENeuralNetwork::bindCallbackFunction(ISNPEInferenceRequestHandler* listener)
{
	ResultCallback.AddRaw(listener, &ISNPEInferenceRequestHandler::InferenceResponseHandling);
}

bool USNPENeuralNetwork::RunInference(const TArray<TArray<float>>& inputs, TMap<FString, TArray<TArray<float>>>& outputs, TMap<FString, TArray<uint8>>& outputsDimensions)
{
	if (NeuralNetwork.IsValid() && NeuralNetwork->IsValid())
	{
		SCOPE_CYCLE_COUNTER(STAT_NPE_InferenceTime);
		return NeuralNetwork->RunInference(inputs,outputs,outputsDimensions);
	}
	return false;
}


bool USNPENeuralNetwork::RunInferenceAsync(const TArray<float>& FlattenInputs, uint8 Batches)
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
		USNPEThreadManager::GetSNPEThreadManager()->CreateSNPEThreadProcess(NeuralNetwork.Get(), Cast<ISNPEInferenceRequestHandler>(this), Batched);
		return true;
	}
	return false;
}

bool USNPENeuralNetwork::RunInferenceAsyncForRequester(UObject* Requester, const TArray<float>& Inputs, uint8 Batches)
{
	if ((int)(Inputs.Num()) % Batches != 0)
		return false;
	TArray<TArray<float>> Batched;
	int CurrentIndex = 0;
	int ElementsPerBatch = Inputs.Num() / Batches;
	for (int i = 0; i < Inputs.Num(); ++i)
	{
		if (i % ElementsPerBatch == 0)
			CurrentIndex = Batched.Add(TArray<float>());
		Batched[CurrentIndex].Add(Inputs[i]);
	}

	if (IsNeuralNetworkValid())
	{
		RunInferenceAsync(Batched, Requester);
		return true;
	}
	return false;
}

bool USNPENeuralNetwork::RunInferenceAsync(const TArray<TArray<float>>& inputs, UObject* requester)
{
	if(requester && requester->GetClass()->ImplementsInterface(USNPEInferenceRequestHandler::StaticClass()))
	{
		if (NeuralNetwork.IsValid() && NeuralNetwork->IsValid())
		{
			USNPEThreadManager::GetSNPEThreadManager()->CreateSNPEThreadProcess(NeuralNetwork.Get(), Cast<ISNPEInferenceRequestHandler>(requester), inputs);
			return true;
		}
	}
	return false;
}


bool USNPENeuralNetwork::GetModelInfo(uint8& inputsRank, TArray<uint8>& inputDimensions, TMap<FString, FString>& OutputsDimensions)
{
	if (NeuralNetwork.IsValid() && NeuralNetwork->IsValid())
	{
		NeuralNetwork->GetModelInfo(inputsRank,inputDimensions,OutputsDimensions);
		return true;
	}
	return false;
}

bool USNPENeuralNetwork::RunInference(const TArray<float>& FlattenInputs, uint8 Batches, FSNPE_Result& Result)
{
	if ((int)(FlattenInputs.Num()) % Batches != 0)
		return false;
	TArray<TArray<float>> Batched;
	int CurrentIndex = 0;
	int ElementsPerBatch = FlattenInputs.Num() / Batches;
	for (int i = 0; i < FlattenInputs.Num(); ++i)
	{
		if (i % ElementsPerBatch == 0)
			CurrentIndex = Batched.Add(TArray<float>());
		Batched[CurrentIndex].Add(FlattenInputs[i]);
	}
	if (IsNeuralNetworkValid())
	{
		Result.Success = RunInference(Batched, Result.Outputs, Result.OutputsDimensions);
	}
	return false;
}
