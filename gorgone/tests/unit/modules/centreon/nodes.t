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
use Data::Dumper;
use centreon::common::logger;
use gorgone::modules::centreon::nodes::class;
my $check_action_ran = {};
my $action_expected = {
    'SETCOREID'     => { id => 1 },
    'UNREGISTERNODES'     => { nodes =>[]},
    'REGISTERNODES' => {
        'nodes' => [
            {
                'address' => '127.0.0.2',
                'type'    => 'push_zmq',
                'port'    => 5556,
                'id'      => 11
            },
            {
                'id'           => 12,
                'ssh_username' => undef,
                'address'      => '127.0.0.3',
                'type'         => 'push_ssh',
                'ssh_port'     => 22
            },
            {
                'port'    => undef,
                'id'      => 13,
                'address' => '127.0.0.4',
                'type'    => 'pull'
            },
            {
                'id'    => 14,
                'token' => 'TokenPullWss',
                'type'  => 'pullwss'
            },

        ] } };

sub main {

    my $gorint = mock 'gorgone::class::module' => (override => [ 'send_internal_action' => sub {
        my $action_name = $_[1]->{action};
        if ($action_name eq 'SETCOREID') {
            is($_[1]->{data}, $action_expected->{ $action_name }, "checking action " . $_[1]->{action});
        }
        elsif ($action_name eq 'REGISTERNODES' or $action_name eq 'UNREGISTERNODES') {
            # let's sort nodes array before comparing
            my @got_nodes = sort {$a->{id} <=> $b->{id}} @{$_[1]->{data}->{nodes}};
            is(\@got_nodes, $action_expected->{ $action_name}->{nodes}, "checking action " . $_[1]->{action});

        }
         $check_action_ran->{$action_name} = ($check_action_ran->{$action_name} // 0) + 1;
    }, ],);

    my $logger = centreon::common::logger->new();
    $logger->severity("debug");

    # this will mock the centreon database, should be a mariadb server but for unit test we can't set up a whole other server
    my $db = gorgone::class::db->new(
        type              => 'SQLite',
        version           => '1.0',
        db                => 'dbname=./test-centreon-nodes.sdb',
        logger            => $logger,
        autocreate_schema => 0,
    );

    my $sqlquery = gorgone::class::sqlquery->new(db_centreon => $db, logger => $logger);

    prepare_db($db);
    $logger->writeLogError("entering real function test");

    my $self = bless { logger => $logger, class_object => $sqlquery }, "gorgone::modules::centreon::nodes::class";
    $logger->writeLogInfo("entering real function test");
    $self->action_centreonnodessync();

    # some action must have been called, checking they had now.
    is($check_action_ran->{REGISTERNODES},1, "REGISTERNODES action was called");

    # let's delete all nodes except central and check again.
    $db->do("DELETE FROM nagios_server WHERE id != 1;");
    $action_expected->{REGISTERNODES}->{nodes} = [ ];; # expecting no nodes now.
    $check_action_ran = {};
    $action_expected->{UNREGISTERNODES} = { nodes => [
            { id => 11 },
            { id => 12 },
            { id => 13 },
            { id => 14 },
        ]
    };
    $self->action_centreonnodessync();

    done_testing();

}
sub prepare_db {
    my $db = shift;
    unlink("./test-centreon-nodes.sdb"); # clean up.

    $db->do('CREATE TABLE IF NOT EXISTS `options` (
    `key` VARCHAR(255) NOT NULL,
    `value` VARCHAR(255) DEFAULT NULL,
    PRIMARY KEY (`key`)
);');
    $db->do('CREATE TABLE IF NOT EXISTS `rs_poller_relation` (
    `remote_server_id` INTEGER PRIMARY KEY,
    `poller_server_id` INTEGER
);');
    $db->do("CREATE TABLE nagios_server (
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
    gorgone_auth_token TEXT,
    remote_id INTEGER,

    FOREIGN KEY (remote_id)
        REFERENCES nagios_server(id)
        ON DELETE SET NULL
        ON UPDATE CASCADE
);");
    $db->do("INSERT INTO `nagios_server` VALUES
(1,'central','1',1,NULL,'127.0.0.2','1',22,'1',5556,'',NULL),
(11,'poller_push','0',0,NULL,'127.0.0.2','1',22,'1',5556,'',NULL),
(12,'poller_ssh','0',0,NULL,'127.0.0.3','1',22,'2',22,'',NULL),
(13,'poller_pull','0',0,NULL,'127.0.0.4','1',22,'3',NULL,'TokenPull',NULL),
(14,'poller_pullwss','0',0,NULL,'127.0.0.5','1',22,'4',NULL,'TokenPullWss',NULL);")

}
&main;