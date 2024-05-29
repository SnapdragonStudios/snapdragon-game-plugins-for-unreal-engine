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
			if (Target.WindowsPlatform.Architecture == UnrealArch.X64)
			{
				// Add the import library
				PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "x86_64-windows-msvc", "SNPE.lib"));

				// Ensure that the DLL is staged along with the executable
				RuntimeDependencies.Add("$(BinaryOutputDir)/SNPE.dll", Path.Combine(ModuleDirectory, "lib", "x86_64-windows-msvc", "SNPE.dll"));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string LibSNPEPath = Path.Combine(ModuleDirectory, "lib", "aarch64-android", "libSNPE.so");
			string[] LibHtpStubPaths = new[]
			{
				Path.Combine(ModuleDirectory, "lib", "aarch64-android", "libSnpeHtpV68Stub.so"),
				Path.Combine(ModuleDirectory, "lib", "aarch64-android", "libSnpeHtpV69Stub.so"),
				Path.Combine(ModuleDirectory, "lib", "aarch64-android", "libSnpeHtpV73Stub.so"),
				Path.Combine(ModuleDirectory, "lib", "aarch64-android", "libSnpeHtpV75Stub.so"),
			};
			string LibHtpPreparePath = Path.Combine(ModuleDirectory, "lib", "aarch64-android", "libSnpeHtpPrepare.so");

			PublicAdditionalLibraries.Add(LibSNPEPath);
			PublicAdditionalLibraries.AddRange(LibHtpStubPaths);
			PublicAdditionalLibraries.Add(LibHtpPreparePath);

			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "SNPEPropertiesForReceipt.xml"));

		}
	}
}
