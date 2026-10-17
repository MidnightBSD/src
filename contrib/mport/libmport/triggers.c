/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
 * Copyright (c) 2020-2026 Baptiste Daroussin <bapt@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Package triggers, after pkg-triggers(5).
 *
 * A trigger is a UCL file naming directories (exact, glob or regexp) and a
 * Lua script.  While packages are installed or removed the parent directory
 * of every file and every directory asset is recorded on the instance;
 * mport_triggers_execute() then runs each trigger whose patterns match a
 * recorded directory, once, with the matches as `arg`.  A trigger file that
 * is itself removed has its `cleanup` block queued at removal time and run
 * first, since the file is gone by then.
 *
 * Per-package triggers sit in a phase subdirectory and match the package's
 * own paths instead; they run inline at that phase and never fail the
 * package.
 *
 * All paths handled here are root-relative (no mport->root prefix), which is
 * what the trigger files name and what the scripts see through rootfd.
 */

#ifdef HAVE_CAPSICUM
#include <sys/capsicum.h>
#endif

#include <sys/stat.h>
#include <sys/wait.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mport_lua.h"

#define MPORT_TRIGGER_EXT ".ucl"

static const char *const trigger_phase_dirs[] = {
	[MPORT_LUA_PRE_INSTALL] = "pre_install",
	[MPORT_LUA_POST_INSTALL] = "post_install",
	[MPORT_LUA_PRE_DEINSTALL] = "pre_deinstall",
	[MPORT_LUA_POST_DEINSTALL] = "post_deinstall",
};

static const char trigger_schema_str[] = "{"
					 "  type = object;"
					 "  properties {"
					 "    description: { type = string };"
					 "    path: { "
					 "      anyOf = [{"
					 "        type = array; "
					 "        item = { type = string };"
					 "      }, {"
					 "        type = string;"
					 "      }]"
					 "    };"
					 "    path_glob: { "
					 "      anyOf = [{"
					 "        type = array; "
					 "        item = { type = string };"
					 "      }, {"
					 "        type = string;"
					 "      }]"
					 "    };"
					 "    path_regexp: { "
					 "      anyOf = [{"
					 "        type = array; "
					 "        item = { type = string };"
					 "      }, {"
					 "        type = string;"
					 "      }]"
					 "    };"
					 "    cleanup = { "
					 "      type = object; "
					 "      properties = {"
					 "        type = { type = string, enum: [lua] };"
					 "        sandbox = { type = boolean };"
					 "        script = { type = string };"
					 "      }; "
					 "      required = [ type, script ];"
					 "    };"
					 "    trigger = { "
					 "      type = object; "
					 "      properties = {"
					 "        type = { type = string, enum: [lua] };"
					 "        sandbox = { type = boolean };"
					 "        script = { type = string };"
					 "      }; "
					 "      required = [ type, script ];"
					 "    };"
					 "  }\n"
					 "  required = [ trigger ];"
					 "}";

static /*@null@*/ ucl_object_t *trigger_open_schema(void);
static bool parse_script_block(
    const ucl_object_t *, const char *, const char *, mportTriggerScript *);
static bool stringlist_contains(const stringlist_t *, const char *);
static bool stringlist_add_unique(stringlist_t *, const char *);
static bool trigger_add_match(mportTrigger *, const char *);
static int trigger_run_script(mportInstance *, const char *, const mportTriggerScript *,
    const stringlist_t *, /*@null@*/ mportPackageMeta *, bool);
static int collect_package_paths(mportInstance *, mportPackageMeta *, const char *, stringlist_t *);
static /*@null@*/ struct _mportTriggerState *trigger_state(mportInstance *);

static void
free_string(char *s)
{
	free(s);
}

static void
free_trigger_cb(mportTrigger *t)
{
	mport_trigger_free(t);
}

/*
 * Collapse repeated slashes and drop a trailing one, so that the path a
 * trigger file names compares equal to the one an asset produced.
 */
void
mport_trigger_normalize_path(char *path)
{
	char *r, *w;

	if (path == NULL || path[0] == '\0')
		return;

	for (r = w = path; *r != '\0'; r++) {
		if (*r == '/' && w > path && w[-1] == '/')
			continue;
		*w++ = *r;
	}
	*w = '\0';
	if (w - path > 1 && w[-1] == '/')
		w[-1] = '\0';
}

bool
mport_triggers_enabled(mportInstance *mport)
{
	char *val;
	bool enabled;

	val = mport_setting_get(mport, MPORT_SETTING_TRIGGERS_ENABLE);
	/* a registry predating the setting behaves as if it were set to yes */
	enabled = (val == NULL) ? true : mport_check_answer_bool(val);
	free(val);

	return enabled;
}

void
mport_triggers_get_dirs(mportInstance *mport, stringlist_t *dirs)
{
	char *val;

	val = mport_setting_get(mport, MPORT_SETTING_TRIGGERS_DIR);
	if (val == NULL || val[0] == '\0') {
		free(val);
		val = strdup(MPORT_TRIGGERS_DIR_DEFAULT);
		if (val == NULL)
			return;
	}
	mport_parselist_tll(val, dirs);
	free(val);

	tll_foreach(*dirs, it)
	{
		mport_trigger_normalize_path(it->item);
	}
}

static /*@null@*/ ucl_object_t *
trigger_open_schema(void)
{
	struct ucl_parser *parser;
	ucl_object_t *schema = NULL;

	parser = ucl_parser_new(0);
	if (parser == NULL)
		return (NULL);
	if (ucl_parser_add_string(parser, trigger_schema_str, sizeof(trigger_schema_str) - 1))
		schema = ucl_parser_get_object(parser);
	ucl_parser_free(parser);

	return (schema);
}

static bool
parse_script_block(const ucl_object_t *block, const char *block_name, const char *trigger_name,
    mportTriggerScript *out)
{
	const ucl_object_t *o;
	const char *type;

	o = ucl_object_lookup(block, "type");
	type = (o == NULL) ? NULL : ucl_object_tostring(o);
	if (type == NULL) {
		SET_ERRORX(MPORT_ERR_FATAL, "%s block of trigger %s has no script type", block_name,
		    trigger_name);
		return (false);
	}
	if (strcasecmp(type, "lua") != 0) {
		SET_ERRORX(MPORT_ERR_FATAL, "Unknown script type '%s' in %s block of trigger %s",
		    type, block_name, trigger_name);
		return (false);
	}
	o = ucl_object_lookup(block, "script");
	if (o == NULL || ucl_object_tostring(o) == NULL) {
		SET_ERRORX(MPORT_ERR_FATAL, "No script in %s block of trigger %s", block_name,
		    trigger_name);
		return (false);
	}
	out->script = strdup(ucl_object_tostring(o));
	if (out->script == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "Out of memory.");
		return (false);
	}
	o = ucl_object_lookup(block, "sandbox");
	out->sandbox = (o == NULL) ? true : ucl_object_toboolean(o);

	return (true);
}

/*
 * Load one trigger file from the directory open on dfd.  With cleanup_only
 * just the cleanup block is read (NULL when the file has none); otherwise the
 * trigger block and its path patterns are required.  Errors are set and NULL
 * returned; a trigger that fails to load is skipped, not fatal.
 */
/*@null@*/ mportTrigger *
mport_trigger_load(mportInstance *mport, int dfd, const char *name, bool cleanup_only)
{
	struct ucl_parser *parser;
	ucl_object_t *obj = NULL;
	ucl_object_t *schema;
	const ucl_object_t *block;
	struct ucl_schema_error err;
	mportTrigger *t;
	int fd;

	(void)mport;

	fd = openat(dfd, name, O_RDONLY | O_CLOEXEC);
	if (fd == -1) {
		SET_ERRORX(MPORT_ERR_FATAL, "Unable to open trigger %s: %s", name, strerror(errno));
		return (NULL);
	}

	parser = ucl_parser_new(0);
	if (parser == NULL) {
		close(fd);
		SET_ERROR(MPORT_ERR_FATAL, "Out of memory.");
		return (NULL);
	}
	if (!ucl_parser_add_fd(parser, fd)) {
		SET_ERRORX(MPORT_ERR_FATAL, "Unable to parse trigger %s: %s", name,
		    ucl_parser_get_error(parser));
		ucl_parser_free(parser);
		close(fd);
		return (NULL);
	}
	close(fd);
	obj = ucl_parser_get_object(parser);
	ucl_parser_free(parser);
	if (obj == NULL) {
		SET_ERRORX(MPORT_ERR_FATAL, "Trigger %s is empty", name);
		return (NULL);
	}

	schema = trigger_open_schema();
	if (schema == NULL) {
		ucl_object_unref(obj);
		SET_ERROR(MPORT_ERR_FATAL, "Unable to build the trigger schema");
		return (NULL);
	}
	if (!ucl_object_validate(schema, obj, &err)) {
		SET_ERRORX(MPORT_ERR_FATAL, "Trigger %s cannot be validated: %s", name, err.msg);
		ucl_object_unref(schema);
		ucl_object_unref(obj);
		return (NULL);
	}
	ucl_object_unref(schema);

	t = calloc(1, sizeof(*t));
	if (t == NULL) {
		ucl_object_unref(obj);
		SET_ERROR(MPORT_ERR_FATAL, "Out of memory.");
		return (NULL);
	}
	t->name = strdup(name);
	if (t->name == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "Out of memory.");
		goto err;
	}

	if (cleanup_only) {
		block = ucl_object_lookup(obj, "cleanup");
		if (block == NULL)
			goto err; /* nothing to run; not an error */
		if (!parse_script_block(block, "cleanup", name, &t->cleanup))
			goto err;
		ucl_object_unref(obj);
		return (t);
	}

	block = ucl_object_lookup(obj, "trigger");
	if (block == NULL || !parse_script_block(block, "trigger", name, &t->script))
		goto err;

	block = ucl_object_lookup(obj, "path");
	if (block != NULL)
		t->path = ucl_object_ref(block);
	block = ucl_object_lookup(obj, "path_glob");
	if (block != NULL)
		t->path_glob = ucl_object_ref(block);
	block = ucl_object_lookup(obj, "path_regexp");
	if (block != NULL)
		t->path_regexp = ucl_object_ref(block);
	if (t->path == NULL && t->path_glob == NULL && t->path_regexp == NULL) {
		SET_ERRORX(
		    MPORT_ERR_FATAL, "No path, path_glob or path_regexp in trigger %s", name);
		goto err;
	}

	ucl_object_unref(obj);
	return (t);

err:
	mport_trigger_free(t);
	ucl_object_unref(obj);
	return (NULL);
}

void
mport_trigger_free(/*@null@*/ mportTrigger *t)
{
	if (t == NULL)
		return;
	free(t->name);
	if (t->path != NULL)
		ucl_object_unref(t->path);
	if (t->path_glob != NULL)
		ucl_object_unref(t->path_glob);
	if (t->path_regexp != NULL)
		ucl_object_unref(t->path_regexp);
	free(t->script.script);
	free(t->cleanup.script);
	tll_free_and_free(t->matched, free_string);
	free(t);
}

/*
 * Append every well-formed *.ucl regular file in dir (root-relative) to list.
 * A missing directory is not an error; a bad file is reported and skipped.
 */
static void
triggers_load_from(mportInstance *mport, const char *dir, mportTriggerList *list)
{
	int dfd;
	DIR *d;
	struct dirent *e;
	struct stat st;
	const char *rel;

	rel = RELATIVE_PATH(dir);
	if (rel[0] == '\0')
		return;

	dfd = openat(mport->rootfd, rel, O_DIRECTORY | O_RDONLY | O_CLOEXEC);
	if (dfd == -1) {
		if (errno != ENOENT && errno != ENOTDIR)
			mport_call_msg_cb(
			    mport, "Unable to open trigger directory %s: %s", dir, strerror(errno));
		return;
	}
	d = fdopendir(dfd);
	if (d == NULL) {
		mport_call_msg_cb(
		    mport, "Unable to read trigger directory %s: %s", dir, strerror(errno));
		close(dfd);
		return;
	}

	while ((e = readdir(d)) != NULL) {
		const char *ext;
		mportTrigger *t;

		if (e->d_name[0] == '.')
			continue;
		ext = strrchr(e->d_name, '.');
		if (ext == NULL || strcmp(ext, MPORT_TRIGGER_EXT) != 0)
			continue;
		if (fstatat(dirfd(d), e->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0 ||
		    !S_ISREG(st.st_mode))
			continue;
		t = mport_trigger_load(mport, dirfd(d), e->d_name, false);
		if (t == NULL) {
			mport_call_msg_cb(mport, "Skipping trigger %s/%s: %s", dir, e->d_name,
			    mport_err_string());
			continue;
		}
		tll_push_back(*list, t);
	}
	closedir(d);
}

int
mport_triggers_load(mportInstance *mport, /*@null@*/ const char *subdir, mportTriggerList *list)
{
	stringlist_t dirs = tll_init();
	char path[FILENAME_MAX];

	mport_triggers_get_dirs(mport, &dirs);
	tll_foreach(dirs, it)
	{
		if (subdir == NULL) {
			triggers_load_from(mport, it->item, list);
		} else {
			(void)snprintf(path, sizeof(path), "%s/%s", it->item, subdir);
			triggers_load_from(mport, path, list);
		}
	}
	tll_free_and_free(dirs, free_string);

	return (MPORT_OK);
}

static bool
stringlist_contains(const stringlist_t *list, const char *s)
{
	tll_foreach(*list, it)
	{
		if (strcmp(it->item, s) == 0)
			return (true);
	}
	return (false);
}

/* true when added; false when already present or out of memory */
static bool
stringlist_add_unique(stringlist_t *list, const char *s)
{
	char *copy;

	if (stringlist_contains(list, s))
		return (false);
	copy = strdup(s);
	if (copy == NULL)
		return (false);
	tll_push_back(*list, copy);
	return (true);
}

static bool
trigger_add_match(mportTrigger *t, const char *path)
{
	(void)stringlist_add_unique(&t->matched, path);
	return (true);
}

/*
 * Match a root-relative path against the trigger's patterns, recording it in
 * the trigger's matched list.  Returns whether it matched.
 */
bool
mport_trigger_match(mportTrigger *t, const char *path)
{
	const ucl_object_t *cur;
	ucl_object_iter_t it;

	if (t->path != NULL) {
		it = NULL;
		while ((cur = ucl_iterate_object(t->path, &it, true)) != NULL) {
			const char *p = ucl_object_tostring(cur);
			char norm[FILENAME_MAX];

			if (p == NULL)
				continue;
			(void)strlcpy(norm, p, sizeof(norm));
			mport_trigger_normalize_path(norm);
			if (strcmp(norm, path) == 0)
				return (trigger_add_match(t, path));
		}
	}

	if (t->path_glob != NULL) {
		it = NULL;
		while ((cur = ucl_iterate_object(t->path_glob, &it, true)) != NULL) {
			const char *p = ucl_object_tostring(cur);

			if (p != NULL && fnmatch(p, path, 0) == 0)
				return (trigger_add_match(t, path));
		}
	}

	if (t->path_regexp != NULL) {
		it = NULL;
		while ((cur = ucl_iterate_object(t->path_regexp, &it, true)) != NULL) {
			const char *p = ucl_object_tostring(cur);
			regex_t re;
			int rc;

			if (p == NULL)
				continue;
			if (regcomp(&re, p, REG_EXTENDED | REG_NOSUB) != 0)
				continue;
			rc = regexec(&re, path, 0, NULL, 0);
			regfree(&re);
			if (rc == 0)
				return (trigger_add_match(t, path));
		}
	}

	return (false);
}

static /*@null@*/ struct _mportTriggerState *
trigger_state(mportInstance *mport)
{
	if (mport->triggers == NULL) {
		mport->triggers = calloc(1, sizeof(*mport->triggers));
		/* calloc's zeroes are tll_init() for both lists */
		if (mport->triggers == NULL)
			return (NULL);
	}
	return (mport->triggers);
}

void
mport_triggers_touch_dir(mportInstance *mport, const char *dir)
{
	struct _mportTriggerState *state;
	char norm[FILENAME_MAX];

	if (dir == NULL || dir[0] == '\0')
		return;
	state = trigger_state(mport);
	if (state == NULL)
		return;
	(void)strlcpy(norm, dir, sizeof(norm));
	mport_trigger_normalize_path(norm);
	(void)stringlist_add_unique(&state->touched, norm);
}

void
mport_triggers_touch_file(mportInstance *mport, const char *file)
{
	char dir[FILENAME_MAX];
	char *slash;

	if (file == NULL || file[0] == '\0')
		return;
	(void)strlcpy(dir, file, sizeof(dir));
	mport_trigger_normalize_path(dir);
	slash = strrchr(dir, '/');
	if (slash == NULL)
		return;
	if (slash == dir)
		slash[1] = '\0'; /* a file in the root: its directory is "/" */
	else
		*slash = '\0';
	mport_triggers_touch_dir(mport, dir);
}

/*
 * Called with a root-relative file about to be removed.  When it is a trigger
 * file, its cleanup block is read now and queued for mport_triggers_execute().
 */
void
mport_triggers_check_cleanup(mportInstance *mport, const char *file)
{
	stringlist_t dirs = tll_init();
	struct _mportTriggerState *state;
	char norm[FILENAME_MAX];
	const char *ext;

	if (file == NULL)
		return;
	ext = strrchr(file, '.');
	if (ext == NULL || strcmp(ext, MPORT_TRIGGER_EXT) != 0)
		return;

	(void)strlcpy(norm, file, sizeof(norm));
	mport_trigger_normalize_path(norm);

	mport_triggers_get_dirs(mport, &dirs);
	tll_foreach(dirs, it)
	{
		size_t len = strlen(it->item);
		const char *rel;
		int dfd;
		mportTrigger *t;

		if (len == 0 || strncmp(norm, it->item, len) != 0 || norm[len] != '/')
			continue;
		rel = norm + len + 1;
		if (rel[0] == '\0')
			continue;

		dfd = openat(
		    mport->rootfd, RELATIVE_PATH(it->item), O_DIRECTORY | O_RDONLY | O_CLOEXEC);
		if (dfd == -1)
			continue;
		t = mport_trigger_load(mport, dfd, rel, true);
		close(dfd);
		if (t == NULL)
			continue;
		state = trigger_state(mport);
		if (state == NULL) {
			mport_trigger_free(t);
			break;
		}
		tll_push_back(state->cleanups, t);
		break;
	}
	tll_free_and_free(dirs, free_string);
}

void
mport_triggers_state_free(mportInstance *mport)
{
	if (mport->triggers == NULL)
		return;
	tll_free_and_free(mport->triggers->touched, free_string);
	tll_free_and_free(mport->triggers->cleanups, free_trigger_cb);
	free(mport->triggers);
	mport->triggers = NULL;
}

/*
 * Run one trigger script in a child: a fresh Lua state with the pkg library,
 * `arg` set to args, and the capsicum sandbox entered when requested.  Output
 * written through pkg.print_msg is relayed to the message callback.
 */
static int
trigger_run_script(mportInstance *mport, const char *name, const mportTriggerScript *s,
    const stringlist_t *args, /*@null@*/ mportPackageMeta *pkg, bool upgrade)
{
	int cur_pipe[2];
	int pstat = 0;
	int ret;
	pid_t pid;

	if (s->script == NULL)
		return (MPORT_OK);

	if (get_socketpair(cur_pipe) == -1)
		RETURN_ERROR(MPORT_ERR_FATAL, "socket pair failed");

	/* the script's print() flushes stdout in the child; do not let it
	 * flush a copy of our own pending output as well */
	(void)fflush(stdout);
	(void)fflush(stderr);
	pid = fork();
	if (pid == 0) {
		lua_State *L;
		char **argv = NULL;
		int argc = 0;

		close(cur_pipe[0]);
		L = luaL_newstate();
		luaL_openlibs(L);
		lua_atpanic(L, (lua_CFunction)stack_dump);
		mport_lua_state_setup(L, mport, pkg, cur_pipe[1], upgrade, s->sandbox);

		if (args != NULL && tll_length(*args) > 0) {
			argv = (char **)calloc(tll_length(*args), sizeof(char *));
			if (argv != NULL) {
				tll_foreach(*args, it)
				{
					argv[argc++] = it->item;
				}
			}
		}
		lua_args_table(L, argv, argc);
		free((void *)argv);

#ifdef HAVE_CAPSICUM
		if (s->sandbox) {
			if (cap_enter() < 0 && errno != ENOSYS) {
				dprintf(cur_pipe[1], "trigger %s: cap_enter failed: %s\n", name,
				    strerror(errno));
				lua_close(L);
				_exit(1);
			}
		}
#endif

		if (luaL_dostring(L, s->script)) {
			dprintf(cur_pipe[1], "trigger %s: failed to execute lua script: %s\n", name,
			    lua_tostring(L, -1));
			lua_close(L);
			_exit(1);
		}
		if (lua_tonumber(L, -1) != 0) {
			lua_close(L);
			_exit(1);
		}
		lua_close(L);
		_exit(0);
	} else if (pid < 0) {
		close(cur_pipe[0]);
		close(cur_pipe[1]);
		RETURN_ERROR(MPORT_ERR_FATAL, "Cannot fork trigger script");
	}

	close(cur_pipe[1]);
	ret = mport_script_run_child(mport, pid, &pstat, cur_pipe[0], name);
	close(cur_pipe[0]);

	return (ret);
}

/*
 * Run the queued cleanup scripts, then every per-transaction trigger that
 * matches a directory touched since the last call.  Every matching trigger
 * is attempted even if an earlier one fails, so one broken trigger does not
 * leave the others' caches stale; the first error is returned.
 */
int
mport_triggers_execute(mportInstance *mport)
{
	struct _mportTriggerState *state;
	mportTriggerList triggers = tll_init();
	stringlist_t empty = tll_init();
	int ret = MPORT_OK;
	int err = MPORT_OK;

	if (mport == NULL || mport->triggers == NULL)
		return (MPORT_OK);
	state = mport->triggers;

	if (!mport_triggers_enabled(mport)) {
		mport_triggers_state_free(mport);
		return (MPORT_OK);
	}

	tll_foreach(state->cleanups, it)
	{
		mportTrigger *t = it->item;

		if (mport->verbosity >= MPORT_VNORMAL)
			mport_call_msg_cb(mport, "Running cleanup trigger: %s", t->name);
		ret = trigger_run_script(mport, t->name, &t->cleanup, &empty, NULL, false);
		if (ret != MPORT_OK) {
			mport_call_msg_cb(
			    mport, "Cleanup trigger %s failed: %s", t->name, mport_err_string());
			err = ret;
		}
	}

	if (tll_length(state->touched) > 0) {
		(void)mport_triggers_load(mport, NULL, &triggers);

		tll_foreach(triggers, tit)
		{
			mportTrigger *t = tit->item;

			tll_foreach(state->touched, dit)
			{
				(void)mport_trigger_match(t, dit->item);
			}
			if (tll_length(t->matched) == 0)
				continue;

			if (mport->verbosity >= MPORT_VNORMAL)
				mport_call_msg_cb(mport, "Running trigger: %s", t->name);
			ret = trigger_run_script(
			    mport, t->name, &t->script, &t->matched, NULL, false);
			if (ret != MPORT_OK) {
				mport_call_msg_cb(
				    mport, "Trigger %s failed: %s", t->name, mport_err_string());
				err = ret;
			}
		}
		tll_free_and_free(triggers, free_trigger_cb);
	}

	mport_triggers_state_free(mport);

	if (err != MPORT_OK)
		RETURN_ERROR(err, "One or more triggers failed");
	return (MPORT_OK);
}

/*
 * Root-relative paths of a package's files, their parent directories and its
 * directory assets, read from the named assets table ("stub.assets" while a
 * bundle is attached for install, "assets" for an installed package).
 */
static int
collect_package_paths(
    mportInstance *mport, mportPackageMeta *pkg, const char *table, stringlist_t *paths)
{
	sqlite3_stmt *stmt = NULL;
	char cwd[FILENAME_MAX];
	char path[FILENAME_MAX];
	int ret;

	(void)strlcpy(cwd, pkg->prefix == NULL ? "" : pkg->prefix, sizeof(cwd));

	if (mport_db_prepare(mport->db, &stmt,
		"SELECT type, data FROM %s WHERE pkg=%Q ORDER BY rowid", table,
		pkg->name) != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		mportAssetListEntryType type = (mportAssetListEntryType)sqlite3_column_int(stmt, 0);
		const char *data = (const char *)sqlite3_column_text(stmt, 1);

		switch (type) {
		case ASSET_CWD:
			(void)strlcpy(cwd,
			    (data == NULL || data[0] == '\0') ?
				(pkg->prefix == NULL ? "" : pkg->prefix) :
				data,
			    sizeof(cwd));
			break;
		case ASSET_FILE:
		case ASSET_FILE_OWNER_MODE:
		case ASSET_SHELL:
		case ASSET_SAMPLE:
		case ASSET_SAMPLE_OWNER_MODE:
		case ASSET_INFO:
			if (data == NULL || data[0] == '\0')
				break;
			if (data[0] == '/')
				(void)strlcpy(path, data, sizeof(path));
			else
				(void)snprintf(path, sizeof(path), "%s/%s", cwd, data);
			if (type == ASSET_SAMPLE || type == ASSET_SAMPLE_OWNER_MODE) {
				char *sp = strpbrk(path, " \t");
				if (sp != NULL)
					*sp = '\0';
			}
			mport_trigger_normalize_path(path);
			(void)stringlist_add_unique(paths, path);
			{
				char *slash = strrchr(path, '/');
				if (slash != NULL && slash != path) {
					*slash = '\0';
					(void)stringlist_add_unique(paths, path);
				}
			}
			break;
		case ASSET_DIR:
		case ASSET_DIRRM:
		case ASSET_DIRRMTRY:
		case ASSET_DIR_OWNER_MODE:
		case ASSET_AUTODIR:
			if (data == NULL || data[0] == '\0')
				break;
			if (data[0] == '/')
				(void)strlcpy(path, data, sizeof(path));
			else
				(void)snprintf(path, sizeof(path), "%s/%s", cwd, data);
			mport_trigger_normalize_path(path);
			(void)stringlist_add_unique(paths, path);
			break;
		default:
			break;
		}
	}
	sqlite3_finalize(stmt);

	if (ret != SQLITE_DONE)
		RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
	return (MPORT_OK);
}

/*
 * Run the per-package triggers of one phase against pkg.  Triggers are
 * reloaded from disk each time so one installed earlier in the same run is
 * seen.  A failing trigger is reported and skipped; the package operation
 * goes on, so this only returns an error when the paths cannot be read.
 */
int
mport_triggers_execute_perpackage(
    mportInstance *mport, mportPackageMeta *pkg, mport_lua_script phase)
{
	mportTriggerList triggers = tll_init();
	stringlist_t paths = tll_init();
	const char *table;
	bool upgrade;
	int ret = MPORT_OK;

	if (pkg == NULL || phase >= MPORT_LUA_UNKNOWN)
		return (MPORT_OK);
	if (!mport_triggers_enabled(mport))
		return (MPORT_OK);

	(void)mport_triggers_load(mport, trigger_phase_dirs[phase], &triggers);
	if (tll_length(triggers) == 0)
		return (MPORT_OK);

	table = (phase == MPORT_LUA_PRE_INSTALL || phase == MPORT_LUA_POST_INSTALL) ?
	    "stub.assets" :
	    "assets";
	if (collect_package_paths(mport, pkg, table, &paths) != MPORT_OK) {
		ret = mport_err_code();
		goto cleanup;
	}

	upgrade = (pkg->action == MPORT_ACTION_UPGRADE || pkg->action == MPORT_ACTION_UPDATE);

	tll_foreach(triggers, tit)
	{
		mportTrigger *t = tit->item;

		tll_foreach(paths, pit)
		{
			(void)mport_trigger_match(t, pit->item);
		}
		if (tll_length(t->matched) == 0)
			continue;

		if (mport->verbosity >= MPORT_VNORMAL)
			mport_call_msg_cb(mport, "Running %s trigger %s for %s",
			    trigger_phase_dirs[phase], t->name, pkg->name);
		if (trigger_run_script(mport, t->name, &t->script, &t->matched, pkg, upgrade) !=
		    MPORT_OK) {
			mport_call_msg_cb(mport, "Trigger %s failed for %s, continuing: %s",
			    t->name, pkg->name, mport_err_string());
		}
	}

cleanup:
	tll_free_and_free(paths, free_string);
	tll_free_and_free(triggers, free_trigger_cb);
	return (ret);
}
