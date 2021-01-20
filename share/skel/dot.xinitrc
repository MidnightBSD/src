#!/bin/sh
# ~/.xinitrc
# Executed by startx (run your window manager from here)

DEFAULT_SESSION="wmaker"

# Source GNUstep so we can use openapp and friends.
if [ -x /usr/local/GNUstep/System/Makefiles/GNUstep.sh ]; then
        . /usr/local/GNUstep/System/Makefiles/GNUstep.sh
fi
if [ -x /usr/local/GNUstep/System/Library/Makefiles/GNUstep.sh ]; then
        . /usr/local/GNUstep/System/Library/Makefiles/GNUstep.sh
fi

if [ -d /usr/local/etc/X11/xinit/xinitrc.d ]; then
  for f in /usr/local/etc/X11/xinit/xinitrc.d/*; do
    [ -x "$f" ] && . "$f"
  done
  unset f
fi


case $1 in
wmaker|startxfce4|twm|etoile)
	exec $1
	;;
*)
	exec $DEFAULT_SESSION
	;;
esac
