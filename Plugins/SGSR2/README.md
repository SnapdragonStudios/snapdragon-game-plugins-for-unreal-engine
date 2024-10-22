# SGSR2 UE Plugin
UE Plugin for Snapdragon Game Super Resolution 2

## Build SGSR2 in UE4.27
This release contains three versions of SGSR2 (3pass, 3passNoAlpha, 2passNoAlpha).<br/>
*1) Push subfolder "GSR" of any version of SGSR2 into folder "Plugins" of UE4 engine source code(Engine\Plugins\Runtime\Qualcomm), or project plugin folder. <br/>
*2) Build the engine or project.<br/>


## Enable SGSR2 in UE4.27
In UE4 Editor:
```
In "Project Settings" tab, apply following configs
	"Engine - Rendering" -> "Mobile" -> "Supports desktop Gen4 TAA on mobile": enable
	"Engine - Rendering" -> "Default Settings" -> "Temporal Upsampling": enable
	"Engine - Rendering" -> "Default Settings" -> "Anti-Aliasing Method": TemporalAA
	"Platforms - Android" -> "APK Packaging" -> "Package game data inside .apk": enable
	"Platforms - Android" -> "APK Packaging" -> "Minimum SDK Version": 19
	"Platforms - Android" -> "APK Packaging" -> "Target SDK Version": 28
	"Platforms - Android" -> "Build" -> "Support arm64": check
	"Platforms - Android" -> "Build" -> "Support OpenGL ES3.1": check
	"Platforms - Android" -> "Build" -> "Support Vulkan": check
	"Platforms - Android" -> "Build" -> "Advanced APK Packaging" -> "Extra Permissions": add two items:
		android.permission.READ_EXTERNAL_STORAGE
		android.permission.WRITE_EXTERNAL_STORAGE
	"Platforms - Android" -> "Platforms - Android SDK" -> "SDKConfig" -> "SDK API Level": android-30
	"Platforms - Android" -> "Platforms - Android SDK" -> "SDKConfig" -> "NDK API Level": android-21
In "Plugins" tab, enable SGSR2:
	"Installed" -> "Rendering" -> "SGSR2": Enabled
```
In Engine\Config\BaseDeviceProfiles.ini:
	set CVars=r.MobileContentScaleFactor=0.0 to [Android_Mid DeviceProfile] and [Android_High DeviceProfile]
	

In SGSR2 Settings Panel:
```
In "Project Settings" tab, open "Plugins - Snapdragon Game Super Resolution 2",
    General Settings:
        r.SGSR2.Enabled(refer to GSRViewExtension.cpp): enabled by default.
    Quality Setting:
        r.SGSR2.Quality(refer to GSRTU.cpp): Upscale quality mode. Default is 1(Quality: 1.5x).
            Available:
                0 - Ultra Quality 		1.25x
                1 - Quality 			1.5x 
                2 - Balanced 			1.7x 
                3 - Performance 		2.0x
            For example: device resoltion is 2400x1080, r.SGSR2.Quality=1, will upscale from 1600x720 to 2400x1080.
```

## Enable SGSR2 on mobile

Make sure SGSR2, TAA and temporal_upscale are enabled. If not, use the following commandline to enable.
```
r.SGSR2.Enabled=1       
r.DefaultFeature.AntiAliasing=2 \\set TAA mode.
r.TemporalAA.Upsampling=1
```
For instance, upscale from 720p to 1080p:
```
r.MobileContentScaleFactor=0,r.DefaultFeature.AntiAliasing=2,r.TemporalAA.Upsampling=1,r.SGSR2.Enabled=1,r.SGSR2.Quality=1
```


## Build Andriod for UE4.27 (if Android SDK > 33 special)
To build Android(target SDK: 34) in UE4.27:
```
1. Install Android API 33/34, Android SDK Build-Tools 30.0.3 (don't install any version higher than this, UE4 uses the latest installed version!), install Android NDK 21.4.7075529, install command line tools 11.
2. In UE settings:
    Platform-Android/APK Packaging/Target SDK Version: 34
    Platform-Android SDK/SDKConfig:
        SDK API Level=android-33
        NDK API Level=android-21
    Platforms-Android/Advanced APK Packaging/Extra Tags for UE4.GameActivity <activity> node:
        android:exported="true"
3. Apply patch, refer to UE4.27_AndroidSDK34.patch
 ```
