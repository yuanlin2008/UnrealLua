// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class Lua : ModuleRules
{
	public Lua(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core" });
        Definitions.Add("LUA_PLATFORM_" + Target.Platform.ToString());
        //if(Target.LinkType == TargetLinkType.Monolithic)
        //    Definitions.Add("LUA_BUILD_AS_DLL");
	}
}
