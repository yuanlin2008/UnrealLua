#pragma once

class FLuaEnv;

/**
 * Representing a lua object referenced by cpp side.
 */
class FLuaObject
{
public:
	/**
	 * Constructor.
	 * @param idx index of object on lua stack.
	 */
	FLuaObject(FLuaEnv* luaEnv, int idx);
	virtual ~FLuaObject();

protected:
	FLuaEnv* env_;
	int ref_;
};

/**
 * Lua table reference.
 */
class FLuaTable : public FLuaObject
{
public:
	FLuaTable(FLuaEnv* luaEnv, int idx);
};

/**
 * Lua closure reference.
 */
class FLuaClosure : public FLuaObject
{
public:
	FLuaClosure(FLuaEnv* luaEnv, int idx);
};