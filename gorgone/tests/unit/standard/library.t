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
    unlink('testU-1.sdb');

    my $mock_logger = mock 'centreon::common::logger'; # this is from Test2::Tools::Mock, included by Test2::V0
    # the test is about not making an error log when the db don't exist because it's created silently, so fail if the log error sub is called.
    $mock_logger->override('writeLogError' => sub {fail()});
    my $logger = centreon::common::logger->new();
    gorgone::standard::library::init_database(
        type              => 'SQLite',
        version           => '1.0',
        db                => 'dbname=./test1.sdb',
        logger            => $logger,
        autocreate_schema => 1,
    );

    is(-r 'test1.sdb', 1, 'database file created without any error.');

    my $db = DBI->connect("DBI:SQLite:dbname=./test1.sdb", undef, undef,
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

    unlink('test1.sdb');

}

sub main {
    init_database_no_file();
    done_testing();
}
main;