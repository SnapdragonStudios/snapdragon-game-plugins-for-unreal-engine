//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "CoreMinimal.h"
#include "SNPEObject.h"
#include "HAL/Platform.h"

#if PLATFORM_ANDROID
#include "DlSystem/DlEnums.hpp"
#include "DlSystem/RuntimeList.hpp"
#include "DlContainer/IDlContainer.hpp"
#include "DlSystem/PlatformConfig.hpp"
#include "SNPE/SNPE.hpp"

class SNPE_API FSNPEAndroidObject: public ISNPEObject
{
public:
	FSNPEAndroidObject(const FString& Model, bool usintInitCaching, RuntimeMode runtime, const TArray<FString>& UDOs);
	~FSNPEAndroidObject() {}
	//TODO-ccastril: move to AndroidPlatformProcess.h
	static FString GetNativeLibraryDir();

protected:
	zdl::DlSystem::Runtime_t Runtime = zdl::DlSystem::Runtime_t::CPU;
	zdl::DlSystem::RuntimeList runtimeList;
	std::unique_ptr<zdl::SNPE::SNPE> snpe;
	zdl::DlSystem::PlatformConfig platformConfig;
	bool UsingInitCaching = false;
	bool Valid = false;

	bool Initialize();

	zdl::DlSystem::Runtime_t checkRuntime(zdl::DlSystem::Runtime_t runtime);
	std::unique_ptr<zdl::DlContainer::IDlContainer> loadContainerFromFile(const FString& containerPath);
	std::unique_ptr<zdl::SNPE::SNPE> setBuilderOptions(std::unique_ptr<zdl::DlContainer::IDlContainer> & container,
                                                   zdl::DlSystem::Runtime_t runtime,
                                                   zdl::DlSystem::RuntimeList runtime_List,
                                                   bool useUserSuppliedBuffers,
                                                   zdl::DlSystem::PlatformConfig platform_Config,
                                                   bool useCaching);
	// Load a batched single input tensor for a network which requires a single input
	std::unique_ptr<zdl::DlSystem::ITensor> loadInputTensor(std::unique_ptr<zdl::SNPE::SNPE>& snpe, const TArray<TArray<float>>& inputs);


	bool getOutput(zdl::DlSystem::TensorMap outputTensorMap,
		TMap<FString, TArray<TArray<float>>>& output,
		TMap<FString, TArray<uint8>>& outputsDimensions,
		size_t batchSize);

	bool getITensorBatched(TArray<TArray<float>>& out, TArray<uint8>& dimensions, const zdl::DlSystem::ITensor* tensor, size_t batchIndex, size_t batchChunk);

	bool loadUDOPackage(TArray<FString>& UdoPackagePath);
	void updateModelInputInfo(std::unique_ptr<zdl::SNPE::SNPE>& _snpe);
	void updateOutputInfo(const zdl::DlSystem::TensorMap& outputTensorMap, const size_t& batchSize);

public:
	virtual bool IsValid() override {return Valid;}

	virtual bool ProcessModel(const TArray<TArray<float>>& inputs, TMap<FString, TArray<TArray<float>>>& outputs, TMap<FString, TArray<uint8>>& outputsDimensions) override;

	virtual bool GetModelInfo(uint8& inputsRank, TArray<uint8>& inputDimensions, TMap<FString, FString>& outputsDimensions) override;

protected:
	FString ModelLocation = "/sdcard/Android/data/com.SnapdragonStudios.AirDerbyEX/files/UnrealGame/AirDerbyEX/AirDerbyEX/Content/MachineLearningModels/";

	uint8 InputsRank;
	TArray<uint8> InputDimensions;
	TMap<FString, TArray<uint8>> OutputsDimensions;
	bool OutputInfoKnown = false;
	FCriticalSection CriticalSection;
};

#endif