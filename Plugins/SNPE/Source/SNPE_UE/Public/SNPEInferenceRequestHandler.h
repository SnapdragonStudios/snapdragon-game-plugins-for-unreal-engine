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
#include "SNPEInferenceRequestHandler.generated.h"

USTRUCT(BlueprintType)
struct SNPE_UE_API FSNPE_Result
{
	GENERATED_BODY()
public:
	TMap<FString, TArray<TArray<float>>> Outputs;
	TMap<FString, TArray<uint8>> OutputsDimensions;
	bool Success = false;
};


UINTERFACE(Blueprintable, BlueprintType)
class SNPE_UE_API USNPEInferenceRequestHandler : public UInterface
{
	GENERATED_BODY()
};

class SNPE_UE_API ISNPEInferenceRequestHandler
{
	GENERATED_BODY()
public:
	virtual void InferenceResponseHandling(const FSNPE_Result& Result) = 0;

private:

};


UCLASS(Blueprintable, BlueprintType)
class SNPE_UE_API USNPEInferenceRequestHandlerObject : public UObject, public ISNPEInferenceRequestHandler
{
	GENERATED_BODY()
public:
	//HTTP request handler
	virtual void InferenceResponseHandling(const FSNPE_Result& Result) override {};
};