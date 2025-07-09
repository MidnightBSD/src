#ifndef _MPORT_LUA_H_
#define _MPORT_LUA_H_

#include <lauxlib.h>
#include <lualib.h>

#include "mport.h"
#include "mport_private.h"

#define RELATIVE_PATH(p) (p + (*p == '/' ? 1 : 0))
#define STREQ(s1, s2) (strcmp ((s1), (s2)) == 0)

typedef enum {
	MPORT_LUA_PRE_INSTALL = 0,
	MPORT_LUA_POST_INSTALL,
	MPORT_LUA_PRE_DEINSTALL,
	MPORT_LUA_POST_DEINSTALL,
	MPORT_LUA_UNKNOWN
} mport_lua_script;

ucl_object_t * mport_lua_script_to_ucl(stringlist_t *scripts);
int mport_lua_script_from_ucl(mportInstance *mport, mportPackageMeta *pkg, const ucl_object_t *obj, mport_lua_script type);
int mport_lua_script_run(mportInstance *mport, mportPackageMeta *pkg, mport_lua_script type);
int mport_lua_script_load(mportInstance *mport, mportPackageMeta *pkg);
int mport_lua_script_read_file(mportInstance *mport, mportPackageMeta *pkg, mport_lua_script type, char *filename);

lua_CFunction stack_dump(lua_State *L);
int lua_print_msg(lua_State *L);
int lua_pkg_copy(lua_State *L);
int lua_pkg_filecmp(lua_State *L);
int lua_pkg_symlink(lua_State *L);
int lua_prefix_path(lua_State *L);
int lua_exec(lua_State *L);
void lua_override_ios(lua_State *L, bool);
int lua_stat(lua_State *L);
int lua_readdir(lua_State *L);
void lua_args_table(lua_State *L, char **argv, int argc);


#endif
