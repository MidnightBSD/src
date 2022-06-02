#!/bin/sh
# ~/.xinitrc
# Executed by startx (run your window manager from here)

if [ -f "/usr/local/bin/startxfce4" ]; then
DEFAULT_SESSION="startxfce4"
else
DEFAULT_SESSION="wmaker"
fi

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
wmaker|startxfce4|gnome-shell|cinnamon-session|twm|etoile)
	exec $1
	;;
Lumina-DE)
	exec start-lumina-desktop
	;;
*)
	exec $DEFAULT_SESSION
	;;
esac
