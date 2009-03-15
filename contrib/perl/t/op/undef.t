#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..36\n";

print defined($a) ? "not ok 1\n" : "ok 1\n";

$a = 1+1;
print defined($a) ? "ok 2\n" : "not ok 2\n";

undef $a;
print defined($a) ? "not ok 3\n" : "ok 3\n";

$a = "hi";
print defined($a) ? "ok 4\n" : "not ok 4\n";

$a = $b;
print defined($a) ? "not ok 5\n" : "ok 5\n";

@ary = ("1arg");
$a = pop(@ary);
print defined($a) ? "ok 6\n" : "not ok 6\n";
$a = pop(@ary);
print defined($a) ? "not ok 7\n" : "ok 7\n";

@ary = ("1arg");
$a = shift(@ary);
print defined($a) ? "ok 8\n" : "not ok 8\n";
$a = shift(@ary);
print defined($a) ? "not ok 9\n" : "ok 9\n";

$ary{'foo'} = 'hi';
print defined($ary{'foo'}) ? "ok 10\n" : "not ok 10\n";
print defined($ary{'bar'}) ? "not ok 11\n" : "ok 11\n";
undef $ary{'foo'};
print defined($ary{'foo'}) ? "not ok 12\n" : "ok 12\n";

print defined(@ary) ? "ok 13\n" : "not ok 13\n";
print defined(%ary) ? "ok 14\n" : "not ok 14\n";
undef @ary;
print defined(@ary) ? "not ok 15\n" : "ok 15\n";
undef %ary;
print defined(%ary) ? "not ok 16\n" : "ok 16\n";
@ary = (1);
print defined @ary ? "ok 17\n" : "not ok 17\n";
%ary = (1,1);
print defined %ary ? "ok 18\n" : "not ok 18\n";

sub foo { print "ok 19\n"; }

&foo || print "not ok 19\n";

print defined &foo ? "ok 20\n" : "not ok 20\n";
undef &foo;
print defined(&foo) ? "not ok 21\n" : "ok 21\n";

eval { undef $1 };
print $@ =~ /^Modification of a read/ ? "ok 22\n" : "not ok 22\n";

eval { $1 = undef };
print $@ =~ /^Modification of a read/ ? "ok 23\n" : "not ok 23\n";

{
    require Tie::Hash;
    tie my %foo, 'Tie::StdHash';
    print defined %foo ? "ok 24\n" : "not ok 24\n";
    %foo = ( a => 1 );
    print defined %foo ? "ok 25\n" : "not ok 25\n";
}

{
    require Tie::Array;
    tie my @foo, 'Tie::StdArray';
    print defined @foo ? "ok 26\n" : "not ok 26\n";
    @foo = ( a => 1 );
    print defined @foo ? "ok 27\n" : "not ok 27\n";
}

{
    # [perl #17753] segfault when undef'ing unquoted string constant
    eval 'undef tcp';
    print $@ =~ /^Can't modify constant item/ ? "ok 28\n" : "not ok 28\n";
}

# bugid 3096
# undefing a hash may free objects with destructors that then try to
# modify the hash. To them, the hash should appear empty.

$test = 29;
%hash = (
    key1 => bless({}, 'X'),
    key2 => bless({}, 'X'),
);
undef %hash;
sub X::DESTROY {
    print "not " if keys   %hash; print "ok $test\n"; $test++;
    print "not " if values %hash; print "ok $test\n"; $test++;
    print "not " if each   %hash; print "ok $test\n"; $test++;
    print "not " if defined delete $hash{'key2'}; print "ok $test\n"; $test++;
}
