#!/bin/sh
# -*- tab-width:  4 -*- ;; Emacs
# vi: set tabstop=4     :: Vi/ViM
#
# Revision: 2.5.1
# Last Modified: November 2nd, 2010
############################################################ COPYRIGHT
#
# (c)2010. Devin Teske. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# AUTHOR      DATE      DESCRIPTION
# dteske   2010.11.02   Fix quotes when replacing null assignment.
# dteske   2010.11.02   Preserve leading/trailing whitespace in sysrc_set().
# dteske   2010.11.02   Deprecate lrev() in favor of tail(1)'s `-r' flag.
# dteske   2010.11.02   Map `-R dir' to `-j jail' if `dir' maps to a single
#                       running jail (or produce an error if `dir' maps to
#                       many running jails).
# dteske   2010.10.20   Make `-j jail' and `-R dir' more secure
# dteske   2010.10.19   Add `-j jail' for operating on jails (see jexec(8)).
# dteske   2010.10.18   Add `-R dir' for operating in different root-dir.
# dteske   2010.10.13   Allow `-f file' multiple times.
# dteske   2010.10.12   Updates per freebsd-hackers thread.
# dteske   2010.09.29   Initial version.
#
# $MidnightBSD$
#
############################################################ INFORMATION
#
# Command Usage:
#
#   sysrc [OPTIONS] name[=value] ...
#
#   OPTIONS:
#   	-h         Print this message to stderr and exit.
#   	-f file    Operate on the specified file(s) instead of rc_conf_files.
#   	           Can be specified multiple times for additional files.
#   	-a         Dump a list of non-default configuration variables.
#   	-A         Dump a list of all configuration variables (incl. defaults).
#   	-d         Print a description of the given variable.
#   	-e         Print query results as `var=value' (useful for producing
#   	           output to be fed back in). Ignored if -n is specified.
#   	-v         Verbose. Print the pathname of the specific rc.conf(5)
#   	           file where the directive was found.
#   	-i         Ignore unknown variables.
#   	-n         Show only variable values, not their names.
#   	-N         Show only variable names, not their values.
#   	-R dir     Operate within the root directory `dir' rather than `/'.
#   	-j jail    The jid or name of the jail to operate within (overrides
#   	           `-R dir'; requires jexec(8)).
# 
#   ENVIRONMENT:
#   	RC_DEFAULTS      Location of `/etc/defaults/rc.conf' file.
#   	SYSRC_VERBOSE    Default verbosity. Set to non-NULL to enable.
#
############################################################ CONFIGURATION

#
# Default verbosity.
#
: ${SYSRC_VERBOSE:=}

#
# Default location of the rc.conf(5) defaults configuration file.
#
: ${RC_DEFAULTS:="/etc/defaults/rc.conf"}

############################################################ GLOBALS

#
# Global exit status variables
#
SUCCESS=0
FAILURE=1

#
# Program name
#
progname="${0##*/}"

#
# Options
#
DESCRIBE=
IGNORE_UNKNOWNS=
JAIL=
RC_CONFS=
ROOTDIR=
SHOW_ALL=
SHOW_EQUALS=
SHOW_NAME=1
SHOW_VALUE=1

############################################################ FUNCTION

# have $anything
#
# A wrapper to the `type' built-in. Returns true if argument is a valid shell
# built-in, keyword, or externally-tracked binary, otherwise false.
#
have()
{
	type "$@" > /dev/null 2>&1
}

# fprintf $fd $fmt [ $opts ... ]
#
# Like printf, except allows you to print to a specific file-descriptor. Useful
# for printing to stderr (fd=2) or some other known file-descriptor.
#
fprintf()
{
	local fd=$1
	[ $# -gt 1 ] || return $FAILURE
	shift 1
	printf "$@" >&$fd
}

# eprintf $fmt [ $opts ... ]
#
# Print a message to stderr (fd=2).
#
eprintf()
{
	fprintf 2 "$@"
}

# die [ $fmt [ $opts ... ]]
#
# Optionally print a message to stderr before exiting with failure status.
#
die()
{
	local fmt="$1"
	[ $# -gt 0 ] && shift 1
	[  "$fmt"  ] && eprintf "$fmt\n" "$@"

	exit $FAILURE
}

# usage
#
# Prints a short syntax statement and exits.
#
usage()
{
	local optfmt="\t%-11s%s\n"
	local envfmt="\t%-17s%s\n"

	eprintf "Usage: %s [OPTIONS] name[=value] ...\n" "$progname"

	eprintf "OPTIONS:\n"
	eprintf "$optfmt" "-h" \
	        "Print this message to stderr and exit."
	eprintf "$optfmt" "-f file" \
	        "Operate on the specified file(s) instead of rc_conf_files."
	eprintf "$optfmt" "" \
	        "Can be specified multiple times for additional files."
	eprintf "$optfmt" "-a" \
	        "Dump a list of non-default configuration variables."
	eprintf "$optfmt" "-A" \
	        "Dump a list of all configuration variables (incl. defaults)."
	eprintf "$optfmt" "-d" \
	        "Print a description of the given variable."
	eprintf "$optfmt" "-e" \
	        "Print query results as \`var=value' (useful for producing"
	eprintf "$optfmt" "" \
	        "output to be fed back in). Ignored if -n is specified."
	eprintf "$optfmt" "-v" \
	        "Verbose. Print the pathname of the specific rc.conf(5)"
	eprintf "$optfmt" "" \
	        "file where the directive was found."
	eprintf "$optfmt" "-i" \
	        "Ignore unknown variables."
	eprintf "$optfmt" "-n" \
	        "Show only variable values, not their names."
	eprintf "$optfmt" "-N" \
	        "Show only variable names, not their values."
	eprintf "$optfmt" "-R dir" \
	        "Operate within the root directory \`dir' rather than \`/'."
	eprintf "$optfmt" "-j jail" \
	        "The jid or name of the jail to operate within (overrides"
	eprintf "$optfmt" "" \
	        "\`-R dir'; requires jexec(8))."
	eprintf "\n"

	eprintf "ENVIRONMENT:\n"
	eprintf "$envfmt" "RC_DEFAULTS" \
	        "Location of \`/etc/defaults/rc.conf' file."
	eprintf "$envfmt" "SYSRC_VERBOSE" \
	        "Default verbosity. Set to non-NULL to enable."

	die
}

# clean_env [ --except $varname ... ]
#
# Unset all environment variables in the current scope. An optional list of
# arguments can be passed, indicating which variables to avoid unsetting; the
# `--except' is required to enable the exclusion-list as the remainder of
# positional arguments.
#
# Be careful not to call this in a shell that you still expect to perform
# $PATH expansion in, because this will blow $PATH away. This is best used
# within a sub-shell block "(...)" or "$(...)" or "`...`".
#
clean_env()
{
	local var arg except=

	#
	# Should we process an exclusion-list?
	#
	if [ "$1" = "--except" ]; then
		except=1
		shift 1
	fi

	#
	# Loop over a list of variable names from set(1) built-in.
	#
	for var in $( set | awk -F= \
		'/^[[:alpha:]_][[:alnum:]_]*=/ {print $1}' \
		| grep -v '^except$'
	); do
		#
		# In POSIX bourne-shell, attempting to unset(1) OPTIND results
		# in "unset: Illegal number:" and causes abrupt termination.
		#
		[ "$var" = OPTIND ] && continue

		#
		# Process the exclusion-list?
		#
		if [ "$except" ]; then
			for arg in "$@" ""; do
				[ "$var" = "$arg" ] && break
			done
			[ "$arg" ] && continue
		fi

		unset "$var"
	done
}

# sysrc_get $varname
#
# Get a system configuration setting from the collection of system-
# configuration files (in order: /etc/defaults/rc.conf /etc/rc.conf
# and /etc/rc.conf).
#
# NOTE: Additional shell parameter-expansion formats are supported. For
# example, passing an argument of "hostname%%.*" (properly quoted) will
# return the hostname up to (but not including) the first `.' (see sh(1),
# "Parameter Expansion" for more information on additional formats).
#
sysrc_get()
{
	# Sanity check
	[ -f "$RC_DEFAULTS" -a -r "$RC_DEFAULTS" ] || return $FAILURE

	# Taint-check variable name
	case "$1" in
	[0-9]*)
		# Don't expand possible positional parameters
		return $FAILURE;;
	*)
		[ "$1" ] || return $FAILURE
	esac

	( # Execute within sub-shell to protect parent environment

		#
		# Clear the environment of all variables, preventing the
		# expansion of normals such as `PS1', `TERM', etc.
		#
		clean_env --except RC_CONFS RC_DEFAULTS

		. "$RC_DEFAULTS"

		#
		# If the query is for `rc_conf_files' then store the value that
		# we inherited from sourcing RC_DEFAULTS (above) so that we may
		# conditionally restore this value after source_rc_confs in the
		# event that RC_CONFS does not customize the value.
		#
		if [ "$1" = "rc_conf_files" ]; then
			_rc_conf_files="$rc_conf_files"
		fi

		#
		# If `-f file' was passed, set $rc_conf_files to an explicit
		# value, modifying the default behavior of source_rc_confs().
		#
		[ "$RC_CONFS" ] && rc_conf_files="$RC_CONFS"

		source_rc_confs

		#
		# If the query was for `rc_conf_files' AND after calling
		# source_rc_confs the value has not changed, then we should
		# restore the value to the one inherited from RC_DEFAULTS
		# before performing the final query (preventing us from
		# returning what was passed in via `-f' when the intent was
		# instead to query the value from the file(s) specified).
		#
		if [ "$1" = "rc_conf_files" -a \
		     "$RC_CONFS" != "" -a \
		     "$rc_conf_files" = "$RC_CONFS" \
		]; then
			rc_conf_files="$_rc_conf_files"
			unset _rc_conf_files
		fi

		unset RC_CONFS
			# no longer needed

		#
		# This must be the last functional line for both the sub-shell,
		# and the function to preserve the return status from formats
		# such as "${varname?}" and "${varname:?}" (see "Parameter
		# Expansion" in sh(1) for more information).
		#
		eval echo '"${'"$1"'}"' 2> /dev/null
	)
}

# sysrc_find $varname
#
# Find which file holds the effective last-assignment to a given variable
# within the rc.conf(5) file(s).
#
# If the variable is found in any of the rc.conf(5) files, the function prints
# the filename it was found in and then returns success. Otherwise output is
# NULL and the function returns with error status.
#
sysrc_find()
{
	local varname="$1"
	local rc_conf_files="$( sysrc_get rc_conf_files )"
	local conf_files=
	local file

	# Check parameters
	[ "$varname" ] || return $FAILURE

	#
	# If `-f file' was passed, set $rc_conf_files to an explicit
	# value, modifying the default behavior of source_rc_confs().
	#
	[ "$RC_CONFS" ] && rc_conf_files="$RC_CONFS"

	#
	# Reverse the order of files in rc_conf_files (the boot process sources
	# these in order, so we will search them in reverse-order to find the
	# last-assignment -- the one that ultimately effects the environment).
	#
	for file in $rc_conf_files; do
		conf_files="$file${conf_files:+ }$conf_files"
	done

	#
	# Append the defaults file (since directives in the defaults file
	# indeed affect the boot process, we'll want to know when a directive
	# is found there).
	#
	conf_files="$conf_files${conf_files:+ }$RC_DEFAULTS"

	#
	# Find which file matches assignment to the given variable name.
	#
	for file in $conf_files; do
		[ -f "$file" -a -r "$file" ] || continue
		if grep -q "^[[:space:]]*$varname=" $file; then
			echo $file
			return $SUCCESS
		fi
	done

	return $FAILURE # Not found
}

# sysrc_set $varname $new_value
#
# Change a setting in the system configuration files (edits the files in-place
# to change the value in the last assignment to the variable). If the variable
# does not appear in the source file, it is appended to the end of the primary
# system configuration file `/etc/rc.conf'.
#
sysrc_set()
{
	local varname="$1" new_value="$2"

	# Check arguments
	[ "$varname" ] || return $FAILURE

	#
	# Find which rc.conf(5) file contains the last-assignment
	#
	local not_found=
	local file="$( sysrc_find "$varname" )"
	if [ "$file" = "$RC_DEFAULTS" -o ! "$file" ]; then
		#
		# We either got a null response (not found) or the variable
		# was only found in the rc.conf(5) defaults. In either case,
		# let's instead modify the first file from $rc_conf_files.
		#

		not_found=1

		#
		# If `-f file' was passed, use $RC_CONFS
		# rather than $rc_conf_files.
		#
		if [ "$RC_CONFS" ]; then
			file="${RC_CONFS%%[$IFS]*}"
		else
			file="$( sysrc_get "rc_conf_files%%[$IFS]*" )"
		fi
	fi

	#
	# If not found, append new value to last file and return.
	#
	if [ "$not_found" ]; then
		echo "$varname=\"$new_value\"" >> "$file"
		return $SUCCESS
	fi

	#
	# Perform sanity checks.
	#
	if [ ! -w $file ]; then
		eprintf "\n%s: cannot create %s: Permission denied\n" \
		        "$progname" "$file"
		return $FAILURE
	fi

	#
	# Operate on the matching file, replacing only the last occurrence.
	#
	local __IFS="$IFS"
	local new_contents="`tail -r $file 2> /dev/null | \
	( found=
	  IFS=
	  while read -r LINE; do
		# If already found, just spew...
	  	if [ "$found" ]; then
			echo "$LINE"
			continue
		fi

		#
		# Determine what type of assignment is being performed
		# and append the proper expression to accurately replace
		# the current value.
		#
		# NOTE: The base regular expression below should match
		#       functionally the regex used by sysrc_find().
		#
		regex="^([[:space:]]*$varname=)"
		if echo "$LINE" | grep -Eq "$regex'"; then
			# found assignment w/ single-quoted value
			found=1
			regex="$regex(')([^']*)('{0,1})"
		elif echo "$LINE" | grep -Eq "$regex"'"'; then
			# found assignment w/ double-quoted value
			found=1
			regex="$regex"'(")([^\\\\]*\\\\")*[^"]*("{0,1})'
		elif echo "$LINE" | grep -Eq "$regex[^[:space:]]"; then
			# found assignment w/ non-quoted value
			found=1
			regex="$regex()([^[:space:]]*)()"

			# Use quotes if replacing with multi-word value
			[ "${new_value%[$__IFS]*}" != "$new_value" ] \
				&& new_value='"'"$new_value"'"'
		elif echo "$LINE" | grep -Eq "$regex"; then
			# found null-assignment
			found=1
			regex="$regex()()()"

			# Always use quotes
			new_value='"'"$new_value"'"'
		fi

		# Do the deed...
		[ "$found" ] && LINE="$( echo "$LINE" \
			| sed -re "s/$regex/\1\2$new_value\4/" )"

		echo "$LINE"
	  done
	) | tail -r`"

	[ "$new_contents" ] || return $FAILURE

	#
	# Create a new temporary file to write to.
	#
	local tmpfile="$( mktemp -t "$progname" )"
	[ "$tmpfile" ] || return $FAILURE

	#
	# Fixup permissions (else we're in for a surprise, as mktemp(1) creates
	# the temporary file with 0600 permissions, and if we simply mv(1) the
	# temporary file over the destination, the destination will inherit the
	# permissions from the temporary file).
	#
	chmod $( stat -f '%#Lp' "$file" ) "$tmpfile" 2> /dev/null

	#
	# Fixup ownerhsip. The destination file _is_ writable (we tested
	# earlier above). However, this will fail if we don't have sufficient
	# permissions (so we throw stderr into the bit-bucket).
	#
	chown $( stat -f '%u:%g' "$file" ) "$tmpfile" 2> /dev/null

	#
	# Write the temporary file contents and move it into place.
	#
	echo "$new_contents" > "$tmpfile" || return $FAILURE
	mv "$tmpfile" "$file"
}

# sysrc_desc $varname
#
# Attempts to return the comments associated with varname from the rc.conf(5)
# defaults file `/etc/defaults/rc.conf' (or whatever RC_DEFAULTS points to).
#
# Multi-line comments are joined together. Results are NULL if no description
# could be found.
#
sysrc_desc()
{
	local varname="$1"

	(
		buffer=
		while read LINE; do
			case "$LINE" in
			$varname=*)
				buffer="$LINE"
				break
			esac
		done

		# Return if the variable wasn't found
		[ "$buffer" ] || return $FAILURE

		regex='[[:alpha:]_][[:alnum:]_]*='
		while read LINE; do
			#
			# Stop reading comments if we reach a new assignment
			# directive or if the line contains only whitespace
			#
			echo "$LINE" | grep -q "^[[:space:]]*$regex" && break
			echo "$LINE" | grep -q "^[[:space:]]*#$regex" && break
			echo "$LINE" | grep -q "^[[:space:]]*$" && break

			# Append new line to buffer
			buffer="$buffer
$LINE"
		done

		# Return if the buffer is empty
		[ "$buffer" ] || return $FAILURE

		#
		# Clean up the buffer.
		#
		regex='^[^#]*\(#[[:space:]]*\)\{0,1\}'
		buffer="$( echo "$buffer" | sed -e "s/$regex//" )"
		buffer="$( echo "$buffer" | tr '\n' ' ' \
			| sed -e 's/^[[:space:]]*//;s/[[:space:]]*$//' )"

		echo "$buffer"

	) < "$RC_DEFAULTS"
}

############################################################ MAIN SOURCE

#
# Perform sanity checks
#
[ $# -gt 0 ] || usage

#
# Process command-line options
#
while getopts hf:aAdevinNR:j: flag; do
	case "$flag" in
	h) usage;;
	f) [ "$OPTARG" ] || die \
	   	"%s: Missing or null argument to \`-f' flag" "$progname"
	   RC_CONFS="$RC_CONFS${RC_CONFS:+ }$OPTARG";;
	a) SHOW_ALL=1;;
	A) SHOW_ALL=2;;
	d) DESCRIBE=1;;
	e) SHOW_EQUALS=1;;
	v) SYSRC_VERBOSE=1;;
	i) IGNORE_UNKNOWNS=1;;
	n) SHOW_NAME=;;
	N) SHOW_VALUE=;;
	R) [ "$OPTARG" ] || die \
	   	"%s: Missing or null argument to \`-R' flag" "$progname"
	   ROOTDIR="$OPTARG";;
	j) [ "$OPTARG" ] || die \
	   	"%s: Missing or null argument to \`-j' flag" "$progname"
	   JAIL="$OPTARG";;
	\?) usage;;
	esac
done
shift $(( $OPTIND - 1 ))

#
# Process `-e', `-n', and `-N' command-line options
#
SEP=': '
[ "$SHOW_EQUALS" ] && SEP='="'
[ "$SHOW_NAME" ] || SHOW_EQUALS=
[ "$SYSRC_VERBOSE" = "0" ] && SYSRC_VERBOSE=
if [ ! "$SHOW_VALUE" ]; then
	SHOW_NAME=1
	SHOW_EQUALS=
fi

#
# Process `-j jail' and `-R dir' command-line options
#
if [ "$JAIL" -o "$ROOTDIR" ]; then
	#
	# Reconstruct the arguments that we want to carry-over
	#
	args="
		${SYSRC_VERBOSE:+-v}
		${RC_CONFS:+-f'$RC_CONFS'}
		$( [ "$SHOW_ALL" = "1" ] && echo \ -a )
		$( [ "$SHOW_ALL" = "2" ] && echo \ -A )
		${DESCRIBE:+-d}
		${SHOW_EQUALS:+-e}
		${IGNORE_UNKNOWNS:+-i}
		$( [ "$SHOW_NAME"  ] || echo \ -n )
		$( [ "$SHOW_VALUE" ] || echo \ -N )
	"
	for arg in "$@"; do
		args="$args '$arg'"
	done

	#
	# If both are supplied, `-j jail' supercedes `-R dir'
	#
	if [ "$JAIL" ]; then
		#
		# Re-execute ourselves with sh(1) via jexec(8)
		#
		( echo set -- $args
		  cat $0
		) | env - RC_DEFAULTS="$RC_DEFAULTS" \
		    	/usr/sbin/jexec "$JAIL" /bin/sh
		exit $?
	elif [ "$ROOTDIR" ]; then
		#
		# Make sure that the root directory specified is not to any
		# running jails.
		#
		# NOTE: To maintain backward compatibility with older jails on
		# older systems, we will not perform this check if either the
		# jls(8) or jexec(8) utilities are missing.
		#
		if have jexec && have jls; then
			jid="`jls jid path | \
			(
				while read JID JROOT; do
					[ "$JROOT" = "$ROOTDIR" ] || continue
					echo $JID
				done
			)`"

			#
			# If multiple running jails match the specified root
			# directory, exit with error.
			#
			if [ "$jid" -a "${jid%[$IFS]*}" != "$jid" ]; then
				die "%s: %s: %s" "$progname" "$ROOTDIR" \
					"$( echo "Multiple jails claim this" \
					         "directory as their root." \
					         "(use \`-j jail' instead)" )"
			fi

			#
			# If only a single running jail matches the specified
			# root directory, implicitly use `-j jail'.
			#
			if [ "$jid" ]; then
				#
				# Re-execute outselves with sh(1) via jexec(8)
				#
				( echo set -- $args
				  cat $0
				) | env - RC_DEFAULTS="$RC_DEFAULTS" \
					/usr/sbin/jexec "$jid" /bin/sh
				exit $?
			fi

			# Otherwise, fall through and allow chroot(8)
		fi

		#
		# Re-execute ourselves with sh(1) via chroot(8)
		#
		( echo set -- $args
		  cat $0
		) | env - RC_DEFAULTS="$RC_DEFAULTS" \
		    	/usr/sbin/chroot "$ROOTDIR" /bin/sh
		exit $?
	fi
fi

#
# Process `-a' or `-A' command-line options
#
if [ "$SHOW_ALL" ]; then
	#
	# Get a list of variables that are currently set in the rc.conf(5)
	# files (included `/etc/defaults/rc.conf') by performing a call to
	# source_rc_confs() in a clean environment.
	#
	( # Operate in a sub-shell to protect the parent environment
		#
		# Set which variables we want to preserve in the environment.
		# Append the pipe-character (|) to the list of internal field
		# separation (IFS) characters, allowing us to use the below
		# list both as an extended grep (-E) pattern and argument list
		# (required to first get clean_env() to preserve these in the
		# environment and then later to prune them from the list of
		# variables produced by set(1)).
		#
		IFS="$IFS|"
		EXCEPT="IFS|EXCEPT|PATH|RC_DEFAULTS|OPTIND|DESCRIBE|SEP"
		EXCEPT="$EXCEPT|SHOW_ALL|SHOW_EQUALS|SHOW_NAME|SHOW_VALUE"
		EXCEPT="$EXCEPT|SYSRC_VERBOSE|RC_CONFS"

		#
		# Clean the environment (except for our required variables)
		# and then source the required files.
		#
		clean_env --except $EXCEPT
		if [ -f "$RC_DEFAULTS" -a -r "$RC_DEFAULTS" ]; then
			. "$RC_DEFAULTS"

			#
			# If passed `-a' (rather than `-A'), re-purge the
			# environment, removing the rc.conf(5) defaults.
			#
			[ "$SHOW_ALL" = "1" ] \
				&& clean_env --except rc_conf_files $EXCEPT

			#
			# If `-f file' was passed, set $rc_conf_files to an
			# explicit value, modifying the default behavior of
			# source_rc_confs().
			#
			[ "$RC_CONFS" ] && rc_conf_files="$RC_CONFS"

			source_rc_confs

			#
			# If passed `-a' (rather than `-A'), remove
			# `rc_conf_files' unless it was defined somewhere
			# other than rc.conf(5) defaults.
			#
			[ "$SHOW_ALL" = "1" -a \
			  "$( sysrc_find rc_conf_files )" = "$RC_DEFAULTS" \
			] \
			&& unset rc_conf_files
		fi

		for NAME in $( set | awk -F= \
			'/^[[:alpha:]_][[:alnum:]_]*=/ {print $1}' \
			| grep -Ev "^($EXCEPT)$"
		); do
			#
			# If enabled, describe rather than expand value
			#
			if [ "$DESCRIBE" ]; then
				echo "$NAME: $( sysrc_desc "$NAME" )"
				continue
			fi

			[ "$SYSRC_VERBOSE" ] && \
				echo -n "$( sysrc_find "$NAME" ): "

			#
			# If `-N' is passed, simplify the output
			#
			if [ ! "$SHOW_VALUE" ]; then
				echo "$NAME"
				continue
			fi

			echo "${SHOW_NAME:+$NAME$SEP}$(
			      sysrc_get "$NAME" )${SHOW_EQUALS:+\"}"
		done
	)

	#
	# Ignore the remainder of positional arguments.
	#
	exit $SUCCESS
fi

#
# Process command-line arguments
#
while [ $# -gt 0 ]; do
	NAME="${1%%=*}"

	[ "$DESCRIBE" ] && \
		echo "$NAME: $( sysrc_desc "$NAME" )"

	case "$1" in
	*=*)
		#
		# Like sysctl(8), if both `-d' AND "name=value" is passed,
		# first describe, then attempt to set
		#

		if [ "$SYSRC_VERBOSE" ]; then
			file="$( sysrc_find "$NAME" )"
			[ "$file" = "$RC_DEFAULTS" -o ! "$file" ] && \
				file="$( sysrc_get "rc_conf_files%%[$IFS]*" )"
			echo -n "$file: "
		fi

		#
		# If `-N' is passed, simplify the output
		#
		if [ ! "$SHOW_VALUE" ]; then
			echo "$NAME"
			sysrc_set "$NAME" "${1#*}"
		else
			echo -n "${SHOW_NAME:+$NAME$SEP}$(
			         sysrc_get "$NAME" )${SHOW_EQUALS:+\"}"
			if sysrc_set "$NAME" "${1#*=}"; then
				echo " -> $( sysrc_get "$NAME" )"
			fi
		fi
		;;
	*)
		if ! IGNORED="$( sysrc_get "$NAME?" )"; then
			[ "$IGNORE_UNKNOWNS" ] \
				|| echo "$progname: unknown variable '$NAME'"
			shift 1
			continue
		fi

		#
		# Like sysctl(8), when `-d' is passed,
		# desribe it rather than expanding it
		#

		if [ "$DESCRIBE" ]; then
			shift 1
			continue
		fi

		[ "$SYSRC_VERBOSE" ] && \
			echo -n "$( sysrc_find "$NAME" ): "

		#
		# If `-N' is passed, simplify the output
		#
		if [ ! "$SHOW_VALUE" ]; then
			echo "$NAME"
		else
			echo "${SHOW_NAME:+$NAME$SEP}$(
			      sysrc_get "$NAME" )${SHOW_EQUALS:+\"}"
		fi
	esac
	shift 1
done
