// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// All rights reserved.

using UnrealBuildTool;

public class SpeechRecognizerSampleTarget : TargetRules
{
    public SpeechRecognizerSampleTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        bOverrideBuildEnvironment = true;
        StaticAllocator = StaticAllocatorType.Ansi;
        ExtraModuleNames.Add("SpeechRecognizerSample");
    }
}
