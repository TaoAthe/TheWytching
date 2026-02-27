// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TheWytching : ModuleRules
{
	public TheWytching(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore",
			"HTTP", "Json", "JsonUtilities", "ImageWrapper",
			"AIModule", "NavigationSystem",
			"GameplayAbilities", "GameplayTags", "GameplayTasks",
			"StateTreeModule", "GameplayStateTreeModule",
			"SmartObjectsModule"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}

