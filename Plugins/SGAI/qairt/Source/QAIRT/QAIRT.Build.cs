// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

using System.Collections.Generic;
using System.IO;
using UnrealBuildTool;

public class QAIRT : ModuleRules
{
    public QAIRT(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });

        PrivateDependencyModuleNames.AddRange(new[] { "Projects" });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            var libsToPackage = new List<string> { "QnnCpu.dll",
                                                   "QnnGenAiTransformer.dll",
                                                   "QnnGenAiTransformerModel.dll",
                                                   "QnnModelDlc.dll",
                                                   "QnnSaver.dll" };
            var htpLibsToPackage = new List<string> { "QnnSystem.dll", "QnnHtpPrepare.dll", "QnnHtp.dll",
                                                      "QnnHtpNetRunExtensions.dll", "QnnHtp*Stub.dll" };
            var libs = new List<string>();
            var qnnv73LibsToPackage =
                new List<string> { "libqnnhtp*.cat", "libQnnHtp*.so", "libQnnHtp*QemuDriver.so", "libQnnHtp*Skel.so" };

            if (Target.WindowsPlatform.Architecture == UnrealArch.X64)
            {
                // x86-64
                PackageLibs(libsToPackage, "x86_64-windows-msvc");
                // arm64x
                PackageLibs(htpLibsToPackage, "arm64x-windows-msvc");
                PackageLibs(qnnv73LibsToPackage, "arm64x-windows-msvc");
            }
            else if (Target.WindowsPlatform.Architecture == UnrealArch.Arm64)
            {
                PackageLibs(htpLibsToPackage, "aarch64-windows-msvc");
                PackageLibs(qnnv73LibsToPackage, "aarch64-windows-msvc");
            }
        }

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "AndroidPackaging.xml"));
        }
    }

    private void PackageLibs(List<string> libs, string platformId)
    {
        foreach (var item in libs)
            RuntimeDependencies.Add("$(ModuleDir)/../ThirdParty/qairt/lib/" + platformId + "/" + item);
    }
}
