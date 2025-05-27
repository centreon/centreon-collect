#!/usr/bin/perl
# we can't use mock() on a non loaded package, so we need to create the class we want to mock first.
# We could have set centreon-common as a dependancy for the test, but it's not that package we are testing right now, so let mock it.
BEGIN {
    package centreon::common::logger;
    sub severity {};
    sub new {return bless({}, 'centreon::common::logger');}
    sub writeLogError {};
    sub writeLogInfo {};
    $INC{ (__PACKAGE__ =~ s{::}{/}rg) . ".pm" } = 1; # this allow the module to be available for other modules anywhere in the code.
}
package main;
use strict;
use warnings;
use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like match};
use FindBin;
use lib "$FindBin::Bin/../../../";
use gorgone::standard::library;
use gorgone::class::db;

sub init_database_no_file {
    unlink('test.sdb');

    my $mock_logger = mock 'centreon::common::logger'; # this is from Test2::Tools::Mock, included by Test2::V0
    # the test is about not making an error log when the db don't exist because it's created silently, so fail if the log error sub is called.
    $mock_logger->override('writeLogError' => sub {fail()});
    my $logger = centreon::common::logger->new();

    my %options = (
        type    => 'SQLite',
        version => '1.0',
        db => 'dbname=./test1.sdb',
        logger  => $logger,
        autocreate_schema => 1,
    );
    gorgone::standard::library::init_database(%options);

    is(-r 'test1.sdb', 1, 'database file created without any error.');

    my $db = DBI->connect("DBI:SQLite:dbname=./test1.sdb", undef, undef,
        { RaiseError => 0, PrintError => 0, AutoCommit => 1, sqlite_unicode => 1 });
    is($db, D(), 'database connection should not be undef.');
    my $prepare_stm = $db->prepare("SELECT `value` FROM gorgone_information WHERE `key` = 'version'");
    is($prepare_stm, D(), 'prepare statement should not be undef.');

    my $sth = $prepare_stm->execute();
    is($prepare_stm->err, U(), 'no error when preparing query to gorgone_information.');

    my $row = $prepare_stm->fetchrow_hashref();
    is($row->{value}, '1.0', 'version should be 1.0');

    $prepare_stm = $db->prepare("SELECT `id`, `token` FROM gorgone_history");
    is($prepare_stm, D(), 'prepare statement should not be undef.');

    $sth = $prepare_stm->execute();
    is($prepare_stm->err, U(), 'no error when preparing query to gorgone_history.');

    $row = $prepare_stm->fetchrow_hashref();
    is($row, U(), 'no error when selecting from gorgone_history');

    # We test the creation of missing tables
    $db->do("DROP TABLE gorgone_history");
    $db->do("DROP TABLE gorgone_target_fingerprint");
    $row = $db->selectrow_arrayref("SELECT count(*) FROM sqlite_master where type='table' and name in ('gorgone_history', 'gorgone_target_fingerprint')");
    is($row->[0], 0, 'tables gorgone_history should not exist before init_database.' );

    gorgone::standard::library::init_database(%options);
    $row = $db->selectrow_arrayref("SELECT count(*) FROM sqlite_master where type='table' and name in ('gorgone_history', 'gorgone_target_fingerprint')");
    is($row->[0], 2, 'tables gorgone_history and gorgonet_target_fingerprint should exist after init_database.' );

    # We test the creation of missing indexes
    $db->do("DROP INDEX idx_gorgone_history_instant");
    $db->do("DROP INDEX idx_gorgone_synchistory_id");

    $row = $db->selectrow_arrayref("SELECT count(*) FROM sqlite_master where type='index' and name in ('idx_gorgone_history_instant', 'idx_gorgone_synchistory_id')"
        . " and tbl_name='gorgone_history'");
    is($row->[0], 0, 'indexes idx_gorgone_history_instant and idx_gorgone_synchistory_id should not exist before init_database.' );

    gorgone::standard::library::init_database(%options);
    $row = $db->selectrow_arrayref("SELECT count(*) FROM sqlite_master where type='index' and name in ('idx_gorgone_history_instant', 'idx_gorgone_synchistory_id')");
    is($row->[0], 2, 'indexes idx_gorgone_history_instant and idx_gorgone_synchistory_id should exist after init_databse.' );

    # Here we test the update of the gorgone_identity table
    $db->do("DROP TABLE gorgone_identity");
    $db->do(q{ CREATE TABLE IF NOT EXISTS `gorgone_identity` (
              `id` INTEGER PRIMARY KEY,
              `ctime` int(11) DEFAULT NULL,
              `mtime` int(11) DEFAULT NULL,
              `identity` varchar(2048) DEFAULT NULL,
              `key` varchar(1024) DEFAULT NULL,
              `iv` varchar(1024) DEFAULT NULL,
              `parent` int(11) DEFAULT '0'
            ) } );

    $row = $db->selectrow_arrayref( qq{ SELECT count(*) FROM pragma_table_info('gorgone_identity') } );
    is($row->[0], 7, 'table gorgone_identity should contains 7 columns before init_database.' );

    gorgone::standard::library::init_database(%options);
    $row = $db->selectrow_arrayref( qq{ SELECT count(*) FROM pragma_table_info('gorgone_identity') } );
    is($row->[0], 9, 'table gorgone_identity should contains 9 columns after init_database.' );

    unlink('test1.sdb');
}

sub main {
    init_database_no_file();
    done_testing();
}
main;
