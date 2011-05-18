#
# $Id: utf8ref.t,v 1.1.1.1 2011-05-18 13:33:28 laffer1 Exp $
#

use strict;
use warnings;
use Encode;
use Test::More;
plan tests => 4;
#plan 'no_plan';

# my $a = find_encoding('ASCII');
my $u = find_encoding('UTF-8');
my $r = [];
no warnings 'uninitialized';
is encode_utf8($r), ''.$r;
is $u->encode($r), '';
$r = {};
is decode_utf8($r), ''.$r;
is $u->decode($r), '';
