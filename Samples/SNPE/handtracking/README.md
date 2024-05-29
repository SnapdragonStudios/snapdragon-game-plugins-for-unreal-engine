# NNERuntimeSNPE Sample - HandTracking
Example project for **NNERuntimeSNPE** plugin.
## Setup
### Install NNERuntimeSNPE Plugin
- Copy the NNERuntimeSNPE plugin into ```\Plugins``` folder
``` bat
xcopy ..\..\..\Plugins\NNERuntimeSNPE\ .\Plugins\NNERuntimeSNPE\ /E
```
### Import Model
- Download the ```MediaPipeHandLandmarkDetector``` model from [Qualcomm® AI Hub](https://aihub.qualcomm.com/models/mediapipe_hand)
- Using the [Qualcomm® Neural Processing SDK](https://developer.qualcomm.com/software/qualcomm-neural-processing-sdk) tools - convert the downloaded ```tflite``` model to SNPE's ```dlc``` format.

```bash
./snpe-tflite-to-dlc --input_network mediapipe_hand-mediapipehandlandmarkdetector.tflite --input_dim 'image' 1,3,256,256 --out_node output_0 --out_node output_1 --out_node output_2
```

- Import the dlc file into ```/Plugins/HandTrackerSubsystem/NN-Model/```
