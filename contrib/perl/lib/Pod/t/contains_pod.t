#!/usr/bin/env perl

# Copyright (C) 2005  Joshua Hoblitt
#
# $Id: contains_pod.t,v 1.1.1.2 2011-02-17 12:49:41 laffer1 Exp $

use strict;
BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    } else {
        use lib qw( ./lib );
    }
}

use Test::More tests => 2;

use Pod::Find qw( contains_pod );

{
    ok(contains_pod('lib/contains_pod.xr'), "contains pod");
}

{
    ok(contains_pod('lib/contains_bad_pod.xr'), "contains bad pod");
}
