#include "LuaEnv.h"
#include "UObjectIterator.h"
#include "lua.hpp"

TMap<lua_State*, FLuaEnv*> FLuaEnv::luaEnvMap_;

FLuaEnv::FLuaEnv():
	luaState_(nullptr)
{
	luaState_ = luaL_newstate();
	check(luaState_);
	luaEnvMap_.Add(luaState_, this);
}

FLuaEnv::~FLuaEnv()
{
	luaEnvMap_.Remove(luaState_);
	lua_close(luaState_);
}

void FLuaEnv::exportBPFLibs()
{
	for (TObjectIterator<UClass> it; it; ++it)
	{
		UClass* cls = *it;
		if (!cls->IsChildOf(UBlueprintFunctionLibrary::StaticClass()))
			continue;
	}
}

void FLuaEnv::pushPropertyValue(UObject* obj, UProperty* prop)
{
}

void FLuaEnv::fetchPerpertyValue(UObject* obj, UProperty* prop, int idx)
{
}