#!/bin/sh
# Submit a problem report to a GNATS site.
# Copyright (C) 1993 Free Software Foundation, Inc.
# Contributed by Brendan Kehoe (brendan@cygnus.com), based on a
# version written by Heinz G. Seidl (hgs@cygnus.com).
#
# This file is part of GNU GNATS.
#
# GNU GNATS is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# GNU GNATS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU GNATS; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
#
# $FreeBSD$

# The version of this send-pr.
VERSION=3.113

# The submitter-id for your site.
# "current-users" is the only allowable value for FreeBSD.
SUBMITTER="current-users"

# Where the GNATS directory lives, if at all.
[ -z "$GNATS_ROOT" ] && 
GNATS_ROOT=

# The default mail address for PR submissions. 
GNATS_ADDR=FreeBSD-gnats-submit@freebsd.org

# Where the gnats category tree lives.
DATADIR=@DATADIR@

# If we've been moved around, try using GCC_EXEC_PREFIX.
[ ! -d $DATADIR/gnats -a -d "$GCC_EXEC_PREFIX" ] && DATADIR="$GCC_EXEC_PREFIX"

# The default release for this host.
DEFAULT_RELEASE="@DEFAULT_RELEASE@"

# The default organization.
DEFAULT_ORGANIZATION=

# The default site to look for.
GNATS_SITE=freefall

# Newer config information?
[ -f ${GNATS_ROOT}/gnats-adm/config ] && . ${GNATS_ROOT}/gnats-adm/config

# What mailer to use.  This must come after the config file, since it is
# host-dependent.
MAIL_AGENT="${MAIL_AGENT:-/usr/sbin/sendmail -oi -t}"

# Path to pw(8)
PW="/usr/sbin/pw"

ECHON=bsd

if [ $ECHON = bsd ] ; then
  ECHON1="echo -n"
  ECHON2=
elif [ $ECHON = sysv ] ; then
  ECHON1=echo
  ECHON2='\c'
else
  ECHON1=echo
  ECHON2=
fi

#

# find a user name
if [ "$LOGNAME" = "" ]; then
	if [ "$USER" != "" ]; then
		LOGNAME="$USER"
	else
		LOGNAME="UNKNOWN"
	fi
fi

# Find out the name of the originator of this PR.
if [ -n "$NAME" ]; then
  ORIGINATOR="$NAME"
elif [ -f $HOME/.fullname ]; then
  ORIGINATOR="`sed -e '1q' $HOME/.fullname`"
else
  PTEMP=`mktemp -t p` || exit 1
  # Must use temp file due to incompatibilities in quoting behavior
  # and to protect shell metacharacters in the expansion of $LOGNAME
  $PW usershow $LOGNAME | awk -F: '{ print $8 }' | sed -e 's/,.*//' > $PTEMP
  ORIGINATOR="`cat $PTEMP`"
  rm -f $PTEMP
fi

FROM="$ORIGINATOR <$LOGNAME>"
REPLY_TO="$ORIGINATOR <${REPLY_TO:-${REPLYTO:-$LOGNAME}}>"

if [ -n "$ORGANIZATION" ]; then
  if [ -f "$ORGANIZATION" ]; then
    ORGANIZATION="`cat $ORGANIZATION`"
  fi
else
  if [ -n "$DEFAULT_ORGANIZATION" ]; then
    ORGANIZATION="$DEFAULT_ORGANIZATION"
  elif [ -f $HOME/.organization ]; then
    ORGANIZATION="`cat $HOME/.organization`"
  fi
fi

# If they don't have a preferred editor set, then use
if [ -z "$VISUAL" ]; then
  if [ -z "$EDITOR" ]; then
    EDIT=vi
  else
    EDIT="$EDITOR"
  fi
else
  EDIT="$VISUAL"
fi

# Find out some information.
SYSTEM=`( [ -f /bin/uname ] && /bin/uname -a ) || \
        ( [ -f /usr/bin/uname ] && /usr/bin/uname -a ) || echo ""`
ARCH=`[ -f /bin/arch ] && /bin/arch`
MACHINE=`[ -f /bin/machine ] && /bin/machine`

COMMAND=`echo $0 | sed -e 's,.*/,,'`
USAGE="Usage: $COMMAND [-PVL] [-t address] [-f filename] [-s severity]
       [-c address] [-a file] [--version]"
REMOVE=
BATCH=
CC=
SEVERITY_C=

while [ $# -gt 0 ]; do
  case "$1" in
    -r) ;; 		# Ignore for backward compat.
    -t | --to) if [ $# -eq 1 ]; then echo "$USAGE"; exit 1; fi
	shift ; GNATS_ADDR="$1"
	EXPLICIT_GNATS_ADDR=true
        ;;
    -f | --file) if [ $# -eq 1 ]; then echo "$USAGE"; exit 1; fi
	shift ; IN_FILE="$1"
	if [ "$IN_FILE" != "-" -a ! -r "$IN_FILE" ]; then
	  echo "$COMMAND: cannot read $IN_FILE"
	  exit 1
	fi
	;;
    -b | --batch) BATCH=true ;;
    -c | --cc) if [ $# -eq 1 ]; then echo "$USAGE"; exit 1; fi
	shift ; CC="$1"
	;;
    -s | --severity) if [ $# -eq 1 ]; then echo "$USAGE"; exit 1; fi
	shift ; SEVERITY_C="$1"
	;;
    -p | -P | --print) PRINT=true ;;
    -L | --list) FORMAT=norm ;;
    -l | -CL | --lisp) FORMAT=lisp ;;
    -h | --help) echo "$USAGE"; exit 0 ;;
    -V | --version) echo "$VERSION"; exit 0 ;;
    -a | --attach) if [ -z "$2" ]; then
	  echo "$USAGE" ; exit 1; 
	fi
	if [ -e "$2" -a ! -d "$2" ]; then
	  PRETTY_NAME=`basename $2`
	  if file $2 | grep "text" >/dev/null 2>/dev/null ; then
	    ATTACHED_FILES="$ATTACHED_FILES
--- $PRETTY_NAME begins here ---
`cat \"$2\"`
--- $PRETTY_NAME ends here ---
"
	  else
	    ATTACHED_FILES="$ATTACHED_FILES
`uuencode \"$PRETTY_NAME\" < \"$2\"`
"
	  fi
	  shift;
	fi;
	;;
    -*) echo "$USAGE" ; exit 1 ;;
    *) if [ -z "$USER_GNATS_SITE" ]; then
	 if [ ! -r "$DATADIR/gnats/$1" ]; then
	   echo "$COMMAND: the GNATS site $1 does not have a categories list."
	   exit 1
	 else
	   # The site name is the alias they'll have to have created.
	   USER_GNATS_SITE=$1
	 fi
       else
	 echo "$USAGE" ; exit 1
       fi
       ;;
 esac
 shift
done

if [ -n "$USER_GNATS_SITE" ] && [ "$USER_GNATS_SITE" != "$GNATS_SITE" ]; then
  GNATS_SITE=$USER_GNATS_SITE
  GNATS_ADDR=$USER_GNATS_SITE-gnats
fi

if [ "$SUBMITTER" = "unknown" -a -z "$IN_FILE" ]; then
  SUBMITTER="current-users"
fi

if [ -r "$DATADIR/gnats/$GNATS_SITE" ]; then
  CATEGORIES=`grep -v '^#' $DATADIR/gnats/$GNATS_SITE | sort`
else
  echo "$COMMAND: could not read $DATADIR/gnats/$GNATS_SITE for categories list."
  exit 1
fi

if [ -z "$CATEGORIES" ]; then
  echo "$COMMAND: the categories list for $GNATS_SITE was empty!"
  exit 1
fi

case "$FORMAT" in
  lisp) echo "$CATEGORIES" | \
        awk 'BEGIN {printf "( "} {printf "(\"%s\") ",$0} END {printf ")\n"}'
        exit 0
        ;;
  norm) l=`echo "$CATEGORIES" | \
	awk 'BEGIN {max = 0; } { if (length($0) > max) { max = length($0); } }
	     END {print max + 1;}'`
	c=`expr 70 / $l`
	if [ $c -eq 0 ]; then c=1; fi
	echo "$CATEGORIES" | \
        awk 'BEGIN {print "Known categories:"; i = 0 }
          { printf ("%-'$l'.'$l's", $0); if ((++i % '$c') == 0) { print "" } }
            END { print ""; }'
        exit 0
        ;;
esac

ORIGINATOR_C='<name of the PR author (one line)>'
ORGANIZATION_C='<organization of PR author (multiple lines)>'
CONFIDENTIAL_C='no <FreeBSD PRs are public data>'
SYNOPSIS_C='<synopsis of the problem (one line)>'
if [ -z "$SEVERITY_C" ]; then
  SEVERITY_C='<[ non-critical | serious | critical ] (one line)>'
fi
PRIORITY_C='<[ low | medium | high ] (one line)>'
CATEGORY_C='<choose from the list of categories above (one line)>'
CLASS_C='<[ sw-bug | doc-bug | change-request | update | maintainer-update ] (one line)>'
RELEASE_C='<release number or tag (one line)>'
ENVIRONMENT_C='<machine, os, target, libraries (multiple lines)>'
DESCRIPTION_C='<precise description of the problem (multiple lines)>'
HOW_TO_REPEAT_C='<code/input/activities to reproduce the problem (multiple lines)>'
FIX_C='<how to correct or work around the problem, if known (multiple lines)>'

# Create temporary files, safely
REF=`mktemp -t pf` || exit 1
TEMP=`mktemp -t pf` || exit 1
# Catch some signals. ($xs kludge needed by Sun /bin/sh)
xs=0
trap 'rm -f $REF $TEMP; exit $xs' 0
trap 'SAV=`mktemp -t pr`;echo "$COMMAND: Aborting ... saving unfinished PR into $SAV"; rm -f $REF ; mv $TEMP $SAV; xs=1; exit' 1 2 3 13 15

# If they told us to use a specific file, then do so.
if [ -n "$IN_FILE" ]; then
  if [ "$IN_FILE" = "-" ]; then
    # The PR is coming from the standard input.
    if [ -n "$EXPLICIT_GNATS_ADDR" ]; then
      sed -e "s;^[Tt][Oo]:.*;To: $GNATS_ADDR;" > $TEMP
    else
      cat > $TEMP
    fi
  else
    # Use the file they named.
    if [ -n "$EXPLICIT_GNATS_ADDR" ]; then
      sed -e "s;^[Tt][Oo]:.*;To: $GNATS_ADDR;" $IN_FILE > $TEMP
    else
      cat $IN_FILE > $TEMP
    fi
  fi
else

  if [ -n "$PR_FORM" -a -z "$PRINT_INTERN" ]; then
    # If their PR_FORM points to a bogus entry, then bail.
    if [ ! -f "$PR_FORM" -o ! -r "$PR_FORM" -o ! -s "$PR_FORM" ]; then
      echo "$COMMAND: can't seem to read your template file (\`$PR_FORM'), ignoring PR_FORM"
      sleep 1
      PRINT_INTERN=bad_prform
    fi
  fi

  if [ -n "$PR_FORM" -a -z "$PRINT_INTERN" ]; then
    cp $PR_FORM $TEMP || 
      ( echo "$COMMAND: could not copy $PR_FORM" ; xs=1; exit )
    [ -n "$ATTACHED_FILES" ] && echo "$ATTACHED_FILES" >> $TEMP
  else
    for file in $TEMP $REF ; do
      cat  > $file << '__EOF__'
SEND-PR: -*- send-pr -*-
SEND-PR: vim: syntax=sendpr
SEND-PR:
SEND-PR: Lines starting with `SEND-PR' will be removed automatically, as
SEND-PR: will all comments (text enclosed in `<' and `>').
SEND-PR:
SEND-PR: Please consult the following URL if you are not sure how to
SEND-PR: fill out a problem report:
SEND-PR: http://www.freebsd.org/doc/en/articles/problem-reports/
SEND-PR:
SEND-PR: Note that the Synopsis field is mandatory.
SEND-PR:
SEND-PR: Please note that (unless you state otherwise) if your report 
SEND-PR: includes a patch then it will be taken under the same license as 
SEND-PR: the one on the file(s) you want to change.
SEND-PR:
SEND-PR: BE ADVISED THAT FREEBSD PROBLEM REPORTS ARE PUBLIC INFORMATION AND
SEND-PR: WILL BE PUBLISHED AS-IS ON THE PROJECT'S MAILING LISTS AND WEB SITES.
SEND-PR: DO NOT SUBMIT ANY INFORMATION YOU DO NOT WANT MADE PUBLIC.
SEND-PR:
SEND-PR: If you wish to submit a problem report confidentially, then contact
SEND-PR: the FreeBSD bugmaster (bugmaster@FreeBSD.org) to arrange for a
SEND-PR: relevant developer to be contacted.
SEND-PR:
SEND-PR: For sensitive security issues, consider contacting the FreeBSD
SEND-PR: security officer team (security-officer@freebsd.org) directly.
SEND-PR:
SEND-PR: Choose from the following categories:
SEND-PR:
__EOF__

      # Format the categories so they fit onto lines.
	l=`echo "$CATEGORIES" | \
	awk 'BEGIN {max = 0; } { if (length($0) > max) { max = length($0); } }
	     END {print max + 1;}'`
	c=`expr 61 / $l`
	if [ $c -eq 0 ]; then c=1; fi
	echo "$CATEGORIES" | \
        awk 'BEGIN {printf "SEND-PR: "; i = 0 }
          { printf ("%-'$l'.'$l's", $0);
	    if ((++i % '$c') == 0) { printf "\nSEND-PR: " } }
            END { printf "\nSEND-PR:\n"; }' >> $file

      cat >> $file << __EOF__
To: $GNATS_ADDR
From: $FROM
Reply-To: $REPLY_TO
Cc: $CC
X-send-pr-version: $VERSION
X-GNATS-Notify: 


>Submitter-Id:	$SUBMITTER
>Originator:	$ORIGINATOR
>Organization:	${ORGANIZATION-$ORGANIZATION_C}
>Confidential:	$CONFIDENTIAL_C
>Synopsis:	$SYNOPSIS_C
>Severity:	$SEVERITY_C
>Priority:	$PRIORITY_C
>Category:	$CATEGORY_C
>Class:		$CLASS_C
>Release:	${DEFAULT_RELEASE-$RELEASE_C}
>Environment:
`[ -n "$SYSTEM" ] && echo System: $SYSTEM`
`[ -n "$ARCH" ] && echo Architecture: $ARCH`
`[ -n "$MACHINE" ] && echo Machine: $MACHINE`
	$ENVIRONMENT_C
>Description:
	$DESCRIPTION_C
>How-To-Repeat:
	$HOW_TO_REPEAT_C
>Fix:

	$FIX_C
$ATTACHED_FILES

__EOF__

    done
  fi

  if [ "$PRINT" = true -o "$PRINT_INTERN" = true ]; then
    cat $TEMP
    xs=0; exit
  fi

  chmod u+w $TEMP
  eval $EDIT $TEMP

  if cmp -s $REF $TEMP ; then
    echo "$COMMAND: problem report not filled out, therefore not sent"
    xs=1; exit
  fi
fi

#
#	Check the enumeration fields

# This is a "sed-subroutine" with one keyword parameter 
# (with workaround for Sun sed bug)
#
SED_CMD='{
s|||
s|<.*>||
s|^[ 	]*||
s|[ 	]*$||
p
q
}'


while true; do
  CNT=0

  # 1) Confidential
  #
  PATTERN=">Confidential:"
  CONFIDENTIAL=`eval sed -n -e "\"/$PATTERN/$SED_CMD\"" $TEMP`
  case "$CONFIDENTIAL" in
    ""|no) CNT=`expr $CNT + 1` ;;
    *) echo "$COMMAND: \`$CONFIDENTIAL' is not a valid value for \`Confidential'." ;;
  esac
  #
  # 2) Severity
  #
  PATTERN=">Severity:"
  SEVERITY=`eval sed -n -e "\"/$PATTERN/$SED_CMD\"" $TEMP`
  case "$SEVERITY" in
    ""|non-critical|serious|critical) CNT=`expr $CNT + 1` ;;
    *)  echo "$COMMAND: \`$SEVERITY' is not a valid value for \`Severity'."
  esac
  #
  # 3) Priority
  #
  PATTERN=">Priority:"
  PRIORITY=`eval sed -n -e "\"/$PATTERN/$SED_CMD\"" $TEMP`
  case "$PRIORITY" in
    ""|low|medium|high) CNT=`expr $CNT + 1` ;;
    *)  echo "$COMMAND: \`$PRIORITY' is not a valid value for \`Priority'."
  esac
  #
  # 4) Category
  #
  PATTERN=">Category:"
  CATEGORY=`eval sed -n -e "\"/$PATTERN/$SED_CMD\"" $TEMP`
  FOUND=
  for C in $CATEGORIES
  do
    if [ "$C" = "$CATEGORY" ]; then FOUND=true ; break ; fi
  done
  if [ -n "$FOUND" ]; then
    CNT=`expr $CNT + 1`	
  else
    if [ -z "$CATEGORY" ]; then
      echo "$COMMAND: you must include a Category: field in your report."
    else
      echo "$COMMAND: \`$CATEGORY' is not a known category."
    fi
  fi
  #
  # 5) Class
  #
  PATTERN=">Class:"
  CLASS=`eval sed -n -e "\"/$PATTERN/$SED_CMD\"" $TEMP`
  case "$CLASS" in
    ""|sw-bug|doc-bug|change-request|update|maintainer-update) CNT=`expr $CNT + 1` ;;
    *)  echo "$COMMAND: \`$CLASS' is not a valid value for \`Class'."
  esac
  #
  # 6) Check that synopsis is not empty
  #
  if grep "^>Synopsis:[ 	]*${SYNOPSIS_C}\$" $TEMP > /dev/null
  then
    echo "$COMMAND: Synopsis must not be empty."
  else
    CNT=`expr $CNT + 1`
  fi

  [ $CNT -lt 6 -a -z "$BATCH" ] && 
    echo "Errors were found with the problem report."

  while true; do
    if [ -z "$BATCH" ]; then
      $ECHON1 "s)end, e)dit or a)bort? $ECHON2"
      read input
    else
      if [ $CNT -eq 6 ]; then
        input=s
      else
        input=a
      fi
    fi
    case "$input" in
      a*)
	if [ -z "$BATCH" ]; then
	  BAD=`mktemp -t pbad`
	  echo "$COMMAND: the problem report remains in $BAD and is not sent."
	  mv $TEMP $BAD
        else
	  echo "$COMMAND: the problem report is not sent."
	fi
	xs=1; exit
	;;
      e*)
        eval $EDIT $TEMP
	continue 2
	;;
      s*)
        if [ $CNT -lt 6 ]; then
          if [ -z "$BATCH" ]; then
            echo "But there are still errors in the problem report!"
            continue 2
          else
            echo "Errors found in PR"
            exit 1
          fi
        fi
	break 2
	;;
    esac
  done
done

#
# Remove the subject field if one is already there.  There's no reason
# for it to be any different than the synopsis.
#
if grep '^Subject:' $TEMP > /dev/null
then
  ed -s $TEMP << __EOF__
/^Subject:/d
w
q
__EOF__
fi

#
# Add the subject field with the value of $SYNOPSIS.  We use the To:
# field as an anchor, which had better be there.
#
SYNOPSIS=`grep '^>Synopsis:' $TEMP | sed -e 's/^>Synopsis:[ 	]*//' |
    sed -e "s;$SYNOPSIS_C;;"`
ed -s $TEMP << __EOF__
/^To:/a
Subject: $SYNOPSIS
.
w
q
__EOF__

#
#	Remove comments and send the problem report
#	(we have to use patterns, where the comment contains regex chars)
#
# /^>Originator:/s;$ORIGINATOR;;
sed  -e "
/^SEND-PR:/d
/^>Organization:/,/^>[A-Za-z-]*:/s;$ORGANIZATION_C;;
/^>Confidential:/s;<.*>;;
/^>Synopsis:/s;$SYNOPSIS_C;;
/^>Severity:/s;<.*>;;
/^>Priority:/s;<.*>;;
/^>Category:/s;$CATEGORY_C;;
/^>Class:/s;<.*>;;
/^>Release:/,/^>[A-Za-z-]*:/s;$RELEASE_C;;
/^>Environment:/,/^>[A-Za-z-]*:/s;$ENVIRONMENT_C;;
/^>Description:/,/^>[A-Za-z-]*:/s;$DESCRIPTION_C;;
/^>How-To-Repeat:/,/^>[A-Za-z-]*:/s;$HOW_TO_REPEAT_C;;
/^>Fix:/,/^>[A-Za-z-]*:/s;$FIX_C;;
" $TEMP > $REF

if $MAIL_AGENT < $REF; then
  echo "$COMMAND: problem report sent"
  xs=0; exit
else
  echo "$COMMAND: mysterious mail failure."
  if [ -z "$BATCH" ]; then
    BAD=`mktemp -t pbad`
    echo "$COMMAND: the problem report remains in $BAD and is not sent."
    mv $TEMP $BAD
  else
    echo "$COMMAND: the problem report is not sent."
  fi
  xs=1; exit
fi
