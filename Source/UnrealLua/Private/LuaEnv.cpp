#include "LuaEnv.h"
#include "lua.hpp"

TMap<lua_State*, FLuaEnv*> FLuaEnv::luaEnvMap_;

FLuaEnv* FLuaEnv::getLuaEnv(lua_State* L)
{
	return luaEnvMap_[L];
}

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

void FLuaEnv::pushPropertyValue(UObject* obj, UProperty* prop)
{
}

void FLuaEnv::fetchPerpertyValue(UObject* obj, UProperty* prop, int idx)
{
}