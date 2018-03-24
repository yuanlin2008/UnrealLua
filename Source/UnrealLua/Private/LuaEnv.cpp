#include "LuaEnv.h"
#include "UObjectIterator.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "lua.hpp"

TMap<lua_State*, FLuaEnv*> FLuaEnv::luaEnvMap_;

struct FUObjectProxy
{
	UObject* ptr;
};

struct FUStructProxy
{
	UScriptStruct* type;
	void* ptr;
};

FLuaEnv::FLuaEnv():
	luaState_(nullptr),
	memUsed_(0),
	uobjTable_(LUA_NOREF),
	luaObjTable_(LUA_NOREF),
	luaObjRefInfoTable_(LUA_NOREF)
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
	luaL_newmetatable(luaState_, "UObjectMT");
	lua_pushcfunction(luaState_, LUA_CALLBACK(uobjMTIndex));
	lua_setfield(luaState_, -2, "__index");
	lua_pushcfunction(luaState_, LUA_CALLBACK(uobjMTNewIndex));
	lua_setfield(luaState_, -2, "__newindex");
	lua_pushcfunction(luaState_, LUA_CALLBACK(uobjMTCall));
	lua_setfield(luaState_, -2, "__call");
	lua_pop(luaState_, 1);

	// Create UStruct proxy metatable.
	luaL_newmetatable(luaState_, "UStructMT");
	lua_pushcfunction(luaState_, LUA_CALLBACK(ustructMTIndex));
	lua_setfield(luaState_, -2, "__index");
	lua_pushcfunction(luaState_, LUA_CALLBACK(ustructMTNewIndex));
	lua_setfield(luaState_, -2, "__newindex");
	lua_pushcfunction(luaState_, LUA_CALLBACK(ustructMTGC));
	lua_setfield(luaState_, -2, "__gc");
	lua_pop(luaState_, 1);

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
	Collector.AllowEliminatingReferences(false);
	// Iterate all referenced UObject from uobjTable.
	lua_rawgeti(luaState_, LUA_REGISTRYINDEX, uobjTable_);
	lua_pushnil(luaState_);
	while(lua_next(luaState_, -2) != 0)
	{
		UObject* uobj = (UObject*)lua_touserdata(luaState_, -2);
		if(uobj)
			Collector.AddReferencedObject(uobj);
		lua_pop(luaState_, 1);
	}
	lua_pop(luaState_, 1);
	// Iterate all structs.
	for(auto& it : structs_)
	{
		UObject* uobj = it;
		Collector.AddReferencedObject(uobj);
	}
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

struct FFuncParamStruct
{
	FFuncParamStruct(UFunction* f, void* b):
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

	~FFuncParamStruct()
	{
		for(int i = 0; i < parmNum; i++)
			parms[i]->DestroyValue_InContainer(buffer);
		if(retParm)
			retParm->DestroyValue_InContainer(buffer);
		ULUA_LOG(Verbose, TEXT("FFuncParamStruct destructed."));
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
	UObject* obj = isStaticFunc ? cls->GetDefaultObject() : checkUObject(2, cls);
	if(!obj)
	{
		throwError("Invalid self UObject");
	}

	// Create param buffer from current stack.
	uint8* paramBuffer = (uint8*)FMemory_Alloca(func->ParmsSize);

	// Initialize param buffer.
	FFuncParamStruct params(func, paramBuffer);

	// Get function parameter value from lua stack.
	for(int i = 0; i < params.parmNum; i++)
	{
		checkPropertyValue(paramBuffer, params.parms[i], paramIdx);
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

void FLuaEnv::checkPropertyValue(void* obj, UProperty* prop, int idx)
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
		p->SetObjectPropertyValue_InContainer(obj, checkUObject(idx, p->PropertyClass));
	}
	else if(auto p = Cast<UInterfaceProperty>(prop))
	{
		UObject* o = checkUObject(idx);
		void* iaddr = o?o->GetInterfaceAddress(p->InterfaceClass):nullptr;
		if(iaddr)
			p->SetPropertyValue_InContainer(obj, FScriptInterface(o, iaddr));
		else
			p->SetPropertyValue_InContainer(obj, FScriptInterface());
	}
	else if(auto p = Cast<UNameProperty>(prop))
		p->SetPropertyValue_InContainer(obj, checkFName(idx));
	else if(auto p = Cast<UStrProperty>(prop))
		p->SetPropertyValue_InContainer(obj, checkFString(idx));
	else if(auto p = Cast<UArrayProperty>(prop))
	{
		luaL_checktype(luaState_, idx, LUA_TTABLE);
		int luaArrLen = lua_rawlen(luaState_, idx);

		FScriptArrayHelper_InContainer cppArr(p, obj);
		int cppArrLen = cppArr.Num();
		// resize array.
		if(cppArrLen < luaArrLen)
			cppArr.AddValues(luaArrLen - cppArrLen);
		else if(cppArrLen > luaArrLen)
			cppArr.RemoveValues(luaArrLen, cppArrLen - luaArrLen);

		for(int i = 0; i < luaArrLen; i++)
		{
			lua_rawgeti(luaState_, idx, i+1);
			checkPropertyValue(cppArr.GetRawPtr(i), p->Inner, lua_gettop(luaState_));
			lua_pop(luaState_, 1);
		}
	}
	else if(auto p = Cast<UMapProperty>(prop))
	{
		luaL_checktype(luaState_, idx, LUA_TTABLE);

		FScriptMapHelper_InContainer cppMap(p, obj);
		cppMap.EmptyValues();

		lua_pushnil(luaState_);
		while(lua_next(luaState_, idx) != 0)
		{
			int elementId = cppMap.AddDefaultValue_Invalid_NeedsRehash();
			uint8* pairPtr = cppMap.GetPairPtr(elementId);
			checkPropertyValue(pairPtr + p->MapLayout.KeyOffset, p->KeyProp, lua_gettop(luaState_) - 1);
			checkPropertyValue(pairPtr, p->ValueProp, lua_gettop(luaState_));
			lua_pop(luaState_, 1);
		}
		cppMap.Rehash();
	}
	else if(auto p = Cast<USetProperty>(prop))
	{
		luaL_checktype(luaState_, idx, LUA_TTABLE);

		FScriptSetHelper_InContainer cppSet(p, obj);
		cppSet.EmptyElements();

		lua_pushnil(luaState_);
		while(lua_next(luaState_, idx) != 0)
		{
			int elementId = cppSet.AddDefaultValue_Invalid_NeedsRehash();
			uint8* elementPtr = cppSet.GetElementPtr(elementId);
			checkPropertyValue(elementPtr, p->ElementProp, lua_gettop(luaState_) - 1);
			lua_pop(luaState_, 1);
		}
		cppSet.Rehash();
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
		p->SetPropertyValue_InContainer(obj, checkFText(idx));
	else if(auto p = Cast<UEnumProperty>(prop))
	{
		// todo.
	}
}

UObject* FLuaEnv::checkUObject(int idx, UClass* cls)
{
	if(lua_isnil(luaState_, idx))
		return nullptr;
	FUObjectProxy* p = (FUObjectProxy*)luaL_checkudata(luaState_, idx, "UObjectMT");
	UObject* o = p->ptr;
	if(cls == nullptr || o->IsA(cls))
		return o;
	else
	{
		throwError("Invalid UObject type, \"%s\" needed.", UTF8_TO_TCHAR(*(cls->GetName())));
	}
	return nullptr;
}

void* FLuaEnv::checkUStruct(int idx, UScriptStruct* structType)
{
	FUStructProxy* p = (FUStructProxy*)luaL_checkudata(luaState_, idx, "UStructMT");
	if(p->type == structType)
		return p->ptr;
	throwError("Invalid UStruct type, \"%s\" needed.", UTF8_TO_TCHAR(*(p->type->GetName())));
	return nullptr;
}

FString FLuaEnv::checkFString(int idx)
{
	return UTF8_TO_TCHAR(luaL_checkstring(luaState_, idx));
}

FText FLuaEnv::checkFText(int idx)
{
	return FText::FromString(UTF8_TO_TCHAR(luaL_checkstring(luaState_, idx)));
}

FName FLuaEnv::checkFName(int idx)
{
	return UTF8_TO_TCHAR(luaL_checkstring(luaState_, idx));
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
		p->ptr = obj;
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

		// set metatable.
		luaL_setmetatable(luaState_, "UObjectMT");
	}
	else
	{
		lua_replace(luaState_, -2);
	}
}

void FLuaEnv::pushUStruct(void* structPtr, UScriptStruct* structType)
{
	structs_.Add(structType);

	FUStructProxy* p = (FUStructProxy*)lua_newuserdata(luaState_, sizeof(FUStructProxy) + structType->GetStructureSize());
	p->type = structType;
	p->ptr = p + 1;

	p->type->InitializeStruct(p->ptr);
	p->type->CopyScriptStruct(p->ptr, structPtr);
	// set metatable.
	luaL_setmetatable(luaState_, "UStructMT");
}

void FLuaEnv::pushFString(const FString& str)
{
	lua_pushstring(luaState_, TCHAR_TO_UTF8(*str));
}

void FLuaEnv::pushFText(const FText& txt)
{
	pushFString(txt.ToString());
}

void FLuaEnv::pushFName(FName name)
{
	// todo: optimize?
	pushFString(name.ToString());
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
	FUObjectProxy* p = (FUObjectProxy*)lua_touserdata(luaState_, 1);
	UObject* obj = p->ptr;
	FName name = checkFName(2);
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
	FUObjectProxy* p = (FUObjectProxy*)lua_touserdata(luaState_, 1);
	UObject* obj = p->ptr;
	FName name = checkFName(2);
	// todo: optimize.
	UProperty* prop = FindField<UProperty>(obj->GetClass(), name);
	if (prop)
	{
		checkPropertyValue(obj, prop, 3);
	}
	else
	{
		throwError("Invalid field name \"%s\"", TCHAR_TO_UTF8(*name.ToString()));
	}
	return 0;
}

int FLuaEnv::uobjMTCall()
{
	FUObjectProxy* p = (FUObjectProxy*)lua_touserdata(luaState_, 1);
	UObject* obj = p->ptr;
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

int FLuaEnv::ustructMTIndex()
{
	FUStructProxy* p = (FUStructProxy*)lua_touserdata(luaState_, 1);
	FName name = checkFName(2);
	// todo: optimize.
	UProperty* prop = FindField<UProperty>(p->type, name);
	if (prop)
	{
		// Return property value.
		pushPropertyValue(p->ptr, prop);
	}
	else
	{
		throwError("Invalid field name %s", TCHAR_TO_UTF8(*name.ToString()));
	}
	return 1;
}

int FLuaEnv::ustructMTNewIndex()
{
	FUStructProxy* p = (FUStructProxy*)lua_touserdata(luaState_, 1);
	FName name = checkFName(2);
	// todo: optimize.
	UProperty* prop = FindField<UProperty>(p->type, name);
	if (prop)
	{
		// Return property value.
		checkPropertyValue(p->ptr, prop, 3);
	}
	else
	{
		throwError("Invalid field name %s", TCHAR_TO_UTF8(*name.ToString()));
	}
	return 0;
}

int FLuaEnv::ustructMTGC()
{
	FUStructProxy* p = (FUStructProxy*)lua_touserdata(luaState_, 1);
	p->type->DestroyStruct(p->ptr);
	ULUA_LOG(Verbose, TEXT("Struct \"%s\" destroyed."), (*(p->type->GetName())));
	return 0;
}
