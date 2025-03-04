# Qualcomm™ NPE Plugin Sample - HandTracking
Example project for **Qualcomm™ NPE Plugin** plugin.
## Setup
### Install Qualcomm™ NPE Plugin

1. Run the `SNPELibrarySetup.bat` batch file to set up the `plugin` folder - see [the plugin's README](../../../Plugins/SNPE/README.md)
2. Copy the plugin into ```.\Plugins``` folder
``` bat
xcopy ..\..\..\Plugins\SNPE\ .\Plugins\SNPE\ /E
```
### Import Model
1. Download the ```MediaPipeHandLandmarkDetector``` model from [Qualcomm® AI Hub](https://aihub.qualcomm.com/models/mediapipe_hand)
2. Using the [Qualcomm® Neural Processing SDK](https://developer.qualcomm.com/software/qualcomm-neural-processing-sdk) tools - convert the downloaded ```tflite``` model to SNPE's ```dlc``` format.

```bash
PATH_TO_SNPE_CONVERTER/snpe-tflite-to-dlc --input_network PATH_TO_MODEL/mediapipe_hand-mediapipehandlandmarkdetector.tflite --input_dim 'image' 1,256,256,3 --out_node scores --out_node lr --out_node landmarks
```

3. Import the dlc file into ```/Plugins/HandTrackerSubsystem/NN-Model/```
