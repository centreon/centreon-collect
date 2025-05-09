#!/usr/bin/perl

use strict;
use warnings;
use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like match};
use FindBin;
use lib "$FindBin::Bin/../../../";
use gorgone::class::db;
use gorgone::class::sqlquery;
use tests::unit::lib::mockLogger;

sub test_begin_transaction {
    my $db_file = "class-sqlquery.sdb";
    unlink ($db_file) if -e $db_file;
    my $logger = centreon::common::logger->new();

    my $db = gorgone::class::db->new(
        type              => 'SQLite',
        version           => '1.0',
        db                => 'dbname=./' . $db_file,
        logger            => $logger,
        autocreate_schema => 1,
    );
    my $sqlquery = gorgone::class::sqlquery->new(
        logger => $logger,
        db_centreon => $db,
    );

    is($sqlquery->begin_transaction(), 0, "begin transaction start a transation");
}

sub main {
    test_begin_transaction();
    done_testing();
}
main;






