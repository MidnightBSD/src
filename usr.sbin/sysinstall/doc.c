/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated for what's essentially a complete rewrite.
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
 * $FreeBSD$
 */

#include "sysinstall.h"

/*
 * This is called from the main menu.  Try to find a copy of Lynx from somewhere
 * and fire it up on the first copy of the handbook we can find.
 */
int
docBrowser(dialogMenuItem *self)
{
    int ret;
    char *browser = variable_get(VAR_BROWSER_PACKAGE);

    if (RunningAsInit && !strstr(variable_get(SYSTEM_STATE), "install")) {
	msgConfirm("This option may only be used after the system is installed, sorry!");
	return DITEM_FAILURE;
    }

    /* First, make sure we have whatever browser we've chosen is here */
    if (!package_installed(browser)) {
	ret = package_add(browser);
    	if (DITEM_STATUS(ret) != DITEM_SUCCESS) {
	    msgConfirm("Unable to install the %s HTML browser package.  You may\n"
		       "wish to verify that your media is configured correctly and\n"
		       "try again.", browser);
	    return ret;
	}
    }

    if (!file_executable(variable_get(VAR_BROWSER_BINARY))) {
	if (!msgYesNo("Hmmm.  The %s package claims to have installed, but I can't\n"
		      "find its binary in %s!  You may wish to try a different\n"
		      "location to load the package from (go to Media menu) and see if that\n"
		      "makes a difference.\n\n"
		      "I suggest that we remove the version that was extracted since it does\n"
		      "not appear to be correct.   Would you like me to do that now?",
		      browser, variable_get(VAR_BROWSER_BINARY)))
	    vsystem("pkg_delete %s %s", !strcmp(variable_get(VAR_CPIO_VERBOSITY), "high") ? "-v" : "", browser);
	return DITEM_FAILURE;
    }

    /* Run browser on the appropriate doc */
    if (dmenuOpenSimple(&MenuHTMLDoc, FALSE))
	return DITEM_SUCCESS;
    else
	return DITEM_FAILURE;
}

/* Try to show one of the documents requested from the HTML doc menu */
int
docShowDocument(dialogMenuItem *self)
{
    char tmp[512], target[512];
    char *where, *browser = variable_get(VAR_BROWSER_BINARY);
    char *str = self->prompt;

    if (!file_executable(browser)) {
	msgConfirm("Can't find the browser in %s!  Please ensure that it's\n"
		   "properly set in the Options editor.", browser);
	return DITEM_FAILURE;
    }
    /* Default to Home */
    where = strcpy(target, "http://www.freebsd.org");
    if (strstr(str, "Other")) {
	where = msgGetInput("http://www.freebsd.org", "Please enter the URL of the location you wish to visit.");
	if (where)
	    strcpy(target, where);
    }
    else if (strstr(str, "FAQ")) {
	where = strcpy(target, "/usr/local/share/doc/freebsd/faq/index.html");
	if (!file_readable(target))
	    where = strcpy(target, "http://www.freebsd.org/doc/en_US.ISO8859-1/books/faq");
    }
    else if (strstr(str, "Handbook")) {
	where = strcpy(target, "/usr/local/share/doc/freebsd/handbook/index.html");
	if (!file_readable(target))
	    where = strcpy(target, "http://www.freebsd.org/doc/en_US.ISO8859-1/books/handbook");
    }
    if (where) {
	sprintf(tmp, "%s %s", browser, target);
	systemExecute(tmp);
	return DITEM_SUCCESS;
    }
    else {
	msgConfirm("Hmmmmm!  I can't seem to access the documentation you selected!\n"
		   "Have you installed the english documentation?  Is your network connected?");
	return DITEM_FAILURE;
    }
}
