#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <string.h>
#include <glib.h>
#include "lua_bhcd.h"
#include "dataset.h"
#include "dataset_gml.h"

static int datasetlua_load(lua_State * ss);
static int datasetlua_push(lua_State * ss, Dataset * dataset);
static Dataset * datasetlua_checkdataset(lua_State * ss);
static int datasetlua_gc(lua_State * ss);
static int datasetlua_tostring(lua_State * ss);
static int datasetlua_filename(lua_State * ss);
static int datasetlua_get(lua_State * ss);
static int datasetlua_labels(lua_State * ss);
static int datasetlua_elems(lua_State * ss);

static int treelua_share(lua_State * ss, const char * name, Tree * tree);
static int treelua_gc(lua_State *ss);
static int treelua_logprob(lua_State *ss);
static int treelua_logresp(lua_State *ss);
static int treelua_logpred(lua_State *ss);
static int treelua_is_leaf(lua_State *ss);
static int treelua_label(lua_State *ss);
static int treelua_children(lua_State *ss);
static int treelua_tostring(lua_State *ss);
static Tree * treelua_checktree(lua_State * ss);
static int treelua_push(lua_State * ss, Tree * tree);

static const struct luaL_reg datasetlua_g[] = {
	{ "load",	datasetlua_load },
	{ NULL, NULL },
};

static const struct luaL_reg datasetlua[] = {
	{ "__gc",	datasetlua_gc },
	{ "__tostring",	datasetlua_tostring },
	{ "filename",	datasetlua_filename },
	{ "get",	datasetlua_get },
	{ "labels",	datasetlua_labels },
	{ "elems",	datasetlua_elems },
	{ NULL, NULL },
};

static const struct luaL_reg treelua_g[] = {
	{ NULL, NULL },
};

/* perhaps should make separate tables for branches and leaves */
static const struct luaL_reg treelua[] = {
	{ "__gc",	treelua_gc },
	{ "__tostring",	treelua_tostring },
	{ "logprob",	treelua_logprob },
	{ "logresp",	treelua_logresp },
	{ "logpred",	treelua_logpred },
	{ "is_leaf",	treelua_is_leaf },
	{ "label",	treelua_label },
	{ "children",	treelua_children },

	{ NULL, NULL },
};

int luaopen_bhcd(lua_State * ss) {
	luaL_newmetatable(ss, "dataset");
	lua_pushstring(ss, "__index");
	lua_pushvalue(ss, -2);
	lua_settable(ss, -3);
	luaL_openlib(ss, NULL, datasetlua, 0);
	luaL_openlib(ss, "dataset", datasetlua_g, 0);

	luaL_newmetatable(ss, "tree");
	lua_pushstring(ss, "__index");
	lua_pushvalue(ss, -2);
	lua_settable(ss, -3);
	luaL_openlib(ss, NULL, treelua, 0);
	luaL_openlib(ss, "tree", treelua_g, 0);

	return 1;
}

static int datasetlua_load(lua_State * ss) {
	const char * filename = luaL_checkstring(ss, 1);
	Dataset * dataset = dataset_gml_load(filename);
	datasetlua_push(ss, dataset);
	dataset_unref(dataset);
	return 1;
}

static int datasetlua_push(lua_State * ss, Dataset * dataset) {
	Dataset ** datap = lua_newuserdata(ss, sizeof(*datap));
	luaL_getmetatable(ss, "dataset");
	if (!lua_setmetatable(ss, -2)) {
		g_warning("%s", lua_tostring(ss, - 1));
	}
	*datap = dataset;
	dataset_ref(dataset);
	return 1;
}

static int datasetlua_gc(lua_State * ss) {
	Dataset * dataset = datasetlua_checkdataset(ss);
	dataset_unref(dataset);
	return 1;
}

static Dataset * datasetlua_checkdataset(lua_State * ss) {
	void * dd = luaL_checkudata(ss, 1, "dataset");
	luaL_argcheck(ss, dd != NULL, 1, "`dataset' expected");
	return *(Dataset **)dd;
}

static int datasetlua_tostring(lua_State * ss) {
	Dataset * dataset = datasetlua_checkdataset(ss);
	GString * out = g_string_new("");
	dataset_tostring(dataset, out);
	lua_pushfstring(ss, "dataset(%s)", out->str);
	g_string_free(out, TRUE);
	return 1;
}

static int datasetlua_filename(lua_State * ss) {
	Dataset * dataset = datasetlua_checkdataset(ss);
	lua_pushstring(ss, dataset_get_filename(dataset));
	return 1;
}


static int datasetlua_get(lua_State * ss) {
	Dataset * dataset = datasetlua_checkdataset(ss);
	const char * src_str = luaL_checkstring(ss, 2);
	const char * dst_str = luaL_checkstring(ss, 3);
	gconstpointer src = dataset_label_lookup(dataset, src_str);
	gconstpointer dst = dataset_label_lookup(dataset, dst_str);
	gboolean missing = FALSE;
	gboolean value = dataset_get(dataset, src, dst, &missing);
	if (missing) {
		lua_pushnil(ss);
	} else {
		lua_pushboolean(ss, value);
	}
	return 1;
}

static int datasetlua_labels(lua_State * ss) {
	Dataset * dataset = datasetlua_checkdataset(ss);
	DatasetLabelIter iter;
	gpointer label;
	int index = 1;

	lua_newtable(ss);
	dataset_labels_iter_init(dataset, &iter);
	while (dataset_labels_iter_next(&iter, &label)) {
		lua_pushstring(ss, dataset_label_to_string(dataset, label));
		lua_rawseti(ss, -2, index);
		index++;
	}
	return 1;
}

static int datasetlua_elems(lua_State * ss) {
	Dataset * dataset = datasetlua_checkdataset(ss);
	DatasetPairIter pairs;
	gpointer lsrc, ldst;
	int index = 1;

	lua_newtable(ss);
	dataset_label_pairs_iter_init(dataset, &pairs);
	while (dataset_label_pairs_iter_next(&pairs, &lsrc, &ldst)) {
		const gchar * src = dataset_label_to_string(dataset, lsrc);
		const gchar * dst = dataset_label_to_string(dataset, ldst);
		lua_newtable(ss);
		lua_pushstring(ss, src);
		lua_rawseti(ss, -2, 1);
		lua_pushstring(ss, dst);
		lua_rawseti(ss, -2, 2);
		lua_rawseti(ss, -2, index);
		index++;
	}
	return 1;
}


static int treelua_share(lua_State * ss, const char * name, Tree * tree) {
	treelua_push(ss, tree);
	lua_setglobal(ss, name);
	return 1;
}

static int treelua_push(lua_State * ss, Tree * tree) {
	Tree ** treep = lua_newuserdata(ss, sizeof(*treep));
	luaL_getmetatable(ss, "tree");
	if (!lua_setmetatable(ss, -2)) {
		g_warning("%s", lua_tostring(ss, - 1));
	}
	*treep = tree;
	tree_ref(tree);
	return 1;
}


static Tree * treelua_checktree(lua_State * ss) {
	void * tt = luaL_checkudata(ss, 1, "tree");
	luaL_argcheck(ss, tt != NULL, 1, "`tree' expected");
	return *(Tree **)tt;
}

static int treelua_gc(lua_State * ss) {
	Tree * tree = treelua_checktree(ss);
	tree_unref(tree);
	return 1;
}

static int treelua_tostring(lua_State *ss) {
	Tree * tree = treelua_checktree(ss);
	GString * out = g_string_new("");
	tree_tostring(tree, out);
	lua_pushfstring(ss, "tree(%s)", out->str);
	g_string_free(out, TRUE);
	return 1;
}

static int treelua_logprob(lua_State *ss) {
	Tree * tree = treelua_checktree(ss);
	lua_pushnumber(ss, tree_get_logprob(tree));
	return 1;
}

static int treelua_logresp(lua_State *ss) {
	Tree * tree = treelua_checktree(ss);
	lua_pushnumber(ss, tree_get_logresponse(tree));
	return 1;
}

static int treelua_logpred(lua_State *ss) {
	Tree * tree = treelua_checktree(ss);
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

static int treelua_is_leaf(lua_State *ss) {
	Tree * tree = treelua_checktree(ss);
	lua_pushboolean(ss, tree_is_leaf(tree));
	return 1;
}

static int treelua_label(lua_State *ss) {
	Tree * tree = treelua_checktree(ss);
	Dataset * dataset = tree_get_params(tree)->dataset;
	gconstpointer label = leaf_get_label(tree);
	lua_pushstring(ss, dataset_label_to_string(dataset, label));
	return 1;
}

static int treelua_children(lua_State *ss) {
	Tree * tree = treelua_checktree(ss);
	guint index = 1;
	lua_newtable(ss);
	for (GList * child = branch_get_children(tree); child != NULL; child = g_list_next(child)) {
		treelua_push(ss, child->data);
		lua_rawseti(ss, -2, index);
		index++;
	}
	return 1;
}


void bhcd_lua_shell(Tree * tree) {
	char line[1024];
	lua_State * ss;
	int error;

	ss = lua_open();
	luaL_openlibs(ss);
	luaopen_bhcd(ss);

	treelua_share(ss, "res", tree);

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

