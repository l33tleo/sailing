using UnrealBuildTool;

public class Sailing : ModuleRules
{
	public Sailing(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine",
			"InputCore",
			"EnhancedInput",
			"ProceduralMeshComponent",
			"UMG",
			"Slate",
			"SlateCore"
		});
	}
}
