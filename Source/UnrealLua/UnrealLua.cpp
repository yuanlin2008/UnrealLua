// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealLua.h"
#include "Modules/ModuleManager.h"
#include "lua.hpp"

void test()
{
	lua_newstate(nullptr, nullptr);
	lua_pushboolean(nullptr, 0);
}

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, UnrealLua, "UnrealLua" );
