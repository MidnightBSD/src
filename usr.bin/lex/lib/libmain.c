/* libmain - flex run-time support library "main" function */

/* $Header: /home/cvs/src/usr.bin/lex/lib/libmain.c,v 1.1.1.2 2006-02-25 02:38:14 laffer1 Exp $
 * $FreeBSD: src/usr.bin/lex/lib/libmain.c,v 1.3 1999/10/27 07:56:49 obrien Exp $ */

extern int yylex();

int main( argc, argv )
int argc;
char *argv[];
	{
	while ( yylex() != 0 )
		;

	return 0;
	}
