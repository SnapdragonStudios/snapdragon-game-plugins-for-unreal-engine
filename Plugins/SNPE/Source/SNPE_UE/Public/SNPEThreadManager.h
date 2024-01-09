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
//Multithread
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "SNPEThreadManager.generated.h"

class FSNPEThreadProcess;

UCLASS()
class SNPE_UE_API USNPEThreadManager : public UObject, public FTickableGameObject
{
	GENERATED_BODY()
	private:
		static USNPEThreadManager* instance;
		USNPEThreadManager();
		~USNPEThreadManager();

		bool bIsCreateOnRunning;
public:
		

		static USNPEThreadManager* GetSNPEThreadManager()
		{
			if (!instance)
				instance = NewObject<USNPEThreadManager>(GetTransientPackage(), "SNPEThreadManager");
			return instance;
		}

		static void ResetInstance()
		{
			instance = nullptr;
		}

		//Tickable
		virtual void Tick(float DeltaTime) override;
		virtual bool IsTickable() const override;
		virtual TStatId GetStatId() const override;

		struct ThreadObjects
		{
			TSharedPtr<FSNPEThreadProcess> Process;
			TSharedPtr<FRunnableThread> RunnableThread;

			~ThreadObjects()
			{
				RunnableThread.Reset();
				Process.Reset();
			}
		};

		void CreateSNPEThreadProcess(FSNPEObject* snpeObject, ISNPEInferenceRequestHandler* requester, const TArray<TArray<float>>& inputs, TWeakPtr<FSNPEThreadProcess>& snpeThreadProcess, TWeakPtr<FRunnableThread>& Runnable);
		void CreateSNPEThreadProcess(FSNPEObject* snpeObject, ISNPEInferenceRequestHandler* requester, const TArray<TArray<float>>& inputs);
		TArray<ThreadObjects> Processes;
};