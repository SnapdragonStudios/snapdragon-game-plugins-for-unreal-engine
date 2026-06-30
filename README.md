# SGSR UE Plugin
UE Plugin for Snapdragon Game Super Resolution, supporting UE 5.0-5.8

## Build SGSR in UE
This release contains 3 methods of SGSR:<br/>
- Spatial Upscaler
- Temporal Upscaler 
  - 2 pass fragment shader
  - 3 pass compute shader
1) Push folder "SGSR" into folder "Plugins" of UE5 engine source code(Engine\Plugins\Runtime\Qualcomm), or project plugin folder.
2) Apply the patch under folder "Patches" to UE5 engine source code if needed. Patches are needed for:
   - UE5.3
   - UE5.4
   - UE5.5
3) Build the engine or project.


## Enable SGSR in UE
In UE5 Editor:
```
In "Plugins" tab, enable SGSR:
	"Installed" -> "Rendering" -> "SGSR": Enabled
In "Project Settings" tab, open "Plugins - Snapdragon Game Super Resolution",
    General Settings:
        r.SGSR.Enabled(refer to SGSRTUViewExtension.cpp): enabled by default.
```
By command line(on fly):
```
r.SGSR.Enabled 1
```
Enable SGSR Spatial Upscaling: <br/>
```
Editor:
In "Project Settings" tab, open "Plugins - Snapdragon Game Super Resolution",
    General Settings:
        Upscaling Method: "Spatial Upscaling".

Command line:
	r.SGSR.Method 0
```
Enable SGSR Temporal Upscaling: <br/>

```
Editor:
In "Project Settings" tab, apply following configs
	"Engine - Rendering" -> "Mobile" -> "Mobile Anti-Aliasing Method"-> "Mobile Anti-Aliasing Method(TAA)"
	"Engine - Rendering" -> "Mobile" -> "Supports desktop Gen4 TAA on mobile": enable
	"Engine - Rendering" -> "Default Settings" -> "Temporal Upsampling": enable
	"Engine - Rendering" -> "Default Settings" -> "Anti-Aliasing Method": TemporalAA

Command line:
	2 pass fragment shader: r.SGSR.Method 1
	3 pass compute shader: r.SGSR.Method 2
```
In Engine\Config\BaseDeviceProfiles.ini:
	set CVars=r.MobileContentScaleFactor=0.0 to [Android_Mid DeviceProfile] and [Android_High DeviceProfile]
	

In SGSR Settings Panel:
```
In "Project Settings" tab, open "Plugins - Snapdragon Game Super Resolution",
General Quality Setting:
        r.SGSR.Quality(refer to SGSRTU.cpp): Quality mode. Default is 1(Quality: 1.5x).
            Available:
                0 - Ultra Quality 		1.25x  ScreenPercentage 80%
                1 - Quality 			1.5x ScreenPercentage 66.7%
                2 - Balanced 			1.7x ScreenPercentage 58.8%
                3 - Performance 		2.0x ScreenPercentage 50%
                4 - Custom              Input custom screen percentage to override default setting.
        r.SGSR.CustomScreenPercentage(refer to SGSRTU.cpp): Custom screen percentage value when r.SGSR.Quality=4. Range: [50.0, 100.0]. Default is 100.
            For example: device resoltion is 2400x1080, r.SGSR.Quality=1, will upscale from 1600x720 to 2400x1080.

	SU Setting:
		r.SGSR.Target(refer to SGSRSubpassScaler.cpp): Spatial Upscale target, each target is a different shader.
			Available:
				0 - Mobile
				1 - High Quality
				2 - VR
	
```

## Enable SGSR on mobile
Run project on mobile:
```
In "Project Settings" tab, apply following configs
	"Platforms - Android" -> "APK Packaging" -> "Package game data inside .apk": enable
	"Platforms - Android" -> "Build" -> "Support arm64": check
	"Platforms - Android" -> "Build" -> "Support OpenGL ES3.1": check
	"Platforms - Android" -> "Build" -> "Support Vulkan": check
	"Platforms - Android" -> "Build" -> "Advanced APK Packaging" -> "Extra Permissions": add two items:
		android.permission.READ_EXTERNAL_STORAGE
		android.permission.WRITE_EXTERNAL_STORAGE
```

Make sure SGSR is enabled. If not, use the following commandline to enable.
```
r.SGSR.Enabled=1       
```
Select desired upscaling method(switching bewteen SU and TU will automatically set corresponding AA methods, no need to set it mannually):
```
r.SGSR.Method=1
```
If TAA is not set correctly for TU, then use:
```
r.AntiAliasingMethod=2,r.Mobile.AntiAliasing=2
```
Push UECommandLine.txt to `/sdcard/Android/data/com.YourCompany.[PROJECT]/files/UnrealGame/[PROJECT]/` before app starts.

For instance, using TU 3pass upscale from 720p to 1080p:
```
r.MobileContentScaleFactor=0,r.SGSR.Enabled=1,r.SGSR.Method=3,r.SGSR.Quality=1
```

If storage permissions required, intall .apk through Install_[PROJECT]-arm64.bat or enter the following code:
```
adb shell pm grant com.YourCompany.[PROJECT] android.permission.READ_EXTERNAL_STORAGE
adb shell pm grant com.YourCompany.[PROJECT] android.permission.WRITE_EXTERNAL_STORAGE
```

## Build Android for UE(workable solution)
- Apply patch to engine source code (if needed)
- SDK: 
  - 5.0-5.2: Android SDK 32
  - 5.3-5.5: Android SDK 33
  - 5.6+: Android SDK 34
- Android SDK Command-line Tools: 8.0
- NDK:
  - 5.0: 21.4.7075529
  - 5.1: 25.2.9519653
  - 5.2-5.6: 25.1.8937393
  - 5.7,5.8: 27.2.12479018
- JRE:
  - 5.0-5.2: Java 1.8.0_242
  - 5.3-5.6: Java 17
  - 5.7,5.8: Java 21
## Settings

### General

| Variant  | Console Variable        | Default Value | Value Range     | Details |
|----------|--------------------------|---------------|------------------|---------|
|     SU & TU  | `r.SGSR.Enabled`        | 1             | 0,1              | Enable / disable GSR. |
|          | `r.SGSR.Method`          | 0             | 0,1,2            | Choose which variant to use. **0 = SU**, **1 = TU 2pass-fs**, **2 = TU 3pass-cs.** |
|          | `r.SGSR.Quality`        | 1             | 0,1,2,3,4              | Choose quality: 0 = Ultra Quality, 1 = Quality, 2 = Balanced, 3 = Performance, 4 = Custom |
|          | `r.SGSR.CustomScreenPercentage`        | 100.0             | [50.0, 100.0]              | Custom screen percentage value when r.SGSR.Quality=4 |
|          | `r.SGSR.HalfPrecision`  | 1             | 0,1              | Enable Half Precision shader arithmetic (platform dependent). May improve performance. Requires enabling FP16 (`bSupportsRealTypes=RuntimeGuaranteed` in `Engine/Config/Android/DataDrivenPlatformInfo.ini`). |

---

### Spatial Upscaler

| Variant  | Console Variable        | Default Value | Value Range     | Details |
|----------|--------------------------|---------------|------------------|---------|
|     SU   | `r.SGSR.Target`        | 0             | 0,2              | Choose target: 0 = Mobile, 1 = High Quality, 2 = VR |

---

### Temporal Upscaler 
- **2pass-fs**

| Variant  | Console Variable        | Default Value | Value Range     | Details |
|----------|--------------------------|---------------|------------------|---------|
| 2pass-fs | `r.SGSR.5Sample`        | 1             | 0,1              | Controls sample number: 0 = 9 samples (better quality), 1 = 5 samples (better performance, default). |
|          | `r.SGSR.LanczosOpt`     | 0             | 0,1              | Use improved Lanczos sampler to reduce flicker. |
|          | `r.SGSR.ThinFeature`    | 0             | 0,1              | Detect and preserve thin features to suppress flicker. |

---

- **3pass-cs**

| Variant  | Console Variable        | Default Value | Value Range     | Details |
|----------|--------------------------|---------------|------------------|---------|
| 3pass-cs | `r.SGSR.5Sample`        | 1             | 0,1              | Controls sample number: 0 = 9 samples (better quality), 1 = 5 samples (better performance, default). |
|          | `r.SGSR.DoSharpening`   | 0             | 0,1              | Add a sharpening pass. |
|          | `r.SGSR.Sharpness`      | 1.12          | [0.0, 1.3]            | Adjust sharpening strength. |
|          | `r.SGSR.PixelLock`      | 0             | 0,1              | Detect and preserve thin features. |