using UnrealBuildTool;

public class Weltall : ModuleRules
{
	public Weltall(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "WeltallCore", "RenderCore" });
		PrivateDependencyModuleNames.AddRange(new[] { "ApplicationCore", "AudioExtensions", "EnhancedInput", "ImageWrapper", "InputCore", "InputDevice", "RHI", "Slate", "SlateCore" });
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Windows' HID and device setup functions, for the gamepads XInput does not read.
			PublicSystemLibraries.AddRange(new[] { "hid.lib", "setupapi.lib" });
		}
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// Android's file picker, through the game's activity: the player's files have no other way in or out.
			PrivateDependencyModuleNames.Add("Launch");
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", System.IO.Path.Combine(ModuleDirectory, "Weltall_UPL.xml"));
		}
	}
}
