#!./perl -T

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bDevel\/Peek\b/) {
        print "1..0 # Skip: Devel::Peek was not built\n";
        exit 0;
    }
}

BEGIN { require "./test.pl"; }

use Devel::Peek;

plan(48);

our $DEBUG = 0;
open(SAVERR, ">&STDERR") or die "Can't dup STDERR: $!";

sub do_test {
    my $pattern = pop;
    if (open(OUT,">peek$$")) {
	open(STDERR, ">&OUT") or die "Can't dup OUT: $!";
	Dump($_[1]);
        print STDERR "*****\n";
        Dump($_[1]); # second dump to compare with the first to make sure nothing changed.
	open(STDERR, ">&SAVERR") or die "Can't restore STDERR: $!";
	close(OUT);
	if (open(IN, "peek$$")) {
	    local $/;
	    $pattern =~ s/\$ADDR/0x[[:xdigit:]]+/g;
	    $pattern =~ s/\$FLOAT/(?:\\d*\\.\\d+(?:e[-+]\\d+)?|\\d+)/g;
	    # handle DEBUG_LEAKING_SCALARS prefix
	    $pattern =~ s/^(\s*)(SV =.* at )/(?:$1ALLOCATED at .*?\n)?$1$2/mg;

	    $pattern =~ s/^ *\$XSUB *\n/
		($] < 5.009) ? "    XSUB = 0\n    XSUBANY = 0\n" : '';
	    /mge;
	    $pattern =~ s/^ *\$ROOT *\n/
		($] < 5.009) ? "    ROOT = 0x0\n" : '';
	    /mge;
	    $pattern =~ s/^ *\$IVNV *\n/
		($] < 5.009) ? "    IV = 0\n    NV = 0\n" : '';
	    /mge;

	    print $pattern, "\n" if $DEBUG;
	    my ($dump, $dump2) = split m/\*\*\*\*\*\n/, scalar <IN>;
	    print $dump, "\n"    if $DEBUG;
	    like( $dump, qr/\A$pattern\Z/ms );

            local $TODO = $dump2 =~ /OOK/ ? "The hash iterator used in dump.c sets the OOK flag" : undef;
            is($dump2, $dump);

	    close(IN);

            return $1;
	} else {
	    die "$0: failed to open peek$$: !\n";
	}
    } else {
	die "$0: failed to create peek$$: $!\n";
    }
}

our   $a;
our   $b;
my    $c;
local $d = 0;

END {
    1 while unlink("peek$$");
}

do_test( 1,
	$a = "foo",
'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(POK,pPOK\\)
  PV = $ADDR "foo"\\\0
  CUR = 3
  LEN = \\d+'
       );

do_test( 2,
        "bar",
'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(.*POK,READONLY,pPOK\\)
  PV = $ADDR "bar"\\\0
  CUR = 3
  LEN = \\d+');

do_test( 3,
        $b = 123,
'SV = IV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(IOK,pIOK\\)
  IV = 123');

do_test( 4,
        456,
'SV = IV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(.*IOK,READONLY,pIOK\\)
  IV = 456');

do_test( 5,
        $c = 456,
'SV = IV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(PADMY,IOK,pIOK\\)
  IV = 456');

# If perl is built with PERL_PRESERVE_IVUV then maths is done as integers
# where possible and this scalar will be an IV. If NO_PERL_PRESERVE_IVUV then
# maths is done in floating point always, and this scalar will be an NV.
# ([NI]) captures the type, referred to by \1 in this regexp and $type for
# building subsequent regexps.
my $type = do_test( 6,
        $c + $d,
'SV = ([NI])V\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(PADTMP,\1OK,p\1OK\\)
  \1V = 456');

($d = "789") += 0.1;

do_test( 7,
       $d,
'SV = PVNV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(NOK,pNOK\\)
  IV = \d+
  NV = 789\\.(?:1(?:000+\d+)?|0999+\d+)
  PV = $ADDR "789"\\\0
  CUR = 3
  LEN = \\d+');

do_test( 8,
        0xabcd,
'SV = IV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(.*IOK,READONLY,pIOK\\)
  IV = 43981');

do_test( 9,
        undef,
'SV = NULL\\(0x0\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(\\)');

do_test(10,
        \$a,
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PV\\($ADDR\\) at $ADDR
    REFCNT = 2
    FLAGS = \\(POK,pPOK\\)
    PV = $ADDR "foo"\\\0
    CUR = 3
    LEN = \\d+');

my $c_pattern;
if ($type eq 'N') {
  $c_pattern = '
    SV = PVNV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(IOK,NOK,pIOK,pNOK\\)
      IV = 456
      NV = 456
      PV = 0';
} else {
  $c_pattern = '
    SV = IV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(IOK,pIOK\\)
      IV = 456';
}
do_test(11,
       [$b,$c],
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVAV\\($ADDR\\) at $ADDR
    REFCNT = 1
    FLAGS = \\(\\)
    ARRAY = $ADDR
    FILL = 1
    MAX = 1
    ARYLEN = 0x0
    FLAGS = \\(REAL\\)
    Elt No. 0
    SV = IV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(IOK,pIOK\\)
      IV = 123
    Elt No. 1' . $c_pattern);

do_test(12,
       {$b=>$c},
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = 1
    FLAGS = \\(SHAREKEYS\\)
    ARRAY = $ADDR  \\(0:7, 1:1\\)
    hash quality = 100.0%
    KEYS = 1
    FILL = 1
    MAX = 7
    RITER = -1
    EITER = 0x0
    Elt "123" HASH = $ADDR' . $c_pattern);

do_test(13,
        sub(){@_},
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVCV\\($ADDR\\) at $ADDR
    REFCNT = 2
    FLAGS = \\(PADMY,POK,pPOK,ANON,WEAKOUTSIDE\\)
    $IVNV
    PROTOTYPE = ""
    COMP_STASH = $ADDR\\t"main"
    START = $ADDR ===> \\d+
    ROOT = $ADDR
    $XSUB
    GVGV::GV = $ADDR\\t"main" :: "__ANON__[^"]*"
    FILE = ".*\\b(?i:peek\\.t)"
    DEPTH = 0
(?:    MUTEXP = $ADDR
    OWNER = $ADDR
)?    FLAGS = 0x90
    OUTSIDE_SEQ = \\d+
    PADLIST = $ADDR
    PADNAME = $ADDR\\($ADDR\\) PAD = $ADDR\\($ADDR\\)
    OUTSIDE = $ADDR \\(MAIN\\)');

do_test(14,
        \&do_test,
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVCV\\($ADDR\\) at $ADDR
    REFCNT = (3|4)
    FLAGS = \\(\\)
    $IVNV
    COMP_STASH = $ADDR\\t"main"
    START = $ADDR ===> \\d+
    ROOT = $ADDR
    $XSUB
    GVGV::GV = $ADDR\\t"main" :: "do_test"
    FILE = ".*\\b(?i:peek\\.t)"
    DEPTH = 1
(?:    MUTEXP = $ADDR
    OWNER = $ADDR
)?    FLAGS = 0x0
    OUTSIDE_SEQ = \\d+
    PADLIST = $ADDR
    PADNAME = $ADDR\\($ADDR\\) PAD = $ADDR\\($ADDR\\)
       \\d+\\. $ADDR<\\d+> \\(\\d+,\\d+\\) "\\$pattern"
      \\d+\\. $ADDR<\\d+> FAKE "\\$DEBUG" flags=0x0 index=0
      \\d+\\. $ADDR<\\d+> \\(\\d+,\\d+\\) "\\$dump"
      \\d+\\. $ADDR<\\d+> \\(\\d+,\\d+\\) "\\$dump2"
    OUTSIDE = $ADDR \\(MAIN\\)');

do_test(15,
        qr(tic),
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVMG\\($ADDR\\) at $ADDR
    REFCNT = 1
    FLAGS = \\(OBJECT,SMG\\)
    IV = 0
    NV = 0
    PV = 0
    MAGIC = $ADDR
      MG_VIRTUAL = $ADDR
      MG_TYPE = PERL_MAGIC_qr\(r\)
      MG_OBJ = $ADDR
        PAT = "\(\?-xism:tic\)"
        REFCNT = 2
    STASH = $ADDR\\t"Regexp"');

do_test(16,
        (bless {}, "Tac"),
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = 1
    FLAGS = \\(OBJECT,SHAREKEYS\\)
    STASH = $ADDR\\t"Tac"
    ARRAY = 0x0
    KEYS = 0
    FILL = 0
    MAX = 7
    RITER = -1
    EITER = 0x0');

do_test(17,
	*a,
'SV = PVGV\\($ADDR\\) at $ADDR
  REFCNT = 5
  FLAGS = \\(MULTI(?:,IN_PAD)?\\)
  NAME = "a"
  NAMELEN = 1
  GvSTASH = $ADDR\\t"main"
  GP = $ADDR
    SV = $ADDR
    REFCNT = 1
    IO = 0x0
    FORM = 0x0  
    AV = 0x0
    HV = 0x0
    CV = 0x0
    CVGEN = 0x0
    LINE = \\d+
    FILE = ".*\\b(?i:peek\\.t)"
    FLAGS = $ADDR
    EGV = $ADDR\\t"a"');

if (ord('A') == 193) {
do_test(18,
	chr(256).chr(0).chr(512),
'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\((?:PADTMP,)?POK,READONLY,pPOK,UTF8\\)
  PV = $ADDR "\\\214\\\101\\\0\\\235\\\101"\\\0 \[UTF8 "\\\x\{100\}\\\x\{0\}\\\x\{200\}"\]
  CUR = 5
  LEN = \\d+');
} else {
do_test(18,
	chr(256).chr(0).chr(512),
'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\((?:PADTMP,)?POK,READONLY,pPOK,UTF8\\)
  PV = $ADDR "\\\304\\\200\\\0\\\310\\\200"\\\0 \[UTF8 "\\\x\{100\}\\\x\{0\}\\\x\{200\}"\]
  CUR = 5
  LEN = \\d+');
}

if (ord('A') == 193) {
do_test(19,
	{chr(256)=>chr(512)},
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = 1
    FLAGS = \\(SHAREKEYS,HASKFLAGS\\)
    ARRAY = $ADDR  \\(0:7, 1:1\\)
    hash quality = 100.0%
    KEYS = 1
    FILL = 1
    MAX = 7
    RITER = -1
    EITER = $ADDR
    Elt "\\\214\\\101" \[UTF8 "\\\x\{100\}"\] HASH = $ADDR
    SV = PV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(POK,pPOK,UTF8\\)
      PV = $ADDR "\\\235\\\101"\\\0 \[UTF8 "\\\x\{200\}"\]
      CUR = 2
      LEN = \\d+');
} else {
do_test(19,
	{chr(256)=>chr(512)},
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = 1
    FLAGS = \\(SHAREKEYS,HASKFLAGS\\)
    ARRAY = $ADDR  \\(0:7, 1:1\\)
    hash quality = 100.0%
    KEYS = 1
    FILL = 1
    MAX = 7
    RITER = -1
    EITER = $ADDR
    Elt "\\\304\\\200" \[UTF8 "\\\x\{100\}"\] HASH = $ADDR
    SV = PV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(POK,pPOK,UTF8\\)
      PV = $ADDR "\\\310\\\200"\\\0 \[UTF8 "\\\x\{200\}"\]
      CUR = 2
      LEN = \\d+');
}

my $x="";
$x=~/.??/g;
do_test(20,
        $x,
'SV = PVMG\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(PADMY,SMG,POK,pPOK\\)
  IV = 0
  NV = 0
  PV = $ADDR ""\\\0
  CUR = 0
  LEN = \d+
  MAGIC = $ADDR
    MG_VIRTUAL = &PL_vtbl_mglob
    MG_TYPE = PERL_MAGIC_regex_global\\(g\\)
    MG_FLAGS = 0x01
      MINMATCH');

#
# TAINTEDDIR is not set on: OS2, AMIGAOS, WIN32, MSDOS
# environment variables may be invisibly case-forced, hence the (?i:PATH)
# C<scalar(@ARGV)> is turned into an IV on VMS hence the (?:IV)?
# VMS is setting FAKE and READONLY flags.  What VMS uses for storing
# ENV hashes is also not always null terminated.
#
do_test(21,
        $ENV{PATH}=@ARGV,  # scalar(@ARGV) is a handy known tainted value
'SV = PVMG\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(GMG,SMG,RMG,pIOK,pPOK\\)
  IV = 0
  NV = 0
  PV = $ADDR "0"\\\0
  CUR = 1
  LEN = \d+
  MAGIC = $ADDR
    MG_VIRTUAL = &PL_vtbl_envelem
    MG_TYPE = PERL_MAGIC_envelem\\(e\\)
(?:    MG_FLAGS = 0x01
      TAINTEDDIR
)?    MG_LEN = -?\d+
    MG_PTR = $ADDR (?:"(?i:PATH)"|=> HEf_SVKEY
    SV = PV(?:IV)?\\($ADDR\\) at $ADDR
      REFCNT = \d+
      FLAGS = \\(TEMP,POK,(?:FAKE,READONLY,)?pPOK\\)
(?:      IV = 0
)?      PV = $ADDR "(?i:PATH)"(?:\\\0)?
      CUR = \d+
      LEN = \d+)
  MAGIC = $ADDR
    MG_VIRTUAL = &PL_vtbl_taint
    MG_TYPE = PERL_MAGIC_taint\\(t\\)');

# blessed refs
do_test(22,
	bless(\\undef, 'Foobar'),
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVMG\\($ADDR\\) at $ADDR
    REFCNT = 2
    FLAGS = \\(OBJECT,ROK\\)
    IV = -?\d+
    NV = $FLOAT
    RV = $ADDR
    SV = NULL\\(0x0\\) at $ADDR
      REFCNT = \d+
      FLAGS = \\(READONLY\\)
    PV = $ADDR ""
    CUR = 0
    LEN = 0
    STASH = $ADDR\s+"Foobar"');

# Constant subroutines

sub const () {
    "Perl rules";
}

do_test(23,
	\&const,
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVCV\\($ADDR\\) at $ADDR
    REFCNT = (2)
    FLAGS = \\(POK,pPOK,CONST\\)
    $IVNV
    PROTOTYPE = ""
    COMP_STASH = 0x0
    $ROOT
    XSUB = $ADDR
    XSUBANY = $ADDR \\(CONST SV\\)
    SV = PV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(.*POK,READONLY,pPOK\\)
      PV = $ADDR "Perl rules"\\\0
      CUR = 10
      LEN = \\d+
    GVGV::GV = $ADDR\\t"main" :: "const"
    FILE = ".*\\b(?i:peek\\.t)"
    DEPTH = 0
(?:    MUTEXP = $ADDR
    OWNER = $ADDR
)?    FLAGS = 0xc00
    OUTSIDE_SEQ = 0
    PADLIST = 0x0
    OUTSIDE = 0x0 \\(null\\)');	

# isUV should show on PVMG
do_test(24,
	do { my $v = $1; $v = ~0; $v },
'SV = PVMG\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(IOK,pIOK,IsUV\\)
  UV = \d+
  NV = 0
  PV = 0');
