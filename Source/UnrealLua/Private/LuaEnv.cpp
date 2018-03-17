#include "LuaEnv.h"
#include "UObjectIterator.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "lua.hpp"

TMap<lua_State*, FLuaEnv*> FLuaEnv::luaEnvMap_;

FLuaEnv::FLuaEnv():
	luaState_(nullptr),
	memUsed_(0)
{
	luaEnvMap_.Add(luaState_, this);
	luaState_ = lua_newstate(luaAlloc, this);
	check(luaState_);

	exportBPFLibs();
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

void* FLuaEnv::memAlloc(void* ptr, size_t osize, size_t nsize)
{
	memUsed_ = memUsed_ - osize + nsize;
	if(nsize == 0)
	{
		FMemory::Free(ptr);
		return NULL;
	}
	else
		return FMemory::Realloc(ptr, nsize);
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
	/** Memory buffer for function parameters. */
	struct FParamBuffer
	{
		FParamBuffer(UFunction* f, uint8* b):func_(f),buffer_(b)
		{
			for(TFieldIterator<UProperty> it(func_); it && (it->PropertyFlags & CPF_Parm); ++it)
				(*it)->InitializeValue_InContainer(buffer_);
		}
		~FParamBuffer()
		{
			for(TFieldIterator<UProperty> it(func_); it && (it->PropertyFlags & CPF_Parm); ++it)
				(*it)->DestroyValue_InContainer(buffer_);
		}
		UFunction* func_;
		uint8* buffer_;
	};

	// UFunction = upvalue[1].
	UFunction* func = (UFunction*)lua_touserdata(luaState_, lua_upvalueindex(1));

	// Create param buffer from current stack.
	uint8* params = (uint8*)FMemory_Alloca(func->ParmsSize);
	FParamBuffer b(func, params);

	//func->HasAnyFunctionFlags(FUNC_HasOutParms);
	if (func->FunctionFlags & FUNC_Static)
	{
	}
	else
	{
	}

	return 0;
}

void* FLuaEnv::luaAlloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
	FLuaEnv* luaEnv = (FLuaEnv*)ud;
	return luaEnv->memAlloc(ptr, osize, nsize);
}

int FLuaEnv::luaPanic(lua_State* L)
{
	ULUA_LOG(Error, TEXT("PANIC:%s"), UTF8_TO_TCHAR(lua_tostring(L, -1)));
	return 0;
}

int FLuaEnv::luaUFunctionWrapper(lua_State* L)
{
	FLuaEnv* luaEnv = getLuaEnv(L);
	return luaEnv->invokeUFunction();
}

