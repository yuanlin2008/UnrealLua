#include "LuaEnv.h"
#include "UObjectIterator.h"
#include "Kismet/BlueprintFunctionLibrary.h"
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
		exportBPFLib(cls);
	}
}

void FLuaEnv::exportBPFLib(UClass* bflCls)
{
	ULUA_LOG(Log, TEXT("Export Blueprint Function Library for \"%s\""), *(bflCls->GetName()));

	// Create a globle table for this library.
	// LibName = {}
	lua_newtable(luaState_);
	const char* clsName = TCHAR_TO_UTF8(*bflCls->GetName());
	lua_setglobal(luaState_, clsName);

	for(TFieldIterator<UFunction> it(bflCls, EFieldIteratorFlags::ExcludeSuper); it; ++it)
		exportBPFLFunc(clsName, *it);
}

static int luaUFunctionWrapper(lua_State* L)
{
	FLuaEnv* luaEnv = FLuaEnv::getLuaEnv(L);
	return luaEnv->invokeUFunction();
}

void FLuaEnv::exportBPFLFunc(const char* clsName, UFunction* f)
{
	ULUA_LOG(Verbose, TEXT("Export Function \"%s\""), *(f->GetName()));
	// ["libname"].func_name = function.
	lua_getglobal(luaState_, clsName);
	pushUFunction(f);
	lua_setfield(luaState_, -2, TCHAR_TO_UTF8(*(f->GetName())));
}

void FLuaEnv::pushUFunction(UFunction* f)
{
	// upvalue[1] = UFunction.
	lua_pushlightuserdata(luaState_, f);
	lua_pushcclosure(luaState_, luaUFunctionWrapper, 1);
}

int FLuaEnv::invokeUFunction()
{
	// UFunction = upvalue[1].
	UFunction* func = (UFunction*)lua_touserdata(luaState_, lua_upvalueindex(1));

	if (func->FunctionFlags & FUNC_Static)
	{
	}
	else
	{
	}

	return 0;
}

void FLuaEnv::pushPropertyValue(UObject* obj, UProperty* prop)
{
}

void FLuaEnv::fetchPerpertyValue(UObject* obj, UProperty* prop, int idx)
{
}