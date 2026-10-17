#ifndef _MPORT_LUA_H_
#define _MPORT_LUA_H_

#include <lauxlib.h>
#include <lualib.h>

#include "mport.h"
#include "mport_private.h"

#define RELATIVE_PATH(p) (p + (*p == '/' ? 1 : 0))
#define STREQ(s1, s2) (strcmp((s1), (s2)) == 0)

typedef enum {
	MPORT_LUA_PRE_INSTALL = 0,
	MPORT_LUA_POST_INSTALL,
	MPORT_LUA_PRE_DEINSTALL,
	MPORT_LUA_POST_DEINSTALL,
	MPORT_LUA_UNKNOWN
} mport_lua_script;

ucl_object_t *mport_lua_script_to_ucl(stringlist_t *scripts);
int mport_lua_script_from_ucl(
    mportInstance *mport, mportPackageMeta *pkg, const ucl_object_t *obj, mport_lua_script type);
int mport_lua_script_run(mportInstance *mport, mportPackageMeta *pkg, mport_lua_script type);
int mport_lua_script_load(mportInstance *mport, mportPackageMeta *pkg);
int mport_lua_script_read_file(
    mportInstance *mport, mportPackageMeta *pkg, mport_lua_script type, char *filename);

/* Shared Lua state setup for package scripts and triggers: the pkg library,
 * msgfd/rootfd, the pkg_* globals (pkg may be NULL for a transaction trigger)
 * and the io/os overrides. */
void mport_lua_state_setup(lua_State *L, mportInstance *mport, /*@null@*/ mportPackageMeta *pkg,
    int msgfd, bool upgrade, bool sandboxed);

/*
 * Triggers (triggers.c): UCL files in the directories named by the
 * MPORT_SETTING_TRIGGERS_DIR setting, same format as pkg-triggers(5).
 * Per-transaction triggers live directly in a trigger directory and run once
 * from mport_triggers_execute() with the matched directories as `arg`.
 * Per-package triggers live in a pre_install/, post_install/, pre_deinstall/
 * or post_deinstall/ subdirectory, match the package's own paths, and run at
 * that phase; their failure does not fail the package operation.
 */
typedef struct {
	/*@null@*/ char *script;
	bool sandbox;
} mportTriggerScript;

typedef struct {
	char *name;
	/*@null@*/ ucl_object_t *path;
	/*@null@*/ ucl_object_t *path_glob;
	/*@null@*/ ucl_object_t *path_regexp;
	mportTriggerScript script;
	mportTriggerScript cleanup;
	stringlist_t matched; /* paths matched so far, root-relative, unique */
} mportTrigger;

typedef tll(mportTrigger *) mportTriggerList;

struct _mportTriggerState {
	stringlist_t touched; /* directories changed this run, root-relative, unique */
	mportTriggerList cleanups; /* cleanup blocks of trigger files removed this run */
};

bool mport_triggers_enabled(mportInstance *);
void mport_triggers_get_dirs(mportInstance *, stringlist_t *);
/*@null@*/ mportTrigger *mport_trigger_load(
    mportInstance *, int dfd, const char *name, bool cleanup_only);
void mport_trigger_free(/*@null@*/ mportTrigger *);
/* subdir NULL loads per-transaction triggers; otherwise the named phase subdir */
int mport_triggers_load(mportInstance *, /*@null@*/ const char *subdir, mportTriggerList *);
bool mport_trigger_match(mportTrigger *, const char *path);
void mport_triggers_touch_dir(mportInstance *, const char *dir);
void mport_triggers_touch_file(mportInstance *, const char *file);
void mport_triggers_check_cleanup(mportInstance *, const char *file);
int mport_triggers_execute_perpackage(mportInstance *, mportPackageMeta *, mport_lua_script phase);
void mport_triggers_state_free(mportInstance *);
void mport_trigger_normalize_path(char *path);

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
