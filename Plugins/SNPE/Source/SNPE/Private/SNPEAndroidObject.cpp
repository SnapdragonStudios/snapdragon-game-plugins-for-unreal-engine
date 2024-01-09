//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SNPEAndroidObject.h"

#if PLATFORM_ANDROID

#include "DlSystem/DlEnums.hpp"
#include "DlSystem/PlatformConfig.hpp"
#include "DlSystem/ITensor.hpp"
#include "DlSystem/ITensorFactory.hpp"
#include "DlSystem/TensorMap.hpp"

#include "DlContainer/IDlContainer.hpp"
#include "SNPE/SNPEFactory.hpp"
#include "SNPE/SNPE.hpp"

#include "SNPE/SNPEBuilder.hpp"
#include "DlSystem/String.hpp"
#include "DlSystem/StringList.hpp"
#include "DlSystem/DlVersion.hpp"
#include "DlSystem/TensorShape.hpp"

#include <Runtime/Core/Public/Android/AndroidPlatform.h>
#include <Runtime/Core/Public/Android/AndroidPlatformFile.h>
#include <Runtime/Core/Public/Android/AndroidPlatformProcess.h>
#include <Runtime/Core/Public/HAL/PlatformFileManager.h>
#include "GenericPlatform/GenericPlatformMisc.h"
#if USE_ANDROID_JNI
#include "Android/AndroidJavaEnv.h"
#endif
#include <stdlib.h>
#include <fstream>

ISNPEObject* ISNPEObject::Create(const FString& Model, bool usintInitCaching, RuntimeMode runtime, const TArray<FString>& UDOs) { return new FSNPEAndroidObject(Model, usintInitCaching, runtime, UDOs); }
void ISNPEObject::Free(TUniquePtr<ISNPEObject>& p) { ISNPEObject* ptr = p.Release(); }

//TODO-CC NOTES ON OBB EXTRACTION IMPLEMENTATION
extern FString GOBBMainFilePath;

FSNPEAndroidObject::FSNPEAndroidObject(const FString& Model, bool usintInitCaching, RuntimeMode runtime, const TArray<FString>& UDOs)
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
    ModelLocation = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	dlc = ModelLocation + Model;
	dlc = FPlatformFileManager::Get().GetPlatformFile().ConvertToAbsolutePathForExternalAppForRead(*dlc);
	if (!FPaths::FileExists(dlc))
	{
        //dlc = "/sdcard/" + Model;
        //TODO-CC NOTES ON OBB EXTRACTION IMPLEMENTATION
		FString PathToModel = FPaths::ProjectContentDir() + Model;
		do {
			PathToModel.RightChopInline(3, false);
		} while (PathToModel.StartsWith(TEXT("../"), ESearchCase::CaseSensitive));
		FString ModelOBB = GOBBMainFilePath / PathToModel;
		dlc = ModelOBB;
	}
	UsingInitCaching = usintInitCaching;
	Valid = Initialize();
}

FString FSNPEAndroidObject::GetNativeLibraryDir()
{
	FString Result = "";
#if USE_ANDROID_JNI
	JNIEnv* JEnv = AndroidJavaEnv::GetJavaEnv();
	if (nullptr != JEnv)
	{
		jobject gameactivity = AndroidJavaEnv::GetGameActivityThis();
		FString packageName = FAndroidPlatformProcess::GetGameBundleId();
		jclass Class = JEnv->GetObjectClass(gameactivity);
		if (nullptr != Class)
		{
			jmethodID getPackageManager = JEnv->GetMethodID(Class, "getPackageManager", "()Landroid/content/pm/PackageManager;");
			jobject instantiatePackageManager = JEnv->CallObjectMethod(gameactivity, getPackageManager);
			if (instantiatePackageManager != nullptr)
			{
				jclass packageManagerClass = JEnv->GetObjectClass(instantiatePackageManager);
				if (packageManagerClass != nullptr)
				{
					jmethodID getAppInfoMethod = JEnv->GetMethodID(packageManagerClass, "getApplicationInfo", "(Ljava/lang/String;I)Landroid/content/pm/ApplicationInfo;");
					jobject appInfo = JEnv->CallObjectMethod(instantiatePackageManager, getAppInfoMethod, JEnv->NewStringUTF(TCHAR_TO_UTF8(*packageName)), 0);
					if (appInfo != nullptr)
					{
						jclass applicationInfoClass = JEnv->GetObjectClass(appInfo);
						if (applicationInfoClass != nullptr)
						{
							jfieldID nativeLibraryDir = JEnv->GetFieldID(applicationInfoClass, "nativeLibraryDir", "Ljava/lang/String;");
							auto javastring = (jstring)JEnv->GetObjectField(appInfo, nativeLibraryDir);
							const auto chars = JEnv->GetStringUTFChars(javastring, 0);
                            Result = FString(UTF8_TO_TCHAR(chars));
						}
					}
				}
			}
		}
	}
#endif
    return Result;
}

bool FSNPEAndroidObject::Initialize()
{
	UE_LOG(LogAndroid, Log, TEXT("SNPE: ADSP_LIBRARY_PATH=%s"), ANSI_TO_TCHAR(getenv("ADSP_LIBRARY_PATH")));

	//Check runtime
	UE_LOG(LogAndroid, Log, TEXT("SNPE: CHECKING RUNTIME"));
	Runtime = checkRuntime(Runtime);

	// Load DLC
	UE_LOG(LogAndroid, Log, TEXT("SNPE: LOADING MODEL"));
    std::unique_ptr<zdl::DlContainer::IDlContainer> container = loadContainerFromFile(dlc);
	if (container == nullptr)
    {
		UE_LOG(LogAndroid, Error, TEXT("SNPE: Error while opening the container file."));
		return false;
    }

	//load UDO package
	if (loadUDOPackage(UDOPaths) == false)
	{
		UE_LOG(LogAndroid, Error, TEXT("Failed to load UDO Package(s)."));
		return false;
	}

	//Set builder
	UE_LOG(LogAndroid, Log, TEXT("SNPE: INITIALIZING SNPE BUILDER"));
	std::string PlatformOptions = "unsignedPD:ON";
    platformConfig.setPlatformOptions(PlatformOptions);
    check(platformConfig.isOptionsValid());
	snpe = setBuilderOptions(container, Runtime, runtimeList, false, platformConfig, UsingInitCaching);
    if (snpe == nullptr)
    {
		UE_LOG(LogAndroid, Error, TEXT("SNPE: Error while building SNPE object."));
		return false;
    }

	updateModelInputInfo(snpe);

	
	if (UsingInitCaching)
    {
		if (container->save(std::string(TCHAR_TO_UTF8(*dlc)).c_str()))
		{
			UE_LOG(LogAndroid, Log, TEXT("SNPE: Saved container into archive successfully"));
		}
		else
		{
			UE_LOG(LogAndroid, Log, TEXT("SNPE: Failed to save container into archive"));
		}
    }
	FString Platform = Runtime == zdl::DlSystem::Runtime_t::DSP? "DSP" : "CPU";
    UE_LOG(LogAndroid, Log, TEXT("SNPE: IS READY TO GO ON %s"), *Platform);
	return true;
}

zdl::DlSystem::Runtime_t FSNPEAndroidObject::checkRuntime(zdl::DlSystem::Runtime_t runtime)
{
    static zdl::DlSystem::Version_t Version = zdl::SNPE::SNPEFactory::getLibraryVersion();
	UE_LOG(LogAndroid, Log, TEXT("SNPE Version: %s"), ANSI_TO_TCHAR(Version.asString().c_str()));
    if (!zdl::SNPE::SNPEFactory::isRuntimeAvailable(runtime, zdl::DlSystem::RuntimeCheckOption_t::UNSIGNEDPD_CHECK))
    {
        UE_LOG(LogAndroid, Warning, TEXT("Selected runtime not present. Falling back to CPU."));
        runtime = zdl::DlSystem::Runtime_t::CPU;
    }
    return runtime;
}

std::unique_ptr<zdl::DlContainer::IDlContainer> FSNPEAndroidObject::loadContainerFromFile(const FString& containerPath)
{
    std::string ModelFile = std::string(TCHAR_TO_ANSI(*containerPath));
    UE_LOG(LogAndroid, Log, TEXT("SNPE: OPENING MODEL FILE: %s"), *containerPath);
    std::ifstream dlcFile(ModelFile);
	if (!dlcFile) {
        UE_LOG(LogAndroid, Error, TEXT("dlc file not valid."));
        return nullptr;
	}

	std::unique_ptr<zdl::DlContainer::IDlContainer> container_ptr;
    container_ptr = zdl::DlContainer::IDlContainer::open(ModelFile);
    if(!container_ptr.get())
    {
        UE_LOG(LogAndroid, Error, TEXT("SNPE: opening model failed at open funcition: %s"), ModelFile.c_str());
    }
    
    return container_ptr;
}

std::unique_ptr<zdl::SNPE::SNPE> FSNPEAndroidObject::setBuilderOptions(std::unique_ptr<zdl::DlContainer::IDlContainer> & container,
                                                   zdl::DlSystem::Runtime_t runtime,
                                                   zdl::DlSystem::RuntimeList runtime_List,
                                                   bool useUserSuppliedBuffers,
                                                   zdl::DlSystem::PlatformConfig platform_Config,
                                                   bool useCaching)
{
    std::unique_ptr<zdl::SNPE::SNPE> snpe_ptr;
    zdl::SNPE::SNPEBuilder snpeBuilder(container.get());

    if(runtime_List.empty())
    {
        runtime_List.add(runtime);
    }

    if (container.get() == nullptr)
    {
        UE_LOG(LogAndroid, Error, TEXT("SNPE: Container is not valid"));
    }
    
	snpe_ptr = snpeBuilder.setOutputLayers({})
		.setRuntimeProcessorOrder(runtime_List)
		.setUseUserSuppliedBuffers(useUserSuppliedBuffers)
		.setPlatformConfig(platform_Config)
		.setInitCacheMode(useCaching)
        .setSingleThreadedInit(true)
		.build();

    check(snpe_ptr.get() != nullptr);
    return snpe_ptr;
}

bool FSNPEAndroidObject::ProcessModel(const TArray<TArray<float>>& inputs, TMap<FString, TArray<TArray<float>>>& outputs, TMap<FString, TArray<uint8>>& outputsDimensions)
{
    //UE_LOG(LogAndroid, Log, TEXT("SNPE: PROCESS MODEL REQUESTED"));
    // Check the batch size for the container
    // SNPE 1.16.0 (and newer) assumes the first dimension of the tensor shape
    // is the batch size.
    zdl::DlSystem::TensorShape tensorShape;
    tensorShape = snpe->getInputDimensions();
    size_t batchSize = tensorShape.getDimensions()[0];
    //UE_LOG(LogAndroid, Log, TEXT("SNPE: INPUT DIMENSIONS: %d, %d"), tensorShape.getDimensions()[0], tensorShape.getDimensions()[1]);
	
	//Validate batch and input size
	if(batchSize != inputs.Num())
	{
		UE_LOG(LogAndroid, Error, TEXT("Wrong batch size of input batches, expected: %d, received: %d"), batchSize, inputs.Num());
		return false;
	}


	// A tensor map for SNPE execution outputs
	zdl::DlSystem::TensorMap outputTensorMap;

    std::unique_ptr<zdl::DlSystem::ITensor> inputTensor = loadInputTensor(snpe, inputs);
	if(!inputTensor)
	{
        UE_LOG(LogAndroid, Error, TEXT("Error loading Input Tensor"));
		return false;
	}
	
	// Execute the input tensor on the model with SNPE
	CriticalSection.Lock();
	bool execStatus = snpe->execute(inputTensor.get(), outputTensorMap);
	CriticalSection.Unlock();
	// Save the execution results if execution successful
	if (execStatus == true)
	{
	   if(getOutput(outputTensorMap, outputs, outputsDimensions, batchSize))
	   {
		   if (!OutputInfoKnown)
		   {
			   updateOutputInfo(outputTensorMap, batchSize);
		   }
		   return true;
	   }
       else
       {
           UE_LOG(LogAndroid, Error, TEXT("Error retrieving Output"));
       }
	}
	else
	{
		UE_LOG(LogAndroid, Error, TEXT("Error while executing the network."));
	}
    return false;
}

bool FSNPEAndroidObject::GetModelInfo(uint8& inputsRank, TArray<uint8>& inputDimensions, TMap<FString, FString>& outputsDimensions)
{
	inputsRank = InputsRank;
	inputDimensions = InputDimensions;

	// Execute the input tensor on the model with SNPE
	if (!OutputInfoKnown && snpe != nullptr)
	{
		zdl::DlSystem::TensorShape tensorShape = snpe->getInputDimensions();
		size_t batchsize = tensorShape.getDimensions()[0];
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
				rank += ("," + FString::FromInt((int)output.Value[i]));
			}
		}
		outputsDimensions.Add(output.Key, rank);
	}
	return IsValid();
}

std::unique_ptr<zdl::DlSystem::ITensor> FSNPEAndroidObject::loadInputTensor(std::unique_ptr<zdl::SNPE::SNPE>& _snpe, const TArray<TArray<float>>& inputs)
{
    std::unique_ptr<zdl::DlSystem::ITensor> input;
    const auto &strList_opt = _snpe->getInputTensorNames();
    checkf(strList_opt, TEXT("Error obtaining Input tensor names"));
    const auto &strList = *strList_opt;
    // Make sure the network requires only a single input
    check(strList.size() == 1);

	/* Create an input tensor that is correctly sized to hold the input of the network. Dimensions that have no fixed size will be represented with a value of 0. */
	const auto& inputDims_opt = _snpe->getInputDimensions(strList.at(0));
	const auto& inputShape = *inputDims_opt;

    // If the network has a single input, each line represents the input file to be loaded for that input
    std::vector<float> inputVec;
    for(size_t i=0; i<inputs.Num(); i++) {
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
		UE_LOG(LogAndroid, Error, TEXT("Size of input does not match network.\nExpecting: %d\nGot: %d\n"), input->getSize(),inputVec.size() );
        return nullptr;
    }

    /* Copy the loaded input file contents into the networks input tensor. SNPE's ITensor supports C++ STL functions like std::copy() */
    std::copy(inputVec.begin(), inputVec.end(), input->begin());
    return input;
}

// Print the results to raw files
// ITensor
bool FSNPEAndroidObject::getOutput(zdl::DlSystem::TensorMap outputTensorMap,
	TMap<FString, TArray<TArray<float>>>& output,
	TMap<FString, TArray<uint8>>& outputsDimensions,
	size_t batchSize)
{
    // Get all output tensor names from the network
    zdl::DlSystem::StringList tensorNames = outputTensorMap.getTensorNames();

    // Iterate through the output Tensor map, and print each output layer name to a raw file
    for( auto& name : tensorNames)
    {
		TArray<TArray<float>> values;
		output.Add(name, values);
		TArray<uint8> dimensions;
		outputsDimensions.Add(name, dimensions);
        // Split the batched output tensor and save the results
        for(size_t i=0; i<batchSize; i++) {
            auto tensorPtr = outputTensorMap.getTensor(name);
            //UE_LOG(LogAndroid, Log, TEXT("Output %s total size: %d"), ANSI_TO_TCHAR(name), (int)tensorPtr->getSize());
            size_t batchChunk = tensorPtr->getSize() / batchSize;
            //UE_LOG(LogAndroid, Log, TEXT("chunk size: %d"),(int)batchChunk);
            if(!getITensorBatched(output[name], outputsDimensions[name], tensorPtr, i, batchChunk))
            {
                UE_LOG(LogAndroid, Error, TEXT("Error Getting ITensor batched"));
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
		//UE_LOG(LogAndroid, Log, TEXT("%s"), char_str);
    }


    return true;
}

bool FSNPEAndroidObject::getITensorBatched(TArray<TArray<float>>& out, TArray<uint8>& dimensions, const zdl::DlSystem::ITensor* tensor, size_t batchIndex, size_t batchChunk)
{
	if(batchChunk == 0)
		batchChunk = tensor->getSize();	  
	
	TArray<float> NewArray;
 
	int numDimensions = tensor->getShape().rank();
	int numValues = tensor->getShape()[numDimensions - 1];

	for (int i = 1; i < numDimensions; ++i)
	{
		dimensions.Add((uint16)tensor->getShape()[i]);
	}
	int currentValueIndex = 0;
	for ( auto it = tensor->cbegin() + batchIndex * batchChunk; it != tensor->cbegin() + (batchIndex+1) * batchChunk; ++it )
	{
		float f = *it;
		NewArray.Add(f);	  
	}
	out.Add(NewArray);


	return true;
}

bool FSNPEAndroidObject::loadUDOPackage(TArray<FString>& UdoPackagePath)
{
	for (const auto& u : UdoPackagePath)
	{
		if (false == zdl::SNPE::SNPEFactory::addOpPackage(std::string(TCHAR_TO_ANSI(*u))))
		{
			UE_LOG(LogAndroid, Error, TEXT("Error while loading UDO package: %s"), *u);
			return false;
		}
	}
	return true;
}


void FSNPEAndroidObject::updateModelInputInfo(std::unique_ptr<zdl::SNPE::SNPE>& _snpe)
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

void FSNPEAndroidObject::updateOutputInfo(const zdl::DlSystem::TensorMap& outputTensorMap, const size_t& batchSize)
{
	TMap<FString, TArray<TArray<float>>> output;
	getOutput(outputTensorMap,
		output,
		OutputsDimensions,
		batchSize);
	OutputInfoKnown = true;
}

#endif