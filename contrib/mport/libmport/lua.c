/*-
 * Copyright (c) 2025 Lucas Holt
 * Copyright (c) 2019-2022 Baptiste Daroussin <bapt@FreeBSD.org>
 * Copyright (c) 2023 Serenity Cyber Security, LLC
 *                    Author: Gleb Popov <arrowd@FreeBSD.org>
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

#ifdef HAVE_CAPSICUM
#include <sys/capsicum.h>
#endif

#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdbool.h>
#include <unistd.h>

#include "mport_lua.h"


#ifndef DEFFILEMODE
#define DEFFILEMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#endif

extern char **environ;

int set_attrsat(int fd, const char *path, mode_t perm, uid_t uid, gid_t gid, const struct timespec *ats, const struct timespec *mts);
int checkflags(const char *mode, int *optr);

lua_CFunction
stack_dump(lua_State *L)
{
    int i;
    int top = lua_gettop(L);
    FILE *stream;
    char *stackstr;
    size_t size;

    stream = open_memstream(&stackstr, &size);
    if (stream == NULL) {
        perror("open_memstream");
        return 0;
    }

    fputs("\nLua Stack\n---------\n"
          "\tType   Data\n\t-----------\n", stream);

    for (i = 1; i <= top; i++) {  /* repeat for each level */
        int t = lua_type(L, i);
        fprintf(stream, "%i", i);
        switch (t) {
        case LUA_TSTRING:  /* strings */
            fprintf(stream, "\tString: `%s'\n", lua_tostring(L, i));
            break;
        case LUA_TBOOLEAN:  /* booleans */
            fprintf(stream, "\tBoolean: %s", lua_toboolean(L, i) ? "\ttrue\n" : "\tfalse\n");
            break;
        case LUA_TNUMBER:  /* numbers */
            fprintf(stream, "\tNumber: %g\n", lua_tonumber(L, i));
            break;
        default:  /* other values */
            fprintf(stream, "\tOther: %s\n", lua_typename(L, t));
            break;
        }
    }

    fclose(stream);
    // TODO: better error handling here.
    fprintf(stderr, "%s\n", stackstr);
    free(stackstr);

    return (0);
}

int
lua_print_msg(lua_State *L)
{
	int n = lua_gettop(L);
	luaL_argcheck(L, n == 1, n > 1 ? 2 : n,
	    "pkg.print_msg takes exactly one argument");
	const char* str = luaL_checkstring(L, 1);
	lua_getglobal(L, "msgfd");
	int fd = lua_tointeger(L, -1);

	dprintf(fd, "%s\n", str);

	return (0);
}


static const char**
luaL_checkarraystrings(lua_State *L, int arg) {
	const char **ret;
	lua_Integer n, i;
	int t;
	int abs_arg = lua_absindex(L, arg);
	luaL_checktype(L, abs_arg, LUA_TTABLE);
	n = lua_rawlen(L, abs_arg);
	ret = lua_newuserdata(L, (n+1)*sizeof(char*));
	for (i=0; i<n; i++) {
		t = lua_rawgeti(L, abs_arg, i+1);
		if (t == LUA_TNIL)
			break;
		luaL_argcheck(L, t == LUA_TSTRING, arg, "expected array of strings");
		ret[i] = lua_tostring(L, -1);
		lua_pop(L, 1);
	}
	ret[i] = NULL;
	return ret;
}

int
lua_exec(lua_State *L)
{
	int r, pstat;
	posix_spawn_file_actions_t action;
	int stdin_pipe[2] = {-1, -1};
	pid_t pid;
	const char **argv;
	int n = lua_gettop(L);
	luaL_argcheck(L, n == 1, n > 1 ? 2 : n,
	    "pkg.exec takes exactly one argument");

#ifdef HAVE_CAPSICUM
	unsigned int capmode;
	if (cap_getmode(&capmode) == 0 && capmode > 0) {
		return (luaL_error(L, "pkg.exec not available in sandbox"));
	}
#endif
	if (pipe(stdin_pipe) < 0)
		return (MPORT_ERR_FATAL);

	posix_spawn_file_actions_init(&action);
	posix_spawn_file_actions_adddup2(&action, stdin_pipe[0], STDIN_FILENO);
	posix_spawn_file_actions_addclose(&action, stdin_pipe[1]);

	argv = luaL_checkarraystrings(L, 1);
	if (0 != (r = posix_spawnp(&pid, argv[0], &action, NULL,
		(char*const*)argv, environ))) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(r));
		lua_pushinteger(L, r);
		return 3;
	}
	while (waitpid(pid, &pstat, 0) == -1) {
		if (errno != EINTR) {
			lua_pushnil(L);
			lua_pushstring(L, strerror(r));
			lua_pushinteger(L, r);
			return 3;
		}
	}

	if (WEXITSTATUS(pstat) != 0) {
		lua_pushnil(L);
		lua_pushstring(L, "Abnormal termination");
		lua_pushinteger(L, r);
		return 3;
	}

	posix_spawn_file_actions_destroy(&action);

	if (stdin_pipe[0] != -1)
		close(stdin_pipe[0]);
	if (stdin_pipe[1] != -1)
		close(stdin_pipe[1]);
	lua_pushinteger(L, pid);
	return 1;
}

int
lua_pkg_copy(lua_State *L)
{
	int n = lua_gettop(L);
	luaL_argcheck(L, n == 2, n > 2 ? 3 : n,
	    "pkg.copy takes exactly two arguments");
	const char* src = luaL_checkstring(L, 1);
	const char* dst = luaL_checkstring(L, 2);
	struct stat s1;
	int fd1, fd2;
	struct timespec ts[2];

#ifdef HAVE_CHFLAGSAT
        bool install_as_user = (getenv("INSTALL_AS_USER") != NULL);
#endif

	lua_getglobal(L, "rootfd");
	int rootfd = lua_tointeger(L, -1);

	if (fstatat(rootfd, RELATIVE_PATH(src), &s1, 0) == -1) {
		lua_pushinteger(L, 2);
		return (1);
	}
	fd1 = openat(rootfd, RELATIVE_PATH(src), O_RDONLY, DEFFILEMODE);
	if (fd1 == -1) {
		lua_pushinteger(L, 2);
		return (1);
	}

	fd2 = openat(rootfd, RELATIVE_PATH(dst), O_RDWR | O_CREAT | O_TRUNC | O_EXCL, s1.st_mode);
	if (fd2 == -1) {
		lua_pushinteger(L, 2);
		return (1);
	}

	if (mport_copy_fd(fd1, fd2) != MPORT_OK) {
		lua_pushinteger(L, 2);
		return (1);
	}
	if (fchown(fd2, s1.st_uid, s1.st_gid) == -1) {
		lua_pushinteger(L, 2);
		return (1);
	}

	fsync(fd2);
	close(fd1);
	close(fd2);

#ifdef HAVE_STRUCT_STAT_ST_MTIM
	ts[0] = s1.st_atim;
	ts[1] = s1.st_mtim;
#else
#if defined(_DARWIN_C_SOURCE) || defined(__APPLE__)
	ts[0] = s1.st_atimespec;
	ts[1] = s1.st_mtimespec;
#else
	ts[0].tv_sec = s1.st_atime;
	ts[0].tv_nsec = 0;
	ts[1].tv_sec = s1.st_mtime;
	ts[1].tv_nsec = 0;
#endif
#endif

	if (set_attrsat(rootfd, RELATIVE_PATH(dst), s1.st_mode, s1.st_uid,
	  s1.st_gid, &ts[0], &ts[1]) != MPORT_OK) {
		lua_pushinteger(L, -1);
		return (1);
	}

#ifdef HAVE_CHFLAGSAT
	if (!install_as_user && s1.st_flags != 0) {
		if (chflagsat(rootfd, RELATIVE_PATH(dst),
		    s1.st_flags, AT_SYMLINK_NOFOLLOW) == -1) {
			pkg_fatal_errno("Fail to chflags %s", dst);
			lua_pushinteger(L, -1);
			return (1);
		}
	}
#endif
	return (0);
}

int
lua_pkg_filecmp(lua_State *L)
{
	int n = lua_gettop(L);
	luaL_argcheck(L, n == 2, n > 2 ? 3 : n,
	    "pkg.filecmp takes exactly two arguments");
	const char* file1 = luaL_checkstring(L, 1);
	const char* file2 = luaL_checkstring(L, 2);
	char *buf1, *buf2;
	struct stat s1, s2;
	int fd1, fd2;
	int ret = 0;

	lua_getglobal(L, "rootfd");
	int rootfd = lua_tointeger(L, -1);

	if (fstatat(rootfd, RELATIVE_PATH(file1), &s1, 0) == -1) {
		lua_pushinteger(L, 2);
		return (1);
	}
	if (fstatat(rootfd, RELATIVE_PATH(file2), &s2, 0) == -1) {
		lua_pushinteger(L, 2);
		return (1);
	}
	if (s1.st_size != s2.st_size) {
		lua_pushinteger(L, 1);
		return (1);
	}
	fd1 = openat(rootfd, RELATIVE_PATH(file1), O_RDONLY, DEFFILEMODE);
	if (fd1 == -1) {
		lua_pushinteger(L, 2);
		return (1);
	}
	buf1 = mmap(NULL, s1.st_size, PROT_READ, MAP_SHARED, fd1, 0);
	close(fd1);
	if (buf1 == NULL) {
		lua_pushinteger(L, -1);
		return (1);
	}
	fd2 = openat(rootfd, RELATIVE_PATH(file2), O_RDONLY, DEFFILEMODE);
	if (fd2 == -1) {
		lua_pushinteger(L, 2);
		return (1);
	}

	buf2 = mmap(NULL, s2.st_size, PROT_READ, MAP_SHARED, fd2, 0);
	close(fd2);
	if (buf2 == NULL) {
		lua_pushinteger(L, -1);
		return (1);
	}
	if (memcmp(buf1, buf2, s1.st_size) != 0)
		ret = 1;

	munmap(buf1, s1.st_size);
	munmap(buf2, s2.st_size);

	lua_pushinteger(L, ret);
	return (1);
}

int
lua_pkg_symlink(lua_State *L)
{
	int n = lua_gettop(L);
	luaL_argcheck(L, n == 2, n > 2 ? 3 : n,
	    "pkg.symlink takes exactly two arguments");
	const char *from = luaL_checkstring(L, 1);
	const char *to = luaL_checkstring(L, 2);
	lua_getglobal(L, "rootfd");
	int rootfd = lua_tointeger(L, -1);
	if (symlinkat(from, rootfd, RELATIVE_PATH(to)) == -1)
		return (luaL_fileresult(L, 0, from));
	return (1);
}

int
lua_prefix_path(lua_State *L)
{
	int n = lua_gettop(L);
	luaL_argcheck(L, n == 1, n > 1 ? 2 : n,
	    "pkg.prefix_path takes exactly one argument");
	const char *str = luaL_checkstring(L, 1);
	lua_getglobal(L, "package");
	mportPackageMeta *p = lua_touserdata(L, -1);

	char path[MAXPATHLEN];
	path[0] = '\0';

	if (*str == '/') {
		strlcat(path, str, MAXPATHLEN);
	} else {
		strlcat(path, p->prefix, MAXPATHLEN);
		strlcat(path, "/", MAXPATHLEN);
		strlcat(path, str, MAXPATHLEN);
	}

	lua_pushstring(L, path);
	return (1);
}

int
lua_stat(lua_State *L)
{
	int n = lua_gettop(L);
	luaL_argcheck(L, n == 1, n > 1 ? 2 : n,
	    "pkg.stat takes exactly one argument");
	const char *path = RELATIVE_PATH(luaL_checkstring(L, 1));
	lua_getglobal(L, "rootfd");
	int rootfd = lua_tointeger(L, -1);
	struct stat s;
	const char *type = "unknown";

	if (fstatat(rootfd, path, &s, AT_SYMLINK_NOFOLLOW) == -1) {
		return lua_pushnil(L), 1;
	}

	lua_settop(L, 2);
	if (!lua_istable(L, 2))
		lua_newtable(L);

	lua_pushinteger(L, s.st_size);
	lua_setfield(L, -2, "size");
	lua_pushinteger(L, s.st_uid);
	lua_setfield(L, -2, "uid");
	lua_pushinteger(L, s.st_gid);
	lua_setfield(L, -2, "gid");
	if (S_ISREG(s.st_mode))
		type = "reg";
	else if (S_ISDIR(s.st_mode))
		type = "dir";
	else if (S_ISCHR(s.st_mode))
		type = "chr";
	else if (S_ISLNK(s.st_mode))
		type = "lnk";
	else if (S_ISSOCK(s.st_mode))
		type = "sock";
	else if (S_ISBLK(s.st_mode))
		type = "blk";
	else if (S_ISFIFO(s.st_mode))
		type = "fifo";
	lua_pushstring(L, type);
	lua_setfield(L, -2, "type");

	return (1);
}

/* stolen from lua.c */
void
lua_args_table(lua_State *L, char **argv, int argc)
{
	lua_createtable(L, argc, 1);
	for (int i = 0; i < argc; i++) {
		lua_pushstring(L, argv[i]);
		/* lua starts counting by 1 */
		lua_rawseti(L, -2, i + 1);
	}
	lua_setglobal(L, "arg");
}


/*
 * this is a copy of lua code to be able to override open
 * merge of newprefile and newfile
 */

static int
my_iofclose(lua_State *L)
{
	luaL_Stream *p = ((luaL_Stream *)luaL_checkudata(L, 1, LUA_FILEHANDLE));
	int res = fclose(p->f);
	return (luaL_fileresult(L, (res == 0), NULL));
}

static luaL_Stream *
newfile(lua_State *L) {
	luaL_Stream *p = (luaL_Stream *)lua_newuserdata(L, sizeof(luaL_Stream));
	p->f = NULL;
	p->closef = &my_iofclose;
	luaL_setmetatable(L, LUA_FILEHANDLE);
	return (p);
}

static int
lua_io_open(lua_State *L)
{
	const char *filename = luaL_checkstring(L, 1);
	const char *mode = luaL_optstring(L, 2, "r");
	lua_getglobal(L, "rootfd");
	int rootfd = lua_tointeger(L, -1);
	int oflags;
	luaL_Stream *p = newfile(L);
	const char *md = mode;
	luaL_argcheck(L, checkflags(md, &oflags), 2, "invalid mode");
	int fd = openat(rootfd, RELATIVE_PATH(filename), oflags, DEFFILEMODE);
	if (fd == -1)
		return (luaL_fileresult(L, 0, filename));
	p->f = fdopen(fd, mode);
	return ((p->f == NULL) ? luaL_fileresult(L, 0, filename) : 1);
}

static int
lua_os_remove(lua_State *L) {
	const char *filename = RELATIVE_PATH(luaL_checkstring(L, 1));
	lua_getglobal(L, "rootfd");
	int rootfd = lua_tointeger(L, -1);
	int flag = 0;
	struct stat st;

	if (fstatat(rootfd, filename, &st, AT_SYMLINK_NOFOLLOW) == -1)
		return (luaL_fileresult(L, 1, NULL));

	if (S_ISDIR(st.st_mode))
		flag = AT_REMOVEDIR;

	return (luaL_fileresult(L, unlinkat(rootfd, filename, flag) == 0, NULL));
}

static int
lua_os_rename(lua_State *L)
{
	const char *fromname = RELATIVE_PATH(luaL_checkstring(L, 1));
	const char *toname = RELATIVE_PATH(luaL_checkstring(L, 2));
	lua_getglobal(L, "rootfd");
	int rootfd = lua_tointeger(L, -1);
	return luaL_fileresult(L, renameat(rootfd, fromname, rootfd, toname) == 0, NULL);
}

static int
lua_os_execute(lua_State *L)
{
	return (luaL_error(L, "os.execute not available"));
}

static int
lua_os_exit(lua_State *L)
{
	return (luaL_error(L, "os.exit not available"));
}

void
lua_override_ios(lua_State *L, bool sandboxed)
{
	lua_getglobal(L, "io");
	lua_pushcfunction(L, lua_io_open);
	lua_setfield(L, -2, "open");

	lua_getglobal(L, "os");
	lua_pushcfunction(L, lua_os_remove);
	lua_setfield(L, -2, "remove");
	lua_pushcfunction(L, lua_os_rename);
	lua_setfield(L, -2, "rename");
	if (sandboxed) {
		lua_pushcfunction(L, lua_os_execute);
		lua_setfield(L, -2, "execute");
	}
	lua_pushcfunction(L, lua_os_exit);
	lua_setfield(L, -2, "exit");
}

int
lua_readdir(lua_State *L)
{
	int n = lua_gettop(L);
	luaL_argcheck(L, n == 1, n > 1 ? 2 : n,
	    "pkg.readdir takes exactly one argument");
	const char *path = luaL_checkstring(L, 1);
	int fd = -1;

	if (*path == '/') {
		lua_getglobal(L, "rootfd");
		int rootfd = lua_tointeger(L, -1);
		if (strlen(path) > 1) {
			fd = openat(rootfd, path +1, O_DIRECTORY);
		} else {
			fd = dup(rootfd);
		}
	} else {
		fd = open(path, O_DIRECTORY);
	}
	if (fd == -1)
		return (luaL_fileresult(L, 0, path));

	DIR *dir = fdopendir(fd);
	if (!dir)
		return (luaL_fileresult(L, 0, path));
	lua_newtable(L);
	struct dirent *e;
	int i = 0;
	while ((e = readdir(dir))) {
		char *name = e->d_name;
		if (STREQ(name, ".") || STREQ(name, ".."))
			continue;
		lua_pushinteger(L, ++i);
		lua_pushstring(L, name);
		lua_settable(L, -3);
	}
	return 1;
}

int
set_attrsat(int fd, const char *path, mode_t perm, uid_t uid, gid_t gid,
    const struct timespec *ats, const struct timespec *mts)
{
	struct stat st;
	struct timespec times[2];

	times[0] = *ats;
	times[1] = *mts;
	if (utimensat(fd, RELATIVE_PATH(path), times,
	    AT_SYMLINK_NOFOLLOW) == -1 && errno != EOPNOTSUPP){
		fprintf(stderr, "Fail to set time on %s", path);
	}

	if (getenv("INSTALL_AS_USER") == NULL) {
		if (fchownat(fd, RELATIVE_PATH(path), uid, gid,
				AT_SYMLINK_NOFOLLOW) == -1) {
			if (errno == ENOTSUP) {
				if (fchownat(fd, RELATIVE_PATH(path), uid, gid, 0) == -1) {
					fprintf(stderr, "Fail to chown(fallback) %s", path);
				}
			}
			else {
				fprintf(stderr, "Fail to chown %s", path);
			}
		}
	}

	/* zfs drops the setuid on fchownat */
	if (fchmodat(fd, RELATIVE_PATH(path), perm, AT_SYMLINK_NOFOLLOW) == -1) {
		if (errno == ENOTSUP) {
			/*
			 * Executing fchmodat on a symbolic link results in
			 * ENOENT (file not found) on platforms that do not
			 * support AT_SYMLINK_NOFOLLOW. The file mode of
			 * symlinks cannot be modified via file descriptor
			 * reference on these systems. The lchmod function is
			 * also not an option because it is not a posix
			 * standard, nor is implemented everywhere. Since
			 * symlink permissions have never been evaluated and
			 * thus cosmetic, just skip them on these systems.
			 */
			if (fstatat(fd, RELATIVE_PATH(path), &st, AT_SYMLINK_NOFOLLOW) == -1) {
				fprintf(stderr, "Fail to get file status %s", path);
			}
			if (!S_ISLNK(st.st_mode)) {
				if (fchmodat(fd, RELATIVE_PATH(path), perm, 0) == -1) {
					fprintf(stderr, "Fail to chmod(fallback) %s", path);
				}
			}
		}
		else {
			fprintf(stderr, "Fail to chmod %s", path);
		}
	}

	return (MPORT_OK);
}

/*
 * Return the (stdio) flags for a given mode.  Store the flags
 * to be passed to an _open() syscall through *optr.
 * Return 1 on error.
 */
int
checkflags(const char *mode, int *optr)
{
	int ret, m, o, known;
	ret = 0;

	switch (*mode++) {

	case 'r':	/* open for reading */
		ret = 1;
		m = O_RDONLY;
		o = 0;
		break;

	case 'w':	/* open for writing */
		ret = 1;
		m = O_WRONLY;
		o = O_CREAT | O_TRUNC;
		break;

	case 'a':	/* open for appending */
		ret = 1;
		m = O_WRONLY;
		o = O_CREAT | O_APPEND;
		break;

	default:	/* illegal mode */
		errno = EINVAL;
		return (0);
	}

	do {
		known = 1;
		switch (*mode++) {
		case 'b':
			/* 'b' (binary) is ignored */
			break;
		case '+':
			/* [rwa][b]\+ means read and write */
			ret = 1;
			m = O_RDWR;
			break;
		case 'x':
			/* 'x' means exclusive (fail if the file exists) */
			o |= O_EXCL;
			break;
		case 'e':
			/* set close-on-exec */
			o |= O_CLOEXEC;
			break;
		default:
			known = 0;
			break;
		}
	} while (known);

	if ((o & O_EXCL) != 0 && m == O_RDONLY) {
		errno = EINVAL;
		return (1);
	}

	*optr = m | o;
	return (ret);
}

