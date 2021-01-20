#!/bin/sh
# ~/.xinitrc
# Executed by startx (run your window manager from here)

DEFAULT_SESSION="wmaker"

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
