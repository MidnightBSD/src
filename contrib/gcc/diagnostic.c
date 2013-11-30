/* Language-independent diagnostic subroutines for the GNU Compiler Collection
   Copyright (C) 1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.
   Contributed by Gabriel Dos Reis <gdr@codesourcery.com>

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */


/* This file implements the language independent aspect of diagnostic
   message module.  */

#include "config.h"
#undef FLOAT /* This is for hpux. They should change hpux.  */
#undef FFS  /* Some systems define this in param.h.  */
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "tm_p.h"
#include "flags.h"
#include "input.h"
#include "toplev.h"
#include "intl.h"
#include "diagnostic.h"
#include "langhooks.h"
#include "langhooks-def.h"


/* Prototypes.  */
static char *build_message_string (const char *, ...) ATTRIBUTE_PRINTF_1;

static void default_diagnostic_starter (diagnostic_context *,
					diagnostic_info *);
static void default_diagnostic_finalizer (diagnostic_context *,
					  diagnostic_info *);

static void error_recursion (diagnostic_context *) ATTRIBUTE_NORETURN;
static bool text_specifies_location (text_info *, location_t *);
static bool diagnostic_count_diagnostic (diagnostic_context *,
					 diagnostic_info *);
static void diagnostic_action_after_output (diagnostic_context *,
					    diagnostic_info *);
static void real_abort (void) ATTRIBUTE_NORETURN;

extern int rtl_dump_and_exit;

/* A diagnostic_context surrogate for stderr.  */
static diagnostic_context global_diagnostic_context;
diagnostic_context *global_dc = &global_diagnostic_context;

/* Boilerplate text used in two locations.  */
#define bug_report_request \
"Please submit a full bug report,\n\
with preprocessed source if appropriate.\n\
See %s for instructions.\n"


/* Return a malloc'd string containing MSG formatted a la printf.  The
   caller is responsible for freeing the memory.  */
static char *
build_message_string (const char *msg, ...)
{
  char *str;
  va_list ap;

  va_start (ap, msg);
  vasprintf (&str, msg, ap);
  va_end (ap);

  return str;
}

/* Same as diagnostic_build_prefix, but only the source FILE is given.  */
char *
file_name_as_prefix (const char *f)
{
  return build_message_string ("%s: ", f);
}



/* Initialize the diagnostic message outputting machinery.  */
void
diagnostic_initialize (diagnostic_context *context)
{
  /* Allocate a basic pretty-printer.  Clients will replace this a
     much more elaborated pretty-printer if they wish.  */
  context->printer = xmalloc (sizeof (pretty_printer));
  pp_construct (context->printer, NULL, 0);
  /* By default, diagnostics are sent to stderr.  */
  context->printer->buffer->stream = stderr;
  /* By default, we emit prefixes once per message.  */
  context->printer->prefixing_rule = DIAGNOSTICS_SHOW_PREFIX_ONCE;

  memset (context->diagnostic_count, 0, sizeof context->diagnostic_count);
  context->warnings_are_errors_message = warnings_are_errors;
  context->abort_on_error = false;
  context->internal_error = NULL;
  diagnostic_starter (context) = default_diagnostic_starter;
  diagnostic_finalizer (context) = default_diagnostic_finalizer;
  context->last_module = 0;
  context->last_function = NULL;
  context->lock = 0;
  context->x_data = NULL;
}

/* Returns true if the next format specifier in TEXT is a format specifier
   for a location_t.  If so, update the object pointed by LOCUS to reflect
   the specified location in *TEXT->args_ptr.  */
static bool
text_specifies_location (text_info *text, location_t *locus)
{
  const char *p;
  /* Skip any leading text.  */
  for (p = text->format_spec; *p && *p != '%'; ++p)
    ;

  /* Extract the location information if any.  */
  if (p[0] == '%' && p[1] == 'H')
    {
      *locus = *va_arg (*text->args_ptr, location_t *);
      text->format_spec = p + 2;
      return true;
    }
  else if (p[0] == '%' && p[1] == 'J')
    {
      tree t = va_arg (*text->args_ptr, tree);
      *locus = DECL_SOURCE_LOCATION (t);
      text->format_spec = p + 2;
      return true;
    }

  return false;
}

void
diagnostic_set_info (diagnostic_info *diagnostic, const char *msgid,
		     va_list *args, location_t location,
		     diagnostic_t kind)
{
  diagnostic->message.err_no = errno;
  diagnostic->message.args_ptr = args;
  diagnostic->message.format_spec = _(msgid);
  /* If the diagnostic message doesn't specify a location,
     use LOCATION.  */
  if (!text_specifies_location (&diagnostic->message, &diagnostic->location))
    diagnostic->location = location;
  diagnostic->kind = kind;
}

/* Return a malloc'd string describing a location.  The caller is
   responsible for freeing the memory.  */
char *
diagnostic_build_prefix (diagnostic_info *diagnostic)
{
  static const char *const diagnostic_kind_text[] = {
#define DEFINE_DIAGNOSTIC_KIND(K, T) (T),
#include "diagnostic.def"
#undef DEFINE_DIAGNOSTIC_KIND
    "must-not-happen"
  };
   if (diagnostic->kind >= DK_LAST_DIAGNOSTIC_KIND)
     abort();

  return diagnostic->location.file
    ? build_message_string ("%s:%d: %s",
                            diagnostic->location.file,
                            diagnostic->location.line,
                            _(diagnostic_kind_text[diagnostic->kind]))
    : build_message_string ("%s: %s", progname,
                            _(diagnostic_kind_text[diagnostic->kind]));
}

/* Count a diagnostic.  Return true if the message should be printed.  */
static bool
diagnostic_count_diagnostic (diagnostic_context *context,
			     diagnostic_info *diagnostic)
{
  diagnostic_t kind = diagnostic->kind;
  switch (kind)
    {
    default:
      abort();
      break;

    case DK_ICE:
#ifndef ENABLE_CHECKING
      /* When not checking, ICEs are converted to fatal errors when an
	 error has already occurred.  This is counteracted by
	 abort_on_error.  */
      if ((diagnostic_kind_count (context, DK_ERROR) > 0
	   || diagnostic_kind_count (context, DK_SORRY) > 0)
	  && !context->abort_on_error)
	{
	  fnotice (stderr, "%s:%d: confused by earlier errors, bailing out\n",
		   diagnostic->location.file, diagnostic->location.line);
	  exit (FATAL_EXIT_CODE);
	}
#endif
      if (context->internal_error)
	(*context->internal_error) (diagnostic->message.format_spec,
				    diagnostic->message.args_ptr);
      /* Fall through.  */

    case DK_FATAL: case DK_SORRY:
    case DK_ANACHRONISM: case DK_NOTE:
      ++diagnostic_kind_count (context, kind);
      break;

    case DK_WARNING:
      if (!diagnostic_report_warnings_p ())
        return false;

      if (!warnings_are_errors)
        {
          ++diagnostic_kind_count (context, DK_WARNING);
          break;
        }

      if (context->warnings_are_errors_message)
        {
	  pp_verbatim (context->printer,
                       "%s: warnings being treated as errors\n", progname);
          context->warnings_are_errors_message = false;
        }

      /* And fall through.  */
    case DK_ERROR:
      ++diagnostic_kind_count (context, DK_ERROR);
      break;
    }

  return true;
}

/* Take any action which is expected to happen after the diagnostic
   is written out.  This function does not always return.  */
static void
diagnostic_action_after_output (diagnostic_context *context,
				diagnostic_info *diagnostic)
{
  switch (diagnostic->kind)
    {
    case DK_DEBUG:
    case DK_NOTE:
    case DK_ANACHRONISM:
    case DK_WARNING:
      break;

    case DK_ERROR:
    case DK_SORRY:
      if (context->abort_on_error)
	real_abort ();
      break;

    case DK_ICE:
      if (context->abort_on_error)
	real_abort ();

      fnotice (stderr, bug_report_request, bug_report_url);
      exit (FATAL_EXIT_CODE);

    case DK_FATAL:
      if (context->abort_on_error)
	real_abort ();

      fnotice (stderr, "compilation terminated.\n");
      exit (FATAL_EXIT_CODE);

    default:
      real_abort ();
    }
}

/* Prints out, if necessary, the name of the current function
  that caused an error.  Called from all error and warning functions.
  We ignore the FILE parameter, as it cannot be relied upon.  */

void
diagnostic_report_current_function (diagnostic_context *context)
{
  diagnostic_report_current_module (context);
  (*lang_hooks.print_error_function) (context, input_filename);
}

void
diagnostic_report_current_module (diagnostic_context *context)
{
  struct file_stack *p;

  if (pp_needs_newline (context->printer))
    {
      pp_newline (context->printer);
      pp_needs_newline (context->printer) = false;
    }

  if (input_file_stack && diagnostic_last_module_changed (context))
    {
      p = input_file_stack;
      pp_verbatim (context->printer,
                   "In file included from %s:%d",
                   p->location.file, p->location.line);
      while ((p = p->next) != NULL)
	pp_verbatim (context->printer,
                     ",\n                 from %s:%d",
                     p->location.file, p->location.line);
      pp_verbatim (context->printer, ":\n");
      diagnostic_set_last_module (context);
    }
}

static void
default_diagnostic_starter (diagnostic_context *context,
			    diagnostic_info *diagnostic)
{
  diagnostic_report_current_function (context);
  pp_set_prefix (context->printer, diagnostic_build_prefix (diagnostic));
}

static void
default_diagnostic_finalizer (diagnostic_context *context,
			      diagnostic_info *diagnostic __attribute__((unused)))
{
  pp_destroy_prefix (context->printer);
}

/* Report a diagnostic message (an error or a warning) as specified by
   DC.  This function is *the* subroutine in terms of which front-ends
   should implement their specific diagnostic handling modules.  The
   front-end independent format specifiers are exactly those described
   in the documentation of output_format.  */

void
diagnostic_report_diagnostic (diagnostic_context *context,
			      diagnostic_info *diagnostic)
{
  if (context->lock++ && diagnostic->kind < DK_SORRY)
    error_recursion (context);

  if (diagnostic_count_diagnostic (context, diagnostic))
    {
      (*diagnostic_starter (context)) (context, diagnostic);
      pp_format_text (context->printer, &diagnostic->message);
      (*diagnostic_finalizer (context)) (context, diagnostic);
      pp_flush (context->printer);
      diagnostic_action_after_output (context, diagnostic);
    }

  context->lock--;
}

/* Given a partial pathname as input, return another pathname that
   shares no directory elements with the pathname of __FILE__.  This
   is used by fancy_abort() to print `Internal compiler error in expr.c'
   instead of `Internal compiler error in ../../GCC/gcc/expr.c'.  */

const char *
trim_filename (const char *name)
{
  static const char this_file[] = __FILE__;
  const char *p = name, *q = this_file;

  /* First skip any "../" in each filename.  This allows us to give a proper
     reference to a file in a subdirectory.  */
  while (p[0] == '.' && p[1] == '.'
	 && (p[2] == DIR_SEPARATOR
#ifdef DIR_SEPARATOR_2
	     || p[2] == DIR_SEPARATOR_2
#endif
	     ))
    p += 3;

  while (q[0] == '.' && q[1] == '.'
	 && (q[2] == DIR_SEPARATOR
#ifdef DIR_SEPARATOR_2
	     || p[2] == DIR_SEPARATOR_2
#endif
	     ))
    q += 3;

  /* Now skip any parts the two filenames have in common.  */
  while (*p == *q && *p != 0 && *q != 0)
    p++, q++;

  /* Now go backwards until the previous directory separator.  */
  while (p > name && p[-1] != DIR_SEPARATOR
#ifdef DIR_SEPARATOR_2
	 && p[-1] != DIR_SEPARATOR_2
#endif
	 )
    p--;

  return p;
}

/* Standard error reporting routines in increasing order of severity.
   All of these take arguments like printf.  */

/* Text to be emitted verbatim to the error message stream; this
   produces no prefix and disables line-wrapping.  Use rarely.  */
void
verbatim (const char *msgid, ...)
{
  text_info text;
  va_list ap;

  va_start (ap, msgid);
  text.err_no = errno;
  text.args_ptr = &ap;
  text.format_spec = _(msgid);
  pp_format_verbatim (global_dc->printer, &text);
  pp_flush (global_dc->printer);
  va_end (ap);
}

/* An informative note.  Use this for additional details on an error
   message.  */
void
inform (const char *msgid, ...)
{
  diagnostic_info diagnostic;
  va_list ap;

  va_start (ap, msgid);
  diagnostic_set_info (&diagnostic, msgid, &ap, input_location, DK_NOTE);
  report_diagnostic (&diagnostic);
  va_end (ap);
}

/* A warning.  Use this for code which is correct according to the
   relevant language specification but is likely to be buggy anyway.  */
void
warning (const char *msgid, ...)
{
  diagnostic_info diagnostic;
  va_list ap;

  va_start (ap, msgid);
  diagnostic_set_info (&diagnostic, msgid, &ap, input_location, DK_WARNING);
  report_diagnostic (&diagnostic);
  va_end (ap);
}

/* A "pedantic" warning: issues a warning unless -pedantic-errors was
   given on the command line, in which case it issues an error.  Use
   this for diagnostics required by the relevant language standard,
   if you have chosen not to make them errors.

   Note that these diagnostics are issued independent of the setting
   of the -pedantic command-line switch.  To get a warning enabled
   only with that switch, write "if (pedantic) pedwarn (...);"  */
void
pedwarn (const char *msgid, ...)
{
  diagnostic_info diagnostic;
  va_list ap;

  va_start (ap, msgid);
  diagnostic_set_info (&diagnostic, msgid, &ap, input_location,
		       pedantic_error_kind ());
  report_diagnostic (&diagnostic);
  va_end (ap);
}

/* A hard error: the code is definitely ill-formed, and an object file
   will not be produced.  */
void
error (const char *msgid, ...)
{
  diagnostic_info diagnostic;
  va_list ap;

  va_start (ap, msgid);
  diagnostic_set_info (&diagnostic, msgid, &ap, input_location, DK_ERROR);
  report_diagnostic (&diagnostic);
  va_end (ap);
}

/* "Sorry, not implemented."  Use for a language feature which is
   required by the relevant specification but not implemented by GCC.
   An object file will not be produced.  */
void
sorry (const char *msgid, ...)
{
  diagnostic_info diagnostic;
  va_list ap;

  va_start (ap, msgid);
  diagnostic_set_info (&diagnostic, msgid, &ap, input_location, DK_SORRY);
  report_diagnostic (&diagnostic);
  va_end (ap);
}

/* An error which is severe enough that we make no attempt to
   continue.  Do not use this for internal consistency checks; that's
   internal_error.  Use of this function should be rare.  */
void
fatal_error (const char *msgid, ...)
{
  diagnostic_info diagnostic;
  va_list ap;

  va_start (ap, msgid);
  diagnostic_set_info (&diagnostic, msgid, &ap, input_location, DK_FATAL);
  report_diagnostic (&diagnostic);
  va_end (ap);

  /* NOTREACHED */
  real_abort ();
}

/* An internal consistency check has failed.  We make no attempt to
   continue.  Note that unless there is debugging value to be had from
   a more specific message, or some other good reason, you should use
   abort () instead of calling this function directly.  */
void
internal_error (const char *msgid, ...)
{
  diagnostic_info diagnostic;
  va_list ap;

  va_start (ap, msgid);
  diagnostic_set_info (&diagnostic, msgid, &ap, input_location, DK_ICE);
  report_diagnostic (&diagnostic);
  va_end (ap);

  /* NOTREACHED */
  real_abort ();
}

/* Special case error functions.  Most are implemented in terms of the
   above, or should be.  */

/* Print a diagnostic MSGID on FILE.  This is just fprintf, except it
   runs its second argument through gettext.  */
void
fnotice (FILE *file, const char *msgid, ...)
{
  va_list ap;

  va_start (ap, msgid);
  vfprintf (file, _(msgid), ap);
  va_end (ap);
}

/* Inform the user that an error occurred while trying to report some
   other error.  This indicates catastrophic internal inconsistencies,
   so give up now.  But do try to flush out the previous error.
   This mustn't use internal_error, that will cause infinite recursion.  */

static void
error_recursion (diagnostic_context *context)
{
  if (context->lock < 3)
    pp_flush (context->printer);

  fnotice (stderr,
	   "Internal compiler error: Error reporting routines re-entered.\n");
  fnotice (stderr, bug_report_request, bug_report_url);
  exit (FATAL_EXIT_CODE);
}

/* Report an internal compiler error in a friendly manner.  This is
   the function that gets called upon use of abort() in the source
   code generally, thanks to a special macro.  */

void
fancy_abort (const char *file, int line, const char *function)
{
  internal_error ("in %s, at %s:%d", function, trim_filename (file), line);
}

/* Really call the system 'abort'.  This has to go right at the end of
   this file, so that there are no functions after it that call abort
   and get the system abort instead of our macro.  */
#undef abort
static void
real_abort (void)
{
  abort ();
}
