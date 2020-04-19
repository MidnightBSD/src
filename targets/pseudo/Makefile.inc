# $FreeBSD: stable/11/targets/pseudo/Makefile.inc 248346 2013-03-15 20:21:18Z sjg $

# we don't do anything in these dirs
# they are just for triggering building of other things

UPDATE_DEPENDFILE= no

.if !target(all)
all:
	@echo Done!
.endif


