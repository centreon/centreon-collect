#!/usr/bin/perl
use strict;
use warnings;
use Test2::V0;
use Test2::Tools::Compare qw{is};
use FindBin;
use lib "$FindBin::Bin/../../../../../";
use tests::unit::lib::mockLogger;
use gorgone::class::core;
use gorgone::standard::library;
use gorgone::modules::core::proxy::hooks;

# central (1) -> remote server (4, used as proxy) -> poller (25)
my $remote_id  = 4;
my $remote_uid = '359252497128229089';
my $poller_id  = 25;
my $poller_uid = '365432887777952551';

my $core_mock = mock 'gorgone::class::core' => (override => [
    # PROXYADDNODE routing stops once pathway is resolved, we don't have proxy processes here.
    waiting_ready_pool => sub { return 0; },
]);
my $library_mock = mock 'gorgone::standard::library' => (override => [
    add_history => sub { return 0; },
]);

sub main {
    my $logger = centreon::common::logger->new();
    my $gorgone = mock {} => (add => [ send_internal_message => sub { return 0; } ]);
    my $dbh = mock {} => (add => [ query => sub { return (-1, undef); } ]);

    gorgone::modules::core::proxy::hooks::register(config => {}, config_core => {});

    gorgone::modules::core::proxy::hooks::register_nodes_from_db(
        gorgone => $gorgone,
        dbh     => $dbh,
        logger  => $logger,
        data    => {
            nodes => [
                {
                    id      => $remote_id,
                    uid     => $remote_uid,
                    type    => 'push_zmq',
                    address => '127.0.0.2',
                    port    => 5556,
                    token   => '',
                    nodes   => [ { id => $poller_id, uid => $poller_uid, pathscore => 1 } ]
                }
            ]
        }
    );

    test_static_subnode($gorgone, $dbh, $logger);
    test_dynamic_subnode($gorgone, $dbh, $logger);
    test_unknown_target($gorgone, $dbh, $logger);

    done_testing();
}

sub pathway {
    my ($gorgone, $dbh, $logger, $target) = @_;

    return [ gorgone::modules::core::proxy::hooks::pathway(
        action  => 'ENGINECOMMAND',
        target  => $target,
        gorgone => $gorgone,
        dbh     => $dbh,
        logger  => $logger
    ) ];
}

sub test_static_subnode {
    my ($gorgone, $dbh, $logger) = @_;

    is(
        pathway($gorgone, $dbh, $logger, $poller_id),
        [ 1, 0, $remote_id . '~~' . $poller_id, $remote_id, $poller_id ],
        'poller behind remote server is reachable by its id'
    );
    is(
        pathway($gorgone, $dbh, $logger, $poller_uid),
        [ 1, 0, $remote_id . '~~' . $poller_uid, $remote_id, $poller_uid ],
        'poller behind remote server is reachable by its uid'
    );
}

# subnodes learnt from the PONG of the remote server
sub test_dynamic_subnode {
    my ($gorgone, $dbh, $logger) = @_;

    gorgone::modules::core::proxy::hooks::register_subnodes(
        id       => $remote_id,
        subnodes => {
            30 => { id => 30, uid => '365432887777952999', type => 'push_zmq', nodes => {} }
        }
    );

    is(
        pathway($gorgone, $dbh, $logger, '365432887777952999'),
        [ 1, 0, $remote_id . '~~365432887777952999', $remote_id, '365432887777952999' ],
        'dynamic subnode is reachable by its uid'
    );
}

sub test_unknown_target {
    my ($gorgone, $dbh, $logger) = @_;

    is(pathway($gorgone, $dbh, $logger, '123456789'), [ -1 ], 'unknown uid is not routed');
}

main();
