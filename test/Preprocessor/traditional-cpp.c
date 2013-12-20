/* Clang supports a very limited subset of -traditional-cpp, basically we only
 * intend to add support for things that people actually rely on when doing
 * things like using /usr/bin/cpp to preprocess non-source files. */

/*
 RUN: %clang_cc1 -traditional-cpp %s -E -o %t
 RUN: FileCheck -strict-whitespace < %t %s
 RUN: %clang_cc1 -traditional-cpp %s -E -C | FileCheck -check-prefix=CHECK-COMMENTS %s
*/

/* -traditional-cpp should eliminate all C89 comments. */
/* CHECK-NOT: /*
 * CHECK-COMMENTS: {{^}}/* -traditional-cpp should eliminate all C89 comments. *{{/$}}
 */

/* CHECK: {{^}}foo // bar{{$}}
 */
foo // bar


/* The lines in this file contain hard tab characters and trailing whitespace; 
 * do not change them! */

/* CHECK: {{^}}	indented!{{$}}
 * CHECK: {{^}}tab	separated	values{{$}}
 */
	indented!
tab	separated	values

#define bracket(x) >>>x<<<
bracket(|  spaces  |)
/* CHECK: {{^}}>>>|  spaces  |<<<{{$}}
 */

/* This is still a preprocessing directive. */
# define foo bar
foo!
-
	foo!	foo!	
/* CHECK: {{^}}bar!{{$}}
 * CHECK: {{^}}	bar!	bar!	{{$}}
 */

/* Deliberately check a leading newline with spaces on that line. */
   
# define foo bar
foo!
-
	foo!	foo!	
/* CHECK: {{^}}bar!{{$}}
 * CHECK: {{^}}	bar!	bar!	{{$}}
 */

/* FIXME: -traditional-cpp should not consider this a preprocessing directive
 * because the # isn't in the first column.
 */
 #define foo2 bar
foo2!
/* If this were working, both of these checks would be on.
 * CHECK-NOT: {{^}} #define foo2 bar{{$}}
 * CHECK-NOT: {{^}}foo2!{{$}}
 */

/* FIXME: -traditional-cpp should not homogenize whitespace in macros.
 */
#define bracket2(x) >>>  x  <<<
bracket2(spaces)
/* If this were working, this check would be on.
 * CHECK-NOT: {{^}}>>>  spaces  <<<{{$}}
 */


/* Check that #if 0 blocks work as expected */
#if 0
#error "this is not an error"

#if 1
a b c in skipped block
#endif

/* Comments are whitespace too */

#endif
/* CHECK-NOT: {{^}}a b c in skipped block{{$}}
 * CHECK-NOT: {{^}}/* Comments are whitespace too
 */

Preserve URLs: http://clang.llvm.org
/* CHECK: {{^}}Preserve URLs: http://clang.llvm.org{{$}}
 */
