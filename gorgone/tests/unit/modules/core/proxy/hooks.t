#!/usr/bin/perl
use strict;
use warnings;
use Test2::V0;
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

my $logger = centreon::common::logger->new();
my $gorgone = mock {} => (add => [ send_internal_message => sub { return 0; } ]);
my $dbh = mock {} => (add => [ query => sub { return (-1, undef); } ]);

sub main {
    gorgone::modules::core::proxy::hooks::register(config => {}, config_core => {});

    register_remote(
        id    => $remote_id,
        uid   => $remote_uid,
        nodes => [ { id => $poller_id, uid => $poller_uid, pathscore => 1 } ]
    );

    test_static_subnode();
    test_dynamic_subnode();
    test_pong_cannot_redirect_uid();
    test_invalid_subnode_uid();
    test_unknown_target();
    test_resync();
    test_unregister();

    done_testing();
}

sub register_remote {
    my (%options) = @_;

    gorgone::modules::core::proxy::hooks::register_nodes_from_db(
        gorgone => $gorgone,
        dbh     => $dbh,
        logger  => $logger,
        data    => {
            nodes => [
                {
                    id      => $options{id},
                    uid     => $options{uid},
                    type    => $options{type} // 'push_zmq',
                    address => '127.0.0.2',
                    port    => 5556,
                    token   => '',
                    (defined($options{nodes}) ? (nodes => $options{nodes}) : ())
                }
            ]
        }
    );
}

sub pathway {
    my ($target) = @_;

    return [ gorgone::modules::core::proxy::hooks::pathway(
        action  => 'ENGINECOMMAND',
        target  => $target,
        gorgone => $gorgone,
        dbh     => $dbh,
        logger  => $logger
    ) ];
}

sub routed_by {
    my ($parent, $target) = @_;

    return [ 1, 0, $parent . '~~' . $target, $parent, $target ];
}

sub test_static_subnode {
    is(pathway($poller_id), routed_by($remote_id, $poller_id), 'poller behind remote server is reachable by its id');
    is(pathway($poller_uid), routed_by($remote_id, $poller_uid), 'poller behind remote server is reachable by its uid');
}

# dynamic routes (from PONG) are keyed by id and shared with the uid alias created from database.
sub test_dynamic_subnode {
    # static route through a pull remote server that never connected: it is skipped.
    register_remote(
        id    => 6,
        uid   => '359252497128229666',
        type  => 'pull',
        nodes => [ { id => 31, uid => '365432887777952531', pathscore => 1 } ]
    );
    register_remote(id => 5, uid => '359252497128229555');
    gorgone::modules::core::proxy::hooks::register_subnodes(
        id       => 5,
        subnodes => { 31 => { id => 31, uid => '365432887777952531', nodes => {} } }
    );

    is(pathway(31), routed_by(5, 31), 'dynamic route is used by id');
    is(pathway('365432887777952531'), routed_by(5, '365432887777952531'), 'dynamic route is used by uid');

    # a later PONG without the subnode removes the dynamic route for both id and uid.
    gorgone::modules::core::proxy::hooks::register_subnodes(id => 5, subnodes => {});
    is(pathway(31), [ -1 ], 'dynamic route removed for id');
    is(pathway('365432887777952531'), [ -1 ], 'dynamic route removed for uid');
}

sub test_pong_cannot_redirect_uid {
    register_remote(id => 7, uid => '359252497128229777');
    gorgone::modules::core::proxy::hooks::register_subnodes(
        id       => 7,
        subnodes => {
            999 => { id => 999, uid => $poller_uid, nodes => {} },
            998 => { id => 998, uid => '365432887777952998', nodes => {} },
        }
    );

    is(pathway($poller_uid), routed_by($remote_id, $poller_uid), 'PONG data cannot redirect a subnode uid');
    is(pathway('365432887777952998'), [ -1 ], 'PONG data cannot declare a subnode uid');
    is(pathway(999), routed_by(7, 999), 'dynamic subnode is still reachable by its id');
}

sub test_invalid_subnode_uid {
    register_remote(
        id    => 8,
        uid   => '359252497128229888',
        nodes => [
            { id => 26, uid => undef, pathscore => 1 },
            { id => 27, uid => '', pathscore => 1 },
            { id => 28, uid => 28, pathscore => 1 },
        ]
    );

    is(pathway(26), routed_by(8, 26), 'subnode without uid is reachable by its id');
    is(pathway(27), routed_by(8, 27), 'subnode with empty uid is reachable by its id');
    is(pathway(28), routed_by(8, 28), 'subnode with uid equal to id is reachable');
    is(pathway(''), [ -1 ], 'empty uid is not routed');
}

sub test_unknown_target {
    is(pathway('123456789'), [ -1 ], 'unknown uid is not routed');
}

# nodes module resyncs periodically: static routes are removed then added again.
sub test_resync {
    register_remote(
        id    => $remote_id,
        uid   => $remote_uid,
        nodes => [ { id => $poller_id, uid => $poller_uid, pathscore => 1 } ]
    );

    is(pathway($poller_id), routed_by($remote_id, $poller_id), 'poller reachable by its id after resync');
    is(pathway($poller_uid), routed_by($remote_id, $poller_uid), 'poller reachable by its uid after resync');
}

sub test_unregister {
    gorgone::modules::core::proxy::hooks::unregister_nodes(
        gorgone => $gorgone,
        dbh     => $dbh,
        logger  => $logger,
        data    => { nodes => [ { id => $remote_id, uid => $remote_uid } ] }
    );

    is(pathway($poller_id), [ -1 ], 'poller not routed by its id once remote server is unregistered');
    is(pathway($poller_uid), [ -1 ], 'poller not routed by its uid once remote server is unregistered');
}

main();
