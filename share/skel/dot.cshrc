# $FreeBSD: src/share/skel/dot.cshrc,v 1.13 2001/01/10 17:35:28 archie Exp $
# $MidnightBSD: src/share/skel/dot.cshrc,v 1.5 2007/10/13 21:25:46 laffer1 Exp $
#
# .cshrc - csh resource script, read at beginning of execution by each shell
#
# see also csh(1), environ(7).
#

alias h		history 25
alias j		jobs -l
alias la	ls -aF
alias lf	ls -FA
alias ll	ls -lAF

# A righteous umask
umask 22

set path = (/sbin /bin /usr/sbin /usr/bin /usr/games /usr/local/sbin /usr/local/bin $HOME/bin)

setenv	EDITOR	vi
setenv	PAGER	less
setenv	BLOCKSIZE	K

if ($?prompt) then
	# An interactive shell -- set some stuff up
	if ($uid == 0) then
		set user = root
	endif
	set prompt = "%n@%m:%/ %# "
	set promptchars = "%#"
	set filec
	set history = 1000
	set savehist = (1000 merge)
	set autolist = ambiguous
	# Use history to aid expansion
	set autoexpand
	set autorehash
	set mail = (/var/mail/$USER)
	if ( $?tcsh ) then
		bindkey "^W" backward-delete-word
		bindkey -k up history-search-backward
		bindkey -k down history-search-forward
	endif
	if ( -x /usr/local/GNUstep/System/Makefiles/GNUstep.csh ) then
		source /usr/local/GNUstep/System/Makefiles/GNUstep.csh
	endif
	if ( -x /usr/local/GNUstep/System/Library/Makefiles/GNUstep.csh ) then
		source /usr/local/GNUstep/System/Library/Makefiles/GNUstep.csh
	endif
endif
