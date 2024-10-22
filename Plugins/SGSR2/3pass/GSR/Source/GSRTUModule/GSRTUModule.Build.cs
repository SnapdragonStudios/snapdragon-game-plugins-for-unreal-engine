//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

using UnrealBuildTool;
using System;
using System.IO;

public class GSRTUModule : ModuleRules
{
	public GSRTUModule(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]{
				// ... add public include paths required here ...
				EngineDirectory + "/Source/Runtime/Renderer/Private",
			}
			);

		PrivateIncludePaths.AddRange(
			new string[]{
				// ... add other private include paths required here ...
			
			}
			);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				// ... add other public dependencies that you statically link with here ...
				"Engine",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"Projects",
				"RenderCore",
				"Renderer",
				"RHI",
				"CoreUObject",
				"DeveloperSettings",
				// ... add private dependencies that you statically link with here ...
			}
			);

		PrecompileForTargets = PrecompileTargetsType.Any;
	}
}