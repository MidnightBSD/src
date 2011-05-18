use strict;
use warnings;
use Test::More tests => 10;

use Socket qw(
   AF_INET NI_NUMERICHOST NI_NUMERICSERV
   getnameinfo pack_sockaddr_in inet_aton
);

my ( $err, $host, $service );

( $err, $host, $service ) = getnameinfo( pack_sockaddr_in( 80, inet_aton( "127.0.0.1" ) ), NI_NUMERICHOST|NI_NUMERICSERV );
cmp_ok( $err, "==", 0, '$err == 0 for {family=AF_INET,port=80,sinaddr=127.0.0.1}/NI_NUMERICHOST|NI_NUMERICSERV' );
cmp_ok( $err, "eq", "", '$err eq "" for {family=AF_INET,port=80,sinaddr=127.0.0.1}/NI_NUMERICHOST|NI_NUMERICSERV' );

is( $host, "127.0.0.1", '$host is 127.0.0.1 for NH/NS' );
is( $service, "80", '$service is 80 for NH/NS' );

# Probably "localhost" but we'd better ask the system to be sure
my $expect_host = gethostbyaddr( inet_aton( "127.0.0.1" ), AF_INET );
defined $expect_host or $expect_host = "127.0.0.1";

( $err, $host, $service ) = getnameinfo( pack_sockaddr_in( 80, inet_aton( "127.0.0.1" ) ), NI_NUMERICSERV );
cmp_ok( $err, "==", 0, '$err == 0 for {family=AF_INET,port=80,sinaddr=127.0.0.1}/NI_NUMERICSERV' );

is( $host, $expect_host, "\$host is $expect_host for NS" );
is( $service, "80", '$service is 80 for NS' );

# Probably "www" but we'd better ask the system to be sure
my $expect_service = getservbyport( 80, "tcp" );
defined $expect_service or $expect_service = "80";

( $err, $host, $service ) = getnameinfo( pack_sockaddr_in( 80, inet_aton( "127.0.0.1" ) ), NI_NUMERICHOST );
cmp_ok( $err, "==", 0, '$err == 0 for {family=AF_INET,port=80,sinaddr=127.0.0.1}/NI_NUMERICHOST' );

is( $host, "127.0.0.1", '$host is 127.0.0.1 for NH' );
is( $service, $expect_service, "\$service is $expect_service for NH" );
