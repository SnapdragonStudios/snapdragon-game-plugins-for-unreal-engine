//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once
#include "SNPEThreadManager.h"
#include "SNPEThreadProcess.h"

USNPEThreadManager* USNPEThreadManager::instance = nullptr;

USNPEThreadManager::USNPEThreadManager()
{
	bIsCreateOnRunning = true;
}

USNPEThreadManager::~USNPEThreadManager()
{
	for (auto& item : Processes)
	{
		item.RunnableThread->Suspend(true);
		bool running = item.Process->IsRunning();
		item.RunnableThread->Suspend(false);
		if (running)
		{
			item.RunnableThread->WaitForCompletion();
		}
		item.RunnableThread->Kill();
	}
	Processes.Empty();
	instance = nullptr;
}

void USNPEThreadManager::Tick(float DeltaTime)
{
	for (int i = 0; i < Processes.Num(); ++i)
	{
		if(Processes[i].RunnableThread.IsValid())
		{
			Processes[i].RunnableThread->Suspend(true);
			bool running = Processes[i].Process->IsRunning();
			Processes[i].RunnableThread->Suspend(false);
			if (!running)
			{
				Processes[i].RunnableThread->Kill();
				Processes[i] = Processes.Last();
				Processes.Pop();
			}
		}
		else
		{
			Processes[i] = Processes.Last();
			Processes.Pop();
		}
	}
}

bool USNPEThreadManager::IsTickable() const
{
	return bIsCreateOnRunning;
}

TStatId USNPEThreadManager::GetStatId() const
{
	return UObject::GetStatID();
}

void USNPEThreadManager::CreateSNPEThreadProcess(FSNPEObject* snpeObject, ISNPEInferenceRequestHandler* requester, const TArray<TArray<float>>& inputs, TWeakPtr<FSNPEThreadProcess>& snpeThreadProcess, TWeakPtr<FRunnableThread>& Runnable)
{
	ThreadObjects NewThreadObject;
	NewThreadObject.Process = TSharedPtr<FSNPEThreadProcess>(new FSNPEThreadProcess(snpeObject, requester, inputs));
	NewThreadObject.RunnableThread = TSharedPtr<FRunnableThread>(FRunnableThread::Create(NewThreadObject.Process.Get(), TEXT("SNPE Thread Process")));
	int index = Processes.Add(NewThreadObject);
	snpeThreadProcess = Processes[index].Process;
	Runnable = Processes[index].RunnableThread;

}

void USNPEThreadManager::CreateSNPEThreadProcess(FSNPEObject* snpeObject, ISNPEInferenceRequestHandler* requester, const TArray<TArray<float>>& inputs)
{
	ThreadObjects NewThreadObject;
	NewThreadObject.Process = TSharedPtr<FSNPEThreadProcess>(new FSNPEThreadProcess(snpeObject, requester, inputs));
	NewThreadObject.RunnableThread = TSharedPtr<FRunnableThread>(FRunnableThread::Create(NewThreadObject.Process.Get(), TEXT("SNPE Thread Process")));
	Processes.Add(NewThreadObject);

}

