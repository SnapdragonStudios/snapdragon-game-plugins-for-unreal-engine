# Qualcomm™ NPE Plugin 

Plugin for **Neural Network Inference** using the *Qualcomm Neural Processing SDK* (also known as *SNPE*) to be used with Unreal Engine's *Neural Network Engine* (*NNE*) inference framework.<br>

This plugin enables hardware acceleration of AI model inference on devices with Qualcomm® Hexagon™ Processors.

Developed and Tested with **Unreal Engine 5.4**.

## Supported Platforms & Hardware

| Platform | Support |
| --- | --- |
|Android|Yes|
|Windows x86_64|Editor Only (CPU inference)|
|Windows arm64|Yes|

**CPU** inference can be enabled via a boolean CVar, `snpe.CpuFallback`.

## Getting Started

### Plugin setup

1. **Clone this repo** and follow the standard steps given by Unreal Engine to add this plugin as an **engine plugin** or **project plugin**.<br>

1. **Download the [Qualcomm Neural Processing SDK](https://developer.qualcomm.com/software/qualcomm-neural-processing-sdk)**, also known as the *SNPE SDK* (currently tested with version 2.30).<br>

1. **`SNPELibrarySetup.bat`** is provided to copy the necessary include and library files from the downloaded SNPE SDK to the plugin. Use it as follows:
```console
>    cd <PATH_TO_THIS_REPO>\plugin\Source\ThirdParty\SNPELibrary
>    .\SNPELibrarySetup.bat C:\Qualcomm\AIStack\SNPE\2.30.0.250109
```
### Usage in your Unreal project

1. Drag-and-drop the model files (`.dlc` format) into your Unreal project's Content Browser so that they are automatically converted into `UNNEModelData` data assets.
    - Use the downloaded SNPE SDK to create DLC files from a variety of neural network file formats.<br>
    Documentation for this step is provided in the [SNPE SDK Reference Guide](https://docs.qualcomm.com/bundle/publicresource/topics/80-63442-2/introduction.html).
    - *This plugin assumes that all DLC models have **fixed input dimensions** (which you define while creating DLC models with the SNPE SDK).*

1. Use this NNE runtime to load the data assets and perform inference via the common NNE interfaces (see this [Quick Start Guide](https://dev.epicgames.com/community/learning/tutorials/34q9/unreal-engine-nne-quick-start-guide-5-3) for an example using Unreal Engine 5.3).
    - We implement `INNERuntimeCPU` and use `NNERuntimeSNPE` as the runtime name. So, for example, the runtime can be retrieved by:
    ```cpp
    TWeakInterfacePtr<INNERuntimeCPU> Runtime = UE::NNE::GetRuntime<INNERuntimeCPU>(TEXT("NNERuntimeSNPE"));
    ```
