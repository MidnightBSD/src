#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Test::More;

BEGIN {
    our $hasst;
    eval { my @n = stat "TEST" };
    $hasst = 1 unless $@ && $@ =~ /unimplemented/;
    unless ($hasst) { plan skip_all => "no stat"; exit 0 }
    use Config;
    $hasst = 0 unless $Config{'i_sysstat'} eq 'define';
    unless ($hasst) { plan skip_all => "no sys/stat.h"; exit 0 }
    our @stat = stat "TEST"; # This is the function stat.
    unless (@stat) { plan skip_all => "1..0 # Skip: no file TEST"; exit 0 }
}

plan tests => 19;

use_ok( 'File::stat' );

my $stat = File::stat::stat( "TEST" ); # This is the OO stat.
ok( ref($stat), 'should build a stat object' );

is( $stat->dev, $stat[0], "device number in position 0" );

# On OS/2 (fake) ino is not constant, it is incremented each time
SKIP: {
	skip('inode number is not constant on OS/2', 1) if $^O eq 'os2';
	is( $stat->ino, $stat[1], "inode number in position 1" );
}

is( $stat->mode, $stat[2], "file mode in position 2" );

is( $stat->nlink, $stat[3], "number of links in position 3" );

is( $stat->uid, $stat[4], "owner uid in position 4" );

is( $stat->gid, $stat[5], "group id in position 5" );

is( $stat->rdev, $stat[6], "device identifier in position 6" );

is( $stat->size, $stat[7], "file size in position 7" );

is( $stat->atime, $stat[8], "last access time in position 8" );

is( $stat->mtime, $stat[9], "last modify time in position 9" );

is( $stat->ctime, $stat[10], "change time in position 10" );

is( $stat->blksize, $stat[11], "IO block size in position 11" );

is( $stat->blocks, $stat[12], "number of blocks in position 12" );

SKIP: {
	local *STAT;
	skip("Could not open file: $!", 2) unless open(STAT, 'TEST');
	ok( File::stat::stat('STAT'), '... should be able to find filehandle' );

	package foo;
	local *STAT = *main::STAT;
	main::ok( my $stat2 = File::stat::stat('STAT'), 
		'... and filehandle in another package' );
	close STAT;

#	VOS open() updates atime; ignore this error (posix-975).
	my $stat3 = $stat2;
	if ($^O eq 'vos') {
		$$stat3[8] = $$stat[8];
	}

	main::skip("Win32: different stat-info on filehandle", 1) if $^O eq 'MSWin32';
	main::skip("dos: inode number is fake on dos", 1) if $^O eq 'dos';

	main::skip("OS/2: inode number is not constant on os/2", 1) if $^O eq 'os2';

	main::is( "@$stat", "@$stat3", '... and must match normal stat' );
}

local $!;
$stat = stat '/notafile';
isn't( $!, '', 'should populate $!, given invalid file' );

# Testing pretty much anything else is unportable.
