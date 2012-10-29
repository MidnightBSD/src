/* libmain - flex run-time support library "main" function */

/* $Header: /home/cvs/src/usr.bin/lex/lib/libmain.c,v 1.2 2012-10-29 20:54:16 laffer1 Exp $
 * $MidnightBSD$ */

extern int yylex();

int main( argc, argv )
int argc;
char *argv[];
	{
	while ( yylex() != 0 )
		;

	return 0;
	}
