use warnings; no warnings 'deprecated';
use strict;

BEGIN {
	if("$]" < 5.011) {
		require Test::More;
		Test::More::plan(skip_all => "no array keys on this Perl");
	}
}

use Test::More tests => 4;

our @t;

$[ = 3;

@t = ();
is_deeply [ scalar keys @t ], [ 0 ];
is_deeply [ keys @t ], [];

@t = qw(a b c d e f);
is_deeply [ scalar keys @t ], [ 6 ];
is_deeply [ keys @t ], [ 3, 4, 5, 6, 7, 8 ];

1;
