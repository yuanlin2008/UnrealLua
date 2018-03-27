// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealLuaGameModeBase.h"




void AUnrealLuaGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	FString script;
	FFileHelper::LoadFileToString(script, *(FPaths::ProjectDir() + TEXT("test.lua")));
	if (luaEnv_.loadString(TCHAR_TO_UTF8(*script)))
	{
		luaEnv_.pushUObject(this);
		luaEnv_.pcall(1, 0);
	}
}
