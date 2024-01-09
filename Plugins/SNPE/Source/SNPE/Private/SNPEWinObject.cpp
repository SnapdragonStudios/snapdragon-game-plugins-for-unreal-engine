//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SNPEWinObject.h"

#if !PLATFORM_ANDROID

#include "DlSystem/DlEnums.hpp"
#include "DlSystem/PlatformConfig.hpp"
#include "DlSystem/ITensor.hpp"
#include "DlSystem/ITensorFactory.hpp"
#include "DlSystem/TensorShape.hpp"
#include "DlSystem/TensorMap.hpp"
#include "DlSystem/StringList.hpp"

#include "DlContainer/IDlContainer.hpp"
#include "SNPE/SNPEFactory.hpp"
#include "SNPE/SNPE.hpp"

#include "SNPE/SNPEBuilder.hpp"
#include "DlSystem/String.hpp"
#include "DlSystem/StringList.hpp"
#include "DlSystem/DlVersion.hpp"
#include "DlSystem/TensorShape.hpp"

#include "GenericPlatform/GenericPlatformMisc.h"

#include <stdlib.h>
#include <fstream>

ISNPEObject* ISNPEObject::Create(const FString& Model, bool usintInitCaching, RuntimeMode runtime, const TArray<FString>& UDOs) { return new FSNPEWinObject(Model, usintInitCaching, runtime, UDOs); }
void ISNPEObject::Free(TUniquePtr<ISNPEObject>& p) { ISNPEObject* ptr = p.Release(); }



FString FSNPEWinObject::CDSP_library_path = "";

FSNPEWinObject::FSNPEWinObject(const FString& Model, bool usintInitCaching, RuntimeMode runtime, const TArray<FString>& UDOs)
{
    UDOPaths = UDOs;
	if (runtime == RTM_DSP)
	{
		Runtime = zdl::DlSystem::Runtime_t::DSP;
	}
    else if (runtime == RTM_GPU)
    {
        Runtime = zdl::DlSystem::Runtime_t::GPU;
    }
	else
	{
		Runtime = zdl::DlSystem::Runtime_t::CPU;
	}
    FString NativeLibraryPath = GetNativeLibraryDir();
    FString CDSP_Variable_Name = "ADSP_LIBRARY_PATH";
    ModelLocation = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
  //  if (NativeLibraryPath.Compare(CDSP_library_path) != 0)
  //  {
  //      CDSP_library_path = NativeLibraryPath;
  //      FString CDSP_value = TCHAR_TO_UTF8(*CDSP_library_path);
  //      CDSP_value += ";/system/lib/rfsa/adsp;/system/vendor/lib/rfsa/adsp;/dsp";
		//if (_putenv_s(TCHAR_TO_UTF8(*CDSP_Variable_Name), TCHAR_TO_UTF8(*CDSP_value)) != 0)
		//{
		//	UE_LOG(LogTemp, Warning, TEXT("SNPE: FAILED TO SET ADSP_LIBRARY_PATH ENVIRONMENT VARIABLE FOR DSP, AIP EXECUTION"));
  //      }
  //  }
    dlc = ModelLocation + Model;
    UsingInitCaching = usintInitCaching;
    Valid = Initialize();
}

FString FSNPEWinObject::GetNativeLibraryDir()
{
	FString Result/* = FWindowsPlatformProcess::ExecutablePath()*/;
 //   Result.ReplaceCharInline(*ANSI_TO_TCHAR("\\"), *ANSI_TO_TCHAR("/"));
	//FString execName = FWindowsPlatformProcess::ExecutableName(false);
 //   Result.RemoveFromEnd(execName);

	return Result;
}


bool FSNPEWinObject::Initialize()
{
    //Check runtime
    UE_LOG(LogTemp, Log, TEXT("SNPE: CHECKING RUNTIME"));
    Runtime = checkRuntime(Runtime);

    // Load DLC
    UE_LOG(LogTemp, Log, TEXT("SNPE: LOADING MODEL"));
    std::unique_ptr<zdl::DlContainer::IDlContainer> container = loadContainerFromFile(dlc);
    if (container == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("SNPE: Error while opening the container file."));
        return false;
    }

	//load UDO package
	if (false == loadUDOPackage(UDOPaths))
	{
        UE_LOG(LogTemp, Error, TEXT("Failed to load UDO Package(s)."));
		return false;
	}

    //Set builder
    UE_LOG(LogTemp, Log, TEXT("SNPE: INITIALIZING SNPE BUILDER"));
    std::string PlatformOptions = "unsignedPD:ON";
    platformConfig.setPlatformOptions(PlatformOptions);
    check(platformConfig.isOptionsValid());
    snpe = setBuilderOptions(container, Runtime, runtimeList, false, platformConfig, UsingInitCaching);
    if (snpe == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("SNPE: Error while building SNPE object."));
        return false;
    }

    updateModelInputInfo(snpe);

    if (UsingInitCaching)
    {
        if (container->save(std::string(TCHAR_TO_UTF8(*dlc)).c_str()))
        {
            UE_LOG(LogTemp, Log, TEXT("SNPE: Saved container into archive successfully"));
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("SNPE: Failed to save container into archive"));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("SNPE: IS READY TO GO"));
    return true;
}

zdl::DlSystem::Runtime_t FSNPEWinObject::checkRuntime(zdl::DlSystem::Runtime_t runtime)
{
    static zdl::DlSystem::Version_t Version = zdl::SNPE::SNPEFactory::getLibraryVersion();
    UE_LOG(LogTemp, Log, TEXT("SNPE Version: %s"), ANSI_TO_TCHAR(Version.asString().c_str()));
    //ccastril TODO->SUPPORT DSP, AIP AND GPU LATER
    if (!zdl::SNPE::SNPEFactory::isRuntimeAvailable(runtime, zdl::DlSystem::RuntimeCheckOption_t::UNSIGNEDPD_CHECK))
    {
        UE_LOG(LogTemp, Warning, TEXT("Selected runtime not present. Falling back to CPU."));
        runtime = zdl::DlSystem::Runtime_t::CPU;
    }
    return runtime;
}

std::unique_ptr<zdl::DlContainer::IDlContainer> FSNPEWinObject::loadContainerFromFile(const FString& containerPath)
{
    std::string ModelFile = std::string(TCHAR_TO_ANSI(*containerPath));
    UE_LOG(LogTemp, Log, TEXT("SNPE: OPENING MODEL FILE: %s"), *containerPath);
    std::ifstream dlcFile(ModelFile);
    if (!dlcFile) {
        UE_LOG(LogTemp, Error, TEXT("dlc file not valid."));
        return nullptr;
    }

    std::unique_ptr<zdl::DlContainer::IDlContainer> container_ptr;
    container_ptr = zdl::DlContainer::IDlContainer::open(ModelFile);
    if (!container_ptr.get())
    {
        UE_LOG(LogTemp, Error, TEXT("SNPE: opening model failed at open funcition: %s"), ModelFile.c_str());
    }

    return container_ptr;
}

std::unique_ptr<zdl::SNPE::SNPE> FSNPEWinObject::setBuilderOptions(std::unique_ptr<zdl::DlContainer::IDlContainer>& container,
    zdl::DlSystem::Runtime_t runtime,
    zdl::DlSystem::RuntimeList runtime_List,
    bool useUserSuppliedBuffers,
    zdl::DlSystem::PlatformConfig platform_Config,
    bool useCaching)
{
    std::unique_ptr<zdl::SNPE::SNPE> snpe_ptr;
    zdl::SNPE::SNPEBuilder snpeBuilder(container.get());

    if (runtime_List.empty())
    {
        runtime_List.add(runtime);
    }

    if (container.get() == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("SNPE: Container is not valid"));
    }

    snpe_ptr = snpeBuilder.setOutputLayers({})
        .setRuntimeProcessorOrder(runtime_List)
        .setUseUserSuppliedBuffers(useUserSuppliedBuffers)
        .setPlatformConfig(platform_Config)
        .setInitCacheMode(useCaching)
        .build();

    return snpe_ptr;
}

bool FSNPEWinObject::ProcessModel(const TArray<TArray<float>>& inputs, TMap<FString, TArray<TArray<float>>>& outputs, TMap<FString, TArray<uint8>>& outputsDimensions)
{
    // Check the batch size for the container
    // SNPE 1.16.0 (and newer) assumes the first dimension of the tensor shape
    // is the batch size.
    zdl::DlSystem::TensorShape tensorShape;
    tensorShape = snpe->getInputDimensions();
    size_t batchSize = tensorShape.getDimensions()[0];
    //UE_LOG(LogTemp, Log, TEXT("SNPE: INPUT DIMENSIONS: %d, %d"), tensorShape.getDimensions()[0], tensorShape.getDimensions()[1]);

    //Validate batch and input size
    if (batchSize != inputs.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("Wrong batch size of input batches, expected: %d, received: %d"), batchSize, inputs.Num());
        return false;
    }


    // A tensor map for SNPE execution outputs
    zdl::DlSystem::TensorMap outputTensorMap;

    std::unique_ptr<zdl::DlSystem::ITensor> inputTensor = loadInputTensor(snpe, inputs);
    if (!inputTensor)
    {
        UE_LOG(LogTemp, Error, TEXT("Error loading Input Tensor"));
        return false;
    }
    // Execute the input tensor on the model with SNPE
    bool execStatus = false;
	CriticalSection.Lock();
    if (snpe.get() != nullptr)
        execStatus = snpe->execute(inputTensor.get(), outputTensorMap);
    CriticalSection.Unlock();
    // Save the execution results if execution successful
	if (execStatus == true)
	{
		if (getOutput(outputTensorMap, outputs, outputsDimensions, batchSize))
		{
            if (!OutputInfoKnown)
            {
                updateOutputInfo(outputTensorMap,batchSize);
            }
			return true;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Error retrieving Output"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Error while executing the network."));
	}
    return false;
}

bool FSNPEWinObject::GetModelInfo(uint8& inputsRank, TArray<uint8>& inputDimensions, TMap<FString, FString>& outputsDimensions)
{
    inputsRank = InputsRank;
    inputDimensions = InputDimensions;
    
    if (!OutputInfoKnown && snpe != nullptr)
    {
		zdl::DlSystem::TensorShape tensorShape = snpe->getInputDimensions();
		size_t batchsize = tensorShape.getDimensions()[0];

        // Execute the input tensor on the model with SNPE
		std::unique_ptr<zdl::DlSystem::ITensor> input = zdl::SNPE::SNPEFactory::getTensorFactory().createTensor(tensorShape);
		zdl::DlSystem::TensorMap outputTensorMap;
		if (input)
		{
			bool execStatus = false;
			CriticalSection.Lock();
			if (snpe.get() != nullptr)
				execStatus = snpe->execute(input.get(), outputTensorMap);
			CriticalSection.Unlock();
			if (execStatus)
			{
				updateOutputInfo(outputTensorMap, batchsize);
			}
		}
    }

    outputsDimensions.Empty();
    for (auto& output : OutputsDimensions)
    {
        FString rank;
        for (int i = output.Value.Num() - 1; i >= 0; --i)
        {
            if (rank.IsEmpty())
            {
                rank = FString::FromInt((int)output.Value[i]);
            }
            else
            {
                rank += (","+FString::FromInt((int)output.Value[i]));
            }
        }
        outputsDimensions.Add(output.Key,rank);
    }
    return IsValid();
}

std::unique_ptr<zdl::DlSystem::ITensor> FSNPEWinObject::loadInputTensor(std::unique_ptr<zdl::SNPE::SNPE>& _snpe, const TArray<TArray<float>>& inputs)
{
    std::unique_ptr<zdl::DlSystem::ITensor> input;
    const auto& strList_opt = _snpe->getInputTensorNames();
    checkf(strList_opt, TEXT("Error obtaining Input tensor names"));
    const auto& strList = *strList_opt;
    // Make sure the network requires only a single input
    check(strList.size() == 1);

    /* Create an input tensor that is correctly sized to hold the input of the network. Dimensions that have no fixed size will be represented with a value of 0. */
    const auto& inputDims_opt = _snpe->getInputDimensions(strList.at(0));
    const auto& inputShape = *inputDims_opt;

    // If the network has a single input, each line represents the input file to be loaded for that input
    std::vector<float> inputVec;
    for (size_t i = 0; i < inputs.Num(); i++) {
        //Validate Input dimensions
        int DataSize = 0;
        for (int k = 1; k < inputShape.rank(); ++k)
        {
            if (k == 1)
                DataSize = inputShape[k];
            else
                DataSize *= inputShape[k];
        }
        check(DataSize != 0 && inputs[i].Num() == DataSize);
        for (const float& item : inputs[i])
        {
            inputVec.push_back(item);
        }
    }

    /* Calculate the total number of elements that can be stored in the tensor so that we can check that the input contains the expected number of elements.
        With the input dimensions computed create a tensor to convey the input into the network. */
    input = zdl::SNPE::SNPEFactory::getTensorFactory().createTensor(inputShape);

    if (input->getSize() != inputVec.size()) {
        UE_LOG(LogTemp, Error, TEXT("Size of input does not match network.\nExpecting: %d\nGot: %d\n"), input->getSize(), inputVec.size());
        return nullptr;
    }

    /* Copy the loaded input file contents into the networks input tensor. SNPE's ITensor supports C++ STL functions like std::copy() */
    std::copy(inputVec.begin(), inputVec.end(), input->begin());
    return input;
}

// Print the results to raw files
// ITensor
bool FSNPEWinObject::getOutput(zdl::DlSystem::TensorMap outputTensorMap,
    TMap<FString, TArray<TArray<float>>>& output,
    TMap<FString, TArray<uint8>>& outputsDimensions,
    size_t batchSize)
{
    // Get all output tensor names from the network
    zdl::DlSystem::StringList tensorNames = outputTensorMap.getTensorNames();

    // Iterate through the output Tensor map, and print each output layer name to a raw file
    for (auto& name : tensorNames)
    {
        TArray<TArray<float>> values;
        output.Add(name, values);
        TArray<uint8> dimensions;
        outputsDimensions.Add(name,dimensions);
        // Split the batched output tensor and save the results
        for (size_t i = 0; i < batchSize; i++) {
            auto tensorPtr = outputTensorMap.getTensor(name);
            //UE_LOG(LogTemp, Log, TEXT("Output %s total size: %d"), ANSI_TO_TCHAR(name), (int)tensorPtr->getSize());
            size_t batchChunk = tensorPtr->getSize() / batchSize;
            //UE_LOG(LogTemp, Log, TEXT("chunk size: %d"),(int)batchChunk);
            if (!getITensorBatched(output[name], outputsDimensions[name], tensorPtr, i, batchChunk))
            {
                UE_LOG(LogTemp, Error, TEXT("Error Getting ITensor batched"));
                return false;
            }
        }

        //FString data = "Model Result: \n";
        //for (auto& arr : output[name])
        //{
        //	for (float& value : arr)
        //	{
        //		data += FString::SanitizeFloat(value, 1) + ", ";
        //	}
        //	data += "\n";
        //}
        //const TCHAR* char_str = *data;
        //UE_LOG(LogTemp, Log, TEXT("%s"), char_str);
    }


    return true;
}

bool FSNPEWinObject::getITensorBatched(TArray<TArray<float>>& out, TArray<uint8>& dimensions, const zdl::DlSystem::ITensor* tensor, size_t batchIndex, size_t batchChunk)
{
    if (batchChunk == 0)
        batchChunk = tensor->getSize();

    TArray<float> NewArray;

    int numDimensions = tensor->getShape().rank();
    int numValues = tensor->getShape()[numDimensions - 1];

    for (int i = 1; i < numDimensions; ++i)
    {
        dimensions.Add((uint8)tensor->getShape()[i]);
    }

    int currentValueIndex = 0;
    for (auto it = tensor->cbegin() + batchIndex * batchChunk; it != tensor->cbegin() + (batchIndex + 1) * batchChunk; ++it)
    {
        float f = *it;
        NewArray.Add(f);
    }
    out.Add(NewArray);


    return true;
}

bool FSNPEWinObject::loadUDOPackage(TArray<FString>& UdoPackagePath)
{
	for (const auto& u : UdoPackagePath)
	{
		if (false == zdl::SNPE::SNPEFactory::addOpPackage(std::string(TCHAR_TO_ANSI(*u))))
		{
            UE_LOG(LogTemp, Error, TEXT("Error while loading UDO package: %s"), *u);
			return false;
		}
	}
	return true;
}




void FSNPEWinObject::updateModelInputInfo(std::unique_ptr<zdl::SNPE::SNPE>& _snpe)
{
	const auto& strList_opt = _snpe->getInputTensorNames();
	checkf(strList_opt, TEXT("Error obtaining Input tensor names"));
	const auto& strList = *strList_opt;
	// Make sure the network requires only a single input
	check(strList.size() == 1);

	/* Create an input tensor that is correctly sized to hold the input of the network. Dimensions that have no fixed size will be represented with a value of 0. */
    zdl::DlSystem::TensorShape tensorShape = _snpe->getInputDimensions();
    InputsRank = (uint16)tensorShape.rank();
    InputDimensions.Empty();
	for (int k = InputsRank; k >= 0; --k)
	{
		InputDimensions.Add((uint16)tensorShape.getDimensions()[k]);
	}
}

void FSNPEWinObject::updateOutputInfo(const zdl::DlSystem::TensorMap& outputTensorMap, const size_t& batchSize)
{
	TMap<FString, TArray<TArray<float>>> output;
	getOutput(outputTensorMap,
		output,
		OutputsDimensions,
        batchSize);
    OutputInfoKnown = true;
}

#endif

