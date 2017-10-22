/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

#include "sysinstall.h"

/* Miscellaneous work routines for menus */
static int
setSrc(dialogMenuItem *self)
{
    Dists |= DIST_SRC;
    SrcDists = DIST_SRC_ALL;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
clearSrc(dialogMenuItem *self)
{
    Dists &= ~DIST_SRC;
    SrcDists = 0;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
setKernel(dialogMenuItem *self)
{
    Dists |= DIST_KERNEL;
    KernelDists = DIST_KERNEL_ALL;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
clearKernel(dialogMenuItem *self)
{
    Dists &= ~DIST_KERNEL;
    KernelDists = 0;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
setDocAll(dialogMenuItem *self)
{
    Dists |= DIST_DOC;
    DocDists = DIST_DOC_ALL;
    return DITEM_SUCCESS | DITEM_REDRAW;
}


#define _IS_SET(dist, set) (((dist) & (set)) == (set))

#define IS_DEVELOPER(dist, extra) (_IS_SET(dist, _DIST_DEVELOPER | extra) || \
	_IS_SET(dist, _DIST_DEVELOPER | extra))

#define IS_USER(dist, extra) (_IS_SET(dist, _DIST_USER | extra) || \
	_IS_SET(dist, _DIST_USER | extra))

static int
checkDistDeveloper(dialogMenuItem *self)
{
    return IS_DEVELOPER(Dists, 0) && _IS_SET(SrcDists, DIST_SRC_ALL);
}

static int
checkDistKernDeveloper(dialogMenuItem *self)
{
    return IS_DEVELOPER(Dists, 0) && _IS_SET(SrcDists, DIST_SRC_SYS);
}

static int
checkDistUser(dialogMenuItem *self)
{
    return IS_USER(Dists, 0);
}

static int
checkDistMinimum(dialogMenuItem *self)
{
    return Dists == (DIST_BASE | DIST_KERNEL);
}

static int
checkDistEverything(dialogMenuItem *self)
{
    return Dists == DIST_ALL &&
	_IS_SET(DocDists, DIST_DOC_ALL) &&
	_IS_SET(SrcDists, DIST_SRC_ALL) &&
	_IS_SET(KernelDists, DIST_KERNEL_ALL);
}

static int
srcFlagCheck(dialogMenuItem *item)
{
    return SrcDists;
}

static int
kernelFlagCheck(dialogMenuItem *item)
{
    return KernelDists;
}

static int
docFlagCheck(dialogMenuItem *item)
{
    return DocDists;
}

static int
checkTrue(dialogMenuItem *item)
{
    return TRUE;
}

/* All the system menus go here.
 *
 * Hardcoded things like version number strings will disappear from
 * these menus just as soon as I add the code for doing inline variable
 * expansion.
 */

DMenu MenuIndex = {
    DMENU_NORMAL_TYPE,
    "Glossary of functions",
    "This menu contains an alphabetized index of the top level functions in\n"
    "this program (sysinstall).  Invoke an option by pressing [SPACE] or\n"
    "[ENTER].  To exit, use [TAB] to move to the Cancel button.",
    "Use PageUp or PageDown to move through this menu faster!",
    NULL,
    { { " Anon FTP",		"Configure anonymous FTP logins.",	dmenuVarCheck, configAnonFTP, NULL, "anon_ftp" },
      { " Commit",		"Commit any pending actions (dangerous!)", NULL, installCustomCommit },
      { " Country",		"Set the system's country",		NULL, configCountry },
#ifdef WITH_SYSCONS
      { " Console settings",	"Customize system console behavior.",	NULL, dmenuSubmenu, NULL, &MenuSyscons },
#endif
      { " Configure",		"The system configuration menu.",	NULL, dmenuSubmenu, NULL, &MenuConfigure },
      { " Defaults, Load (FDD)","Load default settings from floppy.",	NULL, dispatch_load_floppy },
      { " Defaults, Load (CD)",	"Load default settings from CDROM.",	NULL, dispatch_load_cdrom },
      { " Defaults, Load",	"Load default settings (all devices).",	NULL, dispatch_load_menu },
#ifdef WITH_MICE
      { " Device, Mouse",	"The mouse configuration menu.",	NULL, dmenuSubmenu, NULL, &MenuMouse },
#endif
      { " Disklabel",		"The disk label editor",		NULL, diskLabelEditor },
      { " Dists, All",		"Root of the distribution tree.",	NULL, dmenuSubmenu, NULL, &MenuDistributions },
      { " Dists, Basic",		"Basic FreeBSD distribution menu.",	NULL, dmenuSubmenu, NULL, &MenuSubDistributions },
      { " Dists, Developer",	"Select developer's distribution.",	checkDistDeveloper, distSetDeveloper },
      { " Dists, Src",		"Src distribution menu.",		NULL, dmenuSubmenu, NULL, &MenuSrcDistributions },
      { " Dists, Kern Developer", "Select kernel developer's distribution.", checkDistKernDeveloper, distSetKernDeveloper },
      { " Dists, User",		"Select average user distribution.",	checkDistUser, distSetUser },
      { " Distributions, Adding", "Installing additional distribution sets", NULL, distExtractAll },
      { " Documentation",	"Installation instructions, README, etc.", NULL, dmenuSubmenu, NULL, &MenuDocumentation },
      { " Documentation Installation",	"Installation of FreeBSD documentation set", NULL, distSetDocMenu },
      { " Doc, README",		"The distribution README file.",	NULL, dmenuDisplayFile, NULL, "README" },
      { " Doc, Errata",		"The distribution errata.",	NULL, dmenuDisplayFile, NULL, "ERRATA" },
      { " Doc, Hardware",	"The distribution hardware guide.",	NULL, dmenuDisplayFile,	NULL, "HARDWARE" },
      { " Doc, Copyright",	"The distribution copyright notices.",	NULL, dmenuDisplayFile,	NULL, "COPYRIGHT" },
      { " Doc, Release",		"The distribution release notes.",	NULL, dmenuDisplayFile, NULL, "RELNOTES" },
      { " Doc, HTML",		"The HTML documentation menu.",		NULL, docBrowser },
      { " Dump Vars",		"(debugging) dump out internal variables.", NULL, dump_variables },
      { " Emergency shell",	"Start an Emergency Holographic shell.",	NULL, installFixitHoloShell },
#ifdef WITH_SLICES
      { " Fdisk",		"The disk Partition Editor",		NULL, diskPartitionEditor },
#endif
      { " Fixit",		"Repair mode with CDROM or fixit floppy.",	NULL, dmenuSubmenu, NULL, &MenuFixit },
      { " FTP sites",		"The FTP mirror site listing.",		NULL, dmenuSubmenu, NULL, &MenuMediaFTP },
      { " Gateway",		"Set flag to route packets between interfaces.", dmenuVarCheck, dmenuToggleVariable, NULL, "gateway=YES" },
      { " HTML Docs",		"The HTML documentation menu",		NULL, docBrowser },
      { " inetd Configuration",	"Configure inetd and simple internet services.",	dmenuVarCheck, configInetd, NULL, "inetd_enable=YES" },
      { " Install, Standard",	"A standard system installation.",	NULL, installStandard },
      { " Install, Express",	"An express system installation.",	NULL, installExpress },
      { " Install, Custom",	"The custom installation menu",		NULL, dmenuSubmenu, NULL, &MenuInstallCustom },
      { " Label",		"The disk Label editor",		NULL, diskLabelEditor },
      { " Media",		"Top level media selection menu.",	NULL, dmenuSubmenu, NULL, &MenuMedia },
      { " Media, NFS",		"Select NFS installation media.",	NULL, mediaSetNFS },
      { " Media, Floppy",	"Select floppy installation media.",	NULL, mediaSetFloppy },
      { " Media, CDROM/DVD",	"Select CDROM/DVD installation media.",	NULL, mediaSetCDROM },
      { " Media, USB",		"Select USB installation media.",	NULL, mediaSetUSB },
      { " Media, DOS",		"Select DOS installation media.",	NULL, mediaSetDOS },
      { " Media, UFS",		"Select UFS installation media.",	NULL, mediaSetUFS },
      { " Media, FTP",		"Select FTP installation media.",	NULL, mediaSetFTP },
      { " Media, FTP Passive",	"Select passive FTP installation media.", NULL, mediaSetFTPPassive },
      { " Media, HTTP",		"Select FTP via HTTP proxy install media.", NULL, mediaSetHTTP },
      { " Network Interfaces",	"Configure network interfaces",		NULL, tcpMenuSelect },
      { " Networking Services",	"The network services menu.",		NULL, dmenuSubmenu, NULL, &MenuNetworking },
      { " NFS, client",		"Set NFS client flag.",			dmenuVarCheck, dmenuToggleVariable, NULL, "nfs_client_enable=YES" },
      { " NFS, server",		"Set NFS server flag.",			dmenuVarCheck, configNFSServer, NULL, "nfs_server_enable=YES" },
      { " NTP Menu",		"The NTP configuration menu.",		NULL, dmenuSubmenu, NULL, &MenuNTP },
      { " Options",		"The options editor.",			NULL, optionsEditor },
      { " Packages",		"The packages collection",		NULL, configPackages },
#ifdef WITH_SLICES
      { " Partition",		"The disk slice (PC-style partition) editor",	NULL, diskPartitionEditor },
#endif
      { " PCNFSD",		"Run authentication server for PC-NFS.", dmenuVarCheck, configPCNFSD, NULL, "pcnfsd" },
      { " Root Password",	"Set the system manager's password.",   NULL, dmenuSystemCommand, NULL, "passwd root" },
      { " Router",		"Select routing daemon (default: routed)", NULL, configRouter, NULL, "router_enable" },
      { " Security",		"Configure system security options", NULL, dmenuSubmenu, NULL, &MenuSecurity },
#ifdef WITH_SYSCONS
      { " Syscons",		"The system console configuration menu.", NULL, dmenuSubmenu, NULL, &MenuSyscons },
#ifndef PC98
      { " Syscons, Font",	"The console screen font.",	  NULL, dmenuSubmenu, NULL, &MenuSysconsFont },
#endif
      { " Syscons, Keymap",	"The console keymap configuration menu.", NULL, keymapMenuSelect },
      { " Syscons, Keyrate",	"The console key rate configuration menu.", NULL, dmenuSubmenu, NULL, &MenuSysconsKeyrate },
      { " Syscons, Saver",	"The console screen saver configuration menu.",	NULL, dmenuSubmenu, NULL, &MenuSysconsSaver },
#ifndef PC98
      { " Syscons, Screenmap",	"The console screenmap configuration menu.", NULL, dmenuSubmenu, NULL, &MenuSysconsScrnmap },
      { " Syscons, Ttys",       "The console terminal type menu.", NULL, dmenuSubmenu, NULL, &MenuSysconsTtys },
#endif
#endif /* WITH_SYSCONS */
      { " Time Zone",		"Set the system's time zone.",		NULL, dmenuSystemCommand, NULL, "tzsetup" },
      { " TTYs",		"Configure system ttys.",		NULL, configEtcTtys, NULL, "ttys" },
      { " Upgrade",		"Upgrade an existing system.",		NULL, installUpgrade },
      { " Usage",		"Quick start - How to use this menu system.",	NULL, dmenuDisplayFile, NULL, "usage" },
      { " User Management",	"Add user and group information.",	NULL, dmenuSubmenu, NULL, &MenuUsermgmt },
      { NULL } },
};

/* The country menu */
#include "countries.h"

/* The initial installation menu */
DMenu MenuInitial = {
    DMENU_NORMAL_TYPE,
    "sysinstall Main Menu",				/* title */
    "Welcome to the FreeBSD installation and configuration tool.  Please\n" /* prompt */
    "select one of the options below by using the arrow keys or typing the\n"
    "first character of the option name you're interested in.  Invoke an\n"
    "option with [SPACE] or [ENTER].  To exit, use [TAB] to move to Exit.", 
    NULL,
    NULL,
    { { " Select " },
      { "X Exit Install",	NULL, NULL, dmenuExit },
      { " Usage",	"Quick start - How to use this menu system",	NULL, dmenuDisplayFile, NULL, "usage" },
      { "Standard",	"Begin a standard installation (recommended)",	NULL, installStandard },
      { "Express",	"Begin a quick installation (for experts)", NULL, installExpress },
      { " Custom",	"Begin a custom installation (for experts)",	NULL, dmenuSubmenu, NULL, &MenuInstallCustom },
      { "Configure",	"Do post-install configuration of FreeBSD",	NULL, dmenuSubmenu, NULL, &MenuConfigure },
      { "Doc",	"Installation instructions, README, etc.",	NULL, dmenuSubmenu, NULL, &MenuDocumentation },
#ifdef WITH_SYSCONS
      { "Keymap",	"Select keyboard type",				NULL, keymapMenuSelect },
#endif
      { "Options",	"View/Set various installation options",	NULL, optionsEditor },
      { "Fixit",	"Repair mode with CDROM/DVD/floppy or start shell",	NULL, dmenuSubmenu, NULL, &MenuFixit },
      { "Upgrade",	"Upgrade an existing system",			NULL, installUpgrade },
      { "Load Config..","Load default install configuration",		NULL, dispatch_load_menu },
      { "Index",	"Glossary of functions",			NULL, dmenuSubmenu, NULL, &MenuIndex },
      { NULL } },
};

/* The main documentation menu */
DMenu MenuDocumentation = {
    DMENU_NORMAL_TYPE,
    "FreeBSD Documentation Menu",
    "If you are at all unsure about the configuration of your hardware\n"
    "or are looking to build a system specifically for FreeBSD, read the\n"
    "Hardware guide!  New users should also read the Install document for\n"
    "a step-by-step tutorial on installing FreeBSD.  For general information,\n"
    "consult the README file.",
    "Confused?  Press F1 for help.",
    "usage",
    { { "X Exit",	"Exit this menu (returning to previous)",	NULL, dmenuExit },
      { "1 README",	"A general description of FreeBSD.  Read this!", NULL, dmenuDisplayFile, NULL, "README" },
      { "2 Errata",	"Late-breaking, post-release news.", NULL, dmenuDisplayFile, NULL, "ERRATA" },
      { "3 Hardware",	"The FreeBSD survival guide for PC hardware.",	NULL, dmenuDisplayFile,	NULL, "HARDWARE" },
      { "4 Copyright",	"The FreeBSD Copyright notices.",		NULL, dmenuDisplayFile,	NULL, "COPYRIGHT" },
      { "5 Release"	,"The release notes for this version of FreeBSD.", NULL, dmenuDisplayFile, NULL, "RELNOTES" },
      { "6 Shortcuts",	"Creating shortcuts to sysinstall.",		NULL, dmenuDisplayFile, NULL, "shortcuts" },
      { "7 HTML Docs",	"Go to the HTML documentation menu (post-install).", NULL, docBrowser },
      { NULL } },
};

/* The FreeBSD documentation installation menu */
DMenu MenuDocInstall = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "FreeBSD Documentation Installation Menu",
    "This menu will allow you to install the whole documentation set\n"
    "from the FreeBSD Documentation Project: Handbook, FAQ and articles.\n\n"
    "Please select the language versions you wish to install.  At minimum,\n"
    "you should install the English version, this is the original version\n"
    "of the documentation.",
    NULL,
    NULL,
    { { "X Exit",	"Exit this menu (returning to previous)",
	      checkTrue,      dmenuExit, NULL, NULL, '<', '<', '<' },
      { "All",		"Select all below",
	      NULL,	      setDocAll, NULL, NULL, ' ', ' ', ' ' },
      { " bn", 	"Bengali Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_BN },
      { " da", 	"Danish Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_DA },
      { " de", 	"German Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_DE },
      { " el", 	"Greek Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_EL },
      { " en", 	"English Documentation (recommended)",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_EN },
      { " es", 	"Spanish Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_ES },
      { " fr", 	"French Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_FR },
      { " hu", 	"Hungarian Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_HU },
      { " it", 	"Italian Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_IT },
      { " ja", 	"Japanese Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_JA },
      { " mn", 	"Mongolian Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_MN },
      { " nl", 	"Dutch Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_NL },
      { " pl", 	"Polish Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_PL },
      { " pt", 	"Portuguese Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_PT },
      { " ru", 	"Russian Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_RU },
      { " sr", 	"Serbian Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_SR },
      { " tr", 	"Turkish Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_TR },
      { " zh_cn", 	"Simplified Chinese Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_ZH_CN },
      { " zh_tw", 	"Traditional Chinese Documentation",
	      dmenuFlagCheck, dmenuSetFlag, NULL, &DocDists, '[', 'X', ']', DIST_DOC_ZH_TW },
      { NULL } },
};

#ifdef WITH_MICE
DMenu MenuMouseType = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
#ifdef PC98
    "Select a protocol type for your mouse",
    "If your mouse is attached to the bus mouse port, you should always choose\n"
    "\"Auto\", regardless of the model and the brand of the mouse.  All other\n"
    "protocol types are for serial mice and should not be used with the bus\n"
    "mouse.  If you have a serial mouse and are not sure about its protocol,\n"
    "you should also try \"Auto\".  It may not work for the serial mouse if the\n"
    "mouse does not support the PnP standard.  But, it won't hurt.  Many\n"
    "2-button serial mice are compatible with \"Microsoft\" or \"MouseMan\".\n"
    "3-button serial mice may be compatible with \"MouseSystems\" or \"MouseMan\".\n"
    "If the serial mouse has a wheel, it may be compatible with \"IntelliMouse\".",
    NULL,
    NULL,
    { { "1 Auto",	"Bus mouse or PnP serial mouse",	
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=auto" },
#else
    "Select a protocol type for your mouse",
    "If your mouse is attached to the PS/2 mouse port or the bus mouse port,\n"
    "you should always choose \"Auto\", regardless of the model and the brand\n"
    "of the mouse.  All other protocol types are for serial mice and should\n"
    "not be used with the PS/2 port mouse or the bus mouse.  If you have\n"
    "a serial mouse and are not sure about its protocol, you should also try\n"
    "\"Auto\".  It may not work for the serial mouse if the mouse does not\n"
    "support the PnP standard.  But, it won't hurt.  Many 2-button serial mice\n"
    "are compatible with \"Microsoft\" or \"MouseMan\".  3-button serial mice\n"
    "may be compatible with \"MouseSystems\" or \"MouseMan\".  If the serial\n"
    "mouse has a wheel, it may be compatible with \"IntelliMouse\".",
    NULL,
    NULL,
    { { "1 Auto",	"Bus mouse, PS/2 style mouse or PnP serial mouse",	
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=auto" },
#endif /* PC98 */
      { "2 GlidePoint",	"ALPS GlidePoint pad (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=glidepoint" },
      { "3 Hitachi","Hitachi tablet (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=mmhittab" },
      { "4 IntelliMouse",	"Microsoft IntelliMouse (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=intellimouse" },
      { "5 Logitech",	"Logitech protocol (old models) (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=logitech" },
      { "6 Microsoft",	"Microsoft protocol (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=microsoft" },
      { "7 MM Series","MM Series protocol (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=mmseries" },
      { "8 MouseMan",	"Logitech MouseMan/TrackMan models (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=mouseman" },
      { "9 MouseSystems",	"MouseSystems protocol (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=mousesystems" },
      { "A ThinkingMouse","Kensington ThinkingMouse (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=thinkingmouse" },
      { NULL } },
};

#ifdef PC98
DMenu MenuMousePort = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Select your mouse port from the following menu",
    "The built-in pointing device of laptop/notebook computers is usually\n"
    "a BusMouse style device.",
    NULL,
    NULL,
    {
      { "1 BusMouse",	"PC-98x1 bus mouse (/dev/mse0)", 
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/mse0" },
      { "2 COM1",	"Serial mouse on COM1 (/dev/cuau0)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/cuau0" },
      { "3 COM2",	"Serial mouse on COM2 (/dev/cuau1)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/cuau1" },
      { NULL } },
};
#else
DMenu MenuMousePort = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Select your mouse port from the following menu",
    "The built-in pointing device of laptop/notebook computers is usually\n"
    "a PS/2 style device.",
    NULL,
    NULL,
    { { "1 PS/2",	"PS/2 style mouse (/dev/psm0)", 
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/psm0" },
      { "2 COM1",	"Serial mouse on COM1 (/dev/cuau0)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/cuau0" },
      { "3 COM2",	"Serial mouse on COM2 (/dev/cuau1)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/cuau1" },
      { "4 COM3",	"Serial mouse on COM3 (/dev/cuau2)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/cuau2" },
      { "5 COM4",	"Serial mouse on COM4 (/dev/cuau3)", 
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/cuau3" },
      { "6 BusMouse",	"Logitech, ATI or MS bus mouse (/dev/mse0)", 
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/mse0" },
      { NULL } },
};
#endif /* PC98 */

DMenu MenuMouse = {
    DMENU_NORMAL_TYPE,
    "Please configure your mouse",
    "You can cut and paste text in the text console by running the mouse\n"
    "daemon.  Specify a port and a protocol type of your mouse and enable\n"
    "the mouse daemon.  If you don't want this feature, select 6 to disable\n"
    "the daemon.\n"
    "Once you've enabled the mouse daemon, you can specify \"/dev/sysmouse\"\n"
    "as your mouse device and \"SysMouse\" or \"MouseSystems\" as mouse\n"
    "protocol when running the X configuration utility (see Configuration\n"
    "menu).",
    NULL,
    NULL,
    { { "X Exit", "Exit this menu (returning to previous)", NULL, dmenuExit },
      { "2 Enable",	"Test and run the mouse daemon", NULL, mousedTest, NULL, NULL },
      { "3 Type",	"Select mouse protocol type", NULL, dmenuSubmenu, NULL, &MenuMouseType },
      { "4 Port",	"Select mouse port", NULL, dmenuSubmenu, NULL, &MenuMousePort },
      { "5 Flags",      "Set additional flags", dmenuVarCheck, setMouseFlags,
	NULL, VAR_MOUSED_FLAGS "=" },
      { "6 Disable",	"Disable the mouse daemon", NULL, mousedDisable, NULL, NULL },
      { NULL } },
};
#endif /* WITH_MICE */

DMenu MenuMediaCDROM = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a CD/DVD type",
    "FreeBSD can be installed directly from a CD/DVD containing a valid\n"
    "FreeBSD distribution.  If you are seeing this menu it is because\n"
    "more than one CD/DVD drive was found on your system.  Please select one\n"
    "of the following CD/DVD drives as your installation drive.",
    NULL,
    NULL,
    { { NULL } },
};

DMenu MenuMediaFloppy = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a Floppy drive",
    "You have more than one floppy drive.  Please choose which drive\n"
    "you would like to use.",
    NULL,
    NULL,
    { { NULL } },
};

DMenu MenuMediaUSB = {
	DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
	"Choose a USB drive",
	"You have more than one USB drive. Please choose which drive\n"
	"you would like to use.",
	NULL,
	NULL,
	{ { NULL } },
};

DMenu MenuMediaDOS = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a DOS partition",
    "FreeBSD can be installed directly from a DOS partition\n"
    "assuming, of course, that you have copied the relevant\n"
    "distributions into your DOS partition before starting this\n"
    "installation.  If this is not the case then you should reboot\n"
    "DOS at this time and copy the distributions you wish to install\n"
    "into a \"FREEBSD\" subdirectory on one of your DOS partitions.\n"
    "Otherwise, please select the DOS partition containing the FreeBSD\n"
    "distribution files.",
    NULL,
    NULL,
    { { NULL } },
};

DMenu MenuMediaFTP = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Please select a FreeBSD FTP distribution site",
    "Please select the site closest to you or \"other\" if you'd like to\n"
    "specify a different choice.  Also note that not every site listed here\n"
    "carries more than the base distribution kits. Only Primary sites are\n"
    "guaranteed to carry the full range of possible distributions.",
    "Select a site that's close!",
    NULL,
    { { "Main Site",	"ftp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.freebsd.org" },
      { "URL", "Specify some other ftp site by URL", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=other" },
      { "Snapshots Server Japan", "snapshots.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://snapshots.jp.freebsd.org" },
      { "Snapshots Server Sweden", "snapshots.se.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://snapshots.se.freebsd.org" },

      { "IPv6 Main Site", "ftp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.freebsd.org" },
      { " IPv6 Ireland", "ftp3.ie.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.ie.freebsd.org" },
      { " IPv6 Israel", "ftp.il.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.il.freebsd.org" },
      { " IPv6 Japan", "ftp2.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.jp.freebsd.org" },
      { " IPv6 USA", "ftp4.us.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.us.freebsd.org" },
      { " IPv6 Turkey", "ftp2.tr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.tr.freebsd.org" },

      { "Primary",	"ftp1.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp1.freebsd.org" },
      { " Primary #2",	"ftp2.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.freebsd.org" },
      { " Primary #3",	"ftp3.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.freebsd.org" },
      { " Primary #4",	"ftp4.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.freebsd.org" },
      { " Primary #5",	"ftp5.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.freebsd.org" },
      { " Primary #6",	"ftp6.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.freebsd.org" },
      { " Primary #7",	"ftp7.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp7.freebsd.org" },
      { " Primary #8",	"ftp8.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp8.freebsd.org" },
      { " Primary #9",	"ftp9.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp9.freebsd.org" },
      { " Primary #10",	"ftp10.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp10.freebsd.org" },
      { " Primary #11",	"ftp11.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp11.freebsd.org" },
      { " Primary #12",	"ftp12.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp12.freebsd.org" },
      { " Primary #13",	"ftp13.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp13.freebsd.org" },
      { " Primary #14",	"ftp14.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp14.freebsd.org" },

      { "Argentina",	"ftp.ar.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.ar.freebsd.org" },

      { "Australia",	"ftp.au.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.au.freebsd.org" },
      { " Australia #2","ftp2.au.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.au.freebsd.org" },
      { " Australia #3","ftp3.au.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.au.freebsd.org" },

      { "Austria","ftp.at.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.at.freebsd.org" },
      { " Austria #2","ftp2.at.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.at.freebsd.org" },

      { "Brazil",	"ftp.br.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.br.freebsd.org" },
      { " Brazil #2",	"ftp2.br.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.br.freebsd.org" },
      { " Brazil #3",	"ftp3.br.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.br.freebsd.org" },
      { " Brazil #4",	"ftp4.br.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.br.freebsd.org" },
      { " Brazil #5",	"ftp5.br.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.br.freebsd.org" },
      { " Brazil #6",	"ftp6.br.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.br.freebsd.org" },
      { " Brazil #7",	"ftp7.br.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp7.br.freebsd.org" },

      { "Canada",	"ftp.ca.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.ca.freebsd.org" },

      { "China",	"ftp.cn.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.cn.freebsd.org" },
      { " China #2",	"ftp2.cn.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.cn.freebsd.org" },

      { "Croatia",	"ftp.hr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.hr.freebsd.org" },

      { "Czech Republic", "ftp.cz.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.cz.freebsd.org" },

      { "Denmark",	"ftp.dk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.dk.freebsd.org" },
      { " Denmark #2",	"ftp2.dk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.dk.freebsd.org" },

      { "Estonia",	"ftp.ee.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.ee.freebsd.org" },

      { "Finland",	"ftp.fi.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.fi.freebsd.org" },

      { "France",	"ftp.fr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.fr.freebsd.org" },
      { " France #2",	"ftp2.fr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.fr.freebsd.org" },
      { " France #3",	"ftp3.fr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.fr.freebsd.org" },
      { " France #5",	"ftp5.fr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.fr.freebsd.org" },
      { " France #6",	"ftp6.fr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.fr.freebsd.org" },
      { " France #8",	"ftp8.fr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp8.fr.freebsd.org" },

      { "Germany",	"ftp.de.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.de.freebsd.org" },
      { " Germany #2",	"ftp2.de.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.de.freebsd.org" },
      { " Germany #3",	"ftp3.de.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.de.freebsd.org" },
      { " Germany #4",	"ftp4.de.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.de.freebsd.org" },
      { " Germany #5",	"ftp5.de.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.de.freebsd.org" },
      { " Germany #6",	"ftp6.de.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.de.freebsd.org" },
      { " Germany #7",	"ftp7.de.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp7.de.freebsd.org" },
      { " Germany #8",	"ftp8.de.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp8.de.freebsd.org" },

      { "Greece",	"ftp.gr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.gr.freebsd.org" },
      { " Greece #2",	"ftp2.gr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.gr.freebsd.org" },

      { "Hungary",     "ftp.hu.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.hu.freebsd.org" },

      { "Iceland",	"ftp.is.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.is.freebsd.org" },

      { "Ireland",	"ftp.ie.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.ie.freebsd.org" },
      { " Ireland #2",	"ftp2.ie.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.ie.freebsd.org" },
      { " Ireland #3",	"ftp3.ie.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.ie.freebsd.org" },

      { "Israel",	"ftp.il.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.il.freebsd.org" },

      { "Italy",	"ftp.it.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.it.freebsd.org" },

      { "Japan",	"ftp.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.jp.freebsd.org" },
      { " Japan #2",	"ftp2.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.jp.freebsd.org" },
      { " Japan #3",	"ftp3.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.jp.freebsd.org" },
      { " Japan #4",	"ftp4.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.jp.freebsd.org" },
      { " Japan #5",	"ftp5.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.jp.freebsd.org" },
      { " Japan #6",	"ftp6.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.jp.freebsd.org" },
      { " Japan #7",	"ftp7.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp7.jp.freebsd.org" },
      { " Japan #8",	"ftp8.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp8.jp.freebsd.org" },
      { " Japan #9",	"ftp9.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp9.jp.freebsd.org" },

      { "Korea",	"ftp.kr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.kr.freebsd.org" },
      { " Korea #2",	"ftp2.kr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.kr.freebsd.org" },

      { "Lithuania",	"ftp.lt.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.lt.freebsd.org" },

      { "Netherlands",	"ftp.nl.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.nl.freebsd.org" },
      { " Netherlands #2",	"ftp2.nl.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.nl.freebsd.org" },

      { "Norway",	"ftp.no.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.no.freebsd.org" },
      { " Norway #3",	"ftp3.no.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.no.freebsd.org" },

      { "Poland",	"ftp.pl.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.pl.freebsd.org" },
      { " Poland #2",	"ftp2.pl.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.pl.freebsd.org" },
      { " Poland #5",	"ftp5.pl.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.pl.freebsd.org" },

      { "Portugal",	"ftp.pt.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.pt.freebsd.org" },
      { " Portugal #2",	"ftp2.pt.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.pt.freebsd.org" },
      { " Portugal #4",	"ftp4.pt.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.pt.freebsd.org" },

      { "Romania",	"ftp.ro.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.ro.freebsd.org" },

      { "Russia",	"ftp.ru.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.ru.freebsd.org" },
      { " Russia #2",	"ftp2.ru.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.ru.freebsd.org" },
      { " Russia #3",	"ftp3.ru.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.ru.freebsd.org" },
      { " Russia #4",	"ftp4.ru.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.ru.freebsd.org" },

      { "Singapore",	"ftp.sg.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.sg.freebsd.org" },

      { "Slovak Republic",	"ftp.sk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.sk.freebsd.org" },

      { "Slovenia",	"ftp.si.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.si.freebsd.org" },
      { " Slovenia #2",	"ftp2.si.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.si.freebsd.org" },

      { "South Africa",	"ftp.za.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.za.freebsd.org" },
      { " South Africa #2", "ftp2.za.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.za.freebsd.org" },
      { " South Africa #3", "ftp3.za.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.za.freebsd.org" },
      { " South Africa #4", "ftp4.za.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.za.freebsd.org" },

      { "Spain",	"ftp.es.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.es.freebsd.org" },
      { " Spain #2",	"ftp2.es.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.es.freebsd.org" },
      { " Spain #3",	"ftp3.es.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.es.freebsd.org" },

      { "Sweden",	"ftp.se.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.se.freebsd.org" },
      { " Sweden #2",	"ftp2.se.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.se.freebsd.org" },
      { " Sweden #3",	"ftp3.se.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.se.freebsd.org" },
      { " Sweden #4",	"ftp4.se.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.se.freebsd.org" },
      { " Sweden #5",	"ftp5.se.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.se.freebsd.org" },

      { "Switzerland",	"ftp.ch.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.ch.freebsd.org" },
      { " Switzerland #2",	"ftp2.ch.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.ch.freebsd.org" },

      { "Taiwan",	"ftp.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.tw.freebsd.org" },
      { " Taiwan #2",	"ftp2.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.tw.freebsd.org" },
      { " Taiwan #3",	"ftp3.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.tw.freebsd.org" },
      { " Taiwan #4",   "ftp4.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.tw.freebsd.org" },
      { " Taiwan #6",   "ftp6.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.tw.freebsd.org" },
      { " Taiwan #11",   "ftp11.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp11.tw.freebsd.org" },

      { "Turkey",	"ftp.tr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.tr.freebsd.org" },
      { " Turkey #2",	"ftp2.tr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.tr.freebsd.org" },

      { "UK",		"ftp.uk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.uk.freebsd.org" },
      { " UK #2",	"ftp2.uk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.uk.freebsd.org" },
      { " UK #3",	"ftp3.uk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.uk.freebsd.org" },
      { " UK #4",	"ftp4.uk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.uk.freebsd.org" },
      { " UK #5",	"ftp5.uk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.uk.freebsd.org" },
      { " UK #6",	"ftp6.uk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.uk.freebsd.org" },

      { "Ukraine",	"ftp.ua.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.ua.freebsd.org" },
      { " Ukraine #2",	"ftp2.ua.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.ua.freebsd.org" },
      { " Ukraine #5",	"ftp5.ua.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.ua.freebsd.org" },
      { " Ukraine #6",	"ftp6.ua.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.ua.freebsd.org" },
      { " Ukraine #7",	"ftp7.ua.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp7.ua.freebsd.org" },
      { " Ukraine #8",	"ftp8.ua.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp8.ua.freebsd.org" },

      { "USA #1",	"ftp1.us.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp1.us.freebsd.org" },
      { " USA #2",	"ftp2.us.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.us.freebsd.org" },
      { " USA #3",	"ftp3.us.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.us.freebsd.org" },
      { " USA #4",	"ftp4.us.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.us.freebsd.org" },
      { " USA #5",	"ftp5.us.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.us.freebsd.org" },
      { " USA #6",	"ftp6.us.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.us.freebsd.org" },
      { " USA #7",	"ftp7.us.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp7.us.freebsd.org" },
      { " USA #8",	"ftp8.us.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp8.us.freebsd.org" },
      { " USA #9",	"ftp9.us.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp9.us.freebsd.org" },
      { " USA #10",	"ftp10.us.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp10.us.freebsd.org" },
      { " USA #11",	"ftp11.us.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp11.us.freebsd.org" },
      { " USA #12",	"ftp12.us.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp12.us.freebsd.org" },
      { " USA #13",	"ftp13.us.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp13.us.freebsd.org" },
      { " USA #14",	"ftp14.us.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp14.us.freebsd.org" },
      { " USA #15",	"ftp15.us.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp15.us.freebsd.org" },

      { NULL } }
};

DMenu MenuNetworkDevice = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Network interface information required",
    "Please select the ethernet or PLIP device to configure.\n\n"
    "",
    "Press F1 to read network configuration manual",
    "network_device",
    { { NULL } },
};

/* Prototype KLD load menu */
DMenu MenuKLD = {
    DMENU_NORMAL_TYPE,
    "KLD Menu",
    "Load a KLD from a floppy\n",
    NULL,
    NULL,
    { { NULL } },
};

/* Prototype config file load menu */
DMenu MenuConfig = {
    DMENU_NORMAL_TYPE,
    "Config Menu",
    "Please select the device to load your configuration file from.\n"
    "Note that a USB key will show up as daNs1.",
    NULL,
    NULL,
    { { NULL } },
};

/* The media selection menu */
DMenu MenuMedia = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose Installation Media",
    "FreeBSD can be installed from a variety of different installation\n"
    "media, ranging from floppies to an Internet FTP server.  If you're\n"
    "installing FreeBSD from a supported CD/DVD drive then this is generally\n"
    "the best media to use if you have no overriding reason for using other\n"
    "media.",
    "Press F1 for more information on the various media types",
    "media",
    { { "1 CD/DVD",		"Install from a FreeBSD CD/DVD",	NULL, mediaSetCDROM },
      { "2 FTP",		"Install from an FTP server",		NULL, mediaSetFTPActive },
      { "3 FTP Passive",	"Install from an FTP server through a firewall", NULL, mediaSetFTPPassive },
      { "4 HTTP",		"Install from an FTP server through a http proxy", NULL, mediaSetHTTP },
      { "5 DOS",		"Install from a DOS partition",		NULL, mediaSetDOS },
      { "6 NFS",		"Install over NFS",			NULL, mediaSetNFS },
      { "7 File System",	"Install from an existing filesystem",	NULL, mediaSetUFS },
      { "8 Floppy",		"Install from a floppy disk set",	NULL, mediaSetFloppy },
      { "9 USB",		"Install from a USB drive",		NULL, mediaSetUSB },
      { "X Options",		"Go to the Options screen",		NULL, optionsEditor },
      { NULL } },
};

/* The distributions menu */
DMenu MenuDistributions = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Choose Distributions",
    "As a convenience, we provide several \"canned\" distribution sets.\n"
    "These select what we consider to be the most reasonable defaults for the\n"
    "type of system in question.  If you would prefer to pick and choose the\n"
    "list of distributions yourself, simply select \"Custom\".  You can also\n"
    "pick a canned distribution set and then fine-tune it with the Custom item.\n\n"
    "Choose an item by pressing [SPACE] or [ENTER].  When finished, choose the\n"
    "Exit item or move to the OK button with [TAB].",
    "Press F1 for more information on these options.",
    "distributions",
    { { "X Exit", "Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { "All",			"All system sources and binaries",
	checkDistEverything,	distSetEverything, NULL, NULL, ' ', ' ', ' ' },
      { "Reset",		"Reset selected distribution list to nothing",
	NULL,			distReset, NULL, NULL, ' ', ' ', ' ' },
      { "4 Developer",		"Full sources, binaries and doc but no games", 
	checkDistDeveloper,	distSetDeveloper },
      { "5 Kern-Developer",	"Full binaries and doc, kernel sources only",
	checkDistKernDeveloper, distSetKernDeveloper },
      { "6 User",		"Average user - binaries and doc only",
	checkDistUser,		distSetUser },
      { "7 Minimal",		"The smallest configuration possible",
	checkDistMinimum,	distSetMinimum },
      { "8 Custom",		"Specify your own distribution set",
	NULL,			dmenuSubmenu, NULL, &MenuSubDistributions, '>', '>', '>' },
      { NULL } },
};

DMenu MenuSubDistributions = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Select the distributions you wish to install.",
    "Please check off the distributions you wish to install.  At the\n"
    "very minimum, this should be \"base\".",
    NULL,
    NULL,
    { { "X Exit", "Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { "All",		"All system sources and binaries",
	NULL, distSetEverything, NULL, NULL, ' ', ' ', ' ' },
      { "Reset",	"Reset all of the below",
	NULL, distReset, NULL, NULL, ' ', ' ', ' ' },
      { " base",	"Binary base distribution (required)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_BASE },
      { " kernels",	"Binary kernel distributions (required)",
	kernelFlagCheck,distSetKernel },
      { " dict",	"Spelling checker dictionary files",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_DICT },
      { " doc",		"FreeBSD Documentation set",
	docFlagCheck,	distSetDoc },
      { " docuser",		"Miscellaneous userland docs",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_DOCUSERLAND },
      { " games",	"Games (non-commercial)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_GAMES },
      { " info",	"GNU info files",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_INFO },
#if defined(__amd64__) || defined(__powerpc64__)
      { " lib32",	"32-bit runtime compatibility libraries",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_LIB32 },
#endif
      { " man",		"System manual pages - recommended",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_MANPAGES },
      { " catman",	"Preformatted system manual pages",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_CATPAGES },
      { " proflibs",	"Profiled versions of the libraries",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_PROFLIBS },
      { " src",		"Sources for everything",
	srcFlagCheck,	distSetSrc },
      { " ports",	"The FreeBSD Ports collection",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_PORTS },
      { " local",	"Local additions collection",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_LOCAL},
      { NULL } },
};

DMenu MenuKernelDistributions = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Select the operating system kernels you wish to install.",
    "Please check off those kernels you wish to install.\n",
    NULL,
    NULL,
    { { "X Exit", "Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { "All",		"Select all of the below",
	NULL,		setKernel, NULL, NULL, ' ', ' ', ' ' },
      { "Reset",	"Reset all of the below",
	NULL,		clearKernel, NULL, NULL, ' ', ' ', ' ' },
      { " GENERIC",	"GENERIC kernel configuration",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &KernelDists, '[', 'X', ']', DIST_KERNEL_GENERIC },
      { NULL } },
};

DMenu MenuSrcDistributions = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Select the sub-components of src you wish to install.",
    "Please check off those portions of the FreeBSD source tree\n"
    "you wish to install.",
    NULL,
    NULL,
    { { "X Exit", "Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { "All",		"Select all of the below",
	NULL,		setSrc, NULL, NULL, ' ', ' ', ' ' },
      { "Reset",	"Reset all of the below",
	NULL,		clearSrc, NULL, NULL, ' ', ' ', ' ' },
      { " base",	"top-level files in /usr/src",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_BASE },
      { " cddl",	"/usr/src/cddl (software from Sun)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_CDDL },
      { " contrib",	"/usr/src/contrib (contributed software)",
	dmenuFlagCheck,	dmenuSetFlag,	NULL, &SrcDists, '[', 'X', ']', DIST_SRC_CONTRIB },
      { " crypto",	"/usr/src/crypto (contrib encryption sources)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SCRYPTO },
      { " gnu",		"/usr/src/gnu (software from the GNU Project)",
	dmenuFlagCheck,	dmenuSetFlag,	NULL, &SrcDists, '[', 'X', ']', DIST_SRC_GNU },
      { " etc",		"/usr/src/etc (miscellaneous system files)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_ETC },
      { " games",	"/usr/src/games (the obvious!)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_GAMES },
      { " include",	"/usr/src/include (header files)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_INCLUDE },
      { " krb5",	"/usr/src/kerberos5 (sources for Kerberos5)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SKERBEROS5 },
      { " lib",		"/usr/src/lib (system libraries)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_LIB },
      { " libexec",	"/usr/src/libexec (system programs)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_LIBEXEC },
      { " release",	"/usr/src/release (release-generation tools)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_RELEASE },
      { " rescue",	"/usr/src/rescue (static rescue tools)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_RESCUE },
      { " bin",		"/usr/src/bin (system binaries)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_BIN },
      { " sbin",	"/usr/src/sbin (system binaries)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SBIN },
      { " secure",	"/usr/src/secure (BSD encryption sources)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SSECURE },
      { " share",	"/usr/src/share (documents and shared files)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SHARE },
      { " sys",		"/usr/src/sys (FreeBSD kernel)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SYS },
      { " tools",	"/usr/src/tools (miscellaneous tools)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_TOOLS },
      { " ubin",	"/usr/src/usr.bin (user binaries)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_UBIN },
      { " usbin",	"/usr/src/usr.sbin (aux system binaries)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_USBIN },
      { NULL } },
};

DMenu MenuDiskDevices = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Select Drive(s)",
    "Please select the drive, or drives, on which you wish to perform\n"
    "this operation.  If you are attempting to install a boot partition\n"
    "on a drive other than the first one or have multiple operating\n"
    "systems on your machine, you will have the option to install a boot\n"
    "manager later.  To select a drive, use the arrow keys to move to it\n"
    "and press [SPACE] or [ENTER].  To de-select it, press it again.\n\n"
    "Use [TAB] to get to the buttons and leave this menu.",
    "Press F1 for important information regarding disk geometry!",
    "drives",
    { { NULL } },
};

DMenu MenuHTMLDoc = {
    DMENU_NORMAL_TYPE,
    "Select HTML Documentation pointer",
    "Please select the body of documentation you're interested in, the main\n"
    "ones right now being the FAQ and the Handbook.  You can also choose \"other\"\n"
    "to enter an arbitrary URL for browsing.",
    "Press F1 for more help on what you see here.",
    "html",
    { { "X Exit",	"Exit this menu (returning to previous)", NULL,	dmenuExit },
      { "2 Handbook",	"The FreeBSD Handbook.",				NULL, docShowDocument },
      { "3 FAQ",	"The Frequently Asked Questions guide.",		NULL, docShowDocument },
      { "4 Home",	"The Home Pages for the FreeBSD Project (requires net)", NULL, docShowDocument },
      { "5 Other",	"Enter a URL.",						NULL, docShowDocument },
      { NULL } },
};

/* The main installation menu */
DMenu MenuInstallCustom = {
    DMENU_NORMAL_TYPE,
    "Choose Custom Installation Options",
    "This is the custom installation menu. You may use this menu to specify\n"
    "details on the type of distribution you wish to have, where you wish\n"
    "to install it from and how you wish to allocate disk storage to FreeBSD.",
    NULL,
    NULL,
    { { "X Exit",		"Exit this menu (returning to previous)", NULL,	dmenuExit },
      { "2 Options",		"View/Set various installation options", NULL, optionsEditor },
#ifndef WITH_SLICES
      { "3 Label",		"Label disk partitions",		NULL, diskLabelEditor },
      { "4 Distributions",	"Select distribution(s) to extract",	NULL, dmenuSubmenu, NULL, &MenuDistributions },
      { "5 Media",		"Choose the installation media type",	NULL, dmenuSubmenu, NULL, &MenuMedia },
      { "6 Commit",		"Perform any pending Partition/Label/Extract actions", NULL, installCustomCommit },
#else
      { "3 Partition",		"Allocate disk space for FreeBSD",	NULL, diskPartitionEditor },
      { "4 Label",		"Label allocated disk partitions",	NULL, diskLabelEditor },
      { "5 Distributions",	"Select distribution(s) to extract",	NULL, dmenuSubmenu, NULL, &MenuDistributions },
      { "6 Media",		"Choose the installation media type",	NULL, dmenuSubmenu, NULL, &MenuMedia },
      { "7 Commit",		"Perform any pending Partition/Label/Extract actions", NULL, installCustomCommit },
#endif
      { NULL } },
};

#if defined(__i386__) || defined(__amd64__)
#ifdef PC98
/* IPL type menu */
DMenu MenuIPLType = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "overwrite me",		/* will be disk specific label */
    "If you want a FreeBSD Boot Manager, select \"BootMgr\".  If you would\n"
    "prefer your Boot Manager to remain untouched then select \"None\".\n\n",
    "Press F1 to read about drive setup",
    "drives",
    { { "BootMgr",	"Install the FreeBSD Boot Manager",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr },
      { "None",		"Leave the IPL untouched",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr, '(', '*', ')', 1 },
      { NULL } },
};
#else
/* MBR type menu */
DMenu MenuMBRType = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "overwrite me",		/* will be disk specific label */
    "FreeBSD comes with a boot manager that allows you to easily\n"
    "select between FreeBSD and any other operating systems on your machine\n"
    "at boot time.  If you have more than one drive and want to boot\n"
    "from the second one, the boot manager will also make it possible\n"
    "to do so (limitations in the PC BIOS usually prevent this otherwise).\n"
    "If you have other operating systems installed and would like a choice when\n"
    "booting, choose \"BootMgr\". If you would prefer to keep your existing\n"
    "boot manager, select \"None\".\n\n",
    "",    
    "drives",
    { { "Standard",	"Install a standard MBR (non-interactive boot manager)",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr, '(', '*', ')', 1 },
      { "BootMgr",	"Install the FreeBSD Boot Manager",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr, '(', '*', ')', 0 },
      { "None",		"Do not install a boot manager",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr, '(', '*', ')', 2 },
      { NULL } },
};
#endif /* PC98 */
#endif /* __i386__ */

/* Final configuration menu */
DMenu MenuConfigure = {
    DMENU_NORMAL_TYPE,
    "FreeBSD Configuration Menu",	/* title */
    "If you've already installed FreeBSD, you may use this menu to customize\n"
    "it somewhat to suit your particular configuration.  Most importantly,\n"
    "you can use the Packages utility to load extra \"3rd party\"\n"
    "software not provided in the base distributions.",
    "Press F1 for more information on these options",
    "configure",
    { { "X Exit",		"Exit this menu (returning to previous)",
	NULL,	dmenuExit },
      { " Distributions", "Install additional distribution sets",
	NULL, distExtractAll },
      { " Documentation installation", "Install FreeBSD Documentation set",
	NULL, distSetDocMenu },
      { " Packages",	"Install pre-packaged software for FreeBSD",
	NULL, configPackages },
      { " Root Password", "Set the system manager's password",
	NULL,	dmenuSystemCommand, NULL, "passwd root" },
#ifdef WITH_SLICES
      { " Fdisk",	"The disk slice (PC-style partition) editor",
	NULL, diskPartitionEditor },
#endif
      { " Label",	"The disk label editor",
	NULL, diskLabelEditor },
      { " User Management",	"Add user and group information",
	NULL, dmenuSubmenu, NULL, &MenuUsermgmt },
#ifdef WITH_SYSCONS
      { " Console",	"Customize system console behavior",
	NULL,	dmenuSubmenu, NULL, &MenuSyscons },
#endif
      { " Time Zone",	"Set which time zone you're in",
	NULL,	dmenuSystemCommand, NULL, "tzsetup" },
      { " Media",	"Change the installation media type",
	NULL,	dmenuSubmenu, NULL, &MenuMedia },
#ifdef WITH_MICE
      { " Mouse",	"Configure your mouse",
	NULL,	dmenuSubmenu, NULL, &MenuMouse },
#endif
      { " Networking",	"Configure additional network services",
	NULL,	dmenuSubmenu, NULL, &MenuNetworking },
      { " Security",	"Configure system security options",
	NULL,	dmenuSubmenu, NULL, &MenuSecurity },
      { " Startup",	"Configure system startup options",
	NULL,	dmenuSubmenu, NULL, &MenuStartup },
      { " TTYs",	"Configure system ttys.",
	NULL,	configEtcTtys, NULL, "ttys" },
      { " Options",	"View/Set various installation options",
	NULL, optionsEditor },
      { " HTML Docs",	"Go to the HTML documentation menu (post-install)",
	NULL, docBrowser },
      { " Load KLD",	"Load a KLD from a floppy",
	NULL, kldBrowser },
      { NULL } },
};

DMenu MenuStartup = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Startup Services Menu",
    "This menu allows you to configure various aspects of your system's\n"
    "startup configuration.  Use [SPACE] or [ENTER] to select items, and\n"
    "[TAB] to move to the buttons.  Select Exit to leave this menu.",
    NULL,
    NULL,
    { { "X Exit",	"Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
#ifdef __i386__
      { " APM",		"Auto-power management services (typically laptops)",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "apm_enable=YES" },
#endif
      { " ",		" -- ", NULL,	NULL, NULL, NULL, ' ', ' ', ' ' },
      { " Startup dirs",	"Set the list of dirs to look for startup scripts",
	dmenuVarCheck, dmenuISetVariable, NULL, "local_startup" },
      { " named",	"Run a local name server on this host",
	dmenuVarCheck, dmenuToggleVariable, NULL, "named_enable=YES" },
      { " named flags",	"Set default flags to named (if enabled)",
	dmenuVarCheck, dmenuISetVariable, NULL, "named_flags" },
      { " NIS client",	"This host wishes to be an NIS client.",
	dmenuVarCheck, configRpcBind, NULL, "nis_client_enable=YES" },
      { " NIS domainname",	"Set NIS domainname (if enabled)",
	dmenuVarCheck, dmenuISetVariable, NULL, "nisdomainname" },
      { " NIS server",	"This host wishes to be an NIS server.",
	dmenuVarCheck, configRpcBind, NULL, "nis_server_enable=YES" },
      { " ",		" -- ", NULL,	NULL, NULL, NULL, ' ', ' ', ' ' },
      { " Accounting",	"This host wishes to run process accounting.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "accounting_enable=YES" },
      { " lpd",		"This host has a printer and wants to run lpd.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "lpd_enable=YES" },
#ifdef __i386__
      { " SCO",		"This host wants to be able to run IBCS2 binaries.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "ibcs2_enable=YES" },
      { " SVR4",	"This host wants to be able to run SVR4 binaries.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "svr4_enable=YES" },
#endif
      { NULL } },
};

DMenu MenuNetworking = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Network Services Menu",
    "You may have already configured one network device (and the other\n"
    "various hostname/gateway/name server parameters) in the process\n"
    "of installing FreeBSD.  This menu allows you to configure other\n"
    "aspects of your system's network configuration.",
    NULL,
    NULL,
    { { "X Exit",	"Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { " Interfaces",	"Configure additional network interfaces",
	NULL, tcpMenuSelect },
      { " AMD",		"This machine wants to run the auto-mounter service",
	dmenuVarCheck,	configRpcBind, NULL, "amd_enable=YES" },
      { " AMD Flags",	"Set flags to AMD service (if enabled)",
	dmenuVarCheck,	dmenuISetVariable, NULL, "amd_flags" },
      { " Anon FTP",	"This machine wishes to allow anonymous FTP.",
	dmenuVarCheck,	configAnonFTP, NULL, "anon_ftp" },
      { " Gateway",	"This machine will route packets between interfaces",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "gateway_enable=YES" },
      { " inetd",	"This machine wants to run the inet daemon",
	dmenuVarCheck,	configInetd, NULL, "inetd_enable=YES" },
      { " Mail",	"This machine wants to run a Mail Transfer Agent",
	NULL,		dmenuSubmenu, NULL, &MenuMTA },
      { " NFS client",	"This machine will be an NFS client",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "nfs_client_enable=YES" },
      { " NFS server",	"This machine will be an NFS server",
	dmenuVarCheck,	configNFSServer, NULL, "nfs_server_enable=YES" },
      { " Ntpdate",	"Select a clock synchronization server",
	dmenuVarCheck,	dmenuSubmenu, NULL, &MenuNTP, '[', 'X', ']',
	(uintptr_t)"ntpdate_enable=YES" },
      { " PCNFSD",	"Run authentication server for clients with PC-NFS.",
	dmenuVarCheck,	configPCNFSD, NULL, "pcnfsd" },
      { " rpcbind", 	"RPC port mapping daemon (formerly portmapper)",
        dmenuVarCheck, dmenuToggleVariable, NULL, "rpcbind_enable=YES" },
      { " rpc.statd", 	"NFS status monitoring daemon",
	dmenuVarCheck, configRpcBind, NULL, "rpc_statd_enable=YES" },
      { " rpc.lockd",	"NFS file locking daemon",
	dmenuVarCheck, configRpcBind, NULL, "rpc_lockd_enable=YES" },
      { " Routed",	"Select routing daemon (default: routed)",
	dmenuVarCheck,	configRouter, NULL, "router_enable=YES" },
      { " Rwhod",	"This machine wants to run the rwho daemon",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "rwhod_enable=YES" },
      { " sshd",	"This machine wants to run the SSH daemon",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "sshd_enable=YES" },
      { " TCP Extensions", "Allow RFC1323 and RFC1644 TCP extensions?",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "tcp_extensions=YES" },
      { NULL } },
};

DMenu MenuMTA = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Mail Transfer Agent Selection",
    "You can choose which Mail Transfer Agent (MTA) you wish to install and run.\n"
    "Selecting Sendmail local disables sendmail's network socket for\n"
    "incoming mail, but still enables sendmail for local and outbound mail.\n"
    "The Postfix option will install the Postfix MTA from the ports\n"
    "collection.  The Exim option will install the Exim MTA from the ports\n"
    "collection.  To return to the previous menu, select Exit.",
    NULL,
    NULL,
    {
      { "Sendmail",           "Use sendmail",
        dmenuVarCheck, dmenuSetVariable, NULL, "sendmail_enable=YES" },
      { "Sendmail local",    "Use sendmail, but do not listen on the network",
        dmenuVarCheck, dmenuSetVariable, NULL, "sendmail_enable=NO" },
      { "Postfix",            "Use the Postfix MTA",
      NULL, configMTAPostfix, NULL, NULL },
      { "Exim",               "Use the Exim MTA",
      NULL, configMTAExim, NULL, NULL },
      { "None",               "Do not install an MTA",
        dmenuVarCheck, dmenuSetVariable, NULL, "sendmail_enable=NONE" },
      { "X Exit",             "Exit this menu (returning to previous)",
        checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { NULL } },
};

DMenu MenuNTP = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "NTPDATE Server Selection",
    "There are a number of time synchronization servers available\n"
    "for public use around the Internet.  Please select one reasonably\n"
    "close to you to have your system time synchronized accordingly.",
    "These are the primary open-access NTP servers",
    NULL,
    { { "None",		        "No NTP server",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=NO,ntpdate_hosts=none" },
      { "Other",		"Select a site not on this list",
	dmenuVarsCheck, configNTP, NULL, NULL },
      { "Worldwide",		"pool.ntp.org",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=pool.ntp.org" },
      { "Asia",		"asia.pool.ntp.org",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=asia.pool.ntp.org" },
      { "Europe",		"europe.pool.ntp.org",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=europe.pool.ntp.org" },
      { "Oceania",		"oceania.pool.ntp.org",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=oceania.pool.ntp.org" },
      { "North America",	"north-america.pool.ntp.org",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=north-america.pool.ntp.org" },
      { "Argentina",		"tick.nap.com.ar",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=tick.nap.com.ar" },
      { "Argentina #2",		"time.sinectis.com.ar",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=time.sinectis.com.ar" },
      { "Argentina #3",		"tock.nap.com.ar",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=tock.nap.com.ar" },
      { "Australia",		"au.pool.ntp.org",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=au.pool.ntp.org" },
      { "Australia #2",		"augean.eleceng.adelaide.edu.au",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=augean.eleceng.adelaide.edu.au" },
      { "Australia #3",		"ntp.adelaide.edu.au",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=ntp.adelaide.edu.au" },
      { "Australia #4",		"ntp.saard.net",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=ntp.saard.net" },
      { "Australia #5",		"time.deakin.edu.au",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=time.deakin.edu.au" },
      { "Belgium",		"ntp1.belbone.be",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp1.belbone.be" },
      { "Belgium #2",		"ntp2.belbone.be",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp2.belbone.be" },
      { "Brazil",		"a.ntp.br",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=a.ntp.br" },
      { "Brazil #2",		"b.ntp.br",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=b.ntp.br" },
      { "Brazil #3",		"c.ntp.br",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=c.ntp.br" },
      { "Brazil #4",		"ntp.cais.rnp.br",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.cais.rnp.br" },
      { "Brazil #5",		"ntp1.pucpr.br",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp1.pucpr.br" },
      { "Canada",		"ca.pool.ntp.org",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=ca.pool.ntp.org" },
      { "Canada #2",		"ntp.cpsc.ucalgary.ca",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=ntp.cpsc.ucalgary.ca" },
      { "Canada #3",		"ntp1.cmc.ec.gc.ca",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=ntp1.cmc.ec.gc.ca" },
      { "Canada #4",		"ntp2.cmc.ec.gc.ca",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=ntp2.cmc.ec.gc.ca" },
      { "Canada #5",		"tick.utoronto.ca",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=tick.utoronto.ca" },
      { "Canada #6",		"time.chu.nrc.ca",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=time.chu.nrc.ca" },
      { "Canada #7",		"time.nrc.ca",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=time.nrc.ca" },
      { "Canada #8",		"timelord.uregina.ca",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=timelord.uregina.ca" },
      { "Canada #9",		"tock.utoronto.ca",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=tock.utoronto.ca" },
      { "Czech",		"ntp.karpo.cz",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.karpo.cz" },
      { "Czech #2",		"ntp.cgi.cz",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.cgi.cz" },
      { "Denmark",		"clock.netcetera.dk",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=clock.netcetera.dk" },
      { "Denmark",		"clock2.netcetera.dk",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=clock2.netcetera.dk" },
      { "Spain",		"slug.ctv.es",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=slug.ctv.es" },
      { "Finland",		"tick.keso.fi",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=tick.keso.fi" },
      { "Finland #2",		"tock.keso.fi",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=tock.keso.fi" },
      { "France",		"ntp.obspm.fr",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=ntp.obspm.fr" },
      { "France #2",		"ntp.univ-lyon1.fr",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.univ-lyon1.fr" },
      { "France #3",		"ntp.via.ecp.fr",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.via.ecp.fr" },
      { "Croatia",		"zg1.ntp.carnet.hr",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=zg1.ntp.carnet.hr" },
      { "Croatia #2",		"zg2.ntp.carnet.hr",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=zg2.ntp.carnet.hr" },
      { "Croatia #3",		"st.ntp.carnet.hr",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=st.ntp.carnet.hr" },
      { "Croatia #4",		"ri.ntp.carnet.hr",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ri.ntp.carnet.hr" },
      { "Croatia #5",		"os.ntp.carnet.hr",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=os.ntp.carnet.hr" },
      { "Hungary",		"time.kfki.hu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=time.kfki.hu" },
      { "Indonesia",		"ntp.kim.lipi.go.id",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.kim.lipi.go.id" },
      { "Ireland",		"ntp.maths.tcd.ie",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.maths.tcd.ie" },
      { "Italy",		"it.pool.ntp.org",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=it.pool.ntp.org" },
      { "Japan",		"ntp.jst.mfeed.ad.jp",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.jst.mfeed.ad.jp" },
      { "Japan IPv6",		"ntp1.v6.mfeed.ad.jp",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp1.v6.mfeed.ad.jp" },
      { "Korea",		"time.nuri.net",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=time.nuri.net" },
      { "Mexico",		"mx.pool.ntp.org",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=mx.pool.ntp.org" },
      { "Netherlands",		"ntp0.nl.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp0.nl.net" },
      { "Netherlands #2",	"ntp1.nl.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp1.nl.net" },
      { "Netherlands #3",	"ntp2.nl.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp2.nl.net" },
      { "Norway",		"fartein.ifi.uio.no",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=fartein.ifi.uio.no" },
      { "Norway #2",		"time.alcanet.no",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=time.alcanet.no" },
      { "New Zealand",		"ntp.massey.ac.nz",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.massey.ac.nz" },
      { "New Zealand #2",	"ntp.public.otago.ac.nz",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.public.otago.ac.nz" },
      { "New Zealand #3",	"tk1.ihug.co.nz",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=tk1.ihug.co.nz" },
      { "New Zealand #4",	"ntp.waikato.ac.nz",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.waikato.ac.nz" },
      { "Poland",		"info.cyf-kr.edu.pl",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=info.cyf-kr.edu.pl" },
      { "Romania",		"ticks.roedu.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ticks.roedu.net" },
      { "Russia",		"ru.pool.ntp.org",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ru.pool.ntp.org" },
      { "Russia #2",		"ntp.psn.ru",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.psn.ru" },
      { "Sweden",		"se.pool.ntp.org",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=se.pool.ntp.org" },
      { "Sweden #2",		"ntp.lth.se",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=ntp.lth.se" },
      { "Sweden #3",		"ntp1.sp.se",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=ntp1.sp.se" },
      { "Sweden #4",		"ntp2.sp.se",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=ntp2.sp.se" },
      { "Sweden #5",		"ntp.kth.se",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=ntp.kth.se" },
      { "Singapore",		"sg.pool.ntp.org",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=sg.pool.ntp.org" },
      { "Slovenia",		"si.pool.ntp.org",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=si.pool.ntp.org" },
      { "Slovenia #2",		"sizif.mf.uni-lj.si",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=sizif.mf.uni-lj.si" },
      { "Slovenia #3",		"ntp1.arnes.si",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp1.arnes.si" },
      { "Slovenia #4",		"ntp2.arnes.si",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp2.arnes.si" },
      { "Slovenia #5",		"time.ijs.si",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=time.ijs.si" },
      { "Scotland",		"ntp.cs.strath.ac.uk",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.cs.strath.ac.uk" },
      { "Taiwan",              "time.stdtime.gov.tw",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=time.stdtime.gov.tw" },
      { "Taiwan #2",           "clock.stdtime.gov.tw",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=clock.stdtime.gov.tw" },
      { "Taiwan #3",           "tick.stdtime.gov.tw",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=tick.stdtime.gov.tw" },
      { "Taiwan #4",           "tock.stdtime.gov.tw",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=tock.stdtime.gov.tw" },
      { "Taiwan #5",           "watch.stdtime.gov.tw",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=watch.stdtime.gov.tw" },
      { "United Kingdom",	"uk.pool.ntp.org",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=uk.pool.ntp.org" },
      { "United Kingdom #2",	"ntp.exnet.com",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.exnet.com" },
      { "United Kingdom #3",	"ntp0.uk.uu.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp0.uk.uu.net" },
      { "United Kingdom #4",	"ntp1.uk.uu.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp1.uk.uu.net" },
      { "United Kingdom #5",	"ntp2.uk.uu.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp2.uk.uu.net" },
      { "United Kingdom #6",	"ntp2a.mcc.ac.uk",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp2a.mcc.ac.uk" },
      { "United Kingdom #7",	"ntp2b.mcc.ac.uk",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp2b.mcc.ac.uk" },
      { "United Kingdom #8",	"ntp2c.mcc.ac.uk",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp2c.mcc.ac.uk" },
      { "United Kingdom #9",	"ntp2d.mcc.ac.uk",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp2d.mcc.ac.uk" },
      { "U.S.",	"us.pool.ntp.org",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=us.pool.ntp.org" },
      { "U.S. AR",	"sushi.lyon.edu",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=sushi.compsci.lyon.edu" },
      { "U.S. AZ",	"ntp.drydog.com",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=ntp.drydog.com" },
      { "U.S. CA",	"ntp.ucsd.edu",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=ntp.ucsd.edu" },
      { "U.S. CA #2",	"ntp1.mainecoon.com",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=ntp1.mainecoon.com" },
      { "U.S. CA #3",	"ntp2.mainecoon.com",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=ntp2.mainecoon.com" },
      { "U.S. CA #4",	"reloj.kjsl.com",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=reloj.kjsl.com" },
      { "U.S. CA #5",	"time.five-ten-sg.com",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=time.five-ten-sg.com" },
      { "U.S. DE",	"louie.udel.edu",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=louie.udel.edu" },
      { "U.S. GA",		"ntp.shorty.com",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=ntp.shorty.com" },
      { "U.S. GA #2",		"rolex.usg.edu",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=rolex.usg.edu" },
      { "U.S. GA #3",		"timex.usg.edu",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_hosts=timex.usg.edu" },
      { "U.S. IL",	"ntp-0.cso.uiuc.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp-0.cso.uiuc.edu" },
      { "U.S. IL #2",	"ntp-1.cso.uiuc.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp-1.cso.uiuc.edu" },
      { "U.S. IL #3",	"ntp-1.mcs.anl.gov",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp-1.mcs.anl.gov" },
      { "U.S. IL #4",	"ntp-2.cso.uiuc.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp-2.cso.uiuc.edu" },
      { "U.S. IL #5",	"ntp-2.mcs.anl.gov",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp-2.mcs.anl.gov" },
      { "U.S. IN",	"gilbreth.ecn.purdue.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=gilbreth.ecn.purdue.edu" },
      { "U.S. IN #2",	"harbor.ecn.purdue.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=harbor.ecn.purdue.edu" },
      { "U.S. IN #3",	"molecule.ecn.purdue.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=molecule.ecn.purdue.edu" },
      { "U.S. KS",	"ntp1.kansas.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp1.kansas.net" },
      { "U.S. KS #2",	"ntp2.kansas.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp2.kansas.net" },
      { "U.S. MA",	"ntp.ourconcord.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.ourconcord.net" },
      { "U.S. MA #2",	"timeserver.cs.umb.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=timeserver.cs.umb.edu" },
      { "U.S. MN",	"ns.nts.umn.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ns.nts.umn.edu" },
      { "U.S. MN #2",	"nss.nts.umn.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=nss.nts.umn.edu" },
      { "U.S. MO",	"time-ext.missouri.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=time-ext.missouri.edu" },
      { "U.S. MT",	"chronos1.umt.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=chronos1.umt.edu" },
      { "U.S. MT #2",	"chronos2.umt.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=chronos2.umt.edu" },
      { "U.S. MT #3",	"chronos3.umt.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=chronos3.umt.edu" },
      { "U.S. NC",	"clock1.unc.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=clock1.unc.edu" },
      { "U.S. NV",	"cuckoo.nevada.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=cuckoo.nevada.edu" },
      { "U.S. NV #2",	"tick.cs.unlv.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=tick.cs.unlv.edu" },
      { "U.S. NV #3",	"tock.cs.unlv.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=tock.cs.unlv.edu" },
      { "U.S. NY",	"ntp0.cornell.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp0.cornell.edu" },
      { "U.S. NY #2",	"sundial.columbia.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=sundial.columbia.edu" },
      { "U.S. NY #3",	"timex.cs.columbia.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=timex.cs.columbia.edu" },
      { "U.S. PA",	"clock-1.cs.cmu.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=clock-1.cs.cmu.edu" },
      { "U.S. PA #2",	"clock-2.cs.cmu.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=clock-2.cs.cmu.edu" },
      { "U.S. PA #3",	"clock.psu.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=clock.psu.edu" },
      { "U.S. PA #4",	"fuzz.psc.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=fuzz.psc.edu" },
      { "U.S. PA #5",	"ntp-1.ece.cmu.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp-1.ece.cmu.edu" },
      { "U.S. PA #6",	"ntp-2.ece.cmu.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp-2.ece.cmu.edu" },
      { "U.S. TX",	"ntp.fnbhs.com",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.fnbhs.com" },
      { "U.S. TX #2",	"ntp.tmc.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.tmc.edu" },
      { "U.S. TX #3",	"ntp5.tamu.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp5.tamu.edu" },
      { "U.S. TX #4",	"tick.greyware.com",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=tick.greyware.com" },
      { "U.S. TX #5",	"tock.greyware.com",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=tock.greyware.com" },
      { "U.S. VA",	"ntp-1.vt.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp-1.vt.edu" },
      { "U.S. VA #2",	"ntp-2.vt.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp-2.vt.edu" },
      { "U.S. VA #3",	"ntp.cmr.gov",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.cmr.gov" },
      { "U.S. VT",	"ntp0.state.vt.us",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp0.state.vt.us" },
      { "U.S. VT #2",	"ntp1.state.vt.us",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp1.state.vt.us" },
      { "U.S. VT #3",	"ntp2.state.vt.us",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp2.state.vt.us" },
      { "U.S. WA",	"ntp.tcp-udp.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.tcp-udp.net" },
      { "U.S. WI",	"ntp1.cs.wisc.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp1.cs.wisc.edu" },
      { "U.S. WI #2",	"ntp3.cs.wisc.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp3.cs.wisc.edu" },
      { "South Africa",	"ntp.cs.unp.ac.za",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_hosts=ntp.cs.unp.ac.za" },
      { NULL } },
};

#ifdef WITH_SYSCONS
DMenu MenuSyscons = {
    DMENU_NORMAL_TYPE,
    "System Console Configuration",
    "The system console driver for FreeBSD has a number of configuration\n"
    "options which may be set according to your preference.\n\n"
    "When you are done setting configuration options, select Cancel.",
    "Configure your system console settings",
    NULL,
    { { "X Exit",	"Exit this menu (returning to previous)", NULL, dmenuExit },
#ifdef PC98
      { "2 Keymap",	"Choose an alternate keyboard map",	NULL, dmenuSubmenu, NULL, &MenuSysconsKeymap },
      { "3 Repeat",	"Set the rate at which keys repeat",	NULL, dmenuSubmenu, NULL, &MenuSysconsKeyrate },
      { "4 Saver",	"Configure the screen saver",		NULL, dmenuSubmenu, NULL, &MenuSysconsSaver },
#else
      { "2 Font",	"Choose an alternate screen font",	NULL, dmenuSubmenu, NULL, &MenuSysconsFont },
      { "3 Keymap",	"Choose an alternate keyboard map",	NULL, dmenuSubmenu, NULL, &MenuSysconsKeymap },
      { "4 Repeat",	"Set the rate at which keys repeat",	NULL, dmenuSubmenu, NULL, &MenuSysconsKeyrate },
      { "5 Saver",	"Configure the screen saver",		NULL, dmenuSubmenu, NULL, &MenuSysconsSaver },
      { "6 Screenmap",	"Choose an alternate screenmap",	NULL, dmenuSubmenu, NULL, &MenuSysconsScrnmap },
      { "7 Ttys",       "Choose console terminal type",         NULL, dmenuSubmenu, NULL, &MenuSysconsTtys },
#endif
      { NULL } },
};

#ifdef PC98
DMenu MenuSysconsKeymap = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Keymap",
    "The system console driver for FreeBSD defaults to a standard\n"
    "\"PC-98x1\" keyboard map.  Users may wish to choose one of the\n"
    "other keymaps below.",
    "Choose a keyboard map",
    NULL,
    { { "Japanese PC-98x1",		"Japanese PC-98x1 keymap",  dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=jp.pc98" },
      { " Japanese PC-98x1 (ISO)",	"Japanese PC-98x1 (ISO) keymap",  dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=jp.pc98.iso" },
      { NULL } },
};
#else
DMenu MenuSysconsKeymap = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Keymap",
    "The system console driver for FreeBSD defaults to a standard\n"
    "\"US\" keyboard map.  Users may wish to choose one of the\n"
    "other keymaps below.",
    "Choose a keyboard map",
    NULL,
    { { "Belgian",	"Belgian ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=be.iso" },
      { " Brazil CP850",	"Brazil CP850 keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=br275.cp850" },
      { " Brazil ISO (accent)",	"Brazil ISO keymap (accent keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=br275.iso.acc" },
      { " Brazil ISO",	"Brazil ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=br275.iso" },
      { " Bulgarian BDS",	"Bulgarian BDS keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=bg.bds.ctrlcaps" },
      { " Bulgarian Phonetic",	"Bulgarian Phonetic keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=bg.phonetic.ctrlcaps" },
      { "Central European ISO", "Central European ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=ce.iso2" },
      { " Croatian ISO",	"Croatian ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=hr.iso" },
      { " Czech ISO (accent)",	"Czech ISO keymap (accent keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=cs.latin2.qwertz" },
      { "Danish CP865",	"Danish Code Page 865 keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=danish.cp865" },
      { " Danish ISO",	"Danish ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=danish.iso" },
      { "Estonian ISO", "Estonian ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=estonian.iso" },
      { " Estonian ISO 15", "Estonian ISO 8859-15 keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=estonian.iso15" },
      { " Estonian CP850", "Estonian Code Page 850 keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=estonian.cp850" },
      { "Finnish CP850","Finnish Code Page 850 keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=finnish.cp850" },
      { " Finnish ISO",  "Finnish ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=finnish.iso" },
      { " French ISO (accent)", "French ISO keymap (accent keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=fr.iso.acc" },
      { " French ISO",	"French ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=fr.iso" },
      { " French ISO/Macbook",	"French ISO keymap on macbook",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=fr.macbook.acc" },
      { "German CP850",	"German Code Page 850 keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=german.cp850"	},
      { " German ISO",	"German ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=german.iso" },
      { " Greek 101",	"Greek ISO keymap (101 keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=gr.us101.acc" },
      { " Greek 104",	"Greek ISO keymap (104 keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=el.iso07" },
      { " Greek ELOT",	"Greek ISO keymap (ELOT 1000)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=gr.elot.acc" },
      { "Hungarian 101", "Hungarian ISO keymap (101 key)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=hu.iso2.101keys" },
      { " Hungarian 102", "Hungarian ISO keymap (102 key)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=hu.iso2.102keys" },
      { "Icelandic (accent)", "Icelandic ISO keymap (accent keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=icelandic.iso.acc" },
      { " Icelandic",	"Icelandic ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=icelandic.iso" },
      { " Italian",	"Italian ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=it.iso" },
      { "Japanese 106",	"Japanese 106 keymap",  dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=jp.106" },
      { "Latin American (accent)",	"Latin American ISO keymap (accent keys)",	dmenuVarCheck,	dmenuSetKmapVariable,	NULL,	"keymap=latinamerican.iso.acc" },
      { " Latin American",	"Latin American ISO keymap",	dmenuVarCheck,	dmenuSetKmapVariable,	NULL,	"keymap=latinamerican" },
      { "Norway ISO",	"Norwegian ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=norwegian.iso" },
      { "Polish ISO",	"Polish ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=pl_PL.ISO8859-2" },
      { " Portuguese (accent)",	"Portuguese ISO keymap (accent keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=pt.iso.acc" },
      { " Portuguese",	"Portuguese ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=pt.iso" },
      { "Russia KOI8-R", "Russian KOI8-R keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=ru.koi8-r" },
      { "Slovak", "Slovak ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=sk.iso2" },
      { "Slovenian", "Slovenian ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=si.iso" },
      { " Spanish (accent)", "Spanish ISO keymap (accent keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=spanish.iso.acc" },
      { " Spanish",	"Spanish ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=spanish.iso" },
      { " Swedish CP850", "Swedish Code Page 850 keymap", dmenuVarCheck,	dmenuSetKmapVariable, NULL, "keymap=swedish.cp850" },
      { " Swedish ISO",	"Swedish ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=swedish.iso" },
      { " Swiss French ISO (accent)", "Swiss French ISO keymap (accent keys)", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=swissfrench.iso.acc" },
      { " Swiss French ISO", "Swiss French ISO keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=swissfrench.iso" },
      { " Swiss French CP850", "Swiss French Code Page 850 keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=swissfrench.cp850" },
      { " Swiss German ISO (accent)", "Swiss German ISO keymap (accent keys)", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=swissgerman.iso.acc" },
      { " Swiss German ISO", "Swiss German ISO keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=swissgerman.iso" },
      { " Swiss German CP850", "Swiss German Code Page 850 keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=swissgerman.cp850" },
      { "UK CP850",	"UK Code Page 850 keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=uk.cp850" },
      { " UK ISO",	"UK ISO keymap", dmenuVarCheck,	dmenuSetKmapVariable, NULL, "keymap=uk.iso" },
      { " Ukrainian KOI8-U",	"Ukrainian KOI8-U keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=ua.koi8-u" },
      { " Ukrainian KOI8-U+KOI8-R",	"Ukrainian KOI8-U+KOI8-R keymap (alter)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=ua.koi8-u.shift.alt" },
      { " USA CapsLock->Ctrl",	"US standard (Caps as L-Control)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=us.pc-ctrl" },
      { " USA Dvorak",	"US Dvorak keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=us.dvorak" },
      { " USA Dvorak (left)",	"US left handed Dvorak keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=us.dvorakl" },
      { " USA Dvorak (right)",	"US right handed Dvorak keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=us.dvorakr" },
      { " USA Emacs",	"US standard optimized for EMACS",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=us.emacs" },
      { " USA ISO",	"US ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=us.iso" },
      { " USA UNIX",	"US traditional UNIX-workstation",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=us.unix" },
      { NULL } },
};
#endif /* PC98 */

DMenu MenuSysconsKeyrate = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Keyboard Repeat Rate",
    "This menu allows you to set the speed at which keys repeat\n"
    "when held down.",
    "Choose a keyboard repeat rate",
    NULL,
    { { "Slow",	"Slow keyboard repeat rate",	dmenuVarCheck,	dmenuSetVariable, NULL, "keyrate=slow" },
      { "Normal", "\"Normal\" keyboard repeat rate",	dmenuVarCheck,	dmenuSetVariable, NULL, "keyrate=normal" },
      { "Fast",	"Fast keyboard repeat rate",	dmenuVarCheck,	dmenuSetVariable, NULL, "keyrate=fast" },
      { "Default", "Use default keyboard repeat rate",	dmenuVarCheck,	dmenuSetVariable, NULL, "keyrate=NO" },
      { NULL } }
};

DMenu MenuSysconsSaver = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Screen Saver",
    "By default, the console driver will not attempt to do anything\n"
    "special with your screen when it's idle.  If you expect to leave your\n"
    "monitor switched on and idle for long periods of time then you should\n"
    "probably enable one of these screen savers to prevent burn-in.",
    "Choose a nifty-looking screen saver",
    NULL,
    { { "1 Blank",	"Simply blank the screen",
	dmenuVarCheck, configSaver, NULL, "saver=blank" },
      { "2 Beastie",	"\"BSD Daemon\" animated screen saver (graphics)",
	dmenuVarCheck, configSaver, NULL, "saver=beastie" },
      { "3 Daemon",	"\"BSD Daemon\" animated screen saver (text)",
	dmenuVarCheck, configSaver, NULL, "saver=daemon" },
      { "4 Dragon",	"Dragon screensaver (graphics)",
	dmenuVarCheck, configSaver, NULL, "saver=dragon" },
      { "5 Fade",	"Fade out effect screen saver",
	dmenuVarCheck, configSaver, NULL, "saver=fade" },
      { "6 Fire",	"Flames effect screen saver",
	dmenuVarCheck, configSaver, NULL, "saver=fire" },
      { "7 Green",	"\"Green\" power saving mode (if supported by monitor)",
	dmenuVarCheck, configSaver, NULL, "saver=green" },
      { "8 Logo",	"FreeBSD \"logo\" animated screen saver (graphics)",
	dmenuVarCheck, configSaver, NULL, "saver=logo" },
      { "9 Rain",	"Rain drops screen saver",
	dmenuVarCheck, configSaver, NULL, "saver=rain" },
      { "a Snake",	"Draw a FreeBSD \"snake\" on your screen",
	dmenuVarCheck, configSaver, NULL, "saver=snake" },
      { "b Star",	"A \"twinkling stars\" effect",
	dmenuVarCheck, configSaver, NULL, "saver=star" },
      { "c Warp",	"A \"stars warping\" effect",
	dmenuVarCheck, configSaver, NULL, "saver=warp" },
      { "d None",	"Disable the screensaver",
        dmenuVarCheck, configSaver, NULL, "saver=NO" },
      { "Timeout",	"Set the screen saver timeout interval",
	NULL, configSaverTimeout, NULL, NULL, ' ', ' ', ' ' },
      { NULL } }
};

#ifndef PC98
DMenu MenuSysconsScrnmap = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Screenmap",
    "Unless you load a specific font, most PC hardware defaults to\n"
    "displaying characters in the IBM 437 character set.  However,\n"
    "in the Unix world, this character set is very rarely used.  Most\n"
    "Western European countries, for example, prefer ISO 8859-1.\n"
    "American users won't notice the difference since the bottom half\n"
    "of all these character sets is ANSI anyway.\n"
    "If your hardware is capable of downloading a new display font,\n"
    "you should probably choose that option.  However, for hardware\n"
    "where this is not possible (e.g. monochrome adapters), a screen\n"
    "map will give you the best approximation that your hardware can\n"
    "display at all.",
    "Choose a screen map",
    NULL,
    { { "1 None",                 "No screenmap, don't touch font", dmenuVarCheck, dmenuSetVariable, NULL, "scrnmap=NO" },
      { "2 ISO 8859-1 to IBM437", "W-Europe ISO 8859-1 to IBM 437 screenmap", dmenuVarCheck, dmenuSetVariable, NULL, "scrnmap=iso-8859-1_to_cp437" },
      { "3 ISO 8859-7 to IBM437", "Greek ISO 8859-7 to IBM 437 screenmap", dmenuVarCheck, dmenuSetVariable, NULL, "scrnmap=iso-8859-7_to_cp437" },
      { "4 US-ASCII to IBM437",   "US-ASCII to IBM 437 screenmap", dmenuVarCheck, dmenuSetVariable, NULL, "scrnmap=us-ascii_to_cp437" },
      { "5 KOI8-R to IBM866",     "Russian KOI8-R to IBM 866 screenmap", dmenuVarCheck, dmenuSetVariable, NULL, "scrnmap=koi8-r2cp866" },
      { "6 KOI8-U to IBM866u",    "Ukrainian KOI8-U to IBM 866u screenmap", dmenuVarCheck, dmenuSetVariable, NULL, "scrnmap=koi8-u2cp866u" },
      { NULL } },
};

DMenu MenuSysconsTtys = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Terminal Type",
    "For various console encodings, a corresponding terminal type\n"
    "must be chosen in /etc/ttys.\n\n"
    "WARNING: For compatibility reasons, only entries starting with\n"
    "ttyv and terminal types starting with cons[0-9] can be changed\n"
    "via this menu.\n",
    "Choose a terminal type",
    NULL,
    { { "1 None",               "Don't touch anything",  dmenuVarCheck, dmenuSetVariable, NULL, VAR_CONSTERM "=NO" },
      { "2 IBM437 (VGA default)", "cons25", dmenuVarCheck, dmenuSetVariable, NULL, VAR_CONSTERM "=cons25" },
      { "3 ISO 8859-1",         "cons25l1", dmenuVarCheck, dmenuSetVariable, NULL, VAR_CONSTERM "=cons25l1" },
      { "4 ISO 8859-2",         "cons25l2", dmenuVarCheck, dmenuSetVariable, NULL, VAR_CONSTERM "=cons25l2" },
      { "5 ISO 8859-7",         "cons25l7", dmenuVarCheck, dmenuSetVariable, NULL, VAR_CONSTERM "=cons25l7" },
      { "6 KOI8-R",             "cons25r", dmenuVarCheck, dmenuSetVariable, NULL, VAR_CONSTERM "=cons25r" },
      { "7 KOI8-U",             "cons25u", dmenuVarCheck, dmenuSetVariable, NULL, VAR_CONSTERM "=cons25u" },
      { "8 US-ASCII",           "cons25w", dmenuVarCheck, dmenuSetVariable, NULL, VAR_CONSTERM "=cons25w" },
      { NULL } },
};

DMenu MenuSysconsFont = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Font",
    "Most PC hardware defaults to displaying characters in the\n"
    "IBM 437 character set.  However, in the Unix world, this\n"
    "character set is very rarely used.  Most Western European\n"
    "countries, for example, prefer ISO 8859-1.\n"
    "American users won't notice the difference since the bottom half\n"
    "of all these charactersets is ANSI anyway.  However, they might\n"
    "want to load a font anyway to use the 30- or 50-line displays.\n"
    "If your hardware is capable of downloading a new display font,\n"
    "you can select the appropriate font below.",
    "Choose a font",
    NULL,
    { { "1 None", "Use hardware default font", dmenuVarCheck, dmenuSetVariables, NULL,
	"font8x8=NO,font8x14=NO,font8x16=NO" },
      { "2 IBM 437", "English and others, VGA default", dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=cp437-8x8,font8x14=cp437-8x14,font8x16=cp437-8x16" },
      { "3 IBM 850", "Western Europe, IBM encoding",	dmenuVarCheck,	dmenuSetVariables, NULL,
	"font8x8=cp850-8x8,font8x14=cp850-8x14,font8x16=cp850-8x16" },
      { "4 IBM 865", "Norwegian, IBM encoding",	dmenuVarCheck,	dmenuSetVariables, NULL,
	"font8x8=cp865-8x8,font8x14=cp865-8x14,font8x16=cp865-8x16" },
      { "5 IBM 866", "Russian, IBM encoding (use with KOI8-R screenmap)",   dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=cp866-8x8,font8x14=cp866-8x14,font8x16=cp866b-8x16,mousechar_start=3" },
      { "6 IBM 866u", "Ukrainian, IBM encoding (use with KOI8-U screenmap)",   dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=cp866u-8x8,font8x14=cp866u-8x14,font8x16=cp866u-8x16,mousechar_start=3" },
      { "7 IBM 1251", "Cyrillic, MS Windows encoding",  dmenuVarCheck, dmenuSetVariables, NULL,
	"font8x8=cp1251-8x8,font8x14=cp1251-8x14,font8x16=cp1251-8x16,mousechar_start=3" },
      { "8 ISO 8859-1", "Western Europe, ISO encoding", dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=iso-8x8,font8x14=iso-8x14,font8x16=iso-8x16" },
      { "9 ISO 8859-2", "Eastern Europe, ISO encoding", dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=iso02-8x8,font8x14=iso02-8x14,font8x16=iso02-8x16" },
      { "a ISO 8859-4", "Baltic, ISO encoding", dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=iso04-8x8,font8x14=iso04-8x14,font8x16=iso04-8x16" },
      { "b ISO 8859-7", "Greek, ISO encoding", dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=iso07-8x8,font8x14=iso07-8x14,font8x16=iso07-8x16" },
      { "c ISO 8859-8", "Hebrew, ISO encoding", dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=iso08-8x8,font8x14=iso08-8x14,font8x16=iso08-8x16" },
      { "d ISO 8859-15", "Europe, ISO encoding", dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=iso15-8x8,font8x14=iso15-8x14,font8x16=iso15-8x16" },
      { "e SWISS", "English, better resolution", dmenuVarCheck, dmenuSetVariables, NULL,
	"font8x8=swiss-8x8,font8x14=NO,font8x16=swiss-8x16" },
      { NULL } },
};
#endif /* PC98 */
#endif /* WITH_SYSCONS */

DMenu MenuUsermgmt = {
    DMENU_NORMAL_TYPE,
    "User and group management",
    "The submenus here allow to manipulate user groups and\n"
    "login accounts.\n",
    "Configure your user groups and users",
    NULL,
    { { "X Exit",	"Exit this menu (returning to previous)", NULL, dmenuExit },
      { "User",		"Add a new user to the system.",	NULL, userAddUser },
      { "Group",	"Add a new user group to the system.",	NULL, userAddGroup },
      { NULL } },
};

DMenu MenuSecurity = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "System Security Options Menu",
    "This menu allows you to configure aspects of the operating system security\n"
    "policy.  Please read the system documentation carefully before modifying\n"
    "these settings, as they may cause service disruption if used improperly.\n"
    "\n"
    "Most settings will take affect only following a system reboot.",
    "Configure system security options",
    NULL,
    { { "X Exit",      "Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { " Securelevel",	"Configure securelevels for the system",
	NULL, configSecurelevel },
#if 0
      { " LOMAC",         "Use Low Watermark Mandatory Access Control at boot",
	dmenuVarCheck,  dmenuToggleVariable, NULL, "lomac_enable=YES" },
#endif
      { " NFS port",	"Require that the NFS clients use reserved ports",
	dmenuVarCheck,  dmenuToggleVariable, NULL, "nfs_reserved_port_only=YES" },
      { NULL } },
};

DMenu MenuSecurelevel = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Securelevel Configuration Menu",
    "This menu allows you to select the securelevel your system runs with.\n"
    "When operating at a securelevel, certain root privileges are disabled,\n"
    "which may increase resistance to exploits and protect system integrity.\n"
    "In secure mode system flags may not be overriden by the root user,\n"
    "access to direct kernel memory is limited, and kernel modules may not\n"
    "be changed.  In highly secure mode, mounted file systems may not be\n"
    "modified on-disk, tampering with the system clock is prohibited.  In\n"
    "network secure mode configuration changes to firewalling are prohibited.\n",
    "Select a securelevel to operate at - F1 for help",
    "securelevel",
    { { "X Exit",      "Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { "Disabled", "Disable securelevels", NULL, configSecurelevelDisabled, },
      { "Secure", "Secure mode", NULL, configSecurelevelSecure },
      { "Highly Secure", "Highly secure mode", NULL, configSecurelevelHighlySecure }, 
      { "Network Secure", "Network secure mode", NULL, configSecurelevelNetworkSecure },
      { NULL } }
};

DMenu MenuFixit = {
    DMENU_NORMAL_TYPE,
    "Please choose a fixit option",
    "There are three ways of going into \"fixit\" mode:\n"
    "- you can use the live filesystem CDROM/DVD, in which case there will be\n"
    "  full access to the complete set of FreeBSD commands and utilities,\n"
    "- you can use the more limited (but perhaps customized) fixit floppy,\n"
    "- or you can start an Emergency Holographic Shell now, which is\n"
    "  limited to the subset of commands that is already available right now.",
    "Press F1 for more detailed repair instructions",
    "fixit",
{ { "X Exit",		"Exit this menu (returning to previous)",	NULL, dmenuExit },
  { "2 CDROM/DVD",	"Use the live filesystem CDROM/DVD",		NULL, installFixitCDROM },
  { "3 USB",		"Use the live filesystem from a USB drive",	NULL, installFixitUSB },
  { "4 Floppy",	"Use a floppy generated from the fixit image",	NULL, installFixitFloppy },
  { "5 Shell",		"Start an Emergency Holographic Shell",		NULL, installFixitHoloShell },
  { NULL } },
};
