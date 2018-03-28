#pragma once

#include "UnrealLua.h"
#include "GCObject.h"
#include "lua.hpp"

class ULuaDelegate;

class UNREALLUA_API FLuaEnv : public FGCObject
{
public:
	friend class FLuaObject;

	FLuaEnv();
	~FLuaEnv();

	/** FGCObject Interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	//////////////////////////////////////////////////////////////////////////
	// Lua stack to cpp.
	//////////////////////////////////////////////////////////////////////////
	bool		isNil(int idx)		{ return lua_isnil(luaState_, idx); }
	lua_Number	toNumber(int idx)	{ return lua_tonumber(luaState_, idx); }
	lua_Integer	toInteger(int idx)	{ return lua_tointeger(luaState_, idx); }
	bool		toBoolean(int idx)	{ return lua_toboolean(luaState_, idx); }
	UObject*	toUObject(int idx, UClass* cls, bool check);
	void*		toUStruct(int idx, UScriptStruct* structType, bool check);
	FString		toFString(int idx, bool check);
	FText		toFText(int idx, bool check);
	FName		toFName(int idx, bool check);

	void		toPropertyValue(void* obj, UProperty* prop, int idx, bool check);


	//////////////////////////////////////////////////////////////////////////
	// Cpp to lua stack.
	//////////////////////////////////////////////////////////////////////////
	void pushNil()					{ lua_pushnil(luaState_); }
	void pushNumber(lua_Number n)	{ lua_pushnumber(luaState_, n); }
	void pushInteger(lua_Integer n)	{ lua_pushinteger(luaState_, n); }
	void pushBoolean(bool b)		{ lua_pushboolean(luaState_, b?1:0); }
	void pushUObject(UObject* obj);
	void pushUStruct(void* structPtr, UScriptStruct* structType);
	void pushString(const TCHAR* s);
	void pushFString(const FString& str);
	void pushFText(const FText& txt);
	void pushFName(FName name);

	void pushPropertyValue(void* obj, UProperty* prop);

	//////////////////////////////////////////////////////////////////////////
	// Load and Call.
	//////////////////////////////////////////////////////////////////////////
	bool loadString(const char* s);
	bool pcall(int n, int r);

private:
	void throwError(const char* fmt, ...);

	int callUFunction(UFunction* func);
	int callUClass(UClass* cls);
	int callStruct(UScriptStruct* s);

	friend class ULuaDelegate;
	void invokeDelegate(ULuaDelegate* d, void* params);

	lua_State* luaState_;
	/** Total memory used by this lua state. */
	size_t memUsed_;

	/**
	 * A weak table in registry to map UObject ptr to userdata.
	 * UObjectPtr->FUObjectProxy.
	 */
	int uobjTable_;

	/**
	 * Referenced structs.
	 */
	TSet<UScriptStruct*> structs_;

	static FLuaEnv* getLuaEnv(lua_State* L) { return luaEnvMap_[L]; }
	/** Memory allocation function for lua vm. */
	void* memAlloc(void* ptr, size_t osize, size_t nsize);
	static void* _lua_cb_memAlloc(void* ud, void* ptr, size_t osize, size_t nsize) { return ((FLuaEnv*)ud)->memAlloc(ptr, osize, nsize); }

#define DECLARE_LUA_CALLBACK(NAME) \
	int NAME();\
	static int _lua_cb_##NAME(lua_State* L) { return getLuaEnv(L)->NAME(); }
#define LUA_CALLBACK(NAME) _lua_cb_##NAME

	DECLARE_LUA_CALLBACK(handlePanic);
	DECLARE_LUA_CALLBACK(uobjMTIndex);
	DECLARE_LUA_CALLBACK(uobjMTNewIndex);
	DECLARE_LUA_CALLBACK(uobjMTCall);
	DECLARE_LUA_CALLBACK(uobjMTToString);

	DECLARE_LUA_CALLBACK(ustructMTIndex);
	DECLARE_LUA_CALLBACK(ustructMTNewIndex);
	DECLARE_LUA_CALLBACK(ustructMTGC);

	static TMap<lua_State*, FLuaEnv*> luaEnvMap_;
};