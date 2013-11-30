#!/usr/bin/env perl

# Copyright (C) 2005  Joshua Hoblitt
#
# $Id: contains_pod.t,v 1.1.1.1 2011-05-18 13:33:29 laffer1 Exp $

use strict;

use Test::More tests => 2;

use Pod::Find qw( contains_pod );

{
    ok(contains_pod('t/pod/contains_pod.xr'), "contains pod");
}

{
    ok(contains_pod('t/pod/contains_bad_pod.xr'), "contains bad pod");
}
