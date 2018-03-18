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
	void exportBPFLibs();
	void exportBPFLib(UClass* bflCls);
	void exportBPFLFunc(const char* clsName, UFunction* f);

	/** Invoke a UFunction from lua stack. */
	int invokeUFunction();
	/** Memory allocation function for lua vm. */
	void* memAlloc(void* ptr, size_t osize, size_t nsize);

	/************************************************************************/
	/* Lua stack to cpp.                                                    */
	/************************************************************************/

	/** Get property value from lua stack.  */
	void toPropertyValue(void* obj, UProperty* prop, int idx);

	/** Get UObject from lua stack. */
	UObject* toUObject(UClass* cls, int idx);


	/************************************************************************/
	/* Cpp to lua stack.                                                    */
	/************************************************************************/

	/** Push property value to lua stack. */
	void pushPropertyValue(void* obj, UProperty* prop);

	/** Push UObject to lua stack. */
	void pushUObject(UObject* obj);

	/** Push a UFunction to lua stack. */
	void pushUFunction(UFunction* f);

	static FLuaEnv* getLuaEnv(lua_State* L) { return luaEnvMap_[L]; }
	static void* luaAlloc(void* ud, void* ptr, size_t osize, size_t nsize);
	static int luaPanic(lua_State* L);
	static int luaUFunctionWrapper(lua_State* L);

	static TMap<lua_State*, FLuaEnv*> luaEnvMap_;

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
};