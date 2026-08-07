#!/usr/bin/perl
use strict;
use warnings FATAL => 'all';
use Test2::V0;
#use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like match};
use FindBin;
use lib "$FindBin::Bin/../../../../";
use tests::unit::lib::mockCentreonvault;
use gorgone::standard::library;
use gorgone::class::db;
use tests::unit::lib::mockLogger;
use gorgone::modules::centreon::nodes::class;
sub main {
    test_centreonnodessync();
    done_testing();
}


my $check_action_ran = {};
my $action_expected = {
    'SETCOREID'     => { id => 1, uid => '' },
    'UNREGISTERNODES'     => { nodes =>[]},
    'REGISTERNODESFROMDB' => {
        'nodes' => [
            {
                'address' => '127.0.0.2',
                'type'    => 'push_zmq',
                'port'    => 5556,
                'id'      => 11,
                'token'   => '',
                'uid'     => '',
            },
            {
                'id'           => 12,
                'ssh_username' => undef,
                'address'      => '127.0.0.3',
                'type'         => 'push_ssh',
                'ssh_port'     => 22,
                'uid'          => '',
            },
            {
                'port'    => undef,
                'id'      => 13,
                'uid'   => 3999456,
                'address' => '127.0.0.4',

                'type'    => 'pull'
            },
            {
                'id'    => 14,
                'uid' => 499456456,
                'type'  => 'pullwss',
                'token' => '',
            },

        ] } };


sub test_centreonnodessync {

    my $gorint = mock 'gorgone::class::module' => (override => [ 'send_internal_action' => sub {
        my $action_name = $_[1]->{action};
        if ($action_name eq 'SETCOREID') {
            is($_[1]->{data}, $action_expected->{ $action_name }, "checking action " . $_[1]->{action});
        }
        elsif ($action_name eq 'REGISTERNODESFROMDB' or $action_name eq 'UNREGISTERNODES') {
            # let's sort nodes array before comparing
            my @got_nodes = sort {$a->{id} <=> $b->{id}} @{$_[1]->{data}->{nodes}};
            is(\@got_nodes, $action_expected->{ $action_name}->{nodes}, "checking action " . $_[1]->{action});

        }
         $check_action_ran->{$action_name} = ($check_action_ran->{$action_name} // 0) + 1;
    }, ],);

    my $logger = centreon::common::logger->new();
    $logger->severity("debug");


    my $sqlquery = prepare_db($logger);

    my $self = bless { logger => $logger, class_object => $sqlquery }, "gorgone::modules::centreon::nodes::class";
    $logger->writeLogInfo("entering real function test");
    $self->action_centreonnodessync();

    # some action must have been called, checking they had now.
    is($check_action_ran->{REGISTERNODESFROMDB},1, "REGISTERNODESFROMDB action was called");

    # let's delete all nodes except central and check again.
    $sqlquery->do(request =>  "DELETE FROM nagios_server WHERE id != 1;");
    $action_expected->{REGISTERNODESFROMDB}->{nodes} = [ ];; # expecting no nodes now.
    $check_action_ran = {};
    $action_expected->{UNREGISTERNODES} = { nodes => [
            { id => 11, 'uid' => '' },
            { id => 12, 'uid' => '' },
            { id => 13, 'uid' => 3999456 },
            { id => 14, 'uid' => 499456456 },
        ]
    };
    $self->action_centreonnodessync();

}
# create a sqlite db with centreon nodes data. This should be a mariadb database but for unit test we use sqlite for simplicity.
# param : logger
# return : db handle.
sub prepare_db {
    my $logger = shift;
    unlink("./test-centreon-nodes.sdb"); # clean up.

    my $db = gorgone::class::db->new(
        type              => 'SQLite',
        version           => '1.0',
        db                => 'dbname=./test-centreon-nodes.sdb',
        logger            => $logger,
        autocreate_schema => 0, # this would create gorgone tables, we don't want that for this test.
    );

    my $sqlquery = gorgone::class::sqlquery->new(db_centreon => $db, logger => $logger);

    # in sqlite we can't make a single statement with multiple commands separated by ;
    $sqlquery->do(request => 'CREATE TABLE IF NOT EXISTS `options` (
    `key` VARCHAR(255) NOT NULL,
    `value` VARCHAR(255) DEFAULT NULL,
    PRIMARY KEY (`key`)
);');
    $sqlquery->do(request => 'CREATE TABLE IF NOT EXISTS `rs_poller_relation` (
    `remote_server_id` INTEGER PRIMARY KEY,
    `poller_server_id` INTEGER
);');
    $sqlquery->do(request => "CREATE TABLE nagios_server (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT,
    localhost TEXT CHECK (localhost IN ('0', '1')),
    is_default INTEGER DEFAULT 0,
    remote_server_use_as_proxy TEXT CHECK (remote_server_use_as_proxy IN ('0', '1')) DEFAULT '0',
    ns_ip_address TEXT,
    ns_activate TEXT CHECK (ns_activate IN ('1', '0')) DEFAULT '1',
    ssh_port INTEGER,
    gorgone_communication_type TEXT
        CHECK (gorgone_communication_type IN ('1','2','3','4'))
        DEFAULT '1',
    gorgone_port INTEGER,
    uid BIGINT DEFAULT NULL,
    remote_id INTEGER,

    FOREIGN KEY (remote_id)
        REFERENCES nagios_server(id)
        ON DELETE SET NULL
        ON UPDATE CASCADE
);");
    $sqlquery->do(request => "INSERT INTO `nagios_server` VALUES
(1,'central','1',1,NULL,'127.0.0.2','1',22,'1',5556,'',NULL),
(11,'poller_push','0',0,NULL,'127.0.0.2','1',22,'1',5556,'',NULL),
(12,'poller_ssh','0',0,NULL,'127.0.0.3','1',22,'2',22,'',NULL),
(13,'poller_pull','0',0,NULL,'127.0.0.4','1',22,'3',NULL,3999456,NULL),
(14,'poller_pullwss','0',0,NULL,'127.0.0.5','1',22,'4',NULL,499456456,NULL);");
return $sqlquery;
}
&main;