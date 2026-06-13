// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ContrarySurvivor : ModuleRules
{
	public ContrarySurvivor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", });

		// AIModule — для AAIController (AEnemyAIController): MoveToActor, LineOfSightTo, SetFocus.
		// GameplayTasks — транзитивная зависимость AIModule (path following / move tasks).
		// NavigationSystem — UNavigationSystemV1::ProjectPointToNavigation (спавн волков на навмеше).
		PrivateDependencyModuleNames.AddRange(new string[] { "AIModule", "GameplayTasks", "NavigationSystem" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
