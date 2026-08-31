using UnrealBuildTool;

public class FootcandleEditorTarget : TargetRules
{
	public FootcandleEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.Add("Footcandle");
		ExtraModuleNames.Add("FootcandleGen");
		ExtraModuleNames.Add("FootcandleEditor");
		ExtraModuleNames.Add("FootcandleTests");
	}
}
