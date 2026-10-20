#!/bin/sh
# ~/.xinitrc
# Executed by startx (run your window manager from here)

# firstboot may override this with a script in xinitrc.d below.
if [ -f "/usr/local/bin/startxfce4" ]; then
DEFAULT_SESSION="startxfce4"
elif [ -f "/usr/local/bin/midnight-wm" ]; then
DEFAULT_SESSION="midnight-wm"
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

# A display manager may already have registered this login with ConsoleKit.
# For a plain startx session, register it before starting the desktop.
start_session() {
  if [ -z "${XDG_SESSION_COOKIE:-}" ] && command -v ck-launch-session >/dev/null 2>&1; then
    exec ck-launch-session "$@"
  fi
  exec "$@"
}

case $1 in
midnight-wm|wmaker|startxfce4|gnome-shell|cinnamon-session|twm|etoile)
	start_session "$1"
	;;
Lumina-DE)
	start_session start-lumina-desktop
	;;
*)
	start_session "$DEFAULT_SESSION"
	;;
esac
