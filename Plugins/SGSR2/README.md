# SGSR2.0 UE Plugin
UE Plugin for Snapdragon Game Super Resolution 2.0

## Build SGSR2.0 in UE5.2
This release contains three versions of SGSR2 (3pass, 3passNoAlpha, 2passNoAlpha).<br/>
*1) Push subfolder "GSR" of any version of SGSR2 into folder "Plugins" of UE5 engine source code(Engine\Plugins\Marketplace), or project plugin folder. <br/>
*2) Build the engine or project.<br/>


## Enable SGSR2.0 in UE5.2
In UE5 Editor:
```
In "Project Settings" tab, apply following configs
	"Engine - Rendering" -> "Mobile" -> "Supports desktop Gen4 TAA on mobile": enable
	"Engine - Rendering" -> "Mobile" -> "Mobile Anti-Aliasing Method": TAA
	"Engine - Rendering" -> "Default Settings" -> "Temporal Upsampling": enable
	"Platforms - Android" -> "APK Packaging" -> "Package game data inside .apk": enable
	"Platforms - Android" -> "Build" -> "Support arm64": check
	"Platforms - Android" -> "Build" -> "Support OpenGL ES3.1": check
	"Platforms - Android" -> "Build" -> "Support Vulkan": check
	"Platforms - Android" -> "Platforms - Android SDK" -> "SDKConfig" -> "SDK API Level": latest
	"Platforms - Android" -> "Platforms - Android SDK" -> "SDKConfig" -> "NDK API Level": latest
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
r.Mobile.AntiAliasing=2 \\set TAA mode.
r.TemporalAA.Upsampling=1
```
For instance, upscale from 720p to 1080p:
```
r.MobileContentScaleFactor=0,r.Mobile.AntiAliasing=2,r.TemporalAA.Upsampling=1,r.SGSR2.Enabled=1,r.SGSR2.Quality=1
```


## Build Andriod for UE5.2 
To build Android in UE5.2:
```
1. Install Android API 32, Android SDK Build-Tools 30.0.3, Android NDK 25.1.8937393, latest command line tools.
2. Apply patch, refer to UE5.2.patch
 ```
