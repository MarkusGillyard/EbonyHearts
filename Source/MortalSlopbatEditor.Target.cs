using UnrealBuildTool;

public class MortalSlopbatEditorTarget : TargetRules
{
	public MortalSlopbatEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("MortalSlopbat");
	}
}
