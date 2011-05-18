warn "Legacy library @{[(caller(0))[6]]} will be removed from the Perl core distribution in the next major release. Please install it from the CPAN distribution Perl4::CoreLibs. It is being used at @{[(caller)[1]]}, line @{[(caller)[2]]}.\n";

# From: asherman@fmrco.com (Aaron Sherman)
#
# This library is no longer being maintained, and is included for backward
# compatibility with Perl 4 programs which may require it.
# This legacy library is deprecated and will be removed in a future
# release of perl.
#
# In particular, this should not be used as an example of modern Perl
# programming techniques.
#
# Suggested alternative: Sys::Hostname

sub hostname
{
	local(*P,@tmp,$hostname,$_);
	if (open(P,"hostname 2>&1 |") && (@tmp = <P>) && close(P))
	{
		chop($hostname = $tmp[$#tmp]);
	}
	elsif (open(P,"uname -n 2>&1 |") && (@tmp = <P>) && close(P))
	{
		chop($hostname = $tmp[$#tmp]);
	}
	else
	{
		die "$0: Cannot get hostname from 'hostname' or 'uname -n'\n";
	}
	@tmp = ();
	close P; # Just in case we failed in an odd spot....
	$hostname;
}

1;
