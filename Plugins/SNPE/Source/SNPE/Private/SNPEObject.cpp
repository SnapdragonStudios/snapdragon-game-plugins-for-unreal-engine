//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SNPEObject.h"

DEFINE_STAT(STAT_NPE_InferenceTime);

ISNPEObject::ISNPEObject()
{
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------


FSNPEObject::FSNPEObject(const FString& Model, bool usingInitCaching, RuntimeMode runtime, const TArray<FString>& UDOs, bool multithreaded) : PrivateImpl(ISNPEObject::Create(Model, usingInitCaching, runtime, UDOs)), UseMultithread(multithreaded)
{
}

FSNPEObject::~FSNPEObject()
{
	ISNPEObject::Free(PrivateImpl);
	CriticalSection.Unlock();
}


bool FSNPEObject::RunInference(const TArray<TArray<float>>& inputs, TMap<FString, TArray<TArray<float>>>& outputs, TMap<FString, TArray<uint8>>& outputsDimensions)
{
	return ProcessModel(inputs, outputs, outputsDimensions);
}

