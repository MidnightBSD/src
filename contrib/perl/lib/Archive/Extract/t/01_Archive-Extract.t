BEGIN { 
    if( $ENV{PERL_CORE} ) {
        chdir '../lib/Archive/Extract' if -d '../lib/Archive/Extract';
        unshift @INC, '../../..', '../../../..';
    }
}    

BEGIN { chdir 't' if -d 't' };
BEGIN { mkdir 'out' unless -d 'out' };

### left behind, at least on Win32. See core patch #31904
END   { rmtree('out') };        

use strict;
use lib qw[../lib];

use constant IS_WIN32   => $^O eq 'MSWin32' ? 1 : 0;
use constant IS_CYGWIN  => $^O eq 'cygwin'  ? 1 : 0;
use constant IS_VMS     => $^O eq 'VMS'     ? 1 : 0;

use Cwd                         qw[cwd];
use Test::More                  qw[no_plan];
use File::Spec;
use File::Spec::Unix;
use File::Path;
use Data::Dumper;
use File::Basename              qw[basename];
use Module::Load::Conditional   qw[check_install];

### uninitialized value in File::Spec warnings come from A::Zip:
# t/01_Archive-Extract....ok 135/0Use of uninitialized value in concatenation (.) or string at /opt/lib/perl5/5.8.3/File/Spec/Unix.pm line 313.
#         File::Spec::Unix::catpath('File::Spec','','','undef') called at /opt/lib/perl5/site_perl/5.8.3/Archive/Zip.pm line 473
#         Archive::Zip::_asLocalName('') called at /opt/lib/perl5/site_perl/5.8.3/Archive/Zip.pm line 652
#         Archive::Zip::Archive::extractMember('Archive::Zip::Archive=HASH(0x9679c8)','Archive::Zip::ZipFileMember=HASH(0x9678fc)') called at ../lib/Archive/Extract.pm line 753
#         Archive::Extract::_unzip_az('Archive::Extract=HASH(0x966eac)') called at ../lib/Archive/Extract.pm line 674
#         Archive::Extract::_unzip('Archive::Extract=HASH(0x966eac)') called at ../lib/Archive/Extract.pm line 275
#         Archive::Extract::extract('Archive::Extract=HASH(0x966eac)','to','/Users/kane/sources/p4/other/archive-extract/t/out') called at t/01_Archive-Extract.t line 180
#BEGIN { $SIG{__WARN__} = sub { require Carp; Carp::cluck(@_) } };

if ((IS_WIN32 or IS_CYGWIN) && ! $ENV{PERL_CORE}) {
    diag( "Older versions of Archive::Zip may cause File::Spec warnings" );
    diag( "See bug #19713 in rt.cpan.org. It is safe to ignore them" );
}

my $Debug   = $ARGV[0] ? 1 : 0;
my $Me      = basename( $0 );
my $Class   = 'Archive::Extract';
my $Self    = File::Spec->rel2abs( 
                    IS_WIN32 ? &Win32::GetShortPathName( cwd() ) : cwd() 
                );
my $SrcDir  = File::Spec->catdir( $Self,'src' );
my $OutDir  = File::Spec->catdir( $Self,'out' );

use_ok($Class);

### set verbose if debug is on ###
### stupid stupid silly stupid warnings silly! ###
$Archive::Extract::VERBOSE  = $Archive::Extract::VERBOSE = $Debug;
$Archive::Extract::WARN     = $Archive::Extract::WARN    = $Debug ? 1 : 0;

my $tmpl = {
    ### plain files
    'x.bz2' => {    programs    => [qw[bunzip2]],
                    modules     => [qw[IO::Uncompress::Bunzip2]],
                    method      => 'is_bz2',
                    outfile     => 'a',
                },
    'x.tgz' => {    programs    => [qw[gzip tar]],
                    modules     => [qw[Archive::Tar IO::Zlib]],
                    method      => 'is_tgz',
                    outfile     => 'a',
                },
    'x.tar.gz' => { programs    => [qw[gzip tar]],
                    modules     => [qw[Archive::Tar IO::Zlib]],
                    method      => 'is_tgz',
                    outfile     => 'a',
                },
    'x.tar' => {    programs    => [qw[tar]],
                    modules     => [qw[Archive::Tar]],
                    method      => 'is_tar',
                    outfile     => 'a',
                },
    'x.gz'  => {    programs    => [qw[gzip]],
                    modules     => [qw[Compress::Zlib]],
                    method      => 'is_gz',
                    outfile     => 'a',
                },
    'x.Z'   => {    programs    => [qw[uncompress]],
                    modules     => [qw[Compress::Zlib]],
                    method      => 'is_Z',
                    outfile     => 'a',
                },
    'x.zip' => {    programs    => [qw[unzip]],
                    modules     => [qw[Archive::Zip]],
                    method      => 'is_zip',
                    outfile     => 'a',
                },
    'x.jar' => {    programs    => [qw[unzip]],
                    modules     => [qw[Archive::Zip]],
                    method      => 'is_zip',
                    outfile     => 'a',
                },                
    'x.par' => {    programs    => [qw[unzip]],
                    modules     => [qw[Archive::Zip]],
                    method      => 'is_zip',
                    outfile     => 'a',
                },                
    ### with a directory
    'y.tbz'     => {    programs    => [qw[bunzip2 tar]],
                        modules     => [qw[Archive::Tar 
                                           IO::Uncompress::Bunzip2]],
                        method      => 'is_tbz',
                        outfile     => 'z',
                        outdir      => 'y',
                    },
    'y.tar.bz2' => {    programs    => [qw[bunzip2 tar]],
                        modules     => [qw[Archive::Tar 
                                           IO::Uncompress::Bunzip2]],
                        method      => 'is_tbz',
                        outfile     => 'z',
                        outdir      => 'y'
                    },    
    'y.tgz'     => {    programs    => [qw[gzip tar]],
                        modules     => [qw[Archive::Tar IO::Zlib]],
                        method      => 'is_tgz',
                        outfile     => 'z',
                        outdir      => 'y'
                    },
    'y.tar.gz' => {     programs    => [qw[gzip tar]],
                        modules     => [qw[Archive::Tar IO::Zlib]],
                        method      => 'is_tgz',
                        outfile     => 'z',
                        outdir      => 'y'
                    },
    'y.tar' => {    programs    => [qw[tar]],
                    modules     => [qw[Archive::Tar]],
                    method      => 'is_tar',
                    outfile     => 'z',
                    outdir      => 'y'
                },
    'y.zip' => {    programs    => [qw[unzip]],
                    modules     => [qw[Archive::Zip]],
                    method      => 'is_zip',
                    outfile     => 'z',
                    outdir      => 'y'
                },
    'y.par' => {    programs    => [qw[unzip]],
                    modules     => [qw[Archive::Zip]],
                    method      => 'is_zip',
                    outfile     => 'z',
                    outdir      => 'y'
                },
    'y.jar' => {    programs    => [qw[unzip]],
                    modules     => [qw[Archive::Zip]],
                    method      => 'is_zip',
                    outfile     => 'z',
                    outdir      => 'y'
                },
    ### with non-same top dir
    'double_dir.zip' => {
                    programs    => [qw[unzip]],
                    modules     => [qw[Archive::Zip]],
                    method      => 'is_zip',
                    outfile     => 'w',
                    outdir      => 'x'
                },
};

### XXX special case: on older solaris boxes (8),
### bunzip2 is version 0.9.x. Older versions (pre 1),
### only extract files that end in .bz2, and nothing
### else. So remove that test case if we have an older
### bunzip2 :(
{   if( $Class->have_old_bunzip2 ) {
        delete $tmpl->{'y.tbz'};
        diag "Old bunzip2 detected, skipping .tbz test";
    }
}    

### show us the tools IPC::Cmd will use to run binary programs
if( $Debug ) {
    diag( "IPC::Run enabled: $IPC::Cmd::USE_IPC_RUN " );
    diag( "IPC::Run available: " . IPC::Cmd->can_use_ipc_run );
    diag( "IPC::Run vesion: $IPC::Run::VERSION" );
    diag( "IPC::Open3 enabled: $IPC::Cmd::USE_IPC_OPEN3 " );
    diag( "IPC::Open3 available: " . IPC::Cmd->can_use_ipc_open3 );
    diag( "IPC::Open3 vesion: $IPC::Open3::VERSION" );
}

### test all type specifications to new()
### this tests bug #24578: Wrong check for `type' argument
{   my $meth = 'types';

    can_ok( $Class, $meth );

    my @types = $Class->$meth;
    ok( scalar(@types),         "   Got a list of types" );
    
    for my $type ( @types ) {
        my $obj = $Class->new( archive => $Me, type => $type );
        ok( $obj,               "   Object created based on '$type'" );
        ok( !$obj->error,       "       No error logged" );
    }
}    

### XXX whitebox test
### test __get_extract_dir 
SKIP: {   my $meth = '__get_extract_dir';

    ### get the right separator -- File::Spec does clean ups for
    ### paths, so we need to join ourselves.
    my $sep  = [ split '', File::Spec->catfile( 'a', 'b' ) ]->[1];
    
    ### bug #23999: Attempt to generate Makefile.PL gone awry
    ### showed that dirs in the style of './dir/' were reported
    ### to be unpacked in '.' rather than in 'dir'. here we test
    ### for this.
    for my $prefix ( '', '.' ) {
        skip "Prepending ./ to a valid path doesn't give you another valid path on VMS", 2
            if IS_VMS && length($prefix);

        my $dir = basename( $SrcDir );

        ### build a list like [dir, dir/file] and [./dir ./dir/file]
        ### where the dir and file actually exist, which is important
        ### for the method call
        my @files = map { length $prefix 
                                ? join $sep, $prefix, $_
                                : $_
                      } $dir, File::Spec->catfile( $dir, [keys %$tmpl]->[0] );
        
        my $res = $Class->$meth( \@files );
        $res = &Win32::GetShortPathName( $res ) if IS_WIN32;

        ok( $res,               "Found extraction dir '$res'" );
        is( $res, $SrcDir,      "   Is expected dir '$SrcDir'" );
    }        
}

for my $switch (0,1) {

    local $Archive::Extract::PREFER_BIN = $switch;
    diag("Running extract with PREFER_BIN = $Archive::Extract::PREFER_BIN")
        if $Debug;

    for my $archive (keys %$tmpl) {

        diag("Extracting $archive") if $Debug;

        ### check first if we can do the proper

        my $ae = Archive::Extract->new(
                        archive => File::Spec->catfile($SrcDir,$archive) );

        isa_ok( $ae, $Class );

        my $method = $tmpl->{$archive}->{method};
        ok( $ae->$method(),         "Archive type recognized properly" );

    ### 10 tests from here on down ###
    SKIP: {
        my $file        = $tmpl->{$archive}->{outfile};
        my $dir         = $tmpl->{$archive}->{outdir};  # can be undef
        my $rel_path    = File::Spec->catfile( grep { defined } $dir, $file );
        my $abs_path    = File::Spec->catfile( $OutDir, $rel_path );
        my $abs_dir     = File::Spec->catdir( 
                            grep { defined } $OutDir, $dir );
        my $nix_path    = File::Spec::Unix->catfile(
                            grep { defined } $dir, $file );

        ### check if we can run this test ###
        my $pgm_fail; my $mod_fail;
        for my $pgm ( @{$tmpl->{$archive}->{programs}} ) {
            ### no binary extract method
            $pgm_fail++, next unless $pgm;

            ### we dont have the program
            $pgm_fail++ unless $Archive::Extract::PROGRAMS->{$pgm} &&
                               $Archive::Extract::PROGRAMS->{$pgm};

        }

        for my $mod ( @{$tmpl->{$archive}->{modules}} ) {
            ### no module extract method
            $mod_fail++, next unless $mod;

            ### we dont have the module
            $mod_fail++ unless check_install( module => $mod );
        }

        ### where to extract to -- try both dir and file for gz files
        ### XXX test me!
        #my @outs = $ae->is_gz ? ($abs_path, $OutDir) : ($OutDir);
        my @outs = $ae->is_gz || $ae->is_bz2 || $ae->is_Z 
                        ? ($abs_path) 
                        : ($OutDir);

        skip "No binaries or modules to extract ".$archive, 
            (10 * scalar @outs) if $mod_fail && $pgm_fail;

        ### we dont warnings spewed about missing modules, that might
        ### be a problem...
        local $IPC::Cmd::WARN = 0;
        local $IPC::Cmd::WARN = 0;
        
        for my $use_buffer ( IPC::Cmd->can_capture_buffer , 0 ) {

            ### test buffers ###
            my $turn_off = !$use_buffer && !$pgm_fail &&
                            $Archive::Extract::PREFER_BIN;

            ### whitebox test ###
            ### stupid warnings ###
            local $IPC::Cmd::USE_IPC_RUN    = 0 if $turn_off;
            local $IPC::Cmd::USE_IPC_RUN    = 0 if $turn_off;
            local $IPC::Cmd::USE_IPC_OPEN3  = 0 if $turn_off;
            local $IPC::Cmd::USE_IPC_OPEN3  = 0 if $turn_off;


            ### try extracting ###
            for my $to ( @outs ) {

                diag("Extracting to: $to")                  if $Debug;
                diag("Buffers enabled: ".!$turn_off)        if $Debug;
  
                my $rv = $ae->extract( to => $to );
    
                ok( $rv, "extract() for '$archive' reports success");
    
                diag("Extractor was: " . $ae->_extractor)   if $Debug;
    
                SKIP: {
                    my $re  = qr/^No buffer captured/;
                    my $err = $ae->error || '';
              
                    ### skip buffer tests if we dont have buffers or
                    ### explicitly turned them off
                    skip "No buffers available", 7,
                        if ( $turn_off || !IPC::Cmd->can_capture_buffer)
                            && $err =~ $re;

                    ### if we /should/ have buffers, there should be
                    ### no errors complaining we dont have them...
                    unlike( $err, $re,
                                    "No errors capturing buffers" );
    
                    ### might be 1 or 2, depending wether we extracted 
                    ### a dir too
                    my $file_cnt = grep { defined } $file, $dir;
                    is( scalar @{ $ae->files || []}, $file_cnt,
                                    "Found correct number of output files" );
                    is( $ae->files->[-1], $nix_path,
                                    "Found correct output file '$nix_path'" );
    
                    ok( -e $abs_path,
                                    "Output file '$abs_path' exists" );
                    ok( $ae->extract_path,
                                    "Extract dir found" );
                    ok( -d $ae->extract_path,
                                    "Extract dir exists" );
                    is( $ae->extract_path, $abs_dir,
                                    "Extract dir is expected '$abs_dir'" );
                }

                SKIP: {
                    skip "Unlink tests are unreliable on Win32", 3 if IS_WIN32;

                    1 while unlink $abs_path;
                    ok( !(-e $abs_path), "Output file successfully removed" );
        
                    SKIP: {
                        skip "No extract path captured, can't remove paths", 2
                            unless $ae->extract_path;
        
                        ### if something went wrong with determining the out
                        ### path, don't go deleting stuff.. might be Really Bad
                        my $out_re = quotemeta( $OutDir );
                        
                        ### VMS directory layout is different. Craig Berry
                        ### explains:
                        ### the test is trying to determine if C</disk1/foo/bar>
                        ### is part of C</disk1/foo/bar/baz>.  Except in VMS
                        ### syntax, that would mean trying to determine whether
                        ### C<disk1:[foo.bar]> is part of C<disk1:[foo.bar.baz]>
                        ### Because we have both a directory delimiter
                        ### (dot) and a directory spec terminator (right 
                        ### bracket), we have to trim the right bracket from 
                        ### the first one to make it successfully match the
                        ### second one.  Since we're asserting the same truth --
                        ### that one path spec is the leading part of the other
                        ### -- it seems to me ok to have this in the test only.
                        ### 
                        ### so we strip the ']' of the back of the regex
                        $out_re =~ s/\\\]// if IS_VMS; 
                        
                        if( $ae->extract_path !~ /^$out_re/ ) {   
                            ok( 0, "Extractpath WRONG (".$ae->extract_path.")"); 
                            skip(  "Unsafe operation -- skip cleanup!!!" ), 1;
                        }                    
        
                        eval { rmtree( $ae->extract_path ) }; 
                        ok( !$@,        "   rmtree gave no error" );
                        ok( !(-d $ae->extract_path ),
                                        "   Extract dir succesfully removed" );
                    }
                }
            }
        }
    } }
}
