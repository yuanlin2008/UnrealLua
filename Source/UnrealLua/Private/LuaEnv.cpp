#include "LuaEnv.h"
#include "UObjectIterator.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "lua.hpp"

TMap<lua_State*, FLuaEnv*> FLuaEnv::luaEnvMap_;

struct FLuaUserdata
{
	enum class Type : int
	{
		UObject,
		UStruct,
	};
	Type type;
};

struct FUObjectProxy : public FLuaUserdata
{
	UObject* obj;
};

FLuaEnv::FLuaEnv():
	luaState_(nullptr),
	memUsed_(0),
	uobjTable_(LUA_NOREF),
	luaObjTable_(LUA_NOREF),
	luaObjRefInfoTable_(LUA_NOREF),
	uobjMetatable_(LUA_NOREF)
{
	luaEnvMap_.Add(luaState_, this);
	luaState_ = lua_newstate(LUA_CALLBACK(memAlloc), this);
	check(luaState_);
	lua_atpanic(luaState_, LUA_CALLBACK(handlePanic));

	int top = lua_gettop(luaState_);

	// Create UObject table.
	lua_newtable(luaState_);
	lua_newtable(luaState_); // metatable.
	lua_pushstring(luaState_, "v");
	lua_setfield(luaState_, -2, "__mode"); // weak value table.
	lua_setmetatable(luaState_, -2);
	uobjTable_ = luaL_ref(luaState_, LUA_REGISTRYINDEX);

	lua_newtable(luaState_);
	luaObjTable_ = luaL_ref(luaState_, LUA_REGISTRYINDEX);
	lua_newtable(luaState_);
	luaObjRefInfoTable_ = luaL_ref(luaState_, LUA_REGISTRYINDEX);

	// Create UObject proxy metatable.
	lua_newtable(luaState_);
	lua_pushcfunction(luaState_, LUA_CALLBACK(uobjMTIndex));
	lua_setfield(luaState_, -2, "__index");
	lua_pushcfunction(luaState_, LUA_CALLBACK(uobjMTNewIndex));
	lua_setfield(luaState_, -2, "__newindex");
	lua_pushcfunction(luaState_, LUA_CALLBACK(uobjMTCall));
	lua_setfield(luaState_, -2, "__call");
	uobjMetatable_ = luaL_ref(luaState_, LUA_REGISTRYINDEX);

	exportBPFLibs();

	lua_settop(luaState_, top);
}

FLuaEnv::~FLuaEnv()
{
	luaEnvMap_.Remove(luaState_);
	lua_close(luaState_);
}

void FLuaEnv::AddReferencedObjects(FReferenceCollector& Collector)
{
	// Iterate all referenced UObject from uobjTable.
	Collector.AllowEliminatingReferences(false);
	lua_rawgeti(luaState_, LUA_REGISTRYINDEX, uobjTable_);
	lua_pushnil(luaState_);
	while(lua_next(luaState_, -2) != 0)
	{
		UObject* uobj = (UObject*)lua_touserdata(luaState_, -2);
		FUObjectProxy* p = (FUObjectProxy*)lua_touserdata(luaState_, -1);
		if(uobj)
			Collector.AddReferencedObject(uobj);
		lua_pop(luaState_, 1);
	}
	lua_pop(luaState_, 1);
	Collector.AllowEliminatingReferences(true);
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

void FLuaEnv::exportBPFLFunc(const char* clsName, UFunction* f)
{
	ULUA_LOG(Verbose, TEXT("Export Function \"%s\""), *(f->GetName()));
	// ["libname"].func_name = function.
	lua_getglobal(luaState_, clsName);
	//pushUFunction(f);
	// todo.
	lua_setfield(luaState_, -2, TCHAR_TO_UTF8(*(f->GetName())));
}

int FLuaEnv::callUFunction(UFunction* func)
{
	/** Memory buffer for function parameters. */
	struct FParamBuffer
	{
		FParamBuffer(UFunction* f, uint8* b):func_(f),buffer_(b)
		{
			for(TFieldIterator<UProperty> it(func_); it && it->HasAnyPropertyFlags(CPF_Parm); ++it)
				(*it)->InitializeValue_InContainer(buffer_);
		}
		~FParamBuffer()
		{
			for(TFieldIterator<UProperty> it(func_); it && it->HasAnyPropertyFlags(CPF_Parm); ++it)
				(*it)->DestroyValue_InContainer(buffer_);
		}
		UFunction* func_;
		uint8* buffer_;
	};

	// Create param buffer from current stack.
	uint8* params = (uint8*)FMemory_Alloca(func->ParmsSize);
	FParamBuffer b(func, params);

	int paramIdx = 2;
	for(TFieldIterator<UProperty> it(func); it && (it->PropertyFlags & (CPF_Parm|CPF_ReturnParm)) == CPF_Parm; ++it)
	{
		// todo.
	}

	//func->HasAnyFunctionFlags(FUNC_HasOutParms);
	if (func->FunctionFlags & FUNC_Static)
	{
	}
	else
	{
	}

	return 0;
}

int FLuaEnv::callUClass(UClass* cls)
{
	// todo.
	return 0;
}

void FLuaEnv::toPropertyValue(void* obj, UProperty* prop, int idx)
{
	if(auto p = Cast<UByteProperty>(prop))
		p->SetPropertyValue_InContainer(obj, lua_tointeger(luaState_, idx));
	else if(auto p = Cast<UInt8Property>(prop))
		p->SetPropertyValue_InContainer(obj, lua_tointeger(luaState_, idx));
	else if(auto p = Cast<UInt16Property>(prop))
		p->SetPropertyValue_InContainer(obj, lua_tointeger(luaState_, idx));
	else if(auto p = Cast<UIntProperty>(prop))
		p->SetPropertyValue_InContainer(obj, lua_tointeger(luaState_, idx));
	else if(auto p = Cast<UInt64Property>(prop))
		p->SetPropertyValue_InContainer(obj, lua_tointeger(luaState_, idx));
	else if(auto p = Cast<UUInt16Property>(prop))
		p->SetPropertyValue_InContainer(obj, lua_tointeger(luaState_, idx));
	else if(auto p = Cast<UUInt32Property>(prop))
		p->SetPropertyValue_InContainer(obj, lua_tointeger(luaState_, idx));
	else if(auto p = Cast<UUInt64Property>(prop))
		p->SetPropertyValue_InContainer(obj, lua_tointeger(luaState_, idx));
	else if(auto p = Cast<UFloatProperty>(prop))
		p->SetPropertyValue_InContainer(obj, lua_tonumber(luaState_, idx));
	else if(auto p = Cast<UDoubleProperty>(prop))
		p->SetPropertyValue_InContainer(obj, lua_tonumber(luaState_, idx));
	else if(auto p = Cast<UBoolProperty>(prop))
		p->SetPropertyValue_InContainer(obj, lua_toboolean(luaState_, idx));
	/**
	UObjectProperty 
	UWeakObjectProperty 
	ULazyObjectProperty 
	USoftObject 
	UClassProperty
	*/
	else if(auto p = Cast<UObjectPropertyBase>(prop))
	{
		p->SetObjectPropertyValue_InContainer(obj, toUObject(idx, p->PropertyClass));
	}
	else if(auto p = Cast<UInterfaceProperty>(prop))
	{
		UObject* o = toUObject(idx);
		if(auto ip = o->GetInterfaceAddress(p->InterfaceClass))
			p->SetPropertyValue_InContainer(obj, FScriptInterface(o, ip));
		else
			p->SetPropertyValue_InContainer(obj, FScriptInterface());
	}
	else if(auto p = Cast<UNameProperty>(prop))
		p->SetPropertyValue_InContainer(obj, toName(idx));
	else if(auto p = Cast<UStrProperty>(prop))
		p->SetPropertyValue_InContainer(obj, toString(idx));
	else if(auto p = Cast<UArrayProperty>(prop))
	{
		// todo.
	}
	else if(auto p = Cast<UMapProperty>(prop))
	{
		// todo.
	}
	else if(auto p = Cast<USetProperty>(prop))
	{
		// todo.
	}
	else if(auto p = Cast<UStructProperty>(prop))
	{
		// todo.
	}
	else if(auto p = Cast<UDelegateProperty>(prop))
	{
		// todo.
	}
	else if(auto p = Cast<UMulticastDelegateProperty>(prop))
	{
		// todo.
	}
	else if(auto p = Cast<UTextProperty>(prop))
		p->SetPropertyValue_InContainer(obj, toText(idx));
	else if(auto p = Cast<UEnumProperty>(prop))
	{
		// todo.
	}
}

UObject* FLuaEnv::toUObject(int idx, UClass* cls)
{
	FLuaUserdata* u = (FLuaUserdata*)lua_touserdata(luaState_, idx);
	if(u == NULL || u->type != FLuaUserdata::Type::UObject)
		return nullptr;
	FUObjectProxy* p = (FUObjectProxy*)u;
	UObject* o = p->obj;
	// todo: warning?
	if(cls == nullptr || o->IsA(cls))
		return o;
	else
		return nullptr;
}
FString FLuaEnv::toString(int idx)
{
	return UTF8_TO_TCHAR(lua_tostring(luaState_, idx));
}

FText FLuaEnv::toText(int idx)
{
	return FText::FromString(UTF8_TO_TCHAR(lua_tostring(luaState_, idx)));
}

FName FLuaEnv::toName(int idx)
{
	return UTF8_TO_TCHAR(lua_tostring(luaState_, idx));
}

void FLuaEnv::pushPropertyValue(void* obj, UProperty* prop)
{
	// todo.
}

void FLuaEnv::pushUObject(UObject* obj)
{
	// Find in uobjTable first.
	lua_rawgeti(luaState_, LUA_REGISTRYINDEX, uobjTable_);
	lua_pushlightuserdata(luaState_, obj);
	lua_rawget(luaState_, -2);
	//=========================================
	//=>uobjTable_
	//=>FUObjectProxy or nil
	//=========================================
	if(lua_isnil(luaState_, -1))
	{
		lua_pop(luaState_, 1);
		lua_pushlightuserdata(luaState_, obj);
		lua_pushvalue(luaState_, -1);
		FUObjectProxy* p = (FUObjectProxy*)lua_newuserdata(luaState_, sizeof(FUObjectProxy));
		p->type = FLuaUserdata::Type::UObject;
		p->obj = obj;
		//=========================================
		//=>uobjTable_
		//=>uobjptr
		//=>uobjptr
		//=>FUObjectProxy
		//=========================================
		lua_rawset(luaState_, -4);
		lua_rawget(luaState_, -2);
		lua_replace(luaState_, -2);
		//=========================================
		//=>FUObjectProxy
		//=========================================

		// todo: set metatable.
	}
	else
	{
		lua_replace(luaState_, -2);
	}
}

void FLuaEnv::pushString(const FString& str)
{
	lua_pushstring(luaState_, TCHAR_TO_UTF8(*str));
}

void FLuaEnv::pushText(const FText& txt)
{
	pushString(txt.ToString());
}

void FLuaEnv::pushName(FName name)
{
	// todo: optimize?
	pushString(name.ToString());
}

//////////////////////////////////////////////////////////////////////////
/************************************************************************/
/* Lua Callbacks.                                                       */
/************************************************************************/

void* FLuaEnv::memAlloc(void* ptr, size_t osize, size_t nsize)
{
	memUsed_ = memUsed_ - osize + nsize;
	if(nsize == 0)
	{
		FMemory::Free(ptr);
		return nullptr;
	}
	else
		return FMemory::Realloc(ptr, nsize);
}

int FLuaEnv::handlePanic()
{
	ULUA_LOG(Error, TEXT("PANIC:%s"), UTF8_TO_TCHAR(lua_tostring(luaState_, -1)));
	return 0;
}

int FLuaEnv::uobjMTIndex()
{
	UObject* obj = toUObject(1);
	FName propName = toName(2);
	// todo: optimize.
	UField* field = FindField<UField>(obj->GetClass(), propName);
	if (!field)
	{
		lua_pushnil(luaState_);
	}
	else if (auto prop = Cast<UProperty>(field))
	{
		// Return property value.
		pushPropertyValue(obj, prop);
	}
	else if (auto func = Cast<UFunction>(field))
	{
		// Return UFunction.
		pushUObject(func);
	}
	return 1;
}

int FLuaEnv::uobjMTNewIndex()
{
	UObject* obj = toUObject(1);
	FName propName = toName(2);
	// todo: optimize.
	UProperty* prop = FindField<UProperty>(obj->GetClass(), propName);
	if (prop)
	{
		toPropertyValue(obj, prop, 3);
	}
	else
	{
		// todo.
	}
	return 0;
}

int FLuaEnv::uobjMTCall()
{
	UObject* obj = toUObject(1);
	if (auto func = Cast<UFunction>(obj))
	{
		// Call UFunction.
		return callUFunction(func);
	}
	else if(auto cls = Cast<UClass>(obj))
	{
		// Call UClass
		return callUClass(cls);
	}
	else
	{
		// todo.
	}
	return 0;
}
