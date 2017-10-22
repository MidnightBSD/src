/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_LINUX_SYSFS_H_
#define	_LINUX_SYSFS_H_

#include <sys/sysctl.h>

struct attribute {
	const char 	*name;
	struct module	*owner;
	mode_t		mode;
};

struct sysfs_ops {
	ssize_t (*show)(struct kobject *, struct attribute *, char *);
	ssize_t (*store)(struct kobject *, struct attribute *, const char *,
	    size_t);
};

struct attribute_group {
	const char		*name;
	mode_t                  (*is_visible)(struct kobject *,
				    struct attribute *, int);
	struct attribute	**attrs;
};

#define	__ATTR(_name, _mode, _show, _store) {				\
	.attr = { .name = __stringify(_name), .mode = _mode },		\
        .show = _show, .store  = _store,				\
}

#define	__ATTR_RO(_name) {						\
	.attr = { .name = __stringify(_name), .mode = 0444 },		\
	.show   = _name##_show,						\
}

#define	__ATTR_NULL	{ .attr = { .name = NULL } }

/*
 * Handle our generic '\0' terminated 'C' string.
 * Two cases:
 *      a variable string:  point arg1 at it, arg2 is max length.
 *      a constant string:  point arg1 at it, arg2 is zero.
 */

static inline int
sysctl_handle_attr(SYSCTL_HANDLER_ARGS)
{
	struct kobject *kobj;
	struct attribute *attr;
	const struct sysfs_ops *ops;
	void *buf;
	int error;
	ssize_t len;

	kobj = arg1;
	attr = (struct attribute *)arg2;
	buf = (void *)get_zeroed_page(GFP_KERNEL);
	len = 1;	/* Copy out a NULL byte at least. */
	if (kobj->ktype == NULL || kobj->ktype->sysfs_ops == NULL)
		return (ENODEV);
	ops = kobj->ktype->sysfs_ops;
	if (buf == NULL)
		return (ENOMEM);
	if (ops->show) {
		len = ops->show(kobj, attr, buf);
		/*
		 * It's valid not to have a 'show' so we just return 1 byte
		 * of NULL.
	 	 */
		if (len < 0) {
			error = -len;
			len = 1;
			if (error != EIO)
				goto out;
		}
	}
	error = SYSCTL_OUT(req, buf, len);
	if (error || !req->newptr || ops->store == NULL)
		goto out;
	error = SYSCTL_IN(req, buf, PAGE_SIZE);
	if (error)
		goto out;
	len = ops->store(kobj, attr, buf, req->newlen);
	if (len < 0)
		error = -len;
out:
	free_page((unsigned long)buf);

	return (error);
}

static inline int
sysfs_create_file(struct kobject *kobj, const struct attribute *attr)
{

	sysctl_add_oid(NULL, SYSCTL_CHILDREN(kobj->oidp), OID_AUTO,
	    attr->name, CTLTYPE_STRING|CTLFLAG_RW|CTLFLAG_MPSAFE, kobj,
	    (uintptr_t)attr, sysctl_handle_attr, "A", "");

	return (0);
}

static inline void
sysfs_remove_file(struct kobject *kobj, const struct attribute *attr)
{

	if (kobj->oidp)
		sysctl_remove_name(kobj->oidp, attr->name, 1, 1);
}

static inline void
sysfs_remove_group(struct kobject *kobj, const struct attribute_group *grp)
{

	if (kobj->oidp)
		sysctl_remove_name(kobj->oidp, grp->name, 1, 1);
}

static inline int
sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp)
{
	struct attribute **attr;
	struct sysctl_oid *oidp;

	oidp = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(kobj->oidp),
	    OID_AUTO, grp->name, CTLFLAG_RD|CTLFLAG_MPSAFE, NULL, grp->name);
	for (attr = grp->attrs; *attr != NULL; attr++) {
		sysctl_add_oid(NULL, SYSCTL_CHILDREN(oidp), OID_AUTO,
		    (*attr)->name, CTLTYPE_STRING|CTLFLAG_RW|CTLFLAG_MPSAFE,
		    kobj, (uintptr_t)*attr, sysctl_handle_attr, "A", "");
	}

	return (0);
}

static inline int
sysfs_create_dir(struct kobject *kobj)
{

	kobj->oidp = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(kobj->parent->oidp),
	    OID_AUTO, kobj->name, CTLFLAG_RD|CTLFLAG_MPSAFE, NULL, kobj->name);

        return (0);
}

static inline void
sysfs_remove_dir(struct kobject *kobj)
{

	if (kobj->oidp == NULL)
		return;
	sysctl_remove_oid(kobj->oidp, 1, 1);
}

#endif	/* _LINUX_SYSFS_H_ */
