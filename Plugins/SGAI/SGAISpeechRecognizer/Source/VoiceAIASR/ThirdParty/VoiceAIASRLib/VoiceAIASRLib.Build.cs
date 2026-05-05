// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

using System.Collections.Generic;
using System.IO;
using UnrealBuildTool;

public class VoiceAIASRLib : ModuleRules
{
    public VoiceAIASRLib(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
        bUseRTTI = true;
        bEnableExceptions = true;
        bUseUnity = false;

        PrivateDependencyModuleNames.AddRange(new[] { "QAIRT" });

        // Add any include paths for the plugin
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "inc"));

        var libs = new List<string> { "WhisperComponent.lib" };

        var libsToPackage = new List<string> { "dnnvad.dll", "WhisperComponent.dll", "WhisperComponent.winmd",
                                               "WhisperLib.dll", "WhisperProjection.dll" };

        var qnnCpuLibsToPackage = new List<string> { "libQnnCpu.dll" };

        var modelsToPackage =
            new List<string> { "models/vocab.bin", "models/decoder.bin", "models/encoder.bin" };

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicDelayLoadDLLs.Add("WhisperComponent.dll");
            LinkLibs(libs, "lib\\windows\\ARM64X");
            PackageLibs(libsToPackage, "lib\\windows\\ARM64X");
            PackageFiles(modelsToPackage);
        }

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "AndroidPackaging.xml"));
            AdditionalPropertiesForReceipt.Add("AndroidPlugin",
                                               Path.Combine(ModuleDirectory, "AndroidPackaging_QairtCpu.xml"));
        }
    }

    private void PackageFiles(List<string> libs)
    {
        foreach (var item in libs)
            RuntimeDependencies.Add(Path.Combine(ModuleDirectory, item), StagedFileType.NonUFS);
    }

    private void PackageLibs(List<string> libs, string platformId)
    {
        foreach (var item in libs)
            RuntimeDependencies.Add(Path.Combine(ModuleDirectory, platformId, item));
    }

    private void LinkLibs(List<string> libs, string platformId)
    {
        foreach (var item in libs)
            PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, platformId, item));
    }
}
