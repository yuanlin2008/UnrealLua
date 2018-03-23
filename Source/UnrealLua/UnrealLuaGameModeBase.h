// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UnrealLuaGameModeBase.generated.h"

/**
 * 
 */
UCLASS()
class UNREALLUA_API AUnrealLuaGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	UFUNCTION()
	int testfunc(UObject*& o, int& i, const FVector v) { return 0; }
};
