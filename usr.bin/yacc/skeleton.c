/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Paul Corbett.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if 0
#ifndef lint
static char sccsid[] = "@(#)skeleton.c	5.8 (Berkeley) 4/29/95";
#endif
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "defs.h"

/*  The definition of yysccsid in the banner should be replaced with	*/
/*  a #pragma ident directive if the target C compiler supports		*/
/*  #pragma ident directives.						*/
/*									*/
/*  If the skeleton is changed, the banner should be changed so that	*/
/*  the altered version can be easily distinguished from the original.	*/
/*									*/
/*  The #defines included with the banner are there because they are	*/
/*  useful in subsequent code.  The macros #defined in the header or	*/
/*  the body either are not useful outside of semantic actions or	*/
/*  are conditional.							*/

const char *banner[] =
{
    "#include <stdlib.h>",
    "#include <string.h>",
    "#ifndef lint",
    "#ifdef __unused",
    "__unused",
    "#endif",
    "static char const ",
    "yyrcsid[] = \"$FreeBSD$\";",
    "#endif",
    "#define YYBYACC 1",
    "#define YYMAJOR 1",
    "#define YYMINOR 9",
    "#define YYLEX yylex()",
    "#define YYEMPTY -1",
    "#define yyclearin (yychar=(YYEMPTY))",
    "#define yyerrok (yyerrflag=0)",
    "#define YYRECOVERING() (yyerrflag!=0)",
    "#if defined(__cplusplus) || __STDC__",
    "static int yygrowstack(void);",
    "#else",
    "static int yygrowstack();",
    "#endif",
    0
};


const char *tables[] =
{
    "extern const short yylhs[];",
    "extern const short yylen[];",
    "extern const short yydefred[];",
    "extern const short yydgoto[];",
    "extern const short yysindex[];",
    "extern const short yyrindex[];",
    "extern const short yygindex[];",
    "extern const short yytable[];",
    "extern const short yycheck[];",
    "#if YYDEBUG",
    "extern char *yyname[];",
    "extern char *yyrule[];",
    "#endif",
    0
};


const char *header[] =
{
    "#if YYDEBUG",
    "#include <stdio.h>",
    "#endif",
    "#ifdef YYSTACKSIZE",
    "#undef YYMAXDEPTH",
    "#define YYMAXDEPTH YYSTACKSIZE",
    "#else",
    "#ifdef YYMAXDEPTH",
    "#define YYSTACKSIZE YYMAXDEPTH",
    "#else",
    "#define YYSTACKSIZE 10000",
    "#define YYMAXDEPTH 10000",
    "#endif",
    "#endif",
    "#define YYINITSTACKSIZE 200",
    "int yydebug;",
    "int yynerrs;",
    "int yyerrflag;",
    "int yychar;",
    "short *yyssp;",
    "YYSTYPE *yyvsp;",
    "YYSTYPE yyval;",
    "YYSTYPE yylval;",
    "short *yyss;",
    "short *yysslim;",
    "YYSTYPE *yyvs;",
    "int yystacksize;",
    0
};


const char *body[] =
{
    "/* allocate initial stack or double stack size, up to YYMAXDEPTH */",
    "#if defined(__cplusplus) || __STDC__",
    "static int yygrowstack(void)",
    "#else",
    "static int yygrowstack()",
    "#endif",
    "{",
    "    int newsize, i;",
    "    short *newss;",
    "    YYSTYPE *newvs;",
    "",
    "    if ((newsize = yystacksize) == 0)",
    "        newsize = YYINITSTACKSIZE;",
    "    else if (newsize >= YYMAXDEPTH)",
    "        return -1;",
    "    else if ((newsize *= 2) > YYMAXDEPTH)",
    "        newsize = YYMAXDEPTH;",
    "    i = yyssp - yyss;",
    "    newss = yyss ? (short *)realloc(yyss, newsize * sizeof *newss) :",
    "      (short *)malloc(newsize * sizeof *newss);",
    "    if (newss == NULL)",
    "        return -1;",
    "    yyss = newss;",
    "    yyssp = newss + i;",
    "    newvs = yyvs ? (YYSTYPE *)realloc(yyvs, newsize * sizeof *newvs) :",
    "      (YYSTYPE *)malloc(newsize * sizeof *newvs);",
    "    if (newvs == NULL)",
    "        return -1;",
    "    yyvs = newvs;",
    "    yyvsp = newvs + i;",
    "    yystacksize = newsize;",
    "    yysslim = yyss + newsize - 1;",
    "    return 0;",
    "}",
    "",
    "#define YYABORT goto yyabort",
    "#define YYREJECT goto yyabort",
    "#define YYACCEPT goto yyaccept",
    "#define YYERROR goto yyerrlab",
    "",
    "#ifndef YYPARSE_PARAM",
    "#if defined(__cplusplus) || __STDC__",
    "#define YYPARSE_PARAM_ARG void",
    "#define YYPARSE_PARAM_DECL",
    "#else	/* ! ANSI-C/C++ */",
    "#define YYPARSE_PARAM_ARG",
    "#define YYPARSE_PARAM_DECL",
    "#endif	/* ANSI-C/C++ */",
    "#else	/* YYPARSE_PARAM */",
    "#ifndef YYPARSE_PARAM_TYPE",
    "#define YYPARSE_PARAM_TYPE void *",
    "#endif",
    "#if defined(__cplusplus) || __STDC__",
    "#define YYPARSE_PARAM_ARG YYPARSE_PARAM_TYPE YYPARSE_PARAM",
    "#define YYPARSE_PARAM_DECL",
    "#else	/* ! ANSI-C/C++ */",
    "#define YYPARSE_PARAM_ARG YYPARSE_PARAM",
    "#define YYPARSE_PARAM_DECL YYPARSE_PARAM_TYPE YYPARSE_PARAM;",
    "#endif	/* ANSI-C/C++ */",
    "#endif	/* ! YYPARSE_PARAM */",
    "",
    "int",
    "yyparse (YYPARSE_PARAM_ARG)",
    "    YYPARSE_PARAM_DECL",
    "{",
    "    int yym, yyn, yystate;",
    "#if YYDEBUG",
    "    const char *yys;",
    "",
    "    if ((yys = getenv(\"YYDEBUG\")))",
    "    {",
    "        yyn = *yys;",
    "        if (yyn >= '0' && yyn <= '9')",
    "            yydebug = yyn - '0';",
    "    }",
    "#endif",
    "",
    "    yynerrs = 0;",
    "    yyerrflag = 0;",
    "    yychar = (-1);",
    "",
    "    if (yyss == NULL && yygrowstack()) goto yyoverflow;",
    "    yyssp = yyss;",
    "    yyvsp = yyvs;",
    "    *yyssp = yystate = 0;",
    "",
    "yyloop:",
    "    if ((yyn = yydefred[yystate])) goto yyreduce;",
    "    if (yychar < 0)",
    "    {",
    "        if ((yychar = yylex()) < 0) yychar = 0;",
    "#if YYDEBUG",
    "        if (yydebug)",
    "        {",
    "            yys = 0;",
    "            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];",
    "            if (!yys) yys = \"illegal-symbol\";",
    "            printf(\"%sdebug: state %d, reading %d (%s)\\n\",",
    "                    YYPREFIX, yystate, yychar, yys);",
    "        }",
    "#endif",
    "    }",
    "    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&",
    "            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)",
    "    {",
    "#if YYDEBUG",
    "        if (yydebug)",
    "            printf(\"%sdebug: state %d, shifting to state %d\\n\",",
    "                    YYPREFIX, yystate, yytable[yyn]);",
    "#endif",
    "        if (yyssp >= yysslim && yygrowstack())",
    "        {",
    "            goto yyoverflow;",
    "        }",
    "        *++yyssp = yystate = yytable[yyn];",
    "        *++yyvsp = yylval;",
    "        yychar = (-1);",
    "        if (yyerrflag > 0)  --yyerrflag;",
    "        goto yyloop;",
    "    }",
    "    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&",
    "            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)",
    "    {",
    "        yyn = yytable[yyn];",
    "        goto yyreduce;",
    "    }",
    "    if (yyerrflag) goto yyinrecovery;",
    "#if defined(lint) || defined(__GNUC__)",
    "    goto yynewerror;",
    "#endif",
    "yynewerror:",
    "    yyerror(\"syntax error\");",
    "#if defined(lint) || defined(__GNUC__)",
    "    goto yyerrlab;",
    "#endif",
    "yyerrlab:",
    "    ++yynerrs;",
    "yyinrecovery:",
    "    if (yyerrflag < 3)",
    "    {",
    "        yyerrflag = 3;",
    "        for (;;)",
    "        {",
    "            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&",
    "                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)",
    "            {",
    "#if YYDEBUG",
    "                if (yydebug)",
    "                    printf(\"%sdebug: state %d, error recovery shifting\\",
    " to state %d\\n\", YYPREFIX, *yyssp, yytable[yyn]);",
    "#endif",
    "                if (yyssp >= yysslim && yygrowstack())",
    "                {",
    "                    goto yyoverflow;",
    "                }",
    "                *++yyssp = yystate = yytable[yyn];",
    "                *++yyvsp = yylval;",
    "                goto yyloop;",
    "            }",
    "            else",
    "            {",
    "#if YYDEBUG",
    "                if (yydebug)",
    "                    printf(\"%sdebug: error recovery discarding state %d\
\\n\",",
    "                            YYPREFIX, *yyssp);",
    "#endif",
    "                if (yyssp <= yyss) goto yyabort;",
    "                --yyssp;",
    "                --yyvsp;",
    "            }",
    "        }",
    "    }",
    "    else",
    "    {",
    "        if (yychar == 0) goto yyabort;",
    "#if YYDEBUG",
    "        if (yydebug)",
    "        {",
    "            yys = 0;",
    "            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];",
    "            if (!yys) yys = \"illegal-symbol\";",
    "            printf(\"%sdebug: state %d, error recovery discards token %d\
 (%s)\\n\",",
    "                    YYPREFIX, yystate, yychar, yys);",
    "        }",
    "#endif",
    "        yychar = (-1);",
    "        goto yyloop;",
    "    }",
    "yyreduce:",
    "#if YYDEBUG",
    "    if (yydebug)",
    "        printf(\"%sdebug: state %d, reducing by rule %d (%s)\\n\",",
    "                YYPREFIX, yystate, yyn, yyrule[yyn]);",
    "#endif",
    "    yym = yylen[yyn];",
    "    if (yym)",
    "        yyval = yyvsp[1-yym];",
    "    else",
    "        memset(&yyval, 0, sizeof yyval);",
    "    switch (yyn)",
    "    {",
    0
};


const char *trailer[] =
{
    "    }",
    "    yyssp -= yym;",
    "    yystate = *yyssp;",
    "    yyvsp -= yym;",
    "    yym = yylhs[yyn];",
    "    if (yystate == 0 && yym == 0)",
    "    {",
    "#if YYDEBUG",
    "        if (yydebug)",
    "            printf(\"%sdebug: after reduction, shifting from state 0 to\\",
    " state %d\\n\", YYPREFIX, YYFINAL);",
    "#endif",
    "        yystate = YYFINAL;",
    "        *++yyssp = YYFINAL;",
    "        *++yyvsp = yyval;",
    "        if (yychar < 0)",
    "        {",
    "            if ((yychar = yylex()) < 0) yychar = 0;",
    "#if YYDEBUG",
    "            if (yydebug)",
    "            {",
    "                yys = 0;",
    "                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];",
    "                if (!yys) yys = \"illegal-symbol\";",
    "                printf(\"%sdebug: state %d, reading %d (%s)\\n\",",
    "                        YYPREFIX, YYFINAL, yychar, yys);",
    "            }",
    "#endif",
    "        }",
    "        if (yychar == 0) goto yyaccept;",
    "        goto yyloop;",
    "    }",
    "    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&",
    "            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)",
    "        yystate = yytable[yyn];",
    "    else",
    "        yystate = yydgoto[yym];",
    "#if YYDEBUG",
    "    if (yydebug)",
    "        printf(\"%sdebug: after reduction, shifting from state %d \\",
    "to state %d\\n\", YYPREFIX, *yyssp, yystate);",
    "#endif",
    "    if (yyssp >= yysslim && yygrowstack())",
    "    {",
    "        goto yyoverflow;",
    "    }",
    "    *++yyssp = yystate;",
    "    *++yyvsp = yyval;",
    "    goto yyloop;",
    "yyoverflow:",
    "    yyerror(\"yacc stack overflow\");",
    "yyabort:",
    "    return (1);",
    "yyaccept:",
    "    return (0);",
    "}",
    0
};


void
write_section(const char *section[])
{
    int c;
    int i;
    const char *s;
    FILE *f;

    f = code_file;
    for (i = 0; (s = section[i]); ++i)
    {
	++outline;
	while ((c = *s))
	{
	    putc(c, f);
	    ++s;
	}
	putc('\n', f);
    }
}
