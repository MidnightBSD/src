#!./perl
use strict;
use Cwd;

my %Expect_File = (); # what we expect for $_ 
my %Expect_Name = (); # what we expect for $File::Find::name/fullname
my %Expect_Dir  = (); # what we expect for $File::Find::dir
my $symlink_exists = eval { symlink("",""); 1 };
my ($warn_msg, @files, $file);


BEGIN {
    require File::Spec;
    chdir 't' if -d 't';
    # May be doing dynamic loading while @INC is all relative
    unshift @INC => File::Spec->rel2abs('../lib');

    $SIG{'__WARN__'} = sub { $warn_msg = $_[0]; warn "# $_[0]"; }
}

my $test_count = 85;
$test_count += 119 if $symlink_exists;
$test_count += 26 if $^O eq 'MSWin32';
$test_count += 2 if $^O eq 'MSWin32' and $symlink_exists;

print "1..$test_count\n";
#if ( $symlink_exists ) { print "1..199\n"; }
#else                   { print "1..85\n";  }

my $orig_dir = cwd();

# Uncomment this to see where File::Find is chdir'ing to.  Helpful for
# debugging its little jaunts around the filesystem.
# BEGIN {
#     use Cwd;
#     *CORE::GLOBAL::chdir = sub ($) { 
#         my($file, $line) = (caller)[1,2];
#
#         printf "# cwd:      %s\n", cwd();
#         print "# chdir: @_ from $file at $line\n";
#         my($return) = CORE::chdir($_[0]);
#         printf "# newcwd:   %s\n", cwd();
#
#         return $return;
#     };
# }


BEGIN {
    use File::Spec;
    if ($^O eq 'MSWin32' || $^O eq 'cygwin' || $^O eq 'VMS')
     {
      # This is a hack - at present File::Find does not produce native names on 
      # Win32 or VMS, so force File::Spec to use Unix names.
      # must be set *before* importing File::Find
      require File::Spec::Unix;
      @File::Spec::ISA = 'File::Spec::Unix';
     }
     require File::Find;
     import File::Find;
}

cleanup();

$::count_commonsense = 0;
find({wanted => sub { ++$::count_commonsense if $_ eq 'commonsense.t'; } },
   File::Spec->curdir);
if ($::count_commonsense == 1) {
  print "ok 1\n";
} else {
  print "not ok 1 # found $::count_commonsense files named 'commonsense.t'\n";
}

$::count_commonsense = 0;
finddepth({wanted => sub { ++$::count_commonsense if $_ eq 'commonsense.t'; } },
	File::Spec->curdir);
if ($::count_commonsense == 1) {
  print "ok 2\n";
} else {
  print "not ok 2 # found $::count_commonsense files named 'commonsense.t'\n";
}

my $case = 2;
my $FastFileTests_OK = 0;

sub cleanup {
    chdir($orig_dir);
    my $need_updir = 0;
    if (-d dir_path('for_find')) {
        $need_updir = 1 if chdir(dir_path('for_find'));
    }
    if (-d dir_path('fa')) {
	unlink file_path('fa', 'fa_ord'),
	       file_path('fa', 'fsl'),
	       file_path('fa', 'faa', 'faa_ord'),
	       file_path('fa', 'fab', 'fab_ord'),
	       file_path('fa', 'fab', 'faba', 'faba_ord'),
               file_path('fa', 'fac', 'faca'),
	       file_path('fb', 'fb_ord'),
	       file_path('fb', 'fba', 'fba_ord'),
               file_path('fb', 'fbc', 'fbca');
	rmdir dir_path('fa', 'faa');
	rmdir dir_path('fa', 'fab', 'faba');
	rmdir dir_path('fa', 'fab');
	rmdir dir_path('fa', 'fac');
	rmdir dir_path('fa');
	rmdir dir_path('fb', 'fba');
	rmdir dir_path('fb', 'fbc');
	rmdir dir_path('fb');
    }
    if ($need_updir) {
        my $updir = $^O eq 'VMS' ? File::Spec::VMS->updir() : File::Spec->updir;
        chdir($updir);
    }
    if (-d dir_path('for_find')) {
	rmdir dir_path('for_find') or print "# Can't rmdir for_find: $!\n";
    }
}

END {
    cleanup();
}

sub Check($) {
    $case++;
    if ($_[0]) { print "ok $case\n"; }
    else       { print "not ok $case\n"; }
}

sub CheckDie($) {
    $case++;
    if ($_[0]) { print "ok $case\n"; }
    else { print "not ok $case\n $!\n"; exit 0; }
}

sub touch {
    CheckDie( open(my $T,'>',$_[0]) );
}

sub MkDir($$) {
    CheckDie( mkdir($_[0],$_[1]) );
}

sub wanted_File_Dir {
    printf "# \$File::Find::dir => '$File::Find::dir'\t\$_ => '$_'\n";
    s#\.$## if ($^O eq 'VMS' && $_ ne '.');
    s/(.dir)?$//i if ($^O eq 'VMS' && -d _);
    Check( $Expect_File{$_} );
    if ( $FastFileTests_OK ) {
        delete $Expect_File{ $_} 
          unless ( $Expect_Dir{$_} && ! -d _ );
    } else {
        delete $Expect_File{$_} 
          unless ( $Expect_Dir{$_} && ! -d $_ );
    }
}

sub wanted_File_Dir_prune {
    &wanted_File_Dir;
    $File::Find::prune=1 if  $_ eq 'faba';
}

sub wanted_Name {
    my $n = $File::Find::name;
    $n =~ s#\.$## if ($^O eq 'VMS' && $n ne '.');
    print "# \$File::Find::name => '$n'\n";
    my $i = rindex($n,'/');
    my $OK = exists($Expect_Name{$n});
    if ( $OK ) {
	$OK= exists($Expect_Name{substr($n,0,$i)})  if $i >= 0;    
    }
    Check($OK);
    delete $Expect_Name{$n};
}

sub wanted_File {
    print "# \$_ => '$_'\n";
    s#\.$## if ($^O eq 'VMS' && $_ ne '.');
    my $i = rindex($_,'/');
    my $OK = exists($Expect_File{ $_});
    if ( $OK ) {
	$OK= exists($Expect_File{ substr($_,0,$i)})  if $i >= 0;
    }
    Check($OK);
    delete $Expect_File{ $_};
}

sub simple_wanted {
    print "# \$File::Find::dir => '$File::Find::dir'\n";
    print "# \$_ => '$_'\n";
}

sub noop_wanted {}

sub my_preprocess {
    @files = @_;
    print "# --preprocess--\n";
    print "#   \$File::Find::dir => '$File::Find::dir' \n";
    foreach $file (@files) {
        $file =~ s/\.(dir)?$// if $^O eq 'VMS';
        print "#   $file \n";
        delete $Expect_Dir{ $File::Find::dir }->{$file};
    }
    print "# --end preprocess--\n";
    Check(scalar(keys %{$Expect_Dir{ $File::Find::dir }}) == 0);
    if (scalar(keys %{$Expect_Dir{ $File::Find::dir }}) == 0) {
        delete $Expect_Dir{ $File::Find::dir }
    }
    return @files;
}

sub my_postprocess {
    print "# postprocess: \$File::Find::dir => '$File::Find::dir' \n";
    delete $Expect_Dir{ $File::Find::dir};
}


# Use dir_path() to specify a directory path that's expected for
# $File::Find::dir (%Expect_Dir). Also use it in file operations like
# chdir, rmdir etc.
#
# dir_path() concatenates directory names to form a *relative*
# directory path, independent from the platform it's run on, although
# there are limitations. Don't try to create an absolute path,
# because that may fail on operating systems that have the concept of
# volume names (e.g. Mac OS). As a special case, you can pass it a "." 
# as first argument, to create a directory path like "./fa/dir". If there's
# no second argument, this function will return "./"

sub dir_path {
    my $first_arg = shift @_;

    if ($first_arg eq '.') {
	return './' unless @_;
	my $path = File::Spec->catdir(@_);
	# add leading "./"
	$path = "./$path";
	return $path;
    } else { # $first_arg ne '.'
        return $first_arg unless @_; # return plain filename
        return File::Spec->catdir($first_arg, @_); # relative path
    }
}


# Use topdir() to specify a directory path that you want to pass to
# find/finddepth. Historically topdir() differed on Mac OS classic.

*topdir = \&dir_path;


# Use file_path() to specify a file path that's expected for $_
# (%Expect_File). Also suitable for file operations like unlink etc.
#
# file_path() concatenates directory names (if any) and a filename to
# form a *relative* file path (the last argument is assumed to be a
# file). It's independent from the platform it's run on, although
# there are limitations. As a special case, you can pass it a "." as 
# first argument, to create a file path like "./fa/file" on operating 
# systems. If there's no second argument, this function will return the
# string "./" 

sub file_path {
    my $first_arg = shift @_;

    if ($first_arg eq '.') {
	return './' unless @_;
	my $path = File::Spec->catfile(@_);
	# add leading "./" 
	$path = "./$path"; 
	return $path;
    } else { # $first_arg ne '.'
        return $first_arg unless @_; # return plain filename
        return File::Spec->catfile($first_arg, @_); # relative path
    }
}


# Use file_path_name() to specify a file path that's expected for
# $File::Find::Name (%Expect_Name). Note: When the no_chdir => 1
# option is in effect, $_ is the same as $File::Find::Name. In that
# case, also use this function to specify a file path that's expected
# for $_.
#
# Historically file_path_name differed on Mac OS classic.

*file_path_name = \&file_path;



MkDir( dir_path('for_find'), 0770 );
CheckDie(chdir( dir_path('for_find')));
MkDir( dir_path('fa'), 0770 );
MkDir( dir_path('fb'), 0770  );
touch( file_path('fb', 'fb_ord') );
MkDir( dir_path('fb', 'fba'), 0770  );
touch( file_path('fb', 'fba', 'fba_ord') );
CheckDie( symlink('../fb','fa/fsl') ) if $symlink_exists;
touch( file_path('fa', 'fa_ord') );

MkDir( dir_path('fa', 'faa'), 0770  );
touch( file_path('fa', 'faa', 'faa_ord') );
MkDir( dir_path('fa', 'fab'), 0770  );
touch( file_path('fa', 'fab', 'fab_ord') );
MkDir( dir_path('fa', 'fab', 'faba'), 0770  );
touch( file_path('fa', 'fab', 'faba', 'faba_ord') );


%Expect_File = (File::Spec->curdir => 1, file_path('fsl') => 1,
                file_path('fa_ord') => 1, file_path('fab') => 1,
                file_path('fab_ord') => 1, file_path('faba') => 1,
                file_path('faa') => 1, file_path('faa_ord') => 1);

delete $Expect_File{ file_path('fsl') } unless $symlink_exists;
%Expect_Name = ();

%Expect_Dir = ( dir_path('fa') => 1, dir_path('faa') => 1,
                dir_path('fab') => 1, dir_path('faba') => 1,
                dir_path('fb') => 1, dir_path('fba') => 1);

delete @Expect_Dir{ dir_path('fb'), dir_path('fba') } unless $symlink_exists;
File::Find::find( {wanted => \&wanted_File_Dir_prune}, topdir('fa') ); 
Check( scalar(keys %Expect_File) == 0 );


print "# check re-entrancy\n";

%Expect_File = (File::Spec->curdir => 1, file_path('fsl') => 1,
                file_path('fa_ord') => 1, file_path('fab') => 1,
                file_path('fab_ord') => 1, file_path('faba') => 1,
                file_path('faa') => 1, file_path('faa_ord') => 1);

delete $Expect_File{ file_path('fsl') } unless $symlink_exists;
%Expect_Name = ();

%Expect_Dir = ( dir_path('fa') => 1, dir_path('faa') => 1,
                dir_path('fab') => 1, dir_path('faba') => 1,
                dir_path('fb') => 1, dir_path('fba') => 1);

delete @Expect_Dir{ dir_path('fb'), dir_path('fba') } unless $symlink_exists;

File::Find::find( {wanted => sub { wanted_File_Dir_prune();
                                    File::Find::find( {wanted => sub
                                    {} }, File::Spec->curdir ); } },
                                    topdir('fa') );

Check( scalar(keys %Expect_File) == 0 ); 


# no_chdir is in effect, hence we use file_path_name to specify the expected paths for %Expect_File

%Expect_File = (file_path_name('fa') => 1,
		file_path_name('fa', 'fsl') => 1,
                file_path_name('fa', 'fa_ord') => 1,
                file_path_name('fa', 'fab') => 1,
		file_path_name('fa', 'fab', 'fab_ord') => 1,
		file_path_name('fa', 'fab', 'faba') => 1,
		file_path_name('fa', 'fab', 'faba', 'faba_ord') => 1,
		file_path_name('fa', 'faa') => 1,
                file_path_name('fa', 'faa', 'faa_ord') => 1,);

delete $Expect_File{ file_path_name('fa', 'fsl') } unless $symlink_exists;
%Expect_Name = ();

%Expect_Dir = (dir_path('fa') => 1,
	       dir_path('fa', 'faa') => 1,
               dir_path('fa', 'fab') => 1,
	       dir_path('fa', 'fab', 'faba') => 1,
	       dir_path('fb') => 1,
	       dir_path('fb', 'fba') => 1);

delete @Expect_Dir{ dir_path('fb'), dir_path('fb', 'fba') }
    unless $symlink_exists;

File::Find::find( {wanted => \&wanted_File_Dir, no_chdir => 1},
		  topdir('fa') ); Check( scalar(keys %Expect_File) == 0 );


%Expect_File = ();

%Expect_Name = (File::Spec->curdir => 1,
		file_path_name('.', 'fa') => 1,
                file_path_name('.', 'fa', 'fsl') => 1,
                file_path_name('.', 'fa', 'fa_ord') => 1,
                file_path_name('.', 'fa', 'fab') => 1,
                file_path_name('.', 'fa', 'fab', 'fab_ord') => 1,
                file_path_name('.', 'fa', 'fab', 'faba') => 1,
                file_path_name('.', 'fa', 'fab', 'faba', 'faba_ord') => 1,
                file_path_name('.', 'fa', 'faa') => 1,
                file_path_name('.', 'fa', 'faa', 'faa_ord') => 1,
                file_path_name('.', 'fb') => 1,
		file_path_name('.', 'fb', 'fba') => 1,
		file_path_name('.', 'fb', 'fba', 'fba_ord') => 1,
		file_path_name('.', 'fb', 'fb_ord') => 1);

delete $Expect_Name{ file_path('.', 'fa', 'fsl') } unless $symlink_exists;
%Expect_Dir = (); 
File::Find::finddepth( {wanted => \&wanted_Name}, File::Spec->curdir );
Check( scalar(keys %Expect_Name) == 0 );


# no_chdir is in effect, hence we use file_path_name to specify the
# expected paths for %Expect_File

%Expect_File = (File::Spec->curdir => 1,
		file_path_name('.', 'fa') => 1,
                file_path_name('.', 'fa', 'fsl') => 1,
                file_path_name('.', 'fa', 'fa_ord') => 1,
                file_path_name('.', 'fa', 'fab') => 1,
                file_path_name('.', 'fa', 'fab', 'fab_ord') => 1,
                file_path_name('.', 'fa', 'fab', 'faba') => 1,
                file_path_name('.', 'fa', 'fab', 'faba', 'faba_ord') => 1,
                file_path_name('.', 'fa', 'faa') => 1,
                file_path_name('.', 'fa', 'faa', 'faa_ord') => 1,
                file_path_name('.', 'fb') => 1,
		file_path_name('.', 'fb', 'fba') => 1,
		file_path_name('.', 'fb', 'fba', 'fba_ord') => 1,
		file_path_name('.', 'fb', 'fb_ord') => 1);

delete $Expect_File{ file_path_name('.', 'fa', 'fsl') } unless $symlink_exists;
%Expect_Name = ();
%Expect_Dir = (); 

File::Find::finddepth( {wanted => \&wanted_File, no_chdir => 1},
		     File::Spec->curdir );

Check( scalar(keys %Expect_File) == 0 );


print "# check preprocess\n";
%Expect_File = ();
%Expect_Name = ();
%Expect_Dir = (
          File::Spec->curdir                 => {fa => 1, fb => 1}, 
          dir_path('.', 'fa')                => {faa => 1, fab => 1, fa_ord => 1},
          dir_path('.', 'fa', 'faa')         => {faa_ord => 1},
          dir_path('.', 'fa', 'fab')         => {faba => 1, fab_ord => 1},
          dir_path('.', 'fa', 'fab', 'faba') => {faba_ord => 1},
          dir_path('.', 'fb')                => {fba => 1, fb_ord => 1},
          dir_path('.', 'fb', 'fba')         => {fba_ord => 1}
          );

File::Find::find( {wanted => \&noop_wanted,
		   preprocess => \&my_preprocess}, File::Spec->curdir );

Check( scalar(keys %Expect_Dir) == 0 );


print "# check postprocess\n";
%Expect_File = ();
%Expect_Name = ();
%Expect_Dir = (
          File::Spec->curdir                 => 1,
          dir_path('.', 'fa')                => 1,
          dir_path('.', 'fa', 'faa')         => 1,
          dir_path('.', 'fa', 'fab')         => 1,
          dir_path('.', 'fa', 'fab', 'faba') => 1,
          dir_path('.', 'fb')                => 1,
          dir_path('.', 'fb', 'fba')         => 1
          );

File::Find::find( {wanted => \&noop_wanted,
		   postprocess => \&my_postprocess}, File::Spec->curdir );

Check( scalar(keys %Expect_Dir) == 0 );

{
    print "# checking argument localization\n";

    ### this checks the fix of perlbug [19977] ###
    my @foo = qw( a b c d e f );
    my %pre = map { $_ => } @foo;

    File::Find::find( sub {  } , 'fa' ) for @foo;
    delete $pre{$_} for @foo;

    Check( scalar( keys %pre ) == 0 );
}

# see thread starting
# http://www.xray.mpe.mpg.de/mailing-lists/perl5-porters/2004-02/msg00351.html
{
    print "# checking that &_ and %_ are still accessible and that\n",
	"# tie magic on \$_ is not triggered\n";
    
    my $true_count;
    my $sub = 0;
    sub _ {
	++$sub;
    }
    my $tie_called = 0;

    package Foo;
    sub STORE {
	++$tie_called;
    }
    sub FETCH {return 'N'};
    sub TIESCALAR {bless []};
    package main;

    Check( scalar( keys %_ ) == 0 );
    my @foo = 'n';
    tie $foo[0], "Foo";

    File::Find::find( sub { $true_count++; $_{$_}++; &_; } , 'fa' ) for @foo;
    untie $_;

    Check( $tie_called == 0);
    Check( scalar( keys %_ ) == $true_count );
    Check( $sub == $true_count );
    Check( scalar( @foo ) == 1);
    Check( $foo[0] eq 'N' );
}

if ( $symlink_exists ) {
    print "# --- symbolic link tests --- \n";
    $FastFileTests_OK= 1;


    # Verify that File::Find::find will call wanted even if the topdir of
    # is a symlink to a directory, and it shouldn't follow the link
    # unless follow is set, which it isn't in this case
    %Expect_File = ( file_path('fsl') => 1 );
    %Expect_Name = ();
    %Expect_Dir = ();
    File::Find::find( {wanted => \&wanted_File_Dir}, topdir('fa', 'fsl') );
    Check( scalar(keys %Expect_File) == 0 );

 
    %Expect_File = (File::Spec->curdir => 1, file_path('fa_ord') => 1,
                    file_path('fsl') => 1, file_path('fb_ord') => 1,
                    file_path('fba') => 1, file_path('fba_ord') => 1,
                    file_path('fab') => 1, file_path('fab_ord') => 1,
                    file_path('faba') => 1, file_path('faa') => 1,
                    file_path('faa_ord') => 1);

    %Expect_Name = ();

    %Expect_Dir = (File::Spec->curdir => 1, dir_path('fa') => 1,
                   dir_path('faa') => 1, dir_path('fab') => 1,
                   dir_path('faba') => 1, dir_path('fb') => 1,
                   dir_path('fba') => 1);

    File::Find::find( {wanted => \&wanted_File_Dir_prune,
		       follow_fast => 1}, topdir('fa') );

    Check( scalar(keys %Expect_File) == 0 );  


    # no_chdir is in effect, hence we use file_path_name to specify
    # the expected paths for %Expect_File

    %Expect_File = (file_path_name('fa') => 1,
		    file_path_name('fa', 'fa_ord') => 1,
		    file_path_name('fa', 'fsl') => 1,
                    file_path_name('fa', 'fsl', 'fb_ord') => 1,
                    file_path_name('fa', 'fsl', 'fba') => 1,
                    file_path_name('fa', 'fsl', 'fba', 'fba_ord') => 1,
                    file_path_name('fa', 'fab') => 1,
                    file_path_name('fa', 'fab', 'fab_ord') => 1,
                    file_path_name('fa', 'fab', 'faba') => 1,
                    file_path_name('fa', 'fab', 'faba', 'faba_ord') => 1,
                    file_path_name('fa', 'faa') => 1,
                    file_path_name('fa', 'faa', 'faa_ord') => 1);

    %Expect_Name = ();

    %Expect_Dir = (dir_path('fa') => 1,
		   dir_path('fa', 'faa') => 1,
                   dir_path('fa', 'fab') => 1,
		   dir_path('fa', 'fab', 'faba') => 1,
		   dir_path('fb') => 1,
		   dir_path('fb', 'fba') => 1);

    File::Find::find( {wanted => \&wanted_File_Dir, follow_fast => 1,
		       no_chdir => 1}, topdir('fa') );

    Check( scalar(keys %Expect_File) == 0 );

    %Expect_File = ();

    %Expect_Name = (file_path_name('fa') => 1,
		    file_path_name('fa', 'fa_ord') => 1,
		    file_path_name('fa', 'fsl') => 1,
                    file_path_name('fa', 'fsl', 'fb_ord') => 1,
                    file_path_name('fa', 'fsl', 'fba') => 1,
                    file_path_name('fa', 'fsl', 'fba', 'fba_ord') => 1,
                    file_path_name('fa', 'fab') => 1,
                    file_path_name('fa', 'fab', 'fab_ord') => 1,
                    file_path_name('fa', 'fab', 'faba') => 1,
                    file_path_name('fa', 'fab', 'faba', 'faba_ord') => 1,
                    file_path_name('fa', 'faa') => 1,
                    file_path_name('fa', 'faa', 'faa_ord') => 1);

    %Expect_Dir = ();

    File::Find::finddepth( {wanted => \&wanted_Name,
			    follow_fast => 1}, topdir('fa') );

    Check( scalar(keys %Expect_Name) == 0 );

    # no_chdir is in effect, hence we use file_path_name to specify
    # the expected paths for %Expect_File

    %Expect_File = (file_path_name('fa') => 1,
		    file_path_name('fa', 'fa_ord') => 1,
		    file_path_name('fa', 'fsl') => 1,
                    file_path_name('fa', 'fsl', 'fb_ord') => 1,
                    file_path_name('fa', 'fsl', 'fba') => 1,
                    file_path_name('fa', 'fsl', 'fba', 'fba_ord') => 1,
                    file_path_name('fa', 'fab') => 1,
                    file_path_name('fa', 'fab', 'fab_ord') => 1,
                    file_path_name('fa', 'fab', 'faba') => 1,
                    file_path_name('fa', 'fab', 'faba', 'faba_ord') => 1,
                    file_path_name('fa', 'faa') => 1,
                    file_path_name('fa', 'faa', 'faa_ord') => 1);

    %Expect_Name = ();
    %Expect_Dir = ();

    File::Find::finddepth( {wanted => \&wanted_File, follow_fast => 1,
			    no_chdir => 1}, topdir('fa') );

    Check( scalar(keys %Expect_File) == 0 );     

 
    print "# check dangling symbolic links\n";
    MkDir( dir_path('dangling_dir'), 0770 );
    CheckDie( symlink( dir_path('dangling_dir'),
		       file_path('dangling_dir_sl') ) );
    rmdir dir_path('dangling_dir');
    touch(file_path('dangling_file'));  
    CheckDie( symlink('../dangling_file','fa/dangling_file_sl') );
    unlink file_path('dangling_file');

    { 
        # these tests should also emit a warning
	use warnings;

        %Expect_File = (File::Spec->curdir => 1,
	                file_path('dangling_file_sl') => 1,
			file_path('fa_ord') => 1,
                        file_path('fsl') => 1,
                        file_path('fb_ord') => 1,
			file_path('fba') => 1,
                        file_path('fba_ord') => 1,
			file_path('fab') => 1,
                        file_path('fab_ord') => 1,
                        file_path('faba') => 1,
			file_path('faba_ord') => 1,
                        file_path('faa') => 1,
                        file_path('faa_ord') => 1);

        %Expect_Name = ();
        %Expect_Dir = ();
        undef $warn_msg;

        File::Find::find( {wanted => \&wanted_File, follow => 1,
			   dangling_symlinks =>
			       sub { $warn_msg = "$_[0] is a dangling symbolic link" }
                           },
                           topdir('dangling_dir_sl'), topdir('fa') );

        Check( scalar(keys %Expect_File) == 0 );
        Check( $warn_msg =~ m|dangling_file_sl is a dangling symbolic link| );  
        unlink file_path('fa', 'dangling_file_sl'),
                         file_path('dangling_dir_sl');

    }


    print "# check recursion\n";
    CheckDie( symlink('../faa','fa/faa/faa_sl') );
    undef $@;
    eval {File::Find::find( {wanted => \&simple_wanted, follow => 1,
                             no_chdir => 1}, topdir('fa') ); };
    Check( $@ =~ m|for_find[:/]fa[:/]faa[:/]faa_sl is a recursive symbolic link|i );  
    unlink file_path('fa', 'faa', 'faa_sl'); 


    print "# check follow_skip (file)\n";
    CheckDie( symlink('./fa_ord','fa/fa_ord_sl') ); # symlink to a file
    undef $@;

    eval {File::Find::finddepth( {wanted => \&simple_wanted,
                                  follow => 1,
                                  follow_skip => 0, no_chdir => 1},
                                  topdir('fa') );};

    Check( $@ =~ m|for_find[:/]fa[:/]fa_ord encountered a second time|i );


    # no_chdir is in effect, hence we use file_path_name to specify
    # the expected paths for %Expect_File

    %Expect_File = (file_path_name('fa') => 1,
		    file_path_name('fa', 'fa_ord') => 2,
		    # We may encounter the symlink first
		    file_path_name('fa', 'fa_ord_sl') => 2,
		    file_path_name('fa', 'fsl') => 1,
                    file_path_name('fa', 'fsl', 'fb_ord') => 1,
                    file_path_name('fa', 'fsl', 'fba') => 1,
                    file_path_name('fa', 'fsl', 'fba', 'fba_ord') => 1,
                    file_path_name('fa', 'fab') => 1,
                    file_path_name('fa', 'fab', 'fab_ord') => 1,
                    file_path_name('fa', 'fab', 'faba') => 1,
                    file_path_name('fa', 'fab', 'faba', 'faba_ord') => 1,
                    file_path_name('fa', 'faa') => 1,
                    file_path_name('fa', 'faa', 'faa_ord') => 1);

    %Expect_Name = ();

    %Expect_Dir = (dir_path('fa') => 1,
		   dir_path('fa', 'faa') => 1,
                   dir_path('fa', 'fab') => 1,
		   dir_path('fa', 'fab', 'faba') => 1,
		   dir_path('fb') => 1,
		   dir_path('fb','fba') => 1);

    File::Find::finddepth( {wanted => \&wanted_File_Dir, follow => 1,
                           follow_skip => 1, no_chdir => 1},
                           topdir('fa') );
    Check( scalar(keys %Expect_File) == 0 );
    unlink file_path('fa', 'fa_ord_sl');


    print "# check follow_skip (directory)\n";
    CheckDie( symlink('./faa','fa/faa_sl') ); # symlink to a directory
    undef $@;

    eval {File::Find::find( {wanted => \&simple_wanted, follow => 1,
                            follow_skip => 0, no_chdir => 1},
                            topdir('fa') );};

    Check( $@ =~ m|for_find[:/]fa[:/]faa[:/]? encountered a second time|i );

  
    undef $@;

    eval {File::Find::find( {wanted => \&simple_wanted, follow => 1,
                            follow_skip => 1, no_chdir => 1},
                            topdir('fa') );};

    Check( $@ =~ m|for_find[:/]fa[:/]faa[:/]? encountered a second time|i );  

    # no_chdir is in effect, hence we use file_path_name to specify
    # the expected paths for %Expect_File

    %Expect_File = (file_path_name('fa') => 1,
		    file_path_name('fa', 'fa_ord') => 1,
		    file_path_name('fa', 'fsl') => 1,
                    file_path_name('fa', 'fsl', 'fb_ord') => 1,
                    file_path_name('fa', 'fsl', 'fba') => 1,
                    file_path_name('fa', 'fsl', 'fba', 'fba_ord') => 1,
                    file_path_name('fa', 'fab') => 1,
                    file_path_name('fa', 'fab', 'fab_ord') => 1,
                    file_path_name('fa', 'fab', 'faba') => 1,
                    file_path_name('fa', 'fab', 'faba', 'faba_ord') => 1,
                    file_path_name('fa', 'faa') => 1,
                    file_path_name('fa', 'faa', 'faa_ord') => 1,
		    # We may actually encounter the symlink first.
                    file_path_name('fa', 'faa_sl') => 1,
                    file_path_name('fa', 'faa_sl', 'faa_ord') => 1);

    %Expect_Name = ();

    %Expect_Dir = (dir_path('fa') => 1,
		   dir_path('fa', 'faa') => 1,
                   dir_path('fa', 'fab') => 1,
		   dir_path('fa', 'fab', 'faba') => 1,
		   dir_path('fb') => 1,
		   dir_path('fb', 'fba') => 1);

    File::Find::find( {wanted => \&wanted_File_Dir, follow => 1,
		       follow_skip => 2, no_chdir => 1}, topdir('fa') );

    # If we encountered the symlink first, then the entries corresponding to
    # the real name remain, if the real name first then the symlink
    my @names = sort keys %Expect_File;
    Check( @names == 1 );
    # Normalise both to the original name
    s/_sl// foreach @names;
    Check ($names[0] eq file_path_name('fa', 'faa', 'faa_ord'));
    unlink file_path('fa', 'faa_sl');

}


# Win32 checks  - [perl #41555]
if ($^O eq 'MSWin32') {
    require File::Spec::Win32;
    my ($volume) = File::Spec::Win32->splitpath($orig_dir, 1);
    print STDERR "VOLUME = $volume\n";
    
    # with chdir
    %Expect_File = (File::Spec->curdir => 1,
                    file_path('fsl') => 1,
                    file_path('fa_ord') => 1,
                    file_path('fab') => 1,
                    file_path('fab_ord') => 1,
                    file_path('faba') => 1,
                    file_path('faba_ord') => 1,
                    file_path('faa') => 1,
                    file_path('faa_ord') => 1);

    delete $Expect_File{ file_path('fsl') } unless $symlink_exists;
    %Expect_Name = ();

    %Expect_Dir = (dir_path('fa') => 1,
                   dir_path('faa') => 1,
                   dir_path('fab') => 1,
                   dir_path('faba') => 1,
                   dir_path('fb') => 1,
                   dir_path('fba') => 1);
    
    
    
    File::Find::find( {wanted => \&wanted_File_Dir}, topdir('fa'));
    Check( scalar(keys %Expect_File) == 0 );    
    
    # no_chdir
    %Expect_File = ($volume . file_path_name('fa') => 1,
                    $volume . file_path_name('fa', 'fsl') => 1,
                    $volume . file_path_name('fa', 'fa_ord') => 1,
                    $volume . file_path_name('fa', 'fab') => 1,
                    $volume . file_path_name('fa', 'fab', 'fab_ord') => 1,
                    $volume . file_path_name('fa', 'fab', 'faba') => 1,
                    $volume . file_path_name('fa', 'fab', 'faba', 'faba_ord') => 1,
                    $volume . file_path_name('fa', 'faa') => 1,
                    $volume . file_path_name('fa', 'faa', 'faa_ord') => 1);
                    

    delete $Expect_File{ $volume . file_path_name('fa', 'fsl') } unless $symlink_exists;
    %Expect_Name = ();

    %Expect_Dir = ($volume . dir_path('fa') => 1,
                   $volume . dir_path('fa', 'faa') => 1,
                   $volume . dir_path('fa', 'fab') => 1,
                   $volume . dir_path('fa', 'fab', 'faba') => 1);
                   
    File::Find::find( {wanted => \&wanted_File_Dir, no_chdir => 1}, $volume . topdir('fa'));
    Check( scalar(keys %Expect_File) == 0 );
}


if ($symlink_exists) {  # Issue 68260
    print "# BUG  68260\n";
    MkDir (dir_path ('fa', 'fac'), 0770);
    MkDir (dir_path ('fb', 'fbc'), 0770);
    touch (file_path ('fa', 'fac', 'faca'));
    CheckDie (symlink ('..////../fa/fac/faca', 'fb/fbc/fbca'));

    use warnings;
    my $dangling_symlink;
    local $SIG {__WARN__} = sub {
        local $" = " ";
        $dangling_symlink ++ if "@_" =~ /dangling symbolic link/;
    };

    File::Find::find (
        {
            wanted            => sub {1;},
            follow            => 1,
            follow_skip       => 2,
            dangling_symlinks => 1,
        },
        File::Spec -> curdir
    );

    Check (!$dangling_symlink);
}


if ($^O eq 'MSWin32') {
    # Check F:F:f correctly handles a root directory path.
    # Rather than processing the entire drive (!), simply test that the
    # first file passed to the wanted routine is correct and then bail out.
    $orig_dir =~ /^(\w:)/ or die "expected a drive: $orig_dir";
    my $drive = $1;

    # Determine the file in the root directory which would be
    # first if processed in sorted order. Create one if necessary.
    my $expected_first_file;
    opendir(ROOT_DIR, "/") or die "cannot opendir /: $!\n";
    foreach my $f (sort readdir ROOT_DIR) {
        if (-f "/$f") {
            $expected_first_file = $f;
            last;
        }
    }
    closedir ROOT_DIR;
    my $created_file;
    unless (defined $expected_first_file) {
        $expected_first_file = '__perl_File_Find_test.tmp';
        open(F, ">", "/$expected_first_file") && close(F)
            or die "cannot create file in root directory: $!\n";
        $created_file = 1;
    }

    # Run F:F:f with/without no_chdir for each possible style of root path.
    # NB. If HOME were "/", then an inadvertent chdir('') would fluke the
    # expected result, so ensure it is something else:
    local $ENV{HOME} = $orig_dir;
    foreach my $no_chdir (0, 1) {
        foreach my $root_dir ("/", "\\", "$drive/", "$drive\\") {
            eval {
                File::Find::find({
                    'no_chdir' => $no_chdir,
                    'preprocess' => sub { return sort @_ },
                    'wanted' => sub {
                        -f or return; # the first call is for $root_dir itself.
                        my $got = $File::Find::name;
                        my $exp = "$root_dir$expected_first_file";
                        print "# no_chdir=$no_chdir $root_dir '$got'\n";
                        Check($got eq $exp);
                        die "done"; # don't process the entire drive!
                    },
                }, $root_dir);
            };
            # If F:F:f did not die "done" then it did not Check() either.
            unless ($@ and $@ =~ /done/) {
                print "# no_chdir=$no_chdir $root_dir ",
                    ($@ ? "error: $@" : "no files found"), "\n";
                Check(0);
            }
        }
    }
    if ($created_file) {
        unlink("/$expected_first_file")
            or warn "can't unlink /$expected_first_file: $!\n";
    }
}
