#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <string.h>
#include <glib.h>
#include "tree_lua.h"

static int treelib_gc(lua_State *ss);
static int treelib_logprob(lua_State *ss);
static int treelib_logresp(lua_State *ss);
static int treelib_logpred(lua_State *ss);
static int treelib_is_leaf(lua_State *ss);
static int treelib_label(lua_State *ss);
static int treelib_children(lua_State *ss);
static int treelib_tostring(lua_State *ss);
static Tree * treelib_checktree(lua_State * ss);
static int treelib_push(lua_State * ss, Tree * tree);

static const struct luaL_reg treelib_g[] = {
	{ NULL, NULL },
};

/* perhaps should make separate tables for branches and leaves */
static const struct luaL_reg treelib[] = {
	{ "__gc",	treelib_gc },
	{ "__tostring",	treelib_tostring },
	{ "logprob",	treelib_logprob },
	{ "logresp",	treelib_logresp },
	{ "logpred",	treelib_logpred },
	{ "is_leaf",	treelib_is_leaf },
	{ "label",	treelib_label },
	{ "children",	treelib_children },

	{ NULL, NULL },
};

static int treelib_open(lua_State * ss) {
	luaL_newmetatable(ss, "tree");

	/* set metatable */
	lua_pushstring(ss, "__index");
	lua_pushvalue(ss, -2);
	lua_settable(ss, -3);
	luaL_openlib(ss, NULL, treelib, 0);
	luaL_openlib(ss, "tree", treelib_g, 0);
	return 1;
}

static int treelib_share(lua_State * ss, const char * name, Tree * tree) {
	treelib_push(ss, tree);
	lua_setglobal(ss, name);
	return 1;
}

static int treelib_push(lua_State * ss, Tree * tree) {
	Tree ** treep = lua_newuserdata(ss, sizeof(*treep));
	luaL_getmetatable(ss, "tree");
	if (!lua_setmetatable(ss, -2)) {
		g_warning("%s", lua_tostring(ss, - 1));
	}
	*treep = tree;
	tree_ref(tree);
	return 1;
}


static Tree * treelib_checktree(lua_State * ss) {
	void * tt = luaL_checkudata(ss, 1, "tree");
	luaL_argcheck(ss, tt != NULL, 1, "`tree' expected");
	return *(Tree **)tt;
}

static int treelib_gc(lua_State * ss) {
	Tree * tree = treelib_checktree(ss);
	tree_unref(tree);
	return 1;
}

static int treelib_tostring(lua_State *ss) {
	Tree * tree = treelib_checktree(ss);
	GString * out = g_string_new("");
	tree_tostring(tree, out);
	lua_pushfstring(ss, "tree(%s)", out->str);
	g_string_free(out, TRUE);
	return 1;
}

static int treelib_logprob(lua_State *ss) {
	Tree * tree = treelib_checktree(ss);
	lua_pushnumber(ss, tree_get_logprob(tree));
	return 1;
}

static int treelib_logresp(lua_State *ss) {
	Tree * tree = treelib_checktree(ss);
	lua_pushnumber(ss, tree_get_logresponse(tree));
	return 1;
}

static int treelib_logpred(lua_State *ss) {
	Tree * tree = treelib_checktree(ss);
	Dataset * dataset = tree_get_params(tree)->dataset;
	const char * src_str = luaL_checkstring(ss, 2);
	const char * dst_str = luaL_checkstring(ss, 3);
	gconstpointer src = dataset_label_lookup(dataset, src_str);
	gconstpointer dst = dataset_label_lookup(dataset, dst_str);

	lua_newtable(ss);
	lua_pushnumber(ss, tree_logpredict(tree, src, dst, FALSE));
	lua_rawseti(ss, -2, 0);
	lua_pushnumber(ss, tree_logpredict(tree, src, dst, TRUE));
	lua_rawseti(ss, -2, 1);
	return 1;
}

static int treelib_is_leaf(lua_State *ss) {
	Tree * tree = treelib_checktree(ss);
	lua_pushboolean(ss, tree_is_leaf(tree));
	return 1;
}

static int treelib_label(lua_State *ss) {
	Tree * tree = treelib_checktree(ss);
	Dataset * dataset = tree_get_params(tree)->dataset;
	gconstpointer label = leaf_get_label(tree);
	lua_pushstring(ss, dataset_label_to_string(dataset, label));
	return 1;
}

static int treelib_children(lua_State *ss) {
	Tree * tree = treelib_checktree(ss);
	guint index = 1;
	lua_newtable(ss);
	for (GList * child = branch_get_children(tree); child != NULL; child = g_list_next(child)) {
		treelib_push(ss, child->data);
		lua_rawseti(ss, -2, index);
		index++;
	}
	return 1;
}


void tree_lua_shell(Tree * tree) {
	char line[1024];
	lua_State * ss;
	int error;

	ss = lua_open();
	luaL_openlibs(ss);
	treelib_open(ss);

	treelib_share(ss, "res", tree);

	while (fgets(line, sizeof(line), stdin) != NULL) {
		error = luaL_loadbuffer(ss, line, strlen(line), "line") ||
				lua_pcall(ss, 0, 1, 0);
		if (error) {
			g_warning("%s", lua_tostring(ss, - 1));
			lua_pop(ss, 1);
		}
		if (!lua_isnil(ss, -1)) {
			lua_getfield(ss, LUA_GLOBALSINDEX, "print");
			lua_pushvalue(ss, -2);
			error = lua_pcall(ss, 1, 0, 0);
			if (error) {
				g_warning("%s", lua_tostring(ss, - 1));
				lua_pop(ss, 1);
			}
			lua_settop(ss, 0);
		}
	}
	lua_close(ss);
}

