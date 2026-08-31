using UnrealBuildTool;

public class FootcandleTarget : TargetRules
{
	public FootcandleTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.Add("Footcandle");
		ExtraModuleNames.Add("FootcandleGen");
	}
}
