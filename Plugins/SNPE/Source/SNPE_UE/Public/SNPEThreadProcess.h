//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once
#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "SNPEInferenceRequestHandler.h"

class FSNPEObject;
class FRunnableThread;

DECLARE_DELEGATE_OneParam(FSNPEInferenceRequestDelegate, const FSNPE_Result&);

class SNPE_UE_API FSNPEThreadProcess : public FRunnable
{
public:
	friend class USNPEThreadManager;

	virtual bool Init();

	virtual uint32 Run();

	virtual void Stop();

	virtual void Exit();

	bool IsRunning();

	void UpdateInputs(const TArray<TArray<float>>& inputs);


	FSNPEInferenceRequestDelegate OnInferenceComplete;
	~FSNPEThreadProcess();


protected:
	FSNPEThreadProcess(FSNPEObject* snpeObject, ISNPEInferenceRequestHandler* requester, const TArray<TArray<float>>& inputs);

	FCriticalSection CriticalSection;
	FSNPEObject* SNPEObject = nullptr;
	bool Running = false;
	TArray<TArray<float>> Inputs;

};
