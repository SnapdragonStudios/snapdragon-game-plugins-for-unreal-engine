//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SNPEBlueprintFunctionLibrary.h"


USNPERunInference* USNPERunInference::RunInferenceAsync(USNPENeuralNetwork* NeuralNetwork, const TArray<float>& Inputs, uint8 Batches)
{
	USNPERunInference* Instance = NewObject<USNPERunInference>();
	Instance->RunningNeuralNetwork = NeuralNetwork;
	Instance->NetInputs = Inputs;
	Instance->BatchNum = Batches;
	return Instance;
}

TArray<FString> USNPEBlueprintFunctionLibrary::GetOutputNames(const FSNPE_Result& Result)
{
	TArray<FString> keys;
	Result.Outputs.GenerateKeyArray(keys);
	return keys;
}

bool USNPEBlueprintFunctionLibrary::GetOutputsDimensionsForNamedOutput(const FSNPE_Result& Result, const FString& OutputName, TArray<uint8>& Dimensions)
{
	if (Result.OutputsDimensions.Contains(OutputName))
	{
		Dimensions.Empty();
		Dimensions = Result.OutputsDimensions[OutputName];
		return true;
	}
	return false;

}

bool USNPEBlueprintFunctionLibrary::GetFlattenValuesForNamedOutput(const FSNPE_Result& Result, const FString& OutputName, TArray<float>& Output, uint8& batches)
{
	if (Result.Outputs.Contains(OutputName))
	{
		Output.Empty();
		batches = (int8)Result.Outputs[OutputName].Num();
		for (auto& batch : Result.Outputs[OutputName])
		{
			for (auto& val : batch)
			{
				Output.Add(val);
			}
		}
		return true;
	}
	return false;
}

bool USNPEBlueprintFunctionLibrary::GetOutputsDimensionsAt(const FSNPE_Result& Result, uint8 index, TArray<uint8>& Dimensions)
{
	if (index < (uint8)Result.OutputsDimensions.Num())
	{
		TArray<FString> keys;
		Result.OutputsDimensions.GenerateKeyArray(keys);
		Dimensions.Empty();
		Dimensions = Result.OutputsDimensions[keys[index]];
		return true;
	}
	return false;

}

bool USNPEBlueprintFunctionLibrary::GetFlattenValuesAt(const FSNPE_Result& Result, uint8 index, TArray<float>& Output, uint8& batches)
{
	if (index < (uint8)Result.Outputs.Num())
	{
		TArray<FString> keys;
		Result.OutputsDimensions.GenerateKeyArray(keys);
		Output.Empty();
		batches = (int8)Result.Outputs[keys[index]].Num();
		for (auto& batch : Result.Outputs[keys[index]])
		{
			for (auto& val : batch)
			{
				Output.Add(val);
			}
		}
		return true;
	}
	return false;

}

void USNPEBlueprintFunctionLibrary::SplitUDOFiles(const FString& UDOs, TArray<FString>& UDOFiles)
{
	FString left, right;
	while (UDOs.Split(FString(","), &left, &right))
	{
		UDOFiles.Add(left);
		SplitUDOFiles(right, UDOFiles);
	}
}

void USNPERunInference::Activate()
{
	if ((int)(NetInputs.Num()) % BatchNum != 0)
	{
		FSNPE_Result Result;
		Result.Success = false;
		Failure.Broadcast(Result);
		return;
	}
	int ElementsPerBatch = NetInputs.Num() / BatchNum;
	TArray<TArray<float>> Batched;
	int CurrentIndex = 0;
	for (int i = 0; i < NetInputs.Num(); ++i)
	{
		if (i % ElementsPerBatch == 0)
			CurrentIndex = Batched.Add(TArray<float>());
		Batched[CurrentIndex].Add(NetInputs[i]);
	}

	if (RunningNeuralNetwork->IsNeuralNetworkValid())
	{
		RunningNeuralNetwork->RunInferenceAsync(Batched, this);
	}
	else
	{
		FSNPE_Result Result;
		Result.Success = false;
		Failure.Broadcast(Result);
	}
}

void USNPERunInference::BeginDestroy()
{
	Super::BeginDestroy();
}

void USNPERunInference::InferenceResponseHandling(const FSNPE_Result& Result)
{
	if (Result.Success)
	{
		Success.Broadcast(Result);
	}
	else
	{
		Failure.Broadcast(Result);
	}
	SetReadyToDestroy();
}
