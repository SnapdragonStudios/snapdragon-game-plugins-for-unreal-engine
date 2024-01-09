//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once
#include "CoreMinimal.h"
#include "HAL/Platform.h"

enum RuntimeMode : uint8
{
	RTM_CPU,
	RTM_GPU,
	RTM_DSP,
};

DECLARE_STATS_GROUP(TEXT("Qualcomm_NPE"), STATGROUP_NPE, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Inference_time"), STAT_NPE_InferenceTime, STATGROUP_NPE, SNPE_API);

class SNPE_API ISNPEObject
{
public:
	friend class TUniquePtr<ISNPEObject>;
	friend struct TDefaultDelete<ISNPEObject>;

	virtual bool IsValid() = 0;
	virtual bool ProcessModel(const TArray<TArray<float>>& inputs, TMap<FString, TArray<TArray<float>>>& outputs, TMap<FString, TArray<uint8>>& outputsDimensions) = 0;
	virtual bool GetModelInfo(uint8& inputsRank, TArray<uint8>& inputDimensions, TMap<FString, FString>& outputsDimensions) = 0;

	static ISNPEObject* Create(const FString& Model, bool usintInitCaching, RuntimeMode runtime, const TArray<FString>& UDOs);
	static void Free(TUniquePtr<ISNPEObject>& p);

protected:

	ISNPEObject();
	virtual ~ISNPEObject() {};

	FString dlc = "";
	TArray<FString> UDOPaths = {};
};

class SNPE_API FSNPEObject
{
	TUniquePtr<ISNPEObject> PrivateImpl = nullptr;
public:
	FSNPEObject(const FString& Model, bool usingInitCaching, RuntimeMode runtime, const TArray<FString>& UDOs = {}, bool multithreaded = false);
	~FSNPEObject();
	bool IsValid() { return PrivateImpl->IsValid(); }

	bool RunInference(const TArray<TArray<float>>& inputs, TMap<FString, TArray<TArray<float>>>& outputs, TMap<FString, TArray<uint8>>& outputsDimensions);

	bool ProcessModel(const TArray<TArray<float>>& inputs, TMap<FString, TArray<TArray<float>>>& outputs, TMap<FString, TArray<uint8>>& outputsDimensions) { return PrivateImpl->ProcessModel(inputs, outputs, outputsDimensions); }

	void GetModelInfo(uint8& inputsRank, TArray<uint8>& inputDimensions, TMap<FString,FString>& outputsDimensions) { PrivateImpl->GetModelInfo(inputsRank, inputDimensions, outputsDimensions); }

private:
	FSNPEObject(const FSNPEObject& other) {}
	FSNPEObject& operator=(const FSNPEObject& other){ return *this;}

	//Multithreading
	bool UseMultithread = false;
	FCriticalSection CriticalSection;
};

