# $Id: roken.awk,v 1.9 2003/03/04 10:37:26 lha Exp $

BEGIN {
	print "#ifdef HAVE_CONFIG_H"
	print "#include <config.h>"
	print "#endif"
	print "#include <stdio.h>"
	print ""
	print "int main(int argc, char **argv)"
	print "{"
	    print "puts(\"/* This is an OS dependent, generated file */\");"
	print "puts(\"\\n\");"
	print "puts(\"#ifndef __ROKEN_H__\");"
	print "puts(\"#define __ROKEN_H__\");"
	print "puts(\"\");"
}

$1 == "\#ifdef" || $1 == "\#ifndef" || $1 == "\#if" || $1 == "\#else" || $1 == "\#elif" || $1 == "\#endif" || $1 == "#ifdef" || $1 == "#ifndef" || $1 == "#if" || $1 == "#else" || $1 == "#elif" || $1 == "#endif" {
	print $0;
	next
}

{
	s = ""
	for(i = 1; i <= length; i++){
		x = substr($0, i, 1)
		if(x == "\"" || x == "\\")
			s = s "\\";
		s = s x;
	}
	print "puts(\"" s "\");"
}

END {
	print "puts(\"#define ROKEN_VERSION \" VERSION );"
	print "puts(\"\");"
	print "puts(\"#endif /* __ROKEN_H__ */\");"
	print "return 0;"
	print "}"
}
