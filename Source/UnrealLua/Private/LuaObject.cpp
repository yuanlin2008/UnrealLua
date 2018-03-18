#include "LuaObject.h"
#include "LuaEnv.h"
#include "lua.hpp"

struct FLuaObjRefInfo
{
	int ref;
	int num;
};

FLuaObject::FLuaObject(FLuaEnv* luaEnv, int idx):
	env_(luaEnv),
	ref_(LUA_NOREF)
{
	lua_State* L = env_->luaState_;
	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, env_->luaObjRefInfoTable_);
	lua_pushvalue(L, idx);
	//=========================================
	//=>luaObjRefInfoTable
	//=>luaObj
	//=========================================

	lua_rawget(L, -2);
	//=========================================
	//=>luaObjRefInfoTable
	//=>FLuaObjRefInfo or nil
	//=========================================
	if(lua_isnil(L, -1))
	{
		// new object reference.
		lua_pop(L, 1);
		//=========================================
		//=>luaObjRefInfoTable
		//=========================================
		lua_rawgeti(L, LUA_REGISTRYINDEX, env_->luaObjTable_);
		lua_pushvalue(L, idx);
		//=========================================
		//=>luaObjRefInfoTable
		//=>luaObjTable
		//=>luaObj
		//=========================================
		ref_ = luaL_ref(L, -2);
		lua_pop(L, 1);
		//=========================================
		//=>luaObjRefInfoTable
		//=========================================
		lua_pushvalue(L, idx);
		FLuaObjRefInfo* info = (FLuaObjRefInfo*)lua_newuserdata(L, sizeof(FLuaObjRefInfo));
		info->ref = ref_;
		info->num = 1;
		//=========================================
		//=>luaObjRefInfoTable
		//=>luaObj
		//=>FLuaObjRefInfo
		//=========================================
		lua_rawset(L, -3);
	}
	else
	{
		// existed object reference.
		FLuaObjRefInfo* info = (FLuaObjRefInfo*)lua_touserdata(L, -1);
		ref_ = info->ref;
		info->num++;
	}
	lua_settop(L, top);
}

FLuaObject::~FLuaObject()
{
	lua_State* L = env_->luaState_;
	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, env_->luaObjTable_);
	lua_rawgeti(L, LUA_REGISTRYINDEX, env_->luaObjRefInfoTable_);
	//=========================================
	//=>luaObjTable
	//=>luaObjRefInfoTable
	//=========================================
	lua_rawgeti(L, -2, ref_);
	//=========================================
	//=>luaObjTable
	//=>luaObjRefInfoTable
	//=>luaObj
	//=========================================
	lua_pushvalue(L, -1);
	//=========================================
	//=>luaObjTable
	//=>luaObjRefInfoTable
	//=>luaObj
	//=>luaObj
	//=========================================
	lua_rawget(L, -3);
	//=========================================
	//=>luaObjTable
	//=>luaObjRefInfoTable
	//=>luaObj
	//=>FLuaObjRefInfo
	//=========================================
	FLuaObjRefInfo* info = (FLuaObjRefInfo*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	//=========================================
	//=>luaObjTable
	//=>luaObjRefInfoTable
	//=>luaObj
	//=========================================
	info->num--;
	if(info->num <= 0)
	{
		lua_pushnil(L);
		//=========================================
		//=>luaObjTable
		//=>luaObjRefInfoTable
		//=>luaObj
		//=>nil
		//=========================================
		lua_rawset(L, -3);
		//=========================================
		//=>luaObjTable
		//=>luaObjRefInfoTable
		//=========================================
		lua_pop(L, 1);
		luaL_unref(L, -1, ref_);
	}
	lua_settop(L, top);
}