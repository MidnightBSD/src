# This -*- perl -*- script makes the Makefile
# $Id: Makefile.PL,v 1.1.1.1 2009-03-15 19:18:41 ctriv Exp $

require 5.002;
use ExtUtils::MakeMaker;

#--- MY package

sub MY::libscan
{
 my($self,$path) = @_;

 return '' 
	if($path =~ m:/(RCS|CVS|SCCS)/: ||
	   $path =~ m:[~%]$: ||
	   $path =~ m:\.(orig|rej)$:
	  );
 
 $path;
}

WriteMakefile(
	VERSION_FROM	=> "SysV.pm",
	NAME      	=> "IPC::SysV",
	MAN3PODS	=> {}, 	# Pods will be built by installman.
 
	'dist'    	=> {COMPRESS => 'gzip -9f',
			    SUFFIX   => 'gz', 
			    DIST_DEFAULT => 'all tardist',
			   },
 
	'clean'   	=> {FILES => join(" ",
					map { "$_ */$_ */*/$_" }
					qw(*% *.html *.b[ac]k *.old))
			   },
	'macro'		=> { INSTALLDIRS => 'perl' },
);
