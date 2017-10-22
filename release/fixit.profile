:
# $FreeBSD$

export BLOCKSIZE=K
export PS1="Fixit# "
export EDITOR="/mnt2/rescue/vi"
export PAGER="/mnt2/usr/bin/more"
export SCSI_MODES="/mnt2/usr/share/misc/scsi_modes"
# the root MFS doesn't have /dev/nrsa0, pick a better default for mt(1)
export TAPE=/mnt2/dev/nrsa0
# make geom(8) utilities find their modules
export GEOM_LIBRARY_PATH="/mnt2/lib/geom:/lib/geom"

alias ls="ls -F"
alias ll="ls -l"
alias m="more -e"

echo '+---------------------------------------------------------------+'
echo '| You are now running from FreeBSD "fixit" media.               |'
echo '| ------------------------------------------------------------- |'
echo "| When you're finished with this shell, please type exit.       |"
echo '| The fixit media is mounted as /mnt2.                          |'
echo '|                                                               |'
echo '| You might want to symlink /mnt/etc/*pwd.db and /mnt/etc/group |'
echo '| to /etc after mounting a root filesystem from your disk.      |'
echo '| tar(1) will not restore all permissions correctly otherwise!  |'
echo '|                                                               |'
echo '| In order to load kernel modules you might want to add the     |'
echo '| fixit media to the kern.module_path sysctl variable so that   |'
echo '| the kernel knows where to find them.                          |'
echo '|                                                               |'
echo '| Note: you can use the arrow keys to browse through the        |'
echo '| command history of this shell.                                |'
echo '+---------------------------------------------------------------+'
echo
echo 'Good Luck!'
echo

# Make the arrow keys work; everybody will love this.
set -o emacs 2>/dev/null

cd /
