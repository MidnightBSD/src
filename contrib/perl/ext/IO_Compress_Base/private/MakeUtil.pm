package MakeUtil ;
package main ;

use strict ;

use Config qw(%Config);
use File::Copy;


BEGIN
{
    eval { require File::Spec::Functions ; File::Spec::Functions->import() } ;
    if ($@)
    {
        *catfile = sub { return "$_[0]/$_[1]" }
    }
}

require VMS::Filespec if $^O eq 'VMS';


unless($ENV{PERL_CORE}) {
    $ENV{PERL_CORE} = 1 if grep { $_ eq 'PERL_CORE=1' } @ARGV;
}

$ENV{SKIP_FOR_CORE} = 1 if $ENV{PERL_CORE} || $ENV{MY_PERL_CORE} ;



sub MY::libscan
{
    my $self = shift;
    my $path = shift;

    return undef
        if $path =~ /(~|\.bak|_bak)$/ ||
           $path =~ /\..*\.sw(o|p)$/  ||
           $path =~ /\B\.svn\b/;

    return $path;
}

sub MY::postamble 
{
    return ''
        if $ENV{PERL_CORE} ;

    my @files = getPerlFiles('MANIFEST');

    my $postamble = '

MyTrebleCheck:
	@echo Checking for $$^W in files: '. "@files" . '
	@perl -ne \'						\
	    exit 1 if /^\s*local\s*\(\s*\$$\^W\s*\)/;		\
         \' ' . " @files || " . '				\
	(echo found unexpected $$^W ; exit 1)
	@echo All is ok.

';

    return $postamble;
}

sub getPerlFiles
{
    my @manifests = @_ ;

    my @files = ();

    for my $manifest (@manifests)
    {
        my $prefix = './';

        $prefix = $1
            if $manifest =~ m#^(.*/)#;

        open M, "<$manifest"
            or die "Cannot open '$manifest': $!\n";
        while (<M>)
        {
            chomp ;
            next if /^\s*#/ || /^\s*$/ ;

            s/^\s+//;
            s/\s+$//;

            /^(\S+)\s*(.*)$/;

            my ($file, $rest) = ($1, $2);

            if ($file =~ /\.(pm|pl|t)$/ and $file !~ /MakeUtil.pm/)
            {
                push @files, "$prefix$file";
            }
            elsif ($rest =~ /perl/i)
            {
                push @files, "$prefix$file";
            }

        }
        close M;
    }

    return @files;
}

sub UpDowngrade
{
    return if defined $ENV{TipTop};

    my @files = @_ ;

    # our and use bytes/utf8 is stable from 5.6.0 onward
    # warnings is stable from 5.6.1 onward

    # Note: this code assumes that each statement it modifies is not
    #       split across multiple lines.


    my $warn_sub = '';
    my $our_sub = '' ;

    my $upgrade ;
    my $downgrade ;
    my $do_downgrade ;

    my $caller = (caller(1))[3] || '';

    if ($caller =~ /downgrade/)
    {
        $downgrade = 1;
    }
    elsif ($caller =~ /upgrade/)
    {
        $upgrade = 1;
    }
    else
    {
        $do_downgrade = 1
            if $] < 5.006001 ;
    }

#    else
#    {
#        my $opt = shift @ARGV || '' ;
#        $upgrade = ($opt =~ /^-upgrade/i);
#        $downgrade = ($opt =~ /^-downgrade/i);
#        push @ARGV, $opt unless $downgrade || $upgrade;
#    }


    if ($downgrade || $do_downgrade) {
        # From: use|no warnings "blah"
        # To:   local ($^W) = 1; # use|no warnings "blah"
        $warn_sub = sub {
                            s/^(\s*)(no\s+warnings)/${1}local (\$^W) = 0; #$2/ ;
                            s/^(\s*)(use\s+warnings)/${1}local (\$^W) = 1; #$2/ ;
                        };
    }
    #elsif ($] >= 5.006001 || $upgrade) {
    elsif ($upgrade) {
        # From: local ($^W) = 1; # use|no warnings "blah"
        # To:   use|no warnings "blah"
        $warn_sub = sub {
            s/^(\s*)local\s*\(\$\^W\)\s*=\s*\d+\s*;\s*#\s*((no|use)\s+warnings.*)/$1$2/ ;
          };
    }

    if ($downgrade || $do_downgrade) {
        $our_sub = sub {
	    if ( /^(\s*)our\s+\(\s*([^)]+\s*)\)/ ) {
                my $indent = $1;
                my $vars = join ' ', split /\s*,\s*/, $2;
                $_ = "${indent}use vars qw($vars);\n";
            }
	    elsif ( /^(\s*)((use|no)\s+(bytes|utf8)\s*;.*)$/)
            {
                $_ = "$1# $2\n";
            }
          };
    }
    #elsif ($] >= 5.006000 || $upgrade) {
    elsif ($upgrade) {
        $our_sub = sub {
	    if ( /^(\s*)use\s+vars\s+qw\((.*?)\)/ ) {
                my $indent = $1;
                my $vars = join ', ', split ' ', $2;
                $_ = "${indent}our ($vars);\n";
            }
	    elsif ( /^(\s*)#\s*((use|no)\s+(bytes|utf8)\s*;.*)$/)
            {
                $_ = "$1$2\n";
            }
          };
    }

    if (! $our_sub && ! $warn_sub) {
        warn "Up/Downgrade not needed.\n";
	if ($upgrade || $downgrade)
          { exit 0 }
        else
          { return }
    }

    foreach (@files) {
        #if (-l $_ )
          { doUpDown($our_sub, $warn_sub, $_) }
          #else  
          #{ doUpDownViaCopy($our_sub, $warn_sub, $_) }
    }

    warn "Up/Downgrade complete.\n" ;
    exit 0 if $upgrade || $downgrade;

}


sub doUpDown
{
    my $our_sub = shift;
    my $warn_sub = shift;

    return if -d $_[0];

    local ($^I) = ($^O eq 'VMS') ? "_bak" : ".bak";
    local (@ARGV) = shift;
 
    while (<>)
    {
        print, last if /^__(END|DATA)__/ ;

        &{ $our_sub }() if $our_sub ;
        &{ $warn_sub }() if $warn_sub ;
        print ;
    }

    return if eof ;

    while (<>)
      { print }
}

sub doUpDownViaCopy
{
    my $our_sub = shift;
    my $warn_sub = shift;
    my $file     = shift ;

    use File::Copy ;

    return if -d $file ;

    my $backup = $file . ($^O eq 'VMS') ? "_bak" : ".bak";

    copy($file, $backup)
        or die "Cannot copy $file to $backup: $!";

    my @keep = ();

    {
        open F, "<$file"
            or die "Cannot open $file: $!\n" ;
        while (<F>)
        {
            if (/^__(END|DATA)__/)
            {
                push @keep, $_;
                last ;
            }
            
            &{ $our_sub }() if $our_sub ;
            &{ $warn_sub }() if $warn_sub ;
            push @keep, $_;
        }

        if (! eof F)
        {
            while (<F>)
              { push @keep, $_ }
        }
        close F;
    }

    {
        open F, ">$file"
            or die "Cannot open $file: $!\n";
        print F @keep ;
        close F;
    }
}

package MakeUtil ;

1;


