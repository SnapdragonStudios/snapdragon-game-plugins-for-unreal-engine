//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "NNERuntimeSNPEModelDataFactory.h"

#include "CoreMinimal.h"
#include "Editor.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "NNEModelData.h"
#include "Subsystems/ImportSubsystem.h"


UNNERuntimeSNPEModelDataFactory::UNNERuntimeSNPEModelDataFactory(const FObjectInitializer& ObjectInitializer)
	: UFactory(ObjectInitializer)
{
	bCreateNew = false;
	bEditorImport = true;
	SupportedClass = UNNEModelData::StaticClass();
	ImportPriority = DefaultImportPriority;
	Formats.Add("dlc;Deep Learning Container Format by Qualcomm");
}

UObject* UNNERuntimeSNPEModelDataFactory::FactoryCreateBinary(
	UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags,
	UObject* Context, const TCHAR* Type,
	const uint8*& Buffer, const uint8* BufferEnd,
	FFeedbackContext* Warn
)
{
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, Class, InParent, Name, Type);

	if (!Type || !Buffer || !BufferEnd || BufferEnd - Buffer <= 0)
	{
		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);
		return nullptr;
	}

	TConstArrayView<uint8> BufferView = MakeArrayView(Buffer, BufferEnd - Buffer);

	// parse and add any additional file data needed to this map
	TMap<FString, TConstArrayView<uint8>> AdditionalFileData;

	UNNEModelData* ModelData = NewObject<UNNEModelData>(InParent, Class, Name, Flags);
	ModelData->Init(Type, BufferView, AdditionalFileData);

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, ModelData);

	return ModelData;
}

bool UNNERuntimeSNPEModelDataFactory::FactoryCanImport(const FString& Filename)
{
	return Filename.EndsWith(FString("dlc"));
}
