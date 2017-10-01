use warnings; no warnings 'deprecated';
use strict;

use Test::More tests => 8;

our @i4 = (3, 5, 3, 5);

$[ = 3;

is_deeply [ scalar qw(a b c d e f)[3,4] ], [ qw(b) ];
is_deeply [ qw(a b c d e f)[3,4,8,9] ], [ qw(a b f), undef ];
is_deeply [ scalar qw(a b c d e f)[@i4] ], [ qw(c) ];
is_deeply [ qw(a b c d e f)[@i4] ], [ qw(a c a c) ];
is_deeply [ 3, 4, qw(a b c d e f)[@i4] ], [ 3, 4, qw(a c a c) ];

is_deeply [ qw(a b c d e f)[-1,-2] ], [ qw(f e) ];
is_deeply [ qw(a b c d e f)[2,1] ], [ qw(f e) ];
{
 $[ = -3;
 is_deeply [qw(a b c d e f)[-3]], ['a'];
}

1;
