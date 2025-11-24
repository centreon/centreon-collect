#!/usr/bin/perl
use strict;
use warnings FATAL => 'all';
use Test2::V0;
#use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like match};
use FindBin;
use lib "$FindBin::Bin/../../../../";
#use tests::unit::lib::mockLogger;
use tests::unit::lib::mockCentreonvault;
use gorgone::standard::library;
use gorgone::class::db;
use Data::Dumper;
use centreon::common::logger;
use gorgone::modules::centreon::nodes::class;

# here a sql script to create 4 poller with each communication type

# INSERT INTO `nagios_server` VALUES (10,'poller_push','0',0,0,'127.0.0.4','1','0','systemctl start centengine','systemctl stop centengine','systemctl restart centengine','systemctl reload centengine','/usr/sbin/centengine','/usr/sbin/centenginestats','/var/log/centreon-engine/service-perfdata','systemctl reload cbd','/etc/centreon-broker','/usr/share/centreon/lib/centreon-broker','/usr/lib64/centreon-connector',22,'1',5556,'', 'centreontrapd','/etc/snmp/centreon_traps/',NULL,NULL,NULL,NULL,'1','0',1);
# INSERT INTO `nagios_server` VALUES (11,'poller_ssh','0',0,0,'127.0.0.5','1','0','systemctl start centengine','systemctl stop centengine','systemctl restart centengine','systemctl reload centengine','/usr/sbin/centengine','/usr/sbin/centenginestats','/var/log/centreon-engine/service-perfdata','systemctl reload cbd','/etc/centreon-broker','/usr/share/centreon/lib/centreon-broker','/usr/lib64/centreon-connector',22,'2',22,'', 'centreontrapd','/etc/snmp/centreon_traps/',NULL,NULL,NULL,NULL,'1','0',1);
# INSERT INTO `nagios_server` VALUES (12,'poller_pullwss','0',0,0,'127.0.0.7','1','0','systemctl start centengine','systemctl stop centengine','systemctl restart centengine','systemctl reload centengine','/usr/sbin/centengine','/usr/sbin/centenginestats','/var/log/centreon-engine/service-perfdata','systemctl reload cbd','/etc/centreon-broker','/usr/share/centreon/lib/centreon-broker','/usr/lib64/centreon-connector',22,'4','443','tokenForWSS', 'centreontrapd','/etc/snmp/centreon_traps/',NULL,NULL,NULL,NULL,'1','0');
# INSERT INTO `nagios_server` VALUES (13,'poller_pull','0',0,0,'127.0.0.6','1','0','systemctl start centengine','systemctl stop centengine','systemctl restart centengine','systemctl reload centengine','/usr/sbin/centengine','/usr/sbin/centenginestats','/var/log/centreon-engine/service-perfdata','systemctl reload cbd','/etc/centreon-broker','/usr/share/centreon/lib/centreon-broker','/usr/lib64/centreon-connector',22,'3','5556','', 'centreontrapd','/etc/snmp/centreon_traps/',NULL,NULL,NULL,NULL,'1','0',1);

my $gorint = mock 'gorgone::class::module' => (override => [ 'send_internal_action' => sub {1}, ],);
my $logger =  centreon::common::logger->new();
$logger->severity("debug");

my %options = (
    logger => $logger,
    force => 0,
    dsn => "mysql:host=localhost:port=3306;dbname=centreon",
    user=> "centreon",
    password => '?0@*p28wuRT8yN!K',
);
my $db = gorgone::class::db->new(%options);
my $sqlquery = gorgone::class::sqlquery->new(db_centreon => $db, logger => $logger);
print("Db : " . Dumper($db) . "\n");
$logger->writeLogError("entering real function test");

my $self = bless { logger => $logger, class_object => $sqlquery }, "gorgone::modules::centreon::nodes::class";
$logger->writeLogInfo("entering real function test");
$self->action_centreonnodessync();
