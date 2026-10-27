using UnrealBuildTool;

public class MortalSlopbat : ModuleRules
{
	public MortalSlopbat(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Json" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "ApplicationCore", "AudioMixer", "AssetRegistry", "RenderCore", "RHI" });
		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			// The game window is SDL's; full screen is checked against it (SlopGameInstance).
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL3");
		}
	}
}
