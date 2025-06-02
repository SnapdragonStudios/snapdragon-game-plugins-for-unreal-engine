# SGSR2.0 UE Plugin
UE Plugin for Snapdragon Game Super Resolution 2.0

## Build SGSR2.0 in UE5.5
This release contains two versions of SGSR2 (3pass, 2passNoAlpha).<br/>
*1) Push subfolder "GSR" of any version of SGSR2 into folder "Plugins" of UE5 engine source code(Engine\Plugins\Runtime\Qualcomm), or project plugin folder. <br/>
*2) Build the engine or project.<br/>


## Enable SGSR2.0 in UE5.5
In UE5 Editor:
```
In "Project Settings" tab, apply following configs
	"Engine - Rendering" -> "Mobile" -> "Mobile Anti-Aliasing Method"-> "Mobile Anti-Aliasing Method(TAA)"
	"Engine - Rendering" -> "Mobile" -> "Supports desktop Gen4 TAA on mobile": enable
	"Engine - Rendering" -> "Default Settings" -> "Temporal Upsampling": enable
	"Engine - Rendering" -> "Default Settings" -> "Anti-Aliasing Method": TemporalAA
	"Platforms - Android" -> "APK Packaging" -> "Package game data inside .apk": enable
	"Platforms - Android" -> "APK Packaging" -> "Minimum SDK Version": 26
	"Platforms - Android" -> "APK Packaging" -> "Target SDK Version": 33
	"Platforms - Android" -> "Build" -> "Support arm64": check
	"Platforms - Android" -> "Build" -> "Support OpenGL ES3.1": check
	"Platforms - Android" -> "Build" -> "Support Vulkan": check
	"Platforms - Android" -> "Build" -> "Advanced APK Packaging" -> "Extra Permissions": add two items:
		android.permission.READ_EXTERNAL_STORAGE
		android.permission.WRITE_EXTERNAL_STORAGE
	"Platforms - Android" -> "Platforms - Android SDK" -> "SDKConfig" -> "SDK API Level": android-33 (according to your needs)
	"Platforms - Android" -> "Platforms - Android SDK" -> "SDKConfig" -> "NDK API Level": android-25 (according to your needs)
In "Plugins" tab, enable SGSR2:
	"Installed" -> "Rendering" -> "SGSR2": Enabled
```
In Engine\Config\BaseDeviceProfiles.ini:
	set CVars=r.MobileContentScaleFactor=0.0 to [Android_Mid DeviceProfile] and [Android_High DeviceProfile]
	

In SGSR2.0 Settings Panel:
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

## Enable SGSR2.0 on mobile

Make sure SGSR2, TAA and temporal_upscale are enabled. If not, use the following commandline to enable.
```
r.SGSR2.Enabled=1       
r.DefaultFeature.AntiAliasing=2 \\set TAA mode.
r.TemporalAA.Upsampling=1
```
Push UECommandLine.txt to `/sdcard/Android/data/com.YourCompany.[PROJECT]/files/UnrealGame/[PROJECT]/` before app starts.

For instance, upscale from 720p to 1080p:
```
r.MobileContentScaleFactor=0,r.DefaultFeature.AntiAliasing=2,r.TemporalAA.Upsampling=1,r.SGSR2.Enabled=1,r.SGSR2.Quality=1
```

If storage permissions required, intall .apk through Install_[PROJECT]-arm64.bat or enter the following code:
```
adb shell pm grant com.YourCompany.[PROJECT] android.permission.READ_EXTERNAL_STORAGE
adb shell pm grant com.YourCompany.[PROJECT] android.permission.WRITE_EXTERNAL_STORAGE
```

## Build Andriod for UE5.5 (workable solution)
- Apply patch, refer to UE5.5.patch
- SDK: 33.0.3
- NDK: 25.1.8937393
- JRE: Java 17
- Gradle: `com.android.tools.build:gradle:7.2.0`, `gradle-7.6.4-bin.zip`
- If Android SDK = 35 ，use sdk/platforms/android-35/android.jar from other android version

