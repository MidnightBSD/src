/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated for what's essentially a complete rewrite.
 *
 * $FreeBSD$
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"
#include <sys/param.h>
#include <errno.h>

#define MAX_MENU		15

static Boolean exited;

int
dmenuDisplayFile(dialogMenuItem *tmp)
{
    systemDisplayHelp((char *)tmp->data);
    return DITEM_SUCCESS;
}

int
dmenuSubmenu(dialogMenuItem *tmp)
{
    return (dmenuOpenSimple((DMenu *)(tmp->data), FALSE) ? DITEM_SUCCESS : DITEM_FAILURE);
}

int
dmenuSystemCommand(dialogMenuItem *self)
{
    WINDOW *w = NULL;	/* Keep lint happy */

    /* If aux is set, the command is known not to produce any screen-spoiling output */
    if (!self->aux)
	w = savescr();
    systemExecute((char *)self->data);
    if (!self->aux)
	restorescr(w);
    return DITEM_SUCCESS;
}

int
dmenuSystemCommandBox(dialogMenuItem *tmp)
{
    WINDOW *w = savescr();
    
    use_helpfile(NULL);
    use_helpline("Select OK to dismiss this dialog");
    dialog_prgbox(tmp->title, (char *)tmp->data, 22, 76, 1, 1);
    restorescr(w);
    return DITEM_SUCCESS;
}

int
dmenuExit(dialogMenuItem *tmp)
{
    exited = TRUE;
    return DITEM_LEAVE_MENU;
}

int
dmenuSetVariable(dialogMenuItem *tmp)
{
    variable_set((char *)tmp->data, *((char *)tmp->data) != '_');
    return DITEM_SUCCESS;
}

int
dmenuSetVariables(dialogMenuItem *tmp)
{
    char *cp1, *cp2;
    char *copy = strdup((char *)tmp->data);

    for (cp1 = copy; cp1 != NULL;) {
	cp2 = index(cp1, ',');
	if (cp2 != NULL) *cp2++ = '\0';
	variable_set(cp1, *cp1 != '_');
	cp1 = cp2;
    }
    free(copy);
    return DITEM_SUCCESS;
}

int
dmenuSetCountryVariable(dialogMenuItem *tmp)
{
    variable_set((char *)tmp->data, FALSE);
#ifdef WITH_SYSCONS
    /* Don't prompt the user for a keymap if they're using the default locale. */
    if (!strcmp(variable_get(VAR_COUNTRY), DEFAULT_COUNTRY))
	return DITEM_SUCCESS;

    return keymapMenuSelect(tmp);
#else
    return DITEM_SUCCESS;
#endif
}

int
dmenuSetKmapVariable(dialogMenuItem *tmp)
{
    char *lang;
    int err;

    variable_set((char *)tmp->data, TRUE);
    lang = variable_get(VAR_KEYMAP);
    if (lang != NULL)
    {
	err = loadKeymap(lang);
	if (err == -1)
	    msgConfirm("No appropriate keyboard map found, sorry.");
	else if (err == -2)
	    msgConfirm("Error installing keyboard map, errno = %d.", errno);
    }
    return DITEM_SUCCESS;
}

int
dmenuToggleVariable(dialogMenuItem *tmp)
{
    char *var, *cp;
    int status;

    if (!(var = strdup((char *)tmp->data))) {
	msgConfirm("Incorrect data field for `%s'!", tmp->title);
	return DITEM_FAILURE;
    }
    if (!(cp = index(var, '='))) {
	msgConfirm("Data field for %s is not in var=value format!", tmp->title);
	return DITEM_FAILURE;
    }
    status = variable_check(var);
    *cp = '\0';
    variable_set2(var, status ? "NO" : "YES", *var != '_');
    free(var);
    return DITEM_SUCCESS;
}

int
dmenuISetVariable(dialogMenuItem *tmp)
{
    char *ans, *p, *var;

    if (!(var = strdup((char *)tmp->data))) {
	msgConfirm("Incorrect data field for `%s'!", tmp->title);
	return DITEM_FAILURE;
    }
    if ((p = index(var, '=')) != NULL)
	*p = '\0';
    ans = msgGetInput(variable_get(var), tmp->title, 1);
    if (!ans) {
	free(var);
	return DITEM_FAILURE;
    } else if (!*ans)
	variable_unset(var);
    else
	variable_set2(var, ans, *var != '_');
    free(var);
    return DITEM_SUCCESS;
}

int
dmenuSetFlag(dialogMenuItem *tmp)
{
    if (*((unsigned int *)tmp->data) & tmp->aux)
	*((unsigned int *)tmp->data) &= ~tmp->aux;
    else
	*((unsigned int *)tmp->data) |= tmp->aux;
    return DITEM_SUCCESS;
}

int
dmenuSetValue(dialogMenuItem *tmp)
{
    *((unsigned int *)tmp->data) = tmp->aux;
    return DITEM_SUCCESS;
}

/* Traverse menu but give user no control over positioning */
Boolean
dmenuOpenSimple(DMenu *menu, Boolean buttons)
{
    int choice, scroll, curr, max;

    choice = scroll = curr = max = 0;
    return dmenuOpen(menu, &choice, &scroll, &curr, &max, buttons);
}

/* Work functions for the state hook */
int
dmenuFlagCheck(dialogMenuItem *item)
{
    return (*((unsigned int *)item->data) & item->aux);
}

int
dmenuVarCheck(dialogMenuItem *item)
{
    char *w;

    w = (char *)item->aux;
    if (!w)
	w = (char *)item->data;
    return variable_check(w);
}

int
dmenuVarsCheck(dialogMenuItem *item)
{
    int res, init;
    char *w, *cp1, *cp2;
    char *copy;

    w = (char *)item->aux;
    if (!w)
	w = (char *)item->data;
    if (!w)
	return FALSE;
    
    copy = strdup(w);
    res = TRUE;
    init = FALSE;
    for (cp1 = copy; cp1 != NULL;) {
        init = TRUE;
	cp2 = index(cp1, ',');
	if (cp2 != NULL)
	    *cp2++ = '\0';
	res = res && variable_check(cp1);
	cp1 = cp2;
    }
    free(copy);
    return res && init;
}

int
dmenuRadioCheck(dialogMenuItem *item)
{
    return (*((int *)item->data) == item->aux);
}

static int
menu_height(DMenu *menu, int n)
{
    int max;
    char *t;

    max = MAX_MENU;
    if (StatusLine > 24)
	max += StatusLine - 24;
    for (t = menu->prompt; *t; t++) {
	if (*t == '\n')
	    --max;
    }
    return n > max ? max : n;
}

/* Find a menu item that matches any field. */
int
dmenuFindItem(DMenu *menu, const char *prompt, const char *title, void *data)
{
    dialogMenuItem *items = menu->items;
    int i;

    for (i = 0; items[i].prompt; ++i)
	if ((prompt && !strcmp(items[i].prompt, prompt)) ||
		(title && !strcmp(items[i].title, title)) ||
		(data && items[i].data == data))
	    return i;

    return -1;
}

/* Set the default item for a menu by index and scroll to it. */
void
dmenuSetDefaultIndex(DMenu *menu, int *choice, int *scroll, int *curr, int *max)
{
    int nitem;
    int height;

    *curr = *max = 0;

    for (nitem = 0; menu->items[nitem].prompt; ++nitem);

    height = menu_height(menu, nitem);
    if (*choice > height)
    {
	*scroll = MIN(nitem - height, *choice);
	*choice = *choice - *scroll;
    }
    else
	*scroll = 0;
}

/* Set the default menu item that matches any field and scroll to it. */
Boolean
dmenuSetDefaultItem(DMenu *menu, const char *prompt, const char *title, void *data,
		    int *choice, int *scroll, int *curr, int *max)
{
    if ((*choice = dmenuFindItem(menu, prompt, title, data)) != -1)
    {
	dmenuSetDefaultIndex(menu, choice, scroll, curr, max);
	return TRUE;
    }
    else
    {
	*choice = *scroll = *curr = *max = 0;
	return FALSE;
    }
}

/* Traverse over an internal menu */
Boolean
dmenuOpen(DMenu *menu, int *choice, int *scroll, int *curr, int *max, Boolean buttons)
{
    int n, rval = 0;
    dialogMenuItem *items;

    items = menu->items;
    if (buttons)
	items += 2;
    /* Count up all the items */
    for (n = 0; items[n].title; n++);

    while (1) {
	char buf[FILENAME_MAX];
	WINDOW *w = savescr();

	/* Any helpful hints, put 'em up! */
	use_helpline(menu->helpline);
	use_helpfile(systemHelpFile(menu->helpfile, buf));
	dialog_clear_norefresh();
	/* Pop up that dialog! */
	if (menu->type & DMENU_NORMAL_TYPE)
	    rval = dialog_menu((u_char *)menu->title, (u_char *)menu->prompt,
		-1, -1, menu_height(menu, n), -n, items,
		(char *)(uintptr_t)buttons, choice, scroll);

	else if (menu->type & DMENU_RADIO_TYPE)
	    rval = dialog_radiolist((u_char *)menu->title,
		(u_char *)menu->prompt, -1, -1, menu_height(menu, n), -n,
		items, (char *)(uintptr_t)buttons);

	else if (menu->type & DMENU_CHECKLIST_TYPE)
	    rval = dialog_checklist((u_char *)menu->title,
		(u_char *)menu->prompt, -1, -1, menu_height(menu, n), -n,
		items, (char *)(uintptr_t)buttons);
	else
	    msgFatal("Menu: `%s' is of an unknown type\n", menu->title);
	if (exited) {
	    exited = FALSE;
	    restorescr(w);
	    return TRUE;
	}
	else if (rval) {
	    restorescr(w);
	    return FALSE;
	}
	else if (menu->type & DMENU_SELECTION_RETURNS) {
	    restorescr(w);
	    return TRUE;
	}
    }
}
