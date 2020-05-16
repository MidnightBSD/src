# $MidnightBSD: src/share/skel/dot.profile,v 1.11 2012/07/07 23:34:29 laffer1 Exp $
#
# .profile - Bourne Shell startup script for login shells
#
# see also sh(1), ksh(1), environ(7).
#

# remove /usr/games if you want
PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/games:/usr/local/sbin:/usr/local/bin:$HOME/bin; export PATH

# A righteous umask
umask 22

# set a few alias
alias h="fc -l"
alias j="jobs -l"
alias la="ls -aF"
alias lf="ls -FA"
alias ll="ls -lAF"

# Setting TERM is normally done through /etc/ttys.  Do only override
# if you're sure that you'll never log in via telnet or xterm or a
# serial line.
# TERM=xterm; 	export TERM

BLOCKSIZE=K;	export BLOCKSIZE
EDITOR=vi;   	export EDITOR
PAGER=more;  	export PAGER
VISUAL=vi;	export VISUAL

# set ENV to a file invoked each time sh is started for interactive use.
if [ $SHELL = "/bin/ksh" ]; then
	ENV=$HOME/.kshrc; export ENV
elif [ $SHELL = "/bin/sh" ]; then
	ENV=$HOME/.shrc; export ENV
fi

# Source GNUstep so we can use openapp and friends.
if [ -x /usr/local/GNUstep/System/Makefiles/GNUstep.sh ]; then
	. /usr/local/GNUstep/System/Makefiles/GNUstep.sh
fi
if [ -x /usr/local/GNUstep/System/Library/Makefiles/GNUstep.sh ]; then
	. /usr/local/GNUstep/System/Library/Makefiles/GNUstep.sh
fi

SSHAGENT=/usr/bin/ssh-agent
if [ -z "$SSH_AUTH_SOCK" -a -x "$SSHAGENT" ]; then
  eval `$SSHAGENT`
  trap "kill $SSH_AGENT_PID" 0
fi

# Query terminal size; useful for serial lines.
if [ -x /usr/bin/resizewin ] ; then /usr/bin/resizewin -z ; fi

if [ -x /usr/games/fortune ] ; then  /usr/games/fortune fortunes; fi
