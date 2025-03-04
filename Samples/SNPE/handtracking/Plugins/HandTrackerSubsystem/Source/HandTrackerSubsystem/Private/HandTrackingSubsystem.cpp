//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "HandTrackingSubsystem.h"

// 
// Extract Pixels from RenderTarget into TArray
static void ExtractRawPixels(UTextureRenderTarget2D* renderTarget, TArray<FColor>& tmpColorData, TArray<float>& rawPixels)
{
	int width = renderTarget->GetSurfaceWidth();
	int height = renderTarget->GetSurfaceHeight();
	int numPixels = width * height;

	tmpColorData.SetNum(numPixels);
	rawPixels.SetNum(numPixels * 3);

	FTextureRenderTarget2DResource* textureResource = (FTextureRenderTarget2DResource*)renderTarget->GetResource();
	textureResource->ReadPixels(tmpColorData, FReadSurfaceDataFlags());

	// planar
	for (int i = 0; i < numPixels; i++)
	{
		rawPixels[3 * i + 0] = tmpColorData[i].R / 255.0f;
		rawPixels[3 * i + 1] = tmpColorData[i].G / 255.0f;
		rawPixels[3 * i + 2] = tmpColorData[i].B / 255.0f;
	}
}

// 
// 
void UHandTrackingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UNNEModelData* ModelData = LoadObject<UNNEModelData>(nullptr, TEXT("/HandTrackerSubsystem/NN-Model/mediapipe_hand-mediapipehandlandmarkdetector.mediapipe_hand-mediapipehandlandmarkdetector"));
	runtime = UE::NNE::GetRuntime<INNERuntimeCPU>(TEXT("NNERuntimeSNPE"));
	if (ensureMsgf(runtime.IsValid(), TEXT("Could not find requested NNE Runtime")) &&
		ensureMsgf(ModelData != nullptr, TEXT("Couldn't find Model")))
	{
		// load the model
		Model = runtime->CreateModelCPU(ModelData);
		// create model instance
		ModelInstance = Model->CreateModelInstanceCPU();
		// initialize
		ModelInstance->SetInputTensorShapes(ModelInstance->GetInputTensorShapes());

		// allocate image buffers
		// intermediate color data
		tmpColorData.SetNum(kImageSize * kImageSize);
		// planar format
		rawPixels.SetNum(kImageSize * kImageSize * 3);

		//setup input bindings
		InputBinding.SetNum(1);
		InputBinding[0] = { .Data = rawPixels.GetData(), .SizeInBytes = rawPixels.Num() * sizeof(float) };
		
		// allocate output buffers, setup output bindings
		const auto& OutputTensorDescs = ModelInstance->GetOutputTensorDescs();
		int NumOutputs = OutputTensorDescs.Num();
		outputs.SetNum(NumOutputs);
		OutputBinding.SetNum(NumOutputs);
		for (int OutputIndex = 0; OutputIndex < NumOutputs; ++OutputIndex)
		{
			const auto& OutputTensorName = OutputTensorDescs[OutputIndex].GetName();
			auto& OutputBuffer = outputs[OutputIndex];
			auto& Binding = OutputBinding[OutputIndex];

			if (OutputTensorName == "scores")
			{
				// accuracy
				accOutputIndex = OutputIndex;
				OutputBuffer.SetNum(1);
			}
			else if (OutputTensorName == "lr")
			{
				// handedness (left/right)
				OutputBuffer.SetNum(1);
			}
			else if (OutputTensorName == "landmarks")
			{
				// trackers xyz
				landmarkOutputIndex = OutputIndex;
				OutputBuffer.SetNum(kNumTrackers * 3);
			}

			Binding.Data = OutputBuffer.GetData();
			Binding.SizeInBytes = OutputBuffer.Num() * sizeof(float);
		}

		trackers.SetNum(kNumTrackers);
	}
}

// 
// 
void UHandTrackingSubsystem::Deinitialize()
{

}

// 
// 
void UHandTrackingSubsystem::Process(UTextureRenderTarget2D* renderTarget2D)
{
	if (renderTarget2D != nullptr)
	{
		ExtractRawPixels(renderTarget2D, tmpColorData, rawPixels);
		UE::NNE::IModelInstanceCPU::ERunSyncStatus result = ModelInstance->RunSync(InputBinding, OutputBinding);
		accuracy = outputs[accOutputIndex][0];
		for (int i = 0; i < kNumTrackers; i++)
		{
			float X = outputs[landmarkOutputIndex][i * 3 + 0];
			float Y = outputs[landmarkOutputIndex][i * 3 + 1];
			float Z = outputs[landmarkOutputIndex][i * 3 + 2];

			trackers[i].X = (Z);
			trackers[i].Y = (X - 0.5f);
			trackers[i].Z = -(Y - 0.5f);
		}
	}
}
