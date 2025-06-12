// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AITagging : ModuleRules
{
	public AITagging(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"EditorSubsystem",
				"EditorScriptingUtilities",
				"Json",
				"RHI",
				"RHICore",
				"Slate",
				"SlateCore",
			}
			);
	}
}
