// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LuaEnv.h"
#include "UnrealLuaGameModeBase.generated.h"

USTRUCT()
struct FTestA
{
	GENERATED_BODY()
	UPROPERTY()
	FActorDestroyedSignature OnDestroyed;
};

/**
 * 
 */
UCLASS()
class UNREALLUA_API AUnrealLuaGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	AUnrealLuaGameModeBase();

	UPROPERTY()
	FString PropStr;

	UFUNCTION()
	int testfunc(UObject*& o, int& i, const FVector v) { return 0; }

	//UFUNCTION()
	//void testfunc1(FActorDestroyedSignature s) {}

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	FLuaEnv* luaEnv_;
};
