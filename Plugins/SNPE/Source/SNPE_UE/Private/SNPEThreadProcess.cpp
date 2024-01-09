//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SNPEThreadProcess.h"
#include "SNPEObject.h"
#include "SNPEInferenceRequestHandler.h"

FSNPEThreadProcess::FSNPEThreadProcess(FSNPEObject* snpeObject, ISNPEInferenceRequestHandler* requester, const TArray<TArray<float>>& inputs)
{
	SNPEObject = snpeObject;
	check(snpeObject != nullptr);
	if(requester != nullptr)
	{
		ISNPEInferenceRequestHandler* IRequester = Cast<ISNPEInferenceRequestHandler>(requester);
		OnInferenceComplete.BindRaw(requester, &ISNPEInferenceRequestHandler::InferenceResponseHandling);
	}

	Inputs.Empty();
	for (const TArray<float>& arr : inputs)
	{
		Inputs.Add(arr);
	}
}

FSNPEThreadProcess::~FSNPEThreadProcess()
{
	OnInferenceComplete.Unbind();
	CriticalSection.Unlock();
}

bool FSNPEThreadProcess::Init()
{
	CriticalSection.Lock();
	Running = true;
	CriticalSection.Unlock();
	return true;
}

uint32 FSNPEThreadProcess::Run()
{
	FSNPE_Result result = FSNPE_Result();
	TMap<FString, TArray<TArray<float>>> outputs;
	TMap<FString, TArray<uint8>> outputsDimensions;
	{		
		SCOPE_CYCLE_COUNTER(STAT_NPE_InferenceTime);
		result.Success = SNPEObject->ProcessModel(Inputs, outputs, outputsDimensions);
	}
	if(OnInferenceComplete.IsBound())
	{
		result.Outputs = outputs;
		result.OutputsDimensions = outputsDimensions;
		result.Success = true;
		OnInferenceComplete.Execute(result);
	}
	return 0;
}

void FSNPEThreadProcess::Stop()
{
	CriticalSection.Lock();
	Running = false;
	CriticalSection.Unlock();

}

void FSNPEThreadProcess::Exit()
{
	CriticalSection.Lock();
	Running = false;
	CriticalSection.Unlock();
}

bool FSNPEThreadProcess::IsRunning()
{
	bool Result;
	CriticalSection.Lock();
	Result = Running;
	CriticalSection.Unlock();
	return Result;
}

void FSNPEThreadProcess::UpdateInputs(const TArray<TArray<float>>& inputs)
{
	bool Update = true;
	CriticalSection.Lock();
	Update = !Running;
	CriticalSection.Unlock();
	if(Update)
	{
		Inputs.Empty();
		for (const TArray<float>& arr : inputs)
		{
			Inputs.Add(arr);
		}
	}
}

