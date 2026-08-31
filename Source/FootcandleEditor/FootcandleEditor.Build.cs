using UnrealBuildTool;

public class FootcandleEditor : ModuleRules
{
	public FootcandleEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"Footcandle",
			"FootcandleGen",
		});
	}
}
