#pragma once

#include "UnrealLua.h"
#include "GCObject.h"

struct lua_State;

class FLuaEnv : public FGCObject
{
public:
	FLuaEnv();
	~FLuaEnv();

private:
	void exportBPFLibs();
	void exportBPFLib(UClass* bflCls);
	void exportBPFLFunc(const char* clsName, UFunction* f);

	void pushUFunction(UFunction* f);
	/** Invoke a UFunction from lua stack. */
	int invokeUFunction();
	/** Memory allocation function for lua vm. */
	void* memAlloc(void* ptr, size_t osize, size_t nsize);

	static FLuaEnv* getLuaEnv(lua_State* L) { return luaEnvMap_[L]; }
	static void* luaAlloc(void* ud, void* ptr, size_t osize, size_t nsize);
	static int luaPanic(lua_State* L);
	static int luaUFunctionWrapper(lua_State* L);

	static TMap<lua_State*, FLuaEnv*> luaEnvMap_;

	lua_State* luaState_;
	/** Total memory used by this lua state. */
	size_t memUsed_;
};