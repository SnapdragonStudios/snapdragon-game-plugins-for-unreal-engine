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
            //if (Target.WindowsPlatform.Architecture == UnrealArch.Arm64) //Uncomment this line and comment out the line below if you will use this plugin as an engine plugin
			if (Target.WindowsPlatform.Architecture == WindowsArchitecture.ARM64)
            {
                // Add the import library
                PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "aarch64-windows-msvc", "SNPE.lib"));

                // Delay-load the DLL, so we can load it from the right place first
                //PublicDelayLoadDLLs.Add("SNPE.dll");

                // Ensure that the DLL is staged along with the executable
                RuntimeDependencies.Add("$(BinaryOutputDir)/SNPEarm64.dll",Path.Combine(ModuleDirectory, "lib/aarch64-windows-msvc/SNPE.dll"));
                RuntimeDependencies.Add("$(BinaryOutputDir)/SnpeDspV66Stub.dll", Path.Combine(ModuleDirectory, "lib/aarch64-windows-msvc/SnpeDspV66Stub.dll"));
                RuntimeDependencies.Add("$(BinaryOutputDir)/SnpeHtpPrepare.dll", Path.Combine(ModuleDirectory, "lib/aarch64-windows-msvc/SnpeHtpPrepare.dll"));
                RuntimeDependencies.Add("$(BinaryOutputDir)/SnpeHtpV68Stub.dll", Path.Combine(ModuleDirectory, "lib/aarch64-windows-msvc/SnpeHtpV68Stub.dll"));
                RuntimeDependencies.Add("$(BinaryOutputDir)/msvcp140.dll", Path.Combine(ModuleDirectory, "lib/aarch64-windows-msvc/msvcp140.dll"));
                RuntimeDependencies.Add("$(BinaryOutputDir)/vcruntime140.dll", Path.Combine(ModuleDirectory, "lib/aarch64-windows-msvc/vcruntime140.dll"));
                RuntimeDependencies.Add("$(BinaryOutputDir)/libSnpeDspV66Skel.so", Path.Combine(ModuleDirectory, "lib/dsp/libSnpeDspV66Skel.so"));
                RuntimeDependencies.Add("$(BinaryOutputDir)/libSnpeHtpV68Skel.so", Path.Combine(ModuleDirectory, "lib/dsp/libSnpeHtpV68Skel.so"));

            }
            else
            {
                // Add the import library
                PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "x86_64-windows-msvc", "SNPE.lib"));

                // Delay-load the DLL, so we can load it from the right place first
                //PublicDelayLoadDLLs.Add("SNPE.dll");

                // Ensure that the DLL is staged along with the executable
                RuntimeDependencies.Add("$(BinaryOutputDir)/SNPE.dll", Path.Combine(ModuleDirectory, "lib/x86_64-windows-msvc/SNPE.dll"));
            }
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            string Path0 = Path.Combine(ModuleDirectory, "lib", "aarch64-android", "libc++_shared.so");
            string Path1 = Path.Combine(ModuleDirectory, "lib", "aarch64-android", "libSNPE.so");
            string Path2 = Path.Combine(ModuleDirectory, "lib", "aarch64-android", "libSnpeHtpPrepare.so");
            string Path3 = Path.Combine(ModuleDirectory, "lib", "aarch64-android", "libSnpeHtpV68Stub.so");
            string Path4 = Path.Combine(ModuleDirectory, "lib", "aarch64-android", "libSnpeHtpV69Stub.so");
            string Path5 = Path.Combine(ModuleDirectory, "lib", "aarch64-android", "libSnpeHtpV73Stub.so");
            //string Path6 = Path.Combine(ModuleDirectory, "lib", "Android", "aarch64-android-clang8.0", "libsnpe_dsp_domains_v2.so");
            //string Path7 = Path.Combine(ModuleDirectory, "lib", "Android", "aarch64-android-clang8.0", "libSnpeHtpPrepare.so");
            //string Path8 = Path.Combine(ModuleDirectory, "lib", "Android", "aarch64-android-clang8.0", "libSnpeHtpV68Stub.so");
            //string Path9 = Path.Combine(ModuleDirectory, "lib", "Android", "aarch64-android-clang8.0", "libSnpeHtpV69Stub.so");
            //string Path10 = Path.Combine(ModuleDirectory, "lib", "Android", "aarch64-android-clang8.0", "libSnpeHtpV73Stub.so");
            string PathDSP1 = Path.Combine(ModuleDirectory, "lib", "dsp", "libcalculator_skel.so");
            string PathDSP2 = Path.Combine(ModuleDirectory, "lib", "dsp", "libSnpeDspV65Skel.so");
            string PathDSP3 = Path.Combine(ModuleDirectory, "lib", "dsp", "libSnpeDspV66Skel.so");
            string PathDSP4 = Path.Combine(ModuleDirectory, "lib", "dsp", "libSnpeHtpV68Skel.so");
            string PathDSP5 = Path.Combine(ModuleDirectory, "lib", "dsp", "libSnpeHtpV69Skel.so");
            string PathDSP6 = Path.Combine(ModuleDirectory, "lib", "dsp", "libSnpeHtpV73Skel.so");

            PublicAdditionalLibraries.AddRange(new string[] { Path0, Path1, Path2, Path3, Path4, Path5, /*Path6, Path7, Path8, Path9, Path10*/});

            RuntimeDependencies.Add(Path0);
            RuntimeDependencies.Add(Path1);
            RuntimeDependencies.Add(Path2);
            RuntimeDependencies.Add(Path3);
            RuntimeDependencies.Add(Path4);
            RuntimeDependencies.Add(Path5);
            //RuntimeDependencies.Add(Path6);
            //RuntimeDependencies.Add(Path7);
            //RuntimeDependencies.Add(Path10);
            RuntimeDependencies.Add(PathDSP1, StagedFileType.SystemNonUFS);
            RuntimeDependencies.Add(PathDSP2, StagedFileType.SystemNonUFS);
            RuntimeDependencies.Add(PathDSP3, StagedFileType.SystemNonUFS);
            RuntimeDependencies.Add(PathDSP4, StagedFileType.SystemNonUFS);
            RuntimeDependencies.Add(PathDSP5, StagedFileType.SystemNonUFS);
            RuntimeDependencies.Add(PathDSP6, StagedFileType.SystemNonUFS);

            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "SNPEPropertiesForReceipt.xml"));

        }
    }
}
