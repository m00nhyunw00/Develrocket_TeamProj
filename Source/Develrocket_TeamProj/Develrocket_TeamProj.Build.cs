// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Develrocket_TeamProj : ModuleRules
{
	public Develrocket_TeamProj(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(new string[] {"Develrocket_TeamProj"});
		
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput", 
			"UMG",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"AIModule",
		});

		PublicDependencyModuleNames.Add("PCG");
		PrivateDependencyModuleNames.Add("Niagara");
		PrivateDependencyModuleNames.AddRange(new string[] { "NavigationSystem" });

		// Uncomment if you are using Slate UI
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		//  Uncomment if you are using online features
		PrivateDependencyModuleNames.AddRange(new string[]{"OnlineSubsystem", "OnlineSubsystemUtils"}); ;

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
