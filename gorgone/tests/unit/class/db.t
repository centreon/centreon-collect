#!/usr/bin/perl

use strict;
use warnings;
use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like match};
use FindBin;
use lib "$FindBin::Bin/../../../";
use gorgone::class::db;
use tests::unit::lib::mockLogger;
use Fcntl qw(:flock SEEK_END);

sub create_table_with_transaction {
    my $db_file = "class-db.sdb";
    unlink ($db_file) if -e $db_file;
    my $logger = centreon::common::logger->new();
    my $db = gorgone::class::db->new(
        type              => 'SQLite',
        version           => '1.0',
        db                => 'dbname=./' . $db_file,
        logger            => $logger,
        autocreate_schema => 1,
    );
    ok($db->connect() != -1, 'database connection should succeed.');
    is($db->start_transaction(), 0, 'first start transaction should succeed.');
    is($db->start_transaction(), -1, 'second start transaction should fail.');

    my ($status, $sth) = $db->query({
        query => "CREATE TABLE `gorgone_information` (`key` varchar(1024) DEFAULT NULL,`value` varchar(1024) DEFAULT NULL)",
    });
    is($status, 0, 'query should succeed.');
    is($sth, D(), 'sth should not be undef.');
    is($db->commit(), 0, 'commit should succeed.');

    is($db->commit(), 0, 'commit silently fail if not in a transaction.');
    is($db->start_transaction(), 0, 'start a transaction, delete the instance.');
    my $instance = delete($db->{instance});
    is($db->commit(), -1, 'commit should fail if no instance is present.');
    $db->{instance} = $instance;
    $db->disconnect();
    is($db->{instance}, U(), 'instance should be undef after disconnect.');
    ($status, $sth) = $db->query({
        query => "SELECT `key` FROM gorgone_information",
    });
    is($status, 0, 'query should reconnect and succeed.');
    is($sth, D(), 'sth should not be undef.');
    unlink($db_file);

}

sub main {
    create_table_with_transaction();
    done_testing();
}
main;





