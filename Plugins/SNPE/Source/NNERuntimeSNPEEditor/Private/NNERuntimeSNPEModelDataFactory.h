//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "UObject/UObjectGlobals.h"
#include "Factories/Factory.h"

#include "NNERuntimeSNPEModelDataFactory.generated.h"

UCLASS()
class NNERUNTIMESNPEEDITOR_API UNNERuntimeSNPEModelDataFactory : public UFactory
{
	GENERATED_BODY()

public:
	UNNERuntimeSNPEModelDataFactory(const FObjectInitializer& ObjectInitializer);

public:
	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateBinary(
		UClass* Class, UObject* InParent,
		FName Name, EObjectFlags Flags,
		UObject* Context, const TCHAR* Type,
		const uint8*& Buffer, const uint8* BufferEnd,
		FFeedbackContext* Warn
	) override;

	virtual bool FactoryCanImport(const FString& Filename) override;
	//~ End UFactory Interface
};