// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CombatSystem : ModuleRules
{
	public CombatSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"CombatForge",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"CombatSystem",
			"CombatSystem/Variant_Platforming",
			"CombatSystem/Variant_Platforming/Animation",
			"CombatSystem/Variant_Combat",
			"CombatSystem/Variant_Combat/AI",
			"CombatSystem/Variant_Combat/Animation",
			"CombatSystem/Variant_Combat/Gameplay",
			"CombatSystem/Variant_Combat/Interfaces",
			"CombatSystem/Variant_Combat/UI",
			"CombatSystem/Variant_SideScrolling",
			"CombatSystem/Variant_SideScrolling/AI",
			"CombatSystem/Variant_SideScrolling/Gameplay",
			"CombatSystem/Variant_SideScrolling/Interfaces",
			"CombatSystem/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
