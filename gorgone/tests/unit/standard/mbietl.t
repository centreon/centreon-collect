#!/usr/bin/perl

package main;
use strict;
use warnings;

# we can't use mock() on a non loaded package, so we need to create the class we want to mock first.
# We could have set centreon-common as a dependancy for the test, but it's not that package we are testing right now, so let mock it.
BEGIN {
    package centreon::common::centreonvault;
    sub get_secret {};
    sub new {};
    $INC{ (__PACKAGE__ =~ s{::}{/}rg) . ".pm" } = 1;
}

# same here, gorgone use a logger, but we don't want to test it right now, so we mock it.
BEGIN {
    package centreon::common::logger;
    sub writeLogInfo { warn "@_\n" };
    sub writeLogError { warn "@_\n" };
    sub writeLogDebug { warn "@_\n" };
    $INC{ (__PACKAGE__ =~ s{::}{/}rg) . ".pm" } = 1; # this allow the module to be available for other modules anywhere in the code.
}

package main;
use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like match};
use FindBin;
use lib "$FindBin::Bin/../../../";
use gorgone::standard::library;
use gorgone::class::db;
use gorgone::modules::centreon::mbi::etlworkers::class;
use gorgone::modules::centreon::mbi::etlworkers::hooks;
use gorgone::modules::centreon::mbi::etlworkers::dimensions::main;
use gorgone::class::module;
use tests::unit::lib::misc;

# This test uses the MySQL database created for Gorgone robot tests.
my $db_mon = $ENV{DBHOST} || 'mariadb';
my $db_bi = $ENV{DBHOST} || 'mariadb';

sub test_etl_dimensions {
    # this is from Test2::Tools::Mock, included by Test2::V0
    my $gorint = mock 'gorgone::class::module' => ( override => [ 'send_internal_action' => sub { 1 }, ], );
    my $logger = mock('centreon::common::logger');

    my %options = (
        logger => $logger->class,
            module_id => 'testmbietl',
            module_id => gorgone::modules::centreon::mbi::etlworkers::hooks::NAME,
            config_core => { gorgonecore => { internal_com_crypt => 0 } },
            noconfig => 1,
            data => {
                content => {
                    dbbi => {
                        centstorage => { dsn => "mysql:host=$db_bi;dbname=centreon_storage", user => "centreon", password=>"password" },
                        centreon => { dsn => "mysql:host=$db_bi;dbname=centreon", user => "centreon", password=>"password" }, },
                    dbmon => {
                        centstorage => { dsn => "mysql:host=$db_mon;dbname=centreon_storage", user => "centreon", password=>"password" },
                        centreon => { dsn => "mysql:host=$db_mon;dbname=centreon", user => "centreon", password=>"password" },
                    },
                    options => {
                        rebuild => 1, # Same as "centreonBIETL -r" command
                        nopurge => 0,
                    },
                    etlProperties => {
                        'dimension.all.hostcategories' => 1,
                        'dimension.hostcategories' => '',
                        'dimension.all.hostgroups' => 1,
                        'dimension.hostgroups' => '',
                        'dimension.all.servicecategories' => 1,
                        'dimension.servicecategories' => '',
                        'dimension.all.servicecategories' => 1,
                        'dimension.servicecategories' => '',
                        'liveservices.availability' => { '1' => 1, }, # This is 24x7 periods
                        'liveservices.perfdata' => { '1' => 1 },
                    },
                },
            },
        );

        # Execution errors from gorgone::modules::centreon::mbi::* modules are checked using Test2::Plugin::NoWarnings.
        my $gorgone = gorgone::modules::centreon::mbi::etlworkers::class->new( %options );
        $gorgone->db_connections(
            dbmon => $options{data}->{content}->{dbmon},
            dbbi => $options{data}->{content}->{dbbi}
        );

        # Test database connections
        my $dbmon_centreon = $gorgone->{dbmon_centreon_con};
        is($dbmon_centreon, D(), 'centreon database object should not be undef.');
        ok($dbmon_centreon->connect() == 0, 'trying to connect to centreon database.');

        my $db = $gorgone->{dbbi_centstorage_con};
        is($db, D(), 'BI database object should not be undef.');

        ok($db->connect() == 0, 'trying to connect to BI database.');

        return unless $dbmon_centreon->{instance} && $db->{instance};

        is( tests::unit::lib::misc::exec_sql($dbmon_centreon->{instance}, "$FindBin::Bin/mbietl_clear.sql"), U(), 'test data reinitialized successfully.');
        is( tests::unit::lib::misc::exec_sql($dbmon_centreon->{instance}, "$FindBin::Bin/mbietl_data.sql"), U(), 'test data loaded successfully.');

        my $dbh = $db->{instance};

        # First we truncate the BI tables to be sure we have a clean state
        gorgone::modules::centreon::mbi::etlworkers::dimensions::main::initVars( $gorgone, %{$options{data}->{content}} );
        gorgone::modules::centreon::mbi::etlworkers::dimensions::main::truncateDimensionTables( $gorgone, %{$options{data}->{content}}, );

        my $count;
        foreach my $table ('mod_bi_hosts', 'mod_bi_services', 'mod_bi_servicecategories', 'mod_bi_hostgroups') {
            $count = $dbh->selectrow_array("SELECT COUNT(*) FROM $table");
            is($count, 0, "table $table should be empty.");
        }

        # The real tests start here
        $gorgone->action_centreonmbietlworkersdimensions( %options, );

        # Check that data with semicolons are present in all tables
        $count = $dbh->selectrow_array("SELECT COUNT(*) FROM mod_bi_hosts WHERE host_name='host;pointvirgule'");
        ok(defined($count) && $count > 0, 'hosts with semicolon should be inserted into mod_bi_hosts.');

        $count = $dbh->selectrow_array("SELECT COUNT(*) FROM mod_bi_services WHERE sc_name='categorie;pointvirgule'");
        ok(defined($count) && $count > 0, 'services with semicolon should be inserted into mod_bi_services.');

        $count = $dbh->selectrow_array("select COUNT(*) from mod_bi_servicecategories where sc_name='categorie;pointvirgule'");
        ok(defined($count) && $count > 0, 'servicecategories with semicolon should be inserted into mod_bi_servicecategories.');

        $count = $dbh->selectrow_array("select COUNT(*) from mod_bi_hostgroups where hg_name='hostgroup;pointvirgule'");
        ok(defined($count) && $count > 0, 'hostgroups with semicolon should be inserted into mod_bi_hostgroups.');

        # Clean up
        tests::unit::lib::misc::exec_sql($dbmon_centreon->{instance}, "$FindBin::Bin/mbietl_clear.sql");
}

sub main {
    test_etl_dimensions();
    done_testing();
}
main
