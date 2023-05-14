# mport
MidnightBSD Package Manager

## Requirements
Will run on MidnightBSD 1.2 and higher. 

Depends on:
* sqlite3
* libarchive
* bzip2
* lzma

Versions prior to 2.1.0 also depended on
* libdispatch
* Blocksruntime

## Backward compatibility
There was a breaking change in 2.1.6 in libmport with respect to mport_install and mport_install_primative use

## Using mport

In addition to the man page, you can also look at the BSD Magazine article on mport in the Feb 2012 issue.
https://ia902902.us.archive.org/6/items/bsdmagazine/BSD_02_2012.pdf

### Quick and Dirty

Installing a package named xorg:

`mport install xorg`

Deleting the xorg meta package:

`mport delete xorg`

Fetching the latest index

`mport index`

Upgrading all packages

`mport upgrade`

Update a specific package (and it's dependencies)

`mport update xorg`

### Installing from package file

Example installing a vim package that is already built locally

`/usr/libexec/mport.install /usr/mports/Packages/amd64/All/vim-8.2.3394.mport` 

### Getting info on an installed package
`mport info gmake`

```
gmake-4.3_2
Name            : gmake
Version         : 4.3_2
Latest          : 4.3_2
Licenses        : gpl3
Origin          : devel/gmake
Flavor          : 
OS              : 3.0
CPE             : cpe:2.3:a:gnu:make:4.3:::::midnightbsd3:x64:2
PURL            : pkg:mport/midnightbsd/gmake@4.3_2?arch=amd64&osrel=3.0
Locked          : no
Prime           : yes
Shared library  : no
Deprecated      : no
Expiration Date : 
Install Date    : Tue Mar 28 17:51:14 2023
Comment         : GNU version of 'make' utility
Options         : 
Type            : Application
Description     :
```
### Security related commands

`mport audit`
Displays vulnerable packages based on their CPE indetifiers using the NVD data provided by https://sec.midnightbsd.org

`mport audit -r`
Prints out vulnerable packages and a list of packages depending on that one.

`mport -q audit`
Prints out vulnerable package name and version with no descriptions or details

`mport cpe`
Lists all CPE info on installed packages

`mport verify`
Runs a checksum on all installed files from packages against data from time of installation to see if files have been modified.
