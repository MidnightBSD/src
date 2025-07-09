/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Lucas Holt
 * Copyright (c) 2019 Baptiste Daroussin <bapt@FreeBSD.org>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>

#include <errno.h>
#include <poll.h>
#include <err.h>

extern char **environ;

#include "mport_lua.h"

int
get_socketpair(int *pipe)
{
	int r;
	r = socketpair(AF_LOCAL, SOCK_DGRAM, 0, pipe);

	return (r);
}

int
mport_script_run_child(mportInstance *mport, int pid, int *pstat, int inputfd, const char *script_name) 
{
	struct pollfd pfd;
	bool wait_for_child;
	char msgbuf[16384+1];

	memset(&pfd, 0, sizeof(pfd));
	pfd.events = POLLIN | POLLERR | POLLHUP;
	pfd.fd = inputfd;

	// Wait for child to exit, and read input, including all queued input on child exit.
	wait_for_child = true;
	do {
		pfd.revents = 0;
		errno = 0;
		// Check if child is running, get exitstatus if newly terminated.
		pid_t p = 0;
		while (wait_for_child && (p = waitpid(pid, pstat, WNOHANG)) == -1) {
			if (errno != EINTR) {
                RETURN_ERRORX(MPORT_ERR_FATAL, "waitpid() failed: %s", strerror(errno));
			}
		}
		if (p > 0) {
			wait_for_child = false;
		}
		// Check for input from child, but only wait for more if child is still running.
		// Read/print all available input.
		ssize_t readsize;
		do {
			readsize = 0;
			int pres;
			while ((pres = poll(&pfd, 1, wait_for_child ? 1000 : 0)) == -1) {
				if (errno != EINTR) {
                    RETURN_ERRORX(MPORT_ERR_FATAL, "poll() failed: %s", strerror(errno));
				}
			}
			if (pres > 0 && pfd.revents & POLLIN) {
				while ((readsize = read(inputfd, msgbuf, sizeof msgbuf - 1)) < 0) {
					// MacOS gives us ECONNRESET on child exit
					if (errno == EAGAIN || errno == ECONNRESET) {
						break;
					}
					if (errno != EINTR) {
                        RETURN_ERRORX(MPORT_ERR_FATAL, "read() failed: %s", strerror(errno));
					}
				}
				if (readsize > 0) {
					msgbuf[readsize] = '\0';
                    
                    mport_call_msg_cb(mport, msgbuf);
				}
			}
		} while (readsize > 0);
	} while (wait_for_child);

	if (WEXITSTATUS(*pstat) != 0) {
		if (WEXITSTATUS(*pstat) == 3)
			exit(0);

        RETURN_ERRORX(MPORT_ERR_FATAL, "%s script failed", script_name);
	}
	return (MPORT_OK);
}

int
mport_lua_script_run(mportInstance *mport, mportPackageMeta *pkg, mport_lua_script type)
{
	int ret = MPORT_OK;
	int pstat;
	int cur_pipe[2];
	char *line = NULL;

	if (tll_length(pkg->lua_scripts[type]) == 0)
		return (MPORT_OK);

	tll_foreach(pkg->lua_scripts[type], s) {
		if (get_socketpair(cur_pipe) == -1) {
            SET_ERROR(MPORT_ERR_FATAL, "socket pair failed");
			goto cleanup;
		}
		pid_t pid = fork();
		if (pid == 0) {
			static const luaL_Reg pkg_lib[] = {
				{ "print_msg", lua_print_msg },
				{ "prefixed_path", lua_prefix_path },
				{ "filecmp", lua_pkg_filecmp },
				{ "copy", lua_pkg_copy },
				{ "stat", lua_stat },
				{ "readdir", lua_readdir },
				{ "exec", lua_exec },
				{ "symlink", lua_pkg_symlink },
				{ NULL, NULL },
			};
			close(cur_pipe[0]);
			lua_State *L = luaL_newstate();
			luaL_openlibs( L );
			lua_atpanic(L, (lua_CFunction)stack_dump );
			lua_pushinteger(L, cur_pipe[1]);
			lua_setglobal(L, "msgfd");
			lua_pushlightuserdata(L, pkg);
			lua_setglobal(L, "package");
			lua_pushinteger(L, mport->rootfd);
			lua_setglobal(L, "rootfd");
			lua_pushstring(L, pkg->prefix);
			lua_setglobal(L, "pkg_prefix");
			lua_pushstring(L, pkg->name);
			lua_setglobal(L, "pkg_name");
			lua_pushstring(L, mport->root);
			lua_setglobal(L, "pkg_rootdir");
			lua_pushboolean(L, (pkg->action == MPORT_ACTION_UPGRADE? 1 : 0));
			lua_setglobal(L, "pkg_upgrade");
			luaL_newlib(L, pkg_lib);
			lua_setglobal(L, "pkg");
			lua_override_ios(L, true);

			/* parse and set arguments of the line is in the comments */
			if (mport_starts_with("-- args: ", s->item)) {
				char *walk, *begin, *line = NULL;
				int spaces, argc = 0;
				char **args = NULL;

				walk = strchr(s->item, '\n');
				begin = s->item + strlen("-- args: ");
				line = strndup(begin, walk - begin);
				spaces = mport_count_spaces(line);
				args = malloc((spaces + 1)* sizeof(char *));
				walk = strdup(line);
				while (walk != NULL) {
					args[argc++] = mport_tokenize(&walk);
				}
				lua_args_table(L, args, argc);
			}

			//pkg_debug(3, "Scripts: executing lua\n--- BEGIN ---\n%s\nScripts: --- END ---", s->item);
			if (luaL_dostring(L, s->item)) {
                SET_ERRORX(MPORT_ERR_FATAL, "Failed to execute lua script: %s", lua_tostring(L, -1));
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
            SET_ERROR(MPORT_ERR_FATAL, "Cannot fork lua script");
			ret = MPORT_ERR_FATAL;
			goto cleanup;
		}

		close(cur_pipe[1]);

		ret = mport_script_run_child(mport, pid, &pstat, cur_pipe[0], "lua");
	}


cleanup:
	free(line);

	return (ret);
}

ucl_object_t *
mport_lua_script_to_ucl(stringlist_t *scripts)
{
	ucl_object_t *array;

	array = ucl_object_typed_new(UCL_ARRAY);
	tll_foreach(*scripts, s)
		ucl_array_append(array, ucl_object_fromstring_common(s->item,
		    strlen(s->item), UCL_STRING_RAW|UCL_STRING_TRIM));

	return (array);
}

int
mport_lua_script_from_ucl(mportInstance *mport, mportPackageMeta *pkg, const ucl_object_t *obj, mport_lua_script type)
{
	const ucl_object_t *cur;
	ucl_object_iter_t it = NULL;

	while ((cur = ucl_iterate_object(obj, &it, true))) {
		if (ucl_object_type(cur) != UCL_STRING) {
            RETURN_ERROR(MPORT_ERR_FATAL, "lua scripts should be strings.\n");
		}
		tll_push_back(pkg->lua_scripts[type], strdup(ucl_object_tostring(cur)));
	}
	return (MPORT_OK);
}

int
mport_lua_script_load(mportInstance *mport, mportPackageMeta *pkg)
{
	mport_lua_script_read_file(mport, pkg, MPORT_LUA_PRE_INSTALL, MPORT_LUA_PRE_INSTALL_FILE);
	mport_lua_script_read_file(mport, pkg, MPORT_LUA_POST_INSTALL, MPORT_LUA_POST_INSTALL_FILE);
	mport_lua_script_read_file(mport, pkg, MPORT_LUA_PRE_DEINSTALL, MPORT_LUA_PRE_DEINSTALL_FILE);
	mport_lua_script_read_file(mport, pkg, MPORT_LUA_POST_DEINSTALL, MPORT_LUA_POST_DEINSTALL_FILE);

	return (MPORT_OK);
}

int
mport_lua_script_read_file(mportInstance *mport, mportPackageMeta *pkg, mport_lua_script type, char *luafile)
{
	char filename[FILENAME_MAX];
	char *buf;
	struct stat st;
	FILE *file;
	struct ucl_parser *parser;
	ucl_object_t *obj;

	/* Assumes copy_metafile has run on install already */
	(void)snprintf(filename, FILENAME_MAX, "%s%s/%s-%s/%s", mport->root, MPORT_INST_INFRA_DIR,
	    pkg->name, pkg->version, luafile);

	if (stat(filename, &st) == -1) {
		/* if we couldn't stat the file, we assume there isn't a lua file*/
		return MPORT_OK;
	}

	if ((file = fopen(filename, "re")) == NULL)
		RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't open %s: %s", filename, strerror(errno));

	if ((buf = (char *)calloc((size_t)(st.st_size + 1), sizeof(char))) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	if (fread(buf, sizeof(char), (size_t)st.st_size, file) != (size_t)st.st_size) {
		free(buf);
		RETURN_ERRORX(MPORT_ERR_FATAL, "Read error: %s", strerror(errno));
	}

	buf[st.st_size] = '\0';

	if (buf[0] == '[') {
		parser = ucl_parser_new(0);
		// remove leading/trailing array entries
		buf[0] = ' ';
		buf[st.st_size - 1] = '\0';

		if (ucl_parser_add_chunk(parser, (const unsigned char *)buf, st.st_size)) {
			obj = ucl_parser_get_object(parser);
		    int ret = mport_lua_script_from_ucl(mport, pkg, obj, type);
			ucl_parser_free(parser);
			free(buf);

			ucl_object_unref(obj);

			return ret;
		}

		ucl_parser_free(parser);
	}

	return (MPORT_OK);
}
