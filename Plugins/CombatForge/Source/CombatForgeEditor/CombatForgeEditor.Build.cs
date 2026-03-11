using UnrealBuildTool;

public class CombatForgeEditor : ModuleRules
{
	public CombatForgeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CombatForge"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"UnrealEd"
		});
	}
}
