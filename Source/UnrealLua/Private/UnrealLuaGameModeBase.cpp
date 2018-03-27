// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealLuaGameModeBase.h"


AUnrealLuaGameModeBase::AUnrealLuaGameModeBase()
{
}

void AUnrealLuaGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	luaEnv_ = new FLuaEnv();

	FString script;
	FFileHelper::LoadFileToString(script, *(FPaths::ProjectDir() + TEXT("test.lua")));
	if (luaEnv_->loadString(TCHAR_TO_UTF8(*script)))
	{
		luaEnv_->pushUObject(this);
		luaEnv_->pcall(1, 0);
	}
}

void AUnrealLuaGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	delete luaEnv_;
	luaEnv_ = nullptr;
	Super::EndPlay(EndPlayReason);
}

