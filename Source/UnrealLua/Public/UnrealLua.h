// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(UnrealLua, Log, All);

#define ULUA_LOG(Verbosity, Format, ...) UE_LOG(UnrealLua, Verbosity, Format, __VA_ARGS__)