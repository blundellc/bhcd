#ifndef	LUA_BHCD_H
#define	LUA_BHCD_H

#include <lua.h>
#include "tree.h"

int luaopen_bhcd(lua_State * ss);
void bhcd_lua_shell(Tree * tree);

#endif /*LUA_BHCD_H*/
