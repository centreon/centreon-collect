#!/usr/bin/perl
use strict;
use warnings;
use Test2::V0;
use Data::Dumper;
#use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like match};
use FindBin;
use lib "$FindBin::Bin/../../../../../";
use tests::unit::lib::mockCentreonvault;
use gorgone::standard::library;
use gorgone::modules::core::proxy::httpserver;
use centreon::common::logger;
use gorgone::class::db;

sub main {
    my $logger = centreon::common::logger->new();
    $logger->severity("debug");
    my $class = bless({
        ws_clients => {},
        identities => {},
        nodes      => {},
        logger     => $logger,
        config     => { httpserver => { token => 'TheDefaultToken' } },
    },
        "gorgone::modules::core::proxy::httpserver");
    print(Dumper($logger));

    ok($class->is_token_ok(), "no token is ok");

    done_testing();

}
&main;