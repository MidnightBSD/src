BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir '../lib/Archive/Tar' if -d '../lib/Archive/Tar';
    }       
    use lib '../../..';
}

BEGIN { chdir 't' if -d 't' }

use Test::More      'no_plan';
use File::Basename  'basename';
use strict;
use lib '../lib';

my $NO_UNLINK   = @ARGV ? 1 : 0;

my $Class       = 'Archive::Tar';
my $FileClass   = $Class . '::File';

use_ok( $Class );
use_ok( $FileClass );

### bug #13636
### tests for @longlink behaviour on files that have a / at the end
### of their shortened path, making them appear to be directories
{   ok( 1,                      "Testing bug 13636" );

    ### dont use the prefix, otherwise A::T will not use @longlink
    ### encoding style
    local $Archive::Tar::DO_NOT_USE_PREFIX = 1;
    local $Archive::Tar::DO_NOT_USE_PREFIX = 1;
    
    my $dir =   'Catalyst-Helper-Controller-Scaffold-HTML-Template-0_03/' . 
                'lib/Catalyst/Helper/Controller/Scaffold/HTML/';
    my $file =  'Template.pm';
    my $out =   $$ . '.tar';
    
    ### first create the file
    {   my $tar = $Class->new;
        
        isa_ok( $tar, $Class,   "   Object" );
        ok( $tar->add_data( $dir.$file => $$ ),
                                "       Added long file" );
        
        ok( $tar->write($out),  "       File written to $out" );
    }
    
    ### then read it back in
    {   my $tar = $Class->new;
        isa_ok( $tar, $Class,   "   Object" );
        ok( $tar->read( $out ), "       Read in $out again" );
        
        my @files = $tar->get_files;
        is( scalar(@files), 1,  "       Only 1 entry found" );
        
        my $entry = shift @files;
        ok( $entry->is_file,    "       Entry is a file" );
        is( $entry->name, $dir.$file,
                                "       With the proper name" );
    }                                
    
    ### remove the file
    unless( $NO_UNLINK ) { 1 while unlink $out }
}    

### bug #14922
### There's a bug in Archive::Tar that causes a file like: foo/foo.txt 
### to be stored in the tar file as: foo/.txt
### XXX could not be reproduced in 1.26 -- leave test to be sure
{   ok( 1,                      "Testing bug 14922" );

    my $dir     = $$ . '/';
    my $file    = $$ . '.txt';
    my $out     = $$ . '.tar';
    
    ### first create the file
    {   my $tar = $Class->new;
        
        isa_ok( $tar, $Class,   "   Object" );
        ok( $tar->add_data( $dir.$file => $$ ),
                                "       Added long file" );
        
        ok( $tar->write($out),  "       File written to $out" );
    }

    ### then read it back in
    {   my $tar = $Class->new;
        isa_ok( $tar, $Class,   "   Object" );
        ok( $tar->read( $out ), "       Read in $out again" );
        
        my @files = $tar->get_files;
        is( scalar(@files), 1,  "       Only 1 entry found" );
        
        my $entry = shift @files;
        ok( $entry->is_file,    "       Entry is a file" );
        is( $entry->full_path, $dir.$file,
                                "       With the proper name" );
    }                                
    
    ### remove the file
    unless( $NO_UNLINK ) { 1 while unlink $out }
}    
    
### bug #30380: directory traversal vulnerability in Archive-Tar    
### Archive::Tar allowed files to be extracted to a dir outside
### it's cwd(), effectively allowing you to overwrite any files
### on the system, given the right permissions.
{   ok( 1,                      "Testing bug 30880" );

    my $tar = $Class->new;
    isa_ok( $tar, $Class,       "   Object" );    
    
    ### absolute paths are already taken care of. Only relative paths
    ### matter
    my $in_file     = basename($0);
    my $out_file    = '../' . $in_file . ".$$";
    
    ok( $tar->add_files( $in_file ), 
                                "       Added '$in_file'" );
    ok( $tar->rename( $in_file, $out_file ),
                                "       Renamed to '$out_file'" );
    
    ### first, test with strict extract permissions on
TODO:
    {   local $Archive::Tar::INSECURE_EXTRACT_MODE = 0;

        ### we quell the error on STDERR
        local $Archive::Tar::WARN = 0;
        local $Archive::Tar::WARN = 0;

        ok( 1,                  "   Extracting in secure mode" );

        ok( ! $tar->extract_file( $out_file ),
                                "       File not extracted" );
        ok( ! -e $out_file,     "       File '$out_file' does not exist" );
    
        ok( $tar->error,        "       Error message stored" );

        local $TODO = 'Exposed unrelated filespec handling bugs on VMS' if $^O eq 'VMS';

        like( $tar->error, qr/attempting to leave/,
                                "           Proper violation detected" );
    }
    
    ### now disable those
TODO:
    {   local $Archive::Tar::INSECURE_EXTRACT_MODE = 1;
        ok( 1,                  "   Extracting in insecure mode" );
    
        local $TODO = 'Exposed unrelated filespec handling bugs on VMS' if $^O eq 'VMS';

        ok( $tar->extract_file( $out_file ),
                                "       File extracted" );
        ok( -e $out_file,       "       File '$out_file' exists" );
        
        ### and clean up
        unless( $NO_UNLINK ) { 1 while unlink $out_file };
    }    
    

}
