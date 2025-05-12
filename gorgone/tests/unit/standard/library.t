#!/usr/bin/perl

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
use tests::unit::lib::mockLogger;

sub init_database_no_file {
    my $db_file = 'test-library.sdb';
    unlink($db_file) if -e $db_file;

    my $mock_logger = mock 'centreon::common::logger'; # this is from Test2::Tools::Mock, included by Test2::V0
    # the test is about not making an error log when the db don't exist because it's created silently, so fail if the log error sub is called.
    $mock_logger->override('writeLogError' => sub {fail()});

    my $logger = centreon::common::logger->new();
    gorgone::standard::library::init_database(
        type              => 'SQLite',
        version           => '1.0',
        db                => 'dbname=./' . $db_file,
        logger            => $logger,
        autocreate_schema => 1,
    );

    is(-r $db_file, 1, 'database file created without any error.');
    # let's check with a real dbi object the database is correct
    my $db = DBI->connect("DBI:SQLite:dbname=./$db_file", undef, undef,
        { RaiseError => 0, PrintError => 0, AutoCommit => 1, sqlite_unicode => 1 });
    is($db, D(), 'database connection should not be undef.');
    my $prepare_stm = $db->prepare("SELECT `value` FROM gorgone_information WHERE `key` = 'version'");
    is($prepare_stm, D(), 'prepare statement should not be undef.');

    my $sth = $prepare_stm->execute();
    is($prepare_stm->err, U(), 'no error when preparing query to gorgone_information');

    my $row = $prepare_stm->fetchrow_hashref();
    is($row->{value}, '1.0', 'version should be 1.0');

    $prepare_stm = $db->prepare("SELECT `id`, `token` FROM gorgone_history");
    is($prepare_stm, D(), 'prepare statement should not be undef.');

    $sth = $prepare_stm->execute();
    is($prepare_stm->err, U(), 'no error when preparing query to gorgone_history');

    $row = $prepare_stm->fetchrow_hashref();
    is($row, U(), 'no error when selecting from gorgone_history');

    unlink($db_file);

}

sub main {
    init_database_no_file();
    done_testing();
}
main;