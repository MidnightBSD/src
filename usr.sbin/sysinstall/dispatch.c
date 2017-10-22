/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
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
#include <ctype.h>
#include <errno.h>
#include <sys/signal.h>
#include <sys/fcntl.h>

#include "list.h"

static int dispatch_shutdown(dialogMenuItem *unused);
static int dispatch_systemExecute(dialogMenuItem *unused);
static int dispatch_msgConfirm(dialogMenuItem *unused);
static int dispatch_mediaOpen(dialogMenuItem *unused);
static int dispatch_mediaClose(dialogMenuItem *unused);
static int cfgModuleFire(dialogMenuItem *self);

static struct _word {
    char *name;
    int (*handler)(dialogMenuItem *self);
} resWords[] = {
    { "configAnonFTP",		configAnonFTP		},
    { "configRouter",		configRouter		},
    { "configInetd",		configInetd		},
    { "configNFSServer",	configNFSServer		},
    { "configNTP",		configNTP		},
    { "configPCNFSD",		configPCNFSD		},
    { "configPackages",		configPackages		},
    { "configUsers",		configUsers		},
#ifdef WITH_SLICES
    { "diskPartitionEditor",	diskPartitionEditor	},
#endif
    { "diskPartitionWrite",	diskPartitionWrite	},
    { "diskLabelEditor",	diskLabelEditor		},
    { "diskLabelCommit",	diskLabelCommit		},
    { "distReset",		distReset		},
    { "distSetCustom",		distSetCustom		},
    { "distUnsetCustom",	distUnsetCustom		},
    { "distSetDeveloper",	distSetDeveloper	},
    { "distSetKernDeveloper",	distSetKernDeveloper	},
    { "distSetUser",		distSetUser		},
    { "distSetMinimum",		distSetMinimum		},
    { "distSetEverything",	distSetEverything	},
    { "distSetSrc",		distSetSrc		},
    { "distExtractAll",		distExtractAll		},
    { "docBrowser",		docBrowser		},
    { "docShowDocument",	docShowDocument		},
    { "installCommit",		installCommit		},
    { "installExpress",		installExpress		},
    { "installStandard",	installStandard		},
    { "installUpgrade",		installUpgrade		},
    { "installFixupBase",	installFixupBase	},
    { "installFixitHoloShell",	installFixitHoloShell	},
    { "installFixitCDROM",	installFixitCDROM	},
    { "installFixitUSB",	installFixitUSB		},
    { "installFixitFloppy",	installFixitFloppy	},
    { "installFilesystems",	installFilesystems	},
    { "installVarDefaults",	installVarDefaults	},
    { "loadConfig",		dispatch_load_file	},
    { "loadFloppyConfig",	dispatch_load_floppy	},
    { "loadCDROMConfig",	dispatch_load_cdrom	},
    { "mediaOpen",		dispatch_mediaOpen	},
    { "mediaClose",		dispatch_mediaClose	},
    { "mediaSetCDROM",		mediaSetCDROM		},
    { "mediaSetFloppy",		mediaSetFloppy		},
    { "mediaSetUSB",		mediaSetUSB		},
    { "mediaSetDOS",		mediaSetDOS		},
    { "mediaSetFTP",		mediaSetFTP		},
    { "mediaSetFTPActive",	mediaSetFTPActive	},
    { "mediaSetFTPPassive",	mediaSetFTPPassive	},
    { "mediaSetHTTP",		mediaSetHTTP		},
    { "mediaSetUFS",		mediaSetUFS		},
    { "mediaSetNFS",		mediaSetNFS		},
    { "mediaSetFTPUserPass",	mediaSetFTPUserPass	},
    { "mediaSetCPIOVerbosity",	mediaSetCPIOVerbosity	},
    { "mediaGetType",		mediaGetType		},
    { "msgConfirm",		dispatch_msgConfirm	},
    { "optionsEditor",		optionsEditor		},
    { "packageAdd",		packageAdd		},
    { "addGroup",		userAddGroup		},
    { "addUser",		userAddUser		},
    { "shutdown",		dispatch_shutdown 	},
    { "system",			dispatch_systemExecute	},
    { "dumpVariables",		dump_variables		},
    { "tcpMenuSelect",		tcpMenuSelect		},
    { NULL, NULL },
};

/*
 * Helper routines for buffering data.
 *
 * We read an entire configuration into memory before executing it
 * so that we are truely standalone and can do things like nuke the
 * file or disk we're working on.
 */

typedef struct command_buffer_ {
    qelement	queue;
    char *	string;
} command_buffer;

static void
dispatch_free_command(command_buffer *item)
{
    if (item != NULL) {
	REMQUE(item);
	free(item->string);
	item->string = NULL;
    }

    free(item);
}

static void
dispatch_free_all(qelement *head)
{
    command_buffer *item;

    while (!EMPTYQUE(*head)) {
	item = (command_buffer *) head->q_forw;
	dispatch_free_command(item);
    }
}

static command_buffer *
dispatch_add_command(qelement *head, char *string)
{
    command_buffer *new = NULL;

    new = malloc(sizeof(command_buffer));

    if (new != NULL) {

	new->string = strdup(string);

	/*
	 * We failed to copy `string'; clean up the allocated
	 * resources.
	 */
	if (new->string == NULL) {
	    free(new);
	    new = NULL;
	} else {
	    INSQUEUE(new, head->q_back);
	}
    }

    return new;
}

/*
 * Command processing
 */

/* Just convenience */
static int
dispatch_shutdown(dialogMenuItem *unused)
{
    systemShutdown(0);
    return DITEM_FAILURE;
}

static int
dispatch_systemExecute(dialogMenuItem *unused)
{
    char *cmd = variable_get(VAR_COMMAND);

    if (cmd)
	return systemExecute(cmd) ? DITEM_FAILURE : DITEM_SUCCESS;
    else
	msgDebug("_systemExecute: No command passed in `command' variable.\n");
    return DITEM_FAILURE;
}

static int
dispatch_msgConfirm(dialogMenuItem *unused)
{
    char *msg = variable_get(VAR_COMMAND);

    if (msg) {
	msgConfirm("%s", msg);
	return DITEM_SUCCESS;
    }

    msgDebug("_msgConfirm: No message passed in `command' variable.\n");
    return DITEM_FAILURE;
}

static int
dispatch_mediaOpen(dialogMenuItem *unused)
{
    return mediaOpen();
}

static int
dispatch_mediaClose(dialogMenuItem *unused)
{
    mediaClose();
    return DITEM_SUCCESS;
}

static int
call_possible_resword(char *name, dialogMenuItem *value, int *status)
{
    int i, rval;

    rval = 0;
    for (i = 0; resWords[i].name; i++) {
	if (!strcmp(name, resWords[i].name)) {
	    *status = resWords[i].handler(value);
	    rval = 1;
	    break;
	}
    }
    return rval;
}

/* For a given string, call it or spit out an undefined command diagnostic */
int
dispatchCommand(char *str)
{
    int i;
    char *cp;

    if (!str || !*str) {
	msgConfirm("Null or zero-length string passed to dispatchCommand");
	return DITEM_FAILURE;
    }

    /* Fixup DOS abuse */
    if ((cp = index(str, '\r')) != NULL)
	*cp = '\0';

    /* If it's got a `=' sign in there, assume it's a variable setting */
    if (index(str, '=')) {
	if (isDebug())
	    msgDebug("dispatch: setting variable `%s'\n", str);
	variable_set(str, 0);
	i = DITEM_SUCCESS;
    }
    else {
	/* A command might be a pathname if it's encoded in argv[0], which
	   we also support */
	if ((cp = rindex(str, '/')) != NULL)
	    str = cp + 1;
	if (isDebug())
	    msgDebug("dispatch: calling resword `%s'\n", str);
	if (!call_possible_resword(str, NULL, &i)) {
	    msgNotify("Warning: No such command ``%s''", str);
	    i = DITEM_FAILURE;
	}
	/*
	 * Allow a user to prefix a command with "noError" to cause
	 * us to ignore any errors for that one command.
	 */
	if (i != DITEM_SUCCESS && variable_get(VAR_NO_ERROR))
	    i = DITEM_SUCCESS;
	variable_unset(VAR_NO_ERROR);
    }
    return i;
}


/*
 * File processing
 */

static qelement *
dispatch_load_fp(FILE *fp)
{
    qelement *head;
    char buf[BUFSIZ], *cp;

    head = malloc(sizeof(qelement));

    if (!head)
	return NULL;

    INITQUE(*head);

    while (fgets(buf, sizeof buf, fp)) {
	/* Fix up DOS abuse */
	if ((cp = index(buf, '\r')) != NULL)
	    *cp = '\0';
	/* If it's got a new line, trim it */
       if ((cp = index(buf, '\n')) != NULL)
            *cp = '\0';
	if (*buf == '\0' || *buf == '#')
	    continue;

	if (!dispatch_add_command(head, buf))
	    return NULL;
    }

    return head;
}

static int
dispatch_execute(qelement *head)
{
    int result = DITEM_SUCCESS;
    command_buffer *item;
    char *old_interactive;

    if (!head)
	return result | DITEM_FAILURE;

    old_interactive = variable_get(VAR_NONINTERACTIVE);
    if (old_interactive)
	 old_interactive = strdup(old_interactive);	/* save copy */

    /* Hint to others that we're running from a script, should they care */
    variable_set2(VAR_NONINTERACTIVE, "yes", 0);

    while (!EMPTYQUE(*head)) {
	item = (command_buffer *) head->q_forw;

	if (DITEM_STATUS(dispatchCommand(item->string)) != DITEM_SUCCESS) {
	    msgConfirm("Command `%s' failed - rest of script aborted.\n",
		       item->string);
	    result |= DITEM_FAILURE;
	    break;
	}
	dispatch_free_command(item);
    }

    dispatch_free_all(head);

    if (!old_interactive)
	variable_unset(VAR_NONINTERACTIVE);
    else {
	variable_set2(VAR_NONINTERACTIVE, old_interactive, 0);
	free(old_interactive);
    }

    return result;
}

int
dispatch_load_file_int(int quiet)
{
    FILE *fp;
    char *cp;
    int  i;
    qelement *list;

    static const char *names[] = {
	"install.cfg",
	"/stand/install.cfg",
	"/tmp/install.cfg",
	NULL
    };

    fp = NULL;
    cp = variable_get(VAR_CONFIG_FILE);
    if (!cp) {
	for (i = 0; names[i]; i++)
	    if ((fp = fopen(names[i], "r")) != NULL)
		break;
    } else
	fp = fopen(cp, "r");

    if (!fp) {
	if (!quiet)
	    msgConfirm("Unable to open %s: %s", cp, strerror(errno));
	return DITEM_FAILURE;
    }

    list = dispatch_load_fp(fp);
    fclose(fp);

    return dispatch_execute(list);
}

int
dispatch_load_file(dialogMenuItem *self)
{
    return dispatch_load_file_int(FALSE);
}

int
dispatch_load_floppy(dialogMenuItem *self)
{
    int             what = DITEM_SUCCESS;
    extern char    *distWanted;
    char           *cp;
    FILE           *fp;
    qelement	   *list;

    mediaClose();
    cp = variable_get_value(VAR_INSTALL_CFG,
			    "Specify the name of a configuration file", 0);
    if (!cp || !*cp) {
	variable_unset(VAR_INSTALL_CFG);
	what |= DITEM_FAILURE;
	return what;
    }

    distWanted = cp;
    /* Try to open the floppy drive */
    if (DITEM_STATUS(mediaSetFloppy(NULL)) == DITEM_FAILURE) {
	msgConfirm("Unable to set media device to floppy.");
	what |= DITEM_FAILURE;
	mediaClose();
	return what;
    }

    if (!DEVICE_INIT(mediaDevice)) {
	msgConfirm("Unable to mount floppy filesystem.");
	what |= DITEM_FAILURE;
	mediaClose();
	return what;
    }

    fp = DEVICE_GET(mediaDevice, cp, TRUE);
    if (fp) {
	list = dispatch_load_fp(fp);
	fclose(fp);
	mediaClose();

	what |= dispatch_execute(list);
    }
    else {
	if (!variable_get(VAR_NO_ERROR))
	    msgConfirm("Configuration file '%s' not found.", cp);
	variable_unset(VAR_INSTALL_CFG);
	what |= DITEM_FAILURE;
	mediaClose();
    }
    return what;
}

int
dispatch_load_cdrom(dialogMenuItem *self)
{
    int             what = DITEM_SUCCESS;
    extern char    *distWanted;
    char           *cp;
    FILE           *fp;
    qelement	   *list;

    mediaClose();
    cp = variable_get_value(VAR_INSTALL_CFG,
			    "Specify the name of a configuration file\n"
			    "residing on the CDROM.", 0);
    if (!cp || !*cp) {
	variable_unset(VAR_INSTALL_CFG);
	what |= DITEM_FAILURE;
	return what;
    }

    distWanted = cp;
    /* Try to open the floppy drive */
    if (DITEM_STATUS(mediaSetCDROM(NULL)) == DITEM_FAILURE) {
	msgConfirm("Unable to set media device to CDROM.");
	what |= DITEM_FAILURE;
	mediaClose();
	return what;
    }

    if (!DEVICE_INIT(mediaDevice)) {
	msgConfirm("Unable to CDROM filesystem.");
	what |= DITEM_FAILURE;
	mediaClose();
	return what;
    }

    fp = DEVICE_GET(mediaDevice, cp, TRUE);
    if (fp) {
	list = dispatch_load_fp(fp);
	fclose(fp);
	mediaClose();

	what |= dispatch_execute(list);
    }
    else {
	if (!variable_get(VAR_NO_ERROR))
	    msgConfirm("Configuration file '%s' not found.", cp);
	variable_unset(VAR_INSTALL_CFG);
	what |= DITEM_FAILURE;
	mediaClose();
    }
    return what;
}

/*
 * Create a menu based on available disk devices
 */
int
dispatch_load_menu(dialogMenuItem *self)
{
    DMenu	*menu;
    Device	**devlist;
    char	*err;
    int		what, i, j, msize, count;
    DeviceType	dtypes[] = {DEVICE_TYPE_FLOPPY, DEVICE_TYPE_CDROM,
	DEVICE_TYPE_DOS, DEVICE_TYPE_UFS, DEVICE_TYPE_USB};

    fprintf(stderr, "dispatch_load_menu called\n");

    msize = sizeof(DMenu) + (sizeof(dialogMenuItem) * 2);
    count = 0;
    err = NULL;
    what = DITEM_SUCCESS;

    if ((menu = malloc(msize)) == NULL) {
	err = "Failed to allocate memory for menu";
	goto errout;
    }

    bcopy(&MenuConfig, menu, sizeof(DMenu));

    bzero(&menu->items[count], sizeof(menu->items[0]));
    menu->items[count].prompt = strdup("X Exit");
    menu->items[count].title = strdup("Exit this menu (returning to previous)");
    menu->items[count].fire = dmenuExit;
    count++;

    for (i = 0; i < sizeof(dtypes) / sizeof(dtypes[0]); i++) {
	if ((devlist = deviceFind(NULL, dtypes[i])) == NULL) {
	    fprintf(stderr, "No devices found for type %d\n", dtypes[i]);
	    continue;
	}

	for (j = 0; devlist[j] != NULL; j++) {
	    fprintf(stderr, "device type %d device name %s\n", dtypes[i], devlist[j]->name);
	    msize += sizeof(dialogMenuItem);
	    if ((menu = realloc(menu, msize)) == NULL) {
		err = "Failed to allocate memory for menu item";
		goto errout;
	    }

	    bzero(&menu->items[count], sizeof(menu->items[0]));
	    menu->items[count].fire = cfgModuleFire;

	    menu->items[count].prompt = strdup(devlist[j]->name);
	    menu->items[count].title = strdup(devlist[j]->description);
	    /* XXX: dialog(3) sucks */
	    menu->items[count].aux = (long)devlist[j];
	    count++;
	}
    }

    menu->items[count].prompt = NULL;
    menu->items[count].title = NULL;

    dmenuOpenSimple(menu, FALSE);

  errout:
    for (i = 0; i < count; i++) {
	free(menu->items[i].prompt);
	free(menu->items[i].title);
    }

    free(menu);

    if (err != NULL) {
	what |= DITEM_FAILURE;
	if (!variable_get(VAR_NO_ERROR))
	    msgConfirm("%s", err);
    }

    return (what);

}

static int
cfgModuleFire(dialogMenuItem *self) {
    Device	*d;
    FILE	*fp;
    int		what = DITEM_SUCCESS;
    extern char *distWanted;
    qelement	*list;
    char	*cp;

    d = (Device *)self->aux;

    msgDebug("cfgModuleFire: User selected %s (%s)\n", self->prompt, d->devname);

    mediaClose();

    cp = variable_get_value(VAR_INSTALL_CFG,
			    "Specify the name of a configuration file", 0);
    if (!cp || !*cp) {
	variable_unset(VAR_INSTALL_CFG);
	what |= DITEM_FAILURE;
	return what;
    }

    distWanted = cp;

    mediaDevice = d;
    if (!DEVICE_INIT(mediaDevice)) {
	msgConfirm("Unable to mount filesystem.");
	what |= DITEM_FAILURE;
	mediaClose();
	return what;
    }
    msgDebug("getting fp for %s\n", cp);

    fp = DEVICE_GET(mediaDevice, cp, TRUE);
    if (fp) {
	msgDebug("opened OK, processing..\n");

	list = dispatch_load_fp(fp);
	fclose(fp);
	mediaClose();

	what |= dispatch_execute(list);
    } else {
	if (!variable_get(VAR_NO_ERROR))
	    msgConfirm("Configuration file '%s' not found.", cp);
	variable_unset(VAR_INSTALL_CFG);
	what |= DITEM_FAILURE;
	mediaClose();
    }

    return(what);
 }
