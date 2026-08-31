using UnrealBuildTool;

public class FootcandleTests : ModuleRules
{
	public FootcandleTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Footcandle",
			"FootcandleGen",
		});
	}
}
