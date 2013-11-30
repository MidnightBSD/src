/* Instruction printing code for the ARM
   Copyright 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.
   Contributed by Richard Earnshaw (rwe@pegasus.esprit.ec.org)
   Modification by James G. Smith (jsmith@cygnus.co.uk)

   This file is part of libopcodes.

   This program is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "sysdep.h"
#include "dis-asm.h"
#define DEFINE_TABLE
#include "arm-opc.h"
#include "coff/internal.h"
#include "libcoff.h"
#include "opintl.h"
#include "safe-ctype.h"

/* FIXME: This shouldn't be done here.  */
#include "elf-bfd.h"
#include "elf/internal.h"
#include "elf/arm.h"

#ifndef streq
#define streq(a,b)	(strcmp ((a), (b)) == 0)
#endif

#ifndef strneq
#define strneq(a,b,n)	(strncmp ((a), (b), (n)) == 0)
#endif

#ifndef NUM_ELEM
#define NUM_ELEM(a)     (sizeof (a) / sizeof (a)[0])
#endif

static char * arm_conditional[] =
{"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
 "hi", "ls", "ge", "lt", "gt", "le", "", "nv"};

typedef struct
{
  const char * name;
  const char * description;
  const char * reg_names[16];
}
arm_regname;

static arm_regname regnames[] =
{
  { "raw" , "Select raw register names",
    { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"}},
  { "gcc",  "Select register names used by GCC",
    { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "sl",  "fp",  "ip",  "sp",  "lr",  "pc" }},
  { "std",  "Select register names used in ARM's ISA documentation",
    { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "sp",  "lr",  "pc" }},
  { "apcs", "Select register names used in the APCS",
    { "a1", "a2", "a3", "a4", "v1", "v2", "v3", "v4", "v5", "v6", "sl",  "fp",  "ip",  "sp",  "lr",  "pc" }},
  { "atpcs", "Select register names used in the ATPCS",
    { "a1", "a2", "a3", "a4", "v1", "v2", "v3", "v4", "v5", "v6", "v7",  "v8",  "IP",  "SP",  "LR",  "PC" }},
  { "special-atpcs", "Select special register names used in the ATPCS",
    { "a1", "a2", "a3", "a4", "v1", "v2", "v3", "WR", "v5", "SB", "SL",  "FP",  "IP",  "SP",  "LR",  "PC" }},
  { "iwmmxt_regnames", "Select register names used on the Intel Wireless MMX technology coprocessor",
    { "wr0", "wr1", "wr2", "wr3", "wr4", "wr5", "wr6", "wr7", "wr8", "wr9", "wr10", "wr11", "wr12", "wr13", "wr14", "wr15"}},
  { "iwmmxt_Cregnames", "Select control register names used on the Intel Wireless MMX technology coprocessor",
    {"wcid", "wcon", "wcssf", "wcasf", "reserved", "reserved", "reserved", "reserved", "wcgr0", "wcgr1", "wcgr2", "wcgr3", "reserved", "reserved", "reserved", "reserved"}}
};

static char * iwmmxt_wwnames[] =
{"b", "h", "w", "d"};

static char * iwmmxt_wwssnames[] =
{"b", "bus", "b", "bss",
 "h", "hus", "h", "hss",
 "w", "wus", "w", "wss",
 "d", "dus", "d", "dss"
};

/* Default to GCC register name set.  */
static unsigned int regname_selected = 1;

#define NUM_ARM_REGNAMES  NUM_ELEM (regnames)
#define arm_regnames      regnames[regname_selected].reg_names

static bfd_boolean force_thumb = FALSE;

static char * arm_fp_const[] =
{"0.0", "1.0", "2.0", "3.0", "4.0", "5.0", "0.5", "10.0"};

static char * arm_shift[] =
{"lsl", "lsr", "asr", "ror"};

/* Forward declarations.  */
static void arm_decode_shift
  PARAMS ((long, fprintf_ftype, void *));
static int  print_insn_arm
  PARAMS ((bfd_vma, struct disassemble_info *, long));
static int  print_insn_thumb
  PARAMS ((bfd_vma, struct disassemble_info *, long));
static void parse_disassembler_options
  PARAMS ((char *));
static int  print_insn
  PARAMS ((bfd_vma, struct disassemble_info *, bfd_boolean));
static int set_iwmmxt_regnames
  PARAMS ((void));

int get_arm_regname_num_options
  PARAMS ((void));
int set_arm_regname_option
  PARAMS ((int));
int get_arm_regnames
  PARAMS ((int, const char **, const char **, const char ***));

/* Functions.  */
int
get_arm_regname_num_options ()
{
  return NUM_ARM_REGNAMES;
}

int
set_arm_regname_option (option)
     int option;
{
  int old = regname_selected;
  regname_selected = option;
  return old;
}

int
get_arm_regnames (option, setname, setdescription, register_names)
     int option;
     const char **setname;
     const char **setdescription;
     const char ***register_names;
{
  *setname = regnames[option].name;
  *setdescription = regnames[option].description;
  *register_names = regnames[option].reg_names;
  return 16;
}

static void
arm_decode_shift (given, func, stream)
     long given;
     fprintf_ftype func;
     void * stream;
{
  func (stream, "%s", arm_regnames[given & 0xf]);

  if ((given & 0xff0) != 0)
    {
      if ((given & 0x10) == 0)
	{
	  int amount = (given & 0xf80) >> 7;
	  int shift = (given & 0x60) >> 5;

	  if (amount == 0)
	    {
	      if (shift == 3)
		{
		  func (stream, ", rrx");
		  return;
		}

	      amount = 32;
	    }

	  func (stream, ", %s #%d", arm_shift[shift], amount);
	}
      else
	func (stream, ", %s %s", arm_shift[(given & 0x60) >> 5],
	      arm_regnames[(given & 0xf00) >> 8]);
    }
}

static int
set_iwmmxt_regnames ()
{
  const char * setname;
  const char * setdesc;
  const char ** regnames;
  int iwmmxt_regnames = 0;
  int num_regnames = get_arm_regname_num_options ();

  get_arm_regnames (iwmmxt_regnames, &setname,
		    &setdesc, &regnames);
  while ((strcmp ("iwmmxt_regnames", setname))
	 && (iwmmxt_regnames < num_regnames))
    get_arm_regnames (++iwmmxt_regnames, &setname, &setdesc, &regnames);

  return iwmmxt_regnames;
}
			  
/* Print one instruction from PC on INFO->STREAM.
   Return the size of the instruction (always 4 on ARM). */

static int
print_insn_arm (pc, info, given)
     bfd_vma pc;
     struct disassemble_info *info;
     long given;
{
  const struct arm_opcode *insn;
  void *stream = info->stream;
  fprintf_ftype func   = info->fprintf_func;
  static int iwmmxt_regnames = 0;

  for (insn = arm_opcodes; insn->assembler; insn++)
    {
      if (insn->value == FIRST_IWMMXT_INSN
	  && info->mach != bfd_mach_arm_XScale
	  && info->mach != bfd_mach_arm_iWMMXt)
	insn = insn + IWMMXT_INSN_COUNT;

      if ((given & insn->mask) == insn->value)
	{
	  char * c;

	  for (c = insn->assembler; *c; c++)
	    {
	      if (*c == '%')
		{
		  switch (*++c)
		    {
		    case '%':
		      func (stream, "%%");
		      break;

		    case 'a':
		      if (((given & 0x000f0000) == 0x000f0000)
			  && ((given & 0x02000000) == 0))
			{
			  int offset = given & 0xfff;

			  func (stream, "[pc");

			  if (given & 0x01000000)
			    {
			      if ((given & 0x00800000) == 0)
				offset = - offset;

			      /* Pre-indexed.  */
			      func (stream, ", #%d]", offset);

			      offset += pc + 8;

			      /* Cope with the possibility of write-back
				 being used.  Probably a very dangerous thing
				 for the programmer to do, but who are we to
				 argue ?  */
			      if (given & 0x00200000)
				func (stream, "!");
			    }
			  else
			    {
			      /* Post indexed.  */
			      func (stream, "], #%d", offset);

			      /* ie ignore the offset.  */
			      offset = pc + 8;
			    }

			  func (stream, "\t; ");
			  info->print_address_func (offset, info);
			}
		      else
			{
			  func (stream, "[%s",
				arm_regnames[(given >> 16) & 0xf]);
			  if ((given & 0x01000000) != 0)
			    {
			      if ((given & 0x02000000) == 0)
				{
				  int offset = given & 0xfff;
				  if (offset)
				    func (stream, ", #%s%d",
					  (((given & 0x00800000) == 0)
					   ? "-" : ""), offset);
				}
			      else
				{
				  func (stream, ", %s",
					(((given & 0x00800000) == 0)
					 ? "-" : ""));
				  arm_decode_shift (given, func, stream);
				}

			      func (stream, "]%s",
				    ((given & 0x00200000) != 0) ? "!" : "");
			    }
			  else
			    {
			      if ((given & 0x02000000) == 0)
				{
				  int offset = given & 0xfff;
				  if (offset)
				    func (stream, "], #%s%d",
					  (((given & 0x00800000) == 0)
					   ? "-" : ""), offset);
				  else
				    func (stream, "]");
				}
			      else
				{
				  func (stream, "], %s",
					(((given & 0x00800000) == 0)
					 ? "-" : ""));
				  arm_decode_shift (given, func, stream);
				}
			    }
			}
		      break;

		    case 's':
                      if ((given & 0x004f0000) == 0x004f0000)
			{
                          /* PC relative with immediate offset.  */
			  int offset = ((given & 0xf00) >> 4) | (given & 0xf);

			  if ((given & 0x00800000) == 0)
			    offset = -offset;

			  func (stream, "[pc, #%d]\t; ", offset);

			  (*info->print_address_func)
			    (offset + pc + 8, info);
			}
		      else
			{
			  func (stream, "[%s",
				arm_regnames[(given >> 16) & 0xf]);
			  if ((given & 0x01000000) != 0)
			    {
                              /* Pre-indexed.  */
			      if ((given & 0x00400000) == 0x00400000)
				{
                                  /* Immediate.  */
                                  int offset = ((given & 0xf00) >> 4) | (given & 0xf);
				  if (offset)
				    func (stream, ", #%s%d",
					  (((given & 0x00800000) == 0)
					   ? "-" : ""), offset);
				}
			      else
				{
                                  /* Register.  */
				  func (stream, ", %s%s",
					(((given & 0x00800000) == 0)
					 ? "-" : ""),
                                        arm_regnames[given & 0xf]);
				}

			      func (stream, "]%s",
				    ((given & 0x00200000) != 0) ? "!" : "");
			    }
			  else
			    {
                              /* Post-indexed.  */
			      if ((given & 0x00400000) == 0x00400000)
				{
                                  /* Immediate.  */
                                  int offset = ((given & 0xf00) >> 4) | (given & 0xf);
				  if (offset)
				    func (stream, "], #%s%d",
					  (((given & 0x00800000) == 0)
					   ? "-" : ""), offset);
				  else
				    func (stream, "]");
				}
			      else
				{
                                  /* Register.  */
				  func (stream, "], %s%s",
					(((given & 0x00800000) == 0)
					 ? "-" : ""),
                                        arm_regnames[given & 0xf]);
				}
			    }
			}
		      break;

		    case 'b':
		      (*info->print_address_func)
			(BDISP (given) * 4 + pc + 8, info);
		      break;

		    case 'c':
		      func (stream, "%s",
			    arm_conditional [(given >> 28) & 0xf]);
		      break;

		    case 'm':
		      {
			int started = 0;
			int reg;

			func (stream, "{");
			for (reg = 0; reg < 16; reg++)
			  if ((given & (1 << reg)) != 0)
			    {
			      if (started)
				func (stream, ", ");
			      started = 1;
			      func (stream, "%s", arm_regnames[reg]);
			    }
			func (stream, "}");
		      }
		      break;

		    case 'o':
		      if ((given & 0x02000000) != 0)
			{
			  int rotate = (given & 0xf00) >> 7;
			  int immed = (given & 0xff);
			  immed = (((immed << (32 - rotate))
				    | (immed >> rotate)) & 0xffffffff);
			  func (stream, "#%d\t; 0x%x", immed, immed);
			}
		      else
			arm_decode_shift (given, func, stream);
		      break;

		    case 'p':
		      if ((given & 0x0000f000) == 0x0000f000)
			func (stream, "p");
		      break;

		    case 't':
		      if ((given & 0x01200000) == 0x00200000)
			func (stream, "t");
		      break;

		    case 'A':
		      func (stream, "[%s", arm_regnames [(given >> 16) & 0xf]);

		      if ((given & (1 << 24)) != 0)
			{
			  int offset = given & 0xff;

			  if (offset)
			    func (stream, ", #%s%d]%s",
				  ((given & 0x00800000) == 0 ? "-" : ""),
				  offset * 4,
				  ((given & 0x00200000) != 0 ? "!" : ""));
			  else
			    func (stream, "]");
			}
		      else
			{
			  int offset = given & 0xff;

			  func (stream, "]");

			  if (given & (1 << 21))
			    {
			      if (offset)
				func (stream, ", #%s%d",
				      ((given & 0x00800000) == 0 ? "-" : ""),
				      offset * 4);
			    }
			  else
			    func (stream, ", {%d}", offset);
			}
		      break;

		    case 'B':
		      /* Print ARM V5 BLX(1) address: pc+25 bits.  */
		      {
			bfd_vma address;
			bfd_vma offset = 0;

			if (given & 0x00800000)
			  /* Is signed, hi bits should be ones.  */
			  offset = (-1) ^ 0x00ffffff;

			/* Offset is (SignExtend(offset field)<<2).  */
			offset += given & 0x00ffffff;
			offset <<= 2;
			address = offset + pc + 8;

			if (given & 0x01000000)
			  /* H bit allows addressing to 2-byte boundaries.  */
			  address += 2;

		        info->print_address_func (address, info);
		      }
		      break;

		    case 'I':
		      /* Print a Cirrus/DSP shift immediate.  */
		      /* Immediates are 7bit signed ints with bits 0..3 in
			 bits 0..3 of opcode and bits 4..6 in bits 5..7
			 of opcode.  */
		      {
			int imm;

			imm = (given & 0xf) | ((given & 0xe0) >> 1);

			/* Is ``imm'' a negative number?  */
			if (imm & 0x40)
			  imm |= (-1 << 7);

			func (stream, "%d", imm);
		      }

		      break;

		    case 'C':
		      func (stream, "_");
		      if (given & 0x80000)
			func (stream, "f");
		      if (given & 0x40000)
			func (stream, "s");
		      if (given & 0x20000)
			func (stream, "x");
		      if (given & 0x10000)
			func (stream, "c");
		      break;

		    case 'F':
		      switch (given & 0x00408000)
			{
			case 0:
			  func (stream, "4");
			  break;
			case 0x8000:
			  func (stream, "1");
			  break;
			case 0x00400000:
			  func (stream, "2");
			  break;
			default:
			  func (stream, "3");
			}
		      break;

		    case 'P':
		      switch (given & 0x00080080)
			{
			case 0:
			  func (stream, "s");
			  break;
			case 0x80:
			  func (stream, "d");
			  break;
			case 0x00080000:
			  func (stream, "e");
			  break;
			default:
			  func (stream, _("<illegal precision>"));
			  break;
			}
		      break;
		    case 'Q':
		      switch (given & 0x00408000)
			{
			case 0:
			  func (stream, "s");
			  break;
			case 0x8000:
			  func (stream, "d");
			  break;
			case 0x00400000:
			  func (stream, "e");
			  break;
			default:
			  func (stream, "p");
			  break;
			}
		      break;
		    case 'R':
		      switch (given & 0x60)
			{
			case 0:
			  break;
			case 0x20:
			  func (stream, "p");
			  break;
			case 0x40:
			  func (stream, "m");
			  break;
			default:
			  func (stream, "z");
			  break;
			}
		      break;

		    case '0': case '1': case '2': case '3': case '4':
		    case '5': case '6': case '7': case '8': case '9':
		      {
			int bitstart = *c++ - '0';
			int bitend = 0;
			while (*c >= '0' && *c <= '9')
			  bitstart = (bitstart * 10) + *c++ - '0';

			switch (*c)
			  {
			  case '-':
			    c++;

			    while (*c >= '0' && *c <= '9')
			      bitend = (bitend * 10) + *c++ - '0';

			    if (!bitend)
			      abort ();

			    switch (*c)
			      {
			      case 'r':
				{
				  long reg;

				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;

				  func (stream, "%s", arm_regnames[reg]);
				}
				break;
			      case 'd':
				{
				  long reg;

				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;

				  func (stream, "%d", reg);
				}
				break;
			      case 'W':
				{
				  long reg;
				  
				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;
				  
				  func (stream, "%d", reg + 1);
				}
				break;
			      case 'x':
				{
				  long reg;

				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;

				  func (stream, "0x%08x", reg);

				  /* Some SWI instructions have special
				     meanings.  */
				  if ((given & 0x0fffffff) == 0x0FF00000)
				    func (stream, "\t; IMB");
				  else if ((given & 0x0fffffff) == 0x0FF00001)
				    func (stream, "\t; IMBRange");
				}
				break;
			      case 'X':
				{
				  long reg;

				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;

				  func (stream, "%01x", reg & 0xf);
				}
				break;
			      case 'f':
				{
				  long reg;

				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;

				  if (reg > 7)
				    func (stream, "#%s",
					  arm_fp_const[reg & 7]);
				  else
				    func (stream, "f%d", reg);
				}
				break;

			      case 'w':
				{
				  long reg;

				  if (bitstart != bitend)
				    {
				      reg = given >> bitstart;
				      reg &= (2 << (bitend - bitstart)) - 1;
				      if (bitend - bitstart == 1)
					func (stream, "%s", iwmmxt_wwnames[reg]);
				      else
					func (stream, "%s", iwmmxt_wwssnames[reg]);
				    }
				  else
				    {
				      reg = (((given >> 8)  & 0x1) |
					     ((given >> 22) & 0x1));
				      func (stream, "%s", iwmmxt_wwnames[reg]);
				    }
				}
				break;

			      case 'g':
				{
				  long reg;
				  int current_regnames;

				  if (! iwmmxt_regnames)
				    iwmmxt_regnames = set_iwmmxt_regnames ();
				  current_regnames = set_arm_regname_option
				    (iwmmxt_regnames);

				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;
				  func (stream, "%s", arm_regnames[reg]);
				  set_arm_regname_option (current_regnames);
				}
				break;

			      case 'G':
				{
				  long reg;
				  int current_regnames;

				  if (! iwmmxt_regnames)
				    iwmmxt_regnames = set_iwmmxt_regnames ();
				  current_regnames = set_arm_regname_option
				    (iwmmxt_regnames + 1);

				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;
				  func (stream, "%s", arm_regnames[reg]);
				  set_arm_regname_option (current_regnames);
				}
				break;

			      default:
				abort ();
			      }
			    break;

			  case 'y':
			  case 'z':
			    {
			      int single = *c == 'y';
			      int regno;

			      switch (bitstart)
				{
				case 4: /* Sm pair */
				  func (stream, "{");
				  /* Fall through.  */
				case 0: /* Sm, Dm */
				  regno = given & 0x0000000f;
				  if (single)
				    {
				      regno <<= 1;
				      regno += (given >> 5) & 1;
				    }
				  break;

				case 1: /* Sd, Dd */
				  regno = (given >> 12) & 0x0000000f;
				  if (single)
				    {
				      regno <<= 1;
				      regno += (given >> 22) & 1;
				    }
				  break;

				case 2: /* Sn, Dn */
				  regno = (given >> 16) & 0x0000000f;
				  if (single)
				    {
				      regno <<= 1;
				      regno += (given >> 7) & 1;
				    }
				  break;

				case 3: /* List */
				  func (stream, "{");
				  regno = (given >> 12) & 0x0000000f;
				  if (single)
				    {
				      regno <<= 1;
				      regno += (given >> 22) & 1;
				    }
				  break;


				default:
				  abort ();
				}

			      func (stream, "%c%d", single ? 's' : 'd', regno);

			      if (bitstart == 3)
				{
				  int count = given & 0xff;

				  if (single == 0)
				    count >>= 1;

				  if (--count)
				    {
				      func (stream, "-%c%d",
					    single ? 's' : 'd',
					    regno + count);
				    }

				  func (stream, "}");
				}
			      else if (bitstart == 4)
				func (stream, ", %c%d}", single ? 's' : 'd',
				      regno + 1);

			      break;
			    }

			  case '`':
			    c++;
			    if ((given & (1 << bitstart)) == 0)
			      func (stream, "%c", *c);
			    break;
			  case '\'':
			    c++;
			    if ((given & (1 << bitstart)) != 0)
			      func (stream, "%c", *c);
			    break;
			  case '?':
			    ++c;
			    if ((given & (1 << bitstart)) != 0)
			      func (stream, "%c", *c++);
			    else
			      func (stream, "%c", *++c);
			    break;
			  default:
			    abort ();
			  }
			break;

		      case 'L':
			switch (given & 0x00400100)
			  {
			  case 0x00000000: func (stream, "b"); break;
			  case 0x00400000: func (stream, "h"); break;
			  case 0x00000100: func (stream, "w"); break;
			  case 0x00400100: func (stream, "d"); break;
			  default:
			    break;
			  }
			break;

		      case 'Z':
			{
			  int value;
			  /* given (20, 23) | given (0, 3) */
			  value = ((given >> 16) & 0xf0) | (given & 0xf);
			  func (stream, "%d", value);
			}
			break;

		      case 'l':
			/* This is like the 'A' operator, except that if
			   the width field "M" is zero, then the offset is
			   *not* multiplied by four.  */
			{
			  int offset = given & 0xff;
			  int multiplier = (given & 0x00000100) ? 4 : 1;

			  func (stream, "[%s", arm_regnames [(given >> 16) & 0xf]);

			  if (offset)
			    {
			      if ((given & 0x01000000) != 0)
				func (stream, ", #%s%d]%s",
				      ((given & 0x00800000) == 0 ? "-" : ""),
				      offset * multiplier,
				      ((given & 0x00200000) != 0 ? "!" : ""));
			      else
				func (stream, "], #%s%d",
				      ((given & 0x00800000) == 0 ? "-" : ""),
				      offset * multiplier);
			    }
			  else
			    func (stream, "]");
			}
			break;

		      default:
			abort ();
		      }
		    }
		}
	      else
		func (stream, "%c", *c);
	    }
	  return 4;
	}
    }
  abort ();
}

/* Print one instruction from PC on INFO->STREAM.
   Return the size of the instruction. */

static int
print_insn_thumb (pc, info, given)
     bfd_vma pc;
     struct disassemble_info *info;
     long given;
{
  const struct thumb_opcode *insn;
  void *stream = info->stream;
  fprintf_ftype func = info->fprintf_func;

  for (insn = thumb_opcodes; insn->assembler; insn++)
    {
      if ((given & insn->mask) == insn->value)
        {
          char * c = insn->assembler;

          /* Special processing for Thumb 2 instruction BL sequence:  */
          if (!*c) /* Check for empty (not NULL) assembler string.  */
            {
	      long offset;

	      info->bytes_per_chunk = 4;
	      info->bytes_per_line  = 4;

	      offset = BDISP23 (given);
	      offset = offset * 2 + pc + 4;

	      if ((given & 0x10000000) == 0)
		{
		  func (stream, "blx\t");
		  offset &= 0xfffffffc;
		}
	      else
		func (stream, "bl\t");

	      info->print_address_func (offset, info);
              return 4;
            }
          else
            {
	      info->bytes_per_chunk = 2;
	      info->bytes_per_line  = 4;

              given &= 0xffff;

              for (; *c; c++)
                {
                  if (*c == '%')
                    {
                      int domaskpc = 0;
                      int domasklr = 0;

                      switch (*++c)
                        {
                        case '%':
                          func (stream, "%%");
                          break;

                        case 'S':
                          {
                            long reg;

                            reg = (given >> 3) & 0x7;
                            if (given & (1 << 6))
                              reg += 8;

                            func (stream, "%s", arm_regnames[reg]);
                          }
                          break;

                        case 'D':
                          {
                            long reg;

                            reg = given & 0x7;
                            if (given & (1 << 7))
                             reg += 8;

                            func (stream, "%s", arm_regnames[reg]);
                          }
                          break;

                        case 'T':
                          func (stream, "%s",
                                arm_conditional [(given >> 8) & 0xf]);
                          break;

                        case 'N':
                          if (given & (1 << 8))
                            domasklr = 1;
                          /* Fall through.  */
                        case 'O':
                          if (*c == 'O' && (given & (1 << 8)))
                            domaskpc = 1;
                          /* Fall through.  */
                        case 'M':
                          {
                            int started = 0;
                            int reg;

                            func (stream, "{");

                            /* It would be nice if we could spot
                               ranges, and generate the rS-rE format: */
                            for (reg = 0; (reg < 8); reg++)
                              if ((given & (1 << reg)) != 0)
                                {
                                  if (started)
                                    func (stream, ", ");
                                  started = 1;
                                  func (stream, "%s", arm_regnames[reg]);
                                }

                            if (domasklr)
                              {
                                if (started)
                                  func (stream, ", ");
                                started = 1;
                                func (stream, arm_regnames[14] /* "lr" */);
                              }

                            if (domaskpc)
                              {
                                if (started)
                                  func (stream, ", ");
                                func (stream, arm_regnames[15] /* "pc" */);
                              }

                            func (stream, "}");
                          }
                          break;


                        case '0': case '1': case '2': case '3': case '4':
                        case '5': case '6': case '7': case '8': case '9':
                          {
                            int bitstart = *c++ - '0';
                            int bitend = 0;

                            while (*c >= '0' && *c <= '9')
                              bitstart = (bitstart * 10) + *c++ - '0';

                            switch (*c)
                              {
                              case '-':
                                {
                                  long reg;

                                  c++;
                                  while (*c >= '0' && *c <= '9')
                                    bitend = (bitend * 10) + *c++ - '0';
                                  if (!bitend)
                                    abort ();
                                  reg = given >> bitstart;
                                  reg &= (2 << (bitend - bitstart)) - 1;
                                  switch (*c)
                                    {
                                    case 'r':
                                      func (stream, "%s", arm_regnames[reg]);
                                      break;

                                    case 'd':
                                      func (stream, "%d", reg);
                                      break;

                                    case 'H':
                                      func (stream, "%d", reg << 1);
                                      break;

                                    case 'W':
                                      func (stream, "%d", reg << 2);
                                      break;

                                    case 'a':
				      /* PC-relative address -- the bottom two
					 bits of the address are dropped
					 before the calculation.  */
                                      info->print_address_func
					(((pc + 4) & ~3) + (reg << 2), info);
                                      break;

                                    case 'x':
                                      func (stream, "0x%04x", reg);
                                      break;

                                    case 'I':
                                      reg = ((reg ^ (1 << bitend)) - (1 << bitend));
                                      func (stream, "%d", reg);
                                      break;

                                    case 'B':
                                      reg = ((reg ^ (1 << bitend)) - (1 << bitend));
                                      (*info->print_address_func)
                                        (reg * 2 + pc + 4, info);
                                      break;

                                    default:
                                      abort ();
                                    }
                                }
                                break;

                              case '\'':
                                c++;
                                if ((given & (1 << bitstart)) != 0)
                                  func (stream, "%c", *c);
                                break;

                              case '?':
                                ++c;
                                if ((given & (1 << bitstart)) != 0)
                                  func (stream, "%c", *c++);
                                else
                                  func (stream, "%c", *++c);
                                break;

                              default:
                                 abort ();
                              }
                          }
                          break;

                        default:
                          abort ();
                        }
                    }
                  else
                    func (stream, "%c", *c);
                }
             }
          return 2;
       }
    }

  /* No match.  */
  abort ();
}

/* Disallow mapping symbols ($a, $b, $d, $t etc) from
   being displayed in symbol relative addresses.  */

bfd_boolean
arm_symbol_is_valid (asymbol * sym,
		     struct disassemble_info * info ATTRIBUTE_UNUSED)
{
  const char * name;
  
  if (sym == NULL)
    return FALSE;

  name = bfd_asymbol_name (sym);

  return (name && *name != '$');
}

/* Parse an individual disassembler option.  */

void
parse_arm_disassembler_option (option)
     char * option;
{
  if (option == NULL)
    return;

  if (strneq (option, "reg-names-", 10))
    {
      int i;

      option += 10;

      for (i = NUM_ARM_REGNAMES; i--;)
	if (strneq (option, regnames[i].name, strlen (regnames[i].name)))
	  {
	    regname_selected = i;
	    break;
	  }

      if (i < 0)
	/* XXX - should break 'option' at following delimiter.  */
	fprintf (stderr, _("Unrecognised register name set: %s\n"), option);
    }
  else if (strneq (option, "force-thumb", 11))
    force_thumb = 1;
  else if (strneq (option, "no-force-thumb", 14))
    force_thumb = 0;
  else
    /* XXX - should break 'option' at following delimiter.  */
    fprintf (stderr, _("Unrecognised disassembler option: %s\n"), option);

  return;
}

/* Parse the string of disassembler options, spliting it at whitespaces
   or commas.  (Whitespace separators supported for backwards compatibility).  */

static void
parse_disassembler_options (options)
     char * options;
{
  if (options == NULL)
    return;

  while (*options)
    {
      parse_arm_disassembler_option (options);

      /* Skip forward to next seperator.  */
      while ((*options) && (! ISSPACE (*options)) && (*options != ','))
	++ options;
      /* Skip forward past seperators.  */
      while (ISSPACE (*options) || (*options == ','))
	++ options;      
    }
}

/* NOTE: There are no checks in these routines that
   the relevant number of data bytes exist.  */

static int
print_insn (pc, info, little)
     bfd_vma pc;
     struct disassemble_info * info;
     bfd_boolean little;
{
  unsigned char      b[4];
  long               given;
  int                status;
  int                is_thumb;

  if (info->disassembler_options)
    {
      parse_disassembler_options (info->disassembler_options);

      /* To avoid repeated parsing of these options, we remove them here.  */
      info->disassembler_options = NULL;
    }

  is_thumb = force_thumb;

  if (!is_thumb && info->symbols != NULL)
    {
      if (bfd_asymbol_flavour (*info->symbols) == bfd_target_coff_flavour)
	{
	  coff_symbol_type * cs;

	  cs = coffsymbol (*info->symbols);
	  is_thumb = (   cs->native->u.syment.n_sclass == C_THUMBEXT
		      || cs->native->u.syment.n_sclass == C_THUMBSTAT
		      || cs->native->u.syment.n_sclass == C_THUMBLABEL
		      || cs->native->u.syment.n_sclass == C_THUMBEXTFUNC
		      || cs->native->u.syment.n_sclass == C_THUMBSTATFUNC);
	}
      else if (bfd_asymbol_flavour (*info->symbols) == bfd_target_elf_flavour)
	{
	  elf_symbol_type *  es;
	  unsigned int       type;

	  es = *(elf_symbol_type **)(info->symbols);
	  type = ELF_ST_TYPE (es->internal_elf_sym.st_info);

	  is_thumb = (type == STT_ARM_TFUNC) || (type == STT_ARM_16BIT);
	}
    }

  info->bytes_per_chunk = 4;
  info->display_endian  = little ? BFD_ENDIAN_LITTLE : BFD_ENDIAN_BIG;

  if (little)
    {
      status = info->read_memory_func (pc, (bfd_byte *) &b[0], 4, info);
      if (status != 0 && is_thumb)
	{
	  info->bytes_per_chunk = 2;

	  status = info->read_memory_func (pc, (bfd_byte *) b, 2, info);
	  b[3] = b[2] = 0;
	}

      if (status != 0)
	{
	  info->memory_error_func (status, pc, info);
	  return -1;
	}

      given = (b[0]) | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
    }
  else
    {
      status = info->read_memory_func
	(pc & ~ 0x3, (bfd_byte *) &b[0], 4, info);
      if (status != 0)
	{
	  info->memory_error_func (status, pc, info);
	  return -1;
	}

      if (is_thumb)
	{
	  if (pc & 0x2)
	    {
	      given = (b[2] << 8) | b[3];

	      status = info->read_memory_func
		((pc + 4) & ~ 0x3, (bfd_byte *) b, 4, info);
	      if (status != 0)
		{
		  info->memory_error_func (status, pc + 4, info);
		  return -1;
		}

	      given |= (b[0] << 24) | (b[1] << 16);
	    }
	  else
	    given = (b[0] << 8) | b[1] | (b[2] << 24) | (b[3] << 16);
	}
      else
	given = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | (b[3]);
    }

  if (info->flags & INSN_HAS_RELOC)
    /* If the instruction has a reloc associated with it, then
       the offset field in the instruction will actually be the
       addend for the reloc.  (We are using REL type relocs).
       In such cases, we can ignore the pc when computing
       addresses, since the addend is not currently pc-relative.  */
    pc = 0;

  if (is_thumb)
    status = print_insn_thumb (pc, info, given);
  else
    status = print_insn_arm (pc, info, given);

  return status;
}

int
print_insn_big_arm (pc, info)
     bfd_vma pc;
     struct disassemble_info * info;
{
  return print_insn (pc, info, FALSE);
}

int
print_insn_little_arm (pc, info)
     bfd_vma pc;
     struct disassemble_info * info;
{
  return print_insn (pc, info, TRUE);
}

void
print_arm_disassembler_options (FILE * stream)
{
  int i;

  fprintf (stream, _("\n\
The following ARM specific disassembler options are supported for use with\n\
the -M switch:\n"));

  for (i = NUM_ARM_REGNAMES; i--;)
    fprintf (stream, "  reg-names-%s %*c%s\n",
	     regnames[i].name,
	     (int)(14 - strlen (regnames[i].name)), ' ',
	     regnames[i].description);

  fprintf (stream, "  force-thumb              Assume all insns are Thumb insns\n");
  fprintf (stream, "  no-force-thumb           Examine preceeding label to determine an insn's type\n\n");
}
