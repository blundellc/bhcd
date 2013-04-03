#ifndef	TREE_LUA_H
#define	TREE_LUA_H

#include <lua.h>
#include "tree.h"

int luaopen_nrt(lua_State * ss);
void nrt_lua_shell(Tree * tree);

#endif /*TREE_LUA_H*/
