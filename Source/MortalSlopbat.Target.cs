using UnrealBuildTool;

public class MortalSlopbatTarget : TargetRules
{
	public MortalSlopbatTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("MortalSlopbat");
	}
}
