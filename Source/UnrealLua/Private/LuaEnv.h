#pragma once

#include "CoreMinimal.h"
#include "GCObject.h"

struct lua_State;

class FLuaEnv : public FGCObject
{
public:
	/** Get FLuaEnv from lua_State. */
	static FLuaEnv* getLuaEnv(lua_State* L) { return luaEnvMap_[L]; }

	FLuaEnv();
	~FLuaEnv();

	/** Get lua_State.  */
	lua_State* getLuaState() { return luaState_;}

	/** Push obj's property value to lua stack. */
	void pushPropertyValue(UObject* obj, UProperty* prop);

	/** Fetch obj's perperty value from lua stack. */
	void fetchPerpertyValue(UObject* obj, UProperty* prop, int idx);

private:
	void exportBPFLibs();

	static TMap<lua_State*, FLuaEnv*> luaEnvMap_;

	lua_State* luaState_;
};