using UnrealBuildTool;

// Pure generation module. HARD RULES (docs/adr/0006):
//  - No dependency on the Footcandle game module.
//  - No editor dependencies, ever - headless seed soaking depends on it.
//  - Output is plain data (no actors); spawning lives in the game module.
public class FootcandleGen : ModuleRules
{
	public FootcandleGen(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
		});
	}
}
