using UnrealBuildTool;

public class Footcandle : ModuleRules
{
	public Footcandle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"FootcandleGen",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RHI",
			"RenderCore",
			"DeveloperSettings",
			"NavigationSystem",
			"AIModule",
		});
	}
}
