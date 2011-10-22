#!/bin/sh
# cflow(1) - a flow graph creator for C, Lex, Yacc and Assembler sources.

# Let the environment override the CC and CPP variables.
: ${CC:="cc"}
: ${CPP:="$CC -E"}

progprefix=/usr/bin
program=""
graphfile=""
usecpp=0
asparams=""
cgparams=""
cppparams=""
programset=""
fileargs=""
PROGNAME=`basename $0`
# any
usage()
{ 
    echo "usage: $PROGNAME [-aAcCGgnpPr] [-d n] [-D name[=value]] [-i x|_] [-U name]"
    echo "                [-I directory] [-R root] file ... "
}

# Check the arguments.
while getopts AcCd:D:gGi:I:hpPrR:U: arg; do
    case $arg in
        a)
            asparams="$asparams -a"
            ;;
        A)
            cgparams="$cgparams -A"
            ;;
        c)
            params="$params -c"
            ;;
        C)
            cgparams="$cgparams -C"
            ;;
        d)
            params="$params -d $OPTARG"
            ;;
        D)
            cppparams="$cppparams -D $OPTARG"
            usecpp=1
            ;;
        G)
            params="$cgparams -G"
            ;;
        g)
            params="$params -g"
            ;;
        i)
            params="$params -i $OPTARG"
            ;;
        I)
            cppparams="$cppparams -I $OPTARG"
            usecpp=1
            ;;
        n)
            asparams="$asparams -n"
            ;;
        p)
            usecpp=1
            ;;
        P)
            params="$cgparams -P"
            ;;
        r)
            params="$params -r"
            ;;
        R)
            params="$params -R $OPTARG"
            ;;
        U)
            cppparams="$cppparams -U $OPTARG"
            usecpp=1
            ;;
        \? | h)
            usage
            exit 2
    esac
done

# Forward the argument list.
shift $(expr $OPTIND - 1)
if [ $# -eq 0 ]; then
    usage
    exit 1
fi

# Check for the file and invoke the correct graph generator.
for f in $@; do
    case $f in
        *.c|*.cc|*.C)
            if [ -n "$programset" -a "$programset" != "c" ]; then
                echo "Can not parse different types of files"
                exit 2
            fi
            program="$progprefix/cgraph"
            programset="c"
            graphfile="$graphfile $f"
            params="$cgparams $params"
            ;;
        *.i)
            if [ -n "$programset" -a "$programset" != "c" ]; then
                echo "Can not parse different types of files"
                exit 2
            fi
            program="$progprefix/cgraph"
            graphfile="$graphfile $f"
            # We do not need to preprocess the file.
            usecpp=0
            params="$cgparams $params"
            ;;
        *.s|*.S)
            if [ -n "$programset" -a "$programset" != "asm" ]; then
                echo "Can not parse different types of files"
                exit 2
            fi
            program="$progprefix/asmgraph"
            programset="asm"
            graphfile="graphfile $f"
            if [ "$asparams" = "" ]; then
                asparams=" -n" # Implicitly use NASM syntax on demand.
            fi
            params="$asparams $params"
            ;;
#         *.l)
#             if [ $programset != "lex" ]; then
#                 echo "Can not parse different types of files"
#                 exit 2
#             fi
#             program="$progprefix/lexgraph"
#             programset="lex"
#             graphfile="$graphfile $f"
#             ;;
#         *.y)
#             if [ $programset != "yacc" ]; then
#                 echo "Can not parse different types of files"
#                 exit 2
#             fi
#             program="$progprefix/yaccgraph"
#             programset="yacc"
#             graphfile="$graphfile $f"
#             ;;
        *)
            usage
            exit 2
            ;;
    esac
done

# Do we want C preprocessing?
if [ "$program" = "$progprefix/cgraph" ]; then
    if [ $usecpp -eq 1 ]; then
        # Create a temporary file for the preprocessor.
        tmpfile=`mktemp -t $PROGNAME` || exit 2
        
        $CPP $cppparams $graphfile > $tmpfile
        exec $program $params $tmpfile || {
            rm $tmpfile
            exit 2
        }

        # Clean up
        rm $tmpfile
        exit 0
    fi
fi

exec $program $params $graphfile || exit 2
