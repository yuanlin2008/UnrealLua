#pragma once

#include "CoreMinimal.h"
#include "LuaDelegate.generated.h"

UCLASS()
class UNREALLUA_API ULuaDelegate : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION()
	void invoke() {}

	virtual void ProcessEvent(UFunction* f, void* params) override;

	/** Object this delegate binded to. */
	FWeakObjectPtr bindedToObj;

	/** Property this delegate binded to. */
	UPROPERTY()
	UProperty* bindedToProp;

	/** Lua Env this delegate belongs to. */
	// todo: use TWeakPtr.
	class FLuaEnv* luaEnv;

	/** Lua object to call. */
	int luaObjRef;
};