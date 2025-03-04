//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
using System.IO;
using UnrealBuildTool;

public class SNPELibrary : ModuleRules
{
    public SNPELibrary(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
        bUseRTTI = true;
        bEnableExceptions = true;

        // Add any macros that need to be set
        PublicDefinitions.Add("WITH_SNPELIBRARY=1");

        // Add any include paths for the plugin
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "inc"));

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string ArchDir = Target.WindowsPlatform.Architecture.ToString();
            if (Target.WindowsPlatform.Architecture == UnrealArch.Arm64)
            {

                // Add the import library
                PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "Arm64", "SNPE.lib"));

                // Delay-load the DLL, so we can load it from the right place first
                PublicDelayLoadDLLs.Add("SNPE.dll");

                // Ensure that the DLL is staged along with the executable
                RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/SNPE/Arm64/SNPE.dll");
                RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/SNPE/Arm64/SnpeHtpPrepare.dll");
                RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/SNPE/Arm64/SnpeHtpV68Stub.dll");
                RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/SNPE/Arm64/SnpeHtpV73Stub.dll");
                RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/SNPE/Arm64/libSnpeHtpV68Skel.so");
                RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/SNPE/Arm64/libSnpeHtpV73Skel.so");
            }
            else//if (Target.WindowsPlatform.Architecture == UnrealArch.X64)
            {
                // Add the import library
                PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "x64", "SNPE.lib"));

                // Delay-load the DLL, so we can load it from the right place first
                PublicDelayLoadDLLs.Add("SNPE.dll");

                // Ensure that the DLL is staged along with the executable
                RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/SNPE/x64/SNPE.dll");
            }
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            string LibSNPEPath = Path.Combine(ModuleDirectory, "lib", "aarch64-android", "libSNPE.so");
            PublicAdditionalLibraries.Add(LibSNPEPath);

            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "SNPEPropertiesForReceipt.xml"));

        }
    }
}