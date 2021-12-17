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

