#pragma once

#include "UnrealLua.h"
#include "GCObject.h"

struct lua_State;

class FLuaEnv : public FGCObject
{
public:
	friend class FLuaObject;

	FLuaEnv();
	~FLuaEnv();

	/** FGCObject Interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	void throwError(const char* fmt, ...);

	int callUFunction(UFunction* func);
	int callUClass(UClass* cls);
	int callStruct(UScriptStruct* s);

	/************************************************************************/
	/* Lua stack to cpp.                                                    */
	/************************************************************************/

	/** Get property value from lua stack.  */
	void toPropertyValue(void* obj, UProperty* prop, int idx);
	/** Get UObject from lua stack. */
	UObject* toUObject(int idx, UClass* cls = nullptr);
	/** Get FString from lua stack. */
	FString toString(int idx);
	/** Get FText from lua stack. */
	FText toText(int idx);
	/** Get FName from lua stack. */
	FName toName(int idx);


	/************************************************************************/
	/* Cpp to lua stack.                                                    */
	/************************************************************************/

	/** Push property value to lua stack. */
	void pushPropertyValue(void* obj, UProperty* prop);
	/** Push UObject to lua stack. */
	void pushUObject(UObject* obj);
	/** Push FString to lua stack. */
	void pushString(const FString& str);
	/** Push FText to lua stack. */
	void pushText(const FText& txt);
	/** Push FName to lua stack. */
	void pushName(FName name);


	lua_State* luaState_;
	/** Total memory used by this lua state. */
	size_t memUsed_;

	/**
	 * A weak table in registry to map UObject ptr to userdata.
	 * UObjectPtr->FUObjectProxy.
	 */
	int uobjTable_;
	/** 
	 * A table in registry to create lua object references. 
	 * refs=>luaobj 
	 */
	int luaObjTable_;
	/** 
	 * A table in registry to save lua object reference info. 
	 * luaobj=>FLuaObjRefInfo 
	 */
	int luaObjRefInfoTable_;
	/**
	 * UObject proxy metatable.
	 */
	int uobjMetatable_;


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

	static TMap<lua_State*, FLuaEnv*> luaEnvMap_;
};