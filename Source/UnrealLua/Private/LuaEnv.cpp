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
	lua_newtable(luaState_);12124e23dcvds2
	lua_pushcfunction(luaState_, LUA_CALLBACK(uobjMTIndex));
	lua_setfield(luaState_, -2, "__index");
	lua_pushcfunction(luaState_, LUA_CALLBACK(uobjMTNewIndex));
	lua_setfield(luaState_, -2, "__newindex");
	lua_pushcfunction(luaState_, LUA_CALLBACK(uobjMTCall));
	lua_setfield(luaState_, -2, "__call");
	uobjMetatable_ = luaL_ref(luaState_, LUA_REGISTRYINDEX);

	lua_settop(luaState_, top);
	ULUA_LOG(Log, TEXT("FLuaEnv created."));
}

FLuaEnv::~FLuaEnv()
{
	luaEnvMap_.Remove(luaState_);
	lua_close(luaState_);
	ULUA_LOG(Log, TEXT("FLuaEnv destroyed."));
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

void FLuaEnv::throwError(const char* fmt, ...)
{
  va_list argp;
  va_start(argp, fmt);
  lua_pushvfstring(luaState_, fmt, argp);
  va_end(argp);
  lua_error(luaState_);
}

struct FUFunctionParams
{
	FUFunctionParams(UFunction* f, void* b):
		buffer(b),
		parmNum(0), 
		retParm(nullptr),
		outParmNum(0)
	{
		for(TFieldIterator<UProperty> it(f); it && it->HasAnyPropertyFlags(CPF_Parm); ++it)
		{
			UProperty* parm = *it;
			parm->InitializeValue_InContainer(buffer);
			if(parm->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				// return parameter.
				retParm = parm;
			}
			else
			{
				parms[parmNum] = parm;
				parmNum++;
				check(parmNum <= ParmMax);
				if((parm->PropertyFlags & (CPF_ConstParm | CPF_OutParm)) == CPF_OutParm)
				{
					// out parameter.
					outParms[outParmNum] = parm;
					outParmNum++;
				}
			}
		}
	}

	~FUFunctionParams()
	{
		for(int i = 0; i < parmNum; i++)
			parms[i]->DestroyValue_InContainer(buffer);
		if(retParm)
			retParm->DestroyValue_InContainer(buffer);
		ULUA_LOG(Verbose, TEXT("FUFunctionParams destructed."));
	}

	void*		buffer;
	enum {ParmMax = 16};
	UProperty*	parms[ParmMax];
	int			parmNum;
	UProperty*	retParm;
	UProperty*	outParms[ParmMax];
	int			outParmNum;
};

int FLuaEnv::callUFunction(UFunction* func)
{
	// Get Self Object.
	bool isStaticFunc = func->HasAnyFunctionFlags(FUNC_Static);
	int paramIdx = isStaticFunc?2:3;
	UClass* cls = func->GetOwnerClass();
	UObject* obj = isStaticFunc ? cls->GetDefaultObject() : toUObject(2, cls);
	if(!obj)
	{
		throwError("Invalid self UObject");
	}

	// Create param buffer from current stack.
	uint8* paramBuffer = (uint8*)FMemory_Alloca(func->ParmsSize);

	// Initialize param buffer.
	FUFunctionParams params(func, paramBuffer);

	// Get function parameter value from lua stack.
	for(int i = 0; i < params.parmNum; i++)
	{
		toPropertyValue(paramBuffer, params.parms[i], paramIdx);
		paramIdx++;
	}

	// Call UFunction.
	obj->ProcessEvent(func, paramBuffer);

	int retNum = 0;
	// Return value to lua stack.
	if(params.retParm)
	{
		pushPropertyValue(paramBuffer, params.retParm);
		retNum++;
	}

	// Return out value to lua stack.
	for(int i = 0; i < params.outParmNum; i++)
	{
		pushPropertyValue(paramBuffer, params.outParms[i]);
		retNum++;
	}

	return retNum;
}

int FLuaEnv::callUClass(UClass* cls)
{
	// todo.
	return 0;
}

int FLuaEnv::callStruct(UScriptStruct* s)
{
	// todo.
	return 0;
}

void FLuaEnv::toPropertyValue(void* obj, UProperty* prop, int idx)
{
	if(auto p = Cast<UByteProperty>(prop))
		p->SetPropertyValue_InContainer(obj, luaL_checkinteger(luaState_, idx));
	else if(auto p = Cast<UInt8Property>(prop))
		p->SetPropertyValue_InContainer(obj, luaL_checkinteger(luaState_, idx));
	else if(auto p = Cast<UInt16Property>(prop))
		p->SetPropertyValue_InContainer(obj, luaL_checkinteger(luaState_, idx));
	else if(auto p = Cast<UIntProperty>(prop))
		p->SetPropertyValue_InContainer(obj, luaL_checkinteger(luaState_, idx));
	else if(auto p = Cast<UInt64Property>(prop))
		p->SetPropertyValue_InContainer(obj, luaL_checkinteger(luaState_, idx));
	else if(auto p = Cast<UUInt16Property>(prop))
		p->SetPropertyValue_InContainer(obj, luaL_checkinteger(luaState_, idx));
	else if(auto p = Cast<UUInt32Property>(prop))
		p->SetPropertyValue_InContainer(obj, luaL_checkinteger(luaState_, idx));
	else if(auto p = Cast<UUInt64Property>(prop))
		p->SetPropertyValue_InContainer(obj, luaL_checkinteger(luaState_, idx));
	else if(auto p = Cast<UFloatProperty>(prop))
		p->SetPropertyValue_InContainer(obj, luaL_checknumber(luaState_, idx));
	else if(auto p = Cast<UDoubleProperty>(prop))
		p->SetPropertyValue_InContainer(obj, luaL_checknumber(luaState_, idx));
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
	FName name = toName(2);
	UClass* cls = Cast<UClass>(obj);
	// todo: optimize.
	UField* field = FindField<UField>(cls, name);
	if (auto prop = Cast<UProperty>(field))
	{
		// Return property value.
		pushPropertyValue(obj, prop);
	}
	else if (auto func = Cast<UFunction>(field))
	{
		// Return UFunction.
		pushUObject(func);
	}
	else
	{
		throwError("Invalid field name %s", TCHAR_TO_UTF8(*name.ToString()));
	}
	return 1;
}

int FLuaEnv::uobjMTNewIndex()
{
	UObject* obj = toUObject(1);
	FName name = toName(2);
	// todo: optimize.
	UProperty* prop = FindField<UProperty>(obj->GetClass(), name);
	if (prop)
	{
		toPropertyValue(obj, prop, 3);
	}
	else
	{
		throwError("Invalid field name \"%s\"", TCHAR_TO_UTF8(*name.ToString()));
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
		// New object.
		return callUClass(cls);
	}
	else if(auto s = Cast<UScriptStruct>(obj))
	{
		// New struct.
		return callStruct(s);
	}
	else
	{
		throwError("Invalid object \"%s\"", TCHAR_TO_UTF8(*(obj->GetName())));
	}
	return 0;
}
