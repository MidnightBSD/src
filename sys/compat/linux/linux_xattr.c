/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/extattr.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/syscallsubr.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif

#include <compat/linux/linux_util.h>

#define	LINUX_XATTR_SIZE_MAX	65536
#define	LINUX_XATTR_LIST_MAX	65536
#define	LINUX_XATTR_NAME_MAX	255

#define	LINUX_XATTR_CREATE	0x1
#define	LINUX_XATTR_REPLACE	0x2

struct listxattr_args {
	int		fd;
	const char	*path;
	char		*list;
	l_size_t	size;
	int		follow;
	enum uio_seg	pathseg;
};

struct setxattr_args {
	int		fd;
	const char	*path;
	const char	*name;
	void		*value;
	l_size_t	size;
	l_int		flags;
	int		follow;
	enum uio_seg	pathseg;
};

struct getxattr_args {
	int		fd;
	const char	*path;
	const char	*name;
	void		*value;
	l_size_t	size;
	int		follow;
	enum uio_seg	pathseg;
};

struct removexattr_args {
	int		fd;
	const char	*path;
	const char	*name;
	int		follow;
	enum uio_seg	pathseg;
};

static const char *const extattr_namespace_names[] = EXTATTR_NAMESPACE_NAMES;

static int
error_to_xattrerror(int attrnamespace, int error)
{

	if (attrnamespace == EXTATTR_NAMESPACE_SYSTEM && error == EPERM)
		return (EOPNOTSUPP);
	return (error);
}

static int
xattr_to_extattr(const char *uattrname, int *attrnamespace, char *attrname)
{
	char name[LINUX_XATTR_NAME_MAX + 1], *dot;
	int error;

	error = copyinstr(uattrname, name, sizeof(name), NULL);
	if (error != 0)
		return (error == ENAMETOOLONG ? ERANGE : error);
	dot = strchr(name, '.');
	if (dot == NULL)
		return (EOPNOTSUPP);
	*dot++ = '\0';
	for (*attrnamespace = EXTATTR_NAMESPACE_USER;
	    *attrnamespace < nitems(extattr_namespace_names);
	    (*attrnamespace)++) {
		if (strcmp(name,
		    extattr_namespace_names[*attrnamespace]) == 0) {
			if (*dot == '\0')
				return (EINVAL);
			strcpy(attrname, dot);
			return (0);
		}
	}
	return (EOPNOTSUPP);
}

static int
listxattr(struct thread *td, struct listxattr_args *args)
{
	char name[LINUX_XATTR_NAME_MAX + 1], *data, *key;
	const char *prefix;
	size_t count, remaining, prefixlen, keylen, namelen;
	int attrnamespace, error;

	data = malloc(LINUX_XATTR_LIST_MAX, M_LINUX, M_WAITOK);
	count = 0;
	for (attrnamespace = EXTATTR_NAMESPACE_USER;
	    attrnamespace < nitems(extattr_namespace_names);
	    attrnamespace++) {
		if (args->path != NULL)
			error = kern_extattr_list_path(td, args->path,
			    attrnamespace, data, LINUX_XATTR_LIST_MAX,
			    args->follow, args->pathseg, UIO_SYSSPACE);
		else
			error = kern_extattr_list_fd(td, args->fd,
			    attrnamespace, data, LINUX_XATTR_LIST_MAX,
			    UIO_SYSSPACE);
		if (attrnamespace == EXTATTR_NAMESPACE_SYSTEM &&
		    (error == EPERM || error == EOPNOTSUPP))
			continue;
		if (error != 0)
			goto out;

		prefix = extattr_namespace_names[attrnamespace];
		prefixlen = strlen(prefix);
		key = data;
		remaining = td->td_retval[0];
		if (remaining > LINUX_XATTR_LIST_MAX) {
			error = EIO;
			goto out;
		}
		while (remaining != 0) {
			keylen = (unsigned char)*key++;
			remaining--;
			namelen = prefixlen + 1 + keylen + 1;
			if (keylen == 0 || keylen > remaining) {
				error = EIO;
				goto out;
			}
			if (namelen > sizeof(name) ||
			    count + namelen > LINUX_XATTR_LIST_MAX) {
				error = E2BIG;
				goto out;
			}
			if (args->size != 0) {
				if (count + namelen > args->size) {
					error = ERANGE;
					goto out;
				}
				memcpy(name, prefix, prefixlen);
				name[prefixlen] = '.';
				memcpy(name + prefixlen + 1, key, keylen);
				name[namelen - 1] = '\0';
				error = copyout(name, args->list + count,
				    namelen);
				if (error != 0)
					goto out;
			}
			count += namelen;
			key += keylen;
			remaining -= keylen;
		}
	}
	error = 0;
	td->td_retval[0] = count;
out:
	free(data, M_LINUX);
	return (error);
}

int
linux_listxattr(struct thread *td, struct linux_listxattr_args *args)
{
	struct listxattr_args eargs = {
		.fd = -1,
		.path = args->path,
		.list = __DECONST(char *, args->list),
		.size = args->size,
		.follow = FOLLOW,
		.pathseg = UIO_USERSPACE,
	};
	char *path;
	int error;

	if (args->path == NULL)
		return (EFAULT);
	if (!LUSECONVPATH(td))
		return (listxattr(td, &eargs));
	LCONVPATHEXIST(args->path, &path);
	eargs.path = path;
	eargs.pathseg = UIO_SYSSPACE;
	error = listxattr(td, &eargs);
	LFREEPATH(path);
	return (error);
}

int
linux_llistxattr(struct thread *td, struct linux_llistxattr_args *args)
{
	struct listxattr_args eargs = {
		.fd = -1,
		.path = args->path,
		.list = __DECONST(char *, args->list),
		.size = args->size,
		.follow = NOFOLLOW,
		.pathseg = UIO_USERSPACE,
	};
	char *path;
	int error;

	if (args->path == NULL)
		return (EFAULT);
	if (!LUSECONVPATH(td))
		return (listxattr(td, &eargs));
	LCONVPATHEXIST(args->path, &path);
	eargs.path = path;
	eargs.pathseg = UIO_SYSSPACE;
	error = listxattr(td, &eargs);
	LFREEPATH(path);
	return (error);
}

int
linux_flistxattr(struct thread *td, struct linux_flistxattr_args *args)
{
	struct listxattr_args eargs = {
		.fd = args->fd,
		.path = NULL,
		.list = __DECONST(char *, args->list),
		.size = args->size,
		.follow = 0,
	};

	return (listxattr(td, &eargs));
}

static int
removexattr(struct thread *td, struct removexattr_args *args)
{
	char attrname[LINUX_XATTR_NAME_MAX + 1];
	int attrnamespace, error;

	error = xattr_to_extattr(args->name, &attrnamespace, attrname);
	if (error != 0)
		return (error);
	if (args->path != NULL)
		error = kern_extattr_delete_path(td, args->path, attrnamespace,
		    attrname, args->follow, args->pathseg);
	else
		error = kern_extattr_delete_fd(td, args->fd, attrnamespace,
		    attrname);
	return (error_to_xattrerror(attrnamespace, error));
}

int
linux_removexattr(struct thread *td, struct linux_removexattr_args *args)
{
	struct removexattr_args eargs = {
		.fd = -1,
		.path = args->path,
		.name = args->name,
		.follow = FOLLOW,
		.pathseg = UIO_USERSPACE,
	};
	char *path;
	int error;

	if (args->path == NULL)
		return (EFAULT);
	if (!LUSECONVPATH(td))
		return (removexattr(td, &eargs));
	LCONVPATHEXIST(args->path, &path);
	eargs.path = path;
	eargs.pathseg = UIO_SYSSPACE;
	error = removexattr(td, &eargs);
	LFREEPATH(path);
	return (error);
}

int
linux_lremovexattr(struct thread *td, struct linux_lremovexattr_args *args)
{
	struct removexattr_args eargs = {
		.fd = -1,
		.path = args->path,
		.name = args->name,
		.follow = NOFOLLOW,
		.pathseg = UIO_USERSPACE,
	};
	char *path;
	int error;

	if (args->path == NULL)
		return (EFAULT);
	if (!LUSECONVPATH(td))
		return (removexattr(td, &eargs));
	LCONVPATHEXIST(args->path, &path);
	eargs.path = path;
	eargs.pathseg = UIO_SYSSPACE;
	error = removexattr(td, &eargs);
	LFREEPATH(path);
	return (error);
}

int
linux_fremovexattr(struct thread *td, struct linux_fremovexattr_args *args)
{
	struct removexattr_args eargs = {
		.fd = args->fd,
		.path = NULL,
		.name = args->name,
		.follow = 0,
	};

	return (removexattr(td, &eargs));
}

static int
getxattr(struct thread *td, struct getxattr_args *args)
{
	char attrname[LINUX_XATTR_NAME_MAX + 1];
	int attrnamespace, error;

	error = xattr_to_extattr(args->name, &attrnamespace, attrname);
	if (error != 0)
		return (error);
	if (args->size == 0)
		args->value = NULL;
	args->size = MIN(args->size, LINUX_XATTR_SIZE_MAX);
	if (args->path != NULL)
		error = kern_extattr_get_path(td, args->path, attrnamespace,
		    attrname, args->value, args->size, args->follow,
		    args->pathseg, true);
	else
		error = kern_extattr_get_fd(td, args->fd, attrnamespace,
		    attrname, args->value, args->size, true);
	if (attrnamespace == EXTATTR_NAMESPACE_SYSTEM && error == EPERM)
		return (ENOATTR);
	return (error);
}

int
linux_getxattr(struct thread *td, struct linux_getxattr_args *args)
{
	struct getxattr_args eargs = {
		.fd = -1,
		.path = args->path,
		.name = args->name,
		.value = args->value,
		.size = args->size,
		.follow = FOLLOW,
		.pathseg = UIO_USERSPACE,
	};
	char *path;
	int error;

	if (args->path == NULL)
		return (EFAULT);
	if (!LUSECONVPATH(td))
		return (getxattr(td, &eargs));
	LCONVPATHEXIST(args->path, &path);
	eargs.path = path;
	eargs.pathseg = UIO_SYSSPACE;
	error = getxattr(td, &eargs);
	LFREEPATH(path);
	return (error);
}

int
linux_lgetxattr(struct thread *td, struct linux_lgetxattr_args *args)
{
	struct getxattr_args eargs = {
		.fd = -1,
		.path = args->path,
		.name = args->name,
		.value = args->value,
		.size = args->size,
		.follow = NOFOLLOW,
		.pathseg = UIO_USERSPACE,
	};
	char *path;
	int error;

	if (args->path == NULL)
		return (EFAULT);
	if (!LUSECONVPATH(td))
		return (getxattr(td, &eargs));
	LCONVPATHEXIST(args->path, &path);
	eargs.path = path;
	eargs.pathseg = UIO_SYSSPACE;
	error = getxattr(td, &eargs);
	LFREEPATH(path);
	return (error);
}

int
linux_fgetxattr(struct thread *td, struct linux_fgetxattr_args *args)
{
	struct getxattr_args eargs = {
		.fd = args->fd,
		.path = NULL,
		.name = args->name,
		.value = args->value,
		.size = args->size,
		.follow = 0,
	};

	return (getxattr(td, &eargs));
}

static int
setxattr(struct thread *td, struct setxattr_args *args)
{
	char attrname[LINUX_XATTR_NAME_MAX + 1];
	int attrnamespace, flags, error;

	switch (args->flags) {
	case 0:
		flags = 0;
		break;
	case LINUX_XATTR_CREATE:
		flags = EXTATTR_CREATE;
		break;
	case LINUX_XATTR_REPLACE:
		flags = EXTATTR_REPLACE;
		break;
	default:
		return (EINVAL);
	}
	if (args->size > LINUX_XATTR_SIZE_MAX)
		return (E2BIG);
	error = xattr_to_extattr(args->name, &attrnamespace, attrname);
	if (error != 0)
		return (error);
	if (args->path != NULL)
		error = kern_extattr_set_path(td, args->path, attrnamespace,
		    attrname, args->value, args->size, args->follow,
		    args->pathseg, flags);
	else
		error = kern_extattr_set_fd(td, args->fd, attrnamespace,
		    attrname, args->value, args->size, flags);
	td->td_retval[0] = 0;
	return (error_to_xattrerror(attrnamespace, error));
}

int
linux_setxattr(struct thread *td, struct linux_setxattr_args *args)
{
	struct setxattr_args eargs = {
		.fd = -1,
		.path = args->path,
		.name = args->name,
		.value = __DECONST(void *, args->value),
		.size = args->size,
		.flags = args->flags,
		.follow = FOLLOW,
		.pathseg = UIO_USERSPACE,
	};
	char *path;
	int error;

	if (args->path == NULL)
		return (EFAULT);
	if (!LUSECONVPATH(td))
		return (setxattr(td, &eargs));
	LCONVPATHEXIST(args->path, &path);
	eargs.path = path;
	eargs.pathseg = UIO_SYSSPACE;
	error = setxattr(td, &eargs);
	LFREEPATH(path);
	return (error);
}

int
linux_lsetxattr(struct thread *td, struct linux_lsetxattr_args *args)
{
	struct setxattr_args eargs = {
		.fd = -1,
		.path = args->path,
		.name = args->name,
		.value = __DECONST(void *, args->value),
		.size = args->size,
		.flags = args->flags,
		.follow = NOFOLLOW,
		.pathseg = UIO_USERSPACE,
	};
	char *path;
	int error;

	if (args->path == NULL)
		return (EFAULT);
	if (!LUSECONVPATH(td))
		return (setxattr(td, &eargs));
	LCONVPATHEXIST(args->path, &path);
	eargs.path = path;
	eargs.pathseg = UIO_SYSSPACE;
	error = setxattr(td, &eargs);
	LFREEPATH(path);
	return (error);
}

int
linux_fsetxattr(struct thread *td, struct linux_fsetxattr_args *args)
{
	struct setxattr_args eargs = {
		.fd = args->fd,
		.path = NULL,
		.name = args->name,
		.value = __DECONST(void *, args->value),
		.size = args->size,
		.flags = args->flags,
		.follow = 0,
	};

	return (setxattr(td, &eargs));
}
