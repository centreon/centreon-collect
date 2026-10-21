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

# fake database handle, only used to check which driver private attribute kill() reads.
package fakeDbh {
    sub new { my ($class, %attr) = @_; return bless {%attr, queries => []}, $class; }
    sub do { my ($self, $query) = @_; push @{$self->{queries}}, $query; return 1; }
    sub disconnect { }
}

# DBI->connect is mocked: we only check the DSN and the attributes given to the driver.
sub connect_with {
    my (%options) = @_;

    my @calls;
    my $mock = mock 'DBI' => (
        override => [ connect => sub { shift; push @calls, [@_]; return fakeDbh->new(); } ]
    );
    my $db = gorgone::class::db->new(logger => centreon::common::logger->new(), user => 'centreon', password => 'pass', %options);
    is($db->connect(), 0, 'connection should succeed.');
    is(scalar(@calls), 1, 'DBI->connect should be called once.');
    return ($db, $calls[0]->[0], $calls[0]->[3]);
}

sub test_driver_attributes {
    my ($db, $dsn, $attributes) = connect_with(dsn => 'mysql:host=localhost;port=3306;database=centreon');
    is($db->type(), 'mysql', 'mysql DSN should select the mysql driver.');
    is($dsn, 'DBI:mysql:host=localhost;port=3306;database=centreon', 'mysql DSN should not be modified.');
    is($attributes, { RaiseError => 0, PrintError => 0, AutoCommit => 1, mysql_enable_utf8 => 1 },
        'DBD::mysql should still get mysql_enable_utf8.');

    ($db, $dsn, $attributes) = connect_with(
        dsn => '"MariaDB:host=db;port=3306;database=centreon;mariadb_ssl=1;mariadb_ssl_ca_file=/etc/ssl/ca.pem;mariadb_ssl_verify_server_cert=1"'
    );
    is($db->type(), 'MariaDB', 'quoted MariaDB DSN should select the MariaDB driver.');
    is($dsn, 'DBI:MariaDB:host=db;port=3306;database=centreon;mariadb_ssl=1;mariadb_ssl_ca_file=/etc/ssl/ca.pem;mariadb_ssl_verify_server_cert=1',
        'MariaDB DSN should be given as is, without quotes.');
    is($attributes, { RaiseError => 0, PrintError => 0, AutoCommit => 1 },
        'DBD::MariaDB should not get any mysql_* attribute (it rejects them).');

    ($db, $dsn, $attributes) = connect_with(type => 'MariaDB', db => 'centreon', host => 'db', port => undef);
    is($dsn, 'DBI:MariaDB:centreon:db:3306', 'MariaDB without DSN should use the default port.');
    is($attributes, { RaiseError => 0, PrintError => 0, AutoCommit => 1 },
        'DBD::MariaDB without DSN should not get any mysql_* attribute.');

    ($db, $dsn, $attributes) = connect_with(db => 'centreon', host => 'db', port => 3307);
    is($dsn, 'DBI:mysql:centreon:db:3307', 'default driver should still be mysql.');
    is($attributes->{mysql_enable_utf8}, 1, 'default driver should still get mysql_enable_utf8.');
}

sub test_multi_statements {
    my ($db, $dsn, $attributes) = connect_with(dsn => 'mysql:host=localhost;database=centreon', multi_statements => 1);
    is($dsn, 'DBI:mysql:host=localhost;database=centreon;mysql_multi_statements=1', 'mysql multi statements option.');

    ($db, $dsn, $attributes) = connect_with(dsn => '"MariaDB:host=localhost;database=centreon"', multi_statements => 1);
    is($dsn, 'DBI:MariaDB:host=localhost;database=centreon;mariadb_multi_statements=1', 'MariaDB multi statements option.');

    ($db, $dsn, $attributes) = connect_with(dsn => 'MariaDB:host=localhost;database=centreon');
    is($dsn, 'DBI:MariaDB:host=localhost;database=centreon', 'multi statements should be disabled by default.');
}

sub test_kill {
    for my $test ([ 'mysql:host=localhost', 'mysql_thread_id' ], [ 'MariaDB:host=localhost', 'mariadb_thread_id' ]) {
        my $db = gorgone::class::db->new(logger => centreon::common::logger->new(), dsn => $test->[0]);
        $db->{instance} = fakeDbh->new($test->[1] => 42);
        $db->kill();
        is($db->{instance}->{queries}, [ 'KILL QUERY 42' ], "kill should use $test->[1].");
    }
}

sub main {
    create_table_with_transaction();
    test_driver_attributes();
    test_multi_statements();
    test_kill();
    done_testing();
}
main;





